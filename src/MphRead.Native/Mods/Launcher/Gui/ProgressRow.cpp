#include "ProgressRow.hpp"

#include <algorithm>
#include <cmath>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;

    ProgressRow::ProgressRow()
    {
        Height(44);
        IsVisible(false);
    }

    void ProgressRow::Set(double fraction, std::string stage)
    {
        _fraction = std::clamp(fraction, 0.0, 1.0);
        _stage = std::move(stage);
        IsVisible(true);
        InvalidateVisual();
    }

    void ProgressRow::Render(Media::DrawingContext& context)
    {
        const double width = Bounds().Width;
        constexpr double barHeight = 8;
        const double barTop = Bounds().Height - barHeight - 2;

        const Media::FormattedText stage(_stage, Media::InvariantCulture, Media::FlowDirection::LeftToRight,
            GuiTheme::Face(false), 12, GuiTheme::TextDimBrush);
        context.DrawText(stage, Point{0, 2});

        const std::string percent = std::to_string(static_cast<std::int32_t>(std::round(_fraction * 100))) + "%";
        const Media::FormattedText number(percent, Media::InvariantCulture, Media::FlowDirection::LeftToRight,
            GuiTheme::Face(true), 12, GuiTheme::TextBrush);
        context.DrawText(number, Point{width - number.Width(), 2});

        context.DrawRectangle(std::make_shared<Media::SolidColorBrush>(GuiTheme::Ink), nullptr,
            RoundedRect(Rect(0, barTop, width, barHeight), barHeight / 2));
        const double filled = width * _fraction;
        if (filled > 1)
        {
            // Rounded at both ends, so a bar that has barely started still
            // looks like a bar.
            context.DrawRectangle(GuiTheme::AccentBrush, nullptr,
                RoundedRect(Rect(0, barTop, std::max(filled, barHeight), barHeight), barHeight / 2));
        }
    }
}
