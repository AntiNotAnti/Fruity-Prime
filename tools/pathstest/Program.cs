using System;
using System.IO;
using System.Reflection;
using MphRead;
using MphRead.Mods.Launcher;
using MphRead.Mods.Network;

// Headless, asset-free regression for portable extraction paths.
string originalDirectory = Directory.GetCurrentDirectory();
string originalGameFilesRoot = GameFiles.Root;
string fixture = Directory.CreateTempSubdirectory("fruity-paths-").FullName;
int failures = 0;

void Check(bool condition, string description)
{
    Console.WriteLine($"{(condition ? "PASS" : "FAIL")} {description}");
    if (!condition)
    {
        failures++;
    }
}

try
{
    string oldInstall = Path.Combine(fixture, "old");
    string newInstall = Path.Combine(fixture, "new");
    string oldMph = Path.Combine(oldInstall, "files", Ver.AMHE1);
    string oldFh = Path.Combine(oldInstall, "files", Ver.AMFE0, "data");
    string external = Path.Combine(fixture, "external");
    Directory.CreateDirectory(oldMph);
    Directory.CreateDirectory(oldFh);
    Directory.CreateDirectory(external);
    File.WriteAllText(Path.Combine(oldInstall, "paths.txt"),
        $"0.19.0.0{Environment.NewLine}"
        + $"{Ver.AMHE1}=files/{Ver.AMHE1}{Environment.NewLine}"
        + $"{Ver.AMFE0}=files/{Ver.AMFE0}/data{Environment.NewLine}"
        + $"{Ver.AMHE0}={external}{Environment.NewLine}");

    Directory.Move(oldInstall, newInstall);
    Directory.SetCurrentDirectory(newInstall);
    Check(RomPaths.ToStoredPath(Ver.AMHE1, Path.Combine(newInstall, "files", Ver.AMHE1), newInstall)
            == $"files/{Ver.AMHE1}",
        "new extraction stores an install-local MPH path relatively");
    Check(RomPaths.ToStoredPath(Ver.AMHE0, external, newInstall) == external,
        "external extraction path remains absolute when saved");
    Paths.UpdatePaths();
    Paths.ChooseMphPath();
    Paths.ChooseFhPath();
    Check(Paths.FileSystem == Path.Combine(newInstall, "files", Ver.AMHE1),
        "relative MPH path resolves after moving the install");
    Check(Paths.FhFileSystem == Path.Combine(newInstall, "files", Ver.AMFE0, "data"),
        "relative First Hunt data resolves after moving the install");
    Check(Paths.AllPaths[Ver.AMHE0] == external,
        "existing external absolute path is preserved");

    // The downloaded server gets a copy of paths.txt, not the extracted tree.
    string serverDirectory = Path.Combine(fixture, "server");
    Directory.CreateDirectory(serverDirectory);
    GameFiles.Root = newInstall;
    typeof(LocalServer).GetMethod("CopyPaths", BindingFlags.NonPublic | BindingFlags.Static)!
        .Invoke(null, new object[] { serverDirectory });
    Directory.SetCurrentDirectory(serverDirectory);
    Paths.UpdatePaths();
    Check(Paths.AllPaths[Ver.AMHE1] == Path.Combine(newInstall, "files", Ver.AMHE1),
        "copied server configuration resolves the client's ROM files");
    Check(Paths.AllPaths[Ver.AMFE0] == Path.Combine(newInstall, "files", Ver.AMFE0, "data"),
        "copied server configuration resolves First Hunt data");
    Check(Paths.AllPaths[Ver.AMHE0] == external,
        "copied server configuration keeps external ROM paths");
    Directory.SetCurrentDirectory(newInstall);

    File.WriteAllText("paths.txt",
        $"0.19.0.0{Environment.NewLine}"
        + $"{Ver.AMHE1}={oldMph}{Environment.NewLine}"
        + $"{Ver.AMFE0}={oldFh}{Environment.NewLine}"
        + $"{Ver.AMHE0}={external}{Environment.NewLine}");
    Paths.UpdatePaths();
    Paths.ChooseMphPath();
    Paths.ChooseFhPath();
    Check(Paths.FileSystem == Path.Combine(newInstall, "files", Ver.AMHE1),
        "legacy absolute MPH path rebases after moving the install");
    Check(Paths.FhFileSystem == Path.Combine(newInstall, "files", Ver.AMFE0, "data"),
        "legacy absolute First Hunt path rebases after moving the install");
    Check(Paths.AllPaths[Ver.AMHE0] == external,
        "legacy repair does not replace an existing external path");

    Directory.Move(Path.Combine(newInstall, "files"), Path.Combine(newInstall, "elsewhere"));
    Paths.UpdatePaths();
    Check(Paths.AllPaths[Ver.AMHE1] == oldMph,
        "missing candidate does not fabricate a relocated path");
}
finally
{
    Directory.SetCurrentDirectory(originalDirectory);
    GameFiles.Root = originalGameFilesRoot;
    Directory.Delete(fixture, recursive: true);
}

return failures == 0 ? 0 : 1;
