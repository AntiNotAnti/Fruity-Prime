#pragma once

#include "../../../NativeRuntime/Avalonia/Avalonia.hpp"

#include <memory>
#include <string>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;
    class UiMark;

    // One question and the two marks that answer it.
    class ConfirmScreen final : public Av::Controls::UserControl
    {
    public:
        ConfirmScreen(std::string question, std::string yes = "yes", std::string no = "no",
            bool overGame = false);

        Av::Event<ConfirmScreen&, bool> Answered;

    protected:
        void OnKeyDown(Av::Input::KeyEventArgs& e) override;

    private:
        std::shared_ptr<UiMark> _no;
    };
}
