#include "GamepadGlyph.hpp"

#include "TrackedText.hpp"
#include "../../Input/GamepadGlyphs.hpp"
#include "../../Input/GamepadManager.hpp"
#include "../../Input/GamepadOptions.hpp"

#include <algorithm>
#include <memory>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Input = ::MphRead::Mods::Input;
    using namespace ::MphRead::NativeRuntime::Avalonia;

    void GamepadGlyph::Render(Media::DrawingContext& context)
    {
        Draw(context, Rect(Bounds().GetSize()), _button, GuiTheme::TextBrush);
    }

    void GamepadGlyph::Draw(Media::DrawingContext& context, const Rect& bounds,
        Input::GamepadButtons button, const Media::IBrushPtr& brush)
    {
        Input::GamepadFamily family = Input::GamepadOptions::GlyphStyle();
        if (family == Input::GamepadFamily::Unknown)
        {
            const std::optional<Input::GamepadDeviceSnapshot> active = Input::GamepadManager::ActiveDevice();
            family = active.has_value() ? active->Family() : Input::GamepadFamily::Generic;
        }

        const Media::IPenPtr pen = std::make_shared<Media::Pen>(brush, 1.5);
        const Point center = bounds.Center();
        const double cx = center.X;
        const double cy = center.Y;
        const double radius = std::min(bounds.Width, bounds.Height) * 0.43;
        const bool face = button == Input::GamepadButtons::A || button == Input::GamepadButtons::B
            || button == Input::GamepadButtons::X || button == Input::GamepadButtons::Y;
        if (face)
        {
            context.DrawEllipse({}, pen, center, radius, radius);
        }
        else
        {
            context.DrawRectangle({}, pen, bounds.Deflate(1), 3, 3);
        }

        const double r = radius * 0.55;
        if (family == Input::GamepadFamily::PlayStation && face)
        {
            if (button == Input::GamepadButtons::A)
            {
                context.DrawLine(pen, Point(cx - r, cy - r), Point(cx + r, cy + r));
                context.DrawLine(pen, Point(cx - r, cy + r), Point(cx + r, cy - r));
            }
            else if (button == Input::GamepadButtons::B)
            {
                context.DrawEllipse({}, pen, Point(cx, cy), r, r);
            }
            else if (button == Input::GamepadButtons::X)
            {
                context.DrawRectangle({}, pen, Rect(cx - r, cy - r, r * 2, r * 2));
            }
            else
            {
                context.DrawLine(pen, Point(cx, cy - r), Point(cx + r, cy + r));
                context.DrawLine(pen, Point(cx + r, cy + r), Point(cx - r, cy + r));
                context.DrawLine(pen, Point(cx - r, cy + r), Point(cx, cy - r));
            }
            return;
        }

        const Media::FormattedText text = TrackedText::Make(
            Input::GamepadGlyphs::Resolve(button, family), face ? 12 : 9, false, brush);
        context.DrawText(text, Point(cx - text.Width() / 2, cy - text.Height() / 2));
    }
}
