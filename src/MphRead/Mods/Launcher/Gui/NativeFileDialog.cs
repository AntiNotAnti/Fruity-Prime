using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;
using System.Threading;
using System.Threading.Tasks;
#if MPHREAD_SHELL
using OpenTK.Windowing.GraphicsLibraryFramework;
#endif

namespace MphRead.Mods.Launcher.Gui
{
    /// <summary>
    /// Ask the desktop for a file, without a toolkit window to hang the
    /// dialog off.
    ///
    /// The launcher runs on Avalonia's *headless* backend so that the whole
    /// program is one window, and headless has no windowing system behind it
    /// to open a file chooser with: <c>TopLevel.StorageProvider</c> is
    /// Avalonia's no-op, whose <c>OpenFilePickerAsync</c> returns an empty
    /// list without showing anything and without failing. That is "I press
    /// browse and nothing happens" -- not a crash to find in a log, a picker
    /// that was never opened. So the platform is asked directly: comdlg32 on
    /// Windows, zenity or kdialog on Linux, osascript on macOS.
    ///
    /// It runs off the calling thread, because the caller is the game's
    /// thread drawing the frame the screen is being rendered into: a modal
    /// dialog pumped there stops the game for as long as it is up.
    /// </summary>
    internal static class NativeFileDialog
    {
        /// <summary>
        /// A path, nothing at all (the player cancelled), or a reason there
        /// was no chooser to open -- which is the one case a screen has to
        /// say out loud.
        /// </summary>
        internal readonly record struct FilePick(string? Path, string? Problem);

        /// <summary>Is there anything on this machine that opens one?</summary>
        public static bool Available => OperatingSystem.IsWindows()
            || OperatingSystem.IsMacOS() || Helper() != null;

        public static Task<FilePick> OpenAsync(string title, string label,
            string extension, string? startIn = null)
        {
            string pattern = "*" + (extension.StartsWith('.') ? extension : "." + extension);
            return OnItsOwnThread(() =>
            {
                if (OperatingSystem.IsWindows())
                {
                    return WindowsOpen(title, label, pattern, startIn);
                }
                if (OperatingSystem.IsMacOS())
                {
                    return MacOpen(title, pattern.TrimStart('*', '.'));
                }
                return LinuxOpen(title, label, pattern, startIn);
            });
        }

        private static Task<FilePick> OnItsOwnThread(Func<FilePick> open)
        {
            var done = new TaskCompletionSource<FilePick>();
            var thread = new Thread(() =>
            {
                try
                {
                    done.SetResult(open());
                }
                catch (Exception ex)
                {
                    Mods.DebugLog.Exception("launcher", ex);
                    done.SetResult(new FilePick(null, "The file chooser could not be "
                        + $"opened: {ex.Message}"));
                }
            })
            {
                IsBackground = true
            };
            if (OperatingSystem.IsWindows())
            {
                // comdlg32 puts COM controls in its dialog; an MTA thread gets
                // a chooser with no places bar or none at all.
                thread.SetApartmentState(ApartmentState.STA);
            }
            thread.Start();
            return done.Task;
        }

        // ------------------------------------------------------------ Windows

        private const int _fileMustExist = 0x00001000;
        private const int _pathMustExist = 0x00000800;
        private const int _noChangeDir = 0x00000008;
        private const int _explorer = 0x00080000;
        private const int _hideReadOnly = 0x00000004;

        [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
        private struct OpenFileName
        {
            public int StructSize;
            public IntPtr Owner;
            public IntPtr Instance;
            public string? Filter;
            public string? CustomFilter;
            public int MaxCustomFilter;
            public int FilterIndex;
            public IntPtr File;
            public int MaxFile;
            public string? FileTitle;
            public int MaxFileTitle;
            public string? InitialDir;
            public string? Title;
            public int Flags;
            public short FileOffset;
            public short FileExtension;
            public string? DefExt;
            public IntPtr CustomData;
            public IntPtr Hook;
            public string? TemplateName;
            public IntPtr Reserved;
            public int Reserved2;
            public int FlagsEx;
        }

        [DllImport("comdlg32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        private static extern bool GetOpenFileNameW(ref OpenFileName options);

        [DllImport("comdlg32.dll")]
        private static extern int CommDlgExtendedError();

        [SupportedOSPlatform("windows")]
        private static FilePick WindowsOpen(string title, string label, string pattern,
            string? startIn)
        {
            IntPtr buffer = Marshal.AllocHGlobal(sizeof(char) * 1024);
            try
            {
                for (int i = 0; i < 1024; i++)
                {
                    Marshal.WriteInt16(buffer, i * sizeof(char), 0);
                }
                var options = new OpenFileName
                {
                    StructSize = Marshal.SizeOf<OpenFileName>(),
                    Owner = OwnerWindow(),
                    // Double-terminated pairs of label and pattern; the
                    // marshaller adds the last terminator.
                    Filter = $"{label}\0{pattern}\0All files\0*.*\0",
                    FilterIndex = 1,
                    File = buffer,
                    MaxFile = 1024,
                    InitialDir = Directory.Exists(startIn) ? startIn : null,
                    Title = title,
                    // NoChangeDir: everything this program reads is found
                    // relative to the working directory it set at startup.
                    Flags = _fileMustExist | _pathMustExist | _noChangeDir
                        | _explorer | _hideReadOnly
                };
                if (GetOpenFileNameW(ref options))
                {
                    return new FilePick(Marshal.PtrToStringUni(buffer), null);
                }
                int error = CommDlgExtendedError();
                return new FilePick(null, error == 0
                    ? null
                    : $"The file chooser failed (comdlg32 error 0x{error:X}).");
            }
            finally
            {
                Marshal.FreeHGlobal(buffer);
            }
        }

        /// <summary>
        /// The game window, so the dialog is drawn over it rather than behind
        /// it -- which for a fullscreen window is the same complaint again.
        ///
        /// Only the desktop has one to name: the Android head leaves Shell.cs
        /// out of the build entirely, so this cannot so much as mention it.
        /// </summary>
        private static IntPtr OwnerWindow()
        {
#if MPHREAD_SHELL
            try
            {
                RenderWindow? window = Shell.Window;
                if (window == null)
                {
                    return IntPtr.Zero;
                }
                unsafe
                {
                    return GLFW.GetWin32Window(window.WindowPtr);
                }
            }
            catch (Exception)
            {
                // An unowned dialog is worse than an owned one, not a failure.
            }
#endif
            return IntPtr.Zero;
        }

        // -------------------------------------------------------------- Unix

        /// <summary>zenity or kdialog, whichever this desktop has.</summary>
        private static string? Helper()
        {
            return Which("zenity") ?? Which("kdialog") ?? Which("qarma");
        }

        private static FilePick LinuxOpen(string title, string label, string pattern,
            string? startIn)
        {
            string? helper = Helper();
            if (helper == null)
            {
                return new FilePick(null, "No file chooser on this system. Install zenity "
                    + "or kdialog, or run the text launcher (-launcher -text) and type the "
                    + "path.");
            }
            string name = Path.GetFileName(helper);
            var args = new System.Collections.Generic.List<string>();
            if (name == "kdialog")
            {
                args.Add("--title");
                args.Add(title);
                args.Add("--getopenfilename");
                args.Add(Directory.Exists(startIn) ? startIn! : ".");
                args.Add($"{pattern}|{label}");
            }
            else
            {
                args.Add("--file-selection");
                args.Add("--title=" + title);
                args.Add($"--file-filter={label} ({pattern}) | {pattern}");
                args.Add("--file-filter=All files | *");
                if (Directory.Exists(startIn))
                {
                    args.Add("--filename=" + startIn!.TrimEnd('/') + "/");
                }
            }
            return Run(helper, args);
        }

        [SupportedOSPlatform("macos")]
        private static FilePick MacOpen(string title, string extension)
        {
            var args = new System.Collections.Generic.List<string>
            {
                "-e", "tell application \"System Events\" to activate",
                "-e", $"POSIX path of (choose file with prompt \"{title}\" "
                    + $"of type {{\"{extension}\"}})"
            };
            return Run("/usr/bin/osascript", args);
        }

        private static FilePick Run(string program, System.Collections.Generic.List<string> args)
        {
            var info = new ProcessStartInfo(program)
            {
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                UseShellExecute = false
            };
            foreach (string arg in args)
            {
                info.ArgumentList.Add(arg);
            }
            using Process? process = Process.Start(info);
            if (process == null)
            {
                return new FilePick(null, $"{Path.GetFileName(program)} would not start.");
            }
            string output = process.StandardOutput.ReadToEnd();
            process.StandardError.ReadToEnd();
            process.WaitForExit();
            // Every one of them answers a cancel with a non-zero exit and an
            // empty line, which is nothing to report.
            output = output.Trim();
            if (process.ExitCode != 0 || output.Length == 0)
            {
                return new FilePick(null, null);
            }
            return new FilePick(output, null);
        }

        private static string? Which(string program)
        {
            string paths = Environment.GetEnvironmentVariable("PATH") ?? "";
            foreach (string directory in paths.Split(Path.PathSeparator,
                StringSplitOptions.RemoveEmptyEntries))
            {
                try
                {
                    string candidate = Path.Combine(directory, program);
                    if (File.Exists(candidate))
                    {
                        return candidate;
                    }
                }
                catch (ArgumentException)
                {
                    // A malformed PATH entry is one directory to skip.
                }
            }
            return null;
        }
    }
}
