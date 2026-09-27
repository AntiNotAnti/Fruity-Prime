#include "KeyRow.hpp"

#include "FocusNavigator.hpp"
#include "GuiTheme.hpp"
#include "PadRow.hpp"
#include "SettingsView.hpp"
#include "TrackedText.hpp"
#include "../../../NativeRuntime/Avalonia/TopLevel.hpp"
#include "../../../NativeRuntime/System/Exceptions.hpp"
#include "../../Input/PadAction.hpp"
#include "../../Input/GamepadManager.hpp"
#include "../../InputSettings.hpp"

#include <algorithm>
#include <memory>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;
    namespace AvInput = ::MphRead::NativeRuntime::Avalonia::Input;
    namespace ModInput = ::MphRead::Mods::Input;
    using ::MphRead::NativeRuntime::MathMax;

    bool KeyRow::_anyListening = false;

    KeyRow::KeyRow(const ::MphRead::Mods::InputBindingProperty& property, double labelWidth)
        : _property(&property), _labelWidth(labelWidth)
    {
        Height(32);
        Focusable(true);
        Cursor(std::make_shared<AvInput::Cursor>(AvInput::StandardCursorType::Hand));
    }

    KeyRow::KeyRow(std::string label, Getter get, Setter set, double labelWidth)
        : _label(std::move(label)), _get(std::move(get)), _set(std::move(set)), _labelWidth(labelWidth)
    {
        Height(32);
        Focusable(true);
        Cursor(std::make_shared<AvInput::Cursor>(AvInput::StandardCursorType::Hand));
    }

    std::string_view KeyRow::BindingName() const noexcept
    {
        if (_property != nullptr)
        {
            return _property->Name;
        }
        return _label.has_value() ? std::string_view(*_label) : std::string_view{};
    }

    Av::Rect KeyRow::Box() const
    {
        const Av::Rect bounds = Bounds();
        return {_labelWidth, 2, MathMax(60.0, bounds.Width - _labelWidth - 4), bounds.Height - 4};
    }

    const ::MphRead::Mods::InputBindingProperty& KeyRow::RequireProperty() const
    {
        if (_property == nullptr)
        {
            throw ::System::NullReferenceException();
        }
        return *_property;
    }

    void KeyRow::OnPointerPressed(AvInput::PointerPressedEventArgs& e)
    {
        Focus();
        const AvInput::PointerPointProperties properties = e.GetCurrentPoint(this).Properties;
        if (!_listening)
        {
            // Listening begins on the release: a press that starts a scroll
            // down the Controls page must not activate every row it crosses.
            if (Box().Contains(e.GetPosition(this)))
            {
                _tap.Press(e, *this);
            }
            e.Handled = true;
            Control::OnPointerPressed(e);
            return;
        }

        std::optional<KeyRowMouseButton> button;
        switch (properties.PointerUpdateKind)
        {
        case AvInput::PointerUpdateKind::LeftButtonPressed:
            button = KeyRowMouseButton::Left;
            break;
        case AvInput::PointerUpdateKind::RightButtonPressed:
            button = KeyRowMouseButton::Right;
            break;
        case AvInput::PointerUpdateKind::MiddleButtonPressed:
            button = KeyRowMouseButton::Middle;
            break;
        case AvInput::PointerUpdateKind::XButton1Pressed:
            button = KeyRowMouseButton::Button4;
            break;
        case AvInput::PointerUpdateKind::XButton2Pressed:
            button = KeyRowMouseButton::Button5;
            break;
        default:
            break;
        }
        if (button.has_value() && _property != nullptr)
        {
            ::MphRead::Mods::InputSettings::Rebind(*_property,
                ::MphRead::Entities::ButtonType::Mouse,
                KeyRowGlfwKey::Unknown, *button);
            Done();
        }
        e.Handled = true;
        Control::OnPointerPressed(e);
    }

    void KeyRow::OnPointerMoved(AvInput::PointerEventArgs& e)
    {
        _tap.Moved(e, *this);
        Control::OnPointerMoved(e);
    }

    void KeyRow::OnPointerReleased(AvInput::PointerReleasedEventArgs& e)
    {
        if (!_listening && _tap.Release(e, *this) && Box().Contains(e.GetPosition(this)))
        {
            SetListening(true);
            InvalidateVisual();
        }
        Control::OnPointerReleased(e);
    }

    void KeyRow::OnPointerCaptureLost(AvInput::PointerCaptureLostEventArgs& e)
    {
        _tap.Cancel();
        Control::OnPointerCaptureLost(e);
    }

    void KeyRow::OnPointerWheelChanged(AvInput::PointerWheelEventArgs& e)
    {
        if (_listening && e.Delta.Y != 0 && _property != nullptr)
        {
            ::MphRead::Mods::InputSettings::Rebind(*_property,
                e.Delta.Y > 0 ? ::MphRead::Entities::ButtonType::ScrollUp : ::MphRead::Entities::ButtonType::ScrollDown,
                KeyRowGlfwKey::Unknown, KeyRowMouseButton::Left);
            Done();
            e.Handled = true;
        }
        Control::OnPointerWheelChanged(e);
    }

    void KeyRow::OnPointerEntered(AvInput::PointerEventArgs& e)
    {
        _hot = true;
        InvalidateVisual();
        Control::OnPointerEntered(e);
    }

    void KeyRow::OnPointerExited(AvInput::PointerEventArgs& e)
    {
        _hot = false;
        InvalidateVisual();
        Control::OnPointerExited(e);
    }

    void KeyRow::OnKeyDown(AvInput::KeyEventArgs& e)
    {
        if (!_listening)
        {
            if (e.Key == AvInput::Key::Enter || e.Key == AvInput::Key::Space)
            {
                SetListening(true);
                InvalidateVisual();
                e.Handled = true;
            }
            Control::OnKeyDown(e);
            return;
        }

        // While listening every key belongs to this row, even keys normally
        // used to move focus or close the window.
        e.Handled = true;
        if (e.Key == AvInput::Key::Escape)
        {
            Done();
            return;
        }
        if (e.Key == AvInput::Key::Back || e.Key == AvInput::Key::Delete)
        {
            Assign(KeyRowGlfwKey::Unknown);
            Done();
            return;
        }
        if (const auto key = Translate(e.Key); key.has_value())
        {
            Assign(*key);
            Done();
        }
    }

    void KeyRow::Assign(KeyRowGlfwKey key)
    {
        if (_set)
        {
            _set(key);
            return;
        }
        ::MphRead::Mods::InputSettings::Rebind(RequireProperty(),
            ::MphRead::Entities::ButtonType::Key,
            static_cast<::MphRead::Mods::InputKey>(key), KeyRowMouseButton::Left);
    }

    void KeyRow::SetListening(bool value)
    {
        if (_listening == value)
        {
            return;
        }
        _listening = value;
        _anyListening = value;
        if (value)
        {
            _controllerHint.reset();
            (void)_padEdges.Update(ModInput::GamepadManager::Snapshot());
        }
    }

    KeyRowGamepadButtons KeyRow::ControllerPress(const KeyRowGamepadSnapshot& snapshot)
    {
        return _padEdges.Update(snapshot);
    }

    void KeyRow::OpenControllerBinding(KeyRowGamepadButtons pressed)
    {
        std::optional<ModInput::PadAction> action;
        const std::string_view binding = BindingName();
        if (binding == "Shoot" || binding == "AltAttack") action = ModInput::PadAction::Shoot;
        else if (binding == "Jump" || binding == "Boost") action = ModInput::PadAction::Jump;
        else if (binding == "Zoom") action = ModInput::PadAction::Zoom;
        else if (binding == "Morph") action = ModInput::PadAction::Morph;
        else if (binding == "Scan") action = ModInput::PadAction::Scan;
        else if (binding == "ScanVisor") action = ModInput::PadAction::ScanVisor;
        else if (binding == "WeaponMenu") action = ModInput::PadAction::WeaponWheel;
        else if (binding == "Pause") action = ModInput::PadAction::Scoreboard;
        else if (binding == "NextWeapon") action = ModInput::PadAction::NextWeapon;
        else if (binding == "PrevWeapon") action = ModInput::PadAction::PrevWeapon;
        else if (binding == "Missile") action = ModInput::PadAction::Missile;
        else if (binding == "PowerBeam") action = ModInput::PadAction::PowerBeam;
        else if (binding == "Chat") action = ModInput::PadAction::Chat;
        else if (binding == "VoltDriver") action = ModInput::PadAction::VoltDriver;
        else if (binding == "Battlehammer") action = ModInput::PadAction::Battlehammer;
        else if (binding == "Imperialist") action = ModInput::PadAction::Imperialist;
        else if (binding == "Judicator") action = ModInput::PadAction::Judicator;
        else if (binding == "Magmaul") action = ModInput::PadAction::Magmaul;
        else if (binding == "ShockCoil") action = ModInput::PadAction::ShockCoil;
        else if (binding == "OmegaCannon") action = ModInput::PadAction::OmegaCannon;
        else if (binding == "AffinitySlot") action = ModInput::PadAction::AffinitySlot;

        SettingsView* settings = nullptr;
        for (Av::Visual* ancestor : GetVisualAncestors())
        {
            if (auto* candidate = dynamic_cast<SettingsView*>(ancestor); candidate != nullptr)
            {
                settings = candidate;
                break;
            }
        }
        SetListening(false);
        if (action.has_value() && settings != nullptr)
        {
            settings->ShowSection("Controls", 1);
            if (Av::TopLevel* top = Av::TopLevel::GetTopLevel(settings))
            {
                top->UpdateLayout();
            }
            PadRow* row = nullptr;
            for (Av::Visual* descendant : settings->GetVisualDescendants())
            {
                if (auto* candidate = dynamic_cast<PadRow*>(descendant);
                    candidate != nullptr && candidate->Action() == *action)
                {
                    row = candidate;
                    break;
                }
            }
            if (row == nullptr)
            {
                throw ::System::InvalidOperationException();
            }
            FocusNavigator::Focus(row);
            row->Capture(pressed);
            return;
        }
        _controllerHint = "Keyboard only; configure sticks under Gamepad";
        InvalidateVisual();
    }

    void KeyRow::Done()
    {
        SetListening(false);
        InvalidateVisual();
        Rebound(*this);
    }

    void KeyRow::OnLostFocus(AvInput::FocusChangedEventArgs& e)
    {
        SetListening(false);
        _controllerHint.reset();
        InvalidateVisual();
        Control::OnLostFocus(e);
    }

    void KeyRow::OnGotFocus(AvInput::GotFocusEventArgs& e)
    {
        InvalidateVisual();
        Control::OnGotFocus(e);
    }

    std::optional<KeyRowGlfwKey> KeyRow::Translate(AvInput::Key key) noexcept
    {
        const std::int32_t value = static_cast<std::int32_t>(key);
        if (key >= AvInput::Key::A && key <= AvInput::Key::Z)
        {
            return static_cast<KeyRowGlfwKey>(65 + value - static_cast<std::int32_t>(AvInput::Key::A));
        }
        if (key >= AvInput::Key::D0 && key <= AvInput::Key::D9)
        {
            return static_cast<KeyRowGlfwKey>(48 + value - static_cast<std::int32_t>(AvInput::Key::D0));
        }
        if (key >= AvInput::Key::NumPad0 && key <= AvInput::Key::NumPad9)
        {
            return static_cast<KeyRowGlfwKey>(320 + value - static_cast<std::int32_t>(AvInput::Key::NumPad0));
        }
        if (key >= AvInput::Key::F1 && key <= AvInput::Key::F12)
        {
            return static_cast<KeyRowGlfwKey>(290 + value - static_cast<std::int32_t>(AvInput::Key::F1));
        }
        switch (key)
        {
        case AvInput::Key::Space: return static_cast<KeyRowGlfwKey>(32);
        case AvInput::Key::Tab: return static_cast<KeyRowGlfwKey>(258);
        case AvInput::Key::Enter: return static_cast<KeyRowGlfwKey>(257);
        case AvInput::Key::LeftShift: return static_cast<KeyRowGlfwKey>(340);
        case AvInput::Key::RightShift: return static_cast<KeyRowGlfwKey>(344);
        case AvInput::Key::LeftCtrl: return static_cast<KeyRowGlfwKey>(341);
        case AvInput::Key::RightCtrl: return static_cast<KeyRowGlfwKey>(345);
        case AvInput::Key::LeftAlt: return static_cast<KeyRowGlfwKey>(342);
        case AvInput::Key::RightAlt: return static_cast<KeyRowGlfwKey>(346);
        case AvInput::Key::Left: return static_cast<KeyRowGlfwKey>(263);
        case AvInput::Key::Right: return static_cast<KeyRowGlfwKey>(262);
        case AvInput::Key::Up: return static_cast<KeyRowGlfwKey>(265);
        case AvInput::Key::Down: return static_cast<KeyRowGlfwKey>(264);
        case AvInput::Key::Insert: return static_cast<KeyRowGlfwKey>(260);
        case AvInput::Key::Home: return static_cast<KeyRowGlfwKey>(268);
        case AvInput::Key::End: return static_cast<KeyRowGlfwKey>(269);
        case AvInput::Key::PageUp: return static_cast<KeyRowGlfwKey>(266);
        case AvInput::Key::PageDown: return static_cast<KeyRowGlfwKey>(267);
        case AvInput::Key::CapsLock: return static_cast<KeyRowGlfwKey>(280);
        case AvInput::Key::OemMinus: return static_cast<KeyRowGlfwKey>(45);
        case AvInput::Key::OemPlus: return static_cast<KeyRowGlfwKey>(61);
        case AvInput::Key::OemOpenBrackets: return static_cast<KeyRowGlfwKey>(91);
        case AvInput::Key::OemCloseBrackets: return static_cast<KeyRowGlfwKey>(93);
        case AvInput::Key::OemSemicolon: return static_cast<KeyRowGlfwKey>(59);
        case AvInput::Key::OemQuotes: return static_cast<KeyRowGlfwKey>(39);
        case AvInput::Key::OemComma: return static_cast<KeyRowGlfwKey>(44);
        case AvInput::Key::OemPeriod: return static_cast<KeyRowGlfwKey>(46);
        case AvInput::Key::OemQuestion: return static_cast<KeyRowGlfwKey>(47);
        case AvInput::Key::OemBackslash:
        case AvInput::Key::OemPipe: return static_cast<KeyRowGlfwKey>(92);
        case AvInput::Key::OemTilde: return static_cast<KeyRowGlfwKey>(96);
        case AvInput::Key::Add: return static_cast<KeyRowGlfwKey>(334);
        case AvInput::Key::Subtract: return static_cast<KeyRowGlfwKey>(333);
        case AvInput::Key::Multiply: return static_cast<KeyRowGlfwKey>(332);
        case AvInput::Key::Divide: return static_cast<KeyRowGlfwKey>(331);
        default: return std::nullopt;
        }
    }

    void KeyRow::Render(Media::DrawingContext& context)
    {
        const Av::Rect bounds = Bounds();
        if (bounds.Width <= 0 || bounds.Height <= 0)
        {
            return;
        }
        context.FillRectangle(Media::Brushes::Transparent(), Av::Rect(0, 0, bounds.Width, bounds.Height));
        const std::string labelText = _label.has_value()
            ? *_label
            : ::MphRead::Mods::InputSettings::ActionName(RequireProperty());
        const Media::FormattedText label = TrackedText::Make(labelText, 12, true, GuiTheme::TextBrush);
        context.DrawText(label, Av::Point(4, (bounds.Height - label.Height()) / 2));

        const Av::Rect box = Box();
        const Media::Color edge = _listening ? GuiTheme::Warm
            : IsFocused() || _hot ? GuiTheme::Accent : GuiTheme::Edge;
        context.DrawRectangle(GuiTheme::PanelLightBrush,
            std::make_shared<Media::Pen>(std::make_shared<Media::SolidColorBrush>(edge), 1),
            Av::RoundedRect(box, 4));

        std::string text;
        if (_listening)
        {
            text = _get ? "press a key" : "press a key, a mouse button or the wheel";
        }
        else if (_controllerHint.has_value())
        {
            text = *_controllerHint;
        }
        else if (_get)
        {
            if (_get() == KeyRowGlfwKey::Unknown)
            {
                text = "none";
            }
            else
            {
                text = ::MphRead::Mods::InputSettings::KeyName(static_cast<::MphRead::Mods::InputKey>(_get()));
            }
        }
        else
        {
            text = ::MphRead::Mods::InputSettings::Describe(
                ::MphRead::Mods::InputSettings::Bind(RequireProperty()));
        }
        const Media::IBrushPtr valueBrush = std::make_shared<Media::SolidColorBrush>(
            _listening ? GuiTheme::Warm : GuiTheme::Text);
        Media::FormattedText value = TrackedText::Make(text, 12, true, valueBrush);
        value.MaxTextWidth(MathMax(20.0, box.Width - 12));
        value.MaxTextHeight(box.Height);
        value.Trimming(Media::TextTrimming::CharacterEllipsis);
        context.DrawText(value, Av::Point(box.X + (box.Width - value.Width()) / 2,
            box.Y + (box.Height - value.Height()) / 2));
    }
}
