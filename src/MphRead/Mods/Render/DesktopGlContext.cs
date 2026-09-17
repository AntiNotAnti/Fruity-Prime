using System;
using OpenTK.Mathematics;
using OpenTK.Windowing.Common;
using OpenTK.Windowing.Desktop;

namespace MphRead.Mods.Render
{
    internal static class DesktopGlContext
    {
        public static NativeWindowSettings Settings()
        {
            // Install before NativeWindowSettings initializes GLFW/monitors.
            // OpenTK's default handler throws across native frames and aborts
            // on macOS; NativeWindow checks a failed CreateWindow itself and
            // throws safely after returning to managed code.
            GLFWProvider.SetErrorCallback((code, description) =>
                Console.Error.WriteLine($"[window] GLFW {code}: {description}"));
            return new NativeWindowSettings
            {
                ClientSize = new Vector2i(1280, 768),
                Title = Branding.Name,
                // Legacy immediate mode/GLSL 1.20 need the 2.1 context on macOS;
                // Apple's 3.2+ contexts are core-only.
                Profile = OperatingSystem.IsMacOS() ? ContextProfile.Any : ContextProfile.Compatability,
                Flags = ContextFlags.Default,
                APIVersion = OperatingSystem.IsMacOS() ? new Version(2, 1) : new Version(3, 2),
                StartVisible = false
            };
        }
    }
}
