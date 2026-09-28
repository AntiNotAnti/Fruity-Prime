#include "StartScreen.hpp"

#include "ConfirmScreen.hpp"
#include "CreateServerScreen.hpp"
#include "Deck.hpp"
#include "DeckButton.hpp"
#include "DeckChip.hpp"
#include "DeckWordmark.hpp"
#include "LobbyScreen.hpp"
#include "PauseMenuView.hpp"
#include "PlayScreen.hpp"
#include "SettingsView.hpp"
#include "SetupScreen.hpp"
#include "UiLayout.hpp"
#include "UiWord.hpp"
#include "GamepadNavigation.hpp"
#include "../../Branding.hpp"
#include "../../Chat/ChatBox.hpp"
#include "../../Credits.hpp"
#include "../../DebugLog.hpp"
#include "../../Network/DemoRecorder.hpp"
#include "../../Network/MapVote.hpp"
#include "../../Network/NetSession.hpp"
#include "../../SpectatorMode.hpp"
#include "../../ThumbnailGenerator.hpp"
#include "../../ThumbnailHost.hpp"
#include "../../Update/BuildVersion.hpp"
#include "../../Update/UpdateCheck.hpp"
#include "../../Update/UpdateInstall.hpp"
#include "../../Update/Updater.hpp"
#include "../Portable/GameFiles.hpp"
#include "../Portable/LauncherPrefs.hpp"
#include "../../Input/InputPrompt.hpp"
#include "../../Input/InputSourceTracker.hpp"
#include "../../../Menu.hpp"
#include "../../../NativeRuntime/Avalonia/Threading.hpp"
#include "../../../NativeRuntime/System/Globalization.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"
#include "../../../NativeRuntime/System/Tasks.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <iostream>
#include <limits>
#include <optional>
#include <utility>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Runtime = ::MphRead::NativeRuntime;
    namespace Network = ::MphRead::Mods::Network;
    namespace Update = ::MphRead::Mods::Update;
    namespace Threading = ::MphRead::NativeRuntime::Avalonia::Threading;
    namespace GameInput = ::MphRead::Mods::Input;
    namespace AvInput = ::MphRead::NativeRuntime::Avalonia::Input;
    using namespace ::MphRead::NativeRuntime::Avalonia;

    StartScreen::StartScreen(const std::shared_ptr<::MphRead::MenuSettings>& settings,
        const std::vector<std::string>& rooms)
        : _settings(settings), _rooms(rooms)
    {
        Focusable(true);

        auto root = UiLayout::Backdrop(false, UiLayout::BackdropWash::Light);
        _root = root;
        _ground.reserve(root->Children.Count());
        for (const Controls::ControlPtr& child : root->Children)
        {
            _ground.push_back(child);
        }
        _dark = std::make_shared<Controls::Border>();
        _dark->Background(GuiTheme::InkBrush);
        _dark->IsVisible(false);
        root->Children.Insert(0, _dark);

        _menu = std::make_shared<Controls::StackPanel>();
        _menu->Spacing(0);
        _menu->HorizontalAlignment(Layout::HorizontalAlignment::Center);
        _menu->VerticalAlignment(Layout::VerticalAlignment::Center);

        _wordmark = std::make_shared<DeckWordmark>();
        _wordmark->HorizontalAlignment(Layout::HorizontalAlignment::Center);
        _wordmark->VerticalAlignment(Layout::VerticalAlignment::Center);
        _wordmark->Margin(Thickness(0, 0, 0, 30));
        _menu->Children.Add(_wordmark);

        _subtitle = std::make_shared<Controls::TextBlock>();
        _subtitle->Text("METROID PRIME HUNTERS  \xC2\xB7  REBORN");
        _subtitle->FontFamily(GuiTheme::Display());
        _subtitle->FontSize(11);
        _subtitle->Foreground(GuiTheme::TextDimBrush);
        _subtitle->HorizontalAlignment(Layout::HorizontalAlignment::Center);
        _subtitle->Margin(Thickness(0, -18, 0, 0));
        _menu->Children.Add(_subtitle);

        _bar = std::make_shared<Controls::StackPanel>();
        _bar->Orientation(Layout::Orientation::Horizontal);
        _bar->Spacing(12);
        _bar->HorizontalAlignment(Layout::HorizontalAlignment::Center);
        _bar->Margin(Thickness(0, 6, 0, 0));

        auto play = std::make_shared<DeckButton>("PLAY", Deck::Face::Blue());
        play->Idle = true;
        play->Click += [this](DeckButton&) { OpenPlay(); };
        _bar->Children.Add(play);

        auto options = std::make_shared<DeckButton>("SETTINGS", Deck::Face::Brass());
        options->Click += [this](DeckButton&) { OpenSettings(); };
        _bar->Children.Add(options);

        auto quit = std::make_shared<DeckButton>("QUIT", Deck::Face::Rust());
        quit->Click += [this](DeckButton&) { AskToQuit(); };
        _bar->Children.Add(quit);
        root->Children.Add(_menu);

        _chip = std::make_shared<DeckChip>("Profile", PlayerNameOrDefault());
        _chip->VerticalAlignment(Layout::VerticalAlignment::Bottom);
        _heart = SupportMark();
        _heart->VerticalAlignment(Layout::VerticalAlignment::Bottom);

        _heartCorner = SupportMark();
        _heartCorner->HorizontalAlignment(Layout::HorizontalAlignment::Left);
        _heartCorner->VerticalAlignment(Layout::VerticalAlignment::Top);
        _heartCorner->Margin(Thickness(14, 14, 0, 0));
        _heartCorner->IsVisible(false);
        root->Children.Add(_heartCorner);

        _foot = std::make_shared<Controls::Grid>();
        _foot->VerticalAlignment(Layout::VerticalAlignment::Bottom);
        _foot->Margin(Thickness(26, 0, 26, 24));
        _foot->ColumnDefinitions(Controls::ColumnDefinitions("Auto,*,Auto"));
        Controls::Grid::SetColumn(*_chip, 0);
        _foot->Children.Add(_chip);
        _bar->VerticalAlignment(Layout::VerticalAlignment::Bottom);
        Controls::Grid::SetColumn(*_bar, 1);
        _foot->Children.Add(_bar);
        Controls::Grid::SetColumn(*_heart, 2);
        _foot->Children.Add(_heart);
        root->Children.Add(_foot);

        _root->SizeChanged += [this](Controls::Control&, Size newSize)
        {
            LayOutBar(newSize.Width);
            LayOutWordmark(newSize);
        };
        LayOutBar(WindowWidthGuess);

        _version = std::make_shared<Controls::TextBlock>();
        _version->Text(VersionNumber());
        _version->FontFamily(GuiTheme::Display());
        _version->FontSize(12);
        _version->Foreground(GuiTheme::TextDimBrush);
        _version->HorizontalAlignment(Layout::HorizontalAlignment::Center);

        _versionBox = std::make_shared<Controls::Border>();
        _versionBox->Background(Media::Brushes::Transparent());
        _versionBox->HorizontalAlignment(Layout::HorizontalAlignment::Right);
        _versionBox->VerticalAlignment(Layout::VerticalAlignment::Top);
        _versionBox->Margin(Thickness(0, 18, 24, 0));
        _versionBox->Child(_version);
        _versionBox->Focusable(true);
        _versionBox->KeyDown += [this](AvInput::InputElement&, AvInput::KeyEventArgs& e)
        {
            if ((e.Key == AvInput::Key::Enter || e.Key == AvInput::Key::Space) && _updatable)
            {
                e.Handled = true;
                UpdateNow();
            }
        };
        _versionBox->PointerPressed += [this](AvInput::InputElement&, AvInput::PointerPressedEventArgs& e)
        {
            if (_updatable)
            {
                e.Handled = true;
                UpdateNow();
            }
        };
        root->Children.Add(_versionBox);

        _help = std::make_shared<Controls::TextBlock>();
        _help->Foreground(GuiTheme::TextDimBrush);
        _help->FontSize(12);
        _help->Margin(Thickness(20, 0, 0, 3));
        _help->VerticalAlignment(Layout::VerticalAlignment::Bottom);
        _help->HorizontalAlignment(Layout::HorizontalAlignment::Left);
        _help->IsHitTestVisible(false);
        _hintTimer = std::make_shared<Threading::DispatcherTimer>(Threading::TimeSpan(0.25),
            Threading::DispatcherPriority::Background, [this]
            {
                std::string prompt;
                if (GameInput::InputSourceTracker::Current() == GameInput::InputSource::Gamepad)
                {
                    prompt = GameInput::InputPrompt::For(GameInput::UiAction::Accept).Glyph() + " Select   "
                        + GameInput::InputPrompt::For(GameInput::UiAction::Back).Glyph() + " Back   "
                        + GameInput::InputPrompt::For(GameInput::UiAction::PreviousTab).Glyph() + "/"
                        + GameInput::InputPrompt::For(GameInput::UiAction::NextTab).Glyph() + " Tabs";
                }
                else
                {
                    prompt = "Enter Select   Esc Back";
                }
                if (prompt != _controllerPrompt)
                {
                    _help->Text(prompt);
                    _controllerPrompt = std::move(prompt);
                }
            });

        _overlay = std::make_shared<Controls::Panel>();
        _overlay->Background(Media::Brushes::Transparent());
        _overlay->IsVisible(false);
        root->Children.Add(_overlay);
        root->Children.Add(_help);
        Content(root);

#if defined(__ANDROID__)
        _gamepadNavigation = std::make_shared<GamepadNavigation>();
        _gamepadTimer = std::make_shared<Threading::DispatcherTimer>(Threading::TimeSpan(0.016),
            Threading::DispatcherPriority::Input, [this]
            {
                if (GameInput::GamepadContexts::MenuVisible())
                {
                    _gamepadNavigation->Update(*this);
                }
            });
#endif

    }

    std::shared_ptr<StartScreen> StartScreen::Create(
        const std::shared_ptr<::MphRead::MenuSettings>& settings,
        const std::vector<std::string>& rooms)
    {
        auto screen = std::shared_ptr<StartScreen>(new StartScreen(settings, rooms));
        screen->StartUpdateCheck();
        screen->RefreshVersionLine();
        screen->CatchUpPreviews();
        return screen;
    }

    StartScreen::~StartScreen()
    {
        if (_hintTimer != nullptr)
        {
            _hintTimer->Stop();
        }
        if (_gamepadTimer != nullptr)
        {
            _gamepadTimer->Stop();
        }
    }

    std::shared_ptr<StartScreen> StartScreen::Self()
    {
        return std::dynamic_pointer_cast<StartScreen>(shared_from_this());
    }

    void StartScreen::StartUpdateCheck()
    {
        if (_updateCheckStarted || !Launcher::LauncherPrefs::AutoUpdate())
        {
            return;
        }
        _updateCheckStarted = true;
        const std::shared_ptr<StartScreen> self = Self();
        Update::Updater::CheckInBackground(
            [self](Update::UpdateInfo)
            {
                Threading::Dispatcher::UIThread().Post([self]
                {
                    self->RefreshVersionLine();
                });
            },
            [self]
            {
                Threading::Dispatcher::UIThread().Post([self]
                {
                    self->RefreshVersionLine();
                });
            });
    }

    std::shared_ptr<DeckButton> StartScreen::SupportMark()
    {
        auto mark = std::make_shared<DeckButton>("", Deck::Face::Rust(), 1.55, 0.9, 0.7, 5);
        mark->Glyph = DeckHeart::DrawHeart;
        mark->GlyphEms = Size(1.9, 1.27);
        mark->GlyphColour = Media::Color::FromRgb(0xe8, 0xa0, 0xa0);
        mark->Tip = "Support this project <3";
        mark->Click += [](DeckButton&)
        {
            (void)Update::Updater::OpenLink(std::string(::MphRead::Mods::Credits::SupportUrl));
        };
        return mark;
    }

    std::string StartScreen::PlayerNameOrDefault()
    {
        std::string name = Runtime::StringTrim(Launcher::LauncherPrefs::PlayerName());
        return name.empty() ? "Player" : name;
    }

    void StartScreen::LayOutBar(double width)
    {
        if (_bar == nullptr)
        {
            return;
        }
        const bool column = width < BarTurnsWidth;
        _bar->Orientation(column ? Layout::Orientation::Vertical : Layout::Orientation::Horizontal);
        _bar->Spacing(column ? 9 : 12);
        _bar->Width(column
            ? std::max(160.0, std::min(320.0, width - 48))
            : std::numeric_limits<double>::quiet_NaN());

        if (_foot != nullptr)
        {
            _foot->ColumnDefinitions(Controls::ColumnDefinitions(column ? "*" : "Auto,*,Auto"));
            _foot->RowDefinitions(Controls::RowDefinitions(column ? "Auto,Auto" : "*"));
            if (_chip != nullptr)
            {
                Controls::Grid::SetColumn(*_chip, 0);
                Controls::Grid::SetRow(*_chip, 0);
                _chip->HorizontalAlignment(column
                    ? Layout::HorizontalAlignment::Stretch : Layout::HorizontalAlignment::Left);
                _chip->Margin(Thickness(0, 0, 0, column ? 9 : 0));
            }
            Controls::Grid::SetColumn(*_bar, column ? 0 : 1);
            Controls::Grid::SetRow(*_bar, column ? 1 : 0);
            if (_heart != nullptr)
            {
                Controls::Grid::SetColumn(*_heart, column ? 0 : 2);
                _heart->IsVisible(!column);
            }
            if (_heartCorner != nullptr)
            {
                _heartCorner->IsVisible(column);
            }
            _foot->Margin(Thickness(column ? 14 : 26, 0, column ? 14 : 26,
                column ? 18 : 24));
        }
        for (const Controls::ControlPtr& child : _bar->Children)
        {
            child->HorizontalAlignment(column
                ? Layout::HorizontalAlignment::Stretch : Layout::HorizontalAlignment::Center);
        }
    }

    void StartScreen::LayOutWordmark(Size frame)
    {
        if (frame.Width <= 0 || frame.Height <= 0)
        {
            return;
        }
        const bool landscape = frame.Width > frame.Height;
        const double ems = !Deck::Phone() ? 7.6 : landscape ? 4.4 : 5.2;
        if (_wordmark != nullptr)
        {
            _wordmark->SizeEms = ems;
            _wordmark->Margin(Thickness(0, 0, 0, 0));
        }
        const double em = Deck::EmFor(frame.Width, frame.Height);
        if (_subtitle != nullptr)
        {
            _subtitle->FontSize(std::max(8.0,
                static_cast<double>(Runtime::MathRoundToInt32(em * 0.82))));
            _subtitle->Margin(Thickness(0,
                static_cast<double>(Runtime::MathRoundToInt32(em * 0.82 * 1.4)) - 10, 0, 0));
        }
    }

    std::shared_ptr<UiWord> StartScreen::Word(const std::string& text,
        const Media::FontFamilyPtr& font, double size, Media::Color colour,
        std::function<void()> go)
    {
        auto word = std::make_shared<UiWord>(text, size, font, colour);
        word->Click += [go = std::move(go)](UiWord&) { go(); };
        return word;
    }

    void StartScreen::OnAttachedToVisualTree()
    {
        UserControl::OnAttachedToVisualTree();
        if (_hintTimer != nullptr)
        {
            _hintTimer->Start();
        }
#if defined(__ANDROID__)
        if (_gamepadTimer != nullptr)
        {
            _gamepadTimer->Start();
        }
#endif
        if (!Launcher::GameFiles::Ready() && _stack.empty())
        {
            const std::shared_ptr<StartScreen> self = Self();
            Deck::NextFrame(*this, [self]
            {
                if (!Launcher::GameFiles::Ready() && self->_stack.empty())
                {
                    self->OpenSetup();
                }
            });
        }
    }

    void StartScreen::OnDetachedFromVisualTree()
    {
        if (_hintTimer != nullptr)
        {
            _hintTimer->Stop();
        }
        if (_gamepadTimer != nullptr)
        {
            _gamepadTimer->Stop();
        }
        UserControl::OnDetachedFromVisualTree();
    }

    void StartScreen::OnKeyDown(AvInput::KeyEventArgs& e)
    {
        if (e.Key == AvInput::Key::Escape && _stack.empty())
        {
            AskToQuit();
            e.Handled = true;
            return;
        }
        UserControl::OnKeyDown(e);
    }

    void StartScreen::ResumeLobby()
    {
        if (_lobby != nullptr)
        {
            _lobby->Resume();
        }
    }

    void StartScreen::SuspendLobby()
    {
        if (_lobby != nullptr)
        {
            _lobby->Suspend();
        }
    }

    void StartScreen::Reset()
    {
        if (_lobby != nullptr)
        {
            _lobby->Suspend();
            _lobby.reset();
        }
        _finished = false;
        _plan = {};
        while (!_stack.empty())
        {
            Pop();
        }
        ShowGround(true);
        Launcher::Hunters::Reroll();
        Launcher::LauncherPrefs::Load();
        RefreshRooms();
        RefreshVersionLine();
        if (!Launcher::GameFiles::Ready())
        {
            OpenSetup();
        }
    }

    bool StartScreen::GoBack()
    {
        if (_stack.empty())
        {
            return false;
        }
        if (const std::shared_ptr<LobbyScreen> lobby
            = std::dynamic_pointer_cast<LobbyScreen>(_stack.back()))
        {
            lobby->Leave("");
        }
        else
        {
            Pop();
        }
        return true;
    }

    void StartScreen::ShowGround(bool show)
    {
        if (_groundShown == show)
        {
            return;
        }
        _groundShown = show;
        _dark->IsVisible(!show);
        if (_foot != nullptr)
        {
            _foot->IsVisible(show);
        }
        if (_heartCorner != nullptr)
        {
            _heartCorner->IsVisible(show && _bar != nullptr
                && _bar->Orientation() == Layout::Orientation::Vertical);
        }
        if (show)
        {
            for (std::size_t i = 0; i < _ground.size(); i++)
            {
                _root->Children.Insert(i + 1, _ground[i]);
            }
            return;
        }
        for (const Controls::ControlPtr& layer : _ground)
        {
            _root->Children.Remove(layer);
        }
    }

    void StartScreen::Push(const Controls::ControlPtr& view)
    {
        _stack.push_back(view);
        _overlay->Children.Clear();
        _overlay->Children.Add(view);
        _overlay->IsVisible(true);
        _menu->IsVisible(false);
        _versionBox->IsVisible(false);
        Threading::Dispatcher::UIThread().Post([view] { view->Focus(); },
            Threading::DispatcherPriority::Background);
    }

    void StartScreen::Pop()
    {
        if (!_stack.empty())
        {
            _stack.pop_back();
        }
        _overlay->Children.Clear();
        if (!_stack.empty())
        {
            const Controls::ControlPtr top = _stack.back();
            _overlay->Children.Add(top);
            Threading::Dispatcher::UIThread().Post([top] { top->Focus(); },
                Threading::DispatcherPriority::Background);
            return;
        }
        _overlay->IsVisible(false);
        _menu->IsVisible(true);
        _versionBox->IsVisible(true);
        ShowGround(true);
        RefreshVersionLine();
        const std::shared_ptr<StartScreen> self = Self();
        Threading::Dispatcher::UIThread().Post([self]
        {
            self->Focus();
        }, Threading::DispatcherPriority::Background);
    }

    void StartScreen::Finish(Launcher::LaunchPlan plan)
    {
        if (_finished)
        {
            return;
        }
        _finished = true;
        _plan = std::move(plan);
        Done(*this, _plan);
    }

    void StartScreen::OpenPlay()
    {
        if (!Launcher::GameFiles::Ready())
        {
            OpenSetup();
            return;
        }
        auto view = std::make_shared<PlayScreen>(_settings, _rooms);
        view->Closed += [this](PlayScreen&) { Pop(); };
        view->Launched += [this](PlayScreen&, Launcher::LaunchPlan plan)
        {
            ConnectedOrFinished(std::move(plan));
        };
        view->CreateRequested += [this](PlayScreen&) { OpenCreateServer(); };
        Push(view);
    }

    void StartScreen::OpenCreateServer()
    {
        auto view = std::make_shared<CreateServerScreen>(_rooms,
            std::optional<std::string>(Runtime::RequireReference(_settings).RoomKey));
        view->Closed += [this](CreateServerScreen&) { Pop(); };
        view->Launched += [this](CreateServerScreen&, Launcher::LaunchPlan plan)
        {
            Pop();
            ConnectedOrFinished(std::move(plan));
        };
        Push(view);
    }

    void StartScreen::ConnectedOrFinished(Launcher::LaunchPlan plan)
    {
        if (Network::NetSession::Active() && Network::NetSession::PersistentLobby())
        {
            _lobby = std::make_shared<LobbyScreen>(_rooms, plan.Lobby());
            _lobby->MatchRequested += [this](LobbyScreen&, Launcher::LaunchPlan match)
            {
                MatchRequested(*this, std::move(match));
            };
            _lobby->Closed += [this](LobbyScreen&, std::string reason)
            {
                _lobby.reset();
                Pop();
                if (!_stack.empty())
                {
                    if (const auto play = std::dynamic_pointer_cast<PlayScreen>(_stack.back()))
                    {
                        play->SessionEnded(reason);
                    }
                }
            };
            Push(_lobby);
        }
        else
        {
            Finish(std::move(plan));
        }
    }

    void StartScreen::OpenSettings()
    {
        auto view = std::make_shared<SettingsView>(_settings);
        view->Closed += [this](SettingsView&) { Pop(); };
        view->GameFilesRequested += [this](SettingsView&)
        {
            Pop();
            OpenSetup();
        };
        Push(view);
    }

    void StartScreen::OpenSetup()
    {
        auto view = std::make_shared<SetupScreen>();
        view->Closed += [this](SetupScreen&)
        {
            Pop();
            RefreshRooms();
        };
        Push(view);
    }

    void StartScreen::AskToQuit()
    {
        auto view = std::make_shared<ConfirmScreen>(
            "Quit " + std::string(::MphRead::Mods::Branding::Name) + "?");
        view->Answered += [this](ConfirmScreen&, bool yes)
        {
            Pop();
            if (yes)
            {
                Finish({});
            }
        };
        Push(view);
    }

    void StartScreen::ShowPauseMenu(std::function<void()> onResume,
        std::function<void()> onLeave, std::function<void()> onQuit)
    {
        ShowGround(false);
        auto view = std::make_shared<PauseMenuView>(false);
        view->Resumed += [this, onResume](PauseMenuView&) { Pop(); onResume(); };
        view->LeaveRequested += [this, onLeave](PauseMenuView&) { Pop(); onLeave(); };
        view->QuitRequested += [this, onQuit](PauseMenuView&) { Pop(); onQuit(); };
        view->SpectateRequested += [this, onResume](PauseMenuView&)
        {
            Pop();
            ::MphRead::Mods::SpectatorMode::Start();
            onResume();
        };
        view->RejoinRequested += [this, onResume](PauseMenuView&)
        {
            Pop();
            ::MphRead::Mods::SpectatorMode::Rejoin();
            onResume();
        };
        view->RecordToggleRequested += [this, onResume](PauseMenuView&)
        {
            if (Network::DemoRecorder::IsRecording())
            {
                const std::optional<std::string> path = Network::DemoRecorder::CurrentPath();
                std::cout << "[demo] recording saved to " << path.value_or("") << '\n';
                Network::DemoRecorder::Stop();
            }
            else
            {
                (void)Network::DemoRecorder::Start();
            }
            Pop();
            onResume();
        };
        view->VoteMapRequested += [this](PauseMenuView&) { OpenVote(); };
        view->SettingsRequested += [this](PauseMenuView&)
        {
            auto settings = std::make_shared<SettingsView>(_settings, true);
            settings->Closed += [this](SettingsView&) { Pop(); };
            Push(settings);
        };
        Push(view);
        view->FocusResume();
    }

    void StartScreen::OpenVote()
    {
        const std::string why = Network::MapVote::WhyNotProposing();
        if (!why.empty())
        {
            ::MphRead::Mods::Chat::ChatBox::System(why);
            Pop();
            return;
        }
        std::vector<std::string> rooms = _rooms;
        if (rooms.empty())
        {
            try
            {
                rooms = ::MphRead::Mods::ThumbnailGenerator::MultiplayerRooms();
            }
            catch (const std::exception& exception)
            {
                ::MphRead::Mods::DebugLog::Exception("pause", exception);
                rooms.clear();
            }
        }
        if (rooms.empty())
        {
            ::MphRead::Mods::Chat::ChatBox::System(std::string("no maps to vote for"));
            Pop();
            return;
        }
        auto view = std::make_shared<PlayScreen>(_settings, rooms, PlayScreen::Face::Vote, true);
        view->Closed += [this](PlayScreen&) { Pop(); };
        view->Voted += [this](PlayScreen&, std::string room)
        {
            Network::MapVote::Propose(room);
            Pop();
            Pop();
        };
        Push(view);
    }

    void StartScreen::CatchUpPreviews()
    {
        try
        {
            if (!Launcher::GameFiles::Ready() || !::MphRead::Mods::ThumbnailHost::CanRender()
                || ::MphRead::Mods::ThumbnailGenerator::MissingThumbnails().empty())
            {
                return;
            }
            _previewTask = ::MphRead::Mods::ThumbnailHost::RenderMissingAsync(
                [](const std::string&) {});
        }
        catch (...)
        {
            // The C# fire-and-forget task keeps failures on its task rather
            // than throwing through the screen constructor.
        }
    }

    void StartScreen::RefreshRooms()
    {
        if (!Launcher::GameFiles::Ready())
        {
            return;
        }
        _rooms.clear();
        for (const std::string& room : ::MphRead::Mods::ThumbnailGenerator::MultiplayerRooms())
        {
            _rooms.push_back(room);
        }
    }

    std::string StartScreen::VersionNumber()
    {
        const std::optional<Update::Version>& current = Update::BuildVersion::Current();
        return current.has_value() ? current->ToString(3) : "a local build";
    }

    void StartScreen::Say(std::string text, Media::Color colour, bool pressable)
    {
        _version->Text(std::move(text));
        _version->Foreground(std::make_shared<Media::SolidColorBrush>(colour));
        _updatable = pressable;
        _versionBox->Cursor(std::make_shared<AvInput::Cursor>(pressable
            ? AvInput::StandardCursorType::Hand : AvInput::StandardCursorType::Arrow));
    }

    void StartScreen::RefreshVersionLine()
    {
        if (_updating)
        {
            return;
        }
        const std::string number = VersionNumber();
        if (Update::Updater::Available().has_value())
        {
            Say(number + " -- update available, click here", GuiTheme::Warm, true);
            return;
        }
        Say(number, Update::BuildVersion::IsRelease() && Update::Updater::Checked()
            ? GuiTheme::Good : GuiTheme::TextDim);
    }

    void StartScreen::UpdateNow()
    {
        const std::optional<Update::UpdateInfo> found = Update::Updater::Available();
        if (!found.has_value())
        {
            return;
        }
        const Update::UpdateInfo update = *found;
        if (Update::UpdateInstall::CanInstall(update))
        {
            const std::shared_ptr<Update::IUpdateInstaller> installer
                = Update::UpdateInstall::Current();
            if (installer == nullptr)
            {
                return;
            }
            FetchAndInstall(update, installer);
            return;
        }
        if (!Update::Updater::OpenPage(update))
        {
            Say(update.PageUrl.Get().value_or(""), GuiTheme::Warm);
        }
    }

    void StartScreen::FetchAndInstall(Update::UpdateInfo update,
        const std::shared_ptr<Update::IUpdateInstaller>& installer)
    {
        if (_updating)
        {
            return;
        }
        const std::string number = VersionNumber();
        if (!installer->Allowed())
        {
            Say(number + " -- allow installs from this app, then press again", GuiTheme::Warm, true);
            (void)installer->RequestPermission();
            return;
        }
        _updating = true;
        const std::shared_ptr<StartScreen> self = Self();
        installer->Finished([self, number](bool ok, std::string message)
        {
            Threading::Dispatcher::UIThread().Post([self, number, ok, message = std::move(message)]
            {
                self->_updating = false;
                self->Say(ok ? number : number + " -- " + message,
                    ok ? GuiTheme::TextDim : GuiTheme::Warm, !ok);
            });
        });
        const std::string label = update.AssetName.Get().value_or("").empty()
            ? update.Tag.Get().value_or("") : update.AssetName.Get().value_or("");
        Say(number + " -- downloading " + label + "...", GuiTheme::Warm);

        struct ProgressState final
        {
            std::mutex Mutex;
            std::int32_t Shown = -1;
        };
        const auto reported = std::make_shared<ProgressState>();
        const std::function<void(float)> progress = [self, number, label, reported](float fraction)
        {
            const std::int32_t percent = fraction < 0 ? -1 : static_cast<std::int32_t>(fraction * 100);
            {
                std::lock_guard lock(reported->Mutex);
                if (percent == reported->Shown)
                {
                    return;
                }
                reported->Shown = percent;
            }
            Threading::Dispatcher::UIThread().Post([self, number, label, percent]
            {
                self->Say(percent < 0
                    ? number + " -- downloading " + label + "..."
                    : number + " -- downloading " + label + "... " + std::to_string(percent) + "%",
                    GuiTheme::Warm);
            });
        };

        try
        {
            Runtime::TaskRun([self, installer, update = std::move(update), progress, number, label]
            {
                std::string error;
                bool ready = false;
                try
                {
                    ready = installer->Prepare(update, progress, error);
                }
                catch (...)
                {
                    return;
                }
                Threading::Dispatcher::UIThread().Post([self, installer, number, label, ready,
                    error = std::move(error)]() mutable
                {
                    if (!ready)
                    {
                        self->_updating = false;
                        self->Say(number + " -- " + (error.empty() ? "the download failed" : error),
                            GuiTheme::Warm, true);
                        return;
                    }
                    try
                    {
                        self->Say(installer->ExitAfterInstall()
                            ? number + " -- restarting to finish..."
                            : number + " -- waiting for the system installer...", GuiTheme::Warm);
                        if (!installer->Install(error))
                        {
                            self->_updating = false;
                            self->Say(number + " -- " + (error.empty()
                                ? "the install could not be started" : error), GuiTheme::Warm, true);
                            return;
                        }
                        if (installer->ExitAfterInstall())
                        {
                            self->Finish({});
                        }
                    }
                    catch (...)
                    {
                        // FetchAndInstall is fire-and-forget in C#: an async
                        // exception faults its task and does not escape here.
                    }
                });
            });
        }
        catch (...)
        {
            // Task.Run failure faults the C# async method's task.
        }
    }
}
