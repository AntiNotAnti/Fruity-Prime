#pragma once

#include "DeckButton.hpp"

#include <memory>
#include <string>

namespace MphRead::Mods::Launcher::Gui
{
    // The pair at the foot of every screen: leaving on the left, and what the
    // screen is for on the right. It is a DeckButton.
    class UiMark final : public Av::Controls::Decorator
    {
    public:
        enum class Shape
        {
            Cancel,
            Accept,
            // A plus: make a new one of something. Neither yes nor no.
            Add,
            // An arrow into a tray: fetch something that is missing.
            Fetch
        };

        static Av::StyledProperty<std::string>& LabelProperty;

        UiMark(Shape shape, const std::string& label);

        [[nodiscard]] std::string Label() const { return GetValue(LabelProperty); }
        void Label(std::string value) { SetValue(LabelProperty, std::move(value)); }

        Av::Event<UiMark&> Click;

        // The bob, for the one button on a screen that wants looking at.
        [[nodiscard]] bool Idle() const noexcept { return _button->Idle; }
        void Idle(bool value) noexcept { _button->Idle = value; }

        bool Focus() { return _button->Focus(); }

    protected:
        void OnPropertyChanged(const Av::AvaloniaPropertyChangedEventArgs& change) override;
        void OnKeyDown(Av::Input::KeyEventArgs& e) override;

    private:
        [[nodiscard]] static Deck::Face FaceFor(Shape shape);
        [[nodiscard]] static std::string KeyFor(Shape shape);
        [[nodiscard]] std::string Case(const std::string& label) const;

        Shape _shape;
        std::shared_ptr<DeckButton> _button;
    };
}
