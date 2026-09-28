#include "FocusNavigator.hpp"

#include "ControllerNav.hpp"

#include <cmath>
#include <limits>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;
    using ::MphRead::Mods::Input::UiAction;

    bool FocusNavigator::Eligible(Controls::Control& control, const Controls::Control& root)
    {
        if (!control.Focusable() || !control.IsEffectivelyVisible() || !control.IsEffectivelyEnabled()
            || control.Bounds().Width <= 0 || control.Bounds().Height <= 0)
        {
            return false;
        }
        // Screen roots use Focusable for keyboard bubbling, not as menu entries.
        if (dynamic_cast<Controls::UserControl*>(&control) != nullptr)
        {
            return false;
        }
        for (Visual* v = &control; v != nullptr && v != &root; v = v->GetVisualParent())
        {
            auto* input = dynamic_cast<Av::Input::InputElement*>(v);
            if (!v->IsVisible() || (input != nullptr && !input->IsHitTestVisible()))
            {
                return false;
            }
        }
        return true;
    }

    Controls::Control* FocusNavigator::Focused(Controls::Control& root)
    {
        Controls::Control& modal = ControllerNav::ModalRoot(root);
        TopLevel* top = TopLevel::GetTopLevel(&modal);
        auto* focused = top != nullptr ? dynamic_cast<Controls::Control*>(top->FocusedElement()) : nullptr;
        return focused != nullptr && (focused == &modal || modal.IsVisualAncestorOf(focused)) && Eligible(*focused, modal)
            ? focused
            : nullptr;
    }

    Controls::Control* FocusNavigator::Ensure(Controls::Control& root)
    {
        Controls::Control& modal = ControllerNav::ModalRoot(root);
        Controls::Control* current = Focused(modal);
        if (current != nullptr)
        {
            return current;
        }
        const std::vector<Visual*> descendants = modal.GetVisualDescendants();
        for (Visual* v : descendants)
        {
            auto* c = dynamic_cast<Controls::Control*>(v);
            if (c != nullptr && Eligible(*c, modal) && c->GetValue(ControllerNav::NavDefaultProperty))
            {
                current = c;
                break;
            }
        }
        if (current == nullptr)
        {
            for (Visual* v : descendants)
            {
                auto* c = dynamic_cast<Controls::Control*>(v);
                if (c != nullptr && Eligible(*c, modal))
                {
                    current = c;
                    break;
                }
            }
        }
        Focus(current);
        return current;
    }

    void FocusNavigator::Focus(Controls::Control* control)
    {
        if (control == nullptr)
        {
            return;
        }
        control->Focus(Av::Input::NavigationMethod::Directional);
        if (TopLevel* top = TopLevel::GetTopLevel(control))
        {
            top->UpdateLayout();
        }
        control->BringIntoView();
    }

    bool FocusNavigator::Key(Controls::Control& control, Av::Input::Key key)
    {
        Av::Input::KeyEventArgs down(&Av::Input::InputElement::KeyDownEvent);
        down.Key = key;
        control.RaiseEvent(down);
        Av::Input::KeyEventArgs up(&Av::Input::InputElement::KeyUpEvent);
        up.Key = key;
        control.RaiseEvent(up);
        return down.Handled;
    }

    void FocusNavigator::Move(Controls::Control& root, UiAction direction)
    {
        Controls::Control* current = Ensure(root);
        if (current == nullptr)
        {
            return;
        }
        Controls::Control& scope = ControllerNav::Scope(*current, root);
        Controls::Control* explicitTarget = ControllerNav::Find(scope, ControllerNav::Neighbor(*current, direction));
        if (explicitTarget != nullptr && Eligible(*explicitTarget, scope))
        {
            Focus(explicitTarget);
            return;
        }
        Controls::Control& area = scope;
        const std::optional<Point> origin
            = current->TranslatePoint(Point{current->Bounds().Width / 2, current->Bounds().Height / 2}, &area);
        if (!origin.has_value())
        {
            return;
        }
        const bool vertical = direction == UiAction::Up || direction == UiAction::Down;
        const double sign = direction == UiAction::Up || direction == UiAction::Left ? -1 : 1;
        Controls::Control* best = nullptr;
        Controls::Control* wrapped = nullptr;
        double score = std::numeric_limits<double>::max();
        double wrapScore = std::numeric_limits<double>::max();
        for (Visual* v : area.GetVisualDescendants())
        {
            auto* candidate = dynamic_cast<Controls::Control*>(v);
            if (candidate == nullptr || candidate == current || !Eligible(*candidate, area))
            {
                continue;
            }
            const std::optional<Point> point
                = candidate->TranslatePoint(Point{candidate->Bounds().Width / 2, candidate->Bounds().Height / 2}, &area);
            if (!point.has_value())
            {
                continue;
            }
            const double dx = point->X - origin->X;
            const double dy = point->Y - origin->Y;
            const double forward = (vertical ? dy : dx) * sign;
            const double across = std::abs(vertical ? dx : dy);
            if (forward <= 1)
            {
                const double distanceBack = forward + across * 3;
                if (distanceBack < wrapScore)
                {
                    wrapScore = distanceBack;
                    wrapped = candidate;
                }
                continue;
            }
            const double distance = forward + across * 3;
            if (distance < score)
            {
                score = distance;
                best = candidate;
            }
        }
        if (best == nullptr && area.GetValue(ControllerNav::NavWrapProperty))
        {
            best = wrapped;
        }
        Focus(best);
    }
}
