#include "GamepadNavigation.hpp"

#include "ControllerKeyboard.hpp"
#include "FocusNavigator.hpp"
#include "KeyRow.hpp"
#include "PadRow.hpp"
#include "Rows.hpp"
#include "SliderRow.hpp"
#include "UiTabs.hpp"
#include "../../../NativeRuntime/System/Runtime.hpp"

#include <algorithm>
#include <memory>
#include <utility>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Input = ::MphRead::Mods::Input;
    using namespace ::MphRead::NativeRuntime::Avalonia;

    GamepadNavigation::GamepadNavigation()
    {
        _router.Action.Add([this](Input::UiAction action) { Dispatch(action); });
    }

    void GamepadNavigation::Update(Controls::Control& root)
    {
        if (!Input::GamepadContexts::Focused())
        {
            _router.Reset();
            return;
        }
        if (_root.get() != &root)
        {
            _root = std::dynamic_pointer_cast<Controls::Control>(root.shared_from_this());
            _router.Reset();
        }
        const Input::GamepadSnapshot snapshot = Input::GamepadManager::Snapshot();
        Controls::Control* focusedRaw = FocusNavigator::Focused(root);
        const std::shared_ptr<Controls::Control> focusedHold = focusedRaw != nullptr
            ? std::dynamic_pointer_cast<Controls::Control>(focusedRaw->shared_from_this()) : nullptr;
        Controls::Control* focused = focusedHold.get();
        if (auto* padRow = dynamic_cast<PadRow*>(focused); padRow != nullptr && Input::GamepadContexts::Capturing())
        {
            // Binding capture receives this UI tick's published snapshot so a
            // physical press cannot fall between dispatcher timer callbacks.
            padRow->Check(snapshot);
            _router.Reset();
            return;
        }
        if (auto* keyRow = dynamic_cast<KeyRow*>(focused); keyRow != nullptr && keyRow->Listening())
        {
            const KeyRowGamepadButtons pressed = keyRow->ControllerPress(snapshot);
            if (pressed != KeyRowGamepadButtons::None)
            {
                keyRow->OpenControllerBinding(pressed);
                _router.Reset();
                Changed();
                return;
            }
        }
        _router.Update(snapshot, Input::GamepadContexts::Capturing()
            ? Input::GamepadContext::BindingCapture : Input::GamepadContext::Menu,
            ::MphRead::NativeRuntime::EnvironmentTickCount64());
    }

    void GamepadNavigation::Dispatch(Input::UiAction action)
    {
        Changed();
        std::shared_ptr<Controls::Control> rootHold;
        if (_keyboard != nullptr)
        {
            rootHold = std::dynamic_pointer_cast<Controls::Control>(_keyboard->NavigationRoot().shared_from_this());
        }
        else
        {
            rootHold = _root;
        }
        if (rootHold == nullptr)
        {
            return;
        }
        Controls::Control& root = *rootHold;
        Controls::Control* focusedRaw = FocusNavigator::Ensure(root);
        const std::shared_ptr<Controls::Control> focusedHold = focusedRaw != nullptr
            ? std::dynamic_pointer_cast<Controls::Control>(focusedRaw->shared_from_this()) : nullptr;
        Controls::Control* focused = focusedHold.get();
        if (focused == nullptr)
        {
            return;
        }
        if (action == Input::UiAction::Accept)
        {
            if (auto* keyboardRow = dynamic_cast<KeyRow*>(focused); keyboardRow != nullptr)
            {
                keyboardRow->OpenControllerBinding();
                _router.Reset();
                return;
            }
            if (auto* text = dynamic_cast<Controls::TextBox*>(focused); text != nullptr && _keyboard == nullptr)
            {
                _keyboard = std::make_shared<ControllerKeyboard>(*text, [this]
                {
                    _keyboard.reset();
                    _router.Reset();
                });
                return;
            }
        }
        if (action == Input::UiAction::Back && _keyboard != nullptr)
        {
            _keyboard->Close(false);
            return;
        }
        if (action == Input::UiAction::PreviousTab || action == Input::UiAction::NextTab)
        {
            for (Visual* visual : root.GetVisualDescendants())
            {
                auto* tabs = dynamic_cast<UiTabs*>(visual);
                if (tabs != nullptr && tabs->IsEffectivelyVisible())
                {
                    const std::shared_ptr<UiTabs> tabsHold
                        = std::dynamic_pointer_cast<UiTabs>(tabs->shared_from_this());
                    tabsHold->Index(tabsHold->Index() + (action == Input::UiAction::NextTab ? 1 : -1));
                    (void)FocusNavigator::Ensure(root);
                    break;
                }
            }
            return;
        }
        if (action == Input::UiAction::PageUp || action == Input::UiAction::PageDown)
        {
            for (Visual* visual : focused->GetVisualAncestors())
            {
                auto* scroll = dynamic_cast<Controls::ScrollViewer*>(visual);
                if (scroll == nullptr)
                {
                    continue;
                }
                const std::shared_ptr<Controls::ScrollViewer> scrollHold
                    = std::dynamic_pointer_cast<Controls::ScrollViewer>(scroll->shared_from_this());
                const Vector offset = scrollHold->Offset();
                const double direction = action == Input::UiAction::PageDown ? 1.0 : -1.0;
                scrollHold->Offset(Vector{offset.X,
                    std::max(0.0, offset.Y + direction * scrollHold->Viewport().Height * 0.8)});
                break;
            }
            return;
        }
        if (action == Input::UiAction::Accept || action == Input::UiAction::Back)
        {
            (void)FocusNavigator::Key(*focused,
                action == Input::UiAction::Accept ? Av::Input::Key::Enter : Av::Input::Key::Escape);
            return;
        }
        Av::Input::Key key = Av::Input::Key::Right;
        switch (action)
        {
        case Input::UiAction::Up: key = Av::Input::Key::Up; break;
        case Input::UiAction::Down: key = Av::Input::Key::Down; break;
        case Input::UiAction::Left: key = Av::Input::Key::Left; break;
        case Input::UiAction::Right: key = Av::Input::Key::Right; break;
        default: break;
        }
        if (action == Input::UiAction::Left || action == Input::UiAction::Right)
        {
            const bool editable = dynamic_cast<ChoiceRow*>(focused) != nullptr
                || dynamic_cast<SliderRow*>(focused) != nullptr
                || dynamic_cast<PadRow*>(focused) != nullptr
                || dynamic_cast<Controls::TextBox*>(focused) != nullptr;
            if (editable && FocusNavigator::Key(*focused, key))
            {
                return;
            }
        }
        FocusNavigator::Move(root, action);
    }
}
