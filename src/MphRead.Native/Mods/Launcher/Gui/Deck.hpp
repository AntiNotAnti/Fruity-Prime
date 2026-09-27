#pragma once

#include "GuiTheme.hpp"

#include <functional>

namespace MphRead::Mods::Launcher::Gui
{
    // The deck theme's unit, its faces, and the two type faces it sets.
    //
    // Everything in these screens is sized in ems, and the em is not a
    // constant: it is read off the frame's own width, so one rule draws a
    // desktop, a phone held upright and the same phone turned. It is
    // published as an inherited attached property so a control deep in a
    // list can ask for it; DeckStage is what puts it there.
    class Deck final
    {
    public:
        Deck() = delete;

        // A face colour and the solid edge under it, two stops down.
        struct Face final
        {
            Av::Media::Color Fill{};
            Av::Media::Color Lip{};

            [[nodiscard]] static Face Blue() { return {Rgb(0x2b4e6b), Rgb(0x16293a)}; }
            [[nodiscard]] static Face Brass() { return {Rgb(0x7a6130), Rgb(0x3f3118)}; }
            [[nodiscard]] static Face Rust() { return {Rgb(0x6b3636), Rgb(0x381b1b)}; }
            [[nodiscard]] static Face Moss() { return {Rgb(0x2c5a4e), Rgb(0x15302a)}; }
            [[nodiscard]] static Face Slate() { return {Rgb(0x232a36), Rgb(0x12161e)}; }
            // The stepper and the tab strip's own grey, a shade up from Slate.
            [[nodiscard]] static Face Step() { return {Rgb(0x2a3140), Rgb(0x151a23)}; }

            friend constexpr bool operator==(const Face&, const Face&) noexcept = default;
        };

        [[nodiscard]] static constexpr Av::Media::Color Rgb(std::int32_t hex) noexcept
        {
            return Av::Media::Color::FromRgb(static_cast<std::uint8_t>(hex >> 16),
                static_cast<std::uint8_t>((hex >> 8) & 0xff), static_cast<std::uint8_t>(hex & 0xff));
        }

        // A colour at a CSS alpha: rgba(0,0,0,.55) is Fade(0, .55).
        [[nodiscard]] static Av::Media::Color Fade(std::int32_t hex, double alpha);

        // One CSS shadow: offsetX offsetY blur spread colour, and inset.
        [[nodiscard]] static Av::Media::BoxShadow Shadow(double x, double y, double blur, double spread,
            Av::Media::Color colour, bool inset = false);

        // The body face, in two real files rather than a synthesised bold.
        static inline const Av::Media::FontFamilyPtr Mono = std::make_shared<Av::Media::FontFamily>(
            "avares://FruityPrime/Assets/Fonts/JetBrainsMono-Regular.ttf#JetBrains Mono");
        static inline const Av::Media::FontFamilyPtr MonoBold = std::make_shared<Av::Media::FontFamily>(
            "avares://FruityPrime/Assets/Fonts/JetBrainsMono-Bold.ttf#JetBrains Mono");

        [[nodiscard]] static Av::Media::Typeface Body(bool bold);
        // The display face: labels, names, headings, the wordmark. 600 for a
        // button, the browser's own 400 for a row.
        [[nodiscard]] static Av::Media::Typeface Label(bool strong = true);

        // Whether the frame is a phone's: changes the curve the em is read
        // off and nothing else.
        [[nodiscard]] static bool Phone() noexcept { return _phone; }
        static void Phone(bool value) noexcept { _phone = value; }

        // Draw everything at rest: no spring in flight, no idle bob.
        [[nodiscard]] static bool Still() noexcept { return _still || _asleep; }
        static void Still(bool value) noexcept { _still = value; }

        // The screens are not on the glass, so nothing on them need move.
        [[nodiscard]] static bool Asleep() noexcept { return _asleep; }
        static void Asleep(bool value) noexcept { _asleep = value; }

        // Run something before the next frame these screens are drawn in,
        // and mark that frame as wanted.
        static void NextFrame(Av::Visual& asker, std::function<void()> step, bool idling = false);

        // Whether the keyboard, rather than a pointer, is driving the screens:
        // the state behind :focus-visible.
        [[nodiscard]] static bool KeyboardDriving() noexcept { return _keyboardDriving; }
        static void DrivingByKeyboard() noexcept { _keyboardDriving = true; }
        static void DrivingByPointer() noexcept { _keyboardDriving = false; }

        // --spring, cubic-bezier(.18, 1.55, .35, 1), solved for a progress.
        [[nodiscard]] static double Spring(double progress);
        [[nodiscard]] static double Bezier(double progress, double x1, double y1, double x2, double y2);

        [[nodiscard]] static double EmFor(double width, double height);

        // The row's own em, which is not the stage's: a button does not
        // inherit font-size.
        static constexpr double RowEm = 13.3333;

        // Below this the frame drops a row's middle columns.
        static constexpr double NarrowFrame = 560;

        static Av::AttachedProperty<double>& EmProperty;
        // The frame's width, for the one container query there is.
        static Av::AttachedProperty<double>& FrameWidthProperty;

        [[nodiscard]] static double GetEm(const Av::AvaloniaObject& o) { return o.GetValue(EmProperty); }
        static void SetEm(Av::AvaloniaObject& o, double value) { o.SetValue(EmProperty, value); }
        [[nodiscard]] static double GetFrameWidth(const Av::AvaloniaObject& o) { return o.GetValue(FrameWidthProperty); }
        static void SetFrameWidth(Av::AvaloniaObject& o, double value) { o.SetValue(FrameWidthProperty, value); }

        // Ems to points, on this control's own frame.
        [[nodiscard]] static double Px(const Av::AvaloniaObject& o, double ems) { return GetEm(o) * ems; }
        // True where the frame has dropped to a phone's width.
        [[nodiscard]] static bool Narrow(const Av::AvaloniaObject& o) { return GetFrameWidth(o) <= NarrowFrame; }

    private:
        [[nodiscard]] static double Curve(double t, double a, double b);
        [[nodiscard]] static double Slope(double t, double a, double b);

#if defined(__ANDROID__)
        static inline bool _phone = true;
#else
        static inline bool _phone = false;
#endif
        static inline bool _still = false;
        static inline bool _asleep = false;
        static inline bool _keyboardDriving = false;
    };

    // The frame every screen is laid out in, and the one place the em is
    // worked out: it stamps the em on itself before its children measure.
    class DeckStage final : public Av::Controls::Panel
    {
    protected:
        Av::Size MeasureOverride(Av::Size availableSize) override;

    private:
        double _em = -1;
        double _width = -1;
    };
}
