using System.Text.RegularExpressions;
using Microsoft.CodeAnalysis;
using Microsoft.CodeAnalysis.CSharp;
using Microsoft.CodeAnalysis.CSharp.Syntax;

string root = args.Length > 0 ? Path.GetFullPath(args[0]) : Directory.GetCurrentDirectory();
while (!Directory.Exists(Path.Combine(root, "src", "MphRead")))
    root = Directory.GetParent(root)?.FullName ?? throw new InvalidOperationException("Repository root not found.");
var options = new CSharpParseOptions(preprocessorSymbols: new[] { "MPHREAD_AVALONIA", "MPHREAD_SHELL" });
var trees = Directory.EnumerateFiles(Path.Combine(root, "src", "MphRead"), "*.cs", SearchOption.AllDirectories)
    .Where(p => !p.Split(Path.DirectorySeparatorChar).Any(part => part is "bin" or "obj"))
    .Select(p => CSharpSyntaxTree.ParseText(File.ReadAllText(p), options, p)).ToArray();
var references = ((string)AppContext.GetData("TRUSTED_PLATFORM_ASSEMBLIES")!).Split(Path.PathSeparator)
    .Select(p => MetadataReference.CreateFromFile(p));
var compilation = CSharpCompilation.Create("UiCopyCheck", trees, references);
int failed = 0;
foreach (var tree in trees.Where(t => PlayerFacing(t.FilePath)))
    foreach (var issue in Check(tree, compilation.GetSemanticModel(tree)))
    {
        Console.Error.WriteLine($"{Path.GetRelativePath(root, tree.FilePath)}:{issue.Line}: {issue.Message}");
        failed++;
    }

// Positive and negative controls protect against a checker that silently stops scanning.
var fixture = CSharpSyntaxTree.ParseText("""
enum GameMode { BattleTeams }
class View {
  string Allowed = ".fpdemo"; // Record demo is an internal historical comment.
  string Bad = "Record demo";
  string Show(GameMode mode) => mode.ToString();
  string Also(GameMode mode) => $"Mode: {mode}";
}
""", options);
var fixtureCompilation = CSharpCompilation.Create("Fixture", new[] { fixture }, references);
if (Check(fixture, fixtureCompilation.GetSemanticModel(fixture)).Count() != 3)
    throw new InvalidOperationException("Copy checker positive/negative controls failed.");
Console.WriteLine($"[uicopycheck] {(failed == 0 ? "PASS" : "FAIL")}: {failed} regressions; parser controls passed.");
return failed == 0 ? 0 : 1;

static bool PlayerFacing(string path) => path.Contains(Path.Combine("Launcher", "Gui"))
    || path.Contains(Path.Combine("Launcher", "Portable"))
    || Path.GetFileName(path) is "DemoPlayback.cs" or "DemoLibrary.cs";

static IEnumerable<(int Line, string Message)> Check(SyntaxTree tree, SemanticModel model)
{
    var legacy = new Regex(@"\b(?:demo|demos|Record demo|as a demo|Fruity Prime demo|Unready|Require ready|Every file|Suit colour|OWNER ACTIONS|gamepad|cancelled)\b", RegexOptions.IgnoreCase);
    foreach (var node in tree.GetRoot().DescendantNodes())
    {
        string? text = node switch
        {
            LiteralExpressionSyntax literal when literal.IsKind(SyntaxKind.StringLiteralExpression) => literal.Token.ValueText,
            InterpolatedStringTextSyntax part => part.TextToken.ValueText,
            _ => null
        };
        int line = node.GetLocation().GetLineSpan().StartLinePosition.Line + 1;
        // Persisted keys, logging tags and internal type names are compatibility identifiers.
        if (text != null && !text.StartsWith('[') && legacy.IsMatch(text))
            yield return (line, $"Use player-facing terminology: {text}");
        ExpressionSyntax? expression = node switch
        {
            InvocationExpressionSyntax { Expression: MemberAccessExpressionSyntax member } when member.Name.Identifier.Text == "ToString" => member.Expression,
            InterpolationSyntax interpolation => interpolation.Expression,
            _ => null
        };
        if (expression != null && model.GetTypeInfo(expression).Type?.Name is "GameMode" or "MatchFormat" or "SessionPhase")
            yield return (line, "Format multiplayer values through UiText.");
    }
}
