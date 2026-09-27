#pragma once

#include "Deck.hpp"
#include "Tap.hpp"
#include "../../../NativeRuntime/System/Stopwatch.hpp"

#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace MphRead::Mods::Launcher::Gui
{
    // A button with a thickness: a solid, unblurred edge below the face in
    // the face's own hue two stops down, a press that travels by exactly the
    // edge it loses, a spring on hover, and a lean toward the pointer.
    class DeckButton final : public Av::Controls::Control
    {
    public:
        DeckButton(std::string text, Deck::Face face, double sizeEms = 1.55, double padXEms = 1.5,
            double padYEms = 0.85, double lip = 6);

        Av::Event<DeckButton&> Click;

        // The key that does the same thing, in a chip on the face.
        std::string KeyCap;

        // Something drawn on the face instead of a word, in the box it is
        // given, in the colour the label would have been.
        std::function<void(Av::Media::DrawingContext&, Av::Rect, const Av::Media::IBrushPtr&)> Glyph{};
        // The glyph's box, in this button's own ems.
        Av::Size GlyphEms{};
        // The glyph's own colour, before the face's filter.
        std::optional<Av::Media::Color> GlyphColour{};
        // What this says when the pointer is on it, drawn above it.
        std::string Tip;
        // Two and a half points of bob on a 3.4 second cycle, on the one
        // button the screen wants looked at.
        bool Idle = false;

        [[nodiscard]] const std::string& Text() const noexcept { return _text; }
        void Text(std::string value);

        // Change face without rebuilding, so a spring in flight survives.
        void Wear(Deck::Face face, bool selected);

        [[nodiscard]] bool Selected() const noexcept { return _selected; }
        void Selected(bool value);

        void Render(Av::Media::DrawingContext& context) override;

    protected:
        Av::Size MeasureOverride(Av::Size availableSize) override;
        void OnPointerEntered(Av::Input::PointerEventArgs& e) override;
        void OnPointerExited(Av::Input::PointerEventArgs& e) override;
        void OnPointerMoved(Av::Input::PointerEventArgs& e) override;
        void OnPointerPressed(Av::Input::PointerPressedEventArgs& e) override;
        void OnPointerReleased(Av::Input::PointerReleasedEventArgs& e) override;
        void OnPointerCaptureLost(Av::Input::PointerCaptureLostEventArgs& e) override;
        void OnKeyDown(Av::Input::KeyEventArgs& e) override;
        void OnGotFocus(Av::Input::GotFocusEventArgs& e) override;
        void OnLostFocus(Av::Input::FocusChangedEventArgs& e) override;

    private:
        [[nodiscard]] double Size() const;
        [[nodiscard]] double KeyGap() const { return Size() * 0.5; }
        [[nodiscard]] double KeyWidth(double size) const;
        void StartHop();
        bool Settle();
        [[nodiscard]] std::pair<double, double> Hop(std::int32_t index, double size) const;
        bool Hopping();
        [[nodiscard]] double Bob() const;
        void RequestAnotherFrame(bool idling = false);
        void DrawTip(Av::Media::DrawingContext& context, double w, double size, bool hot);

        std::string _text;
        Deck::Face _face;
        bool _selected = false;
        const double _sizeEms;
        const double _padXEms;
        const double _padYEms;
        const double _lip;
        Tap _tap;

        static constexpr double LipPressed = 2;
        static constexpr double RadiusEms = 0.55;
        static constexpr double MaxTilt = 7;

        double _pop = 1;
        double _popVelocity = 0;
        double _popTarget = 1;
        double _tilt = 0;
        double _tiltTarget = 0;
        ::MphRead::NativeRuntime::Stopwatch _clock = ::MphRead::NativeRuntime::Stopwatch::StartNew();
        ::MphRead::NativeRuntime::TimeSpan _last{};

        static constexpr double Stiffness = 220;
        static constexpr double Damping = 18;

        static constexpr double HopSeconds = 0.42;
        static constexpr double HopStagger = 0.022;
        static constexpr double HopPeak = 0.38;
        static constexpr double HopLift = 0.18;
        static constexpr double HopTilt = 3;
        ::MphRead::NativeRuntime::Stopwatch _hop;

        // The reference's :focus-visible, not :focus.
        bool _ringVisible = false;
        bool _framePending = false;
        double _tipShow = 0;
        bool _tipMoving = false;
    };

    // The two filters the reference puts on a hovered or a disabled face,
    // done as arithmetic rather than as a blend towards a colour.
    class DeckPaint final
    {
    public:
        DeckPaint() = delete;
        [[nodiscard]] static Av::Media::Color Brightness(Av::Media::Color c, double k);
        // The sRGB luma matrix, which is what a CSS saturate() filter is.
        [[nodiscard]] static Av::Media::Color Saturate(Av::Media::Color c, double s);

    private:
        [[nodiscard]] static std::uint8_t Clamp(double v);
    };
}
