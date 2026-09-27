#include "GlfwPathCheck.hpp"

#if defined(MPHREAD_SHELL)

#include "../Launcher/Portable/GameFiles.hpp"
#include "../ThumbnailCapture.hpp"
#include "../../Formats/Formats.hpp"
#include "../../NativeRuntime/System/Console.hpp"
#include "../../NativeRuntime/System/ExceptionText.hpp"
#include "../../NativeRuntime/System/Exceptions.hpp"
#include "../../NativeRuntime/System/Guid.hpp"
#include "../../NativeRuntime/System/IO.hpp"
#include "../../NativeRuntime/System/Runtime.hpp"
#include "../../Program.hpp"

#include <exception>
#include <string>

namespace MphRead::Mods::Diagnostics
{
    namespace Runtime = ::MphRead::NativeRuntime;

    int GlfwPathCheck::Run()
    {
        const std::string previousDirectory = Runtime::EnvironmentCurrentDirectory();
        const std::string previousRoot = Launcher::GameFiles::Root();
        std::string fixture = Runtime::DirectoryCreateTempSubdirectory("fruity-extraction-");
        int result = 0;
        try
        {
            Runtime::DirectorySetCurrentDirectory(fixture);
            // macOS can canonicalize /var to /private/var in getcwd.
            fixture = Runtime::EnvironmentCurrentDirectory();
            Launcher::GameFiles::Root(fixture);
            Runtime::DirectoryCreateDirectory(Runtime::PathCombine(fixture, "files", Ver::AMHE1));
            Runtime::FileWriteAllText("paths.txt", Program::Version.ToString() + "\n"
                + Ver::AMHE1 + "=files/" + Ver::AMHE1 + "\n");
            if (const std::optional<std::string> before = Launcher::GameFiles::Problem();
                before.has_value())
            {
                throw System::InvalidOperationException("Invalid path fixture: " + *before);
            }

            // NativeWindowSettings initializes GLFW/monitors, but does not
            // create a GL context. In a .app this used to switch to Resources.
            const RendererPlatform::WindowSettings settings
                = ThumbnailCapture::WindowSettings(64, 64);
            const bool mac = Runtime::IsMacOS();
            const int expectedMajor = mac ? 2 : 3;
            const int expectedMinor = mac ? 1 : 2;
            const auto expectedProfile = mac
                ? RendererPlatform::WindowSettings::ContextProfile::Any
                : RendererPlatform::WindowSettings::ContextProfile::Compatability;
            if (settings.ApiMajor != expectedMajor || settings.ApiMinor != expectedMinor
                || settings.Profile != expectedProfile
                || settings.Flags != RendererPlatform::WindowSettings::ContextFlags::Default
                || settings.StartVisible)
            {
                throw System::InvalidOperationException(
                    "Thumbnail worker requested an incompatible GL context.");
            }
            const std::string profile = mac ? "Any" : "Compatability";
            Runtime::ConsoleWriteLine("Thumbnail context policy passed: "
                + std::to_string(expectedMajor) + "." + std::to_string(expectedMinor)
                + ", " + profile + ".");
            if (Runtime::EnvironmentCurrentDirectory() != fixture)
            {
                throw System::InvalidOperationException("GLFW changed the extraction working directory.");
            }
            if (const std::optional<std::string> after = Launcher::GameFiles::Problem();
                after.has_value())
            {
                throw System::InvalidOperationException(
                    "Extracted files disappeared after GLFW initialization: " + *after);
            }
            Runtime::ConsoleWriteLine("GLFW extraction path check passed.");
        }
        catch (const std::exception&)
        {
            Runtime::ConsoleErrorWriteLine("[glfwpathcheck] "
                + Runtime::ExceptionToString(std::current_exception()));
            result = 1;
        }
        Runtime::DirectorySetCurrentDirectory(previousDirectory);
        Launcher::GameFiles::Root(previousRoot);
        Paths::UpdatePaths();
        Runtime::DirectoryDelete(fixture, true);
        return result;
    }
}

#endif
