using System;
using System.Collections.Generic;
using System.IO;
using System.Security;

namespace MphRead.Mods.Launcher
{
    internal enum RomFileEntryKind
    {
        Directory,
        Rom
    }

    internal readonly record struct RomFileEntry(
        string Name, string FullPath, RomFileEntryKind Kind);

    /// <summary>One directory at a time; no UI or recursive filesystem work.</summary>
    internal sealed class RomFileBrowserModel
    {
        private readonly List<RomFileEntry> _entries = new();
        public string Extension { get; }

        /// <summary>Empty only while showing the Windows drive list.</summary>
        public string CurrentDirectory { get; private set; } = "";
        public IReadOnlyList<RomFileEntry> Entries => _entries;
        public string? Error { get; private set; }
        public bool ShowingDrives => OperatingSystem.IsWindows() && CurrentDirectory.Length == 0;

        public RomFileBrowserModel(string? lastDirectory = null, string extension = ".nds")
        {
            Extension = extension;
            string home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
            string? root = Path.GetPathRoot(Environment.CurrentDirectory);
            if (TryStart(lastDirectory) || TryStart(home) || TryStart(root))
            {
                return;
            }
            if (OperatingSystem.IsWindows())
            {
                ShowDrives();
            }
            else
            {
                NavigateTo(Path.DirectorySeparatorChar.ToString());
            }
        }

        private bool TryStart(string? path) => !String.IsNullOrWhiteSpace(path)
            && NavigateTo(path);

        public bool NavigateTo(string path)
        {
            try
            {
                string full = Path.GetFullPath(path);
                // Materialize before changing state. A failed or disconnected
                // directory leaves the previous list and location usable.
                var entries = new List<RomFileEntry>();
                foreach (string directory in Directory.EnumerateDirectories(full))
                {
                    entries.Add(new RomFileEntry(Path.GetFileName(directory),
                        directory, RomFileEntryKind.Directory));
                }
                foreach (string file in Directory.EnumerateFiles(full))
                {
                    if (MatchesExtension(file))
                    {
                        entries.Add(new RomFileEntry(Path.GetFileName(file),
                            file, RomFileEntryKind.Rom));
                    }
                }
                entries.Sort((a, b) =>
                {
                    int kind = a.Kind.CompareTo(b.Kind);
                    return kind != 0 ? kind
                        : StringComparer.OrdinalIgnoreCase.Compare(a.Name, b.Name);
                });
                CurrentDirectory = full;
                _entries.Clear();
                _entries.AddRange(entries);
                Error = null;
                return true;
            }
            catch (Exception ex) when (IsPathError(ex))
            {
                Error = $"This folder could not be opened: {ex.Message}";
                return false;
            }
        }

        public bool GoUp()
        {
            if (ShowingDrives)
            {
                return false;
            }
            string? parent = Directory.GetParent(CurrentDirectory)?.FullName;
            if (parent != null)
            {
                return NavigateTo(parent);
            }
            return OperatingSystem.IsWindows() && ShowDrives();
        }

        public void Refresh()
        {
            if (ShowingDrives)
            {
                ShowDrives();
            }
            else
            {
                NavigateTo(CurrentDirectory);
            }
        }

        /// <summary>Resolve a pasted path relative to the folder on screen.</summary>
        public string? Resolve(string? path)
        {
            if (String.IsNullOrWhiteSpace(path))
            {
                Error = $"Enter a folder or {Extension} file path.";
                return null;
            }
            try
            {
                string input = path.Trim().Trim('"');
                return Path.GetFullPath(Path.IsPathRooted(input) || ShowingDrives
                    ? input : Path.Combine(CurrentDirectory, input));
            }
            catch (Exception ex) when (IsPathError(ex))
            {
                Error = $"This path could not be opened: {ex.Message}";
                return null;
            }
        }

        public bool TrySelectRom(string path, out string? fullPath)
        {
            fullPath = Resolve(path);
            if (fullPath == null)
            {
                return false;
            }
            if (!MatchesExtension(fullPath))
            {
                Error = $"Choose a {Extension} file.";
                return false;
            }
            if (!File.Exists(fullPath))
            {
                Error = $"That {Extension} file could not be found or read.";
                return false;
            }
            Error = null;
            return true;
        }

        private bool MatchesExtension(string path) => String.Equals(Path.GetExtension(path), Extension, StringComparison.OrdinalIgnoreCase);

        private bool ShowDrives()
        {
            try
            {
                var drives = new List<RomFileEntry>();
                foreach (DriveInfo drive in DriveInfo.GetDrives())
                {
                    try
                    {
                        if (drive.IsReady)
                        {
                            drives.Add(new RomFileEntry(drive.Name, drive.RootDirectory.FullName,
                                RomFileEntryKind.Directory));
                        }
                    }
                    catch (Exception ex) when (IsPathError(ex))
                    {
                        // A removed drive must not hide the drives still here.
                    }
                }
                drives.Sort((a, b) => StringComparer.OrdinalIgnoreCase.Compare(a.Name, b.Name));
                CurrentDirectory = "";
                _entries.Clear();
                _entries.AddRange(drives);
                Error = drives.Count == 0 ? "No available drives were found." : null;
                return true;
            }
            catch (Exception ex) when (IsPathError(ex))
            {
                Error = $"The drive list could not be opened: {ex.Message}";
                return false;
            }
        }

        private static bool IsRomExtension(string path) => String.Equals(
            Path.GetExtension(path), ".nds", StringComparison.OrdinalIgnoreCase);

        private static bool IsPathError(Exception ex) => ex is ArgumentException
            or NotSupportedException or IOException or UnauthorizedAccessException
            or SecurityException;
    }
}
