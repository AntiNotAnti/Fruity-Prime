#include "UiLayout.hpp"

#include "BakedBackdrop.hpp"
#include "ControllerNav.hpp"
#include "DeckCard.hpp"
#include "DeckSheet.hpp"
#include "MovingBackdrop.hpp"
#include "UiMark.hpp"
#include "../../../NativeRuntime/System/Globalization.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"

#if !defined(__ANDROID__)
#include "../../Render/LauncherPhoto.hpp"
#endif

#include <algorithm>
#include <cmath>
#include <exception>
#include <mutex>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;
    using ::MphRead::NativeRuntime::RoundToEven;

    namespace
    {
        // A Grid whose row gap is an em rather than a number, written only
        // when it has moved.
        class GapGrid final : public Controls::Grid
        {
        public:
            GapGrid(double gapEms, std::string_view rows)
                : _gapEms(gapEms)
            {
                RowDefinitions(Controls::RowDefinitions(rows));
            }

        protected:
            Size MeasureOverride(Size availableSize) override
            {
                const double gap = RoundToEven(Deck::GetEm(*this) * _gapEms);
                if (std::abs(gap - RowSpacing()) > 0.01)
                {
                    RowSpacing(gap);
                }
                return Grid::MeasureOverride(availableSize);
            }

        private:
            const double _gapEms;
        };

        // The same, for the row of marks: a flex gap in ems.
        class GapDock final : public Controls::DockPanel
        {
        public:
            explicit GapDock(double gapEms)
                : _gapEms(gapEms)
            {
            }

        protected:
            Size MeasureOverride(Size availableSize) override
            {
                const double gap = RoundToEven(Deck::GetEm(*this) * _gapEms);
                for (const Controls::ControlPtr& child : Children)
                {
                    const Thickness want = GetDock(*child) == Controls::Dock::Right ? Thickness(gap, 0, 0, 0) : Thickness(0);
                    if (child->Margin() != want)
                    {
                        child->Margin(want);
                    }
                }
                return DockPanel::MeasureOverride(availableSize);
            }

        private:
            const double _gapEms;
        };

        // The sheet's own padding: 1.1em .9em, around the one panel it holds.
        class SheetPad final : public Controls::Decorator
        {
        protected:
            Size MeasureOverride(Size availableSize) override
            {
                const double em = Deck::GetEm(*this);
                const Thickness want(RoundToEven(em * 0.9), RoundToEven(em * 1.1), RoundToEven(em * 0.9), RoundToEven(em * 1.1));
                if (Padding() != want)
                {
                    Padding(want);
                }
                return Decorator::MeasureOverride(availableSize);
            }
        };
    }

    std::shared_ptr<Controls::Border> UiLayout::Wash(std::uint8_t core, std::uint8_t edge)
    {
        auto brush = std::make_shared<Media::LinearGradientBrush>();
        brush->StartPoint = RelativePoint(0.5, 0, RelativeUnit::Relative);
        brush->EndPoint = RelativePoint(0.5, 1, RelativeUnit::Relative);
        brush->GradientStops = {Media::GradientStop(Media::Color::FromArgb(edge, 10, 12, 16), 0),
            Media::GradientStop(Media::Color::FromArgb(core, 10, 12, 16), 0.16),
            Media::GradientStop(Media::Color::FromArgb(core, 10, 12, 16), 0.88),
            Media::GradientStop(Media::Color::FromArgb(edge, 10, 12, 16), 1)};
        auto border = std::make_shared<Controls::Border>();
        border->Background(brush);
        return border;
    }

    Media::Color UiLayout::Void(double alpha)
    {
        return Media::Color::FromArgb(static_cast<std::uint8_t>(RoundToEven(std::clamp(alpha, 0.0, 1.0) * 255)), 5, 7, 10);
    }

    std::shared_ptr<Controls::Border> UiLayout::Ground(bool horizontal)
    {
        auto brush = std::make_shared<Media::LinearGradientBrush>();
        brush->StartPoint = RelativePoint(horizontal ? 0 : 0.5, horizontal ? 0.5 : 0, RelativeUnit::Relative);
        brush->EndPoint = RelativePoint(horizontal ? 1 : 0.5, horizontal ? 0.5 : 1, RelativeUnit::Relative);
        if (horizontal)
        {
            brush->GradientStops.emplace_back(Void(0.55), 0);
            brush->GradientStops.emplace_back(Void(0), 0.38);
            brush->GradientStops.emplace_back(Void(0), 1);
        }
        else
        {
            brush->GradientStops.emplace_back(Void(0.72), 0);
            brush->GradientStops.emplace_back(Void(0.10), 0.30);
            brush->GradientStops.emplace_back(Void(0.35), 0.62);
            brush->GradientStops.emplace_back(Void(0.90), 1);
        }
        auto border = std::make_shared<Controls::Border>();
        border->Background(brush);
        return border;
    }

    bool UiLayout::PhotoDrawnBelow()
    {
#if !defined(__ANDROID__)
        return ::MphRead::Mods::Render::LauncherPhoto::Enabled();
#else
        return false;
#endif
    }

    double UiLayout::Factor(double width, double height)
    {
        const double room = std::max(height, 1.0) / 720.0;
        const double raw = std::pow(room, 1.5);
        const double fits = std::min(std::max(width, 1.0) / MinBoxWidth, std::max(height, 1.0) / MinBoxHeight);
        // The curve rounds to the nearest eighth and the cap rounds down.
        const double stepped = std::min(RoundToEven(raw * 8), std::floor(fits * 8)) / 8;
        return std::clamp(stepped, 0.6, 4.0);
    }

    std::shared_ptr<Controls::Panel> UiLayout::Marks(const std::vector<std::shared_ptr<UiMark>>& marks)
    {
        auto row = std::make_shared<GapDock>(0.6);
        row->LastChildFill(false);
        row->VerticalAlignment(Layout::VerticalAlignment::Center);
        // Docked right in reverse, so they read left to right along the right.
        for (std::size_t i = marks.size(); i-- > 1;)
        {
            const std::shared_ptr<UiMark>& mark = marks[i];
            if (mark == nullptr)
            {
                continue;
            }
            mark->VerticalAlignment(Layout::VerticalAlignment::Center);
            Controls::DockPanel::SetDock(*mark, Controls::Dock::Right);
            row->Children.Add(mark);
        }
        if (!marks.empty() && marks[0] != nullptr)
        {
            const std::shared_ptr<UiMark>& cancel = marks[0];
            cancel->VerticalAlignment(Layout::VerticalAlignment::Center);
            Controls::DockPanel::SetDock(*cancel, Controls::Dock::Left);
            row->Children.Add(cancel);
        }
        return row;
    }

    std::shared_ptr<Controls::Panel> UiLayout::Page(bool overGame, double widthEms, const std::string& heading,
        const Controls::ControlPtr& strip, const Controls::ControlPtr& body, const std::shared_ptr<UiMark>& no,
        const std::shared_ptr<UiMark>& yes, bool centreBody, const std::shared_ptr<UiMark>& extra,
        const Controls::ControlPtr& note)
    {
        // The wash goes into the backdrop rather than over it.
        std::shared_ptr<Controls::Panel> root = Backdrop(overGame, overGame ? BackdropWash::None : BackdropWash::Standard);
        root->SetValue(ControllerNav::NavScopeProperty, std::optional<std::string>("page"));
        if (no != nullptr)
        {
            ControllerNav::Identify(*no, "page.back");
        }
        if (yes != nullptr)
        {
            ControllerNav::Identify(*yes, "page.accept");
        }
        if (extra != nullptr)
        {
            ControllerNav::Identify(*extra, "page.extra");
        }
        if (no != nullptr && yes != nullptr)
        {
            no->SetValue(ControllerNav::NavRightProperty, std::optional<std::string>("page.accept"));
            yes->SetValue(ControllerNav::NavLeftProperty, std::optional<std::string>("page.back"));
        }
        const bool hasMarks = no != nullptr || yes != nullptr || extra != nullptr;

        // The heading, for the screens with no strip.
        Controls::ControlPtr title;
        if (strip == nullptr && !heading.empty())
        {
            title = Heading(::MphRead::NativeRuntime::ToLowerInvariant(heading));
        }

        auto inside = std::make_shared<GapGrid>(0.65, "Auto,*,Auto");
        if (title != nullptr)
        {
            title->HorizontalAlignment(Layout::HorizontalAlignment::Center);
            Controls::Grid::SetRow(*title, 0);
            inside->Children.Add(title);
        }
        else if (strip != nullptr)
        {
            strip->HorizontalAlignment(Layout::HorizontalAlignment::Center);
            Controls::Grid::SetRow(*strip, 0);
            inside->Children.Add(strip);
        }
        Controls::Grid::SetRow(*body, 1);
        if (centreBody)
        {
            body->VerticalAlignment(Layout::VerticalAlignment::Center);
        }
        inside->Children.Add(body);

        if (hasMarks || note != nullptr)
        {
            // .foot: the row of marks, and the line under it, .45em apart.
            auto foot = std::make_shared<GapGrid>(0.45, hasMarks ? "Auto,Auto" : "Auto");
            if (hasMarks)
            {
                const std::shared_ptr<Controls::Panel> marks = Marks({no, extra, yes});
                Controls::Grid::SetRow(*marks, 0);
                foot->Children.Add(marks);
            }
            if (note != nullptr)
            {
                Controls::Grid::SetRow(*note, hasMarks ? 1 : 0);
                foot->Children.Add(note);
            }
            Controls::Grid::SetRow(*foot, 2);
            inside->Children.Add(foot);
        }

        // The panel: min(44em, 100%) wide, its own height, centred.
        auto card = std::make_shared<DeckCard>();
        card->Child(inside);
        card->MaxWidthEms = widthEms;
        // .sheet: the scrim and the panel on it, fading in together.
        auto sheet = std::make_shared<DeckSheet>();
        auto scrim = std::make_shared<Controls::Border>();
        scrim->Background(SheetBrush);
        sheet->Children.Add(scrim);
        auto pad = std::make_shared<SheetPad>();
        pad->Child(card);
        sheet->Children.Add(pad);
        root->Children.Add(sheet);
        return root;
    }

    std::shared_ptr<Media::Imaging::Bitmap> UiLayout::Load(const std::string& asset)
    {
        try
        {
            return Media::Imaging::Bitmap::FromBytes(Platform::AssetLoader::Open("avares://FruityPrime/Assets/" + asset));
        }
        catch (const std::exception&)
        {
            // A build missing the asset gets no picture rather than no launcher.
            return nullptr;
        }
    }

    const std::shared_ptr<Media::Imaging::Bitmap>& UiLayout::Background()
    {
        static std::once_flag once;
        static std::shared_ptr<Media::Imaging::Bitmap> image;
        std::call_once(once, [] { image = Load("Backgrounds/launcher-bg.jpg"); });
        return image;
    }

    const std::shared_ptr<Media::Imaging::Bitmap>& UiLayout::WordmarkImage()
    {
        static std::once_flag once;
        static std::shared_ptr<Media::Imaging::Bitmap> image;
        std::call_once(once, [] { image = Load("fruity-prime-logo.png"); });
        return image;
    }

    std::shared_ptr<Controls::Panel> UiLayout::Backdrop(bool overGame, BackdropWash wash)
    {
        // A DeckStage, not a bare Panel: this is the frame, and the frame is
        // where the em comes from.
        auto root = std::make_shared<DeckStage>();
        if (overGame)
        {
            auto scrim = std::make_shared<Controls::Border>();
            scrim->Background(GuiTheme::ScrimBrush);
            root->Children.Add(scrim);
            GuiTheme::PixelPerfect(*root);
            return root;
        }
        if (PhotoDrawnBelow())
        {
            // GL has the photograph and the moving layer over it.
            root->Children.Add(std::make_shared<BakedBackdrop>(wash));
        }
        else
        {
            // Three pieces, so the moving layer lands between the photograph
            // and the washes.
            root->Children.Add(std::make_shared<BakedBackdrop>(wash, BackdropPart::Photo));
            root->Children.Add(std::make_shared<MovingBackdrop>());
            root->Children.Add(std::make_shared<BakedBackdrop>(wash, BackdropPart::Washes));
        }
        // One stamp at the root of every screen.
        GuiTheme::PixelPerfect(*root);
        return root;
    }

    std::shared_ptr<Controls::Panel> UiLayout::BackdropLayers(BackdropWash wash, BackdropPart part)
    {
        (void)wash;
        auto root = std::make_shared<Controls::Panel>();
        const bool photoBelow = PhotoDrawnBelow();
        if (!photoBelow && part != BackdropPart::Washes)
        {
            auto image = std::make_shared<Controls::Image>();
            image->Source(Background());
            image->Stretch(Media::Stretch::UniformToFill);
            root->Children.Add(image);
        }
        // #ground, both gradients, in the order CSS paints them.
        if (part != BackdropPart::Photo)
        {
            root->Children.Add(Ground(true));
            root->Children.Add(Ground(false));
        }
        return root;
    }

    std::shared_ptr<Controls::StackPanel> UiLayout::Column(double spacing)
    {
        auto column = std::make_shared<Controls::StackPanel>();
        column->Spacing(spacing);
        column->HorizontalAlignment(Layout::HorizontalAlignment::Left);
        column->VerticalAlignment(Layout::VerticalAlignment::Bottom);
        column->Margin(Thickness(ColumnLeft, 0, 0, ColumnBottom));
        return column;
    }

    std::shared_ptr<Controls::TextBlock> UiLayout::Footer(const std::string& text)
    {
        auto block = std::make_shared<Controls::TextBlock>();
        block->Text(text);
        block->FontFamily(GuiTheme::Display());
        block->FontSize(12);
        block->Foreground(GuiTheme::TextDimBrush);
        block->HorizontalAlignment(Layout::HorizontalAlignment::Left);
        block->VerticalAlignment(Layout::VerticalAlignment::Bottom);
        block->Margin(Thickness(ColumnLeft - 50, 0, 0, FooterBottom));
        return block;
    }

    std::shared_ptr<Controls::Image> UiLayout::Wordmark()
    {
        auto image = std::make_shared<Controls::Image>();
        image->Source(WordmarkImage());
        image->Stretch(Media::Stretch::Uniform);
        image->Width(220);
        image->HorizontalAlignment(Layout::HorizontalAlignment::Right);
        image->VerticalAlignment(Layout::VerticalAlignment::Bottom);
        image->Margin(Thickness(0, 0, 32, 28));
        image->Opacity(0.92);
        return image;
    }

    std::shared_ptr<Controls::TextBlock> UiLayout::Heading(const std::string& text)
    {
        auto block = std::make_shared<Controls::TextBlock>();
        block->Text(::MphRead::NativeRuntime::ToLowerInvariant(text));
        block->FontFamily(GuiTheme::Display());
        block->FontSize(HeadingSize);
        block->Foreground(GuiTheme::TextDimBrush);
        block->HorizontalAlignment(Layout::HorizontalAlignment::Left);
        block->VerticalAlignment(Layout::VerticalAlignment::Top);
        block->Margin(Thickness(ColumnLeft, 26, 0, 0));
        return block;
    }
}
