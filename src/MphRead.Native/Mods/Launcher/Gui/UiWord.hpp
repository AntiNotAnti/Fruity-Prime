#pragma once

#include "GuiTheme.hpp"
#include "Tap.hpp"

#include <optional>
#include <string>

namespace MphRead::Mods::Launcher::Gui
{
    // One word of a menu, and nothing else: focus, Enter, and a press that
    // survives a finger.
    class UiWord final : public Av::Controls::Control
    {
    public:
        static Av::StyledProperty<std::string>& TextProperty;
        static Av::StyledProperty<bool>& SelectedProperty;

        explicit UiWord(const std::string& text, double size = 20, Av::Media::FontFamilyPtr font = nullptr,
            std::optional<Av::Media::Color> colour = std::nullopt);

        [[nodiscard]] std::string Text() const { return GetValue(TextProperty); }
        void Text(std::string value) { SetValue(TextProperty, std::move(value)); }
        // The entry whose page is open, in a strip of them.
        [[nodiscard]] bool Selected() const { return GetValue(SelectedProperty); }
        void Selected(bool value) { SetValue(SelectedProperty, value); }

        Av::Event<UiWord&> Click;

        void Render(Av::Media::DrawingContext& context) override;

    protected:
        Av::Size MeasureOverride(Av::Size availableSize) override;
        void OnPointerEntered(Av::Input::PointerEventArgs& e) override;
        void OnPointerExited(Av::Input::PointerEventArgs& e) override;
        void OnPointerPressed(Av::Input::PointerPressedEventArgs& e) override;
        void OnPointerMoved(Av::Input::PointerEventArgs& e) override;
        void OnPointerReleased(Av::Input::PointerReleasedEventArgs& e) override;
        void OnPointerCaptureLost(Av::Input::PointerCaptureLostEventArgs& e) override;
        void OnGotFocus(Av::Input::GotFocusEventArgs& e) override;
        void OnLostFocus(Av::Input::FocusChangedEventArgs& e) override;
        void OnKeyDown(Av::Input::KeyEventArgs& e) override;

    private:
        [[nodiscard]] Av::Media::FormattedText Label(const Av::Media::IBrushPtr& brush) const;

        const double _size;
        const Av::Media::FontFamilyPtr _font;
        const Av::Media::Color _colour;
        Tap _tap;
    };
}
