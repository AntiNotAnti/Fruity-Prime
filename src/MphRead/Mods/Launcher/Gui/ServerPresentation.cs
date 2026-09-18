using MphRead.Mods.Network;

namespace MphRead.Mods.Launcher.Gui
{
    internal enum UiTone { Neutral, Good, Warning, Bad }

    // Pure presentation: neither probing a server nor initializing the UI is required.
    internal readonly record struct ServerPresentation(string State, string JoinLabel, string Reason,
        bool CanJoin, bool IsLobby, bool IsPlaying, bool IsFull, bool IsCompatible, UiTone Tone)
    {
        public static ServerPresentation From(ServerStatus status)
        {
            bool compatible = status.Protocol == 0 || status.Protocol == NetConfig.ProtocolVersion;
            bool full = status.MaxPlayers > 0 && status.Players >= status.MaxPlayers;
            bool lobby = status.LobbyEnabled && status.Phase == SessionPhase.Lobby;
            bool playing = !status.LobbyEnabled || status.Phase == SessionPhase.InMatch;
            string state, label = lobby ? UiText.JoinLobby : UiText.JoinMatch, reason = "";
            bool canJoin = true;
            UiTone tone = UiTone.Good;
            if (!status.Online) { state = UiText.ServerOffline; reason = status.Message.Length > 0 ? status.Message : "The server did not respond. Check its address and try again."; canJoin = false; tone = UiTone.Bad; }
            else if (!compatible) { state = UiText.UpdateRequired; reason = $"Server protocol {status.Protocol}; this build uses {NetConfig.ProtocolVersion}. Install a matching build."; canJoin = false; tone = UiTone.Bad; }
            else if (full) { state = UiText.ServerFull; reason = "All player slots are occupied."; canJoin = false; tone = UiTone.Warning; }
            else if (status.LobbyEnabled && status.Phase == SessionPhase.Starting) { state = UiText.StartingMatch; reason = "The match is starting. Wait for the server to finish loading."; canJoin = false; tone = UiTone.Warning; }
            else if (playing && status.LobbyEnabled && !status.AllowJoinInProgress) { state = UiText.InMatch; reason = "Joining in progress is disabled. Wait for the lobby to return."; canJoin = false; tone = UiTone.Warning; }
            else if (status.Legacy || !status.LobbyEnabled) { state = "Legacy / continuous"; label = UiText.JoinMatch; reason = "This server enters matches directly; it has no persistent lobby."; tone = UiTone.Neutral; }
            else if (lobby) state = "Lobby";
            else if (playing) state = UiText.InMatch;
            else { state = "Returning to lobby"; label = UiText.JoinLobby; }
            return new(state, label, reason, canJoin, lobby, playing, full, compatible, tone);
        }
    }
}
