using System;
using System.IO;

namespace MphRead.Mods.MapGen
{
    /// <summary>Compatibility entry point for legacy callers and v1 bundles.</summary>
    public static class MapBundle
    {
        public const string Extension = ".fpmap";
        public static bool Is(string path) => Path.GetExtension(path).Equals(Extension, StringComparison.OrdinalIgnoreCase);
        public static string Cook(MapDefinition definition, string recipePath, string? outputPath, bool verbose = true)
        {
            string destination = outputPath ?? Path.Combine(CustomRooms.WritableMapDirectory,
                Path.GetFileNameWithoutExtension(recipePath) + Extension);
            if (CustomRooms.IsReadOnlyMapPath(destination))
                throw new IOException("Choose a package path outside the application bundle.");
            CustomRooms.PrepareImportForBuild(definition);
            string path = MapPackageBuilder.Build(definition, destination);
            if (verbose) Console.WriteLine($"[mappackage] {definition.Name} -> {path}");
            return path;
        }
        public static string? ReadRecipe(string bundlePath)
        {
            using var package = new MapPackageReader(bundlePath);
            return package.ReadProject();
        }
        public static byte[]? ReadEntry(string bundlePath, string name)
        {
            if (string.IsNullOrEmpty(name)) return null;
            using var package = new MapPackageReader(bundlePath);
            return package.Read(name);
        }
    }
}
