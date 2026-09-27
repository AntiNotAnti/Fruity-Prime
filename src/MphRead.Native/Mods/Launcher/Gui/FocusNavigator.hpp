#pragma once

#include "GuiTheme.hpp"
#include "../../Input/GamepadUiRouter.hpp"

namespace MphRead::Mods::Launcher::Gui
{
    // Where focus goes on a pad: explicit neighbours first, then the nearest
    // control in the direction pressed.
    class FocusNavigator final
    {
    public:
        FocusNavigator() = delete;

        [[nodiscard]] static Av::Controls::Control* Focused(Av::Controls::Control& root);
        static Av::Controls::Control* Ensure(Av::Controls::Control& root);
        static void Focus(Av::Controls::Control* control);
        static bool Key(Av::Controls::Control& control, Av::Input::Key key);
        static void Move(Av::Controls::Control& root, ::MphRead::Mods::Input::UiAction direction);

    private:
        [[nodiscard]] static bool Eligible(Av::Controls::Control& control, const Av::Controls::Control& root);
    };
}
