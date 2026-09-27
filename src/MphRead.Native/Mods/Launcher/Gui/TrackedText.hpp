#pragma once

#include "GuiTheme.hpp"

#include <string_view>

namespace MphRead::Mods::Launcher::Gui
{
    // Text with extra space between the letters, drawn glyph by glyph. Fine
    // for the handful of short labels involved; not for running text.
    class TrackedText final
    {
    public:
        TrackedText() = delete;

        [[nodiscard]] static Av::Media::FormattedText Make(std::string_view text, double size, bool bold,
            const Av::Media::IBrushPtr& brush);
        static void Draw(Av::Media::DrawingContext& context, std::string_view text, double size,
            const Av::Media::IBrushPtr& brush, double x, double y, double tracking);
        [[nodiscard]] static double Measure(std::string_view text, double size, double tracking);
        // Height of a line at this size, measured off a real glyph.
        [[nodiscard]] static double LineHeight(double size);
        // How wide a space is: the difference between "n n" and "nn", since a
        // lone space measures as very nearly nothing.
        [[nodiscard]] static double SpaceWidth(double size);
    };
}
