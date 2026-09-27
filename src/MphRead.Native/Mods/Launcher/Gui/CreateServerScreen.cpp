#include "CreateServerScreen.hpp"

#include "GuiTheme.hpp"
#include "ProgressRow.hpp"
#include "Rows.hpp"
#include "UiLayout.hpp"
#include "UiList.hpp"
#include "UiMark.hpp"
#include "../../../Entities/Players/PlayerEntity.hpp"
#include "../../../Metadata/Metadata.hpp"
#include "../../Branding.hpp"
#include "../../DebugLog.hpp"
#include "../../Network/LocalServer.hpp"
#include "../../Network/MatchDefinition.hpp"
#include "../../Network/NetHostSession.hpp"
#include "../../Network/NetLaunch.hpp"
#include "../../Network/NetProtocol.hpp"
#include "../../Network/NetSession.hpp"
#include "../../Launcher/Portable/LauncherPrefs.hpp"
#include "../../Update/UpdateCheck.hpp"
#include "../../../NativeRuntime/Avalonia/Threading.hpp"
#include "../../../NativeRuntime/System/Globalization.hpp"
#include "../../../NativeRuntime/System/Runtime.hpp"
#include "../../../NativeRuntime/System/Sort.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <future>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace MphRead::Mods::Launcher::Gui
{
    using namespace ::MphRead::NativeRuntime::Avalonia;
    using ::MphRead::NativeRuntime::ManagedSort;
    using ::MphRead::NativeRuntime::StringTrim;

    namespace
    {
        struct ModeOption final
        {
            const char* Label;
            MphRead::GameMode Mode;
        };

        const std::array<ModeOption, 12>& Modes()
        {
            static const std::array<ModeOption, 12> values = {{
                {"Battle", MphRead::GameMode::Battle},
                {"Battle teams", MphRead::GameMode::BattleTeams},
                {"Survival", MphRead::GameMode::Survival},
                {"Survival teams", MphRead::GameMode::SurvivalTeams},
                {"Capture", MphRead::GameMode::Capture},
                {"Bounty", MphRead::GameMode::Bounty},
                {"Bounty teams", MphRead::GameMode::BountyTeams},
                {"Defender", MphRead::GameMode::Defender},
                {"Defender teams", MphRead::GameMode::DefenderTeams},
                {"Nodes", MphRead::GameMode::Nodes},
                {"Nodes teams", MphRead::GameMode::NodesTeams},
                {"Prime hunter", MphRead::GameMode::PrimeHunter},
            }};
            return values;
        }

        std::vector<std::string> ModeLabels()
        {
            std::vector<std::string> result;
            result.reserve(Modes().size());
            for (const ModeOption& mode : Modes())
            {
                result.emplace_back(mode.Label);
            }
            return result;
        }

        std::vector<std::string> HunterNames()
        {
            std::vector<std::string> result;
            result.reserve(static_cast<std::size_t>(Launcher::Hunters::Playable + 1));
            for (std::int32_t i = 0; i < Launcher::Hunters::Playable; i++)
            {
                result.push_back(MphRead::ToString(static_cast<MphRead::Hunter>(i)));
            }
            result.push_back(MphRead::ToString(MphRead::Hunter::Random));
            return result;
        }

        std::int32_t HunterIndex(const std::vector<std::string>& names, MphRead::Hunter hunter)
        {
            const std::string wanted = MphRead::ToString(hunter);
            const auto found = std::find(names.begin(), names.end(), wanted);
            return found == names.end() ? 0 : static_cast<std::int32_t>(found - names.begin());
        }

        template <typename Result, typename Continuation>
        void Await(std::shared_future<Result> task, Continuation continuation)
        {
            if (task.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
            {
                continuation(task.get());
                return;
            }

            Threading::DispatcherTimer::Run(
                [task = std::move(task), continuation = std::move(continuation)]() mutable
                {
                    if (task.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
                    {
                        return true;
                    }
                    continuation(task.get());
                    return false;
                },
                Threading::TimeSpan(0.016), Threading::DispatcherPriority::Normal);
        }
    }

    CreateServerScreen::CreateServerScreen(const std::vector<std::string>& rooms,
        std::optional<std::string> firstMap)
        : _rooms(rooms),
          _root(std::make_shared<Controls::Panel>()),
          _form(std::make_shared<Controls::StackPanel>()),
          _note(std::make_shared<Note>("")),
          _progress(std::make_shared<ProgressRow>())
    {
        Background(Media::Brushes::Transparent());
        Focusable(true);
        _form->Spacing(2);

        const std::string player = StringTrim(LauncherPrefs::PlayerName());
        _name = std::make_shared<FieldRow>("Lobby name",
            (player.empty() ? "Player" : player) + "'s lobby", 230);
        _mode = std::make_shared<ChoiceRow>("Game type", ModeLabels());

        const std::vector<std::string> hunters = HunterNames();
        _hunter = std::make_shared<ChoiceRow>("Your hunter", hunters,
            HunterIndex(hunters, LauncherPrefs::LastHunter()));

        _host = std::make_shared<PickRow>("Host on");
        _host->Set("asking...");
        _host->Clicked += [this](PickRow&) { OpenHosts(); };
        _maps = std::make_shared<PickRow>("Map rotation");
        _maps->Clicked += [this](PickRow&) { OpenMaps(); };

        _kind = std::make_shared<ChoiceRow>("Hosting", std::vector<std::string>{"Hosted lobby", "Dedicated server"}, 0);
        _kind->Changed += [this](ChoiceRow&) { Refresh(); };

        _form->Children.Add(_name);
        _form->Children.Add(_mode);
        _form->Children.Add(_hunter);
        _form->Children.Add(_maps);
        _form->Children.Add(_host);
        if (CanRunHere())
        {
            _form->Children.Add(_kind);
        }
        _form->Children.Add(_progress);
        _form->Children.Add(_note);

        _back = std::make_shared<UiMark>(UiMark::Shape::Cancel, "back");
        _back->Click += [this](UiMark&) { Leave(); };
        _go = std::make_shared<UiMark>(UiMark::Shape::Accept, "continue");
        _go->Click += [this](UiMark&) { Go(); };
        _fetch = std::make_shared<UiMark>(UiMark::Shape::Fetch, "files required -- install");
        _fetch->IsVisible(false);
        _fetch->Click += [this](UiMark&) { Fetch(); };

        auto body = std::make_shared<Controls::ScrollViewer>();
        body->Content(_form);
        body->HorizontalScrollBarVisibility(Controls::ScrollBarVisibility::Disabled);
        body->VerticalScrollBarVisibility(Controls::ScrollBarVisibility::Auto);
        _page = UiLayout::Page(false, UiLayout::WellSettings, "create lobby", nullptr,
            body, _back, _go, false, _fetch);
        _root->Children.Add(_page);
        Content(_root);

        std::optional<std::string> start;
        if (firstMap.has_value()
            && std::find(_rooms.begin(), _rooms.end(), *firstMap) != _rooms.end())
        {
            start = std::move(firstMap);
        }
        else if (!_rooms.empty())
        {
            start = _rooms[0];
        }
        if (start.has_value())
        {
            _rotation.push_back(std::move(*start));
        }
        _maps->Set(Describe());
        Refresh();

        // shared_from_this is available once the screen has been installed in
        // the visual tree. Start discovery then, before the first visible
        // interaction, and retain the C# screen while its callbacks are live.
        AttachedToVisualTree += [this](Controls::Control&)
        {
            const std::shared_ptr<CreateServerScreen> self = Self();
            Threading::Dispatcher::UIThread().Post([self]
            {
                self->_name->Box->Focus();
            }, Threading::DispatcherPriority::Background);
            if (!self->_directoriesStarted)
            {
                self->_directoriesStarted = true;
                self->AskDirectories();
            }
        };
        DetachedFromVisualTree += [this](Controls::Control&)
        {
            if (_work != nullptr)
            {
                _work->request_stop();
            }
        };
    }

    bool CreateServerScreen::CanRunHere()
    {
        return !::MphRead::NativeRuntime::IsAndroid();
    }

    std::shared_ptr<CreateServerScreen> CreateServerScreen::Self()
    {
        return std::static_pointer_cast<CreateServerScreen>(shared_from_this());
    }

    void CreateServerScreen::ShowDedicated()
    {
        _kind->Index(1);
        Refresh();
    }

    void CreateServerScreen::OnKeyDown(Av::Input::KeyEventArgs& e)
    {
        if (e.Key == Av::Input::Key::Escape)
        {
            Leave();
            e.Handled = true;
            return;
        }
        UserControl::OnKeyDown(e);
    }

    bool CreateServerScreen::Dedicated() const
    {
        return CanRunHere() && _kind->Index() == 1;
    }

    void CreateServerScreen::Leave()
    {
        if (_finished || _busy)
        {
            return;
        }
        _finished = true;
        if (_work != nullptr)
        {
            _work->request_stop();
        }
        const std::shared_ptr<CreateServerScreen> self = Self();
        Closed(*self);
    }

    void CreateServerScreen::Refresh()
    {
        const bool dedicated = Dedicated();
        _host->IsVisible(!dedicated);
        if (!dedicated)
        {
            _fetch->IsVisible(false);
            _go->IsEnabled(!_busy);
            if (_asking)
            {
                Say("Asking who can run one...", GuiTheme::TextDim);
            }
            else if (_chosen.has_value())
            {
                Say(_chosen->Label + " opens the match on its own machine. Nothing to forward here.",
                    GuiTheme::TextDim);
            }
            else
            {
                Say(CanRunHere()
                    ? "No server will open one for you. Pick Dedicated server to run it here."
                    : "No server will open one for you. Try again in a moment, or join somebody else's from the browser.",
                    GuiTheme::Warm);
            }
            return;
        }

        const bool ready = Network::LocalServer::Ready();
        _fetch->IsVisible(!ready && !_busy);
        _go->IsEnabled(ready && !_busy);
        if (!ready)
        {
            Say(Network::LocalServer::CanInstall()
                ? ::MphRead::Mods::Update::UpdateCheck::ServerBinaryName() + " is not here yet -- install it below."
                : "No server package is published for this platform. Use Hosted.",
                GuiTheme::Warm);
            return;
        }
        Say("Runs here, in its own window. Forward UDP "
            + std::to_string(Network::NetConfig::DefaultPort)
            + " to this PC for anyone outside to join; you join over 127.0.0.1 either way.",
            GuiTheme::Warm);
    }

    void CreateServerScreen::Say(std::string text, Media::Color colour)
    {
        _note->Text(std::move(text));
        _note->Foreground(std::make_shared<Media::SolidColorBrush>(colour));
    }

    std::string CreateServerScreen::Describe() const
    {
        if (_rotation.empty())
        {
            return "none picked";
        }
        const auto [meta, roomId] = ::MphRead::Metadata::GetRoomByName(_rotation[0]);
        (void)roomId;
        const std::string first = meta != nullptr ? meta->InGameName.value_or(_rotation[0]) : _rotation[0];
        return _rotation.size() == 1
            ? first
            : first + " +" + std::to_string(_rotation.size() - 1) + " more";
    }

    void CreateServerScreen::AskDirectories()
    {
        _asking = true;
        _candidates.clear();
        _chosen.reset();
        const std::shared_ptr<CreateServerScreen> self = Self();
        Network::NetMasterClient::FindHosts(
            LauncherPrefs::MasterHost(), LauncherPrefs::MasterPort(),
            [self](Network::HostCandidate candidate)
            {
                Threading::Dispatcher::UIThread().Post([self, candidate = std::move(candidate)]() mutable
                {
                    self->Arrived(std::move(candidate));
                });
            },
            [self]
            {
                Threading::Dispatcher::UIThread().Post([self]
                {
                    if (self->_finished)
                    {
                        return;
                    }
                    self->_asking = false;
                    DebugLog::Line("net", std::to_string(self->_candidates.size())
                        + " host(s) asked, "
                        + std::to_string(std::count_if(self->_candidates.begin(), self->_candidates.end(),
                            [](const Network::HostCandidate& c) { return c.WillHost(); }))
                        + " can run a match");
                    if (!self->_chosen.has_value())
                    {
                        self->_host->Set("nobody");
                    }
                    self->Refresh();
                });
            });
    }

    void CreateServerScreen::Arrived(Network::HostCandidate candidate)
    {
        if (_finished)
        {
            return;
        }
        Network::NetMasterClient::Merge(_candidates, candidate);
        std::optional<Network::HostCandidate> best;
        for (const Network::HostCandidate& entry : _candidates)
        {
            if (entry.WillHost())
            {
                best = entry;
                break;
            }
        }
        if (best.has_value() && (!_chosen.has_value() || !_chosen->WillHost()))
        {
            _chosen = *best;
            _host->Set(best->Label);
            _asking = false;
            Refresh();
        }
        if (_picker != nullptr)
        {
            _picker->Show(_candidates, _asking);
        }
    }

    void CreateServerScreen::OpenHosts()
    {
        if (_busy || Dedicated())
        {
            return;
        }
        auto picker = std::make_shared<HostPicker>();
        picker->Show(_candidates, _asking);
        const std::weak_ptr<HostPicker> weakPicker = picker;
        picker->Done += [this, weakPicker](HostPicker&, Network::HostCandidate host)
        {
            const std::shared_ptr<HostPicker> keep = weakPicker.lock();
            if (keep == nullptr)
            {
                return;
            }
            _chosen = std::move(host);
            _host->Set(_chosen->Label);
            ClosePage();
            Refresh();
        };
        picker->Cancelled += [this, weakPicker](HostPicker&)
        {
            if (weakPicker.lock() != nullptr)
            {
                ClosePage();
            }
        };
        picker->RefreshRequested += [this, weakPicker](HostPicker&)
        {
            if (const std::shared_ptr<HostPicker> keep = weakPicker.lock())
            {
                _host->Set("asking...");
                AskDirectories();
                keep->Show(_candidates, true);
            }
        };
        _picker = picker;
        OpenPage(picker);
    }

    void CreateServerScreen::OpenMaps()
    {
        if (_busy)
        {
            return;
        }
        auto picker = std::make_shared<MapRotationPicker>(_rooms, _rotation);
        const std::weak_ptr<MapRotationPicker> weakPicker = picker;
        picker->Done += [this, weakPicker](MapRotationPicker&, std::vector<std::string> picked)
        {
            if (weakPicker.lock() == nullptr)
            {
                return;
            }
            _rotation = std::move(picked);
            _maps->Set(Describe());
            ClosePage();
        };
        picker->Cancelled += [this, weakPicker](MapRotationPicker&)
        {
            if (weakPicker.lock() != nullptr)
            {
                ClosePage();
            }
        };
        OpenPage(picker);
    }

    void CreateServerScreen::OpenPage(const Controls::ControlPtr& page)
    {
        _root->Children.Clear();
        _root->Children.Add(page);
        Threading::Dispatcher::UIThread().Post([page] { page->Focus(); },
            Threading::DispatcherPriority::Background);
    }

    void CreateServerScreen::ClosePage()
    {
        _picker.reset();
        _root->Children.Clear();
        _root->Children.Add(_page);
        const std::shared_ptr<CreateServerScreen> self = Self();
        Threading::Dispatcher::UIThread().Post([self] { self->_name->Box->Focus(); },
            Threading::DispatcherPriority::Background);
    }

    void CreateServerScreen::Fetch()
    {
        if (_busy || !Network::LocalServer::CanInstall())
        {
            return;
        }
        Busy(true, "installing");
        _progress->Set(0, "Fetching the dedicated-server package");
        const auto cancel = std::make_shared<std::stop_source>();
        _work = cancel;
        const std::shared_ptr<CreateServerScreen> self = Self();
        auto task = std::async(std::launch::async, [self, cancel]
        {
            return Network::LocalServer::Install([self, cancel](float fraction)
            {
                if (cancel->stop_requested())
                {
                    throw std::runtime_error("cancelled");
                }
                Threading::Dispatcher::UIThread().Post([self, cancel, fraction]
                {
                    if (!cancel->stop_requested())
                    {
                        self->_progress->Set(fraction < 0 ? 0 : fraction,
                            "Fetching the dedicated-server package");
                    }
                });
            });
        }).share();
        Await(std::move(task), [self, cancel](bool ok)
        {
            if (cancel->stop_requested())
            {
                return;
            }
            self->_progress->IsVisible(false);
            if (!ok)
            {
                self->Fail(Network::LocalServer::LastError().value_or(
                    "the package could not be installed"));
                return;
            }
            self->Busy(false, "continue");
            self->Refresh();
        });
    }

    void CreateServerScreen::Fail(std::optional<std::string> why)
    {
        Busy(false, "continue");
        Refresh();
        Say(why.value_or("that did not work"), GuiTheme::Bad);
    }

    void CreateServerScreen::Busy(bool busy, std::string label)
    {
        _busy = busy;
        _go->Label(std::move(label));
        _go->IsEnabled(!busy);
        _back->IsEnabled(!busy);
        _fetch->IsEnabled(!busy);
        if (busy)
        {
            _fetch->IsVisible(false);
        }
    }

    void CreateServerScreen::Go()
    {
        if (_finished || _busy)
        {
            return;
        }
        if (_rotation.empty())
        {
            Say("Pick at least one map first.", GuiTheme::Warm);
            return;
        }
        std::string name = StringTrim(_name->Value());
        if (name.empty())
        {
            name = "Fruity lobby";
        }
        const MphRead::GameMode mode = Modes().at(static_cast<std::size_t>(_mode->Index())).Mode;
        const MphRead::Hunter hunter = static_cast<MphRead::Hunter>(_hunter->Index() < Launcher::Hunters::Playable
            ? _hunter->Index() : static_cast<std::int32_t>(MphRead::Hunter::Random));
        std::vector<std::pair<std::string, MphRead::GameMode>> maps;
        maps.reserve(_rotation.size());
        for (const std::string& room : _rotation)
        {
            maps.emplace_back(room, mode);
        }
        std::string player = StringTrim(LauncherPrefs::PlayerName());
        if (player.empty())
        {
            player = "Player";
        }
        LauncherPrefs::LastHunter(hunter);
        LauncherPrefs::Save();

        if (Dedicated())
        {
            StartHere(std::move(name), std::move(player), hunter, std::move(maps));
            return;
        }
        StartOnServer(std::move(name), std::move(player), hunter, mode, std::move(maps));
    }

    void CreateServerScreen::StartHere(std::string name, std::string player, MphRead::Hunter hunter,
        std::vector<std::pair<std::string, MphRead::GameMode>> maps)
    {
        Busy(true, "starting");
        Say("Starting " + name + " on this machine...", GuiTheme::TextDim);
        const auto cancel = std::make_shared<std::stop_source>();
        _work = cancel;
        const std::shared_ptr<CreateServerScreen> self = Self();
        auto task = std::async(std::launch::async,
            [name, maps, cancel]
            {
                return Network::LocalServer::Start(name, maps,
                    Entities::PlayerEntity::SlotCapacity, 7 * 60,
                    Network::MatchGoalRules::DefaultValue(maps[0].second),
                    LauncherPrefs::MasterHost(), LauncherPrefs::MasterPort(),
                    LauncherPrefs::ListHostedGame(), cancel->get_token(), true);
            }).share();
        Await(std::move(task), [self, cancel, name = std::move(name), player = std::move(player),
            hunter, maps = std::move(maps)](std::int32_t port) mutable
        {
            (void)cancel;
            if (port < 0)
            {
                self->Fail(Network::LocalServer::LastError().value_or("the server would not start"));
                return;
            }
            self->Say("Joining your server on 127.0.0.1:" + std::to_string(port) + "...",
                GuiTheme::TextDim);
            auto join = std::async(std::launch::async,
                [port, player, hunter]
                {
                    return Network::NetLaunch::Connect("127.0.0.1", port, player, hunter,
                        8000, -1, Network::LocalServer::OwnerToken());
                }).share();
            Await(std::move(join), [self, name = std::move(name), player = std::move(player),
                hunter, maps = std::move(maps), port](bool joined) mutable
            {
                self->JoinedHere(std::move(name), std::move(player), hunter,
                    std::move(maps), port, joined);
            });
        });
    }

    void CreateServerScreen::JoinedHere(std::string name, std::string player, MphRead::Hunter hunter,
        std::vector<std::pair<std::string, MphRead::GameMode>> maps,
        std::int32_t port, bool joined)
    {
        if (!joined)
        {
            // Keep the process available to people who may already be joining;
            // only this client's failed connection is stopped.
            Network::NetSession::Stop();
            Fail(Network::NetLaunch::LastJoinError()
                + " -- the server is running; try joining 127.0.0.1:"
                + std::to_string(port) + " from the browser.");
            return;
        }
        LauncherPrefs::ServerAddress("127.0.0.1");
        LauncherPrefs::ServerPort(port);
        LauncherPrefs::LastKind(static_cast<std::int32_t>(Launcher::LaunchKind::Online));
        LauncherPrefs::Save();
        const std::string endpoint = "Local server · port " + std::to_string(port);
        Launcher::LaunchPlan::Init init;
        init.Kind = Launcher::LaunchKind::Online;
        init.Lobby = std::make_shared<Launcher::LobbyContext>(std::move(name), endpoint, true);
        init.Hunter = hunter;
        init.RoomKey = std::string();
        init.Mode = maps[0].second;
        init.Port = port;
        init.PlayerName = std::move(player);
        Finish(Launcher::LaunchPlan(init));
    }

    void CreateServerScreen::StartOnServer(std::string name, std::string player,
        MphRead::Hunter hunter, MphRead::GameMode mode,
        std::vector<std::pair<std::string, MphRead::GameMode>> maps)
    {
        if (!_chosen.has_value())
        {
            Say(CanRunHere()
                ? "No server will open one for you. Pick Dedicated server to run it here."
                : "No server will open one for you. Try again in a moment, or join somebody else's from the browser.",
                GuiTheme::Warm);
            return;
        }
        const std::string host = _chosen->Host;
        const std::int32_t port = _chosen->Port;
        Busy(true, "starting");
        Say("Asking " + host + " to open your lobby...", GuiTheme::TextDim);
        const std::shared_ptr<CreateServerScreen> self = Self();
        auto task = std::async(std::launch::async,
            [host, port, name, mode, maps]
            {
                return Network::NetMasterClient::RequestGame(host, port, maps[0].first, mode,
                    7 * 60, Network::MatchGoalRules::DefaultValue(mode),
                    Entities::PlayerEntity::SlotCapacity, name, 6000,
                    std::optional<std::vector<std::pair<std::string, MphRead::GameMode>>>(maps),
                    Network::ServerSessionPolicy::Lobby);
            }).share();
        Await(std::move(task), [self, name = std::move(name), player = std::move(player), hunter,
            mode, maps = std::move(maps), host](Network::HostedGame game) mutable
        {
            if (!game.Started)
            {
                self->Fail(game.Reason.empty()
                    ? std::optional<std::string>(host + " would not open a game")
                    : std::optional<std::string>(std::move(game.Reason)));
                return;
            }
            auto join = std::async(std::launch::async,
                [game, player, hunter]
                {
                    return Network::NetLaunch::Connect(game.Host, game.Port, player, hunter,
                        8000, -1, game.OwnerToken);
                }).share();
            Await(std::move(join), [self, name = std::move(name), player = std::move(player),
                hunter, mode, game = std::move(game)](bool joined) mutable
            {
                self->JoinedHosted(std::move(name), std::move(player), hunter,
                    mode, std::move(game), joined);
            });
        });
    }

    void CreateServerScreen::JoinedHosted(std::string name, std::string player,
        MphRead::Hunter hunter, MphRead::GameMode mode, Network::HostedGame game, bool joined)
    {
        if (!joined)
        {
            Network::NetSession::Stop();
            Network::NetHostSession::Stop();
            Fail(Network::NetLaunch::LastJoinError());
            return;
        }
        LauncherPrefs::ServerAddress(game.Host);
        LauncherPrefs::ServerPort(game.Port);
        LauncherPrefs::LastKind(static_cast<std::int32_t>(Launcher::LaunchKind::Host));
        LauncherPrefs::Save();
        Launcher::LaunchPlan::Init init;
        init.Kind = Launcher::LaunchKind::Host;
        init.Lobby = std::make_shared<Launcher::LobbyContext>(std::move(name),
            game.Host + ":" + std::to_string(game.Port));
        init.Hunter = hunter;
        init.RoomKey = std::string();
        init.Mode = mode;
        init.Port = game.Port;
        init.PlayerName = std::move(player);
        Finish(Launcher::LaunchPlan(init));
    }

    void CreateServerScreen::Finish(Launcher::LaunchPlan plan)
    {
        if (_finished)
        {
            return;
        }
        _finished = true;
        _busy = false;
        Launched(*this, std::move(plan));
    }

    // --------------------------------------------------------------- PickRow

    PickRow::PickRow(std::string label)
        : _label(std::move(label))
    {
        Height(34);
        Focusable(true);
        Cursor(std::make_shared<Av::Input::Cursor>(Av::Input::StandardCursorType::Hand));
    }

    std::shared_ptr<PickRow> PickRow::Self()
    {
        return std::static_pointer_cast<PickRow>(shared_from_this());
    }

    void PickRow::Set(std::string value)
    {
        _value = std::move(value);
        InvalidateVisual();
    }

    void PickRow::OnPointerEntered(Av::Input::PointerEventArgs& e)
    {
        _hot = true;
        InvalidateVisual();
        Control::OnPointerEntered(e);
    }

    void PickRow::OnPointerExited(Av::Input::PointerEventArgs& e)
    {
        _hot = false;
        _tap.Cancel();
        InvalidateVisual();
        Control::OnPointerExited(e);
    }

    void PickRow::OnPointerPressed(Av::Input::PointerPressedEventArgs& e)
    {
        _tap.Press(e, *this);
        Control::OnPointerPressed(e);
    }

    void PickRow::OnPointerMoved(Av::Input::PointerEventArgs& e)
    {
        _tap.Moved(e, *this);
        Control::OnPointerMoved(e);
    }

    void PickRow::OnPointerReleased(Av::Input::PointerReleasedEventArgs& e)
    {
        if (_tap.Release(e, *this))
        {
            Focus();
            const std::shared_ptr<PickRow> self = Self();
            Clicked(*self);
        }
        Control::OnPointerReleased(e);
    }

    void PickRow::OnPointerCaptureLost(Av::Input::PointerCaptureLostEventArgs& e)
    {
        _tap.Cancel();
        Control::OnPointerCaptureLost(e);
    }

    void PickRow::OnKeyDown(Av::Input::KeyEventArgs& e)
    {
        if (e.Key == Av::Input::Key::Enter || e.Key == Av::Input::Key::Space
            || e.Key == Av::Input::Key::Right)
        {
            const std::shared_ptr<PickRow> self = Self();
            Clicked(*self);
            e.Handled = true;
            return;
        }
        Control::OnKeyDown(e);
    }

    void PickRow::OnGotFocus(Av::Input::GotFocusEventArgs& e)
    {
        InvalidateVisual();
        Control::OnGotFocus(e);
    }

    void PickRow::OnLostFocus(Av::Input::FocusChangedEventArgs& e)
    {
        InvalidateVisual();
        Control::OnLostFocus(e);
    }

    void PickRow::Render(Media::DrawingContext& context)
    {
        context.FillRectangle(Media::Brushes::Transparent(), Rect(0, 0, Bounds().Width, Bounds().Height));
        const bool lit = _hot || IsFocused();
        Media::FormattedText label(_label, Media::InvariantCulture, Media::FlowDirection::LeftToRight,
            GuiTheme::Face(false), 13, GuiTheme::TextDimBrush);
        context.DrawText(label, Point{4, (Bounds().Height - label.Height()) / 2});
        Media::FormattedText value(_value + "   >", Media::InvariantCulture, Media::FlowDirection::LeftToRight,
            GuiTheme::Face(true), 13,
            std::make_shared<Media::SolidColorBrush>(lit ? GuiTheme::Accent : GuiTheme::Text));
        context.DrawText(value, Point{Bounds().Width - value.Width() - 4,
            (Bounds().Height - value.Height()) / 2});
    }

    // ------------------------------------------------------------- HostPicker

    HostPicker::HostPicker()
        : _list(std::make_shared<UiList>()),
          _note(std::make_shared<Note>(""))
    {
        Background(Media::Brushes::Transparent());
        Focusable(true);
        auto back = std::make_shared<UiMark>(UiMark::Shape::Cancel, "back");
        back->Click += [this](UiMark&)
        {
            const std::shared_ptr<HostPicker> self = Self();
            Cancelled(*self);
        };
        auto again = std::make_shared<UiMark>(UiMark::Shape::Add, "ask again");
        again->Click += [this](UiMark&)
        {
            const std::shared_ptr<HostPicker> self = Self();
            RefreshRequested(*self);
        };

        auto body = std::make_shared<Controls::Grid>();
        body->RowDefinitions(Controls::RowDefinitions("*,Auto"));
        Controls::Grid::SetRow(*_note, 1);
        body->Children.Add(_list);
        body->Children.Add(_note);
        Content(UiLayout::Page(false, UiLayout::WellPlay, "host on", nullptr,
            body, back, nullptr, false, again));

        AttachedToVisualTree += [this](Controls::Control&)
        {
            const std::shared_ptr<HostPicker> self = Self();
            Threading::Dispatcher::UIThread().Post([self] { self->_list->FocusFirst(); },
                Threading::DispatcherPriority::Background);
        };
    }

    std::shared_ptr<HostPicker> HostPicker::Self()
    {
        return std::static_pointer_cast<HostPicker>(shared_from_this());
    }

    void HostPicker::Show(const std::vector<Network::HostCandidate>& candidates, bool asking)
    {
        std::vector<Network::HostCandidate> ordered(candidates);
        ManagedSort(ordered, [](const Network::HostCandidate& a, const Network::HostCandidate& b)
        {
            return a.WillHost() == b.WillHost() ? 0 : a.WillHost() ? -1 : 1;
        });
        _list->Clear();
        std::int32_t usable = 0;
        for (const Network::HostCandidate& candidate : ordered)
        {
            auto row = std::make_shared<UiListRow>(candidate.Label, candidate.Describe());
            row->Choice = candidate;
            if (candidate.WillHost())
            {
                usable++;
                const Network::HostCandidate picked = candidate;
                _list->Add(row);
                row->Clicked += [this, picked](UiListRow&)
                {
                    const std::shared_ptr<HostPicker> self = Self();
                    Done(*self, picked);
                };
            }
            else
            {
                // UiList::Add retains the row visually and deliberately skips
                // adding a non-focusable row to keyboard navigation.
                row->Focusable(false);
                _list->Add(row);
            }
        }
        if (asking)
        {
            _list->AddNote("asking the rest...", GuiTheme::TextDim);
        }
        else if (ordered.empty())
        {
            _list->AddNote("The directory did not answer, so there is no list of servers to ask.",
                GuiTheme::Warm);
        }
        if (usable > 0)
        {
            _note->Text(std::to_string(usable)
                + " can run a match for you. A server can open one when its admin allows it a port range.");
        }
        else if (asking)
        {
            _note->Text("");
        }
        else
        {
            _note->Text(CreateServerScreen::CanRunHere()
                ? "None of these hosts are configured to create lobbies. The server admin can enable hosted lobbies with -hostports FIRST-LAST, or Dedicated server can run one on this machine."
                : "None of these hosts are configured to create lobbies. The server admin must enable a hosted-game port range.");
        }
        _note->Foreground(usable > 0 ? GuiTheme::TextDimBrush : GuiTheme::WarmBrush);
        if (!_focused && usable > 0)
        {
            _focused = true;
            _list->FocusFirst();
        }
    }

    void HostPicker::OnKeyDown(Av::Input::KeyEventArgs& e)
    {
        if (e.Key == Av::Input::Key::Escape)
        {
            const std::shared_ptr<HostPicker> self = Self();
            Cancelled(*self);
            e.Handled = true;
            return;
        }
        if (_list->HandleKey(e.Key))
        {
            e.Handled = true;
            return;
        }
        UserControl::OnKeyDown(e);
    }

    // ------------------------------------------------------ MapRotationPicker

    MapRotationPicker::MapRotationPicker(const std::vector<std::string>& rooms,
        const std::vector<std::string>& picked, bool single)
        : _list(std::make_shared<UiList>()),
          _note(std::make_shared<Note>("")),
          _picked(picked),
          _rooms(rooms),
          _single(single)
    {
        Background(Media::Brushes::Transparent());
        Focusable(true);
        auto back = std::make_shared<UiMark>(UiMark::Shape::Cancel, "back");
        back->Click += [this](UiMark&)
        {
            const std::shared_ptr<MapRotationPicker> self = Self();
            Cancelled(*self);
        };
        auto done = std::make_shared<UiMark>(UiMark::Shape::Accept,
            single ? "use this map" : "use these maps");
        done->Click += [this](UiMark&) { Commit(); };

        auto body = std::make_shared<Controls::Grid>();
        body->RowDefinitions(Controls::RowDefinitions("*,Auto"));
        Controls::Grid::SetRow(*_note, 1);
        body->Children.Add(_list);
        body->Children.Add(_note);
        Content(UiLayout::Page(false, UiLayout::WellPlay,
            single ? "choose map" : "map rotation", nullptr, body, back, done));
        Fill();

        AttachedToVisualTree += [this](Controls::Control&)
        {
            const std::shared_ptr<MapRotationPicker> self = Self();
            Threading::Dispatcher::UIThread().Post([self] { self->_list->FocusFirst(); },
                Threading::DispatcherPriority::Background);
        };
    }

    std::shared_ptr<MapRotationPicker> MapRotationPicker::Self()
    {
        return std::static_pointer_cast<MapRotationPicker>(shared_from_this());
    }

    void MapRotationPicker::Fill()
    {
        _list->Clear();
        _byRoom.clear();
        if (_rooms.empty())
        {
            _list->AddNote("No multiplayer rooms were found. Set the game files up from Settings.",
                GuiTheme::Warm);
            return;
        }
        for (const std::string& room : _rooms)
        {
            const auto [meta, roomId] = ::MphRead::Metadata::GetRoomByName(room);
            (void)roomId;
            auto row = std::make_shared<UiListRow>(meta != nullptr ? meta->InGameName.value_or(room) : room, "");
            row->Choice = room;
            _byRoom[room] = row;
            _list->Add(row);
            row->Clicked += [this, room](UiListRow&) { Toggle(room); };
        }
        Mark();
    }

    void MapRotationPicker::Toggle(const std::string& room)
    {
        if (_single)
        {
            _picked.clear();
            _picked.push_back(room);
            Mark();
            return;
        }
        const auto found = std::find(_picked.begin(), _picked.end(), room);
        if (found == _picked.end())
        {
            if (_picked.size() >= Network::HostRequestPacket::MaxRotation)
            {
                _note->Text(std::to_string(Network::HostRequestPacket::MaxRotation)
                    + " maps is as long as a rotation can be sent.");
                _note->Foreground(GuiTheme::WarmBrush);
                return;
            }
            _picked.push_back(room);
        }
        else
        {
            _picked.erase(found);
        }
        Mark();
    }

    void MapRotationPicker::Mark()
    {
        for (const auto& [room, row] : _byRoom)
        {
            const auto found = std::find(_picked.begin(), _picked.end(), room);
            row->Detail(found == _picked.end()
                ? ""
                : "#" + std::to_string(static_cast<std::size_t>(found - _picked.begin()) + 1));
        }
        _note->Foreground(GuiTheme::TextDimBrush);
        if (_picked.empty())
        {
            _note->Text(_single ? "Pick a map."
                : "Press maps to build the cycle. They are played in the order you press them.");
            return;
        }
        if (_single)
        {
            const auto [meta, roomId] = ::MphRead::Metadata::GetRoomByName(_picked[0]);
            (void)roomId;
            _note->Text("Selected: " + (meta != nullptr ? meta->InGameName.value_or(_picked[0]) : _picked[0]));
            return;
        }
        std::string sequence;
        for (const std::string& room : _picked)
        {
            const auto [meta, roomId] = ::MphRead::Metadata::GetRoomByName(room);
            (void)roomId;
            if (!sequence.empty())
            {
                sequence += "  >  ";
            }
            sequence += meta != nullptr ? meta->InGameName.value_or(room) : room;
        }
        _note->Text(std::move(sequence));
    }

    void MapRotationPicker::Commit()
    {
        if (_picked.empty())
        {
            _note->Text(_single ? "Pick a map." : "Pick at least one map.");
            _note->Foreground(GuiTheme::WarmBrush);
            return;
        }
        const std::shared_ptr<MapRotationPicker> self = Self();
        Done(*self, _picked);
    }

    void MapRotationPicker::OnKeyDown(Av::Input::KeyEventArgs& e)
    {
        if (e.Key == Av::Input::Key::Escape)
        {
            const std::shared_ptr<MapRotationPicker> self = Self();
            Cancelled(*self);
            e.Handled = true;
            return;
        }
        if (_list->HandleKey(e.Key))
        {
            e.Handled = true;
            return;
        }
        UserControl::OnKeyDown(e);
    }
}
