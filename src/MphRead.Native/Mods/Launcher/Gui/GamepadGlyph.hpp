#pragma once

#include "../../../NativeRuntime/Avalonia/Avalonia.hpp"
#include "../../Input/GamepadState.hpp"

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;

    // Drawn geometry keeps controller symbols independent of installed font coverage.
    class GamepadGlyph final : public Av::Controls::Control
    {
    public:
        [[nodiscard]] ::MphRead::Mods::Input::GamepadButtons Button() const noexcept { return _button; }
        void Button(::MphRead::Mods::Input::GamepadButtons value) noexcept { _button = value; }

        void Render(Av::Media::DrawingContext& context) override;

        static void Draw(Av::Media::DrawingContext& context, const Av::Rect& bounds,
            ::MphRead::Mods::Input::GamepadButtons button, const Av::Media::IBrushPtr& brush);

    private:
        ::MphRead::Mods::Input::GamepadButtons _button
            = ::MphRead::Mods::Input::GamepadButtons::None;
    };
}
