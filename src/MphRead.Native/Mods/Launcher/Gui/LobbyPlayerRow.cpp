#include "LobbyPlayerRow.hpp"

#include "Deck.hpp"
#include "GuiTheme.hpp"
#include "../../../Formats/Enums.hpp"
#include "../../../NativeRuntime/System/Encoding.hpp"
#include "../../../NativeRuntime/System/Managed.hpp"
#include "../../../NativeRuntime/System/Number.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace MphRead::Mods::Launcher::Gui
{
    namespace Runtime = ::MphRead::NativeRuntime;
    using namespace Runtime::Avalonia;

    LobbyPlayerRow::LobbyPlayerRow(const ::MphRead::Mods::Network::RosterPacket& roster,
        std::int32_t index, std::uint8_t owner, bool showTeam)
    {
        const std::uint8_t slot = Runtime::ManagedAt(roster.Slots, index);
        const std::int8_t teamValue = Runtime::ManagedAt(roster.Teams, index);
        std::string team;
        if (teamValue < 0)
        {
            team = "FFA";
        }
        else
        {
            const std::u16string teamLetter(1, static_cast<char16_t>('A' + teamValue));
            team = "Team " + Runtime::Utf16ToUtf8(teamLetter);
        }
        const std::string state = Runtime::ManagedAt(roster.LobbyReady, index) ? "READY" : "WAIT";
        const std::optional<std::string>& rosterName = Runtime::ManagedAt(roster.Names, index);
        const std::string name = rosterName.value_or(std::string{})
            + (slot == owner ? "  [OWNER]" : "");
        const std::string teamPart = showTeam ? " \xC2\xB7 " + team : std::string{};
        const auto hunter = static_cast<::MphRead::Hunter>(Runtime::ManagedAt(roster.Hunters, index));
        const std::int32_t suit = static_cast<std::int32_t>(Runtime::ManagedAt(roster.Colors, index)) + 1;
        const std::uint16_t ping = Runtime::ManagedAt(roster.Pings, index);
        const std::string detail = ::MphRead::ToString(hunter) + " \xC2\xB7 S" + Runtime::ToString(suit)
            + teamPart + " \xC2\xB7 " + Runtime::ToString(ping) + " ms";

        auto line = std::make_shared<Controls::Grid>();
        line->ColumnDefinitions(Controls::ColumnDefinitions("Auto,*,Auto"));
        line->ColumnSpacing(10);

        auto ready = std::make_shared<Controls::TextBlock>();
        ready->Text(state);
        ready->FontFamily(GuiTheme::Display());
        ready->FontSize(11);
        ready->Foreground(Runtime::ManagedAt(roster.LobbyReady, index)
            ? GuiTheme::GoodBrush : GuiTheme::TextDimBrush);
        ready->VerticalAlignment(Layout::VerticalAlignment::Center);

        auto player = std::make_shared<Controls::TextBlock>();
        player->Text(name);
        player->FontFamily(GuiTheme::Display());
        player->FontSize(13);
        player->Foreground(GuiTheme::TextBrush);
        player->TextTrimming(Media::TextTrimming::CharacterEllipsis);
        player->VerticalAlignment(Layout::VerticalAlignment::Center);

        auto facts = std::make_shared<Controls::TextBlock>();
        facts->Text(detail);
        facts->FontFamily(Deck::Mono);
        facts->FontSize(10);
        facts->Foreground(GuiTheme::TextDimBrush);
        facts->VerticalAlignment(Layout::VerticalAlignment::Center);

        Controls::Grid::SetColumn(*player, 1);
        Controls::Grid::SetColumn(*facts, 2);
        line->Children.Add(ready);
        line->Children.Add(player);
        line->Children.Add(facts);
        Padding(Thickness(4, 3));
        Child(line);
    }
}
