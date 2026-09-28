#include "GuiTheme.hpp"

#include "../../../NativeRuntime/System/Managed.hpp"

#include <exception>
#include <mutex>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;

    double GuiTheme::PixelSize(double wanted)
    {
        // Nearest even point, floor of 9.
        return ::MphRead::NativeRuntime::MathMax(9.0,
            ::MphRead::NativeRuntime::RoundToEven(wanted / 2) * 2);
    }

    Media::Typeface GuiTheme::Face(bool bold)
    {
        return Media::Typeface(bold ? PixelSemi : Pixel, Media::FontStyle::Normal, Media::FontWeight::Normal);
    }

    Media::FormattedText GuiTheme::Lay(std::string_view text, double size, const Media::IBrushPtr& brush, bool bold)
    {
        return Media::FormattedText(text, Media::InvariantCulture, Media::FlowDirection::LeftToRight, Face(bold),
            PixelSize(size), brush);
    }

    void GuiTheme::PixelPerfect(Visual& visual)
    {
        // Edges aliased, glyphs not: the chunky look is the shapes, and a
        // pixel face at eleven points with no antialiasing loses the
        // difference between an "e" and an "o".
        Media::RenderOptions::SetEdgeMode(visual, Media::EdgeMode::Aliased);
        Media::TextOptions::SetTextRenderingMode(visual, Media::TextRenderingMode::Antialias);
        // Nearest-neighbour for bitmaps too, so a map render scaled into a
        // row is scaled the way the rest of the screen is drawn.
        Media::RenderOptions::SetBitmapInterpolationMode(visual, Media::BitmapInterpolationMode::None);
    }

    std::shared_ptr<Controls::WindowIcon> GuiTheme::AppIcon()
    {
        static std::once_flag once;
        static std::shared_ptr<Controls::WindowIcon> icon;
        std::call_once(once, []
        {
            try
            {
                icon = Controls::WindowIcon::FromBytes(
                    Platform::AssetLoader::Open("avares://FruityPrime/Assets/fruity-prime-mark.png"));
            }
            catch (const std::exception&)
            {
                icon = nullptr;
            }
        });
        return icon;
    }

    Media::Color GuiTheme::Shade(Media::Color color, double amount)
    {
        const double t = amount < 0 ? -amount : amount;
        const int target = amount >= 0 ? 255 : 0;
        return Media::Color::FromArgb(color.A, static_cast<std::uint8_t>(color.R + (target - color.R) * t),
            static_cast<std::uint8_t>(color.G + (target - color.G) * t),
            static_cast<std::uint8_t>(color.B + (target - color.B) * t));
    }
}
