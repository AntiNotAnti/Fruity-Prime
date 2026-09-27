#pragma once

#include "Deck.hpp"

namespace MphRead::Mods::Launcher::Gui
{
    // The panel every screen is dealt onto: the reference's .panel,
    // min(44em, 100%) wide, as tall as what it holds.
    class DeckCard final : public Av::Controls::Decorator
    {
    public:
        DeckCard();

        // width: min(44em, 100%). A maximum either way.
        double MaxWidthEms = 44;
        // Take the height offered rather than the height the content wants.
        bool Fill = false;
        // padding: 1em, inside the face.
        static constexpr double PadEms = 1;

        void Render(Av::Media::DrawingContext& context) override;

    protected:
        Av::Size MeasureOverride(Av::Size availableSize) override;
        Av::Size ArrangeOverride(Av::Size finalSize) override;

    private:
        // The solid edge under the face: ten points flat, not an em.
        static constexpr double Lip = 10;
        [[nodiscard]] double Em() const { return Deck::GetEm(*this); }
    };
}
