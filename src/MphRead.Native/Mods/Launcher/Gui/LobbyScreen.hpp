#pragma once

#include "../../../NativeRuntime/Avalonia/Avalonia.hpp"
#include "../../Multiplayer/TeamLayout.hpp"
#include "../../Network/MatchDefinition.hpp"
#include "../../Network/SessionProtocol.hpp"
#include "../Portable/LaunchPlan.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace MphRead::NativeRuntime::Avalonia::Threading
{
    class DispatcherTimer;
}

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;
    using TeamLayout = ::MphRead::Mods::Multiplayer::TeamLayout;

    class ButtonToggleRow;
    class ChoiceRow;
    class DeckButton;
    class FieldRow;
    class LobbyPlayerRow;
    class MapCardPicker;
    class Note;
    class PickRow;
    class UiMark;

    class LobbyScreen final : public Av::Controls::UserControl
    {
    public:
        LobbyScreen(const std::vector<std::string>& rooms,
            std::shared_ptr<::MphRead::Mods::Launcher::LobbyContext> context = nullptr);
        ~LobbyScreen() override;

        Av::Event<LobbyScreen&, ::MphRead::Mods::Launcher::LaunchPlan> MatchRequested;
        Av::Event<LobbyScreen&, std::string> Closed;

        void Resume();
        void Suspend();
        void Leave(std::string reason);

    protected:
        void OnAttachedToVisualTree() override;
        void OnDetachedFromVisualTree() override;

    private:
        struct GameType final
        {
            const char* Label;
            ::MphRead::GameMode Free;
            ::MphRead::GameMode Team;
            bool TeamOnly;
            bool FfaOnly;
        };

        struct Matchup final
        {
            const char* Label;
            ::MphRead::Mods::Network::MatchFormat Format;
        };

        void Tick();
        void Refresh();
        void Identify();
        void SendChat();
        void Admin(::MphRead::Mods::Network::LobbyCommandType type);
        void MatchChoiceChanged(bool resetGoal);
        [[nodiscard]] ::MphRead::Mods::Network::MatchDefinition DraftMatch() const;
        void DraftChanged();
        void RefreshDraft();
        [[nodiscard]] bool TryBuildMatch(::MphRead::Mods::Network::MatchDefinition& match,
            std::string& reason) const;
        void TryAutoApply();
        void OpenMapPicker();
        void OpenCustomTeams();
        void OpenPage(const Av::Controls::ControlPtr& page);
        void ClosePage();
        void SetPreview(const std::string& room);
        [[nodiscard]] static std::string RoomName(const std::string& room);
        [[nodiscard]] static std::string Minutes(std::uint16_t seconds);
        [[nodiscard]] bool TryTimeSeconds(std::uint16_t& seconds) const;
        [[nodiscard]] static bool PlayerChoosesTeam(const ::MphRead::Mods::Network::MatchDefinition& match);
        [[nodiscard]] static std::string GoalLabel(::MphRead::GameMode mode);
        [[nodiscard]] static std::string GoalDisplay(::MphRead::GameMode mode, std::uint16_t value);
        [[nodiscard]] bool TryGoalValue(::MphRead::GameMode mode,
            std::uint16_t& value, std::string& reason) const;
        [[nodiscard]] ::MphRead::Mods::Network::MatchFormat SelectedFormat() const;
        [[nodiscard]] static std::int32_t MatchupIndex(::MphRead::Mods::Network::MatchFormat format);
        [[nodiscard]] static std::int32_t MatchupIndex(const ::MphRead::Mods::Network::MatchDefinition& match);
        [[nodiscard]] static std::int32_t BaseModeIndex(::MphRead::GameMode mode);

        static const std::array<GameType, 7> _gameTypes;
        static const std::array<Matchup, 8> _matchups;

        std::shared_ptr<::MphRead::NativeRuntime::Avalonia::Threading::DispatcherTimer> _timer;
        std::shared_ptr<Av::Controls::Grid> _root;
        Av::Controls::ControlPtr _mainPage;
        std::shared_ptr<Av::Controls::StackPanel> _players;
        std::shared_ptr<Av::Controls::StackPanel> _ownerControls;
        std::shared_ptr<Av::Controls::StackPanel> _administration;
        std::shared_ptr<Note> _status;
        std::shared_ptr<Note> _chat;
        std::shared_ptr<Av::Controls::ScrollViewer> _chatHistory;
        std::shared_ptr<Av::Controls::TextBox> _chatEntry;
        std::shared_ptr<ChoiceRow> _hunter;
        std::shared_ptr<ChoiceRow> _suit;
        std::shared_ptr<ChoiceRow> _team;
        std::shared_ptr<ChoiceRow> _mode;
        std::shared_ptr<ChoiceRow> _format;
        std::shared_ptr<ChoiceRow> _target;
        std::shared_ptr<ChoiceRow> _moveTeam;
        std::shared_ptr<PickRow> _map;
        std::shared_ptr<PickRow> _customTeams;
        std::shared_ptr<ButtonToggleRow> _fire;
        std::shared_ptr<ButtonToggleRow> _affinity;
        std::shared_ptr<ButtonToggleRow> _freeze;
        std::shared_ptr<ButtonToggleRow> _requireReady;
        std::shared_ptr<ButtonToggleRow> _join;
        std::shared_ptr<ButtonToggleRow> _lockTeams;
        std::shared_ptr<ButtonToggleRow> _opponentHealth;
        std::shared_ptr<Note> _layoutSummary;
        std::shared_ptr<FieldRow> _time;
        std::shared_ptr<FieldRow> _goal;
        std::shared_ptr<UiMark> _ready;
        std::shared_ptr<UiMark> _start;
        std::shared_ptr<DeckButton> _moveButton;
        std::shared_ptr<Av::Controls::Image> _preview;
        std::vector<std::string> _rooms;
        std::vector<std::uint8_t> _targetSlots;
        std::shared_ptr<Av::Media::Imaging::Bitmap> _bitmap;
        std::optional<::MphRead::Mods::Network::MatchDefinition> _shownMatch;
        std::optional<std::uint16_t> _shownRevision;
        std::optional<std::uint32_t> _shownRosterRevision;
        std::int32_t _chatRevision = -1;
        std::int32_t _rosterCount = 0;
        double _nextPingRefresh = 0;
        ::MphRead::Mods::Network::SessionRules _shownRules = ::MphRead::Mods::Network::SessionRules::None;
        std::string _draftRoom;
        TeamLayout _customLayout{2, 2, 2};
        bool _syncing = false;
        bool _suspended = false;
        bool _closed = false;
        bool _draftDirty = false;
        bool _timerConnected = false;
        double _draftChangedAt = 0;
    };

    class CustomTeamPicker final : public Av::Controls::UserControl
    {
    public:
        CustomTeamPicker(TeamLayout current, std::int32_t maxPlayers);

        Av::Event<CustomTeamPicker&, TeamLayout> Done;
        Av::Event<CustomTeamPicker&> Cancelled;

    private:
        [[nodiscard]] TeamLayout Value() const;
        void Refresh();

        std::shared_ptr<ChoiceRow> _count;
        std::array<std::shared_ptr<ChoiceRow>, 4> _sizes{};
        std::shared_ptr<Note> _note;
        std::shared_ptr<UiMark> _use;
        std::int32_t _maxPlayers;
    };
}
