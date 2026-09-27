#pragma once

#include <exception>
#include <string>

namespace MphRead::Mods::Diagnostics
{
    class PlatformDiagnostics final
    {
    public:
        PlatformDiagnostics() = delete;

        static void Start();
        static void Report(std::string library, const std::exception& exception);
        static void Report(std::string library, std::exception_ptr exception);

    private:
        [[nodiscard]] static std::string Describe(const std::string& path);
        static void Persist(const std::string& message, bool append);
        [[nodiscard]] static std::string LogPath();
    };
}
