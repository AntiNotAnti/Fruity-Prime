#include "UiMark.hpp"

#include "../../../NativeRuntime/System/Globalization.hpp"

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;

    StyledProperty<std::string>& UiMark::LabelProperty = Register<UiMark, std::string>("Label", std::string{});

    Deck::Face UiMark::FaceFor(Shape shape)
    {
        // Two roles, two colours: leaving is brass; everything that does what
        // the screen is for is the one blue.
        switch (shape)
        {
        case Shape::Cancel:
            return Deck::Face::Brass();
        case Shape::Accept:
            return Deck::Face::Blue();
        default:
            return Deck::Face::Slate();
        }
    }

    std::string UiMark::KeyFor(Shape shape)
    {
        // Only the two that have one.
        switch (shape)
        {
        case Shape::Cancel:
            return "ESC";
        case Shape::Accept:
            return "⏎";
        default:
            return "";
        }
    }

    UiMark::UiMark(Shape shape, const std::string& label)
        : _shape(shape)
    {
        const bool accept = shape == Shape::Accept;
        _button = std::make_shared<DeckButton>(Case(label), FaceFor(shape), accept ? 1.4 : 1.05, accept ? 1.4 : 1.0,
            accept ? 0.4 : 0.5, accept ? 6 : 4);
        _button->KeyCap = KeyFor(shape);
        _button->Click += [this](DeckButton&)
        {
            if (IsEnabled())
            {
                Click(*this);
            }
        };
        Child(_button);
        SetCurrentValue(LabelProperty, label);
    }

    std::string UiMark::Case(const std::string& label) const
    {
        // The commit shouts and the way out does not: "START" and "Back".
        if (label.empty())
        {
            return label;
        }
        if (_shape == Shape::Accept)
        {
            return ::MphRead::NativeRuntime::ToUpperInvariant(label);
        }
        const std::u32string chars = Media::ToUtf32(label);
        const std::string first = ::MphRead::NativeRuntime::ToUpperInvariant(Media::ToUtf8(chars.substr(0, 1)));
        return first + Media::ToUtf8(chars.substr(1));
    }

    void UiMark::OnPropertyChanged(const AvaloniaPropertyChangedEventArgs& change)
    {
        Decorator::OnPropertyChanged(change);
        if (&change.Property == &LabelProperty)
        {
            if (_button != nullptr)
            {
                _button->Text(Case(Label()));
            }
        }
        else if (&change.Property == &IsEnabledProperty)
        {
            if (_button != nullptr)
            {
                _button->IsEnabled(IsEnabled());
            }
        }
    }

    void UiMark::OnKeyDown(Input::KeyEventArgs& e)
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
        Decorator::OnKeyDown(e);
    }
}
