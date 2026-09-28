#pragma once

#include "../../../NativeRuntime/Avalonia/Avalonia.hpp"
#include "../../Input/GamepadUiRouter.hpp"

#include <memory>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;

    class ControllerKeyboard;

    class GamepadNavigation final
    {
    public:
        Av::Event<> Changed;

        GamepadNavigation();
        void Update(Av::Controls::Control& root);

    private:
        void Dispatch(::MphRead::Mods::Input::UiAction action);

        ::MphRead::Mods::Input::GamepadUiRouter _router;
        std::shared_ptr<Av::Controls::Control> _root;
        std::shared_ptr<ControllerKeyboard> _keyboard;
    };
}
