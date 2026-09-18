using System;
using Avalonia.Controls;
using MphRead.Mods.Network;

namespace MphRead.Mods.Launcher.Gui
{
    internal sealed class LobbyMatchPanel : StackPanel
    {
        private readonly Note _summary = new("");
        private readonly UiMark _edit = new(UiMark.Shape.Accept, "Edit Match");
        private readonly UiMark _cancel = new(UiMark.Shape.Cancel, "Cancel Changes");
        private readonly Control _controls;
        private (SessionStatePacket Session, bool Owner, bool Connected, bool Pending, bool Editing)? _shown;
        public bool Editing { get; private set; }
        public event Action? EditRequested;
        public event Action? CancelRequested;
        public LobbyMatchPanel(Control preview, Control controls)
        {
            Spacing = 6; _controls = controls;
            Children.Add(new Caption("Match")); Children.Add(preview); Children.Add(_summary);
            Children.Add(_edit); Children.Add(controls); Children.Add(_cancel);
            _edit.Click += (_, _) => { Editing = true; EditRequested?.Invoke(); };
            _cancel.Click += (_, _) => { Editing = false; CancelRequested?.Invoke(); };
        }
        public void CloseEditor() => Editing = false;
        internal void BeginEdit() { Editing = true; EditRequested?.Invoke(); }
        public void Refresh(SessionStatePacket session, bool owner, bool connected, bool pending)
        {
            if (!owner) Editing = false;
            var current = (session, owner, connected, pending, Editing);
            if (_shown is { } previous && previous.Equals(current)) return;
            _shown = current;
            _summary.Text = $"{UiText.Map(session.Match.RoomKey)}\n{UiText.Mode(session.Match.Mode)} · {UiText.Format(session.Match.Format)}\n"
                + $"Time: {(session.Match.TimeLimitSeconds == 0 ? "No limit" : session.Match.TimeLimitSeconds + " seconds")} · Score: {(session.Match.PointGoal == 0 ? "No limit" : session.Match.PointGoal.ToString())}\n"
                + $"Friendly fire: {On(session.Match.FriendlyFire)} · Affinity: {On(session.Match.AffinityWeapons)}\nShadow freeze: {On(session.Match.ShadowFreeze)} · Opponent health: {(session.Match.HideOpponentHealth ? "Hidden" : "Visible")}\n"
                + $"Ready required: {On(session.RequireReady)} · Join in progress: {On(session.AllowJoinInProgress)}\n"
                + (LobbyRules.TeamCount(session.Match) > 0 ? $"Teams: {LobbyRules.ResolveTeamLayout(session.Match)} · Changes {(session.LockTeams ? "locked" : "open")}\n" : "")
                + (owner ? "You control this match." : "The host controls match settings.");
            _summary.IsVisible = !Editing; _controls.IsVisible = _cancel.IsVisible = Editing;
            _edit.IsVisible = owner && !Editing; _edit.IsEnabled = connected && !pending;
            _cancel.IsEnabled = !pending;
        }
        private static string On(bool value) => value ? "On" : "Off";
    }
}
