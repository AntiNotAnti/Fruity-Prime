using System;
using MphRead.Mods.Network;

namespace MphRead.Mods.Launcher
{
    /// <summary>Player-facing names, shared by graphical and text launchers.</summary>
    internal static class UiText
    {
        public const string JoinLobby = "Join Lobby", JoinMatch = "Join Match";
        public const string ServerOffline = "Offline", ServerFull = "Full", UpdateRequired = "Update required";
        public const string CheckingServer = "Checking…", StartingMatch = "Starting", InMatch = "In Match";

        public static string Mode(GameMode mode) => mode switch
        {
            GameMode.SinglePlayer => "Story",
            GameMode.Battle => "Battle",
            GameMode.BattleTeams => "Battle teams",
            GameMode.Survival => "Survival",
            GameMode.SurvivalTeams => "Survival teams",
            GameMode.Capture => "Capture",
            GameMode.Bounty => "Bounty",
            GameMode.BountyTeams => "Bounty teams",
            GameMode.Nodes => "Nodes",
            GameMode.NodesTeams => "Nodes teams",
            GameMode.Defender => "Defender",
            GameMode.DefenderTeams => "Defender teams",
            GameMode.PrimeHunter => "Prime Hunter",
            _ => "Unknown mode"
        };

        public static string Format(MatchFormat format) => format switch
        {
            MatchFormat.Auto => "Auto",
            MatchFormat.FreeForAll => "Free for all",
            MatchFormat.OneVsOne => "1v1",
            MatchFormat.TwoVsTwo => "2v2",
            MatchFormat.ThreeVsThree => "3v3",
            MatchFormat.FourVsFour => "4v4",
            MatchFormat.TwoVsTwoVsTwoVsTwo => "2v2v2v2",
            MatchFormat.Custom => "Custom",
            _ => "Unknown format"
        };

        public static string Phase(SessionPhase phase) => phase switch
        {
            SessionPhase.Lobby => "Lobby",
            SessionPhase.Starting => "Loading",
            SessionPhase.InMatch => "In match",
            SessionPhase.PostMatch => "Results",
            _ => "Unknown state"
        };

        public static string Team(int team) => team is >= 0 and < 4
            ? $"Team {(char)('A' + team)}" : "Free for all";
        public static string Count(int count, string singular, string? plural = null) =>
            $"{count} {(count == 1 ? singular : plural ?? singular + "s")}";
        public static string Players(int count) => Count(count, "player");
        public static string Ready(bool ready) => ready ? "Ready" : "Not ready";
        public static string ReplayKind(ReplayType type) => type == ReplayType.Clip ? "Replay clip" : "Full replay";
        public static string Integrity(ReplayIntegrity integrity) => integrity switch
        {
            ReplayIntegrity.Healthy => "Healthy",
            ReplayIntegrity.Recovered => "Recovered",
            ReplayIntegrity.Truncated => "Incomplete",
            ReplayIntegrity.Corrupt => "Damaged",
            _ => "Not checked"
        };
        public static string ReplayStatus(ReplayOpenResult result) => result switch
        {
            ReplayOpenResult.Success => "Compatible",
            ReplayOpenResult.ProtocolMismatch => "Incompatible network version",
            ReplayOpenResult.UnsupportedFormat => "Unsupported replay format",
            ReplayOpenResult.FileMissing => "Replay file missing",
            ReplayOpenResult.Empty => "Empty recording",
            ReplayOpenResult.Truncated => "Interrupted recording",
            ReplayOpenResult.MapMissing => "Map missing",
            ReplayOpenResult.MapHashMismatch => "Map version differs",
            ReplayOpenResult.IoError => "Could not read replay file",
            ReplayOpenResult.StateMismatch => "Recorded state differs",
            ReplayOpenResult.MissingMatchState => "Match information missing",
            _ => "Damaged replay"
        };
        public static string Sentence(string text) => text.Length == 0 ? text : Char.ToUpperInvariant(text[0]) + text[1..];
        public static string Controller(string? name) => String.IsNullOrWhiteSpace(name) ? "Controller" : name;
        public static string Map(string key) => Metadata.GetRoomByName(key).Item1?.InGameName ?? key;
    }
}
