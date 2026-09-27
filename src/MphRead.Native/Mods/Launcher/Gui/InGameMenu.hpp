#pragma once

#include "../../../NativeRuntime/Avalonia/Avalonia.hpp"

#include <memory>
#include <vector>

namespace MphRead
{
    class MenuSettings;
}

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;

    // The in-game screen stack shown by Escape inside the shared game window.
    class InGameMenu final : public Av::Controls::Panel
    {
    public:
        explicit InGameMenu(std::shared_ptr<::MphRead::MenuSettings> settings);

        Av::Event<InGameMenu&> Emptied;

        [[nodiscard]] Av::Controls::ControlPtr Top() const noexcept;
        void Back();

    protected:
        void OnKeyDown(Av::Input::KeyEventArgs& e) override;

    private:
        [[nodiscard]] std::shared_ptr<InGameMenu> Self();
        void Push(const Av::Controls::ControlPtr& view);
        void Pop();
        void OpenPauseMenu();
        void OpenSettings();
        void OpenVote();

        std::shared_ptr<::MphRead::MenuSettings> _settings;
        std::vector<Av::Controls::ControlPtr> _stack;
    };
}
