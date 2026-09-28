#include "DeckButton.hpp"

#include "DeckChip.hpp"
#include "DeckText.hpp"

#include "../../../NativeRuntime/System/Encoding.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;

    DeckButton::DeckButton(std::string text, Deck::Face face, double sizeEms, double padXEms, double padYEms, double lip)
        : _text(std::move(text)), _face(face), _sizeEms(sizeEms), _padXEms(padXEms), _padYEms(padYEms), _lip(lip)
    {
        static const bool registered = []
        {
            AffectsRender<DeckButton>(IsEnabledProperty);
            AffectsMeasure<DeckButton>(Deck::EmProperty);
            AffectsRender<DeckButton>(Deck::EmProperty);
            return true;
        }();
        (void)registered;
        Focusable(true);
        Cursor(std::make_shared<Input::Cursor>(Input::StandardCursorType::Hand));
        // Pixel perfect: no edge feathering, so the face and the type land on
        // the same grid.
        Media::RenderOptions::SetEdgeMode(*this, Media::EdgeMode::Aliased);
    }

    void DeckButton::Text(std::string value)
    {
        if (_text == value)
        {
            return;
        }
        _text = std::move(value);
        InvalidateMeasure();
        InvalidateVisual();
    }

    void DeckButton::Wear(Deck::Face face, bool selected)
    {
        if (_face == face && _selected == selected)
        {
            return;
        }
        _face = face;
        _selected = selected;
        InvalidateVisual();
    }

    void DeckButton::Selected(bool value)
    {
        if (_selected == value)
        {
            return;
        }
        _selected = value;
        InvalidateVisual();
    }

    double DeckButton::Size() const
    {
        return Deck::GetEm(*this) * _sizeEms;
    }

    double DeckButton::KeyWidth(double size) const
    {
        if (KeyCap.empty())
        {
            return 0;
        }
        const double cap = size * 0.5;
        const double text = DeckText::Run(KeyCap, Deck::Body(true), cap, GuiTheme::AccentBrush)->Width();
        return ::MphRead::NativeRuntime::MathMax(cap * 1.5, text + cap * 0.7);
    }

    Size DeckButton::MeasureOverride(Av::Size availableSize)
    {
        const double size = Size();
        if (Glyph)
        {
            return {::MphRead::NativeRuntime::RoundToEven(size * (GlyphEms.Width + _padXEms * 2)),
                ::MphRead::NativeRuntime::RoundToEven(size * (GlyphEms.Height + _padYEms * 2))};
        }
        double width = DeckText::MeasureTracked(_text, Deck::Label(), size, DeckText::LabelTracking) + size * _padXEms * 2;
        if (!KeyCap.empty())
        {
            width += KeyGap() + KeyWidth(size);
        }
        const double line = DeckText::Run("Hg", Deck::Label(), size, GuiTheme::TextBrush)->Height();
        // The face alone: the edge is a box-shadow and hangs outside the box.
        return {::MphRead::NativeRuntime::MathMin(::MphRead::NativeRuntime::RoundToEven(width), availableSize.Width),
            ::MphRead::NativeRuntime::RoundToEven(line + size * _padYEms * 2)};
    }

    void DeckButton::OnPointerEntered(Input::PointerEventArgs& e)
    {
        _popTarget = 1.055;
        StartHop();
        InvalidateVisual();
        Control::OnPointerEntered(e);
    }

    void DeckButton::OnPointerExited(Input::PointerEventArgs& e)
    {
        _popTarget = 1;
        _tiltTarget = 0;
        _hop.Reset();
        InvalidateVisual();
        Control::OnPointerExited(e);
    }

    void DeckButton::StartHop()
    {
        if (Deck::Still())
        {
            return;
        }
        _hop.Restart();
    }

    void DeckButton::OnPointerMoved(Input::PointerEventArgs& e)
    {
        if (_tap.Down() && _tap.Moved(e, *this))
        {
            InvalidateVisual();
        }
        // No pointer-following tilt: it is not drawn, and it redrew every move.
        Control::OnPointerMoved(e);
    }

    void DeckButton::OnPointerPressed(Input::PointerPressedEventArgs& e)
    {
        _tap.Press(e, *this);
        Focus();
        e.Pointer->Capture(this);
        e.Handled = true;
        InvalidateVisual();
        Control::OnPointerPressed(e);
    }

    void DeckButton::OnPointerReleased(Input::PointerReleasedEventArgs& e)
    {
        const bool tapped = _tap.Release(e, *this);
        InvalidateVisual();
        if (tapped)
        {
            e.Handled = true;
            Click(*this);
        }
        Control::OnPointerReleased(e);
    }

    void DeckButton::OnPointerCaptureLost(Input::PointerCaptureLostEventArgs& e)
    {
        _tap.Cancel();
        _popTarget = 1;
        _tiltTarget = 0;
        InvalidateVisual();
        Control::OnPointerCaptureLost(e);
    }

    void DeckButton::OnKeyDown(Input::KeyEventArgs& e)
    {
        if (e.Key == Input::Key::Enter || e.Key == Input::Key::Space)
        {
            e.Handled = true;
            Click(*this);
            return;
        }
        Control::OnKeyDown(e);
    }

    void DeckButton::OnGotFocus(Input::GotFocusEventArgs& e)
    {
        // The ring follows whichever device the player is driving with.
        _ringVisible = e.NavigationMethod != Input::NavigationMethod::Pointer && Deck::KeyboardDriving();
        _popTarget = 1.055;
        StartHop();
        InvalidateVisual();
        Control::OnGotFocus(e);
    }

    void DeckButton::OnLostFocus(Input::FocusChangedEventArgs& e)
    {
        _ringVisible = false;
        if (!IsPointerOver())
        {
            _popTarget = 1;
            _hop.Reset();
        }
        InvalidateVisual();
        Control::OnLostFocus(e);
    }

    bool DeckButton::Settle()
    {
        const ::MphRead::NativeRuntime::TimeSpan now = _clock.Elapsed();
        const double dt = ::MphRead::NativeRuntime::MathMin(0.05, (now - _last).TotalSeconds());
        _last = now;
        if (Deck::Still())
        {
            _pop = _popTarget;
            _popVelocity = 0;
            _tilt = _tiltTarget;
            return false;
        }
        if (dt <= 0)
        {
            return false;
        }
        const double accel = (_popTarget - _pop) * Stiffness - _popVelocity * Damping;
        _popVelocity += accel * dt;
        _pop += _popVelocity * dt;
        _tilt += (_tiltTarget - _tilt) * ::MphRead::NativeRuntime::MathMin(1.0, dt * 14);
        const bool moving = std::abs(_popTarget - _pop) > 0.0005 || std::abs(_popVelocity) > 0.0005
            || std::abs(_tiltTarget - _tilt) > 0.05;
        if (!moving)
        {
            _pop = _popTarget;
            _popVelocity = 0;
            _tilt = _tiltTarget;
        }
        return moving;
    }

    std::pair<double, double> DeckButton::Hop(std::int32_t index, double size) const
    {
        if (!_hop.IsRunning())
        {
            return {0, 0};
        }
        const double t = _hop.Elapsed().TotalSeconds() - index * HopStagger;
        if (t <= 0 || t >= HopSeconds)
        {
            return {0, 0};
        }
        const double phase = t / HopSeconds;
        const double amount = phase < HopPeak ? Deck::Spring(phase / HopPeak)
                                              : 1 - Deck::Spring((phase - HopPeak) / (1 - HopPeak));
        const double tilt = (index % 2 == 0 ? -HopTilt : HopTilt) * amount;
        return {-size * HopLift * amount, tilt};
    }

    bool DeckButton::Hopping()
    {
        if (!_hop.IsRunning())
        {
            return false;
        }
        std::int32_t characters = 0;
        for (const char16_t c : ::MphRead::NativeRuntime::Utf8ToUtf16(_text))
        {
            if (c != u' ')
            {
                characters++;
            }
        }
        const double total = HopSeconds + ::MphRead::NativeRuntime::MathMax(0, characters - 1) * HopStagger;
        if (_hop.Elapsed().TotalSeconds() >= total)
        {
            _hop.Reset();
            return false;
        }
        return true;
    }

    double DeckButton::Bob() const
    {
        if (!Idle || Deck::Still() || IsPointerOver() || (IsFocused() && _ringVisible))
        {
            return 0;
        }
        // A raised cosine over 3.4 seconds, two and a half points deep.
        const double phase = std::fmod(_clock.Elapsed().TotalSeconds(), 3.4) / 3.4;
        return -1.25 * (1 - std::cos(phase * std::numbers::pi * 2));
    }

    void DeckButton::RequestAnotherFrame(bool idling)
    {
        if (_framePending)
        {
            return;
        }
        _framePending = true;
        const std::shared_ptr<DeckButton> self = std::static_pointer_cast<DeckButton>(shared_from_this());
        Deck::NextFrame(*this, [self]
        {
            self->_framePending = false;
            self->InvalidateVisual();
        }, idling);
    }

    void DeckButton::Render(Media::DrawingContext& context)
    {
        const bool moving = Settle();
        const bool down = _tap.Down();
        const bool on = IsEnabled();

        const double lip = down ? LipPressed : _lip;
        // The face drops by exactly what the lip loses.
        const double drop = _lip - lip + Bob();

        const double w = Bounds().Width;
        const double h = Bounds().Height;
        if (w <= 0 || h <= 0)
        {
            return;
        }

        const double size = Size();
        const double radius = size * RadiusEms;
        const double scale = down ? 1 : _pop;
        Media::Color fill = _face.Fill;
        Media::Color lipColour = _face.Lip;
        if (!on)
        {
            // filter: grayscale(.7) brightness(.55).
            fill = DeckPaint::Brightness(DeckPaint::Saturate(fill, 0.3), 0.55);
            lipColour = DeckPaint::Brightness(DeckPaint::Saturate(lipColour, 0.3), 0.55);
        }
        else if (IsPointerOver() || (IsFocused() && _ringVisible))
        {
            // filter: brightness(1.22) saturate(1.15).
            fill = DeckPaint::Saturate(DeckPaint::Brightness(fill, 1.22), 1.15);
            lipColour = DeckPaint::Saturate(DeckPaint::Brightness(lipColour, 1.22), 1.15);
        }

        {
            // Scale and translate only: a rotation sends the blurred shadow and
            // the round-rect clip down Ganesh's CPU mask path.
            auto transform = context.PushTransform(Matrix::CreateTranslation(-w / 2, -h / 2)
                * Matrix::CreateScale(scale, scale)
                * Matrix::CreateTranslation(w / 2, h / 2 + drop));
            const RoundedRect faceRect(Rect(0, 0, w, h), radius);

            // The wedge over the tab that is up, drawn under the face.
            if (_selected)
            {
                const double half = size * 0.4;
                const double top = -size * 0.72;
                Media::StreamGeometry wedge;
                {
                    Media::StreamGeometryContext g = wedge.Open();
                    const double mid = ::MphRead::NativeRuntime::RoundToEven(w / 2);
                    g.BeginFigure(Point{mid - half, top}, true);
                    g.LineTo(Point{mid + half, top});
                    g.LineTo(Point{mid, top + half});
                    g.EndFigure(true);
                }
                context.DrawGeometry(std::make_shared<Media::SolidColorBrush>(fill), nullptr, wedge);
            }

            // The face and its shadows, as the reference declares them, with
            // a two-point accent ring in front while it has the keyboard.
            const std::vector<Media::BoxShadow> cast{Deck::Shadow(0, lip, 0, 0, lipColour),
                Deck::Shadow(0, lip + 4, 12, 0, Deck::Fade(0, 0.55))};
            const bool ring = IsFocused() && _ringVisible;
            const Media::BoxShadow first = ring ? Deck::Shadow(0, 0, 0, 2, GuiTheme::Accent) : cast[0];
            if (ring)
            {
                context.DrawRectangle(std::make_shared<Media::SolidColorBrush>(fill), nullptr, faceRect,
                    Media::BoxShadows(first, cast));
            }
            else
            {
                context.DrawRectangle(std::make_shared<Media::SolidColorBrush>(fill), nullptr, faceRect,
                    Media::BoxShadows(first, {cast[1]}));
            }

            // One hairline of light along the top.
            auto bevel = std::make_shared<Media::LinearGradientBrush>();
            bevel->StartPoint = RelativePoint(0, 0, RelativeUnit::Relative);
            bevel->EndPoint = RelativePoint(0, 1, RelativeUnit::Relative);
            bevel->GradientStops = {Media::GradientStop(Media::Color::FromArgb(0x17, 255, 255, 255), 0),
                Media::GradientStop(Media::Color::FromArgb(0, 255, 255, 255), 1)};
            context.DrawRectangle(bevel, nullptr, RoundedRect(Rect(1, 1, w - 2, h * 0.4), radius - 1));

            const Media::IBrushPtr ink = on ? GuiTheme::TextBrush : GuiTheme::TextDimBrush;
            if (Glyph)
            {
                Media::IBrushPtr paint = ink;
                if (GlyphColour.has_value())
                {
                    Media::Color tint = *GlyphColour;
                    if (!on)
                    {
                        tint = DeckPaint::Brightness(DeckPaint::Saturate(tint, 0.3), 0.55);
                    }
                    else if (IsPointerOver() || (IsFocused() && _ringVisible))
                    {
                        tint = DeckPaint::Saturate(DeckPaint::Brightness(tint, 1.22), 1.15);
                    }
                    paint = std::make_shared<Media::SolidColorBrush>(tint);
                }
                const double gw = size * GlyphEms.Width;
                const double gh = size * GlyphEms.Height;
                Glyph(context, Rect(::MphRead::NativeRuntime::RoundToEven((w - gw) / 2),
                    ::MphRead::NativeRuntime::RoundToEven((h - gh) / 2), gw, gh), paint);
                DrawTip(context, w, size, IsPointerOver() || (IsFocused() && _ringVisible) || Deck::Phone());
                const bool glyphBusy = moving || _hop.IsRunning() || _tipMoving;
                if ((glyphBusy || Idle) && !Deck::Still())
                {
                    RequestAnotherFrame(!glyphBusy);
                }
                return;
            }
            const double keyWidth = KeyWidth(size);
            const double labelWidth = DeckText::MeasureTracked(_text, Deck::Label(), size, DeckText::LabelTracking);
            const double content = labelWidth + (keyWidth > 0 ? KeyGap() + keyWidth : 0);
            const double x = ::MphRead::NativeRuntime::RoundToEven((w - content) / 2);
            const bool hopping = Hopping();
            DeckText::DrawTracked(context, _text, Deck::Label(), size, ink, x, 0, h, DeckText::LabelTracking,
                hopping ? std::function<std::pair<double, double>(std::int32_t)>(
                              [this, size](std::int32_t i) { return Hop(i, size); })
                        : nullptr);
            if (keyWidth > 0 && on)
            {
                DeckChip::DrawKey(context, KeyCap, size,
                    ::MphRead::NativeRuntime::RoundToEven(x + labelWidth + KeyGap()), h);
            }
        }

        // Not while the screen is being photographed.
        const bool busy = moving || _hop.IsRunning() || _tipMoving;
        if ((busy || Idle) && !Deck::Still())
        {
            RequestAnotherFrame(!busy);
        }
    }

    void DeckButton::DrawTip(Media::DrawingContext& context, double w, double size, bool hot)
    {
        if (Tip.empty())
        {
            _tipMoving = false;
            return;
        }
        const double target = hot ? 1 : 0;
        if (Deck::Still())
        {
            _tipShow = target;
        }
        else
        {
            _tipShow += (target - _tipShow) * 0.25;
            if (std::abs(target - _tipShow) < 0.004)
            {
                _tipShow = target;
            }
        }
        _tipMoving = !Deck::Still() && std::abs(target - _tipShow) > 0.0001;
        DeckHeart::DrawTip(context, Tip, w, size, _tipShow, _tipShow);
    }

    Media::Color DeckPaint::Brightness(Media::Color c, double k)
    {
        return Media::Color::FromArgb(c.A, Clamp(c.R * k), Clamp(c.G * k), Clamp(c.B * k));
    }

    Media::Color DeckPaint::Saturate(Media::Color c, double s)
    {
        const double luma = c.R * 0.2126 + c.G * 0.7152 + c.B * 0.0722;
        return Media::Color::FromArgb(c.A, Clamp(luma + (c.R - luma) * s), Clamp(luma + (c.G - luma) * s),
            Clamp(luma + (c.B - luma) * s));
    }

    std::uint8_t DeckPaint::Clamp(double v)
    {
        return static_cast<std::uint8_t>(::MphRead::NativeRuntime::MathClamp(v, 0.0, 255.0));
    }
}
