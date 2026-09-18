using System;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;

namespace MphRead
{
    internal static class ConsoleSetup
    {
        [DllImport("kernel32.dll")]
        private static extern bool GetConsoleMode(IntPtr hConsoleHandle, out uint lpMode);

        [DllImport("kernel32.dll")]
        private static extern bool SetConsoleMode(IntPtr hConsoleHandle, uint dwMode);

        [DllImport("kernel32.dll", SetLastError = true)]
        private static extern IntPtr GetStdHandle(int nStdHandle);

        [DllImport("kernel32.dll")]
        public static extern uint GetLastError();

        /// <summary>
        /// Where the shell was when this was launched.
        ///
        /// <see cref="Run"/> moves the process to the directory the binary is
        /// in (the user-data directory on macOS), where paths.txt lives, so
        /// by the time a command line is parsed, a relative path the player
        /// typed no longer means what they meant. Anything taking a path from
        /// the command line resolves it against this instead.
        /// </summary>
        public static string LaunchDirectory { get; private set; } = Directory.GetCurrentDirectory();

        public static void Run()
        {
            CultureInfo.CurrentCulture = CultureInfo.InvariantCulture;
            LaunchDirectory = Directory.GetCurrentDirectory();
            // Upstream reads/writes settings, saves and extraction output relative
            // to cwd. On macOS this must be writable and outside the signed app.
            Mods.Platform.AppPaths.PrepareUserData();
            Directory.SetCurrentDirectory(Mods.Platform.AppPaths.UserDataDirectory);
            if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
            {
                IntPtr iStdOut = GetStdHandle(-11);
                GetConsoleMode(iStdOut, out uint outConsoleMode);
                outConsoleMode |= 4;
                SetConsoleMode(iStdOut, outConsoleMode);
            }
        }
    }
}
