#pragma once

#include "Deck.hpp"

#include <string_view>

namespace MphRead::Mods::Launcher::Gui
{
    // The name, set rather than drawn: two lines in the pixel face with a
    // hard four-offset outline and a drop under it.
    class DeckWordmark final : public Av::Controls::Control
    {
    public:
        explicit DeckWordmark(double sizeEms = 7.6);

        // The mark's size in the frame's ems.
        double SizeEms = 7.6;

        void Render(Av::Media::DrawingContext& context) override;

    protected:
        Av::Size MeasureOverride(Av::Size availableSize) override;

    private:
        static constexpr double Outline = 3;
        static constexpr double Drop = 10;

        [[nodiscard]] double Size() const { return GuiTheme::PixelSize(Deck::Px(*this, SizeEms)); }
        [[nodiscard]] Av::Media::FormattedText Line(std::string_view text, const Av::Media::IBrushPtr& brush) const;
        static void Draw(Av::Media::DrawingContext& context, std::string_view word, const Av::Media::FormattedText& text,
            double x, double y, double size);
        [[nodiscard]] static Av::Media::FormattedText Recolour(std::string_view word, double size,
            const Av::Media::IBrushPtr& brush);
    };
}
