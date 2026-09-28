#pragma once

#include "../../../NativeRuntime/Avalonia/Avalonia.hpp"
#include "DeckButton.hpp"

#include <functional>
#include <memory>
#include <string>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;

    // The match's pause screen, drawn into the same surface as every other screen.
    class PauseMenuView final : public Av::Controls::UserControl
    {
    public:
        explicit PauseMenuView(bool offerWindowMode);

        Av::Event<PauseMenuView&> Resumed;
        Av::Event<PauseMenuView&> SettingsRequested;
        Av::Event<PauseMenuView&> LeaveRequested;
        Av::Event<PauseMenuView&> QuitRequested;
        Av::Event<PauseMenuView&> FullscreenRequested;
        Av::Event<PauseMenuView&> SpectateRequested;
        Av::Event<PauseMenuView&> RejoinRequested;
        Av::Event<PauseMenuView&> RecordToggleRequested;
        Av::Event<PauseMenuView&> VoteMapRequested;

        void RefreshVote();
        void FocusResume();

    protected:
        void OnKeyDown(Av::Input::KeyEventArgs& e) override;

    private:
        [[nodiscard]] double NeededHeight() const;
        void AnswerVote(bool yes);
        void FitToHost(double height);
        [[nodiscard]] static std::string WindowLabel();
        [[nodiscard]] static std::shared_ptr<DeckButton> Add(
            const std::shared_ptr<Av::Controls::StackPanel>& menu,
            std::string text, std::function<void()> action,
            Deck::Face face = Deck::Face::Slate());

        std::shared_ptr<DeckButton> _resume;
        std::shared_ptr<DeckButton> _voteYes;
        std::shared_ptr<DeckButton> _voteNo;
        std::shared_ptr<Av::Controls::StackPanel> _menu;
        std::shared_ptr<Av::Controls::LayoutTransformControl> _scaler;
        Av::Threading::DispatcherTimer _voteTimer;
    };
}
