#pragma once

#include "../../../NativeRuntime/Avalonia/Avalonia.hpp"

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;

    class GamepadProfilePanel final : public Av::Controls::StackPanel
    {
    public:
        explicit GamepadProfilePanel(std::function<void()> changed);
    };
}
