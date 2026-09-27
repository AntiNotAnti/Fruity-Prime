#include "ControllerNav.hpp"

#include "FocusNavigator.hpp"

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;
    using ::MphRead::Mods::Input::UiAction;

    AttachedProperty<std::optional<std::string>>& ControllerNav::NavIdProperty
        = RegisterAttached<ControllerNav, std::optional<std::string>>("NavId");
    AttachedProperty<std::optional<std::string>>& ControllerNav::NavScopeProperty
        = RegisterAttached<ControllerNav, std::optional<std::string>>("NavScope");
    AttachedProperty<bool>& ControllerNav::NavDefaultProperty = RegisterAttached<ControllerNav, bool>("NavDefault");
    AttachedProperty<bool>& ControllerNav::NavWrapProperty = RegisterAttached<ControllerNav, bool>("NavWrap");
    AttachedProperty<bool>& ControllerNav::ModalProperty = RegisterAttached<ControllerNav, bool>("Modal");
    AttachedProperty<std::optional<std::string>>& ControllerNav::NavUpProperty
        = RegisterAttached<ControllerNav, std::optional<std::string>>("NavUp");
    AttachedProperty<std::optional<std::string>>& ControllerNav::NavDownProperty
        = RegisterAttached<ControllerNav, std::optional<std::string>>("NavDown");
    AttachedProperty<std::optional<std::string>>& ControllerNav::NavLeftProperty
        = RegisterAttached<ControllerNav, std::optional<std::string>>("NavLeft");
    AttachedProperty<std::optional<std::string>>& ControllerNav::NavRightProperty
        = RegisterAttached<ControllerNav, std::optional<std::string>>("NavRight");

    Controls::Control& ControllerNav::ModalRoot(Controls::Control& root)
    {
        Controls::Control* last = nullptr;
        for (Visual* v : root.GetVisualDescendants())
        {
            auto* c = dynamic_cast<Controls::Control*>(v);
            if (c != nullptr && c->GetValue(ModalProperty) && c->IsEffectivelyVisible())
            {
                last = c;
            }
        }
        return last != nullptr ? *last : root;
    }

    Controls::Control& ControllerNav::Scope(Controls::Control& control, Controls::Control& root)
    {
        for (Visual* v : control.GetVisualAncestors())
        {
            auto* c = dynamic_cast<Controls::Control*>(v);
            if (c != nullptr && c->GetValue(NavScopeProperty).has_value())
            {
                return *c;
            }
        }
        return root;
    }

    std::optional<std::string> ControllerNav::Neighbor(const Controls::Control& control, UiAction action)
    {
        switch (action)
        {
        case UiAction::Up:
            return control.GetValue(NavUpProperty);
        case UiAction::Down:
            return control.GetValue(NavDownProperty);
        case UiAction::Left:
            return control.GetValue(NavLeftProperty);
        default:
            return control.GetValue(NavRightProperty);
        }
    }

    Controls::Control* ControllerNav::Find(Controls::Control& root, const std::optional<std::string>& id)
    {
        if (!id.has_value())
        {
            return nullptr;
        }
        for (Visual* v : root.GetVisualDescendants())
        {
            auto* c = dynamic_cast<Controls::Control*>(v);
            if (c != nullptr && c->GetValue(NavIdProperty) == id)
            {
                return c;
            }
        }
        return nullptr;
    }

    void ControllerNav::Identify(Controls::Control& control, const std::string& id, bool initial)
    {
        control.SetValue(NavIdProperty, std::optional<std::string>(id));
        control.SetValue(NavDefaultProperty, initial);
    }

    void ControllerNavScope::Capture(Controls::Control& root)
    {
        Controls::Control* focused = FocusNavigator::Focused(root);
        _focusedId = focused != nullptr ? focused->GetValue(ControllerNav::NavIdProperty) : std::nullopt;
    }

    void ControllerNavScope::Restore(Controls::Control& root)
    {
        Controls::Control* control = ControllerNav::Find(root, _focusedId);
        if (control != nullptr && control->IsEffectivelyVisible() && control->IsEffectivelyEnabled())
        {
            FocusNavigator::Focus(control);
        }
        else
        {
            (void)FocusNavigator::Ensure(root);
        }
    }
}
