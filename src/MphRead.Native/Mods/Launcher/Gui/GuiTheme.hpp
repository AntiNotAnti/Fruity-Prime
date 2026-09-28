#pragma once

#include "../../../NativeRuntime/Avalonia/Avalonia.hpp"

#include <memory>
#include <string_view>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;

    // The front screen's palette and metrics, in Avalonia terms.
    //
    // The colours are LauncherTheme's, value for value, and are meant to stay
    // that way: the numbers, not the types, are the thing being kept in step.
    class GuiTheme final
    {
    public:
        GuiTheme() = delete;

        static constexpr Av::Media::Color Ink = Av::Media::Color::FromRgb(10, 12, 16);
        static constexpr Av::Media::Color Panel = Av::Media::Color::FromRgb(18, 21, 28);
        static constexpr Av::Media::Color PanelLight = Av::Media::Color::FromRgb(26, 31, 41);
        static constexpr Av::Media::Color Edge = Av::Media::Color::FromRgb(38, 46, 60);
        // Under Panel: the well a row or a card sits in.
        static constexpr Av::Media::Color PanelDeep = Av::Media::Color::FromRgb(14, 17, 24);
        static constexpr Av::Media::Color Text = Av::Media::Color::FromRgb(230, 234, 242);
        static constexpr Av::Media::Color TextDim = Av::Media::Color::FromRgb(138, 147, 166);
        // The one accent colour everywhere: the same warm the main menu's Play uses.
        static constexpr Av::Media::Color Accent = Av::Media::Color::FromRgb(255, 179, 71);
        static constexpr Av::Media::Color Warm = Av::Media::Color::FromRgb(255, 179, 71);
        // The deck palette's, not the old neon pair, which buzzed on this panel.
        static constexpr Av::Media::Color Good = Av::Media::Color::FromRgb(0x5f, 0x9e, 0x72);
        static constexpr Av::Media::Color Warn = Av::Media::Color::FromRgb(0xc0, 0x8a, 0x3e);
        static constexpr Av::Media::Color Bad = Av::Media::Color::FromRgb(0xa8, 0x54, 0x54);

        static inline const Av::Media::IBrushPtr InkBrush = std::make_shared<Av::Media::SolidColorBrush>(Ink);
        static inline const Av::Media::IBrushPtr PanelBrush = std::make_shared<Av::Media::SolidColorBrush>(Panel);
        static inline const Av::Media::IBrushPtr PanelLightBrush = std::make_shared<Av::Media::SolidColorBrush>(PanelLight);
        static inline const Av::Media::IBrushPtr EdgeBrush = std::make_shared<Av::Media::SolidColorBrush>(Edge);
        static inline const Av::Media::IBrushPtr TextBrush = std::make_shared<Av::Media::SolidColorBrush>(Text);
        static inline const Av::Media::IBrushPtr TextDimBrush = std::make_shared<Av::Media::SolidColorBrush>(TextDim);
        static inline const Av::Media::IBrushPtr AccentBrush = std::make_shared<Av::Media::SolidColorBrush>(Accent);
        static inline const Av::Media::IBrushPtr WarmBrush = std::make_shared<Av::Media::SolidColorBrush>(Warm);
        static inline const Av::Media::IBrushPtr GoodBrush = std::make_shared<Av::Media::SolidColorBrush>(Good);
        static inline const Av::Media::IBrushPtr WarnBrush = std::make_shared<Av::Media::SolidColorBrush>(Warn);
        static inline const Av::Media::IBrushPtr BadBrush = std::make_shared<Av::Media::SolidColorBrush>(Bad);

        // Panel and PanelLight, thinned so the backdrop still shows through them.
        static inline const Av::Media::IBrushPtr GlassBrush
            = std::make_shared<Av::Media::SolidColorBrush>(Av::Media::Color::FromArgb(220, Panel.R, Panel.G, Panel.B));
        static inline const Av::Media::IBrushPtr GlassLightBrush = std::make_shared<Av::Media::SolidColorBrush>(
            Av::Media::Color::FromArgb(220, PanelLight.R, PanelLight.G, PanelLight.B));

        // What the pause menu and the in-game settings lay over the match:
        // dark enough to read a menu on, clear enough to watch through.
        static inline const Av::Media::IBrushPtr ScrimBrush
            = std::make_shared<Av::Media::SolidColorBrush>(Av::Media::Color::FromArgb(196, Ink.R, Ink.G, Ink.B));

        // Roboto Bold: the few places that are prose rather than interface.
        static inline const Av::Media::FontFamilyPtr Prose
            = std::make_shared<Av::Media::FontFamily>("avares://FruityPrime/Assets/Fonts/Roboto-Bold.ttf#Roboto");

        // The deck theme's face: Pixelify Sans, SIL OFL 1.1.
        static inline const Av::Media::FontFamilyPtr Pixel = std::make_shared<Av::Media::FontFamily>(
            "avares://FruityPrime/Assets/Fonts/PixelifySans-Regular.ttf#Pixelify Sans");

        // The same face at 600 and 700, as their own files, since a
        // synthesised bold puts an extra half pixel on every stem.
        static inline const Av::Media::FontFamilyPtr PixelSemi = std::make_shared<Av::Media::FontFamily>(
            "avares://FruityPrime/Assets/Fonts/PixelifySans-SemiBold.ttf#Pixelify Sans");
        static inline const Av::Media::FontFamilyPtr PixelBold = std::make_shared<Av::Media::FontFamily>(
            "avares://FruityPrime/Assets/Fonts/PixelifySans-Bold.ttf#Pixelify Sans");

        // Hey November, from the same font folder: reserved for Play, and nothing else.
        static inline const Av::Media::FontFamilyPtr Title
            = std::make_shared<Av::Media::FontFamily>("avares://FruityPrime/Assets/Fonts/heyNovember.ttf#Hey November");

        // The display face: a property in C#, because Pixel is declared after it.
        [[nodiscard]] static const Av::Media::FontFamilyPtr& Display() noexcept { return Pixel; }

        // The nearest even point at or below what was wanted, floor of nine.
        [[nodiscard]] static double PixelSize(double wanted);

        // The face every self-drawn control lays its text in.
        [[nodiscard]] static Av::Media::Typeface Face(bool bold);

        // A string laid out on the pixel grid: the size snapped to the face's
        // own step, and nothing else changed.
        [[nodiscard]] static Av::Media::FormattedText Lay(std::string_view text, double size,
            const Av::Media::IBrushPtr& brush, bool bold);

        // Aliased edges, antialiased glyphs, nearest-neighbour bitmaps, set
        // once at the root of a screen and inherited by everything under it.
        static void PixelPerfect(Av::Visual& visual);

        // The window's icon -- the cherry mark alone. Decoded once; a build
        // missing the asset gets no icon rather than a crash.
        [[nodiscard]] static std::shared_ptr<Av::Controls::WindowIcon> AppIcon();

        // Blend towards white or black, for hover and pressed states.
        [[nodiscard]] static Av::Media::Color Shade(Av::Media::Color color, double amount);
    };
}
