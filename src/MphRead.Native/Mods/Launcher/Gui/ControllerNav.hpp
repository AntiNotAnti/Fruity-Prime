#pragma once

#include "GuiTheme.hpp"
#include "../../Input/GamepadUiRouter.hpp"

#include <optional>
#include <string>

namespace MphRead::Mods::Launcher::Gui
{
    // Stable semantic identifiers survive rebuilding a screen and translated labels.
    class ControllerNav final : public Av::AvaloniaObject
    {
    public:
        static Av::AttachedProperty<std::optional<std::string>>& NavIdProperty;
        static Av::AttachedProperty<std::optional<std::string>>& NavScopeProperty;
        static Av::AttachedProperty<bool>& NavDefaultProperty;
        static Av::AttachedProperty<bool>& NavWrapProperty;
        static Av::AttachedProperty<bool>& ModalProperty;
        static Av::AttachedProperty<std::optional<std::string>>& NavUpProperty;
        static Av::AttachedProperty<std::optional<std::string>>& NavDownProperty;
        static Av::AttachedProperty<std::optional<std::string>>& NavLeftProperty;
        static Av::AttachedProperty<std::optional<std::string>>& NavRightProperty;

        [[nodiscard]] static Av::Controls::Control& ModalRoot(Av::Controls::Control& root);
        [[nodiscard]] static Av::Controls::Control& Scope(Av::Controls::Control& control, Av::Controls::Control& root);
        [[nodiscard]] static std::optional<std::string> Neighbor(const Av::Controls::Control& control,
            ::MphRead::Mods::Input::UiAction action);
        [[nodiscard]] static Av::Controls::Control* Find(Av::Controls::Control& root, const std::optional<std::string>& id);
        static void Identify(Av::Controls::Control& control, const std::string& id, bool initial = false);
    };

    class ControllerNavScope final
    {
    public:
        void Capture(Av::Controls::Control& root);
        void Restore(Av::Controls::Control& root);

    private:
        std::optional<std::string> _focusedId;
    };
}
