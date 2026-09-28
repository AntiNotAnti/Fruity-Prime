#include "AppPaths.hpp"

#include "../Branding.hpp"
#include "../../NativeRuntime/System/IO.hpp"
#include "../../NativeRuntime/System/Runtime.hpp"

#include <memory>

namespace MphRead::Mods::Platform
{
    std::string AppPaths::ExecutableDirectory()
    {
        return ::MphRead::NativeRuntime::AppContextBaseDirectory();
    }

    std::string AppPaths::ResourceDirectory()
    {
        const ::MphRead::NativeRuntime::DirectoryInfo executable(ExecutableDirectory());
        if (!::MphRead::NativeRuntime::IsMacOS() || executable.Name() != "MacOS")
        {
            return ExecutableDirectory();
        }

        const std::shared_ptr<::MphRead::NativeRuntime::DirectoryInfo> parent = executable.Parent();
        if (parent == nullptr || parent->Name() != "Contents")
        {
            return ExecutableDirectory();
        }

        const std::shared_ptr<::MphRead::NativeRuntime::DirectoryInfo> grandparent = parent->Parent();
        return grandparent != nullptr && grandparent->Extension() == ".app"
            ? ::MphRead::NativeRuntime::PathCombine(parent->FullName(), "Resources")
            : ExecutableDirectory();
    }

    std::string AppPaths::Maps()
    {
        return ::MphRead::NativeRuntime::PathCombine(ResourceDirectory(), "maps");
    }

    std::string AppPaths::UserDataDirectory()
    {
        return ::MphRead::NativeRuntime::IsMacOS()
            ? ::MphRead::NativeRuntime::PathCombine(
                ::MphRead::NativeRuntime::EnvironmentUserProfile(),
                "Library", "Application Support", std::string(Branding::Name))
            : ExecutableDirectory();
    }

    std::string AppPaths::PathsFile()
    {
        return ::MphRead::NativeRuntime::PathCombine(UserDataDirectory(), "paths.txt");
    }

    void AppPaths::PrepareUserData()
    {
        if (::MphRead::NativeRuntime::IsMacOS())
        {
            ::MphRead::NativeRuntime::DirectoryCreateDirectory(UserDataDirectory());
        }
    }
}
