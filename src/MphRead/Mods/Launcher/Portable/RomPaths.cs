using System;
using System.IO;

namespace MphRead.Mods.Launcher
{
    /// <summary>ROM paths in paths.txt, relative to the directory containing that file.</summary>
    public static class RomPaths
    {
        public static string Resolve(string key, string storedPath, string directory)
        {
            if (!IsRomKey(key) || String.IsNullOrWhiteSpace(storedPath))
            {
                return storedPath;
            }
            try
            {
                string root = Path.GetFullPath(directory);
                if (!IsAbsolute(storedPath))
                {
                    // A desktop paths.txt can also be copied to Android.
                    string relative = storedPath.Replace('\\', Path.DirectorySeparatorChar)
                        .Replace('/', Path.DirectorySeparatorChar);
                    return Path.GetFullPath(relative, root);
                }
                if (Directory.Exists(storedPath) || !HasExtractedSuffix(key, storedPath))
                {
                    return storedPath;
                }
                string candidate = Path.Combine(root, "files", key);
                if (IsFhKey(key))
                {
                    candidate = Path.Combine(candidate, "data");
                }
                return Directory.Exists(candidate) ? candidate : storedPath;
            }
            catch (Exception ex) when (ex is ArgumentException or NotSupportedException
                or PathTooLongException or IOException)
            {
                return storedPath;
            }
        }

        public static string ToStoredPath(string key, string path, string directory)
        {
            if (!IsRomKey(key) || String.IsNullOrWhiteSpace(path))
            {
                return path;
            }
            try
            {
                if (!Path.IsPathFullyQualified(path))
                {
                    return path;
                }
                string relative = Path.GetRelativePath(Path.GetFullPath(directory), path)
                    .Replace('\\', '/');
                // Only the extracted files beneath this installation travel with it.
                if (relative.StartsWith("files/", StringComparison.OrdinalIgnoreCase))
                {
                    return relative;
                }
            }
            catch (Exception ex) when (ex is ArgumentException or NotSupportedException
                or PathTooLongException or IOException)
            {
                // Keep malformed or foreign paths as configured.
            }
            return path;
        }

        private static bool IsRomKey(string key) => IsFhKey(key)
            || key == Ver.A76E0 || key == Ver.AMHE0 || key == Ver.AMHE1
            || key == Ver.AMHJ0 || key == Ver.AMHJ1 || key == Ver.AMHP0
            || key == Ver.AMHP1 || key == Ver.AMHK0;

        private static bool IsFhKey(string key) => key == Ver.AMFE0 || key == Ver.AMFP0;

        private static bool IsAbsolute(string path) => Path.IsPathRooted(path)
            || (path.Length >= 3 && Char.IsLetter(path[0]) && path[1] == ':'
                && (path[2] == '\\' || path[2] == '/'))
            || path.StartsWith("\\\\", StringComparison.Ordinal);

        private static bool HasExtractedSuffix(string key, string path)
        {
            string normalized = path.Replace('\\', '/').TrimEnd('/');
            string suffix = IsFhKey(key) ? $"/files/{key}/data" : $"/files/{key}";
            return normalized.EndsWith(suffix, StringComparison.OrdinalIgnoreCase);
        }
    }
}
