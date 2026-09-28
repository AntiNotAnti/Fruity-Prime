#pragma once

#include "GuiTheme.hpp"

#include <functional>
#include <memory>
#include <string_view>
#include <utility>

namespace MphRead::Mods::Launcher::Gui
{
    // One line of the theme's type, tracked and trimmed, laid out once and
    // kept: shaping is the expensive half of drawing a string, and none of
    // this text depends on time.
    class DeckText final
    {
    public:
        DeckText() = delete;

        // The reference's default tracking on a label, in ems of its own size.
        static constexpr double LabelTracking = 0.04;

        // A plain run: no tracking, trimmed to a width if one is given.
        [[nodiscard]] static std::shared_ptr<Av::Media::FormattedText> Run(std::string_view text,
            const Av::Media::Typeface& face, double size, const Av::Media::IBrushPtr& brush, double maxWidth = 0,
            bool display = true);

        // How wide a tracked run comes out; the trailing air is kept, as CSS
        // keeps it.
        [[nodiscard]] static double MeasureTracked(std::string_view text, const Av::Media::Typeface& face, double size,
            double trackingEms);

        // A tracked run from its left edge, vertically centred in a box of
        // height, on whole pixels. hop gives character n's lift and lean.
        static double DrawTracked(Av::Media::DrawingContext& context, std::string_view text,
            const Av::Media::Typeface& face, double size, const Av::Media::IBrushPtr& brush, double x, double top,
            double height, double trackingEms,
            const std::function<std::pair<double, double>(std::int32_t)>& hop = nullptr);

        // Drop the lot. Only the palette or the face changing needs this.
        static void Forget();

    private:
        // A space's advance, as a fraction of the size.
        static constexpr double SpaceEm = 0.42;

        [[nodiscard]] static std::shared_ptr<Av::Media::FormattedText> Lay(std::string_view text,
            const Av::Media::Typeface& face, double size, const Av::Media::IBrushPtr& brush, bool display,
            double maxWidth);
    };
}
