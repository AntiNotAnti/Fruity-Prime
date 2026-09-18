using System;
using System.IO;
using System.Linq;

namespace MphRead.Mods.Launcher
{
    /// <summary>
    /// Deterministic filesystem checks for the linked ROM browser model.
    /// No launcher, Avalonia, game files or platform dialogs are involved.
    /// </summary>
    internal static class Program
    {
        private static int _failures;

        private static int Main()
        {
            string? root = null;
            try
            {
                root = Directory.CreateTempSubdirectory(
                    "fruity-prime-rom-browser-check-").FullName;
                BuildFixture(root);
                RunChecks(root);
            }
            catch (Exception ex)
            {
                Console.WriteLine($"  [FAIL] unexpected exception: {ex.GetType().Name}: {ex.Message}");
                _failures++;
            }
            finally
            {
                if (root != null)
                {
                    try
                    {
                        Directory.Delete(root, recursive: true);
                    }
                    catch (Exception ex)
                    {
                        Console.WriteLine($"  [FAIL] temporary fixture cleanup: {ex.GetType().Name}: {ex.Message}");
                        _failures++;
                    }
                }
            }

            Console.WriteLine();
            Console.WriteLine(_failures == 0
                ? "ROM BROWSER CHECK PASSED"
                : $"ROM BROWSER CHECK FAILED ({_failures} failure(s))");
            return _failures == 0 ? 0 : 1;
        }

        private static void BuildFixture(string root)
        {
            Directory.CreateDirectory(Path.Combine(root, "Alpha", "Nested"));
            Directory.CreateDirectory(Path.Combine(root, "Beta"));
            // This name sorts after both ROM files if all entries are sorted
            // together, making directories-first ordering observable.
            Directory.CreateDirectory(Path.Combine(root, "Zeta"));

            File.WriteAllText(Path.Combine(root, "ignore.txt"), "ignore");
            File.WriteAllText(Path.Combine(root, "hunters.nds"), "rom");
            File.WriteAllText(Path.Combine(root, "PRIME.NDS"), "rom");
            File.WriteAllText(Path.Combine(root, "Alpha", "inside.NdS"), "rom");
            File.WriteAllText(Path.Combine(root, "Alpha", "ignore.bin"), "ignore");
        }

        private static void RunChecks(string root)
        {
            string fullRoot = Path.GetFullPath(root);
            string alpha = Path.Combine(fullRoot, "Alpha");
            string primeRom = Path.Combine(fullRoot, "PRIME.NDS");
            string ignoredText = Path.Combine(fullRoot, "ignore.txt");

            var model = new RomFileBrowserModel(fullRoot);
            RomFileEntry[] rootEntries = model.Entries.ToArray();
            string[] expectedRootNames =
                { "Alpha", "Beta", "Zeta", "hunters.nds", "PRIME.NDS" };

            Check(SamePath(model.CurrentDirectory, fullRoot),
                "starts in the requested directory");
            Check(model.Error == null, "a valid starting directory has no error");
            Check(rootEntries.Select(entry => entry.Name)
                    .SequenceEqual(expectedRootNames),
                "filters and orders current-directory entries");
            Check(rootEntries.Take(3).All(entry => entry.Kind == RomFileEntryKind.Directory)
                    && rootEntries.Skip(3).All(entry => entry.Kind == RomFileEntryKind.Rom),
                "directories precede ROM files");
            Check(rootEntries.Any(entry => entry.Name == "PRIME.NDS"
                    && entry.Kind == RomFileEntryKind.Rom),
                "accepts an uppercase .NDS extension");
            Check(!rootEntries.Any(entry => entry.Name == "ignore.txt"),
                "filters unrelated files");
            Check(!rootEntries.Any(entry => entry.Name == "inside.NdS"),
                "does not recursively enumerate subdirectories");
            Check(rootEntries.All(entry => SamePath(entry.FullPath,
                    Path.Combine(fullRoot, entry.Name))),
                "returns full paths for displayed entries");

            Check(model.NavigateTo(alpha), "navigates into a directory");
            RomFileEntry[] alphaEntries = model.Entries.ToArray();
            Check(SamePath(model.CurrentDirectory, alpha),
                "updates the current directory after navigation");
            Check(alphaEntries.Select(entry => entry.Name)
                    .SequenceEqual(new[] { "Nested", "inside.NdS" }),
                "filters and orders the navigated directory");
            Check(model.GoUp(), "navigates to the parent directory");
            Check(SamePath(model.CurrentDirectory, fullRoot),
                "parent navigation restores the original directory");

            RomFileEntry[] beforeFailedNavigation = model.Entries.ToArray();
            bool failedNavigation = model.NavigateTo(
                Path.Combine(fullRoot, "does-not-exist"));
            Check(!failedNavigation, "rejects a missing directory");
            Check(SamePath(model.CurrentDirectory, fullRoot)
                    && beforeFailedNavigation.SequenceEqual(model.Entries),
                "failed navigation preserves the previous directory and entries");
            Check(!String.IsNullOrWhiteSpace(model.Error),
                "failed navigation reports an inline error");

            bool selected = model.TrySelectRom(primeRom, out string? selectedPath);
            Check(selected, "validates an existing direct ROM path");
            Check(SamePath(selectedPath, primeRom),
                "returns the normalized direct ROM path");
            Check(model.Error == null, "successful ROM validation clears the error");

            Check(!model.TrySelectRom(alpha, out _),
                "rejects a directory as a ROM path");
            Check(!model.TrySelectRom(ignoredText, out _),
                "rejects an unrelated direct path");
            Check(!model.TrySelectRom(Path.Combine(fullRoot, "missing.nds"), out _),
                "rejects a missing .nds path");
        }

        private static void Check(bool condition, string name)
        {
            Console.WriteLine($"  [{(condition ? "PASS" : "FAIL")}] {name}");
            if (!condition)
            {
                _failures++;
            }
        }

        private static bool SamePath(string? left, string right)
        {
            if (left == null)
            {
                return false;
            }

            StringComparison comparison = OperatingSystem.IsWindows()
                ? StringComparison.OrdinalIgnoreCase
                : StringComparison.Ordinal;
            return String.Equals(Path.GetFullPath(left), Path.GetFullPath(right), comparison);
        }
    }
}
