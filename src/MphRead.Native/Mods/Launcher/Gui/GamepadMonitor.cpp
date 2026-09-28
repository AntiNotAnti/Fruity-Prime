#include "GamepadMonitor.hpp"

#include "GuiTheme.hpp"
#include "TrackedText.hpp"
#include "../../../Mods/Input/GamepadOptions.hpp"
#include "../../../Mods/Input/GamepadProbe.hpp"
#include "../../../Mods/Input/PadBindings.hpp"
#include "../../../NativeRuntime/System/Number.hpp"

#if defined(MPHREAD_SHELL)
#include "UiSurface.hpp"
#endif

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Runtime = ::MphRead::NativeRuntime;
    namespace PadInput = ::MphRead::Mods::Input;
    using namespace Runtime::Avalonia;

    GamepadMonitor::GamepadMonitor()
        : _timer(Threading::TimeSpan(0.1), Threading::DispatcherPriority::Background,
            [this] { if (IsEffectivelyVisible()) Refresh(); })
    {
        Height(182);
        AttachedToVisualTree += [this](Controls::Control&)
        {
            Refresh();
            _timer.Start();
        };
        DetachedFromVisualTree += [this](Controls::Control&) { _timer.Stop(); };
    }

    void GamepadMonitor::Refresh()
    {
        const std::optional<PadInput::GamepadDeviceSnapshot> device = PadInput::GamepadManager::ActiveDevice();
        const PadInput::GamepadSnapshot snapshot = PadInput::GamepadManager::Snapshot();
        const PadInput::GamepadState& state = snapshot.State;
        _raw = device.has_value() ? device->RawState() : PadInput::GamepadState{};
        const std::string status = device.has_value()
            ? device->Name() + " | " + device->Mapping()
            : "No controller detected. Connect it and press a button.";
        const std::string buttons = state.Buttons == PadInput::GamepadButtons::None
            ? "Press a button to test it" : PadInput::PadBindings::Describe(state.Buttons);
        const std::string actions = state.Buttons == PadInput::GamepadButtons::None
            ? "Sticks move/aim; triggers should only fill the LT/RT bars."
            : "Assigned: " + PadInput::GamepadProbe::Actions(state.Buttons);
        if (status == _status && buttons == _buttons && actions == _actions && state == _state)
        {
            return;
        }
        _state = state;
        _status = status;
        _buttons = buttons;
        _actions = actions;
        InvalidateVisual();
#if defined(MPHREAD_SHELL)
        if (auto surface = UiSurface::Current(); surface != nullptr)
        {
            surface->Invalidate();
        }
#endif
    }

    void GamepadMonitor::Render(Media::DrawingContext& context)
    {
        const auto text = [&](const std::string& value, double x, double y, double width, bool dim = false)
        {
            Media::FormattedText formatted = TrackedText::Make(value, 11, false,
                dim ? GuiTheme::TextDimBrush : GuiTheme::TextBrush);
            formatted.MaxTextWidth(std::max(1.0, width));
            formatted.MaxTextHeight(25);
            formatted.Trimming(Media::TextTrimming::CharacterEllipsis);
            context.DrawText(formatted, Point(x, y));
        };

        text(_status, 4, 3, Bounds().Width - 8);
        const auto stick = [&](const std::string& label, double x, float axisX, float axisY,
                               float rawX, float rawY, float dead)
        {
            const Point center(x + 27, 60);
            context.DrawEllipse(GuiTheme::PanelLightBrush,
                std::make_shared<Media::Pen>(GuiTheme::TextDimBrush, 1), center, 23, 23);
            context.DrawEllipse({}, std::make_shared<Media::Pen>(GuiTheme::TextDimBrush, 1),
                center, 23 * dead, 23 * dead);
            context.DrawEllipse({}, std::make_shared<Media::Pen>(GuiTheme::WarmBrush, 1),
                Point(center.X + rawX * 19, center.Y - rawY * 19), 4, 4);
            context.DrawEllipse(GuiTheme::AccentBrush, {},
                Point(center.X + axisX * 19, center.Y - axisY * 19), 4, 4);
            text(label, x, 88, 75, true);
        };
        stick("Left stick", 10, _state.LeftX, _state.LeftY, _raw.LeftX, _raw.LeftY,
            PadInput::GamepadOptions::LeftInner());
        stick("Right stick", 96, _state.RightX, _state.RightY, _raw.RightX, _raw.RightY,
            PadInput::GamepadOptions::RightInner());

        const auto trigger = [&](const std::string& label, double y, float value)
        {
            text(label, 190, y - 1, 24, true);
            const double width = std::clamp(Bounds().Width - 274, 30.0, 160.0);
            context.DrawRectangle(GuiTheme::PanelLightBrush, {}, Rect(218, y, width, 12));
            context.DrawRectangle(GuiTheme::AccentBrush, {},
                Rect(218, y, width * std::clamp(static_cast<double>(value), 0.0, 1.0), 12));
            const double threshold = 218 + width * PadInput::GamepadOptions::TriggerThreshold();
            context.DrawLine(std::make_shared<Media::Pen>(GuiTheme::TextBrush, 1),
                Point(threshold, y), Point(threshold, y + 12));
            text(Runtime::ToString(value, "0.00"), 224 + width, y - 1, 45, true);
        };
        trigger("LT", 42, _state.LeftTrigger);
        trigger("RT", 70, _state.RightTrigger);
        text(_buttons, 4, 112, Bounds().Width - 8);
        text(_actions, 4, 135, Bounds().Width - 8, true);

        std::string rawText = "Raw LT ";
        rawText += Runtime::ToString(_raw.LeftTrigger, "0.00");
        rawText += " RT ";
        rawText += Runtime::ToString(_raw.RightTrigger, "0.00");
        rawText += " | center L ";
        rawText += Runtime::ToString(PadInput::GamepadOptions::LeftCalibration().CenterX(), "0.00");
        rawText += ",";
        rawText += Runtime::ToString(PadInput::GamepadOptions::LeftCalibration().CenterY(), "0.00");
        rawText += " R ";
        rawText += Runtime::ToString(PadInput::GamepadOptions::RightCalibration().CenterX(), "0.00");
        rawText += ",";
        rawText += Runtime::ToString(PadInput::GamepadOptions::RightCalibration().CenterY(), "0.00");
        text(rawText, 4, 158, Bounds().Width - 8, true);
    }
}
