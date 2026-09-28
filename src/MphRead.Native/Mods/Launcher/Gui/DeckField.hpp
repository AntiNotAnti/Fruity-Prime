#pragma once

#include "Deck.hpp"

#include <memory>
#include <string>

namespace MphRead::Mods::Launcher::Gui
{
    // Somewhere to type, sunk into the panel: the reference's .field. The
    // box is Avalonia's own TextBox with its chrome taken away; the well
    // under it is drawn here, in the body face.
    class DeckField final : public Av::Controls::Decorator
    {
    public:
        explicit DeckField(const std::string& value, double widthEms = 11, const std::string& watermark = "");

        const std::shared_ptr<Av::Controls::TextBox> Box;

        [[nodiscard]] std::string Value() const { return Box->Text(); }
        void Value(std::string value) { Box->Text(std::move(value)); }

        void Render(Av::Media::DrawingContext& context) override;

    protected:
        Av::Size MeasureOverride(Av::Size availableSize) override;

    private:
        static constexpr double SizeEms = 0.95;
        static constexpr double PadXEms = 0.7;
        static constexpr double PadYEms = 0.4;
        static constexpr double RadiusEms = 0.35;

        [[nodiscard]] double Size() const { return Deck::GetEm(*this) * SizeEms; }

        // Its natural width, in its own ems, or 0 to take what it is given.
        const double _widthEms;
    };
}
