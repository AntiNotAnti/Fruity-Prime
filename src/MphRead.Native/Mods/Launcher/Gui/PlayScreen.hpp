#pragma once

#include "../../../NativeRuntime/Avalonia/Avalonia.hpp"
#include "../../../Formats/Formats.hpp"
#include "../../Network/NetMaster.hpp"
#include "../../Network/NetStatus.hpp"
#include "../Portable/LaunchPlan.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace MphRead
{
    class MenuSettings;
}

namespace MphRead::NativeRuntime::Avalonia::Threading
{
    class DispatcherTimer;
}

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;

    class ChoiceRow;
    class DeckButton;
    class DeckChip;
    class DeckField;
    class DeckGrid;
    class DeckSide;
    class DeckTile;
    class HunterStand;
    class Note;
    class ServerRow;
    class UiList;
    class UiMark;
    class UiTabs;

    // The four ways to choose something to play, and the map ballot opened
    // from the pause menu.
    class PlayScreen final : public Av::Controls::UserControl
    {
    public:
        enum class Face : std::int32_t
        {
            Online,
            Offline,
            Story,
            Clips,
            Vote
        };

        struct SampleServer final
        {
            std::string Name;
            std::string Endpoint;
            ::MphRead::Mods::Network::ServerStatus Status;
        };

        PlayScreen(const std::shared_ptr<::MphRead::MenuSettings>& settings,
            const std::vector<std::string>& rooms, Face face = Face::Online, bool overGame = false);

        Av::Event<PlayScreen&> Closed;
        Av::Event<PlayScreen&, ::MphRead::Mods::Launcher::LaunchPlan> Launched;
        Av::Event<PlayScreen&, std::string> Voted;
        Av::Event<PlayScreen&> CreateRequested;

        // The server rows shown by -uishot; no value means query the directory.
        static std::optional<std::vector<SampleServer>> Sample;

        [[nodiscard]] Face Current() const noexcept;
        void SessionEnded(const std::string& reason);

    protected:
        Av::Size MeasureOverride(Av::Size availableSize) override;
        void OnKeyDown(Av::Input::KeyEventArgs& e) override;

    private:
        struct Mode final
        {
            const char* Label;
            ::MphRead::GameMode Value;
        };

        struct ImportChoice final
        {
        };

        class BodyGrid;
        class BarRow;

        [[nodiscard]] std::shared_ptr<PlayScreen> Self();
        void SetCompact(bool compact);
        void ClearFaceExtras();
        void WantPreview(bool wanted);
        [[nodiscard]] std::shared_ptr<DeckSide> BuildSidePanel();
        void OpenServerSide(const std::shared_ptr<ServerRow>& row);
        void OpenSide(const std::shared_ptr<DeckTile>& tile);
        void CastVote(const std::shared_ptr<DeckTile>& tile);
        void CloseSide();
        void Fact(const std::string& key, const std::string& value,
            const Av::Media::IBrushPtr& tint = nullptr);
        void RefreshGoLabel();
        void StartPolling();
        void StopPolling();
        void QueryStatusSoon();
        void Leave();
        void Finish(::MphRead::Mods::Launcher::LaunchPlan plan);
        void Rebuild();
        void MakeHunterRows();
        [[nodiscard]] std::shared_ptr<ChoiceRow> AddHunter();
        [[nodiscard]] static std::string PlayerName();
        void BuildOnline();
        void ReloadServers();
        void Answered(std::int32_t asked);
        void AddServerRow(const ::MphRead::Mods::Network::MasterListing& listing);
        [[nodiscard]] std::pair<std::string, std::int32_t> Endpoint() const;
        [[nodiscard]] static std::string Describe(const ::MphRead::Mods::Network::ServerStatus& status);
        void Join();
        void BuildOffline();
        [[nodiscard]] Av::Media::Color SuitColour() const;
        void RefreshSideHunter();
        void FillMapGrid();
        void FillRooms(const std::optional<std::string>& current);
        void StartMatch();
        [[nodiscard]] std::optional<std::string> SelectedRoom() const;
        [[nodiscard]] std::optional<std::string> PreviewRoom() const;
        void BuildStory();
        void RefreshStory();
        [[nodiscard]] std::uint8_t SelectedSlot() const;
        void StartAdventure();
        void BuildDemo();
        void PlayDemo();
        void Watch(const std::string& path);
        void ImportDemo();
        void BuildVote();
        void RefreshBallot();
        void Go();
        void RefreshPreview();
        [[nodiscard]] static bool ParseEndpoint(std::string text, std::string& host, std::int32_t& port);

        static const std::vector<Mode> _modes;
        static const std::vector<std::string> _hunters;
        static const ImportChoice _import;

        std::shared_ptr<::MphRead::MenuSettings> _settings;
        std::vector<std::string> _rooms;
        bool _overGame = false;
        Face _only = Face::Online;
        std::shared_ptr<UiTabs> _tabs;
        std::shared_ptr<UiList> _list;
        std::shared_ptr<Av::Controls::StackPanel> _options;
        std::shared_ptr<Av::Controls::Image> _preview;
        std::shared_ptr<Av::Controls::Border> _previewBox;
        std::shared_ptr<HunterStand> _stand;
        std::shared_ptr<Av::Controls::Grid> _body;
        std::shared_ptr<Av::Controls::ScrollViewer> _side;
        bool _compact = false;
        bool _previewWanted = false;
        bool _standWanted = false;
        std::shared_ptr<Note> _note;
        std::shared_ptr<UiMark> _go;
        std::shared_ptr<UiMark> _back;
        std::shared_ptr<UiMark> _createLobby;
        std::shared_ptr<::MphRead::NativeRuntime::Avalonia::Threading::DispatcherTimer> _statusTimer;
        std::shared_ptr<std::stop_source> _statusCancel;
        bool _finished = false;

        std::shared_ptr<ChoiceRow> _hunter;
        std::shared_ptr<ChoiceRow> _mode;
        std::shared_ptr<ChoiceRow> _bots;
        std::shared_ptr<ChoiceRow> _skill;
        std::shared_ptr<ChoiceRow> _resume;
        std::shared_ptr<ChoiceRow> _suit;
        std::shared_ptr<DeckField> _name;
        std::shared_ptr<DeckField> _address;

        std::shared_ptr<DeckGrid> _grid;
        std::shared_ptr<Av::Controls::ScrollViewer> _gridScroll;
        std::shared_ptr<DeckSide> _sidePanel;
        std::shared_ptr<Av::Controls::StackPanel> _sideFacts;
        std::shared_ptr<Av::Controls::TextBlock> _sideNameText;
        std::shared_ptr<Av::Controls::TextBlock> _sideAddr;
        std::shared_ptr<DeckChip> _sideCode;
        std::shared_ptr<HunterStand> _sideStand;
        std::shared_ptr<DeckButton> _sideGo;
        std::optional<std::string> _picked;
        std::vector<Av::Controls::ControlPtr> _offlineRows;

        std::shared_ptr<Av::Controls::Control> _bar;
        std::int32_t _asked = 0;
        std::int32_t _replied = 0;
        std::int32_t _live = 0;
        // Async replies may already be queued when the screen leaves the tree.
        // This token lets those replies expire before they touch the control.
        std::shared_ptr<std::uint8_t> _lifetime = std::make_shared<std::uint8_t>(0);
    };
}
