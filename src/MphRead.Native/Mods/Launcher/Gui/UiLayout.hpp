#pragma once

#include "Deck.hpp"

#include <memory>
#include <string>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    class UiMark;

    // The one layout every screen in the launcher is built from.
    class UiLayout final
    {
    public:
        UiLayout() = delete;

        // Where the column of words starts, and how far it clears the bottom edge.
        static constexpr double ColumnLeft = 72;
        static constexpr double ColumnBottom = 96;
        // The corner marks, and the dim line under the column.
        static constexpr double CornerX = 64;
        static constexpr double CornerY = 38;
        static constexpr double FooterBottom = 18;
        // A menu word, and the one word a screen is named by.
        static constexpr double WordSize = 20;
        static constexpr double HeadingSize = 15;

        // Where a screen's own content sits: clear of the tabs and both corners.
        [[nodiscard]] static Av::Thickness BodyMargin() { return Av::Thickness(ColumnLeft, 104, CornerX, 96); }
        // Where the tab strip sits: the top-left corner, above the body.
        [[nodiscard]] static Av::Thickness TabMargin() { return Av::Thickness(ColumnLeft, 52, 0, 0); }

        // How wide the well is, in ems.
        static constexpr double WellPlay = 44;
        static constexpr double WellSettings = 44;
        // A question, a menu, a progress log: content, not a table.
        static constexpr double WellShort = 19;
        // What the well clears at the top, and at the foot for the marks.
        static constexpr double WellTop = 44;
        static constexpr double WellBottom = 84;
        // The least ground either side of the well.
        static constexpr double WellGutter = 20;
        // Where the pair of marks sits, and how far apart.
        static constexpr double MarksBottom = 28;
        static constexpr double MarksGap = 64;

        // The wash the well is read against.
        [[nodiscard]] static std::shared_ptr<Av::Controls::Border> Wash(std::uint8_t core = 228, std::uint8_t edge = 120);
        // The front screen's wash: the same gradient, a third of the weight.
        [[nodiscard]] static std::shared_ptr<Av::Controls::Border> LightWash() { return Wash(140, 55); }

        // Which wash a backdrop carries, so that it can be baked in.
        enum class BackdropWash
        {
            // Nothing over the photograph.
            None,
            // What every screen behind the front one is read against.
            Standard,
            // The front screen's: three words need almost no ground.
            Light
        };

        // Which slice of the backdrop a bake holds.
        enum class BackdropPart
        {
            All,
            Photo,
            Washes
        };

        // How many device pixels one layout point is, for whoever is cutting
        // a bitmap rather than drawing into the frame.
        static inline double BakeScale = 1;

        // How much bigger than its own layout a box this tall draws the
        // screens: steeper than proportion, capped by what the box can hold.
        [[nodiscard]] static double Factor(double width, double height);

        // The smallest layout box the screens are allowed to be given.
        static constexpr double MinBoxWidth = 960;
        static constexpr double MinBoxHeight = 600;
        // How short a box has to be before the well stops spending a third of
        // it on its own margins.
        static constexpr double ShortBox = 560;

        // Leaving on the left, everything else on the right.
        [[nodiscard]] static std::shared_ptr<Av::Controls::Panel> Marks(const std::vector<std::shared_ptr<UiMark>>& marks);

        // .sheet's ground, rgba(5,7,10,.72).
        static inline const Av::Media::IBrushPtr SheetBrush
            = std::make_shared<Av::Media::SolidColorBrush>(Av::Media::Color::FromArgb(184, 5, 7, 10));

        // A whole screen: the backdrop, the sheet over it, and one panel
        // centred on that, three rows inside.
        [[nodiscard]] static std::shared_ptr<Av::Controls::Panel> Page(bool overGame, double widthEms,
            const std::string& heading, const Av::Controls::ControlPtr& strip, const Av::Controls::ControlPtr& body,
            const std::shared_ptr<UiMark>& no = nullptr, const std::shared_ptr<UiMark>& yes = nullptr,
            bool centreBody = false, const std::shared_ptr<UiMark>& extra = nullptr,
            const Av::Controls::ControlPtr& note = nullptr);

        // What every screen is painted on.
        [[nodiscard]] static std::shared_ptr<Av::Controls::Panel> Backdrop(bool overGame = false,
            BackdropWash wash = BackdropWash::None);
        // The backdrop layers themselves. The native Ganesh head keeps them
        // live on the GPU; off-screen software rendering can draw them directly too.
        [[nodiscard]] static std::shared_ptr<Av::Controls::Panel> BackdropLayers(BackdropWash wash,
            BackdropPart part = BackdropPart::All);

        // The column of words, anchored where the front screen's is.
        [[nodiscard]] static std::shared_ptr<Av::Controls::StackPanel> Column(double spacing = 16);
        // The dim line under the column.
        [[nodiscard]] static std::shared_ptr<Av::Controls::TextBlock> Footer(const std::string& text);
        // The mark in the opposite corner, at the size the front screen uses.
        [[nodiscard]] static std::shared_ptr<Av::Controls::Image> Wordmark();
        // What a screen is called, lower case and dim.
        [[nodiscard]] static std::shared_ptr<Av::Controls::TextBlock> Heading(const std::string& text);

    private:
        [[nodiscard]] static Av::Media::Color Void(double alpha);
        [[nodiscard]] static std::shared_ptr<Av::Controls::Border> Ground(bool horizontal);
        [[nodiscard]] static bool PhotoDrawnBelow();
        [[nodiscard]] static std::shared_ptr<Av::Media::Imaging::Bitmap> Load(const std::string& asset);
        [[nodiscard]] static const std::shared_ptr<Av::Media::Imaging::Bitmap>& Background();
        [[nodiscard]] static const std::shared_ptr<Av::Media::Imaging::Bitmap>& WordmarkImage();
    };
}
