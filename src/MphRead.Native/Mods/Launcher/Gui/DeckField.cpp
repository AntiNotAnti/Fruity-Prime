#include "DeckField.hpp"

#include "../../../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <cmath>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;
    using ::MphRead::NativeRuntime::MathMin;
    using ::MphRead::NativeRuntime::RoundToEven;

    DeckField::DeckField(const std::string& value, double widthEms, const std::string& watermark)
        : Box(std::make_shared<Controls::TextBox>()), _widthEms(widthEms)
    {
        Box->Text(value);
        Box->PlaceholderText(watermark);
        Box->Background(Media::Brushes::Transparent());
        Box->BorderThickness(Thickness(0));
        Box->Foreground(GuiTheme::TextBrush);
        Box->CaretBrush(GuiTheme::AccentBrush);
        Box->SelectionBrush(std::make_shared<Media::SolidColorBrush>(Media::Color::FromArgb(90, 255, 179, 71)));
        Box->FontFamily(Deck::Mono);
        Box->VerticalContentAlignment(Layout::VerticalAlignment::Center);
        Box->VerticalAlignment(Layout::VerticalAlignment::Stretch);
        Box->HorizontalAlignment(Layout::HorizontalAlignment::Stretch);
        Box->Padding(Thickness(0));
        Box->MinHeight(0);
        Box->MinWidth(0);
        // The Fluent theme paints the box from its own resources in four
        // states; four Styles take its border and fill away in all of them.
        Box->TemplateChromeTransparent = true;
        Child(Box);
        Media::RenderOptions::SetEdgeMode(*this, Media::EdgeMode::Antialias);
    }

    Av::Size DeckField::MeasureOverride(Av::Size availableSize)
    {
        const double size = Size();
        Box->FontSize(size);
        const double padX = RoundToEven(size * PadXEms);
        const double padY = RoundToEven(size * PadYEms);
        const Thickness want(padX, padY, padX, padY);
        if (Padding() != want)
        {
            Padding(want);
        }
        const Av::Size measured = Decorator::MeasureOverride(availableSize);
        const double width = _widthEms > 0 ? MathMin(availableSize.Width, RoundToEven(size * _widthEms)) : measured.Width;
        return {width, measured.Height};
    }

    void DeckField::Render(Media::DrawingContext& context)
    {
        const double w = Bounds().Width;
        const double h = Bounds().Height;
        if (w <= 0 || h <= 0)
        {
            return;
        }
        const double radius = Size() * RadiusEms;
        const RoundedRect box(Rect(0, 0, w, h), radius);
        // Both shadows inset, which is why this reads as a hole; focused,
        // the hairline becomes the accent at two points.
        const bool hot = Box->IsFocused();
        const Media::BoxShadows shadows(Deck::Shadow(0, 2, 0, 0, Deck::Fade(0, 0.4), true),
            {Deck::Shadow(0, 0, 0, hot ? 2 : 1, hot ? GuiTheme::Accent : GuiTheme::Edge, true)});
        context.DrawRectangle(std::make_shared<Media::SolidColorBrush>(GuiTheme::PanelLight), nullptr, box, shadows);
    }
}
