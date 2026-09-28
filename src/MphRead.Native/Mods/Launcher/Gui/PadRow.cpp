#include "PadRow.hpp"

#include "GuiTheme.hpp"
#include "TrackedText.hpp"
#include "../../../NativeRuntime/Avalonia/TopLevel.hpp"
#include "../../Input/GamepadManager.hpp"
#include "../../Input/GamepadOptions.hpp"
#include "../../Input/GamepadUiRouter.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"

#include <array>
#include <memory>
#include <string>
#include <string_view>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;
    using ::MphRead::NativeRuntime::MathClamp;
    using ::MphRead::NativeRuntime::MathMax;
    using ::MphRead::NativeRuntime::MathMin;

    namespace
    {
        constexpr auto& PickButtons = PadInput::GamepadButtonValues;
        constexpr std::array<std::string_view, 4> Resolutions{
            "Swap", "Replace", "Keep Both", "Cancel"
        };

    }

    PadRow::PadRow(PadInput::PadAction action, double labelWidth)
        : _action(action), _labelWidth(labelWidth)
    {
        Height(32);
        Focusable(true);
        Cursor(std::make_shared<Av::Input::Cursor>(Av::Input::StandardCursorType::Hand));
    }

    Av::Rect PadRow::Box() const
    {
        const Av::Rect bounds = Bounds();
        return {_labelWidth, 2, MathMax(60.0, bounds.Width - _labelWidth - 4), 28};
    }

    void PadRow::OnPointerPressed(Av::Input::PointerPressedEventArgs& e)
    {
        Focus();
        if (_conflict.has_value())
        {
            const Av::Point point = e.GetPosition(this);
            if (point.Y > 60)
            {
                _choice = MathClamp(static_cast<std::int32_t>(point.X / MathMax(1.0, Bounds().Width / 4)), 0, 3);
                Resolve();
            }
            e.Handled = true;
            return;
        }
        if (!_listening && Box().Contains(e.GetPosition(this)))
        {
            _tap.Press(e, *this);
        }
        e.Handled = true;
        Control::OnPointerPressed(e);
    }

    void PadRow::OnPointerMoved(Av::Input::PointerEventArgs& e)
    {
        _tap.Moved(e, *this);
        Control::OnPointerMoved(e);
    }

    void PadRow::OnPointerReleased(Av::Input::PointerReleasedEventArgs& e)
    {
        const Av::Point point = e.GetPosition(this);
        const Av::Rect box = Box();
        if (!_listening && _tap.Release(e, *this) && box.Contains(point))
        {
            _slot = point.X < box.Center().X ? 0 : 1;
            Listen();
        }
        Control::OnPointerReleased(e);
    }

    void PadRow::OnPointerCaptureLost(Av::Input::PointerCaptureLostEventArgs& e)
    {
        _tap.Cancel();
        Control::OnPointerCaptureLost(e);
    }

    void PadRow::OnKeyDown(Av::Input::KeyEventArgs& e)
    {
        if (_conflict.has_value())
        {
            if (e.Key == Av::Input::Key::Left) _choice = MathMax(0, _choice - 1);
            if (e.Key == Av::Input::Key::Right) _choice = MathMin(3, _choice + 1);
            if (e.Key == Av::Input::Key::Enter) Resolve();
            if (e.Key == Av::Input::Key::Escape) Done();
            e.Handled = true;
            InvalidateVisual();
            return;
        }
        if (!_listening)
        {
            if (e.Key == Av::Input::Key::Left || e.Key == Av::Input::Key::Right)
            {
                _slot = e.Key == Av::Input::Key::Left ? 0 : 1;
                InvalidateVisual();
                e.Handled = true;
                return;
            }
            if (e.Key == Av::Input::Key::Enter || e.Key == Av::Input::Key::Space)
            {
                Listen();
                e.Handled = true;
            }
            Control::OnKeyDown(e);
            return;
        }
        e.Handled = true;
        if (e.Key == Av::Input::Key::Escape)
        {
            Done();
            return;
        }
        if (e.Key == Av::Input::Key::Back || e.Key == Av::Input::Key::Delete)
        {
            PadInput::PadBindings::SetSlot(_action, _slot, PadInput::GamepadButtons::None);
            Done();
        }
    }

    void PadRow::Capture(PadInput::GamepadButtons pressed)
    {
        Listen();
        if (pressed != PadInput::GamepadButtons::None)
        {
            for (const PadInput::GamepadButtons button : PickButtons)
            {
                if (button != PadInput::GamepadButtons::None && (pressed & button) != PadInput::GamepadButtons::None)
                {
                    Choose(button);
                    break;
                }
            }
        }
    }

    void PadRow::Listen()
    {
        _listening = true;
        _message.reset();
        _modifier = PadInput::GamepadOptions::BindingModifier();
        Height(72);
        PadInput::GamepadContexts::Capturing(true);
        // Read the same published state consumed by gameplay and menus. A
        // capture can begin inside a native keyboard or mouse event callback.
        const PadInput::GamepadSnapshot snapshot = PadInput::GamepadManager::Snapshot();
        _deviceRevision = snapshot.Revision;
        _baseline = snapshot.State.Buttons;
        if (Av::TopLevel* top = Av::TopLevel::GetTopLevel(this))
        {
            top->UpdateLayout();
        }
        BringIntoView();
        if (_watch != nullptr)
        {
            _watch->Stop();
            _watch.reset();
        }
        const std::shared_ptr<PadRow> self = std::static_pointer_cast<PadRow>(shared_from_this());
        _watch = std::make_unique<Av::Threading::DispatcherTimer>(Av::Threading::TimeSpan(0.030),
            Av::Threading::DispatcherPriority::Input, [self] { self->Check(); });
        InvalidateVisual();
    }

    void PadRow::Check()
    {
        Check(PadInput::GamepadManager::Snapshot());
    }

    void PadRow::Check(const PadInput::GamepadSnapshot& snapshot)
    {
        if (!_listening)
        {
            return;
        }
        // The UI navigation pump supplies a new snapshot every frame. The
        // timer remains a fallback for secondary hosts and short captures.
        if (!PadInput::GamepadContexts::Focused() || !snapshot.State.Connected || snapshot.Revision != _deviceRevision)
        {
            _message = !PadInput::GamepadContexts::Focused() ? "Focus lost - try again"
                : !snapshot.State.Connected ? "Connect a controller, then try again" : "Controller changed - try again";
            Done();
            return;
        }
        const PadInput::GamepadButtons pressed = snapshot.State.Buttons & ~_baseline;
        _baseline = snapshot.State.Buttons;
        if (pressed == PadInput::GamepadButtons::None)
        {
            return;
        }
        if (_conflict.has_value())
        {
            if ((pressed & PadInput::GamepadButtons::DpadLeft) != PadInput::GamepadButtons::None)
            {
                _choice = MathMax(0, _choice - 1);
            }
            if ((pressed & PadInput::GamepadButtons::DpadRight) != PadInput::GamepadButtons::None)
            {
                _choice = MathMin(3, _choice + 1);
            }
            if ((pressed & PadInput::GamepadButtons::A) != PadInput::GamepadButtons::None)
            {
                Resolve();
            }
            else if ((pressed & PadInput::GamepadButtons::B) != PadInput::GamepadButtons::None)
            {
                Done();
            }
            InvalidateVisual();
            return;
        }
        if ((pressed & PadInput::GamepadButtons::B) != PadInput::GamepadButtons::None)
        {
            Done();
            return;
        }
        if (_picking)
        {
            if ((pressed & PadInput::GamepadButtons::DpadLeft) != PadInput::GamepadButtons::None)
            {
                _pickIndex = (_pickIndex + static_cast<std::int32_t>(PickButtons.size()) - 1)
                    % static_cast<std::int32_t>(PickButtons.size());
            }
            if ((pressed & PadInput::GamepadButtons::DpadRight) != PadInput::GamepadButtons::None)
            {
                _pickIndex = (_pickIndex + 1) % static_cast<std::int32_t>(PickButtons.size());
            }
            if ((pressed & PadInput::GamepadButtons::A) != PadInput::GamepadButtons::None)
            {
                Choose(PickButtons[static_cast<std::size_t>(_pickIndex)]);
            }
            InvalidateVisual();
            return;
        }
        // The picker also lets the capture commands themselves be assigned.
        if ((pressed & PadInput::GamepadButtons::Start) != PadInput::GamepadButtons::None)
        {
            _picking = true;
            _pickIndex = 1;
            InvalidateVisual();
            return;
        }
        if ((pressed & PadInput::GamepadButtons::Back) != PadInput::GamepadButtons::None)
        {
            PadInput::PadBindings::SetSlot(_action, _slot, PadInput::GamepadButtons::None);
            Done();
            return;
        }
        for (const PadInput::GamepadButtons button : PickButtons)
        {
            if (button == PadInput::GamepadButtons::None || (pressed & button) == PadInput::GamepadButtons::None)
            {
                continue;
            }
            Choose(button);
            return;
        }
    }

    void PadRow::Choose(PadInput::GamepadButtons button)
    {
        if (button != PadInput::GamepadButtons::None && button == _modifier)
        {
            return;
        }
        _picking = false;
        const std::vector<PadInput::PadAction> conflicts = PadInput::PadBindings::Conflicts(_action, button, _modifier);
        if (conflicts.empty())
        {
            PadInput::PadBindings::SetSlot(_action, _slot, button, _modifier);
            Done();
        }
        else
        {
            _pending = button;
            _choice = 0;
            std::string message = _modifier == PadInput::GamepadButtons::None
                ? std::string{} : PadInput::PadBindings::ButtonName(_modifier) + " + ";
            message += PadInput::PadBindings::ButtonName(button) + " is assigned to ";
            for (std::size_t i = 0; i < conflicts.size(); ++i)
            {
                if (i != 0)
                {
                    message += " / ";
                }
                message += PadInput::PadBindings::Name(conflicts[i]);
            }
            _conflict = std::move(message);
            Height(108);
            BringIntoView();
            InvalidateVisual();
        }
    }

    void PadRow::Resolve()
    {
        PadInput::PadBindings::Assign(_action, _slot, _pending,
            Resolutions[static_cast<std::size_t>(_choice)], _modifier);
        Done();
    }

    void PadRow::Done()
    {
        _listening = false;
        PadInput::GamepadContexts::Capturing(false);
        _conflict.reset();
        _picking = false;
        Height(32);
        if (_watch != nullptr)
        {
            _watch->Stop();
            _watch.reset();
        }
        InvalidateVisual();
        Rebound(*this);
    }

    void PadRow::OnPointerEntered(Av::Input::PointerEventArgs& e)
    {
        _hot = true;
        InvalidateVisual();
        Control::OnPointerEntered(e);
    }

    void PadRow::OnPointerExited(Av::Input::PointerEventArgs& e)
    {
        _hot = false;
        InvalidateVisual();
        Control::OnPointerExited(e);
    }

    void PadRow::OnLostFocus(Av::Input::FocusChangedEventArgs& e)
    {
        if (_listening)
        {
            Done();
        }
        Control::OnLostFocus(e);
    }

    void PadRow::OnGotFocus(Av::Input::GotFocusEventArgs& e)
    {
        InvalidateVisual();
        Control::OnGotFocus(e);
    }

    void PadRow::OnDetachedFromVisualTree()
    {
        if (_watch != nullptr)
        {
            _watch->Stop();
            _watch.reset();
        }
        _listening = false;
        PadInput::GamepadContexts::Capturing(false);
        _conflict.reset();
        _picking = false;
        Height(32);
        Control::OnDetachedFromVisualTree();
    }

    void PadRow::Render(Media::DrawingContext& context)
    {
        const Av::Rect bounds = Bounds();
        if (bounds.Width <= 0 || bounds.Height <= 0)
        {
            return;
        }
        context.FillRectangle(Media::Brushes::Transparent(), Av::Rect(0, 0, bounds.Width, bounds.Height));
        const Media::FormattedText label = TrackedText::Make(PadInput::PadBindings::Name(_action), 12, true,
            GuiTheme::TextBrush);
        context.DrawText(label, Av::Point(4, (32 - label.Height()) / 2));

        const Av::Rect box = Box();
        const Media::Color edge = _listening ? GuiTheme::Warm
            : IsFocused() || _hot ? GuiTheme::Accent : GuiTheme::Edge;
        context.DrawRectangle(GuiTheme::PanelLightBrush,
            std::make_shared<Media::Pen>(std::make_shared<Media::SolidColorBrush>(edge), 1),
            Av::RoundedRect(box, 4));

        std::string text;
        if (_conflict.has_value())
        {
            text = "Choose how to use " + PadInput::PadBindings::Describe(_pending);
        }
        else if (_picking)
        {
            text = "< " + PadInput::PadBindings::Describe(PickButtons[static_cast<std::size_t>(_pickIndex)])
                + " >  Accept / Back";
        }
        else if (_listening)
        {
            text = _modifier == PadInput::GamepadButtons::None ? "Press a button"
                : "Choose a button for " + PadInput::PadBindings::ButtonName(_modifier) + " + button";
        }
        else if (_message.has_value())
        {
            text = *_message;
        }
        else
        {
            text = (_slot == 0 && IsFocused() ? "> " : "") + std::string("Primary: ")
                + PadInput::PadBindings::DescribeSlot(_action, 0) + "    "
                + (_slot == 1 && IsFocused() ? "> " : "") + "Secondary: "
                + PadInput::PadBindings::DescribeSlot(_action, 1);
        }

        if (_listening && !_conflict.has_value())
        {
            const std::string hintText = PadInput::PadBindings::ButtonName(PadInput::GamepadButtons::B)
                + " cancel   " + PadInput::PadBindings::ButtonName(PadInput::GamepadButtons::Back)
                + " clear   " + PadInput::PadBindings::ButtonName(PadInput::GamepadButtons::Start) + " choose button";
            Media::FormattedText hint = TrackedText::Make(hintText, 11, true, GuiTheme::TextDimBrush);
            hint.MaxTextWidth(MathMax(20.0, bounds.Width - 8));
            hint.MaxTextHeight(28);
            context.DrawText(hint, Av::Point(4, 40));
        }
        if (_conflict.has_value())
        {
            Media::FormattedText note = TrackedText::Make(*_conflict, 11, true, GuiTheme::TextBrush);
            note.MaxTextWidth(MathMax(20.0, bounds.Width - 8));
            note.MaxTextHeight(28);
            note.Trimming(Media::TextTrimming::CharacterEllipsis);
            context.DrawText(note, Av::Point(4, 36));
            for (std::int32_t i = 0; i < 4; ++i)
            {
                const bool chosen = _choice == i;
                const std::string optionText = (chosen ? "> " : "") + std::string(Resolutions[static_cast<std::size_t>(i)]);
                const Media::IBrushPtr optionBrush = chosen ? GuiTheme::AccentBrush : GuiTheme::TextBrush;
                const Media::FormattedText option = TrackedText::Make(optionText, 12, true, optionBrush);
                context.DrawText(option, Av::Point(i * bounds.Width / 4 + 4, 74));
            }
        }
        Media::FormattedText value = TrackedText::Make(text, 12, true,
            std::make_shared<Media::SolidColorBrush>(_listening ? GuiTheme::Warm : GuiTheme::Text));
        value.MaxTextWidth(MathMax(20.0, box.Width - 12));
        value.MaxTextHeight(box.Height);
        value.Trimming(Media::TextTrimming::CharacterEllipsis);
        context.DrawText(value, Av::Point(box.X + (box.Width - value.Width()) / 2,
            box.Y + (box.Height - value.Height()) / 2));
    }
}
