#include "DeckTile.hpp"

#include "DeckButton.hpp"
#include "DeckText.hpp"
#include "MapShot.hpp"
#include "../../../NativeRuntime/System/Exceptions.hpp"
#include "../../../NativeRuntime/System/Globalization.hpp"
#include "../../../NativeRuntime/System/HashCode.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;
    using ::MphRead::NativeRuntime::MathClamp;
    using ::MphRead::NativeRuntime::MathMax;
    using ::MphRead::NativeRuntime::MathMin;
    using ::MphRead::NativeRuntime::RoundToEven;
    using ::MphRead::NativeRuntime::StringGetHashCode;

    namespace
    {
        [[nodiscard]] Media::IBrushPtr Solid(Media::Color colour)
        {
            return std::make_shared<Media::SolidColorBrush>(colour);
        }
    }

    DeckTile::DeckTile(std::string roomKey, std::string code)
        : RoomKey(std::move(roomKey)), Code(std::move(code))
    {
        static const bool registered = []
        {
            AffectsRender<DeckTile>(Deck::EmProperty);
            return true;
        }();
        (void)registered;
        Focusable(true);
        Cursor(std::make_shared<Input::Cursor>(Input::StandardCursorType::Hand));
    }

    void DeckTile::Chosen(bool value)
    {
        if (_chosen == value)
        {
            return;
        }
        _chosen = value;
        InvalidateVisual();
    }

    Av::Size DeckTile::MeasureOverride(Av::Size availableSize)
    {
        // The grid gives the width; the ratio gives the height.
        const double side = std::isinf(availableSize.Width) ? Em() * 10 : availableSize.Width;
        return {side, RoundToEven(side / MathMax(0.1, Ratio))};
    }

    void DeckTile::Hover(bool over, Point at)
    {
        _over = over;
        _popTarget = over || (IsFocused() && Deck::KeyboardDriving()) ? 1.03 : 1;
        if (over && Bounds().Width > 0 && Bounds().Height > 0)
        {
            const double halfW = Bounds().Width / 2;
            const double halfH = Bounds().Height / 2;
            _tiltTarget = MathClamp((at.X - halfW) / halfW, -1.0, 1.0) * MaxTilt;
            _tiltXTarget = -MathClamp((at.Y - halfH) / halfH, -1.0, 1.0) * MaxTilt;
        }
        else
        {
            _tiltTarget = 0;
            _tiltXTarget = 0;
        }
        InvalidateVisual();
    }

    void DeckTile::OnKeyDown(Input::KeyEventArgs& e)
    {
        if (e.Key == Input::Key::Enter || e.Key == Input::Key::Space)
        {
            e.Handled = true;
            Click(*this);
            return;
        }
        Control::OnKeyDown(e);
    }

    void DeckTile::OnGotFocus(Input::GotFocusEventArgs& e)
    {
        _popTarget = 1.03;
        InvalidateVisual();
        Control::OnGotFocus(e);
    }

    void DeckTile::OnLostFocus(Input::FocusChangedEventArgs& e)
    {
        if (!_over)
        {
            _popTarget = 1;
        }
        InvalidateVisual();
        Control::OnLostFocus(e);
    }

    bool DeckTile::Settle()
    {
        const ::MphRead::NativeRuntime::TimeSpan now = _clock.Elapsed();
        const double dt = MathMin(0.05, (now - _last).TotalSeconds());
        _last = now;
        if (Deck::Still())
        {
            _pop = _popTarget;
            _popVelocity = 0;
            _tilt = _tiltTarget;
            _tiltX = _tiltXTarget;
            return false;
        }
        if (dt <= 0)
        {
            return false;
        }
        const double accel = (_popTarget - _pop) * Stiffness - _popVelocity * Damping;
        _popVelocity += accel * dt;
        _pop += _popVelocity * dt;
        const double ease = MathMin(1.0, dt * 14);
        _tilt += (_tiltTarget - _tilt) * ease;
        _tiltX += (_tiltXTarget - _tiltX) * ease;
        const bool moving = std::abs(_popTarget - _pop) > 0.0005 || std::abs(_popVelocity) > 0.0005
            || std::abs(_tiltTarget - _tilt) > 0.02 || std::abs(_tiltXTarget - _tiltX) > 0.02;
        if (!moving)
        {
            _pop = _popTarget;
            _popVelocity = 0;
            _tilt = _tiltTarget;
            _tiltX = _tiltXTarget;
        }
        return moving;
    }

    void DeckTile::Ask()
    {
        if (_framePending || Deck::Still())
        {
            return;
        }
        _framePending = true;
        const std::shared_ptr<DeckTile> self = std::static_pointer_cast<DeckTile>(shared_from_this());
        Deck::NextFrame(*this, [self]
        {
            self->_framePending = false;
            self->InvalidateVisual();
        });
    }

    void DeckTile::Render(Media::DrawingContext& context)
    {
        const bool moving = Settle();
        const double w = Bounds().Width;
        const double h = Bounds().Height;
        if (w <= 0 || h <= 0)
        {
            return;
        }
        const double em = Em();
        const double radius = em * 0.55;
        const bool hot = _over || (IsFocused() && Deck::KeyboardDriving());

        // Live cards draw directly into the current Ganesh/OpenGL surface.
        // Never bake the card, chrome, or map ground into a CPU bitmap.
        auto transform = context.PushTransform(Matrix::CreateTranslation(-w / 2, -h / 2)
            * Matrix::CreateScale(_pop, _pop)
            * Matrix::CreateRotation((_tilt + _tiltX) * std::numbers::pi / 180 * 0.12)
            * Matrix::CreateTranslation(w / 2, h / 2));
        const RoundedRect face(Rect(0, 0, w, h), radius);
        const Media::Color ring = _chosen || Leader ? GuiTheme::Accent
            : hot ? Media::Color::FromRgb(0x4a, 0x6f, 0x8c) : GuiTheme::Edge;
        const bool raised = hot || _chosen;
        context.DrawRectangle(Solid(GuiTheme::PanelDeep), nullptr, face, Shadows(ring, raised));

        {
            auto clip = context.PushClip(face);
            const std::shared_ptr<Media::Imaging::Bitmap> shot = MapShot::For(RoomKey);
            if (shot != nullptr)
            {
                const double sw = shot->PixelSize().Width;
                const double sh = shot->PixelSize().Height;
                if (sw > 0 && sh > 0)
                {
                    const double scale = MathMax(w / sw, h / sh);
                    const double dw = sw * scale;
                    const double dh = sh * scale;
                    context.DrawImage(*shot, Rect((w - dw) / 2, (h - dh) / 2, dw, dh));
                }
            }
            Drift(context, w, h);
            Scrim(context, w, h);
            Info(context, w, h, em);
            Badge(context, w, em);
        }

        if (moving)
        {
            Ask();
        }
    }

    void DeckTile::Badge(Media::DrawingContext& context, double w, double em) const
    {
        if (Tally < 0)
        {
            return;
        }
        const double size = GuiTheme::PixelSize(em);
        const auto count = DeckText::Run(std::to_string(Tally), Deck::Label(), size,
            Solid(Tally > 0 ? Media::Colors::White : GuiTheme::TextDim));
        const double side = MathMax(em * 1.7, count->Width() + em * 0.7);
        const double high = em * 1.7;
        const Rect box(RoundToEven(w - em * 0.4 - side), RoundToEven(em * 0.4), RoundToEven(side), RoundToEven(high));
        context.DrawRectangle(Solid(Tally > 0 ? Deck::Face::Brass().Fill : Deck::Fade(0x0a0c10, 0.85)),
            std::make_shared<Media::Pen>(Solid(Tally > 0 ? Media::Color::FromRgb(0xc9, 0xa2, 0x27) : GuiTheme::Edge), 1),
            RoundedRect(box, RoundToEven(em * 0.35)));
        context.DrawText(*count, Point{RoundToEven(box.X + (box.Width - count->Width()) / 2),
            RoundToEven(box.Y + (box.Height - count->Height()) / 2)});
    }

    void DeckTile::Drift(Media::DrawingContext& context, double w, double h) const
    {
        // The phase the reference would be at, from the room key.
        const std::int32_t hash = StringGetHashCode(RoomKey);
        if (hash == std::numeric_limits<std::int32_t>::min())
        {
            throw ::System::OverflowException();
        }
        const std::uint32_t magnitude = static_cast<std::uint32_t>(hash < 0 ? -hash : hash);
        const double phase = static_cast<double>(magnitude % 1000U) / 1000.0;
        const double dx = (phase - 0.5) * 0.08 * w;
        const double dy = (phase - 0.5) * 0.06 * h;
        auto warm = std::make_shared<Media::RadialGradientBrush>();
        warm->Center = RelativePoint(0.30, 0.40, RelativeUnit::Relative);
        warm->GradientOrigin = RelativePoint(0.30, 0.40, RelativeUnit::Relative);
        warm->RadiusX = RelativeScalar(0.45, RelativeUnit::Relative);
        warm->RadiusY = RelativeScalar(0.55, RelativeUnit::Relative);
        warm->GradientStops = {Media::GradientStop(Media::Color::FromArgb(56, 0xff, 0xb3, 0x47), 0),
            Media::GradientStop(Media::Color::FromArgb(0, 0xff, 0xb3, 0x47), 1)};
        auto cool = std::make_shared<Media::RadialGradientBrush>();
        cool->Center = RelativePoint(0.72, 0.65, RelativeUnit::Relative);
        cool->GradientOrigin = RelativePoint(0.72, 0.65, RelativeUnit::Relative);
        cool->RadiusX = RelativeScalar(0.50, RelativeUnit::Relative);
        cool->RadiusY = RelativeScalar(0.60, RelativeUnit::Relative);
        cool->GradientStops = {Media::GradientStop(Media::Color::FromArgb(80, 0x2b, 0x4e, 0x6b), 0),
            Media::GradientStop(Media::Color::FromArgb(0, 0x2b, 0x4e, 0x6b), 1)};
        const Rect box(dx - w * 0.25, dy - h * 0.25, w * 1.5, h * 1.5);
        context.FillRectangle(warm, box);
        context.FillRectangle(cool, box);
    }

    void DeckTile::Scrim(Media::DrawingContext& context, double w, double h)
    {
        auto brush = std::make_shared<Media::LinearGradientBrush>();
        brush->StartPoint = RelativePoint(0.5, 0, RelativeUnit::Relative);
        brush->EndPoint = RelativePoint(0.5, 1, RelativeUnit::Relative);
        brush->GradientStops = {Media::GradientStop(Media::Color::FromArgb(77, 10, 12, 16), 0),
            Media::GradientStop(Media::Color::FromArgb(51, 10, 12, 16), 0.40),
            Media::GradientStop(Media::Color::FromArgb(199, 10, 12, 16), 0.72),
            Media::GradientStop(Media::Color::FromArgb(245, 10, 12, 16), 1)};
        context.FillRectangle(brush, Rect(0, 0, w, h));
    }

    void DeckTile::Info(Media::DrawingContext& context, double w, double h, double em) const
    {
        const double pad = RoundToEven(em * 0.5);

        // .tag, with its code in the accent.
        const double tagSize = GuiTheme::PixelSize(em * 0.72);
        const auto code = DeckText::Run(::MphRead::NativeRuntime::ToUpperInvariant(Code), Deck::Body(true), tagSize,
            GuiTheme::AccentBrush);
        const double tagPadX = RoundToEven(tagSize * 0.45);
        const double tagPadY = RoundToEven(tagSize * 0.12);
        const Rect tag(pad, pad, RoundToEven(code->Width() + tagPadX * 2), RoundToEven(code->Height() + tagPadY * 2));
        context.DrawRectangle(Solid(Deck::Fade(0, 0.6)), std::make_shared<Media::Pen>(Solid(Deck::Fade(0xe6eaf2, 0.14)), 1),
            RoundedRect(tag, RoundToEven(tagSize * 0.25)));
        context.DrawText(*code, Point{tag.X + tagPadX, tag.Y + tagPadY});

        // .picked: a four-point lip the full width of the card's inside.
        const double wordSize = GuiTheme::PixelSize(em * 0.9);
        const double lip = 4;
        const double wordH = RoundToEven(wordSize * 1.7);
        double wordY = h - pad - wordH - lip;
        const std::string& word = _chosen ? ChosenVerb : Verb;
        if (word.empty())
        {
            wordY = h - pad + RoundToEven(em * 0.3);
        }
        else
        {
            const Rect slab(pad, wordY, MathMax(0.0, w - pad * 2), wordH);
            const Deck::Face faceColour = _chosen ? Deck::Face::Brass() : Deck::Face::Moss();
            Media::Color fill = faceColour.Fill;
            Media::Color lipColour = faceColour.Lip;
            if (_over)
            {
                fill = DeckPaint::Saturate(DeckPaint::Brightness(fill, 1.22), 1.15);
                lipColour = DeckPaint::Saturate(DeckPaint::Brightness(lipColour, 1.22), 1.15);
            }
            context.DrawRectangle(Solid(fill), nullptr, RoundedRect(slab, RoundToEven(wordSize * 0.55)),
                Media::BoxShadows(Deck::Shadow(0, lip, 0, 0, lipColour)));
            const double wordWidth = DeckText::MeasureTracked(word, Deck::Label(), wordSize, DeckText::LabelTracking);
            DeckText::DrawTracked(context, word, Deck::Label(), wordSize, GuiTheme::TextBrush,
                RoundToEven(slab.X + (slab.Width - wordWidth) / 2), slab.Y, slab.Height, DeckText::LabelTracking);
        }

        if (Blurb.empty())
        {
            return;
        }
        // .blurb: one line, above the word, trimmed to the card.
        const double blurbSize = GuiTheme::PixelSize(em * 0.76);
        const auto blurb = DeckText::Run(Blurb, Deck::Body(false), blurbSize, Solid(Media::Color::FromRgb(0xc7, 0xcf, 0xdd)),
            MathMax(10.0, w - pad * 2));
        context.DrawText(*blurb, Point{pad, RoundToEven(wordY - RoundToEven(em * 0.3) - blurb->Height())});
    }

    // ------------------------------------------------------------ DeckGrid

    DeckGrid::DeckGrid()
    {
        static const bool registered = []
        {
            AffectsMeasure<DeckGrid>(Deck::EmProperty);
            AffectsMeasure<DeckGrid>(Deck::FrameWidthProperty);
            return true;
        }();
        (void)registered;
        // In the tunnel, so the card the toolkit chose never sees it.
        AddHandler<Input::PointerPressedEventArgs>(PointerPressedEvent,
            [this](Interactivity::Interactive&, Input::PointerPressedEventArgs& e) { Pressed(e); },
            Interactivity::RoutingStrategies::Tunnel);
        AddHandler<Input::PointerEventArgs>(PointerMovedEvent,
            [this](Interactivity::Interactive&, Input::PointerEventArgs& e) { Moved(e); },
            Interactivity::RoutingStrategies::Tunnel);
        AddHandler<Input::PointerReleasedEventArgs>(PointerReleasedEvent,
            [this](Interactivity::Interactive&, Input::PointerReleasedEventArgs& e) { Released(e); },
            Interactivity::RoutingStrategies::Tunnel);
    }

    std::shared_ptr<DeckTile> DeckGrid::TileAt(Point at) const
    {
        for (const Controls::ControlPtr& child : Children)
        {
            if (auto tile = std::dynamic_pointer_cast<DeckTile>(child); tile != nullptr && tile->Bounds().Contains(at))
            {
                return tile;
            }
        }
        return nullptr;
    }

    void DeckGrid::Hover(std::shared_ptr<DeckTile> tile, Point at)
    {
        if (_over != tile)
        {
            if (_over != nullptr)
            {
                _over->Hover(false, Point{});
            }
            _over = tile;
        }
        if (tile != nullptr)
        {
            tile->Hover(true, at - static_cast<Vector>(tile->Bounds().Position()));
        }
    }

    void DeckGrid::Pressed(Input::PointerPressedEventArgs& e)
    {
        const Point at = e.GetPosition(this);
        _down = TileAt(at);
        Hover(_down, at);
        if (_down != nullptr)
        {
            _tap.Press(e, *this);
            _down->Focus();
        }
    }

    void DeckGrid::Moved(Input::PointerEventArgs& e)
    {
        const Point at = e.GetPosition(this);
        Hover(TileAt(at), at);
        if (_tap.Down())
        {
            _tap.Moved(e, *this);
        }
    }

    void DeckGrid::Released(Input::PointerReleasedEventArgs& e)
    {
        const std::shared_ptr<DeckTile> tile = _down;
        _down.reset();
        const bool tapped = _tap.Release(e, *this);
        if (tile == nullptr)
        {
            return;
        }
        // Ours either way: the card the toolkit would have sent this to is not
        // the one under the pointer.
        e.Handled = true;
        if (tapped && TileAt(e.GetPosition(this)) == tile)
        {
            tile->Fire();
        }
    }

    void DeckGrid::OnPointerExited(Input::PointerEventArgs& e)
    {
        Hover(nullptr, Point{});
        Panel::OnPointerExited(e);
    }

    void DeckGrid::OnPointerCaptureLost(Input::PointerCaptureLostEventArgs& e)
    {
        _tap.Cancel();
        _down.reset();
        Panel::OnPointerCaptureLost(e);
    }

    std::int32_t DeckGrid::Columns() const
    {
        return FixedColumns > 0 ? FixedColumns : Deck::GetFrameWidth(*this) <= TwoColumnFrame ? 2 : 3;
    }

    double DeckGrid::Gap() const
    {
        return RoundToEven(Deck::GetEm(*this) * 0.5);
    }

    Av::Size DeckGrid::MeasureOverride(Av::Size availableSize)
    {
        const std::int32_t columns = Columns();
        const double gap = Gap();
        const double width = std::isinf(availableSize.Width) ? Deck::GetEm(*this) * 40 : availableSize.Width;
        const double cell = MathMax(1.0, (width - gap * (columns - 1)) / columns);
        const double high = RoundToEven(cell / MathMax(0.1, Ratio));
        const Av::Size slot{cell, high};
        for (const Controls::ControlPtr& child : Children)
        {
            if (auto* tile = dynamic_cast<DeckTile*>(child.get()))
            {
                tile->Ratio = Ratio;
            }
            child->Measure(slot);
        }
        const auto count = static_cast<std::int32_t>(Children.Count());
        const std::int32_t rows = (count + columns - 1) / columns;
        return {width, rows * high + MathMax(0, rows - 1) * gap};
    }

    Av::Size DeckGrid::ArrangeOverride(Av::Size finalSize)
    {
        const std::int32_t columns = Columns();
        const double gap = Gap();
        const double cell = MathMax(1.0, (finalSize.Width - gap * (columns - 1)) / columns);
        const double high = RoundToEven(cell / MathMax(0.1, Ratio));
        for (std::size_t i = 0; i < Children.Count(); i++)
        {
            const auto row = static_cast<std::int32_t>(i) / columns;
            const auto column = static_cast<std::int32_t>(i) % columns;
            Children[i]->Arrange(Rect(column * (cell + gap), row * (high + gap), cell, high));
        }
        return finalSize;
    }
}
