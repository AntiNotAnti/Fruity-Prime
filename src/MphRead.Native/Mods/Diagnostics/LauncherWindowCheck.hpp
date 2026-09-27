#pragma once

#include <string>

namespace MphRead
{
    class RenderWindow;
}

namespace MphRead::Mods::Diagnostics
{
    class LauncherWindowCheck final
    {
    public:
        LauncherWindowCheck() = delete;

        [[nodiscard]] static int Run();
        static void AfterDraw(MphRead::RenderWindow& window);

    private:
        static void Link(const std::string& vertex, const std::string& fragment);

        static bool _active;
        static bool _passed;
        static int _frames;
    };
}
