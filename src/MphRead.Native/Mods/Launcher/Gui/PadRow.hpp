#pragma once

#include "Tap.hpp"
#include "../../Input/PadBindings.hpp"
#include "../../Input/GamepadManager.hpp"
#include "../../../NativeRuntime/Avalonia/Avalonia.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;
    namespace PadInput = ::MphRead::Mods::Input;

    // One rebindable pad action: the action and its two slots, capture,
    // conflict resolution, and the help shown while listening.
    class PadRow final : public Av::Controls::Control
    {
    public:
        explicit PadRow(PadInput::PadAction action, double labelWidth = 160);

        Av::Event<PadRow&> Rebound;

        [[nodiscard]] PadInput::PadAction Action() const noexcept { return _action; }
        void Capture(PadInput::GamepadButtons pressed = PadInput::GamepadButtons::None);
        void Check();
        void Check(const PadInput::GamepadSnapshot& snapshot);

        void Render(Av::Media::DrawingContext& context) override;

    protected:
        void OnPointerPressed(Av::Input::PointerPressedEventArgs& e) override;
        void OnPointerMoved(Av::Input::PointerEventArgs& e) override;
        void OnPointerReleased(Av::Input::PointerReleasedEventArgs& e) override;
        void OnPointerCaptureLost(Av::Input::PointerCaptureLostEventArgs& e) override;
        void OnKeyDown(Av::Input::KeyEventArgs& e) override;
        void OnPointerEntered(Av::Input::PointerEventArgs& e) override;
        void OnPointerExited(Av::Input::PointerEventArgs& e) override;
        void OnLostFocus(Av::Input::FocusChangedEventArgs& e) override;
        void OnGotFocus(Av::Input::GotFocusEventArgs& e) override;
        void OnDetachedFromVisualTree() override;

    private:
        [[nodiscard]] Av::Rect Box() const;
        void Listen();
        void Choose(PadInput::GamepadButtons button);
        void Resolve();
        void Done();

        const PadInput::PadAction _action;
        const double _labelWidth;
        bool _listening = false;
        std::optional<std::string> _message;
        std::int32_t _slot = 0;
        std::int32_t _choice = 0;
        std::int32_t _pickIndex = 0;
        bool _picking = false;
        PadInput::GamepadButtons _pending = PadInput::GamepadButtons::None;
        PadInput::GamepadButtons _modifier = PadInput::GamepadButtons::None;
        std::int64_t _deviceRevision = 0;
        std::optional<std::string> _conflict;
        bool _hot = false;
        std::unique_ptr<Av::Threading::DispatcherTimer> _watch;
        Tap _tap{};
        PadInput::GamepadButtons _baseline = PadInput::GamepadButtons::None;
    };
}
