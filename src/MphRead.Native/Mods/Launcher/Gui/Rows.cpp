#include "Rows.hpp"

#include "DeckButton.hpp"
#include "../../../NativeRuntime/System/Globalization.hpp"

#include <algorithm>
#include <cmath>
#include <memory>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;

    namespace
    {
        [[nodiscard]] Media::FormattedText Text(const std::string& text, bool bold, double size, const Media::IBrushPtr& brush)
        {
            return Media::FormattedText(text, Media::InvariantCulture, Media::FlowDirection::LeftToRight,
                GuiTheme::Face(bold), size, brush);
        }
    }

    // ------------------------------------------------------------ Caption

    Caption::Caption(std::string text)
        : _text(std::move(text))
    {
        Height(26);
    }

    Av::Size Caption::MeasureOverride(Av::Size availableSize)
    {
        // Its own width when nothing constrains it.
        const Av::Size size = Control::MeasureOverride(availableSize);
        return {std::min(Label().Width() + 8, availableSize.Width), size.Height};
    }

    const Media::FormattedText& Caption::Label() const
    {
        if (!_labelLayout)
        {
            _labelLayout.emplace(Text(::MphRead::NativeRuntime::ToUpperInvariant(_text),
                true, 11, GuiTheme::TextDimBrush));
        }
        return *_labelLayout;
    }

    void Caption::Render(Media::DrawingContext& context)
    {
        const Media::FormattedText& text = Label();
        context.DrawText(text, Point{0, Bounds().Height - text.Height() - 4});
        const double y = Bounds().Height - 2;
        context.DrawLine(std::make_shared<Media::Pen>(GuiTheme::EdgeBrush, 1),
            Point{0, y}, Point{Bounds().Width, y});
    }

    // ---------------------------------------------------------- ChoiceRow

    ChoiceRow::ChoiceRow(std::string label, std::vector<std::string> options, std::int32_t index)
        : _label(std::move(label)), _options(std::move(options))
    {
        _index = _options.empty() ? 0 : std::clamp(index, 0, static_cast<std::int32_t>(_options.size()) - 1);
        Height(34);
        Focusable(true);
        Cursor(std::make_shared<Input::Cursor>(Input::StandardCursorType::Hand));
    }

    void ChoiceRow::Index(std::int32_t value)
    {
        const std::int32_t clamped = _options.empty() ? 0 : std::clamp(value, 0, static_cast<std::int32_t>(_options.size()) - 1);
        if (clamped != _index)
        {
            _index = clamped;
            ClearValueLayout();
            InvalidateVisual();
            Changed(*this);
        }
    }

    void ChoiceRow::SetItems(std::vector<std::string> options, std::int32_t index)
    {
        _options = std::move(options);
        _index = _options.empty() ? 0 : std::clamp(index, 0, static_cast<std::int32_t>(_options.size()) - 1);
        ClearValueLayout();
        InvalidateVisual();
    }

    void ChoiceRow::Preview(std::function<void(Media::DrawingContext&, Rect)> value)
    {
        _preview = std::move(value);
        // Room for the picture: a crosshair at its largest is 36 points.
        Height(_preview ? 48 : 34);
        InvalidateVisual();
    }

    Rect ChoiceRow::LeftArrow() const
    {
        // Both arrows sit still: the value gets a fixed column and is trimmed.
        const double floor = std::min(110.0, Bounds().Width * 0.42);
        const double x = Bounds().Width - PreviewRoom() - ArrowWidth - ValueColumn - ArrowWidth;
        return Rect(std::max(floor, x), 0, ArrowWidth, Bounds().Height);
    }

    Rect ChoiceRow::RightArrow() const
    {
        return Rect(Bounds().Width - PreviewRoom() - ArrowWidth, 0, ArrowWidth, Bounds().Height);
    }

    const Media::FormattedText& ChoiceRow::LabelLayout() const
    {
        if (!_labelLayout)
        {
            _labelLayout.emplace(Text(_label, false, 13, GuiTheme::TextDimBrush));
        }
        return *_labelLayout;
    }

    const Media::FormattedText& ChoiceRow::ValueLayout(double room) const
    {
        if (!_valueLayout || !_valueLayoutRoom || *_valueLayoutRoom != room)
        {
            _valueLayout.emplace(Text(Value(), true, 13, GuiTheme::TextBrush));
            if (_valueLayout->Width() > room)
            {
                _valueLayout->MaxTextWidth(std::max(20.0, room));
                // One line, whatever the trimming decides.
                _valueLayout->MaxTextHeight(13 * 1.9);
                _valueLayout->Trimming(Media::TextTrimming::CharacterEllipsis);
            }
            _valueLayoutRoom = room;
        }
        return *_valueLayout;
    }

    void ChoiceRow::ClearValueLayout() noexcept
    {
        _valueLayout.reset();
        _valueLayoutRoom.reset();
    }

    void ChoiceRow::OnPointerMoved(Input::PointerEventArgs& e)
    {
        const Point p = e.GetPosition(this);
        const bool left = LeftArrow().Contains(p);
        const bool right = RightArrow().Contains(p);
        if (left != _leftHot || right != _rightHot)
        {
            _leftHot = left;
            _rightHot = right;
            InvalidateVisual();
        }
        // A finger on its way down the page is not answering this row.
        _tap.Moved(e, *this);
        Control::OnPointerMoved(e);
    }

    void ChoiceRow::OnPointerExited(Input::PointerEventArgs& e)
    {
        _leftHot = false;
        _rightHot = false;
        _tap.Cancel();
        InvalidateVisual();
        Control::OnPointerExited(e);
    }

    void ChoiceRow::OnPointerPressed(Input::PointerPressedEventArgs& e)
    {
        Focus();
        // The press decides nothing: see Tap.
        _tap.Press(e, *this);
        Control::OnPointerPressed(e);
    }

    void ChoiceRow::OnPointerReleased(Input::PointerReleasedEventArgs& e)
    {
        if (_tap.Release(e, *this))
        {
            // Anywhere that is not the back arrow steps forward.
            Step(LeftArrow().Contains(e.GetPosition(this)) ? -1 : 1);
        }
        Control::OnPointerReleased(e);
    }

    void ChoiceRow::OnPointerCaptureLost(Input::PointerCaptureLostEventArgs& e)
    {
        _tap.Cancel();
        Control::OnPointerCaptureLost(e);
    }

    void ChoiceRow::OnKeyDown(Input::KeyEventArgs& e)
    {
        if (e.Key == Input::Key::Left)
        {
            Step(-1);
            e.Handled = true;
            return;
        }
        if (e.Key == Input::Key::Right || e.Key == Input::Key::Enter || e.Key == Input::Key::Space)
        {
            Step(1);
            e.Handled = true;
            return;
        }
        Control::OnKeyDown(e);
    }

    void ChoiceRow::Step(std::int32_t direction)
    {
        if (_options.empty())
        {
            return;
        }
        // Wrapping, because the lists are short.
        const auto count = static_cast<std::int32_t>(_options.size());
        _index = (_index + direction + count) % count;
        ClearValueLayout();
        InvalidateVisual();
        Changed(*this);
    }

    void ChoiceRow::Render(Media::DrawingContext& context)
    {
        // Hit testing follows the drawing.
        context.FillRectangle(Media::Brushes::Transparent(), Rect(0, 0, Bounds().Width, Bounds().Height));
        if (IsFocused())
        {
            context.FillRectangle(GuiTheme::PanelLightBrush, Rect(0, 0, Bounds().Width, Bounds().Height), 4);
        }
        const Media::FormattedText& label = LabelLayout();
        context.DrawText(label, Point{4, (Bounds().Height - label.Height()) / 2});

        // The value lives in the fixed column between the arrows.
        const Rect left = LeftArrow();
        const double room = RightArrow().X - left.Right() - 8;
        const Media::FormattedText& value = ValueLayout(room);
        const double centre = (left.Right() + RightArrow().X) / 2;
        context.DrawText(value, Point{centre - value.Width() / 2, (Bounds().Height - value.Height()) / 2});

        Arrow(context, left, true, _leftHot);
        Arrow(context, RightArrow(), false, _rightHot);
        if (_preview)
        {
            constexpr double inset = 3;
            _preview(context, Rect(Bounds().Width - PreviewWidth + inset, inset, PreviewWidth - inset * 2,
                Bounds().Height - inset * 2));
        }
    }

    void ChoiceRow::Arrow(Media::DrawingContext& context, Rect area, bool pointsLeft, bool hot)
    {
        const double cx = area.X + area.Width / 2;
        const double cy = area.Y + area.Height / 2;
        constexpr double w = 4.5;
        constexpr double h = 6;
        Media::StreamGeometry geometry;
        {
            Media::StreamGeometryContext sink = geometry.Open();
            if (pointsLeft)
            {
                sink.BeginFigure(Point{cx + w, cy - h}, true);
                sink.LineTo(Point{cx - w, cy});
                sink.LineTo(Point{cx + w, cy + h});
            }
            else
            {
                sink.BeginFigure(Point{cx - w, cy - h}, true);
                sink.LineTo(Point{cx + w, cy});
                sink.LineTo(Point{cx - w, cy + h});
            }
            sink.EndFigure(true);
        }
        context.DrawGeometry(hot ? GuiTheme::AccentBrush : GuiTheme::TextDimBrush, nullptr, geometry);
    }

    // ---------------------------------------------------------- ToggleRow

    ToggleRow::ToggleRow(std::string label, bool on)
        : _label(std::move(label)), _on(on)
    {
        Height(34);
        Focusable(true);
        Cursor(std::make_shared<Input::Cursor>(Input::StandardCursorType::Hand));
    }

    void ToggleRow::On(bool value)
    {
        if (_on != value)
        {
            _on = value;
            InvalidateVisual();
            Changed(*this);
        }
    }

    void ToggleRow::OnPointerPressed(Input::PointerPressedEventArgs& e)
    {
        Focus();
        // Not On = !On: a toggle flipped by the press is flipped by every
        // scroll that starts on it.
        _tap.Press(e, *this);
        Control::OnPointerPressed(e);
    }

    void ToggleRow::OnPointerMoved(Input::PointerEventArgs& e)
    {
        _tap.Moved(e, *this);
        Control::OnPointerMoved(e);
    }

    void ToggleRow::OnPointerReleased(Input::PointerReleasedEventArgs& e)
    {
        if (_tap.Release(e, *this))
        {
            On(!On());
        }
        Control::OnPointerReleased(e);
    }

    void ToggleRow::OnPointerExited(Input::PointerEventArgs& e)
    {
        _tap.Cancel();
        Control::OnPointerExited(e);
    }

    void ToggleRow::OnPointerCaptureLost(Input::PointerCaptureLostEventArgs& e)
    {
        _tap.Cancel();
        Control::OnPointerCaptureLost(e);
    }

    void ToggleRow::OnKeyDown(Input::KeyEventArgs& e)
    {
        if (e.Key == Input::Key::Enter || e.Key == Input::Key::Space || e.Key == Input::Key::Left
            || e.Key == Input::Key::Right)
        {
            On(!On());
            e.Handled = true;
            return;
        }
        Control::OnKeyDown(e);
    }

    void ToggleRow::Render(Media::DrawingContext& context)
    {
        context.FillRectangle(Media::Brushes::Transparent(), Rect(0, 0, Bounds().Width, Bounds().Height));
        if (IsFocused())
        {
            context.FillRectangle(GuiTheme::PanelLightBrush, Rect(0, 0, Bounds().Width, Bounds().Height), 4);
        }
        if (!_labelLayout)
        {
            _labelLayout.emplace(Text(_label, false, 13, GuiTheme::TextDimBrush));
        }
        const Media::FormattedText& label = *_labelLayout;
        context.DrawText(label, Point{4, (Bounds().Height - label.Height()) / 2});

        constexpr double w = 40;
        constexpr double h = 20;
        const Rect track(Bounds().Width - w - 4, (Bounds().Height - h) / 2, w, h);
        context.DrawRectangle(std::make_shared<Media::SolidColorBrush>(_on ? GuiTheme::Accent : GuiTheme::Edge), nullptr,
            RoundedRect(track, h / 2));
        const double knob = _on ? track.Right() - h / 2 : track.X + h / 2;
        context.DrawEllipse(std::make_shared<Media::SolidColorBrush>(_on ? GuiTheme::Ink : GuiTheme::TextDim), nullptr,
            Point{knob, track.Y + h / 2}, h / 2 - 3, h / 2 - 3);
    }

    // ---------------------------------------------------- ButtonToggleRow

    ButtonToggleRow::ButtonToggleRow(const std::string& label, bool on)
        : _on(on)
    {
        ColumnDefinitions(Controls::ColumnDefinitions("*,Auto,Auto"));
        ColumnSpacing(5);
        MinHeight(32);

        auto caption = std::make_shared<Controls::TextBlock>();
        caption->Text(label);
        caption->FontFamily(GuiTheme::Display());
        caption->FontSize(12);
        caption->Foreground(GuiTheme::TextDimBrush);
        caption->VerticalAlignment(Layout::VerticalAlignment::Center);
        caption->Margin(Thickness(4, 0, 8, 0));
        Children.Add(caption);

        _off = std::make_shared<DeckButton>("OFF", Deck::Face::Slate(), 0.72, 0.62, 0.26, 3);
        _onButton = std::make_shared<DeckButton>("ON", Deck::Face::Slate(), 0.72, 0.72, 0.26, 3);
        _off->Click += [this](DeckButton&) { On(false); };
        _onButton->Click += [this](DeckButton&) { On(true); };

        Controls::Grid::SetColumn(*_off, 1);
        Controls::Grid::SetColumn(*_onButton, 2);
        Children.Add(_off);
        Children.Add(_onButton);
        Mark();
    }

    void ButtonToggleRow::On(bool value)
    {
        if (_on == value)
        {
            return;
        }
        _on = value;
        Mark();
        Changed(*this);
    }

    void ButtonToggleRow::Mark()
    {
        _off->Wear(_on ? Deck::Face::Slate() : Deck::Face::Rust(), !_on);
        _onButton->Wear(_on ? Deck::Face::Moss() : Deck::Face::Slate(), _on);
    }

    // ----------------------------------------------------------- FieldRow

    FieldRow::FieldRow(const std::string& label, const std::string& value, double boxWidth, bool compact)
        : Box(std::make_shared<Controls::TextBox>()), _caption(std::make_shared<Controls::TextBlock>())
    {
        Height(compact ? 21 : 36);
        _caption->Text(label);
        _caption->FontFamily(GuiTheme::Display());
        _caption->FontSize(13);
        _caption->Foreground(GuiTheme::TextDimBrush);
        _caption->VerticalAlignment(Layout::VerticalAlignment::Center);
        _caption->HorizontalAlignment(Layout::HorizontalAlignment::Left);
        _caption->Margin(Thickness(4, 0, 0, 0));
        // Colours are left to the Fluent dark theme.
        Box->Text(value);
        Box->Width(boxWidth);
        Box->Height(compact ? 21 : std::numeric_limits<double>::quiet_NaN());
        Box->MinHeight(compact ? 21 : 0);
        Box->FontFamily(GuiTheme::Display());
        Box->FontSize(compact ? 11 : 13);
        Box->CornerRadius(CornerRadius(4));
        Box->Padding(compact ? Thickness(6, 0, 6, 0) : Thickness(8, 4, 8, 4));
        Box->VerticalContentAlignment(Layout::VerticalAlignment::Center);
        Box->VerticalAlignment(Layout::VerticalAlignment::Center);
        Box->HorizontalAlignment(compact ? Layout::HorizontalAlignment::Left : Layout::HorizontalAlignment::Right);
        Children.Add(_caption);
        Children.Add(Box);
    }

    // --------------------------------------------------------------- Note

    Note::Note(const std::string& text, std::optional<Media::Color> color, std::int32_t lines)
        : _lines(lines)
    {
        Text(text);
        // The body face: this is the one string on the screen that is a sentence.
        FontFamily(Deck::Mono);
        Foreground(std::make_shared<Media::SolidColorBrush>(color.value_or(GuiTheme::TextDim)));
        TextWrapping(Media::TextWrapping::Wrap);
        if (lines > 0)
        {
            MaxLines(lines);
            TextTrimming(Media::TextTrimming::CharacterEllipsis);
        }
        Margin(Thickness(0));
    }

    Av::Size Note::MeasureOverride(Av::Size availableSize)
    {
        // .76em, and a fixed 2.3em of height whatever it says.
        const double em = Deck::GetEm(*this);
        const double size = em * 0.76;
        if (std::abs(size - FontSize()) > 0.01)
        {
            FontSize(size);
            LineHeight(std::round(size * 1.15));
        }
        const Av::Size measured = TextBlock::MeasureOverride(availableSize);
        if (_lines <= 0)
        {
            return measured;
        }
        const double height = std::round(size * 1.15 * _lines);
        return {measured.Width, height};
    }
}
