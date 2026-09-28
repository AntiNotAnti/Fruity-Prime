#include "PlayScreen.hpp"

#include "Deck.hpp"
#include "DeckButton.hpp"
#include "DeckCard.hpp"
#include "DeckChip.hpp"
#include "DeckField.hpp"
#include "DeckSide.hpp"
#include "DeckTile.hpp"
#include "GuiTheme.hpp"
#include "HunterStand.hpp"
#include "MapCardPicker.hpp"
#include "MapShot.hpp"
#include "Rows.hpp"
#include "ServerRow.hpp"
#include "UiLayout.hpp"
#include "UiList.hpp"
#include "UiMark.hpp"
#include "UiTabs.hpp"
#include "../../Branding.hpp"
#include "../../HunterSuits.hpp"
#include "../../Network/DemoFile.hpp"
#include "../../Network/DemoLibrary.hpp"
#include "../../Network/DemoPlayback.hpp"
#include "../../Network/NetLaunch.hpp"
#include "../../Network/NetMaster.hpp"
#include "../../Network/NetSession.hpp"
#include "../../Network/NetStatus.hpp"
#include "../Portable/AdventureSave.hpp"
#include "../Portable/GameFiles.hpp"
#include "../Portable/LauncherPrefs.hpp"
#include "../Portable/NativeFilePicker.hpp"
#include "../../../Entities/Players/PlayerEntity.hpp"
#include "../../../Menu.hpp"
#include "../../../Metadata/Metadata.hpp"
#include "../../../Metadata/Rooms.hpp"
#include "../../../Mods/ThumbnailGenerator.hpp"
#include "../../../NativeRuntime/Avalonia/Threading.hpp"
#include "../../../NativeRuntime/System/Globalization.hpp"
#include "../../../NativeRuntime/System/IO.hpp"
#include "../../../NativeRuntime/System/Number.hpp"
#include "../../../NativeRuntime/System/Tasks.hpp"

#include <algorithm>
#include <any>
#include <chrono>
#include <cmath>
#include <exception>
#include <future>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Runtime = ::MphRead::NativeRuntime;
    namespace Network = ::MphRead::Mods::Network;
    namespace Threading = ::MphRead::NativeRuntime::Avalonia::Threading;
    using namespace ::MphRead::NativeRuntime::Avalonia;

    namespace
    {
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

        [[nodiscard]] Av::Media::IBrushPtr Solid(Av::Media::Color colour)
        {
            return std::make_shared<Av::Media::SolidColorBrush>(colour);
        }

        [[nodiscard]] std::string DemoExtension()
        {
            std::string extension(Network::DemoFile::Extension);
            if (!extension.empty() && extension.front() == '.')
            {
                extension.erase(extension.begin());
            }
            return extension;
        }
    }

    const std::vector<PlayScreen::Mode> PlayScreen::_modes{
        {"Battle", GameMode::Battle},
        {"Battle teams", GameMode::BattleTeams},
        {"Survival", GameMode::Survival},
        {"Survival teams", GameMode::SurvivalTeams},
        {"Capture", GameMode::Capture},
        {"Bounty", GameMode::Bounty},
        {"Bounty teams", GameMode::BountyTeams},
        {"Defender", GameMode::Defender},
        {"Defender teams", GameMode::DefenderTeams},
        {"Nodes", GameMode::Nodes},
        {"Nodes teams", GameMode::NodesTeams},
        {"Prime hunter", GameMode::PrimeHunter}
    };

    const std::vector<std::string> PlayScreen::_hunters{
        ::MphRead::ToString(Hunter::Samus),
        ::MphRead::ToString(Hunter::Kanden),
        ::MphRead::ToString(Hunter::Trace),
        ::MphRead::ToString(Hunter::Sylux),
        ::MphRead::ToString(Hunter::Noxus),
        ::MphRead::ToString(Hunter::Spire),
        ::MphRead::ToString(Hunter::Weavel),
        ::MphRead::ToString(Hunter::Random)
    };

    const PlayScreen::ImportChoice PlayScreen::_import{};
    std::optional<std::vector<PlayScreen::SampleServer>> PlayScreen::Sample{};

    class PlayScreen::BodyGrid final : public Controls::Grid
    {
    protected:
        Size MeasureOverride(Size availableSize) override
        {
            const double em = Deck::GetEm(*this);
            const double gap = Runtime::MathRoundToInt32(em * 0.6);
            if (std::abs(gap - RowSpacing()) > 0.01)
            {
                RowSpacing(gap);
            }
            const Thickness want(0, 0, Runtime::MathRoundToInt32(em * 0.3), 0);
            if (Margin() != want)
            {
                Margin(want);
            }
            return Grid::MeasureOverride(availableSize);
        }
    };

    class PlayScreen::BarRow final : public Controls::DockPanel
    {
    protected:
        Size MeasureOverride(Size availableSize) override
        {
            const double gap = Runtime::MathRoundToInt32(Deck::GetEm(*this) * 0.45);
            for (const Controls::ControlPtr& child : Children)
            {
                const Controls::Dock dock = Controls::DockPanel::GetDock(*child);
                Thickness want(0);
                if (dock == Controls::Dock::Left)
                {
                    want = Thickness(0, 0, gap, 0);
                }
                else if (dock == Controls::Dock::Right)
                {
                    want = Thickness(gap, 0, 0, 0);
                }
                if (child->Margin() != want)
                {
                    child->Margin(want);
                }
            }
            return DockPanel::MeasureOverride(availableSize);
        }
    };

    PlayScreen::PlayScreen(const std::shared_ptr<::MphRead::MenuSettings>& settings,
        const std::vector<std::string>& rooms, Face face, bool overGame)
        : _settings(settings),
          _rooms(rooms),
          _overGame(overGame),
          _only(face),
          _list(std::make_shared<UiList>()),
          _options(std::make_shared<Controls::StackPanel>()),
          _preview(std::make_shared<Controls::Image>()),
          _previewBox(std::make_shared<Controls::Border>()),
          _stand(std::make_shared<HunterStand>()),
          _note(std::make_shared<Note>("")),
          _go(std::make_shared<UiMark>(UiMark::Shape::Accept, "play")),
          _back(std::make_shared<UiMark>(UiMark::Shape::Cancel, "back")),
          _createLobby(std::make_shared<UiMark>(UiMark::Shape::Add, "create lobby"))
    {
        Background(Media::Brushes::Transparent());
        Focusable(true);
        _options->Spacing(2);
        _options->Width(300);

        _preview->Stretch(Media::Stretch::UniformToFill);
        _previewBox->Height(172);
        _previewBox->CornerRadius(Av::CornerRadius(3));
        _previewBox->ClipToBounds(true);
        _previewBox->IsVisible(false);
        _previewBox->Child(_preview);

        _stand->Width(96);
        _stand->Height(150);
        _stand->IsVisible(false);
        _stand->IsHitTestVisible(false);
        _stand->HorizontalAlignment(Layout::HorizontalAlignment::Right);
        _stand->VerticalAlignment(Layout::VerticalAlignment::Bottom);
        _stand->Name2(_hunters[0]);

        auto body = std::make_shared<BodyGrid>();
        body->ColumnDefinitions(Controls::ColumnDefinitions("*,Auto"));
        body->RowDefinitions(Controls::RowDefinitions("Auto,*"));
        _body = body;
        _previewBox->Margin(Thickness(0, 0, 24, 14));
        Controls::Grid::SetColumn(*_previewBox, 0);
        Controls::Grid::SetRow(*_previewBox, 0);
        body->Children.Add(_previewBox);
        Controls::Grid::SetColumn(*_stand, 0);
        Controls::Grid::SetRow(*_stand, 0);
        body->Children.Add(_stand);

        _side = std::make_shared<Controls::ScrollViewer>();
        _side->Content(_options);
        _side->MaxHeight(190);
        _side->HorizontalScrollBarVisibility(Controls::ScrollBarVisibility::Disabled);
        _side->VerticalScrollBarVisibility(Controls::ScrollBarVisibility::Auto);
        Controls::Grid::SetColumn(*_side, 1);
        Controls::Grid::SetRow(*_side, 0);
        body->Children.Add(_side);

        Controls::Grid::SetColumn(*_list, 0);
        Controls::Grid::SetColumnSpan(*_list, 2);
        Controls::Grid::SetRow(*_list, 1);
        body->Children.Add(_list);

        _grid = std::make_shared<DeckGrid>();
        _gridScroll = std::make_shared<Controls::ScrollViewer>();
        _gridScroll->Content(_grid);
        _gridScroll->IsVisible(false);
        _gridScroll->ClipToBounds(true);
        _gridScroll->HorizontalScrollBarVisibility(Controls::ScrollBarVisibility::Disabled);
        _gridScroll->VerticalScrollBarVisibility(Controls::ScrollBarVisibility::Auto);
        Controls::Grid::SetColumn(*_gridScroll, 0);
        Controls::Grid::SetColumnSpan(*_gridScroll, 2);
        Controls::Grid::SetRow(*_gridScroll, 1);
        body->Children.Add(_gridScroll);

        _back->Click += [this](UiMark&) { Leave(); };
        _go->Click += [this](UiMark&) { Go(); };
        _createLobby->IsVisible(face == Face::Online);
        _createLobby->Click += [this](UiMark&) { CreateRequested(*this); };

        if (face != Face::Vote)
        {
            _tabs = std::make_shared<UiTabs>(
                std::vector<std::string>{"Online", "Offline", "Story", "Clips"}, static_cast<std::int32_t>(face));
            _tabs->Changed += [this](UiTabs&) { Rebuild(); };
        }
        const std::shared_ptr<Controls::Panel> page = UiLayout::Page(_overGame,
            UiLayout::WellPlay, face == Face::Vote ? "vote" : "play", _tabs, body,
            _back, _go, false, _createLobby, _note);
        _sidePanel = BuildSidePanel();
        page->Children.Add(_sidePanel);
        Content(page);

        _list->Activated += [this](UiList&, const Controls::ControlPtr&) { Go(); };
        _list->SelectionChanged += [this](UiList&, const Controls::ControlPtr& selected)
        {
            RefreshPreview();
            RefreshStory();
            RefreshGoLabel();
            if (Current() == Face::Online)
            {
                if (const auto row = std::dynamic_pointer_cast<ServerRow>(selected))
                {
                    OpenServerSide(row);
                }
            }
        };

        AttachedToVisualTree += [this](Controls::Control&)
        {
            const std::shared_ptr<PlayScreen> self = Self();
            if (self->Current() == Face::Online)
            {
                Threading::Dispatcher::UIThread().Post([self]
                {
                    self->_back->Focus();
                });
            }
            else
            {
                self->_list->FocusFirst();
            }
        };
        DetachedFromVisualTree += [this](Controls::Control&) { StopPolling(); };
        Rebuild();
    }

    std::shared_ptr<PlayScreen> PlayScreen::Self()
    {
        return std::static_pointer_cast<PlayScreen>(shared_from_this());
    }

    PlayScreen::Face PlayScreen::Current() const noexcept
    {
        return _tabs == nullptr ? _only : static_cast<Face>(_tabs->Index());
    }

    Size PlayScreen::MeasureOverride(Size availableSize)
    {
        if (std::isfinite(availableSize.Height) && availableSize.Height > 0)
        {
            SetCompact(availableSize.Height < UiLayout::ShortBox);
        }
        return UserControl::MeasureOverride(availableSize);
    }

    void PlayScreen::SetCompact(bool compact)
    {
        if (compact == _compact)
        {
            return;
        }
        _compact = compact;
        _previewBox->IsVisible(_previewWanted);
        _stand->IsVisible(_standWanted && _previewWanted);
        _stand->Height(compact ? 110 : 150);
        if (_bar != nullptr)
        {
            return;
        }
        if (compact)
        {
            _side->MaxHeight(std::numeric_limits<double>::infinity());
            Controls::Grid::SetRow(*_side, 0);
            Controls::Grid::SetRowSpan(*_side, 1);
            _side->Margin(Thickness(18, 0, 0, 0));
            Controls::Grid::SetColumn(*_previewBox, 1);
            Controls::Grid::SetRow(*_previewBox, 1);
            Controls::Grid::SetColumn(*_stand, 1);
            Controls::Grid::SetRow(*_stand, 1);
            _previewBox->Height(std::numeric_limits<double>::quiet_NaN());
            _previewBox->MaxHeight(150);
            _previewBox->VerticalAlignment(Layout::VerticalAlignment::Stretch);
            _previewBox->Margin(Thickness(18, 12, 0, 0));
            _stand->Margin(_previewBox->Margin());
            Controls::Grid::SetRow(*_list, 0);
            Controls::Grid::SetRowSpan(*_list, 2);
            Controls::Grid::SetColumnSpan(*_list, 1);
        }
        else
        {
            _side->MaxHeight(190);
            Controls::Grid::SetRow(*_side, 0);
            Controls::Grid::SetRowSpan(*_side, 1);
            _side->Margin(Thickness(0));
            Controls::Grid::SetColumn(*_stand, 0);
            Controls::Grid::SetRow(*_stand, 0);
            Controls::Grid::SetColumn(*_previewBox, 0);
            Controls::Grid::SetRow(*_previewBox, 0);
            _previewBox->Height(172);
            _previewBox->MaxHeight(std::numeric_limits<double>::infinity());
            _previewBox->VerticalAlignment(Layout::VerticalAlignment::Stretch);
            _previewBox->Margin(Thickness(0, 0, 24, 14));
            _stand->Margin(_previewBox->Margin());
            Controls::Grid::SetRow(*_list, 1);
            Controls::Grid::SetRowSpan(*_list, 1);
            Controls::Grid::SetColumnSpan(*_list, 2);
        }
    }

    void PlayScreen::ClearFaceExtras()
    {
        if (_bar != nullptr)
        {
            _body->Children.Remove(_bar);
            _bar.reset();
        }
        _side->IsVisible(true);
    }

    void PlayScreen::WantPreview(bool wanted)
    {
        _previewWanted = wanted;
        _previewBox->IsVisible(wanted);
        _stand->IsVisible(_standWanted && wanted);
    }

    std::shared_ptr<DeckSide> PlayScreen::BuildSidePanel()
    {
        _sideCode = std::make_shared<DeckChip>("", "");
        _sideNameText = std::make_shared<Controls::TextBlock>();
        _sideNameText->FontFamily(GuiTheme::Display());
        _sideNameText->FontSize(17);
        _sideNameText->Foreground(GuiTheme::TextBrush);
        _sideNameText->TextTrimming(Media::TextTrimming::CharacterEllipsis);
        _sideNameText->VerticalAlignment(Layout::VerticalAlignment::Center);
        _sideAddr = std::make_shared<Controls::TextBlock>();
        _sideAddr->FontFamily(GuiTheme::Display());
        _sideAddr->FontSize(11);
        _sideAddr->Foreground(GuiTheme::TextDimBrush);
        _sideAddr->TextWrapping(Media::TextWrapping::Wrap);

        auto close = std::make_shared<DeckButton>("x", Deck::Face::Slate(), 1, 0.6, 0.3, 2);
        close->Click += [this](DeckButton&) { CloseSide(); };
        auto headLine = std::make_shared<Controls::Grid>();
        headLine->ColumnDefinitions(Controls::ColumnDefinitions("Auto,*,Auto"));
        Controls::Grid::SetColumn(*_sideCode, 0);
        headLine->Children.Add(_sideCode);
        _sideNameText->Margin(Thickness(8, 0, 8, 0));
        Controls::Grid::SetColumn(*_sideNameText, 1);
        headLine->Children.Add(_sideNameText);
        Controls::Grid::SetColumn(*close, 2);
        headLine->Children.Add(close);

        _sideStand = std::make_shared<HunterStand>();
        _sideStand->Height(150);
        _sideStand->HorizontalAlignment(Layout::HorizontalAlignment::Stretch);
        _sideStand->Name2(_hunters[0]);
        _sideFacts = std::make_shared<Controls::StackPanel>();
        _sideFacts->Spacing(4);
        auto bodyStack = std::make_shared<Controls::StackPanel>();
        bodyStack->Spacing(8);
        bodyStack->Children.Add(_sideStand);
        bodyStack->Children.Add(_sideFacts);
        auto scroll = std::make_shared<Controls::ScrollViewer>();
        scroll->Content(bodyStack);
        scroll->HorizontalScrollBarVisibility(Controls::ScrollBarVisibility::Disabled);
        scroll->VerticalScrollBarVisibility(Controls::ScrollBarVisibility::Auto);

        auto back = std::make_shared<DeckButton>("Back", Deck::Face::Brass(), 1.1, 0.9, 0.5, 5);
        back->Click += [this](DeckButton&) { CloseSide(); };
        _sideGo = std::make_shared<DeckButton>("START", Deck::Face::Moss(), 1.4, 1, 0.45, 6);
        _sideGo->Click += [this](DeckButton&) { Go(); };
        auto foot = std::make_shared<Controls::Grid>();
        foot->ColumnDefinitions(Controls::ColumnDefinitions("Auto,*"));
        Controls::Grid::SetColumn(*back, 0);
        foot->Children.Add(back);
        _sideGo->Margin(Thickness(6, 0, 0, 0));
        _sideGo->HorizontalAlignment(Layout::HorizontalAlignment::Stretch);
        Controls::Grid::SetColumn(*_sideGo, 1);
        foot->Children.Add(_sideGo);

        auto stack = std::make_shared<Controls::Grid>();
        stack->RowDefinitions(Controls::RowDefinitions("Auto,*,Auto"));
        stack->RowSpacing(8);
        auto head = std::make_shared<Controls::StackPanel>();
        head->Spacing(3);
        head->Children.Add(headLine);
        head->Children.Add(_sideAddr);
        Controls::Grid::SetRow(*head, 0);
        stack->Children.Add(head);
        Controls::Grid::SetRow(*scroll, 1);
        stack->Children.Add(scroll);
        Controls::Grid::SetRow(*foot, 2);
        stack->Children.Add(foot);

        auto card = std::make_shared<DeckCard>();
        card->Child(stack);
        card->MaxWidthEms = 17.5;
        auto side = std::make_shared<DeckSide>();
        side->Child(card);
        side->Margin(Thickness(0, 14, 12, 14));
        return side;
    }

    void PlayScreen::Fact(const std::string& key, const std::string& value,
        const Media::IBrushPtr& tint)
    {
        auto row = std::make_shared<Controls::Grid>();
        row->ColumnDefinitions(Controls::ColumnDefinitions("*,Auto"));
        row->Height(26);
        auto k = std::make_shared<Controls::TextBlock>();
        k->Text(Runtime::ToUpperInvariant(key));
        k->FontFamily(GuiTheme::Display());
        k->FontSize(10);
        k->Foreground(GuiTheme::TextDimBrush);
        k->VerticalAlignment(Layout::VerticalAlignment::Center);
        auto v = std::make_shared<Controls::TextBlock>();
        v->Text(value);
        v->FontFamily(GuiTheme::Display());
        v->FontSize(12);
        v->Foreground(tint != nullptr ? tint : GuiTheme::TextBrush);
        v->TextTrimming(Media::TextTrimming::CharacterEllipsis);
        v->VerticalAlignment(Layout::VerticalAlignment::Center);
        Controls::Grid::SetColumn(*k, 0);
        row->Children.Add(k);
        Controls::Grid::SetColumn(*v, 1);
        row->Children.Add(v);
        _sideFacts->Children.Add(row);
    }

    void PlayScreen::OpenServerSide(const std::shared_ptr<ServerRow>& row)
    {
        if (row == nullptr || !row->IsLive())
        {
            return;
        }
        _sideCode->Text(row->ModeName().empty() ? "server" : row->ModeName());
        _sideNameText->Text(row->DisplayName());
        _sideAddr->Text(row->Endpoint());
        _sideFacts->Children.Clear();
        Fact("Map", row->MapName());
        Fact("Mode", row->ModeName());
        Fact("Players", row->PlayerCount());
        Fact("Ping", row->PingText() + " ms", row->PingBrush());
        if (_hunter != nullptr)
        {
            _sideFacts->Children.Add(_hunter);
        }
        if (_suit != nullptr)
        {
            _sideFacts->Children.Add(_suit);
        }
        _sideGo->Text("JOIN");
        RefreshSideHunter();
        _sidePanel->Open(true);
    }

    void PlayScreen::OpenSide(const std::shared_ptr<DeckTile>& tile)
    {
        _picked = tile->RoomKey;
        for (const Controls::ControlPtr& child : _grid->Children)
        {
            if (const auto other = std::dynamic_pointer_cast<DeckTile>(child))
            {
                other->Chosen(other->RoomKey == tile->RoomKey);
            }
        }
        _sideCode->Text(tile->Code);
        const auto [meta, ignored] = ::MphRead::Metadata::GetRoomByName(tile->RoomKey);
        (void)ignored;
        _sideNameText->Text(meta != nullptr && meta->InGameName.has_value()
            ? *meta->InGameName : tile->RoomKey);
        _sideAddr->Text("offline \u2014 bots on this machine");
        _sideFacts->Children.Clear();
        for (const Controls::ControlPtr& row : _offlineRows)
        {
            _sideFacts->Children.Add(row);
        }
        _sideGo->Text("START");
        _sidePanel->Open(true);
        _note->Text("");
    }

    void PlayScreen::CastVote(const std::shared_ptr<DeckTile>& tile)
    {
        const bool taking = !tile->Chosen();
        for (const Controls::ControlPtr& child : _grid->Children)
        {
            if (const auto other = std::dynamic_pointer_cast<DeckTile>(child); other != nullptr && other->Chosen())
            {
                other->Chosen(false);
                other->Tally = std::max(0, other->Tally - 1);
            }
        }
        if (taking)
        {
            tile->Chosen(true);
            tile->Tally = std::max(0, tile->Tally) + 1;
            _picked = tile->RoomKey;
        }
        else
        {
            _picked.reset();
        }
        RefreshBallot();
    }

    void PlayScreen::CloseSide()
    {
        _sidePanel->Open(false);
    }

    void PlayScreen::RefreshGoLabel()
    {
        if (Current() != Face::Online)
        {
            return;
        }
        _go->Label("join");
        _go->IsEnabled(std::dynamic_pointer_cast<ServerRow>(_list->Selected()) != nullptr);
        _createLobby->IsVisible(true);
    }

    void PlayScreen::OnKeyDown(Av::Input::KeyEventArgs& e)
    {
        if (e.Key == Av::Input::Key::Escape)
        {
            Leave();
            e.Handled = true;
            return;
        }
        if (_list->HandleKey(e.Key))
        {
            e.Handled = true;
            return;
        }
        if (_tabs != nullptr && _tabs->HandleKey(e.Key))
        {
            e.Handled = true;
            return;
        }
        UserControl::OnKeyDown(e);
    }

    void PlayScreen::StartPolling()
    {
        QueryStatusSoon();
        _statusTimer = std::make_shared<Threading::DispatcherTimer>(
            Threading::TimeSpan(4.0), Threading::DispatcherPriority::Background,
            [this] { QueryStatusSoon(); });
        _statusTimer->Start();
    }

    void PlayScreen::StopPolling()
    {
        if (_statusTimer != nullptr)
        {
            _statusTimer->Stop();
            _statusTimer.reset();
        }
        if (_statusCancel != nullptr)
        {
            _statusCancel->request_stop();
            _statusCancel.reset();
        }
    }

    void PlayScreen::Leave()
    {
        if (_finished)
        {
            return;
        }
        const std::shared_ptr<PlayScreen> keepAlive = Self();
        _finished = true;
        StopPolling();
        Closed(*keepAlive);
    }

    void PlayScreen::Finish(Launcher::LaunchPlan plan)
    {
        if (_finished)
        {
            return;
        }
        const std::shared_ptr<PlayScreen> keepAlive = Self();
        _finished = true;
        StopPolling();
        Launched(*keepAlive, std::move(plan));
    }

    void PlayScreen::Rebuild()
    {
        StopPolling();
        _list->Clear();
        _list->SetHeader(nullptr);
        switch (Current())
        {
        case Face::Online: _go->Label("join"); break;
        case Face::Clips: _go->Label("watch"); break;
        case Face::Vote: _go->Label("vote"); break;
        default: _go->Label("start"); break;
        }
        _go->IsEnabled(true);
        _createLobby->IsVisible(Current() == Face::Online);
        _options->Children.Clear();
        ClearFaceExtras();
        _note->Text("");
        _note->Foreground(GuiTheme::TextDimBrush);
        _hunter = _mode = _bots = _skill = _resume = nullptr;
        _suit.reset();
        _name = _address = nullptr;
        _standWanted = false;
        WantPreview(false);

        const bool offline = Current() == Face::Offline || Current() == Face::Vote;
        _gridScroll->IsVisible(offline);
        _list->IsVisible(!offline);
        if (_sidePanel != nullptr && !offline)
        {
            _sidePanel->Open(false);
        }
        _offlineRows.clear();
        if (_sideFacts != nullptr)
        {
            _sideFacts->Children.Clear();
        }

        switch (Current())
        {
        case Face::Online: BuildOnline(); break;
        case Face::Offline: BuildOffline(); break;
        case Face::Story: BuildStory(); break;
        case Face::Clips: BuildDemo(); break;
        case Face::Vote: BuildVote(); break;
        }
        RefreshPreview();
        if (Current() != Face::Online)
        {
            _list->FocusFirst();
        }
        RefreshGoLabel();
    }

    void PlayScreen::MakeHunterRows()
    {
        const std::string previous = ::MphRead::ToString(Launcher::LauncherPrefs::LastHunter());
        const auto found = std::find(_hunters.begin(), _hunters.end(), previous);
        const std::int32_t hunterIndex = std::max(0, static_cast<std::int32_t>(found - _hunters.begin()));
        _hunter = std::make_shared<ChoiceRow>("Hunter", _hunters, hunterIndex);
        _suit = std::make_shared<ChoiceRow>("Suit", std::vector<std::string>{"1", "2", "3", "4"},
            std::clamp(Launcher::LauncherPrefs::LastColor(), 0, 3));
        _suit->Preview([this](Media::DrawingContext& context, Rect area)
        {
            context.DrawRectangle(Solid(SuitColour()), nullptr, RoundedRect(area, 3));
        });
        _hunter->Changed += [this](ChoiceRow&) { RefreshSideHunter(); };
        _suit->Changed += [this](ChoiceRow&) { RefreshSideHunter(); };
        RefreshSideHunter();
    }

    std::shared_ptr<ChoiceRow> PlayScreen::AddHunter()
    {
        const std::string previous = ::MphRead::ToString(Launcher::LauncherPrefs::LastHunter());
        const auto found = std::find(_hunters.begin(), _hunters.end(), previous);
        const std::int32_t index = std::max(0, static_cast<std::int32_t>(found - _hunters.begin()));
        auto row = std::make_shared<ChoiceRow>("Hunter", _hunters, index);
        _options->Children.Add(row);
        _standWanted = true;
        _stand->IsVisible(_previewBox->IsVisible());
        _stand->Margin(_previewBox->Margin());
        _stand->Name2(_hunters[static_cast<std::size_t>(row->Index())]);
        row->Changed += [this, weak = std::weak_ptr<ChoiceRow>(row)](ChoiceRow&)
        {
            if (const auto held = weak.lock())
            {
                _stand->Name2(_hunters[static_cast<std::size_t>(held->Index())]);
            }
        };
        return row;
    }

    std::string PlayScreen::PlayerName()
    {
        std::string name = Runtime::StringTrim(Launcher::LauncherPrefs::PlayerName());
        return name.empty() ? "Player" : name;
    }

    void PlayScreen::BuildOnline()
    {
        _name = std::make_shared<DeckField>(PlayerName(), 7);
        _address = std::make_shared<DeckField>(
            Launcher::LauncherPrefs::ServerAddress() + ":" + std::to_string(Launcher::LauncherPrefs::ServerPort()),
            0, "host:port \u2014 or pick a row");
        _address->Box->LostFocus += [this](Av::Input::InputElement&,
            Av::Input::FocusChangedEventArgs&)
        {
            QueryStatusSoon();
        };
        WantPreview(false);
        _list->SpacingEms = 0.32;
        _list->AutoSelectFirst = false;
        MakeHunterRows();

        auto refresh = std::make_shared<DeckButton>("Refresh", Deck::Face::Slate(), 0.95, 0.8, 0.4, 3);
        refresh->Click += [this](DeckButton&) { ReloadServers(); };
        auto bar = std::make_shared<BarRow>();
        Controls::DockPanel::SetDock(*_name, Controls::Dock::Left);
        Controls::DockPanel::SetDock(*refresh, Controls::Dock::Right);
        bar->Children.Add(_name);
        bar->Children.Add(refresh);
        bar->Children.Add(_address);
        _bar = bar;
        Controls::Grid::SetColumn(*_bar, 0);
        Controls::Grid::SetColumnSpan(*_bar, 2);
        Controls::Grid::SetRow(*_bar, 0);
        _body->Children.Add(_bar);
        _side->IsVisible(false);
        Controls::Grid::SetRow(*_list, 1);
        Controls::Grid::SetRowSpan(*_list, 1);
        Controls::Grid::SetColumnSpan(*_list, 2);
        ReloadServers();
        StartPolling();
    }

    void PlayScreen::ReloadServers()
    {
        _list->Clear();
        _replied = 0;
        _live = 0;
        if (Sample.has_value())
        {
            for (const SampleServer& sample : *Sample)
            {
                auto row = std::make_shared<ServerRow>(sample.Name, sample.Endpoint);
                _list->Add(row);
                row->SetStatus(sample.Status);
                ++_replied;
                if (sample.Status.Online)
                {
                    ++_live;
                }
            }
            Answered(static_cast<std::int32_t>(Sample->size()));
            return;
        }

        _note->Text("Asking " + Launcher::LauncherPrefs::MasterHost() + "...");
        _note->Foreground(GuiTheme::TextDimBrush);
        const std::weak_ptr<std::uint8_t> lifetime = _lifetime;
        Runtime::TaskRun([this, lifetime]
        {
            const Network::MasterListResult result = Network::NetMasterClient::Query(
                Launcher::LauncherPrefs::MasterHost(), Launcher::LauncherPrefs::MasterPort());
            Threading::Dispatcher::UIThread().Post([this, lifetime, result]
            {
                const std::shared_ptr<std::uint8_t> alive = lifetime.lock();
                if (alive == nullptr || _finished || Current() != Face::Online)
                {
                    return;
                }
                if (!result.Answered)
                {
                    _note->Text("The directory did not answer. It may be down, or UDP "
                        "may not reach it. The address beside the list still works.");
                    _note->Foreground(GuiTheme::WarmBrush);
                    return;
                }
                if (result.Servers == nullptr || result.Servers->empty())
                {
                    _note->Text("The directory is up and has nobody listed.");
                    _note->Foreground(GuiTheme::WarmBrush);
                    return;
                }
                _asked = static_cast<std::int32_t>(result.Servers->size());
                _note->Text("asking " + std::to_string(_asked) + " servers\u2026");
                for (const Network::MasterListing& listing : *result.Servers)
                {
                    AddServerRow(listing);
                }
            });
        });
    }

    void PlayScreen::Answered(std::int32_t asked)
    {
        _asked = asked;
        _note->Foreground(GuiTheme::TextDimBrush);
        _note->Text(_replied < asked
            ? "asking " + std::to_string(asked) + " servers\u2026 " + std::to_string(_replied) + " answered"
            : std::to_string(_live) + " of " + std::to_string(asked) + " answered. Click one to join.");
    }

    void PlayScreen::AddServerRow(const Network::MasterListing& listing)
    {
        const std::string name = listing.ServerName.empty() ? listing.Endpoint() : listing.ServerName;
        auto row = std::make_shared<ServerRow>(name, listing.Endpoint());
        Controls::ToolTip::SetTip(row, listing.Endpoint());
        const std::weak_ptr<PlayScreen> weak = Self();
        _list->Add(row, [weak, address = listing.Address, port = listing.Port](const Controls::ControlPtr&)
        {
            if (const std::shared_ptr<PlayScreen> self = weak.lock(); self != nullptr && self->_address != nullptr)
            {
                self->_address->Value(address + ":" + std::to_string(port));
            }
        });
        const std::shared_ptr<PlayScreen> self = Self();
        Runtime::TaskRun([self, row, address = listing.Address, port = listing.Port]
        {
            const Network::ServerStatus status = Network::NetStatus::Query(address, port, false);
            Threading::Dispatcher::UIThread().Post([self, row, status]
            {
                row->SetStatus(status);
                ++self->_replied;
                if (status.Online)
                {
                    ++self->_live;
                }
                self->Answered(self->_asked);
                if (self->_list->Selected() == row)
                {
                    self->RefreshPreview();
                }
            });
        });
    }

    std::pair<std::string, std::int32_t> PlayScreen::Endpoint() const
    {
        std::string host = Launcher::LauncherPrefs::ServerAddress();
        std::int32_t port = Launcher::LauncherPrefs::ServerPort();
        if (_address != nullptr)
        {
            static_cast<void>(ParseEndpoint(_address->Value(), host, port));
        }
        return {host, port};
    }

    bool PlayScreen::ParseEndpoint(std::string text, std::string& host, std::int32_t& port)
    {
        text = Runtime::StringTrim(text);
        if (text.empty())
        {
            return false;
        }
        const std::size_t colon = text.find_last_of(':');
        if (colon == std::string::npos || colon == 0)
        {
            host = text;
            return true;
        }
        std::int32_t parsed = 0;
        if (!Runtime::Int32TryParseInvariant(std::string_view(text).substr(colon + 1), parsed)
            || parsed < 1 || parsed > 65535)
        {
            return false;
        }
        host = text.substr(0, colon);
        port = parsed;
        return true;
    }

    void PlayScreen::QueryStatusSoon()
    {
        if (Current() != Face::Online)
        {
            return;
        }
        if (_statusCancel != nullptr)
        {
            _statusCancel->request_stop();
        }
        _statusCancel = std::make_shared<std::stop_source>();
        const std::stop_token token = _statusCancel->get_token();
        const auto [host, port] = Endpoint();
        const std::weak_ptr<std::uint8_t> lifetime = _lifetime;
        Runtime::TaskRun([this, lifetime, token, host, port]
        {
            const Network::ServerStatus status = Network::NetStatus::Query(host, port, true);
            if (token.stop_requested())
            {
                return;
            }
            Threading::Dispatcher::UIThread().Post([this, lifetime, token, host, port, status]
            {
                const std::shared_ptr<std::uint8_t> alive = lifetime.lock();
                if (alive == nullptr || token.stop_requested() || _finished || Current() != Face::Online)
                {
                    return;
                }
                if (_asked > 0)
                {
                    return;
                }
                _note->Text(status.Online
                    ? host + ":" + std::to_string(port) + " -- " + Describe(status)
                    : host + ":" + std::to_string(port)
                        + " -- no answer. It may be off, or UDP may be blocked.");
                _note->Foreground(status.Online ? GuiTheme::GoodBrush : GuiTheme::WarmBrush);
            });
        });
    }

    std::string PlayScreen::Describe(const Network::ServerStatus& status)
    {
        const std::string players = status.MaxPlayers > 0
            ? std::to_string(status.Players) + "/" + std::to_string(status.MaxPlayers)
            : std::to_string(status.Players);
        const std::string ping = status.Latency >= 0
            ? std::to_string(status.Latency) + " ms" : "-- ms";
        return status.RoomKey + " (" + Network::NetStatus::ModeName(status.Mode) + ") "
            + players + " players, " + ping;
    }

    void PlayScreen::Join()
    {
        const auto [host, port] = Endpoint();
        const std::string typed = _name != nullptr ? Runtime::StringTrim(_name->Value()) : std::string{};
        const std::string name = typed.empty() ? PlayerName() : typed;
        Hunter hunter{};
        if (!::MphRead::TryParse(_hunter->Value(), false, hunter))
        {
            throw std::invalid_argument("Requested value '" + _hunter->Value() + "' was not found.");
        }
        StopPolling();
        _go->IsEnabled(false);
        _go->Label("joining");
        _note->Text("Connecting to " + host + ":" + std::to_string(port) + "...");
        _note->Foreground(GuiTheme::TextDimBrush);

        Launcher::LauncherPrefs::PlayerName(name);
        Launcher::LauncherPrefs::LastHunter(hunter);
        Launcher::LauncherPrefs::ServerAddress(host);
        Launcher::LauncherPrefs::ServerPort(port);
        Launcher::LauncherPrefs::LastKind(static_cast<std::int32_t>(Launcher::LaunchKind::Online));
        Launcher::LauncherPrefs::Save();

        const std::shared_ptr<PlayScreen> self = Self();
        Runtime::TaskRun([self, host, port, name, hunter]
        {
            const bool joined = Network::NetLaunch::Connect(host, port, name, hunter);
            Threading::Dispatcher::UIThread().Post([self, host, port, name, hunter, joined]
            {
                self->_go->IsEnabled(true);
                self->_go->Label("join");
                if (!joined)
                {
                    Network::NetSession::Stop();
                    self->_note->Text(Network::NetLaunch::LastJoinError());
                    self->_note->Foreground(GuiTheme::BadBrush);
                    self->StartPolling();
                    return;
                }
                Launcher::LaunchPlan::Init init;
                init.Kind = Launcher::LaunchKind::Online;
                init.Hunter = hunter;
                init.PlayerName = name;
                init.RoomKey = "";
                init.Mode = GameMode::Battle;
                init.Port = port;
                self->Finish(Launcher::LaunchPlan(init));
            });
        });
    }

    void PlayScreen::BuildOffline()
    {
        FillMapGrid();
        std::vector<std::string> modes;
        modes.reserve(_modes.size());
        for (const Mode& mode : _modes)
        {
            modes.emplace_back(mode.Label);
        }
        _mode = std::make_shared<ChoiceRow>("Match type", std::move(modes));
        MakeHunterRows();
        std::vector<std::string> bots;
        bots.reserve(static_cast<std::size_t>(Entities::PlayerEntity::SlotCapacity));
        for (std::int32_t i = 0; i < Entities::PlayerEntity::SlotCapacity; ++i)
        {
            bots.push_back(std::to_string(i));
        }
        _bots = std::make_shared<ChoiceRow>("Bots", std::move(bots), Launcher::LauncherPrefs::Bots());
        _skill = std::make_shared<ChoiceRow>("Bot skill",
            std::vector<std::string>{"Easy", "Normal", "Hard", "Insane"}, Launcher::LauncherPrefs::BotLevel());
        _offlineRows.clear();
        _offlineRows.emplace_back(_mode);
        _offlineRows.emplace_back(_hunter);
        _offlineRows.emplace_back(_suit);
        _offlineRows.emplace_back(_bots);
        _offlineRows.emplace_back(_skill);
        RefreshSideHunter();
        WantPreview(false);
    }

    Media::Color PlayScreen::SuitColour() const
    {
        try
        {
            if (_hunter == nullptr)
            {
                return GuiTheme::Accent;
            }
            Hunter which{};
            if (!::MphRead::TryParse(_hunter->Value(), true, which))
            {
                return GuiTheme::Accent;
            }
            const ColorRgba sampled = ::MphRead::Mods::HunterSuits::Color(which, _suit != nullptr ? _suit->Index() : 0);
            return Media::Color::FromRgb(sampled.Red, sampled.Green, sampled.Blue);
        }
        catch (const std::exception&)
        {
            return GuiTheme::Accent;
        }
    }

    void PlayScreen::RefreshSideHunter()
    {
        if (_sideStand != nullptr && _hunter != nullptr)
        {
            _sideStand->Name2(_hunter->Value());
            _sideStand->Suit(_suit != nullptr ? _suit->Index() : 0);
        }
        if (_suit != nullptr)
        {
            _suit->InvalidateVisual();
        }
    }

    void PlayScreen::FillMapGrid()
    {
        if (_grid == nullptr)
        {
            return;
        }
        _grid->Children.Clear();
        if (_rooms.empty())
        {
            _note->Text("No multiplayer rooms were found. Set the game files up from Settings.");
            _note->Foreground(GuiTheme::WarmBrush);
            return;
        }
        for (const std::string& room : _rooms)
        {
            const std::shared_ptr<DeckTile> tile = MapCardFactory::Create(room);
            tile->Chosen(room == _settings->RoomKey);
            const std::weak_ptr<DeckTile> weak = tile;
            tile->Click += [this, weak](DeckTile&)
            {
                if (const auto chosen = weak.lock())
                {
                    if (Current() == Face::Vote)
                    {
                        CastVote(chosen);
                        return;
                    }
                    OpenSide(chosen);
                }
            };
            _grid->Children.Add(tile);
        }
        _picked = _settings->RoomKey;
    }

    void PlayScreen::FillRooms(const std::optional<std::string>& current)
    {
        if (_rooms.empty())
        {
            _list->AddNote("No multiplayer rooms were found. Set the game files up from Settings.", GuiTheme::Warm);
            return;
        }
        for (const std::string& room : _rooms)
        {
            const auto [meta, ignored] = ::MphRead::Metadata::GetRoomByName(room);
            (void)ignored;
            auto row = std::make_shared<UiListRow>(
                meta != nullptr && meta->InGameName.has_value() ? *meta->InGameName : room, room);
            row->Choice = room;
            _list->Add(row);
        }
        if (current.has_value())
        {
            _list->SelectTag(std::any(*current));
        }
    }

    void PlayScreen::StartMatch()
    {
        const std::optional<std::string> roomKey = SelectedRoom();
        if (!roomKey.has_value())
        {
            _note->Text("Pick a map first.");
            _note->Foreground(GuiTheme::WarmBrush);
            return;
        }
        Hunter hunter{};
        if (!::MphRead::TryParse(_hunter->Value(), false, hunter))
        {
            throw std::invalid_argument("Requested value '" + _hunter->Value() + "' was not found.");
        }
        const GameMode mode = _modes[static_cast<std::size_t>(_mode->Index())].Value;
        _settings->RoomKey = *roomKey;
        Launcher::LauncherPrefs::LastHunter(hunter);
        Launcher::LauncherPrefs::LastColor(_suit != nullptr
            ? _suit->Index() : Launcher::LauncherPrefs::LastColor());
        Launcher::LauncherPrefs::Bots(_bots->Index());
        Launcher::LauncherPrefs::BotLevel(_skill->Index());
        Launcher::LauncherPrefs::LastKind(static_cast<std::int32_t>(Launcher::LaunchKind::Offline));
        Launcher::LauncherPrefs::Save();

        Launcher::LaunchPlan::Init init;
        init.Kind = Launcher::LaunchKind::Offline;
        init.Hunter = hunter;
        init.PlayerName = Launcher::LauncherPrefs::PlayerName();
        init.RoomKey = *roomKey;
        init.Mode = mode;
        init.Bots = _bots->Index();
        init.BotLevel = _skill->Index();
        Finish(Launcher::LaunchPlan(init));
    }

    std::optional<std::string> PlayScreen::SelectedRoom() const
    {
        if (Current() == Face::Offline || Current() == Face::Vote)
        {
            return _picked;
        }
        if (const auto row = std::dynamic_pointer_cast<UiListRow>(_list->Selected()))
        {
            if (const auto choice = std::any_cast<std::string>(&row->Choice))
            {
                return *choice;
            }
        }
        return std::nullopt;
    }

    std::optional<std::string> PlayScreen::PreviewRoom() const
    {
        if (const auto server = std::dynamic_pointer_cast<ServerRow>(_list->Selected()))
        {
            const std::string key = server->RoomKey();
            return key.empty() ? std::nullopt : std::optional<std::string>(key);
        }
        return SelectedRoom();
    }

    void PlayScreen::BuildStory()
    {
        _go->Label("start");
        for (std::uint8_t slot = 1; slot <= Launcher::AdventureSave::SlotCount; ++slot)
        {
            const Launcher::AdventureSave::SlotInfo info = Launcher::AdventureSave::Read(slot);
            auto row = std::make_shared<UiListRow>("Slot " + std::to_string(slot), info.Describe());
            row->Choice = slot;
            _list->Add(row);
        }
        _hunter = AddHunter();
        _resume = std::make_shared<ChoiceRow>("Start", std::vector<std::string>{"Continue", "New game"}, 0);
        _options->Children.Add(_resume);
        RefreshStory();
    }

    void PlayScreen::RefreshStory()
    {
        if (_resume == nullptr || Current() != Face::Story)
        {
            return;
        }
        const bool used = Launcher::AdventureSave::Read(SelectedSlot()).Used;
        _resume->SetItems(used
            ? std::vector<std::string>{"Continue", "New game"}
            : std::vector<std::string>{"New game"});
    }

    std::uint8_t PlayScreen::SelectedSlot() const
    {
        if (const auto row = std::dynamic_pointer_cast<UiListRow>(_list->Selected()))
        {
            if (const auto slot = std::any_cast<std::uint8_t>(&row->Choice))
            {
                return *slot;
            }
        }
        return 1;
    }

    void PlayScreen::StartAdventure()
    {
        const std::uint8_t slot = SelectedSlot();
        const bool used = Launcher::AdventureSave::Read(slot).Used;
        const bool newGame = !used || _resume->Value() == "New game";
        Hunter hunter{};
        if (!::MphRead::TryParse(_hunter->Value(), false, hunter))
        {
            throw std::invalid_argument("Requested value '" + _hunter->Value() + "' was not found.");
        }
        Launcher::LauncherPrefs::LastHunter(hunter);
        Launcher::LauncherPrefs::LastKind(static_cast<std::int32_t>(Launcher::LaunchKind::Adventure));
        Launcher::LauncherPrefs::Save();
        Launcher::LaunchPlan::Init init;
        init.Kind = Launcher::LaunchKind::Adventure;
        init.Hunter = hunter;
        init.PlayerName = Launcher::LauncherPrefs::PlayerName();
        init.RoomKey = "";
        init.SaveSlot = slot;
        init.NewGame = newGame;
        Finish(Launcher::LaunchPlan(init));
    }

    void PlayScreen::BuildDemo()
    {
        _go->Label("watch");
        const std::shared_ptr<const std::vector<Network::DemoRecording>> demos = Network::DemoLibrary::List();
        if (demos != nullptr)
        {
            for (const Network::DemoRecording& demo : *demos)
            {
                auto row = std::make_shared<UiListRow>(
                    demo.Room().empty() ? demo.FileName() : demo.Room(), Network::DemoLibrary::Describe(demo));
                row->Choice = demo.Path();
                _list->Add(row);
            }
        }
        if (demos == nullptr || demos->empty())
        {
            _note->Text("Nothing recorded yet. Clips are made from the pause menu during an online match, "
                "and are written to:\n" + Network::DemoLibrary::Directory());
        }
        auto open = std::make_shared<UiListRow>("Open a file...", "a demo from somewhere else on this device");
        open->Choice = _import;
        _list->Add(open);
    }

    void PlayScreen::PlayDemo()
    {
        const auto row = std::dynamic_pointer_cast<UiListRow>(_list->Selected());
        if (row == nullptr || !row->Choice.has_value())
        {
            return;
        }
        if (std::any_cast<ImportChoice>(&row->Choice) != nullptr)
        {
            ImportDemo();
            return;
        }
        if (const auto path = std::any_cast<std::string>(&row->Choice))
        {
            Watch(*path);
        }
    }

    void PlayScreen::SessionEnded(const std::string& reason)
    {
        _finished = false;
        _note->Text(reason);
        _note->Foreground(GuiTheme::BadBrush);
        StartPolling();
    }

    void PlayScreen::Watch(const std::string& path)
    {
        _go->IsEnabled(false);
        _go->Label("loading");
        const std::shared_ptr<PlayScreen> self = Self();
        Runtime::TaskRun([self, path]
        {
            const bool joined = Network::DemoPlayback::Join(path);
            Threading::Dispatcher::UIThread().Post([self, path, joined]
            {
                self->_go->IsEnabled(true);
                self->_go->Label("watch");
                if (!joined)
                {
                    const std::optional<std::string> error = Network::DemoPlayback::LastError();
                    self->_note->Text(error.value_or("That file could not be read as a demo."));
                    self->_note->Foreground(GuiTheme::BadBrush);
                    return;
                }
                Launcher::LaunchPlan::Init init;
                init.Kind = Launcher::LaunchKind::Demo;
                init.DemoPath = path;
                init.Hunter = Hunter::Samus;
                init.PlayerName = "";
                init.RoomKey = "";
                self->Finish(Launcher::LaunchPlan(init));
            });
        });
    }

    void PlayScreen::ImportDemo()
    {
        if (!Launcher::NativeFilePicker::Available())
        {
            _note->Text("This desktop has no file dialog to open (install zenity or kdialog). Clips in "
                + Network::DemoLibrary::Directory() + " are listed here without one.");
            _note->Foreground(GuiTheme::BadBrush);
            return;
        }
        const std::shared_ptr<PlayScreen> self = Self();
        Await(Launcher::NativeFilePicker::OpenFile("Clips",
            std::string(::MphRead::Mods::Branding::Name) + " demo", DemoExtension()),
            [self](std::optional<std::string> chosen)
            {
                if (chosen.has_value())
                {
                    self->Watch(*chosen);
                }
            });
    }

    void PlayScreen::BuildVote()
    {
        _go->Label("call the vote");
        FillMapGrid();
        const std::optional<Network::MatchStatePacket> match = Network::NetSession::ServerMatch();
        _picked = match.has_value() ? match->RoomKey : std::nullopt;
        for (const Controls::ControlPtr& child : _grid->Children)
        {
            if (const auto tile = std::dynamic_pointer_cast<DeckTile>(child))
            {
                tile->Verb = "Pick";
                tile->ChosenVerb = "Picked";
                tile->Tally = 0;
                tile->Chosen(false);
            }
        }
        RefreshBallot();
        WantPreview(false);
    }

    void PlayScreen::RefreshBallot()
    {
        std::int32_t best = 0;
        for (const Controls::ControlPtr& child : _grid->Children)
        {
            if (const auto tile = std::dynamic_pointer_cast<DeckTile>(child); tile != nullptr && tile->Tally > best)
            {
                best = tile->Tally;
            }
        }
        std::optional<std::string> leader;
        for (const Controls::ControlPtr& child : _grid->Children)
        {
            const auto tile = std::dynamic_pointer_cast<DeckTile>(child);
            if (tile == nullptr)
            {
                continue;
            }
            tile->Leader = best > 0 && tile->Tally == best;
            if (tile->Leader && !leader.has_value())
            {
                const auto [meta, ignored] = ::MphRead::Metadata::GetRoomByName(tile->RoomKey);
                (void)ignored;
                leader = meta != nullptr && meta->InGameName.has_value()
                    ? *meta->InGameName : tile->RoomKey;
            }
            tile->InvalidateVisual();
        }
        _note->Foreground(GuiTheme::TextDimBrush);
        _note->Text(leader.has_value()
            ? *leader + " is leading with " + std::to_string(best) + ". Most votes wins."
            : "Nobody has picked. The rotation decides.");
    }

    void PlayScreen::Go()
    {
        if (_finished)
        {
            return;
        }
        switch (Current())
        {
        case Face::Online:
            if (std::dynamic_pointer_cast<ServerRow>(_list->Selected()) != nullptr)
            {
                Join();
            }
            else
            {
                CreateRequested(*this);
            }
            break;
        case Face::Offline:
            StartMatch();
            break;
        case Face::Story:
            StartAdventure();
            break;
        case Face::Clips:
            PlayDemo();
            break;
        case Face::Vote:
            if (const std::optional<std::string> room = SelectedRoom())
            {
                const std::shared_ptr<PlayScreen> keepAlive = Self();
                _finished = true;
                Voted(*keepAlive, *room);
            }
            break;
        }
    }

    void PlayScreen::RefreshPreview()
    {
        if (!_previewBox->IsVisible())
        {
            return;
        }
        _preview->Source(nullptr);
        const std::optional<std::string> room = PreviewRoom();
        if (!room.has_value())
        {
            return;
        }
        try
        {
            const std::string path = ::MphRead::Mods::ThumbnailGenerator::PathFor(*room);
            if (!Runtime::FileExists(path))
            {
                return;
            }
            _preview->Source(Media::Imaging::Bitmap::FromBytes(Runtime::FileReadAllBytes(path)));
        }
        catch (const std::exception&)
        {
            // An interrupted thumbnail write leaves this frame empty.
        }
    }
}
