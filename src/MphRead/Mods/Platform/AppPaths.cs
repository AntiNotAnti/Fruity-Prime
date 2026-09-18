using System;
using System.IO;

namespace MphRead.Mods.Platform
{
    /// <summary>Installation resources and writable desktop state have different roots.</summary>
    internal static class AppPaths
    {
        public static string ExecutableDirectory => AppContext.BaseDirectory;
        public static string ResourceDirectory => GetResourceDirectory(ExecutableDirectory, OperatingSystem.IsMacOS());

        internal static string GetResourceDirectory(string executableDirectory, bool macOS)
        {
            var executable = new DirectoryInfo(executableDirectory);
            bool bundled = macOS && executable.Name == "MacOS"
                && executable.Parent?.Name == "Contents"
                && executable.Parent.Parent?.Extension == ".app";
            return bundled ? Path.Combine(executable.Parent!.FullName, "Resources") : executableDirectory;
        }
        public static string Maps => Path.Combine(ResourceDirectory, "maps");
        public static string WritableMaps => Path.Combine(UserDataDirectory, "maps");

        // Other desktop platforms keep their existing portable layout. Android
        // sets GameFiles.Root and LauncherPrefs.Directory from its activity.
        public static string UserDataDirectory => GetUserDataDirectory(ExecutableDirectory,
            Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), OperatingSystem.IsMacOS());

        internal static string GetUserDataDirectory(string executableDirectory, string userProfile, bool macOS) => macOS
            ? Path.Combine(userProfile,
                "Library", "Application Support", Branding.Name)
            : executableDirectory;

        internal static bool IsReadOnlyMapPath(string path, string resourceMaps, bool macOS)
        {
            if (!macOS) return false;
            string full = Path.GetFullPath(path);
            string root = Path.TrimEndingDirectorySeparator(Path.GetFullPath(resourceMaps));
            if (full.Equals(root, StringComparison.OrdinalIgnoreCase)
                || full.StartsWith(root + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase)) return true;
            // Also guard manually entered destinations elsewhere in an app bundle.
            for (var directory = new DirectoryInfo(full); directory != null; directory = directory.Parent)
                if (directory.Extension.Equals(".app", StringComparison.OrdinalIgnoreCase)) return true;
            return false;
        }

        public static string PathsFile => Path.Combine(UserDataDirectory, "paths.txt");

        public static void PrepareUserData()
        {
            if (OperatingSystem.IsMacOS())
            {
                Directory.CreateDirectory(UserDataDirectory);
            }
        }
    }
}
