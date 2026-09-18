using System;
using System.IO;
using System.Reflection;
using System.Linq;
using MphRead;
using MphRead.Mods.Launcher;
using MphRead.Mods.Network;
using MphRead.Mods.MapGen;
using MphRead.Mods.MapEditor;

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
    string newInstall = Path.Combine(fixture, "new=portable");
    string oldMph = Path.Combine(oldInstall, "files", Ver.AMHE1);
    string oldFh = Path.Combine(oldInstall, "files", Ver.AMFE0, "data");
    string external = Path.Combine(fixture, "external=roms");
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

    string bundledMaps = Path.Combine(fixture, "Fruity Prime.app", "Contents", "Resources", "maps");
    string userMaps = Path.Combine(fixture, "Application Support", "Fruity Prime", "maps");
    Directory.CreateDirectory(bundledMaps);
    Directory.CreateDirectory(userMaps);
    var map = MapTemplates.Create("Path arena").Definition;
    string bundledSource = Path.Combine(bundledMaps, "arena.json");
    map.Save(bundledSource);
    var catalog = new MapCatalog(userMaps, bundledMaps);
    Check(catalog.Refresh().Single().Path == bundledSource, "missing user map preserves bundled discovery");
    string userSource = Path.Combine(userMaps, "arena.json");
    map.Save(userSource);
    Check(catalog.Refresh().Single().Path == userSource, "same-identity user source takes precedence across roots");
    Check(new MapCatalog(userMaps, userMaps + Path.DirectorySeparatorChar).Refresh().Count == 1,
        "equivalent roots are scanned once");
    string package = MapPackageBuilder.Build(map, Path.Combine(bundledMaps, "arena.fpmap"));
    Check(catalog.Refresh().Single().Path == userSource,
        "writable source takes precedence over bundled package of the same identity");
    string installed = Path.Combine(userMaps, ".installed", map.MapId + ".fpmap");
    MapPackageBuilder.Build(map, installed);
    Check(catalog.Refresh().Single().Path == installed, "installed package remains the preferred runtime copy");
    Check(catalog.Refresh(false).Single().Path == userSource, "editor prefers writable source over installed package");

    // A different identity must not silently shadow a shipped runtime name.
    var collision = MapProjectSerializer.Clone(map); collision.MapId = Guid.NewGuid();
    collision.Save(userSource);
    File.Delete(installed);
    Check(catalog.Refresh().All(e => !e.Validation.IsValid), "cross-root runtime-name collision is diagnosed");
    collision.Name = "Other name"; collision.MapId = map.MapId; collision.Save(userSource);
    Check(catalog.Refresh().All(e => !e.Validation.IsValid), "cross-root identity with a different name is diagnosed");
    collision = MapProjectSerializer.Clone(map); collision.Materials.Clear(); collision.Save(userSource);
    Check(catalog.Refresh().Any(e => e.Validation.IsValid && e.Path == package),
        "invalid writable source does not hide valid bundled identity");
    File.Delete(userSource);

    // All writes below target the activity/CLI override, never the source root.
    CustomRooms.MapDirectory = userMaps;
    Check(CustomRooms.WritableMapDirectory == userMaps
        && NetMapTransfer.LibraryDirectory == Path.Combine(userMaps, ".installed"),
        "Android and explicit portable map overrides control writable paths");
    Check(CustomRooms.CreateCatalog().Refresh().Count == 0, "explicit map override does not add shipped maps");
    byte[] bundledBefore = File.ReadAllBytes(bundledSource);
    var document = new MapDocument(MapProjectSerializer.Load(bundledSource), bundledSource);
    document.Edit("Edit bundled source", d => d.Description = "Writable copy");
    document.Autosave(CustomRooms.WritableMapDirectory);
    Check(File.Exists(document.RecoveryPath(userMaps)) && !Directory.Exists(Path.Combine(bundledMaps, ".autosave")),
        "bundled-source autosave uses the writable library");
    string destination = CustomRooms.NewProjectPath(map.Name);
    document.Save(destination);
    Check(File.Exists(destination) && File.ReadAllBytes(bundledSource).SequenceEqual(bundledBefore),
        "saving a writable copy leaves bundled JSON unchanged");
    Check(CustomRooms.NewProjectPath(map.Name) != destination, "new bundled copies do not overwrite an existing project folder");
    var packaged = new MapDocument(MapProjectSerializer.Load(package), package);
    string packageCopy = CustomRooms.NewProjectPath(map.Name);
    packaged.Save(packageCopy);
    Check(packaged.Project.Definition.BundlePath == null
        && packaged.Project.Definition.BaseDirectory == Path.GetDirectoryName(packageCopy),
        "package Save As materializes a writable project context");
    Check(!Directory.Exists(Path.Combine(bundledMaps, ".installed"))
        && Directory.GetFiles(bundledMaps, "*", SearchOption.AllDirectories).Length == 2,
        "read-only discovery and copy workflow add no files to the bundle");

    // Exercise the macOS import preparation on every host: a compile clones its
    // definition before Q3Import can bake, so the redirected path must survive it.
    var prepareImport = typeof(CustomRooms).GetMethod("PrepareImportForBuild", BindingFlags.Static | BindingFlags.NonPublic,
        null, new[] { typeof(MapDefinition), typeof(string), typeof(string), typeof(bool) }, null)!;
    var imported = new MapDefinition { BaseDirectory = bundledMaps,
        Import = new() { BaseDirectory = bundledMaps, Source = "level.pk3", Textures = "missing.tex" } };
    prepareImport.Invoke(null, new object[] { imported, bundledMaps, userMaps, true });
    string bakePath = imported.Import.Textures!;
    Check(Path.IsPathRooted(bakePath) && bakePath.StartsWith(Path.Combine(userMaps, ".cache", "textures") + Path.DirectorySeparatorChar)
        && imported.Import.Source == "level.pk3" && imported.BaseDirectory == bundledMaps,
        "missing bundled import textures bake in user storage while retaining the source context");
    var buildSnapshot = MapProjectSerializer.Clone(imported);
    Check(Path.Combine(buildSnapshot.Import!.BaseDirectory!, buildSnapshot.Import.Textures!) == bakePath,
        "preview/build/package snapshots preserve the safe texture bake destination");
    imported.Import.Textures = Path.Combine(bundledMaps, "absolute-missing.tex");
    prepareImport.Invoke(null, new object[] { imported, bundledMaps, userMaps, true });
    Check(imported.Import.Textures!.StartsWith(userMaps), "absolute bundled texture targets are also redirected");
    imported.Import.Textures = "missing.tex";
    prepareImport.Invoke(null, new object[] { imported, bundledMaps, userMaps, false });
    Check(imported.Import.Textures == "missing.tex", "portable import bake paths remain unchanged");
    string existingTexture = Path.Combine(bundledMaps, "existing.tex"); File.WriteAllBytes(existingTexture, new byte[] { 1 });
    imported.Import.Textures = existingTexture;
    prepareImport.Invoke(null, new object[] { imported, bundledMaps, userMaps, true });
    Check(imported.Import.Textures == existingTexture, "existing bundled texture packs remain read-only inputs");
}
finally
{
    Directory.SetCurrentDirectory(originalDirectory);
    GameFiles.Root = originalGameFilesRoot;
    Directory.Delete(fixture, recursive: true);
}

return failures == 0 ? 0 : 1;
