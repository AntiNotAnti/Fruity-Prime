#include "SliderRow.hpp"

#include "TrackedText.hpp"
#include "../../../NativeRuntime/System/Globalization.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"

#include <algorithm>
#include <cmath>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;
    using ::MphRead::NativeRuntime::RoundToEven;

    SliderRow::SliderRow(std::string label, std::int32_t value, std::function<std::string(std::int32_t)> format,
        double labelWidth, std::int32_t min, std::int32_t max, std::int32_t keyStep)
        : _label(std::move(label)), _labelWidth(labelWidth), _min(min), _max(std::max(min + 1, max)),
          _keyStep(std::max(1, keyStep))
    {
        _value = std::clamp(value, _min, _max);
        _format = format ? std::move(format) : [](std::int32_t v) { return std::to_string(v) + "%"; };
        Height(34);
        Focusable(true);
        Cursor(std::make_shared<Input::Cursor>(Input::StandardCursorType::Hand));
    }

    void SliderRow::Value(std::int32_t value)
    {
        const std::int32_t clamped = std::clamp(value, _min, _max);
        if (clamped != _value)
        {
            _value = clamped;
            InvalidateVisual();
            ValueChanged(*this);
        }
    }

    Rect SliderRow::Track() const
    {
        return Rect(_labelWidth, Bounds().Height / 2 - 2, std::max(40.0, Bounds().Width - _labelWidth - ValueGutter), 4);
    }

    void SliderRow::SetFromPointer(double x)
    {
        const Rect track = Track();
        const double fraction = (x - track.X) / std::max(1.0, track.Width);
        Value(_min + static_cast<std::int32_t>(RoundToEven(std::clamp(fraction, 0.0, 1.0) * (_max - _min))));
    }

    void SliderRow::BeginDrag(Input::PointerEventArgs& e, Point p)
    {
        _dragging = true;
        // Captured so a drag that leaves the row keeps moving the value.
        e.Pointer->Capture(this);
        SetFromPointer(p.X);
    }

    void SliderRow::OnPointerPressed(Input::PointerPressedEventArgs& e)
    {
        Focus();
        const Point p = e.GetPosition(this);
        if (p.X < _labelWidth || !IsEnabled())
        {
            Control::OnPointerPressed(e);
            return;
        }
        if (Tap::Drags(e))
        {
            // A finger is not told from a scroll yet: wait for the direction.
            _tap.Press(e, *this);
        }
        else
        {
            BeginDrag(e, p);
        }
        Control::OnPointerPressed(e);
    }

    void SliderRow::OnPointerMoved(Input::PointerEventArgs& e)
    {
        const Point p = e.GetPosition(this);
        const bool hot = p.X >= _labelWidth;
        if (hot != _hot)
        {
            _hot = hot;
            InvalidateVisual();
        }
        if (_dragging)
        {
            SetFromPointer(p.X);
        }
        else if (_tap.Down())
        {
            // Sideways is the slider and anything else is the page.
            if (Tap::Sideways(_tap.Travel(p)))
            {
                _tap.Cancel();
                BeginDrag(e, p);
            }
            else
            {
                _tap.Moved(e, *this);
            }
        }
        Control::OnPointerMoved(e);
    }

    void SliderRow::OnPointerReleased(Input::PointerReleasedEventArgs& e)
    {
        // A tap on the track still sets the value.
        if (_tap.Release(e, *this) && IsEnabled())
        {
            const Point p = e.GetPosition(this);
            if (p.X >= _labelWidth)
            {
                SetFromPointer(p.X);
            }
        }
        _dragging = false;
        e.Pointer->Capture(nullptr);
        Control::OnPointerReleased(e);
    }

    void SliderRow::OnPointerExited(Input::PointerEventArgs& e)
    {
        _hot = false;
        InvalidateVisual();
        Control::OnPointerExited(e);
    }

    void SliderRow::OnPointerCaptureLost(Input::PointerCaptureLostEventArgs& e)
    {
        _tap.Cancel();
        _dragging = false;
        Control::OnPointerCaptureLost(e);
    }

    void SliderRow::OnKeyDown(Input::KeyEventArgs& e)
    {
        if (!IsEnabled())
        {
            Control::OnKeyDown(e);
            return;
        }
        if (e.Key == Input::Key::Left)
        {
            Value(Value() - _keyStep);
            e.Handled = true;
            return;
        }
        if (e.Key == Input::Key::Right)
        {
            Value(Value() + _keyStep);
            e.Handled = true;
            return;
        }
        Control::OnKeyDown(e);
    }

    void SliderRow::OnGotFocus(Input::GotFocusEventArgs& e)
    {
        InvalidateVisual();
        Control::OnGotFocus(e);
    }

    void SliderRow::OnLostFocus(Input::FocusChangedEventArgs& e)
    {
        InvalidateVisual();
        Control::OnLostFocus(e);
    }

    void SliderRow::Render(Media::DrawingContext& context)
    {
        context.FillRectangle(Media::Brushes::Transparent(), Rect(0, 0, Bounds().Width, Bounds().Height));
        const Media::IBrushPtr dim = std::make_shared<Media::SolidColorBrush>(Media::Color::FromRgb(70, 76, 90));
        TrackedText::Draw(context, ::MphRead::NativeRuntime::ToUpperInvariant(_label), 11,
            IsEnabled() ? GuiTheme::TextDimBrush : dim, 4, (Bounds().Height - TrackedText::LineHeight(11)) / 2, 1);

        const Rect track = Track();
        context.FillRectangle(GuiTheme::PanelLightBrush, track);
        const double filled = track.Width * ((_value - _min) / static_cast<double>(_max - _min));
        const Media::IBrushPtr accent = IsEnabled()
            ? std::make_shared<Media::SolidColorBrush>(IsFocused() || _hot ? GuiTheme::Shade(GuiTheme::Accent, 0.15)
                                                                           : GuiTheme::Accent)
            : dim;
        context.FillRectangle(accent, Rect(track.X, track.Y, filled, track.Height));
        context.DrawEllipse(accent, nullptr, Point{track.X + filled, track.Y + track.Height / 2}, 5, 5);

        const Media::FormattedText value = TrackedText::Make(_format(_value), 12, true,
            IsEnabled() ? GuiTheme::TextBrush : dim);
        context.DrawText(value, Point{Bounds().Width - 4 - value.Width(), (Bounds().Height - value.Height()) / 2});
    }
}
