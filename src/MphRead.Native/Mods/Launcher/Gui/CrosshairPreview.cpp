#include "CrosshairPreview.hpp"

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;
    using ::MphRead::Mods::Render::Crosshair;
    using ::MphRead::Mods::Render::CrosshairBar;

    void CrosshairPreview::Draw(Media::DrawingContext& context, Rect area, ::MphRead::Mods::Render::CrosshairStyle style,
        ::MphRead::Mods::Render::CrosshairSize size)
    {
        context.DrawRectangle(GuiTheme::PanelBrush, std::make_shared<Media::Pen>(GuiTheme::EdgeBrush, 1), RoundedRect(area, 4));
        const double cx = area.X + area.Width / 2;
        const double cy = area.Y + area.Height / 2;
        const float scale = Crosshair::ScaleOf(size);
        const std::vector<CrosshairBar> bars = Crosshair::BarsOf(style, scale);
        for (const CrosshairBar& bar : bars)
        {
            // Whole pixels, and with Y up.
            const auto [left, right, bottom, top] = Crosshair::EdgesOf(bar);
            context.FillRectangle(GuiTheme::TextBrush, Rect(cx + left, cy - top, right - left, top - bottom));
        }
        const auto [radius, thickness] = Crosshair::RingOf(style, scale);
        if (thickness > 0)
        {
            context.DrawEllipse(nullptr, std::make_shared<Media::Pen>(GuiTheme::TextBrush, thickness), Point{cx, cy},
                radius, radius);
        }
    }
}
