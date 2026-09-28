#pragma once

#include "../../../NativeRuntime/Avalonia/Avalonia.hpp"
#include "../../../Mods/Input/GamepadManager.hpp"

#include <string>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;
    namespace PadInput = ::MphRead::Mods::Input;

    // A live check in Settings lets players distinguish hardware layout from action bindings.
    class GamepadMonitor final : public Av::Controls::Control
    {
    public:
        GamepadMonitor();

        [[nodiscard]] const std::string& Status() const noexcept { return _status; }
        void Refresh();
        void Render(Av::Media::DrawingContext& context) override;

    private:
        Av::Threading::DispatcherTimer _timer;
        PadInput::GamepadState _state{};
        PadInput::GamepadState _raw{};
        std::string _status;
        std::string _buttons;
        std::string _actions;
    };
}
