#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace MphRead::Mods::Launcher::Gui
{
    class UiBench final
    {
    public:
        UiBench() = delete;

        static bool FreeFrames;
        static bool Slow;
        static std::optional<std::string> OnlySize;
        static std::optional<std::string> OnlyMove;
        static double ScaleOverride;
        static std::optional<std::string> Shot;
        static bool AsAndroid;

        [[nodiscard]] static std::int32_t Run(const std::optional<std::string>& what);

    private:
        static void Measure(const std::string& screen);
        static void Tail();
    };
}
