using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using MphRead;
using MphRead.Mods.Launcher;

// Stand in for the extraction child: this program never calls Extract.Setup.
if (args.Length == 1 && args[0].EndsWith(".nds", StringComparison.Ordinal))
{
    File.WriteAllText(Path.Combine(Path.GetDirectoryName(args[0])!, "child-started"), args[0]);
    return int.Parse(Environment.GetEnvironmentVariable("FRUITY_SETUP_TEST_EXIT")!);
}

int failures = 0;
void Check(bool pass, string description)
{
    Console.WriteLine($"{(pass ? "PASS" : "FAIL")} {description}");
    if (!pass) failures++;
}
string previousDirectory = Environment.CurrentDirectory;
string previousRoot = GameFiles.Root;
string? previousExit = Environment.GetEnvironmentVariable("FRUITY_SETUP_TEST_EXIT");
string fixture = Directory.CreateTempSubdirectory("fruity-setup-check-").FullName;
// Only this test process accepts the synthetic fixture. Restore the allowlist
// afterwards; neither a ROM nor an extractor is used by the child process.
var known = (Dictionary<string, string>)typeof(RomWhitelist)
    .GetField("_known", BindingFlags.Static | BindingFlags.NonPublic)!.GetValue(null)!;
string? hash = null;
try
{
    string rom = Path.Combine(fixture, "synthetic input.nds");
    File.WriteAllText(rom, "setup child fixture, not a ROM");
    hash = RomWhitelist.Hash(rom)!;
    known.Add(hash, "synthetic test fixture");
    string extracted = Path.Combine(fixture, "old-files");
    Directory.CreateDirectory(extracted);
    File.WriteAllText(Path.Combine(fixture, "paths.txt"), $"0.19.0.0\n{Ver.AMHE1}={extracted}\n");
    GameFiles.Root = fixture;
    Environment.CurrentDirectory = fixture;
    Check(GameFiles.Ready, "pre-existing extraction is ready before the attempt");
    var messages = new List<string>();
    // Output/error callbacks can arrive concurrently.
    void Report(string message) { lock (messages) messages.Add(message); }
    Environment.SetEnvironmentVariable("FRUITY_SETUP_TEST_EXIT", "7");
    bool ok = GameFiles.RunSetup(rom, Report);
    string marker = Path.Combine(fixture, "child-started");
    Check(File.Exists(marker) && File.ReadAllText(marker) == rom,
        "dotnet-hosted setup starts the managed entry point with the full ROM argument");
    Check(!ok && messages.Exists(line => line.Contains("code 7")),
        "nonzero child exit fails setup even when old paths are ready");
    Check(GameFiles.Ready, "failed setup preserves the old configuration");

    Environment.SetEnvironmentVariable("FRUITY_SETUP_TEST_EXIT", "0");
    Check(GameFiles.RunSetup(rom, Report), "successful child and ready files complete setup");
    if (File.Exists(marker)) File.Delete(marker);
    File.Delete(rom);
    Check(!GameFiles.RunSetup(rom, Report) && !File.Exists(marker),
        "a removed selection is rejected before starting a child");
}
finally
{
    if (hash != null) known.Remove(hash);
    Environment.CurrentDirectory = previousDirectory;
    GameFiles.Root = previousRoot;
    Environment.SetEnvironmentVariable("FRUITY_SETUP_TEST_EXIT", previousExit);
    Paths.UpdatePaths();
    Directory.Delete(fixture, recursive: true);
}
return failures == 0 ? 0 : 1;
