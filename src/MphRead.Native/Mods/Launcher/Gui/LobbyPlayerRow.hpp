#pragma once

#include "../../../NativeRuntime/Avalonia/Avalonia.hpp"
#include "../../../Mods/Network/NetProtocol.hpp"

namespace MphRead::Mods::Launcher::Gui
{
    namespace Av = ::MphRead::NativeRuntime::Avalonia;

    class LobbyPlayerRow final : public Av::Controls::Border
    {
    public:
        LobbyPlayerRow(const ::MphRead::Mods::Network::RosterPacket& roster,
            std::int32_t index, std::uint8_t owner, bool showTeam = true);
    };
}
