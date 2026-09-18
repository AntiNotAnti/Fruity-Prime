using System;
using System.IO;
using MphRead.Mods.Platform;

string root = Path.Combine(Path.GetTempPath(), "path fixture with spaces");
string bundle = Path.Combine(root, "Fruity Prime.app", "Contents");
string executable = Path.Combine(bundle, "MacOS");
string resources = Path.Combine(bundle, "Resources", "maps");
var cases = new (string Directory, string Expected)[]
{
    (executable, resources),
    (executable + Path.DirectorySeparatorChar, resources),
    // Ordinary publishes and developer builds retain their portable layout.
    (root, Path.Combine(root, "maps")),
    (Path.Combine(root, "Contents", "MacOS"),
        Path.Combine(root, "Contents", "MacOS", "maps"))
};
foreach (var item in cases)
{
    string syntheticMac = Path.Combine(AppPaths.GetResourceDirectory(item.Directory, true), "maps");
    if (syntheticMac != item.Expected) throw new Exception($"Synthetic macOS resource mismatch: {syntheticMac}");
    string portable = Path.Combine(AppPaths.GetResourceDirectory(item.Directory, false), "maps");
    if (portable != Path.Combine(item.Directory, "maps")) throw new Exception("Portable map directory changed.");
    AppContext.SetData("APP_CONTEXT_BASE_DIRECTORY", item.Directory);
    if (AppContext.BaseDirectory != item.Directory)
    {
        throw new Exception("Could not set the test executable directory.");
    }
    string expected = OperatingSystem.IsMacOS() ? item.Expected : Path.Combine(item.Directory, "maps");
    string actual = AppPaths.Maps;
    if (actual != expected)
    {
        throw new Exception($"Map path mismatch: expected {expected}, got {actual}");
    }
}
string profile = Path.Combine(root, "test user");
string userData = Path.Combine(profile, "Library", "Application Support", MphRead.Mods.Branding.Name);
if (AppPaths.GetUserDataDirectory(executable, profile, true) != userData
    || AppPaths.GetUserDataDirectory(executable, profile, false) != executable)
    throw new Exception("Writable user-data roots changed.");
string writableMaps = Path.Combine(userData, "maps");
foreach (string path in new[] { resources, Path.Combine(resources, "arena.json"),
    Path.Combine(resources, "..", "maps", "preview", "image.png"), Path.Combine(executable, "other.json") })
{
    if (!AppPaths.IsReadOnlyMapPath(path, resources, true)) throw new Exception("Bundle write allowed: " + path);
    if (AppPaths.IsReadOnlyMapPath(path, resources, false)) throw new Exception("Portable write blocked: " + path);
}
foreach (string path in new[] { writableMaps, Path.Combine(writableMaps, ".downloads", "map.tmp"),
    Path.Combine(root, "maps-user"), Path.Combine(root, "external", "map.json") })
    if (AppPaths.IsReadOnlyMapPath(path, resources, true)) throw new Exception("User map write blocked: " + path);
Console.WriteLine("Platform map path regressions passed (synthetic macOS, portable roots and bundle write guards).");
