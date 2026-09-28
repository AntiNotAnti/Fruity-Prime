#include "UiWord.hpp"


#include <algorithm>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;

    StyledProperty<std::string>& UiWord::TextProperty = Register<UiWord, std::string>("Text", std::string{});
    StyledProperty<bool>& UiWord::SelectedProperty = Register<UiWord, bool>("Selected", false);

    UiWord::UiWord(const std::string& text, double size, Media::FontFamilyPtr font, std::optional<Media::Color> colour)
        : _size(size), _font(font != nullptr ? std::move(font) : GuiTheme::Display()),
          _colour(colour.value_or(GuiTheme::Text))
    {
        static const bool registered = []
        {
            AffectsRender<UiWord>(TextProperty, SelectedProperty, IsEnabledProperty);
            AffectsMeasure<UiWord>(TextProperty);
            return true;
        }();
        (void)registered;
        Text(text);
        Focusable(true);
        Cursor(std::make_shared<Input::Cursor>(Input::StandardCursorType::Hand));
    }

    Media::FormattedText UiWord::Label(const Media::IBrushPtr& brush) const
    {
        return Media::FormattedText(Text(), Media::InvariantCulture, Media::FlowDirection::LeftToRight,
            Media::Typeface(_font, Media::FontStyle::Normal, Media::FontWeight::Normal), _size, brush);
    }

    Size UiWord::MeasureOverride(Size availableSize)
    {
        const Media::FormattedText text = Label(GuiTheme::TextBrush);
        // Its own size in both directions: a bare Control measures to nothing.
        return {std::min(text.Width() + 4, availableSize.Width), text.Height() + 2};
    }

    void UiWord::OnPointerEntered(Input::PointerEventArgs& e)
    {
        InvalidateVisual();
        Control::OnPointerEntered(e);
    }

    void UiWord::OnPointerExited(Input::PointerEventArgs& e)
    {
        _tap.Cancel();
        InvalidateVisual();
        Control::OnPointerExited(e);
    }

    void UiWord::OnPointerPressed(Input::PointerPressedEventArgs& e)
    {
        _tap.Press(e, *this);
        Focus();
        e.Pointer->Capture(this);
        e.Handled = true;
        InvalidateVisual();
        Control::OnPointerPressed(e);
    }

    void UiWord::OnPointerMoved(Input::PointerEventArgs& e)
    {
        // A finger that has set off across the screen is scrolling whatever
        // this sits in.
        _tap.Moved(e, *this);
        Control::OnPointerMoved(e);
    }

    void UiWord::OnPointerReleased(Input::PointerReleasedEventArgs& e)
    {
        const bool tapped = _tap.Release(e, *this);
        InvalidateVisual();
        if (e.Pointer->Captured() == this)
        {
            e.Pointer->Capture(nullptr);
        }
        Control::OnPointerReleased(e);
        if (tapped && IsEnabled())
        {
            Click(*this);
        }
    }

    void UiWord::OnPointerCaptureLost(Input::PointerCaptureLostEventArgs& e)
    {
        // The scroll gesture above has taken the pointer.
        _tap.Cancel();
        InvalidateVisual();
        Control::OnPointerCaptureLost(e);
    }

    void UiWord::OnGotFocus(Input::GotFocusEventArgs& e)
    {
        InvalidateVisual();
        Control::OnGotFocus(e);
    }

    void UiWord::OnLostFocus(Input::FocusChangedEventArgs& e)
    {
        InvalidateVisual();
        Control::OnLostFocus(e);
    }

    void UiWord::OnKeyDown(Input::KeyEventArgs& e)
    {
        if (e.Key == Input::Key::Enter || e.Key == Input::Key::Space)
        {
            e.Handled = true;
            if (IsEnabled())
            {
                Click(*this);
            }
            return;
        }
        Control::OnKeyDown(e);
    }

    void UiWord::Render(Media::DrawingContext& context)
    {
        // Avalonia hit-tests what was drawn: without this the word only
        // answers the pointer over its own glyphs.
        context.FillRectangle(Media::Brushes::Transparent(), Rect(0, 0, Bounds().Width, Bounds().Height));
        const bool lit = (IsPointerOver() || IsFocused()) && IsEnabled();
        // The accent, for every word, on the frame the pointer arrives: a
        // state, not an animation.
        const Media::Color colour = !IsEnabled() ? GuiTheme::TextDim
            : lit ? GuiTheme::Shade(GuiTheme::Accent, 0.2)
            : Selected() ? GuiTheme::Accent : _colour;
        context.DrawText(Label(std::make_shared<Media::SolidColorBrush>(colour)), Point{0, 0});
    }
}
