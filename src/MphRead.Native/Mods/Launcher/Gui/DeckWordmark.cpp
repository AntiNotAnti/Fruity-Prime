#include "DeckWordmark.hpp"

#include "../../../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;
    using ::MphRead::NativeRuntime::MathMax;
    using ::MphRead::NativeRuntime::RoundToEven;

    DeckWordmark::DeckWordmark(double sizeEms)
        : SizeEms(sizeEms)
    {
        static const bool registered = []
        {
            AffectsMeasure<DeckWordmark>(Deck::EmProperty);
            AffectsRender<DeckWordmark>(Deck::EmProperty);
            return true;
        }();
        (void)registered;
        IsHitTestVisible(false);
        Media::RenderOptions::SetEdgeMode(*this, Media::EdgeMode::Aliased);
    }

    Media::FormattedText DeckWordmark::Line(std::string_view text, const Media::IBrushPtr& brush) const
    {
        return Media::FormattedText(text, Media::InvariantCulture, Media::FlowDirection::LeftToRight,
            Media::Typeface(GuiTheme::PixelBold, Media::FontStyle::Normal, Media::FontWeight::Normal), Size(), brush);
    }

    Av::Size DeckWordmark::MeasureOverride(Av::Size availableSize)
    {
        (void)availableSize;
        const Media::FormattedText top = Line("FRUITY", GuiTheme::TextBrush);
        const Media::FormattedText bottom = Line("PRIME", GuiTheme::AccentBrush);
        return {MathMax(top.Width(), bottom.Width()) + Outline * 2,
            top.Height() + bottom.Height() * 0.88 + Outline * 2 + Drop};
    }

    void DeckWordmark::Render(Media::DrawingContext& context)
    {
        const Media::FormattedText top
            = Line("FRUITY", std::make_shared<Media::SolidColorBrush>(Media::Color::FromRgb(0xf2, 0xed, 0xe2)));
        const Media::FormattedText bottom = Line("PRIME", GuiTheme::AccentBrush);
        const double w = Bounds().Width;
        // Whole pixels, both lines, or the outline lands half on one row.
        const double topX = RoundToEven((w - top.Width()) / 2);
        const double bottomX = RoundToEven((w - bottom.Width()) / 2);
        const double topY = RoundToEven(Outline);
        const double bottomY = RoundToEven(topY + top.Height() * 0.88);
        const double size = Size();
        Draw(context, "FRUITY", top, topX, topY, size);
        Draw(context, "PRIME", bottom, bottomX, bottomY, size);
    }

    void DeckWordmark::Draw(Media::DrawingContext& context, std::string_view word, const Media::FormattedText& text,
        double x, double y, double size)
    {
        const Media::IBrushPtr ink = std::make_shared<Media::SolidColorBrush>(GuiTheme::Ink);
        // 0 10px 0 rgba(0,0,0,.55) -- 140 is that alpha in bytes.
        const Media::IBrushPtr shadow = std::make_shared<Media::SolidColorBrush>(Media::Color::FromArgb(140, 0, 0, 0));
        // The drop first, then the outline, then the face over both.
        context.DrawText(Recolour(word, size, shadow), Point{x, y + Drop});
        constexpr std::array<std::pair<double, double>, 6> offsets{
            {{-Outline, 0.0}, {Outline, 0.0}, {0.0, -Outline}, {0.0, Outline}, {-Outline, Outline}, {Outline, Outline}}};
        for (const auto& [dx, dy] : offsets)
        {
            context.DrawText(Recolour(word, size, ink), Point{x + dx, y + dy});
        }
        context.DrawText(text, Point{x, y});
    }

    Media::FormattedText DeckWordmark::Recolour(std::string_view word, double size, const Media::IBrushPtr& brush)
    {
        return Media::FormattedText(word, Media::InvariantCulture, Media::FlowDirection::LeftToRight,
            Media::Typeface(GuiTheme::PixelBold, Media::FontStyle::Normal, Media::FontWeight::Normal), size, brush);
    }
}
