#pragma once

#include "../../../NativeRuntime/Avalonia/Avalonia.hpp"

#include <functional>
#include <memory>
#include <string>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;

    // A text-entry overlay drawn and navigated inside the existing launcher surface.
    class ControllerKeyboard final : public std::enable_shared_from_this<ControllerKeyboard>
    {
    public:
        ControllerKeyboard(Av::Controls::TextBox& target, std::function<void()> closed, bool captureOnly = false);
        ~ControllerKeyboard();

        [[nodiscard]] Av::Controls::Control& NavigationRoot() const noexcept { return *_navigationRoot; }
        void Close(bool accept);

    private:
        void Append(const std::string& value);
        void TargetDetached(Av::Controls::Control& sender);
        void UnsubscribeTarget();
        void Cleanup();

        std::shared_ptr<Av::Controls::Popup> _popup;
        std::shared_ptr<Av::Controls::Panel> _parent;
        std::shared_ptr<Av::Controls::TextBox> _target;
        std::shared_ptr<Av::Controls::TextBlock> _preview;
        std::function<void()> _closed;
        std::string _text;
        std::shared_ptr<Av::Controls::Control> _navigationRoot;
        std::size_t _detachedToken = 0;
        bool _upper = false;
        bool _finished = false;
    };
}
