#include "InGameMenu.hpp"

#include "PauseMenuView.hpp"
#include "PlayScreen.hpp"
#include "SettingsView.hpp"
#include "../../Chat/ChatBox.hpp"
#include "../../DebugLog.hpp"
#include "../../Network/DemoRecorder.hpp"
#include "../../Network/MapVote.hpp"
#include "../../PauseMenu.hpp"
#include "../../SpectatorMode.hpp"
#include "../../ThumbnailGenerator.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;

    InGameMenu::InGameMenu(std::shared_ptr<::MphRead::MenuSettings> settings)
        : _settings(std::move(settings))
    {
        Background(Media::Brushes::Transparent());
        Focusable(true);
        OpenPauseMenu();
    }

    Av::Controls::ControlPtr InGameMenu::Top() const noexcept
    {
        return _stack.empty() ? nullptr : _stack.back();
    }

    std::shared_ptr<InGameMenu> InGameMenu::Self()
    {
        return std::static_pointer_cast<InGameMenu>(shared_from_this());
    }

    void InGameMenu::Push(const Av::Controls::ControlPtr& view)
    {
        if (!_stack.empty())
        {
            _stack.back()->IsVisible(false);
        }
        _stack.push_back(view);
        Children.Add(view);
        view->Focus();
    }

    void InGameMenu::Pop()
    {
        // C# keeps `this` alive on the event stack while Emptied can remove
        // the menu from UiSurface. Preserve that lifetime through this call.
        const std::shared_ptr<InGameMenu> keepAlive = Self();
        (void)keepAlive;

        if (_stack.empty())
        {
            return;
        }
        Av::Controls::ControlPtr top = _stack.back();
        _stack.pop_back();
        Children.Remove(top);
        if (!_stack.empty())
        {
            _stack.back()->IsVisible(true);
            _stack.back()->Focus();
            return;
        }
        Emptied(*this);
    }

    void InGameMenu::Back()
    {
        const std::shared_ptr<InGameMenu> keepAlive = Self();
        Pop();
    }

    void InGameMenu::OpenPauseMenu()
    {
        auto view = std::make_shared<PauseMenuView>(true);
        view->Resumed += [this](PauseMenuView&)
        {
            const std::shared_ptr<InGameMenu> keepAlive = Self();
            Pop();
        };
        view->FullscreenRequested += [this](PauseMenuView&)
        {
            const std::shared_ptr<InGameMenu> keepAlive = Self();
            ::MphRead::Mods::PauseMenu::RequestFullscreenToggle();
            Pop();
        };
        view->SettingsRequested += [this](PauseMenuView&)
        {
            const std::shared_ptr<InGameMenu> keepAlive = Self();
            OpenSettings();
        };
        view->VoteMapRequested += [this](PauseMenuView&)
        {
            const std::shared_ptr<InGameMenu> keepAlive = Self();
            OpenVote();
        };
        view->SpectateRequested += [this](PauseMenuView&)
        {
            const std::shared_ptr<InGameMenu> keepAlive = Self();
            ::MphRead::Mods::SpectatorMode::Start();
            Pop();
        };
        view->RejoinRequested += [this](PauseMenuView&)
        {
            const std::shared_ptr<InGameMenu> keepAlive = Self();
            ::MphRead::Mods::SpectatorMode::Rejoin();
            Pop();
        };
        view->RecordToggleRequested += [this](PauseMenuView&)
        {
            const std::shared_ptr<InGameMenu> keepAlive = Self();
            if (::MphRead::Mods::Network::DemoRecorder::IsRecording())
            {
                const std::optional<std::string> path
                    = ::MphRead::Mods::Network::DemoRecorder::CurrentPath();
                std::cout << "[demo] recording saved to " << path.value_or("") << '\n';
                ::MphRead::Mods::Network::DemoRecorder::Stop();
            }
            else
            {
                static_cast<void>(::MphRead::Mods::Network::DemoRecorder::Start());
            }
            Pop();
        };
        view->LeaveRequested += [this](PauseMenuView&)
        {
            const std::shared_ptr<InGameMenu> keepAlive = Self();
            ::MphRead::Mods::PauseMenu::RequestLeave();
            Pop();
        };
        view->QuitRequested += [this](PauseMenuView&)
        {
            const std::shared_ptr<InGameMenu> keepAlive = Self();
            ::MphRead::Mods::PauseMenu::RequestQuit();
            Pop();
        };
        Push(view);
        view->FocusResume();
    }

    void InGameMenu::OpenSettings()
    {
        auto view = std::make_shared<SettingsView>(_settings, true);
        view->Closed += [this](SettingsView&)
        {
            const std::shared_ptr<InGameMenu> keepAlive = Self();
            Pop();
        };
        view->StylusPlacementRequested += [this](SettingsView&)
        {
            const std::shared_ptr<InGameMenu> keepAlive = Self();
            while (!_stack.empty())
            {
                Pop();
            }
        };
        Push(view);
    }

    void InGameMenu::OpenVote()
    {
        const std::string why = ::MphRead::Mods::Network::MapVote::WhyNotProposing();
        if (!why.empty())
        {
            ::MphRead::Mods::Chat::ChatBox::System(std::optional<std::string>(why));
            Pop();
            return;
        }

        std::vector<std::string> rooms;
        try
        {
            rooms = ::MphRead::Mods::ThumbnailGenerator::MultiplayerRooms();
        }
        catch (const std::exception& exception)
        {
            ::MphRead::Mods::Chat::ChatBox::System(
                std::optional<std::string>("no maps to vote for"));
            ::MphRead::Mods::DebugLog::Exception("pause", exception);
            return;
        }
        if (rooms.empty())
        {
            ::MphRead::Mods::Chat::ChatBox::System(
                std::optional<std::string>("no maps to vote for"));
            return;
        }

        auto view = std::make_shared<PlayScreen>(_settings, rooms,
            PlayScreen::Face::Vote, true);
        view->Closed += [this](PlayScreen&)
        {
            const std::shared_ptr<InGameMenu> keepAlive = Self();
            Pop();
        };
        view->Voted += [this](PlayScreen&, std::string room)
        {
            const std::shared_ptr<InGameMenu> keepAlive = Self();
            ::MphRead::Mods::Network::MapVote::Propose(
                std::optional<std::string>(std::move(room)));
            // Pop the ballot and the pause menu it was opened over.
            Pop();
            Pop();
        };
        Push(view);
    }

    void InGameMenu::OnKeyDown(Av::Input::KeyEventArgs& e)
    {
        const std::shared_ptr<InGameMenu> keepAlive = Self();
        if (e.Key == Av::Input::Key::Escape)
        {
            Pop();
            e.Handled = true;
            return;
        }
        Panel::OnKeyDown(e);
    }
}
