#pragma once

#include "Tap.hpp"
#include "../../InputSettings.hpp"
#include "../../Input/GamepadUiRouter.hpp"
#include "../../../NativeRuntime/Avalonia/Avalonia.hpp"

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;
    using KeyRowGlfwKey = ::OpenTK::Windowing::GraphicsLibraryFramework::Keys;
    using KeyRowMouseButton = ::OpenTK::Windowing::GraphicsLibraryFramework::MouseButton;
    using KeyRowGamepadButtons = ::MphRead::Mods::Input::GamepadButtons;
    using KeyRowGamepadSnapshot = ::MphRead::Mods::Input::GamepadSnapshot;

    // One rebindable control: what it does on the left, what it is bound to
    // on the right, click and press to change it.
    class KeyRow final : public Av::Controls::Control
    {
    public:
        using Getter = std::function<KeyRowGlfwKey()>;
        using Setter = std::function<void(KeyRowGlfwKey)>;

        explicit KeyRow(const ::MphRead::Mods::InputBindingProperty& property, double labelWidth = 160);
        KeyRow(std::string label, Getter get, Setter set, double labelWidth = 160);

        Av::Event<KeyRow&> Rebound;

        [[nodiscard]] static bool AnyListening() noexcept { return _anyListening; }
        [[nodiscard]] bool Listening() const noexcept { return _listening; }
        [[nodiscard]] std::string_view BindingName() const noexcept;

        [[nodiscard]] KeyRowGamepadButtons ControllerPress(const KeyRowGamepadSnapshot& snapshot);
        void OpenControllerBinding(KeyRowGamepadButtons pressed = KeyRowGamepadButtons::None);

        void Render(Av::Media::DrawingContext& context) override;

    protected:
        void OnPointerPressed(Av::Input::PointerPressedEventArgs& e) override;
        void OnPointerMoved(Av::Input::PointerEventArgs& e) override;
        void OnPointerReleased(Av::Input::PointerReleasedEventArgs& e) override;
        void OnPointerCaptureLost(Av::Input::PointerCaptureLostEventArgs& e) override;
        void OnPointerWheelChanged(Av::Input::PointerWheelEventArgs& e) override;
        void OnPointerEntered(Av::Input::PointerEventArgs& e) override;
        void OnPointerExited(Av::Input::PointerEventArgs& e) override;
        void OnKeyDown(Av::Input::KeyEventArgs& e) override;
        void OnLostFocus(Av::Input::FocusChangedEventArgs& e) override;
        void OnGotFocus(Av::Input::GotFocusEventArgs& e) override;

    private:
        [[nodiscard]] Av::Rect Box() const;
        void Assign(KeyRowGlfwKey key);
        void Done();
        void SetListening(bool value);
        [[nodiscard]] const ::MphRead::Mods::InputBindingProperty& RequireProperty() const;
        [[nodiscard]] static std::optional<KeyRowGlfwKey> Translate(Av::Input::Key key) noexcept;

        const ::MphRead::Mods::InputBindingProperty* _property = nullptr;
        const double _labelWidth;
        std::optional<std::string> _label;
        Getter _get;
        Setter _set;
        bool _listening = false;
        ::MphRead::Mods::Input::GamepadEdges _padEdges{};
        std::optional<std::string> _controllerHint;
        bool _hot = false;
        Tap _tap{};

        static bool _anyListening;
    };
}
