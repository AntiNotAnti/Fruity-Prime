#include "LobbyScreen.hpp"

#include "CreateServerScreen.hpp"
#include "Deck.hpp"
#include "DeckButton.hpp"
#include "GuiTheme.hpp"
#include "LobbyPlayerRow.hpp"
#include "MapCardPicker.hpp"
#include "Rows.hpp"
#include "UiLayout.hpp"
#include "UiMark.hpp"
#include "../../Chat/NetChat.hpp"
#include "../../Network/LobbyRules.hpp"
#include "../../Network/NetHostSession.hpp"
#include "../../Network/NetSession.hpp"
#include "../../Network/NetProtocol.hpp"
#include "../../Network/SessionProtocol.hpp"
#include "../../ThumbnailGenerator.hpp"
#include "../Portable/LauncherPrefs.hpp"
#include "../../../Menu.hpp"
#include "../../../Metadata/Metadata.hpp"
#include "../../../NativeRuntime/Avalonia/Threading.hpp"
#include "../../../NativeRuntime/System/Globalization.hpp"
#include "../../../NativeRuntime/System/IO.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"
#include "../../../NativeRuntime/System/Number.hpp"

#include <algorithm>
#include <cmath>
#include <exception>
#include <functional>
#include <limits>
#include <string>
#include <utility>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Runtime = ::MphRead::NativeRuntime;
    namespace Network = ::MphRead::Mods::Network;
    namespace Threading = ::MphRead::NativeRuntime::Avalonia::Threading;
    using namespace ::MphRead::NativeRuntime::Avalonia;

    namespace
    {
        [[nodiscard]] std::shared_ptr<ButtonToggleRow> Toggle(const std::string& label, bool on = false)
        {
            return std::make_shared<ButtonToggleRow>(label, on);
        }

        [[nodiscard]] std::shared_ptr<UiMark> ActionButton(const std::string& label,
            const std::function<void()>& action)
        {
            auto button = std::make_shared<UiMark>(
                label == "Leave" ? UiMark::Shape::Cancel : UiMark::Shape::Accept, label);
            button->Margin(Thickness(2));
            button->Click += [action](UiMark&) { action(); };
            return button;
        }

        [[nodiscard]] std::shared_ptr<DeckButton> SmallButton(const std::string& label,
            Deck::Face face, const std::function<void()>& action)
        {
            auto button = std::make_shared<DeckButton>(label, face, 0.82, 0.8, 0.34, 4);
            button->Click += [action](DeckButton&) { action(); };
            return button;
        }

        [[nodiscard]] std::vector<std::string> NumberOptions(std::int32_t first, std::int32_t count)
        {
            std::vector<std::string> values;
            values.reserve(static_cast<std::size_t>(count));
            for (std::int32_t i = 0; i < count; ++i)
            {
                values.push_back(std::to_string(first + i));
            }
            return values;
        }
    }

    const std::array<LobbyScreen::GameType, 7> LobbyScreen::_gameTypes{{
        {"Battle", ::MphRead::GameMode::Battle, ::MphRead::GameMode::BattleTeams, false, false},
        {"Survival", ::MphRead::GameMode::Survival, ::MphRead::GameMode::SurvivalTeams, false, false},
        {"Bounty", ::MphRead::GameMode::Bounty, ::MphRead::GameMode::BountyTeams, false, false},
        {"Defender", ::MphRead::GameMode::Defender, ::MphRead::GameMode::DefenderTeams, false, false},
        {"Nodes", ::MphRead::GameMode::Nodes, ::MphRead::GameMode::NodesTeams, false, false},
        {"Capture", ::MphRead::GameMode::Capture, ::MphRead::GameMode::Capture, true, false},
        {"Prime Hunter", ::MphRead::GameMode::PrimeHunter, ::MphRead::GameMode::PrimeHunter, false, true}
    }};

    const std::array<LobbyScreen::Matchup, 8> LobbyScreen::_matchups{{
        {"FFA", Network::MatchFormat::FreeForAll},
        {"Teams", Network::MatchFormat::Auto},
        {"1v1", Network::MatchFormat::OneVsOne},
        {"2v2", Network::MatchFormat::TwoVsTwo},
        {"3v3", Network::MatchFormat::ThreeVsThree},
        {"4v4", Network::MatchFormat::FourVsFour},
        {"2v2v2v2", Network::MatchFormat::TwoVsTwoVsTwoVsTwo},
        {"Custom", Network::MatchFormat::Custom}
    }};

    LobbyScreen::LobbyScreen(const std::vector<std::string>& rooms,
        std::shared_ptr<::MphRead::Mods::Launcher::LobbyContext> context)
        : _timer(std::make_shared<Threading::DispatcherTimer>(Threading::DispatcherPriority::Background)),
          _root(std::make_shared<Controls::Grid>()),
          _players(std::make_shared<Controls::StackPanel>()),
          _ownerControls(std::make_shared<Controls::StackPanel>()),
          _administration(std::make_shared<Controls::StackPanel>()),
          _status(std::make_shared<Note>("")),
          _chat(std::make_shared<Note>("", std::nullopt, 0)),
          _chatHistory(std::make_shared<Controls::ScrollViewer>()),
          _chatEntry(std::make_shared<Controls::TextBox>()),
          _hunter(std::make_shared<ChoiceRow>("Hunter", [&]
          {
              std::vector<std::string> options;
              options.reserve(::MphRead::Mods::Launcher::Hunters::Playable);
              for (std::int32_t i = 0; i < ::MphRead::Mods::Launcher::Hunters::Playable; ++i)
              {
                  options.push_back(::MphRead::ToString(static_cast<::MphRead::Hunter>(i)));
              }
              return options;
          }(), static_cast<std::int32_t>(Network::NetSession::LocalHunter()))),
          _suit(std::make_shared<ChoiceRow>("Suit", NumberOptions(1, 4), Network::NetSession::LocalColor())),
          _team(std::make_shared<ChoiceRow>("Team", std::vector<std::string>{"Auto", "Team A", "Team B"})),
          _mode(std::make_shared<ChoiceRow>("Game type", [&]
          {
              std::vector<std::string> options;
              options.reserve(_gameTypes.size());
              for (const GameType& type : _gameTypes)
              {
                  options.emplace_back(type.Label);
              }
              return options;
          }())),
          _format(std::make_shared<ChoiceRow>("Matchup", [&]
          {
              std::vector<std::string> options;
              options.reserve(_matchups.size());
              for (const Matchup& matchup : _matchups)
              {
                  options.emplace_back(matchup.Label);
              }
              return options;
          }())),
          _target(std::make_shared<ChoiceRow>("Manage player", std::vector<std::string>{})),
          _moveTeam(std::make_shared<ChoiceRow>("Move to team",
              std::vector<std::string>{"Auto", "Team A", "Team B"})),
          _map(std::make_shared<PickRow>("Map")),
          _customTeams(std::make_shared<PickRow>("Custom teams")),
          _fire(Toggle("Friendly fire")),
          _affinity(Toggle("Affinity weapons")),
          _freeze(Toggle("Shadow freeze")),
          _requireReady(Toggle("Require ready")),
          _join(Toggle("Join in progress")),
          _lockTeams(Toggle("Lock teams")),
          _opponentHealth(Toggle("Opponent health", true)),
          _layoutSummary(std::make_shared<Note>("")),
          _time(std::make_shared<FieldRow>("Time limit (minutes)", "7", 80)),
          _goal(std::make_shared<FieldRow>("Score goal", "7", 80)),
          _ready(std::make_shared<UiMark>(UiMark::Shape::Accept, "Ready")),
          _start(std::make_shared<UiMark>(UiMark::Shape::Accept, "Start match")),
          _moveButton(SmallButton("Move", Deck::Face::Blue(), [this]
          {
              Admin(Network::LobbyCommandType::SetTeam);
          })),
          _preview(std::make_shared<Controls::Image>()),
          _rooms(rooms)
    {
        Focusable(true);
        _timer->Interval(Threading::TimeSpan(1.0 / 30.0));

        _players->Spacing(2);
        _ownerControls->Spacing(2);
        _administration->Spacing(2);
        _preview->Height(104);
        _preview->Stretch(Media::Stretch::UniformToFill);

        _hunter->Changed += [this](ChoiceRow&) { Identify(); };
        _suit->Changed += [this](ChoiceRow&) { Identify(); };
        _team->Changed += [this](ChoiceRow&)
        {
            const std::int32_t slot = Network::NetSession::LocalSlot();
            if (!_syncing && slot >= 0)
            {
                Network::NetSession::SendLobbyCommand(Network::LobbyCommandType::SetTeam,
                    static_cast<std::uint8_t>(slot), static_cast<std::int8_t>(_team->Index() - 1));
            }
        };

        _map->Clicked += [this](PickRow&) { OpenMapPicker(); };
        _mode->Changed += [this](ChoiceRow&) { MatchChoiceChanged(true); };
        _format->Changed += [this](ChoiceRow&) { MatchChoiceChanged(false); };
        _customTeams->IsVisible(false);
        _customTeams->Clicked += [this](PickRow&) { OpenCustomTeams(); };
        _time->Box->TextChanged += [this](Controls::TextBox&) { DraftChanged(); };
        _goal->Box->TextChanged += [this](Controls::TextBox&) { DraftChanged(); };
        for (const std::shared_ptr<ButtonToggleRow>& toggle : {
            _fire, _affinity, _freeze, _opponentHealth, _requireReady, _join, _lockTeams})
        {
            toggle->Changed += [this](ButtonToggleRow&) { DraftChanged(); };
        }

        _ownerControls->Children.Add(_preview);
        _ownerControls->Children.Add(_map);
        _ownerControls->Children.Add(_mode);
        _ownerControls->Children.Add(_format);
        _ownerControls->Children.Add(_customTeams);

        auto limits = std::make_shared<Controls::Grid>();
        limits->ColumnDefinitions(Controls::ColumnDefinitions("*,*"));
        limits->ColumnSpacing(12);
        limits->Children.Add(_time);
        Controls::Grid::SetColumn(*_goal, 1);
        limits->Children.Add(_goal);
        _ownerControls->Children.Add(limits);

        auto toggles = std::make_shared<Controls::Grid>();
        toggles->ColumnDefinitions(Controls::ColumnDefinitions("*,*"));
        toggles->RowDefinitions(Controls::RowDefinitions("Auto,Auto,Auto,Auto"));
        toggles->ColumnSpacing(12);
        toggles->RowSpacing(2);
        const std::array<Av::Controls::ControlPtr, 7> toggleRows{
            _fire, _affinity, _freeze, _opponentHealth, _requireReady, _join, _lockTeams
        };
        for (std::size_t i = 0; i < toggleRows.size(); ++i)
        {
            Controls::Grid::SetColumn(*toggleRows[i], static_cast<std::int32_t>(i % 2));
            Controls::Grid::SetRow(*toggleRows[i], static_cast<std::int32_t>(i / 2));
            toggles->Children.Add(toggleRows[i]);
        }
        _ownerControls->Children.Add(toggles);
        _ownerControls->Children.Add(_layoutSummary);

        auto left = std::make_shared<Controls::StackPanel>();
        left->Spacing(2);
        left->Margin(Thickness(0, 0, 18, 0));
        left->Children.Add(std::make_shared<Caption>("Players"));
        left->Children.Add(_players);
        left->Children.Add(std::make_shared<Caption>("Your player"));
        left->Children.Add(_hunter);
        left->Children.Add(_suit);
        left->Children.Add(_team);

        _administration->Children.Add(std::make_shared<Caption>("Owner actions"));
        _administration->Children.Add(_target);
        _administration->Children.Add(_moveTeam);
        auto adminButtons = std::make_shared<Controls::StackPanel>();
        adminButtons->Orientation(Layout::Orientation::Horizontal);
        adminButtons->Spacing(7);
        adminButtons->HorizontalAlignment(Layout::HorizontalAlignment::Center);
        adminButtons->Children.Add(_moveButton);
        adminButtons->Children.Add(SmallButton("Transfer", Deck::Face::Brass(), [this]
        {
            Admin(Network::LobbyCommandType::TransferOwner);
        }));
        adminButtons->Children.Add(SmallButton("Kick", Deck::Face::Rust(), [this]
        {
            Admin(Network::LobbyCommandType::KickPlayer);
        }));
        _administration->Children.Add(adminButtons);
        left->Children.Add(_administration);

        auto columns = std::make_shared<Controls::Grid>();
        columns->ColumnDefinitions(Controls::ColumnDefinitions("0.82*,1.18*"));
        columns->ColumnSpacing(12);
        columns->Children.Add(left);
        Controls::Grid::SetColumn(*_ownerControls, 1);
        columns->Children.Add(_ownerControls);

        _chatHistory->Content(_chat);
        _chatHistory->Height(58);
        _chatHistory->MinHeight(58);
        _chatHistory->HorizontalScrollBarVisibility(Controls::ScrollBarVisibility::Disabled);
        _chatHistory->VerticalScrollBarVisibility(Controls::ScrollBarVisibility::Auto);
        _chatEntry->Watermark("Message");
        _chatEntry->MaxLength(Network::ChatPacket::MaxTextBytes);
        _chatEntry->Height(30);
        _chatEntry->MinHeight(30);
        _chatEntry->VerticalContentAlignment(Layout::VerticalAlignment::Center);

        auto chatPanel = std::make_shared<Controls::Grid>();
        chatPanel->RowDefinitions(Controls::RowDefinitions("Auto,58,32"));
        chatPanel->RowSpacing(3);
        chatPanel->Margin(Thickness(0, 4, 0, 2));
        auto chatLabel = std::make_shared<Controls::TextBlock>();
        chatLabel->Text("CHAT");
        chatLabel->FontFamily(GuiTheme::Display());
        chatLabel->FontSize(11);
        chatLabel->Foreground(GuiTheme::TextDimBrush);
        chatLabel->Margin(Thickness(0, 0, 0, 2));
        chatPanel->Children.Add(chatLabel);
        Controls::Grid::SetRow(*_chatHistory, 1);
        chatPanel->Children.Add(_chatHistory);

        auto chatInput = std::make_shared<Controls::Grid>();
        chatInput->ColumnDefinitions(Controls::ColumnDefinitions("*,Auto"));
        chatInput->ColumnSpacing(8);
        chatInput->Height(32);
        chatInput->Children.Add(_chatEntry);
        auto send = SmallButton("Send", Deck::Face::Blue(), [this] { SendChat(); });
        Controls::Grid::SetColumn(*send, 1);
        chatInput->Children.Add(send);
        Controls::Grid::SetRow(*chatInput, 2);
        chatPanel->Children.Add(chatInput);
        _chatEntry->KeyDown += [this](Av::Input::InputElement&, Av::Input::KeyEventArgs& e)
        {
            if (e.Key == Av::Input::Key::Enter)
            {
                SendChat();
                e.Handled = true;
            }
        };

        auto footer = std::make_shared<Controls::StackPanel>();
        footer->Orientation(Layout::Orientation::Horizontal);
        footer->Spacing(16);
        footer->HorizontalAlignment(Layout::HorizontalAlignment::Center);
        footer->Children.Add(ActionButton("Leave", [this] { Leave(""); }));
        _ready->Click += [this](UiMark&)
        {
            const std::int32_t slot = Network::NetSession::LocalSlot();
            if (slot >= 0)
            {
                Network::NetSession::SendLobbyCommand(Network::LobbyCommandType::SetReady,
                    255, -1, !Network::NetSession::SlotLobbyReady[static_cast<std::size_t>(slot)]);
            }
        };
        _start->Click += [](UiMark&)
        {
            Network::NetSession::SendLobbyCommand(Network::LobbyCommandType::StartMatch);
        };
        footer->Children.Add(_ready);
        footer->Children.Add(_start);

        auto body = std::make_shared<Controls::Grid>();
        body->RowDefinitions(Controls::RowDefinitions("*,Auto,Auto,Auto"));
        body->RowSpacing(3);
        body->Children.Add(columns);
        Controls::Grid::SetRow(*chatPanel, 1);
        body->Children.Add(chatPanel);
        Controls::Grid::SetRow(*_status, 2);
        body->Children.Add(_status);
        Controls::Grid::SetRow(*footer, 3);
        body->Children.Add(footer);

        auto frame = std::make_shared<Controls::Grid>();
        frame->MaxWidth(1100);
        frame->Margin(Thickness(UiLayout::WellGutter));
        frame->RowDefinitions(Controls::RowDefinitions("Auto,*"));
        std::string title = context != nullptr && !context->ServerName.empty()
            ? Runtime::ToUpperInvariant(context->ServerName + " LOBBY") : "LOBBY";
        auto heading = std::make_shared<Note>(title);
        heading->FontSize(UiLayout::HeadingSize);
        heading->HorizontalAlignment(Layout::HorizontalAlignment::Center);
        heading->Margin(Thickness(0, 0, 0, 8));
        frame->Children.Add(heading);
        Controls::Grid::SetRow(*body, 1);
        frame->Children.Add(body);

        auto backdrop = UiLayout::Backdrop(false, UiLayout::BackdropWash::Standard);
        backdrop->Children.Add(frame);
        _mainPage = backdrop;
        _root->Children.Add(_mainPage);
        Content(_root);

        _shownMatch.reset();
        Refresh();
    }

    LobbyScreen::~LobbyScreen()
    {
        if (_timer != nullptr)
        {
            _timer->Stop();
        }
        if (_preview != nullptr)
        {
            _preview->Source(nullptr);
        }
        _bitmap.reset();
    }

    void LobbyScreen::OnAttachedToVisualTree()
    {
        Av::Controls::UserControl::OnAttachedToVisualTree();
        if (!_timerConnected)
        {
            const std::shared_ptr<LobbyScreen> self
                = std::dynamic_pointer_cast<LobbyScreen>(shared_from_this());
            const std::weak_ptr<LobbyScreen> weak = self;
            _timer->Tick += [weak](Threading::DispatcherTimer&)
            {
                const std::shared_ptr<LobbyScreen> keepAlive = weak.lock();
                if (keepAlive == nullptr)
                {
                    return;
                }
                keepAlive->Tick();
                keepAlive->_administration->IsVisible(Network::NetSession::LocalIsLobbyOwner());
                // A Close event can remove the last UI-owned reference while
                // this timer event is still unwinding. Keep the screen alive
                // until the dispatcher's timer pass has returned.
                Threading::Dispatcher::UIThread().Post([keepAlive] { (void)keepAlive; },
                    Threading::DispatcherPriority::Background);
            };
            _timerConnected = true;
        }
        if (!_suspended && !_closed)
        {
            _timer->Start();
        }
    }

    void LobbyScreen::OnDetachedFromVisualTree()
    {
        _timer->Stop();
        Av::Controls::UserControl::OnDetachedFromVisualTree();
    }

    void LobbyScreen::Resume()
    {
        _suspended = false;
        _shownRevision.reset();
        _timer->Start();
    }

    void LobbyScreen::Suspend()
    {
        _suspended = true;
        _timer->Stop();
    }

    void LobbyScreen::Leave(std::string reason)
    {
        if (_closed)
        {
            return;
        }
        _closed = true;
        _timer->Stop();
        if (_preview != nullptr)
        {
            _preview->Source(nullptr);
        }
        _bitmap.reset();
        Network::NetSession::Stop();
        Network::NetHostSession::Stop();
        Closed(*this, std::move(reason));
    }

    void LobbyScreen::Tick()
    {
        if (_suspended || _closed)
        {
            return;
        }
        Network::NetSession::Pump();
        if (Network::NetSession::Refused() || Network::NetSession::SessionTimedOut()
            || !Network::NetSession::Active())
        {
            const std::string reason = Network::NetSession::Refused()
                ? Network::NetSession::RefusedReason().Describe(std::optional<std::string>("Server"))
                : "The connection to the server was lost.";
            Leave(reason);
            return;
        }
        Refresh();
        TryAutoApply();
        if (Network::NetSession::ShouldLoadMatch())
        {
            Suspend();
            const Network::MatchDefinition match = Network::NetSession::ActiveMatchDefinition().value();
            ::MphRead::Mods::Launcher::LaunchPlan::Init init;
            init.Kind = ::MphRead::Mods::Launcher::LaunchKind::Online;
            init.Hunter = Network::NetSession::LocalHunter();
            init.PlayerName = Network::NetSession::PlayerName();
            init.RoomKey = match.RoomKey;
            init.Mode = match.Mode;
            MatchRequested(*this, ::MphRead::Mods::Launcher::LaunchPlan(init));
        }
    }

    void LobbyScreen::Refresh()
    {
        const std::optional<Network::SessionStatePacket>& sessionValue = Network::NetSession::ServerSession();
        if (!sessionValue.has_value())
        {
            return;
        }
        const Network::SessionStatePacket session = *sessionValue;
        _syncing = true;
        _hunter->Index(static_cast<std::int32_t>(Network::NetSession::LocalHunter()));
        _suit->Index(Network::NetSession::LocalColor());
        const Network::RosterPacket roster = Network::NetSession::LobbyRoster();
        _rosterCount = roster.Count;

        if (_shownRevision != session.Revision || _shownRosterRevision != roster.Revision
            || Network::NetSession::Clock() >= _nextPingRefresh)
        {
            _shownRevision = session.Revision;
            _shownRosterRevision = roster.Revision;
            _nextPingRefresh = Network::NetSession::Clock() + 1;
            std::uint8_t selected = std::numeric_limits<std::uint8_t>::max();
            if (_target->Index() >= 0 && static_cast<std::size_t>(_target->Index()) < _targetSlots.size())
            {
                selected = _targetSlots[static_cast<std::size_t>(_target->Index())];
            }
            _players->Children.Clear();
            _targetSlots.clear();
            std::vector<std::string> names;
            names.reserve(roster.Count);
            for (std::int32_t i = 0; i < roster.Count; ++i)
            {
                _players->Children.Add(std::make_shared<LobbyPlayerRow>(roster, i, session.OwnerSlot,
                    session.Match.Format != Network::MatchFormat::OneVsOne));
                const std::uint8_t slot = Runtime::ManagedAt(roster.Slots, i);
                if (slot != Network::NetSession::LocalSlot())
                {
                    _targetSlots.push_back(slot);
                    names.push_back(Runtime::ManagedAt(roster.Names, i).value_or(std::string{}));
                }
            }
            const auto selectedIt = std::find(_targetSlots.begin(), _targetSlots.end(), selected);
            const std::int32_t selectedIndex = selectedIt == _targetSlots.end()
                ? 0 : static_cast<std::int32_t>(std::distance(_targetSlots.begin(), selectedIt));
            _target->SetItems(std::move(names), selectedIndex);
        }

        if (!_shownMatch.has_value() || *_shownMatch != session.Match || _shownRules != session.RuleFlags)
        {
            _shownMatch = session.Match;
            _shownRules = session.RuleFlags;
            _draftRoom = session.Match.RoomKey.value_or(std::string{});
            _map->Set(RoomName(_draftRoom));
            SetPreview(_draftRoom);
            _mode->Index(BaseModeIndex(session.Match.Mode));
            _format->Index(MatchupIndex(session.Match));
            _time->Value(Minutes(session.Match.TimeLimitSeconds));
            _goal->Label(GoalLabel(session.Match.Mode));
            _goal->Value(GoalDisplay(session.Match.Mode, session.Match.PointGoal));
            _fire->On(session.Match.FriendlyFire);
            _affinity->On(session.Match.AffinityWeapons);
            _freeze->On(session.Match.ShadowFreeze);
            _opponentHealth->On(!session.Match.HideOpponentHealth);
            _requireReady->On(session.RequireReady());
            _join->On(session.AllowJoinInProgress());
            _lockTeams->On(PlayerChoosesTeam(session.Match) && session.LockTeams());

            const TeamLayout layout = Network::LobbyRules::ResolveTeamLayout(session.Match);
            _customLayout = session.Match.CustomTeams.IsValid()
                ? session.Match.CustomTeams
                : layout.IsValid() ? layout : TeamLayout(2, 2, 2);
            _customTeams->Set(_customLayout.ToString());

            std::vector<std::string> teams{"Auto"};
            for (std::int32_t team = 0; team < layout.TeamCount; ++team)
            {
                teams.push_back(std::string("Team ") + static_cast<char>('A' + team));
            }
            _team->SetItems(teams, 0);
            _moveTeam->SetItems(std::move(teams), 0);
        }

        _ownerControls->IsEnabled(Network::NetSession::CanEditLobby()
            && !Network::NetSession::LobbyCommandPending());
        const bool chooseTeams = PlayerChoosesTeam(session.Match);
        _team->IsVisible(chooseTeams);
        const std::int32_t localSlot = Network::NetSession::LocalSlot();
        if (localSlot >= 0)
        {
            _team->Index(Network::NetSession::SlotTeamIndex[static_cast<std::size_t>(localSlot)] + 1);
        }
        const bool playerControlsEnabled = Network::NetSession::IsInLobby()
            && !Network::NetSession::LobbyCommandPending();
        _hunter->IsEnabled(playerControlsEnabled);
        _suit->IsEnabled(playerControlsEnabled);
        _team->IsEnabled(chooseTeams && playerControlsEnabled
            && (!session.LockTeams() || Network::NetSession::LocalIsLobbyOwner()));
        _moveTeam->IsVisible(chooseTeams);
        _moveButton->IsVisible(chooseTeams);
        _ready->IsEnabled(playerControlsEnabled);
        _ready->Label(localSlot >= 0
            && Network::NetSession::SlotLobbyReady[static_cast<std::size_t>(localSlot)] ? "Unready" : "Ready");

        std::string reason;
        const Network::LobbyResultCode valid = Network::LobbyRules::Validate(
            session.Match, roster, session.RequireReady(), reason);
        _start->IsVisible(Network::NetSession::LocalIsLobbyOwner());
        _start->IsEnabled(Network::NetSession::CanEditLobby()
            && valid == Network::LobbyResultCode::Ok
            && !Network::NetSession::LobbyCommandPending());
        _status->Text(Network::NetSession::ConnectionLost()
            ? "Connection lost, retrying..."
            : !Network::NetSession::LobbyMessage().empty()
                ? Network::NetSession::LobbyMessage()
                : session.Phase == Network::SessionPhase::Lobby
                    ? reason
                    : "Waiting for players to finish loading...");

        const std::int32_t chatRevision = ::MphRead::Mods::Chat::NetChat::Revision();
        if (_chatRevision != chatRevision)
        {
            _chatRevision = chatRevision;
            const std::vector<std::string>& history = ::MphRead::Mods::Chat::NetChat::History();
            const std::size_t first = history.size() > 12 ? history.size() - 12 : 0;
            std::string lines;
            for (std::size_t i = first; i < history.size(); ++i)
            {
                if (i > first)
                {
                    lines += "\n";
                }
                lines += history[i];
            }
            _chat->Text(std::move(lines));
            const std::shared_ptr<Controls::ScrollViewer> chatHistory = _chatHistory;
            Threading::Dispatcher::UIThread().Post([chatHistory]
            {
                chatHistory->ScrollToEnd();
            }, Threading::DispatcherPriority::Loaded);
        }

        _syncing = false;
        RefreshDraft();
    }

    void LobbyScreen::Identify()
    {
        if (_syncing || !Network::NetSession::IsInLobby())
        {
            return;
        }
        Network::NetSession::SetLocalHunter(static_cast<::MphRead::Hunter>(_hunter->Index()));
        Network::NetSession::SetLocalColor(_suit->Index());
        ::MphRead::Mods::Launcher::LauncherPrefs::LastHunter(Network::NetSession::LocalHunter());
        ::MphRead::Mods::Launcher::LauncherPrefs::LastColor(Network::NetSession::LocalColor());
        ::MphRead::Mods::Launcher::LauncherPrefs::Save();
        Network::NetSession::SendIdentify();
    }

    void LobbyScreen::SendChat()
    {
        ::MphRead::Mods::Chat::NetChat::Send(_chatEntry->Text());
        _chatEntry->Text("");
    }

    void LobbyScreen::Admin(Network::LobbyCommandType type)
    {
        const std::int32_t index = _target->Index();
        if (index >= 0 && static_cast<std::size_t>(index) < _targetSlots.size())
        {
            Network::NetSession::SendLobbyCommand(type, _targetSlots[static_cast<std::size_t>(index)],
                static_cast<std::int8_t>(_moveTeam->Index() - 1));
        }
    }

    void LobbyScreen::MatchChoiceChanged(bool resetGoal)
    {
        if (_syncing)
        {
            return;
        }
        _syncing = true;
        const std::int32_t modeIndex = std::clamp(_mode->Index(), 0,
            static_cast<std::int32_t>(_gameTypes.size()) - 1);
        const GameType& type = _gameTypes[static_cast<std::size_t>(modeIndex)];
        const Network::MatchFormat format = SelectedFormat();
        std::int32_t target = _format->Index();
        if (type.FfaOnly && format != Network::MatchFormat::FreeForAll)
        {
            target = MatchupIndex(Network::MatchFormat::FreeForAll);
        }
        else if (type.TeamOnly && (format == Network::MatchFormat::FreeForAll
            || format == Network::MatchFormat::TwoVsTwoVsTwoVsTwo))
        {
            target = MatchupIndex(Network::MatchFormat::Auto);
        }
        if (target != _format->Index())
        {
            _format->Index(target);
        }
        const Network::MatchDefinition draft = DraftMatch();
        _goal->Label(GoalLabel(draft.Mode));
        if (resetGoal)
        {
            _goal->Value(GoalDisplay(draft.Mode, Network::MatchGoalRules::DefaultValue(draft.Mode)));
        }
        _syncing = false;
        DraftChanged();
    }

    Network::MatchDefinition LobbyScreen::DraftMatch() const
    {
        const Network::MatchFormat format = SelectedFormat();
        const std::int32_t modeIndex = std::clamp(_mode->Index(), 0,
            static_cast<std::int32_t>(_gameTypes.size()) - 1);
        const GameType& type = _gameTypes[static_cast<std::size_t>(modeIndex)];
        const bool teams = format != Network::MatchFormat::FreeForAll;
        Network::MatchDefinition match;
        match.RoomKey = _draftRoom;
        match.Mode = type.FfaOnly ? type.Free
            : type.TeamOnly ? type.Team
            : teams ? type.Team : type.Free;
        match.Format = format;
        match.CustomTeams = _customLayout;
        return match;
    }

    void LobbyScreen::DraftChanged()
    {
        if (_syncing)
        {
            return;
        }
        _draftDirty = true;
        _draftChangedAt = Network::NetSession::Clock();
        RefreshDraft();
    }

    void LobbyScreen::RefreshDraft()
    {
        if (_syncing)
        {
            return;
        }
        const Network::MatchDefinition draft = DraftMatch();
        _goal->Label(GoalLabel(draft.Mode));
        _customTeams->IsVisible(draft.Format == Network::MatchFormat::Custom);
        const bool chooseTeams = PlayerChoosesTeam(draft);
        _lockTeams->IsVisible(chooseTeams);
        _layoutSummary->IsVisible(draft.Format != Network::MatchFormat::OneVsOne);

        Network::MatchDefinition configured;
        std::string reason;
        const bool valid = TryBuildMatch(configured, reason);
        const TeamLayout layout = Network::LobbyRules::ResolveTeamLayout(configured);
        if (!valid)
        {
            _layoutSummary->Text(reason);
        }
        else if (_draftDirty)
        {
            _layoutSummary->Text("Changes save automatically.");
        }
        else if (layout.TeamCount == 0)
        {
            _layoutSummary->Text("Free for all");
        }
        else
        {
            const std::string rosterSummary = Network::LobbyRules::ExactTeams(configured)
                ? std::to_string(layout.TotalPlayers()) + " players" : std::string("flexible roster");
            _layoutSummary->Text("Teams: " + layout.ToString() + " \xC2\xB7 " + rosterSummary);
        }
        _customTeams->Set(_customLayout.ToString());
        if (!valid)
        {
            _start->IsEnabled(false);
        }
    }

    bool LobbyScreen::TryBuildMatch(Network::MatchDefinition& match, std::string& reason) const
    {
        match = DraftMatch();
        if (Network::LobbyRules::ValidateDefinition(match, reason) != Network::LobbyResultCode::Ok)
        {
            return false;
        }
        const TeamLayout layout = Network::LobbyRules::ResolveTeamLayout(match);
        const std::optional<Network::SessionStatePacket>& session = Network::NetSession::ServerSession();
        if (layout.TeamCount > 0 && (layout.TotalPlayers() < _rosterCount
            || (Network::LobbyRules::ExactTeams(match)
                && layout.TotalPlayers() > (session.has_value() ? session->MaxPlayers : 8))))
        {
            reason = "The matchup must fit the connected players and server limit.";
            return false;
        }
        std::uint16_t seconds = 0;
        if (!TryTimeSeconds(seconds))
        {
            reason = "Match time must be minutes from 0 to 1092.25.";
            return false;
        }
        std::uint16_t goal = 0;
        if (!TryGoalValue(match.Mode, goal, reason))
        {
            return false;
        }
        match.TimeLimitSeconds = seconds;
        match.PointGoal = goal;
        match.FriendlyFire = _fire->On();
        match.AffinityWeapons = _affinity->On();
        match.ShadowFreeze = _freeze->On();
        match.HideOpponentHealth = !_opponentHealth->On();
        return true;
    }

    void LobbyScreen::TryAutoApply()
    {
        if (!_draftDirty || Network::NetSession::LobbyCommandPending() || !Network::NetSession::CanEditLobby()
            || Network::NetSession::Clock() - _draftChangedAt < 0.25)
        {
            return;
        }
        const std::optional<Network::SessionStatePacket>& current = Network::NetSession::ServerSession();
        if (!current.has_value())
        {
            return;
        }
        Network::MatchDefinition match;
        std::string reason;
        if (!TryBuildMatch(match, reason))
        {
            return;
        }
        Network::SessionStatePacket config = *current;
        config.Match = match;
        config.RuleFlags = match.Rules();
        if (_requireReady->On())
        {
            config.RuleFlags |= Network::SessionRules::RequireReady;
        }
        if (_join->On())
        {
            config.RuleFlags |= Network::SessionRules::AllowJoinInProgress;
        }
        if (PlayerChoosesTeam(match) && _lockTeams->On())
        {
            config.RuleFlags |= Network::SessionRules::LockTeams;
        }
        if (Network::NetSession::SendLobbyCommand(Network::LobbyCommandType::UpdateMatch,
            255, -1, false, config))
        {
            _draftDirty = false;
            _layoutSummary->Text("Saving changes...");
        }
    }

    void LobbyScreen::OpenMapPicker()
    {
        if (!Network::NetSession::CanEditLobby() || Network::NetSession::LobbyCommandPending())
        {
            return;
        }
        auto picker = std::make_shared<MapCardPicker>(_rooms, std::optional<std::string>(_draftRoom));
        picker->Done += [this](MapCardPicker&, std::string room)
        {
            _draftRoom = std::move(room);
            _map->Set(RoomName(_draftRoom));
            SetPreview(_draftRoom);
            DraftChanged();
            ClosePage();
        };
        picker->Cancelled += [this](MapCardPicker&) { ClosePage(); };
        OpenPage(picker);
    }

    void LobbyScreen::OpenCustomTeams()
    {
        if (!Network::NetSession::CanEditLobby() || Network::NetSession::LobbyCommandPending())
        {
            return;
        }
        const std::optional<Network::SessionStatePacket>& session = Network::NetSession::ServerSession();
        auto picker = std::make_shared<CustomTeamPicker>(_customLayout,
            session.has_value() ? session->MaxPlayers : 8);
        picker->Done += [this](CustomTeamPicker&, TeamLayout layout)
        {
            _customLayout = layout;
            _customTeams->Set(_customLayout.ToString());
            DraftChanged();
            ClosePage();
        };
        picker->Cancelled += [this](CustomTeamPicker&) { ClosePage(); };
        OpenPage(picker);
    }

    void LobbyScreen::OpenPage(const Av::Controls::ControlPtr& page)
    {
        _root->Children.Clear();
        _root->Children.Add(page);
        Threading::Dispatcher::UIThread().Post([page]
        {
            page->Focus();
        }, Threading::DispatcherPriority::Background);
    }

    void LobbyScreen::ClosePage()
    {
        _root->Children.Clear();
        _root->Children.Add(_mainPage);
        const std::shared_ptr<PickRow> map = _map;
        Threading::Dispatcher::UIThread().Post([map]
        {
            map->Focus();
        }, Threading::DispatcherPriority::Background);
    }

    void LobbyScreen::SetPreview(const std::string& room)
    {
        _preview->Source(nullptr);
        _bitmap.reset();
        try
        {
            const std::string path = ::MphRead::Mods::ThumbnailGenerator::PathFor(room);
            if (!Runtime::StringIsNullOrWhiteSpace(room) && Runtime::FileExists(path))
            {
                _bitmap = Media::Imaging::Bitmap::FromFile(path);
            }
        }
        catch (const std::exception&)
        {
            // A thumbnail is presentation only; map validation belongs to the server.
        }
        _preview->Source(_bitmap);
        _preview->IsVisible(_bitmap != nullptr);
    }

    std::string LobbyScreen::RoomName(const std::string& room)
    {
        const auto [metadata, roomId] = ::MphRead::Metadata::GetRoomByName(room);
        (void)roomId;
        return metadata != nullptr ? metadata->InGameName.value_or(room) : room;
    }

    std::string LobbyScreen::Minutes(std::uint16_t seconds)
    {
        const double minutes = seconds / 60.0;
        const char* format = minutes == std::trunc(minutes) ? "0" : "0.##";
        return Runtime::ToStringInvariant(minutes, format);
    }

    bool LobbyScreen::TryTimeSeconds(std::uint16_t& seconds) const
    {
        seconds = 0;
        double minutes = 0;
        if (!Runtime::DoubleTryParseInvariant(_time->Value(), minutes)
            || !std::isfinite(minutes) || minutes < 0
            || minutes * 60 > std::numeric_limits<std::uint16_t>::max())
        {
            return false;
        }
        seconds = static_cast<std::uint16_t>(std::round(minutes * 60));
        return true;
    }

    bool LobbyScreen::PlayerChoosesTeam(const Network::MatchDefinition& match)
    {
        return ::MphRead::GameState::IsTeamMode(match.Mode)
            && match.Format != Network::MatchFormat::OneVsOne;
    }

    std::string LobbyScreen::GoalLabel(::MphRead::GameMode mode)
    {
        switch (mode)
        {
        case ::MphRead::GameMode::Survival:
        case ::MphRead::GameMode::SurvivalTeams:
            return "Lives";
        case ::MphRead::GameMode::Bounty:
        case ::MphRead::GameMode::BountyTeams:
            return "Bounty goal";
        case ::MphRead::GameMode::Capture:
            return "Captures";
        case ::MphRead::GameMode::Defender:
        case ::MphRead::GameMode::DefenderTeams:
            return "Hold time (minutes)";
        case ::MphRead::GameMode::Nodes:
        case ::MphRead::GameMode::NodesTeams:
            return "Node score";
        case ::MphRead::GameMode::PrimeHunter:
            return "Prime time (minutes)";
        default:
            return "Score goal";
        }
    }

    std::string LobbyScreen::GoalDisplay(::MphRead::GameMode mode, std::uint16_t value)
    {
        if (Network::MatchGoalRules::UsesLives(mode))
        {
            return Runtime::ToStringInvariant(static_cast<std::uint32_t>(value) + 1);
        }
        if (Network::MatchGoalRules::UsesTimeTarget(mode))
        {
            return Minutes(value);
        }
        return Runtime::ToStringInvariant(value);
    }

    bool LobbyScreen::TryGoalValue(::MphRead::GameMode mode,
        std::uint16_t& value, std::string& reason) const
    {
        value = 0;
        reason.clear();
        const Runtime::NumberFormatInfo& invariant = Runtime::NumberFormatInfo::InvariantInfo();
        if (Network::MatchGoalRules::UsesLives(mode))
        {
            std::int32_t lives = 0;
            if (!Runtime::TryParseInteger(_goal->Value(), Runtime::NumberStyles::None,
                    invariant, lives)
                || lives < 1 || lives > static_cast<std::int32_t>(std::numeric_limits<std::uint16_t>::max()) + 1)
            {
                reason = "Lives must be a whole number from 1 to 65536.";
                return false;
            }
            value = static_cast<std::uint16_t>(lives - 1);
            return true;
        }
        if (Network::MatchGoalRules::UsesTimeTarget(mode))
        {
            double minutes = 0;
            if (!Runtime::DoubleTryParseInvariant(_goal->Value(), minutes)
                || !std::isfinite(minutes) || minutes <= 0
                || minutes * 60 > std::numeric_limits<std::uint16_t>::max())
            {
                reason = GoalLabel(mode) + " must be greater than 0 and at most 1092.25.";
                return false;
            }
            const double rounded = std::round(minutes * 60);
            value = static_cast<std::uint16_t>(std::max(1.0, rounded));
            return true;
        }
        if (!Runtime::TryParseInteger(_goal->Value(), Runtime::NumberStyles::None,
                invariant, value))
        {
            reason = GoalLabel(mode) + " must be a whole number from 0 to 65535.";
            return false;
        }
        return true;
    }

    Network::MatchFormat LobbyScreen::SelectedFormat() const
    {
        const std::int32_t index = std::clamp(_format->Index(), 0,
            static_cast<std::int32_t>(_matchups.size()) - 1);
        return _matchups[static_cast<std::size_t>(index)].Format;
    }

    std::int32_t LobbyScreen::MatchupIndex(Network::MatchFormat format)
    {
        const auto found = std::find_if(_matchups.begin(), _matchups.end(),
            [format](const Matchup& matchup) { return matchup.Format == format; });
        return found == _matchups.end() ? 0 : static_cast<std::int32_t>(std::distance(_matchups.begin(), found));
    }

    std::int32_t LobbyScreen::MatchupIndex(const Network::MatchDefinition& match)
    {
        if (match.Format == Network::MatchFormat::Auto && !::MphRead::GameState::IsTeamMode(match.Mode))
        {
            return MatchupIndex(Network::MatchFormat::FreeForAll);
        }
        return MatchupIndex(match.Format);
    }

    std::int32_t LobbyScreen::BaseModeIndex(::MphRead::GameMode mode)
    {
        const auto found = std::find_if(_gameTypes.begin(), _gameTypes.end(),
            [mode](const GameType& type) { return type.Free == mode || type.Team == mode; });
        return found == _gameTypes.end() ? 0 : static_cast<std::int32_t>(std::distance(_gameTypes.begin(), found));
    }

    CustomTeamPicker::CustomTeamPicker(TeamLayout current, std::int32_t maxPlayers)
        : _note(std::make_shared<Note>("")),
          _use(std::make_shared<UiMark>(UiMark::Shape::Accept, "use teams")),
          _maxPlayers(std::clamp(maxPlayers, 2, 8))
    {
        const std::int32_t count = current.IsValid() ? current.TeamCount : 2;
        _count = std::make_shared<ChoiceRow>("Teams", std::vector<std::string>{"2", "3", "4"}, count - 2);
        auto form = std::make_shared<Controls::StackPanel>();
        form->Spacing(2);
        form->Children.Add(_count);
        for (std::int32_t team = 0; team < 4; ++team)
        {
            const std::int32_t initial = current.IsValid() && team < current.TeamCount
                ? std::max(1, static_cast<std::int32_t>(current.Capacity(team))) - 1 : 1;
            _sizes[static_cast<std::size_t>(team)] = std::make_shared<ChoiceRow>(
                std::string("Team ") + static_cast<char>('A' + team) + " size", NumberOptions(1, 8), initial);
            _sizes[static_cast<std::size_t>(team)]->Changed += [this](ChoiceRow&) { Refresh(); };
            form->Children.Add(_sizes[static_cast<std::size_t>(team)]);
        }
        form->Children.Add(_note);
        _count->Changed += [this](ChoiceRow&) { Refresh(); };

        auto back = std::make_shared<UiMark>(UiMark::Shape::Cancel, "back");
        back->Click += [this](UiMark&) { Cancelled(*this); };
        _use->Click += [this](UiMark&)
        {
            const TeamLayout layout = Value();
            if (layout.IsValid() && layout.TotalPlayers() <= _maxPlayers)
            {
                Done(*this, layout);
            }
        };
        Content(UiLayout::Page(false, UiLayout::WellSettings, "custom teams",
            nullptr, form, back, _use));
        Refresh();
    }

    TeamLayout CustomTeamPicker::Value() const
    {
        const std::int32_t count = _count->Index() + 2;
        return TeamLayout(static_cast<std::uint8_t>(count),
            static_cast<std::uint8_t>(_sizes[0]->Index() + 1),
            static_cast<std::uint8_t>(_sizes[1]->Index() + 1),
            count > 2 ? static_cast<std::uint8_t>(_sizes[2]->Index() + 1) : 0,
            count > 3 ? static_cast<std::uint8_t>(_sizes[3]->Index() + 1) : 0);
    }

    void CustomTeamPicker::Refresh()
    {
        const std::int32_t count = _count->Index() + 2;
        for (std::int32_t team = 0; team < 4; ++team)
        {
            _sizes[static_cast<std::size_t>(team)]->IsVisible(team < count);
        }
        const TeamLayout layout = Value();
        const bool valid = layout.IsValid() && layout.TotalPlayers() <= _maxPlayers;
        const std::string message = valid
            ? layout.ToString() + " \xC2\xB7 " + std::to_string(layout.TotalPlayers()) + " player slots"
            : "Custom teams must use no more than " + std::to_string(_maxPlayers) + " player slots.";
        _note->Text(message);
        _note->Foreground(valid ? GuiTheme::TextDimBrush : GuiTheme::WarmBrush);
        _use->IsEnabled(valid);
    }
}
