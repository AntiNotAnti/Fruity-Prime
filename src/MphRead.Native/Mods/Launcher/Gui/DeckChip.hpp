#pragma once

#include "Deck.hpp"
#include "Tap.hpp"
#include "../../../NativeRuntime/System/Stopwatch.hpp"

#include <string>

namespace MphRead::Mods::Launcher::Gui
{
    // A label and the thing it names, in one small slab. Not a button.
    class DeckChip final : public Av::Controls::Control
    {
    public:
        DeckChip(const std::string& key, const std::string& value);

        // The dim word above the value, or nothing.
        [[nodiscard]] const std::string& Label() const noexcept { return _key; }
        void Label(const std::string& value);
        // What the chip reads.
        [[nodiscard]] const std::string& Text() const noexcept { return _value; }
        void Text(const std::string& value);

        // The keycap on a button's face: a well sunk into it with the key in
        // the body face at half the label's size.
        static void DrawKey(Av::Media::DrawingContext& context, const std::string& key, double labelSize, double x,
            double height);

        void Render(Av::Media::DrawingContext& context) override;

    protected:
        Av::Size MeasureOverride(Av::Size availableSize) override;

    private:
        [[nodiscard]] static Av::Media::FormattedText Key(const std::string& s);
        [[nodiscard]] static Av::Media::FormattedText Value(const std::string& s);

        std::string _key;
        std::string _value;
    };

    // The one thing in the bar that is not a menu entry: a pixel heart that
    // opens the support page.
    class DeckHeart final : public Av::Controls::Control
    {
    public:
        DeckHeart();

        Av::Event<DeckHeart&> Click;

        // The pixel heart, drawn into a box, for whoever is carrying it.
        static void DrawHeart(Av::Media::DrawingContext& context, Av::Rect area, const Av::Media::IBrushPtr& ink);

        // What the mark says when the pointer is on it.
        std::string Tip = "Support this project <3";

        // The tip above a control, hanging off its right edge.
        static void DrawTip(Av::Media::DrawingContext& context, const std::string& tip, double w, double em, double show,
            double risen);

        void Render(Av::Media::DrawingContext& context) override;

    protected:
        Av::Size MeasureOverride(Av::Size availableSize) override;
        void OnPointerEntered(Av::Input::PointerEventArgs& e) override;
        void OnPointerExited(Av::Input::PointerEventArgs& e) override;
        void OnGotFocus(Av::Input::GotFocusEventArgs& e) override;
        void OnLostFocus(Av::Input::FocusChangedEventArgs& e) override;
        void OnPointerPressed(Av::Input::PointerPressedEventArgs& e) override;
        void OnPointerReleased(Av::Input::PointerReleasedEventArgs& e) override;

    private:
        [[nodiscard]] double Em() const { return Deck::Px(*this, 1.55); }
        [[nodiscard]] double TipAmount(double seconds) const;
        void TipTo(bool shown);
        void AskFrame();
        void Tip_(Av::Media::DrawingContext& context, double w, double em);

        static constexpr double Lip = 5;
        static constexpr double GlyphWide = 1.9;
        static constexpr double GlyphTall = 1.27;
        static constexpr double PadX = 0.9;
        static constexpr double PadY = 0.7;
        static constexpr double TipFade = 0.16;
        static constexpr double TipRise = 0.24;

        Tap _tap;
        ::MphRead::NativeRuntime::Stopwatch _tip;
        bool _tipShown = false;
        bool _framePending = false;
    };
}
