#include "DeckChip.hpp"

#include "DeckText.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"
#include "../../../NativeRuntime/System/Globalization.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <tuple>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;
    using ::MphRead::NativeRuntime::MathClamp;
    using ::MphRead::NativeRuntime::MathMax;
    using ::MphRead::NativeRuntime::MathMin;
    using ::MphRead::NativeRuntime::RoundToEven;
    using ::MphRead::NativeRuntime::ToUpperInvariant;

    namespace
    {
        // y, x, width -- on a 12-wide grid, mirrored about x = 6.
        constexpr std::array<std::tuple<int, int, int>, 9> Rows{{{0, 2, 3}, {0, 7, 3}, {1, 1, 10}, {2, 1, 10},
            {3, 1, 10}, {4, 2, 8}, {5, 3, 6}, {6, 4, 4}, {7, 5, 2}}};

        [[nodiscard]] Media::IBrushPtr Solid(Media::Color colour, double opacity = 1.0)
        {
            return std::make_shared<Media::SolidColorBrush>(colour, opacity);
        }
    }

    DeckChip::DeckChip(const std::string& key, const std::string& value)
        : _key(ToUpperInvariant(key)), _value(ToUpperInvariant(value))
    {
        IsHitTestVisible(false);
        Media::RenderOptions::SetEdgeMode(*this, Media::EdgeMode::Aliased);
    }

    void DeckChip::Label(const std::string& value)
    {
        const std::string upper = ToUpperInvariant(value);
        if (_key == upper)
        {
            return;
        }
        _key = upper;
        InvalidateMeasure();
        InvalidateVisual();
    }

    void DeckChip::Text(const std::string& value)
    {
        const std::string upper = ToUpperInvariant(value);
        if (_value == upper)
        {
            return;
        }
        _value = upper;
        InvalidateMeasure();
        InvalidateVisual();
    }

    Media::FormattedText DeckChip::Key(const std::string& s)
    {
        return Media::FormattedText(s, Media::InvariantCulture, Media::FlowDirection::LeftToRight, GuiTheme::Face(false), 10,
            GuiTheme::TextDimBrush);
    }

    Media::FormattedText DeckChip::Value(const std::string& s)
    {
        return Media::FormattedText(s, Media::InvariantCulture, Media::FlowDirection::LeftToRight,
            Media::Typeface(GuiTheme::PixelSemi, Media::FontStyle::Normal, Media::FontWeight::Normal),
            GuiTheme::PixelSize(17), GuiTheme::TextBrush);
    }

    Size DeckChip::MeasureOverride(Av::Size availableSize)
    {
        (void)availableSize;
        const Media::FormattedText v = Value(_value);
        if (_key.empty())
        {
            return {v.Width() + 22, v.Height() + 14};
        }
        const Media::FormattedText k = Key(_key);
        return {MathMax(k.Width(), v.Width()) + 22, k.Height() + v.Height() + 18};
    }

    void DeckChip::DrawKey(Media::DrawingContext& context, const std::string& key, double labelSize, double x,
        double height)
    {
        if (key.empty())
        {
            return;
        }
        const double size = labelSize * 0.5;
        const auto text = DeckText::Run(key, Deck::Body(true), size, GuiTheme::AccentBrush);
        const double w = MathMax(size * 1.5, text->Width() + size * 0.7);
        const double h = MathMax(size * 1.5, text->Height());
        const Rect box(x, RoundToEven((height - h) / 2), w, h);
        context.DrawRectangle(Solid(Media::Color::FromArgb(107, 0, 0, 0)), nullptr, RoundedRect(box, size * 0.3));
        context.DrawText(*text, Point{RoundToEven(box.X + (w - text->Width()) / 2),
            RoundToEven(box.Y + (h - text->Height()) / 2)});
    }

    void DeckChip::Render(Media::DrawingContext& context)
    {
        const double w = Bounds().Width;
        const double h = Bounds().Height - 4;
        const Media::FormattedText v = Value(_value);

        context.DrawRectangle(Solid(Media::Color::FromArgb(210, 18, 21, 28)), nullptr, RoundedRect(Rect(0, 0, w, h), 7));
        context.DrawRectangle(Solid(Media::Color::FromArgb(128, 0, 0, 0)), nullptr, RoundedRect(Rect(0, h, w, 4), 2));

        if (_key.empty())
        {
            // .code: the value alone, centred in the chip.
            context.DrawText(v, Point{RoundToEven((w - v.Width()) / 2), RoundToEven((h - v.Height()) / 2)});
            return;
        }
        const Media::FormattedText k = Key(_key);
        context.DrawText(k, Point{RoundToEven((w - k.Width()) / 2), 5});
        // The value sits in its own well.
        const double vy = RoundToEven(k.Height() + 7);
        context.DrawRectangle(Solid(GuiTheme::PanelLight), nullptr, RoundedRect(Rect(5, vy - 2, w - 10, v.Height() + 4), 4));
        context.DrawText(v, Point{RoundToEven((w - v.Width()) / 2), vy});
    }

    // ------------------------------------------------------------ DeckHeart

    DeckHeart::DeckHeart()
    {
        static const bool registered = []
        {
            AffectsMeasure<DeckHeart>(Deck::EmProperty);
            AffectsRender<DeckHeart>(Deck::EmProperty);
            return true;
        }();
        (void)registered;
        Focusable(true);
        Cursor(std::make_shared<Input::Cursor>(Input::StandardCursorType::Hand));
        Media::RenderOptions::SetEdgeMode(*this, Media::EdgeMode::Aliased);
    }

    void DeckHeart::DrawHeart(Media::DrawingContext& context, Rect area, const Media::IBrushPtr& ink)
    {
        // Whole pixels, so the heart stays a pixel heart. Twelve by eight.
        const double cell = MathMax(1.0, std::floor(MathMin(area.Width / 12, area.Height / 8)));
        const double ox = RoundToEven(area.X + (area.Width - 12 * cell) / 2);
        const double oy = RoundToEven(area.Y + (area.Height - 8 * cell) / 2);
        for (const auto& [y, x, w] : Rows)
        {
            context.FillRectangle(ink, Rect(ox + x * cell, oy + y * cell, w * cell, cell));
        }
    }

    double DeckHeart::TipAmount(double seconds) const
    {
        const double t = _tip.IsRunning() ? _tip.Elapsed().TotalSeconds() : std::numeric_limits<double>::max();
        const double progress = seconds <= 0 ? 1 : MathClamp(t / seconds, 0.0, 1.0);
        return _tipShown ? progress : 1 - progress;
    }

    void DeckHeart::TipTo(bool shown)
    {
        if (_tipShown == shown)
        {
            return;
        }
        _tipShown = shown;
        if (Deck::Still())
        {
            return;
        }
        _tip.Restart();
        AskFrame();
    }

    void DeckHeart::AskFrame()
    {
        if (_framePending || Deck::Still())
        {
            return;
        }
        _framePending = true;
        const std::shared_ptr<DeckHeart> self = std::static_pointer_cast<DeckHeart>(shared_from_this());
        Deck::NextFrame(*this, [self]
        {
            self->_framePending = false;
            self->InvalidateVisual();
        });
    }

    void DeckHeart::Tip_(Media::DrawingContext& context, double w, double em)
    {
        DrawTip(context, Tip, w, em, TipAmount(TipFade), Deck::Spring(TipAmount(TipRise)));
        if (_tip.IsRunning() && _tip.Elapsed().TotalSeconds() >= MathMax(TipFade, TipRise))
        {
            _tip.Reset();
        }
        else if (_tip.IsRunning())
        {
            AskFrame();
        }
    }

    void DeckHeart::DrawTip(Media::DrawingContext& context, const std::string& tip, double w, double em, double show,
        double risen)
    {
        if (show <= 0.001 || tip.empty())
        {
            return;
        }
        const double size = GuiTheme::PixelSize(em * 0.4);
        const auto text = DeckText::Run(tip, Deck::Body(false), size, Solid(GuiTheme::Text, show));
        const double padX = RoundToEven(size * 0.7);
        const double padY = RoundToEven(size * 0.45);
        const double tw = RoundToEven(text->Width() + padX * 2);
        const double th = RoundToEven(text->Height() + padY * 2);
        // bottom: calc(100% + .8em); right: 0, sliding the last four points up.
        const double slide = 4 * (1 - risen);
        const double x = w - tw;
        const double y = -th - RoundToEven(em * 0.8) + slide;
        const RoundedRect box(Rect(x, y, tw, th), RoundToEven(size * 0.35));
        context.DrawRectangle(Solid(GuiTheme::Panel, show),
            std::make_shared<Media::Pen>(Solid(GuiTheme::Edge, show), 1), box,
            Media::BoxShadows(Deck::Shadow(0, 4, 0, 0,
                                  Media::Color::FromArgb(static_cast<std::uint8_t>(RoundToEven(255 * show)),
                                      GuiTheme::PanelDeep.R, GuiTheme::PanelDeep.G, GuiTheme::PanelDeep.B)),
                {Deck::Shadow(0, 8, 16, 0, Deck::Fade(0, 0.6 * show))}));
        context.DrawText(*text, Point{RoundToEven(x + padX), RoundToEven(y + padY)});
    }

    Size DeckHeart::MeasureOverride(Av::Size availableSize)
    {
        (void)availableSize;
        const double em = Em();
        return {RoundToEven(em * (GlyphWide + PadX * 2)), RoundToEven(em * (GlyphTall + PadY * 2))};
    }

    void DeckHeart::OnPointerEntered(Input::PointerEventArgs& e)
    {
        TipTo(true);
        InvalidateVisual();
        Control::OnPointerEntered(e);
    }

    void DeckHeart::OnPointerExited(Input::PointerEventArgs& e)
    {
        TipTo(false);
        InvalidateVisual();
        Control::OnPointerExited(e);
    }

    void DeckHeart::OnGotFocus(Input::GotFocusEventArgs& e)
    {
        TipTo(Deck::KeyboardDriving());
        InvalidateVisual();
        Control::OnGotFocus(e);
    }

    void DeckHeart::OnLostFocus(Input::FocusChangedEventArgs& e)
    {
        if (!IsPointerOver())
        {
            TipTo(false);
        }
        InvalidateVisual();
        Control::OnLostFocus(e);
    }

    void DeckHeart::OnPointerPressed(Input::PointerPressedEventArgs& e)
    {
        _tap.Press(e, *this);
        Focus();
        e.Pointer->Capture(this);
        e.Handled = true;
        InvalidateVisual();
        Control::OnPointerPressed(e);
    }

    void DeckHeart::OnPointerReleased(Input::PointerReleasedEventArgs& e)
    {
        if (_tap.Release(e, *this))
        {
            e.Handled = true;
            Click(*this);
        }
        InvalidateVisual();
        Control::OnPointerReleased(e);
    }

    void DeckHeart::Render(Media::DrawingContext& context)
    {
        const double w = Bounds().Width;
        const double h = Bounds().Height;
        if (w <= 0 || h <= 0)
        {
            return;
        }
        const bool hot = IsPointerOver() || IsFocused();
        const double em = Em();
        Tip_(context, w, em);

        context.DrawRectangle(Solid(hot ? Media::Color::FromRgb(0x84, 0x42, 0x42) : Media::Color::FromRgb(0x6b, 0x36, 0x36)),
            nullptr, RoundedRect(Rect(0, 0, w, h), em * 0.55));
        context.DrawRectangle(Solid(Media::Color::FromRgb(0x38, 0x1b, 0x1b)), nullptr, RoundedRect(Rect(0, h, w, Lip), em * 0.3));

        // 12 x 8 cells, centred, at whole-pixel scale, sized off the glyph the
        // reference asks for.
        const double cell = MathMax(1.0, std::floor(em * GlyphWide / 12));
        const double ox = RoundToEven((w - 12 * cell) / 2);
        const double oy = RoundToEven((h - 8 * cell) / 2);
        const Media::IBrushPtr ink = Solid(hot ? Media::Color::FromRgb(0xff, 0xd0, 0xd0) : Media::Color::FromRgb(0xe8, 0xa0, 0xa0));
        for (const auto& [y, x, rw] : Rows)
        {
            context.FillRectangle(ink, Rect(ox + x * cell, oy + y * cell, rw * cell, cell));
        }
    }
}
