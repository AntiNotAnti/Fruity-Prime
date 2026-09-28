#pragma once

#include "GuiTheme.hpp"
#include "Tap.hpp"

#include <functional>
#include <string>

namespace MphRead::Mods::Launcher::Gui
{
    // A labelled slider, shaped like ChoiceRow so a column of rows lines up.
    class SliderRow final : public Av::Controls::Control
    {
    public:
        SliderRow(std::string label, std::int32_t value, std::function<std::string(std::int32_t)> format = nullptr,
            double labelWidth = 120, std::int32_t min = 0, std::int32_t max = 100, std::int32_t keyStep = 5);

        Av::Event<SliderRow&> ValueChanged;

        [[nodiscard]] std::int32_t Value() const noexcept { return _value; }
        void Value(std::int32_t value);

        void Render(Av::Media::DrawingContext& context) override;

    protected:
        void OnPointerPressed(Av::Input::PointerPressedEventArgs& e) override;
        void OnPointerMoved(Av::Input::PointerEventArgs& e) override;
        void OnPointerReleased(Av::Input::PointerReleasedEventArgs& e) override;
        void OnPointerExited(Av::Input::PointerEventArgs& e) override;
        void OnPointerCaptureLost(Av::Input::PointerCaptureLostEventArgs& e) override;
        void OnKeyDown(Av::Input::KeyEventArgs& e) override;
        void OnGotFocus(Av::Input::GotFocusEventArgs& e) override;
        void OnLostFocus(Av::Input::FocusChangedEventArgs& e) override;

    private:
        // Room kept on the right for the value.
        static constexpr double ValueGutter = 112;

        [[nodiscard]] Av::Rect Track() const;
        void SetFromPointer(double x);
        void BeginDrag(Av::Input::PointerEventArgs& e, Av::Point p);

        const std::string _label;
        const double _labelWidth;
        std::function<std::string(std::int32_t)> _format;
        const std::int32_t _min;
        const std::int32_t _max;
        const std::int32_t _keyStep;
        std::int32_t _value = 0;
        bool _dragging = false;
        bool _hot = false;
        Tap _tap;
    };
}
