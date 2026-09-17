using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using MphRead.Mods.Chat;
using MphRead.Mods.Network;
using MphRead.Mods.Multiplayer;

namespace MphRead.Mods.Launcher.Gui
{
    internal sealed class LobbyScreen : UserControl
    {
        public event EventHandler<LaunchPlan>? MatchRequested;
        public event EventHandler<string>? Closed;
        private readonly DispatcherTimer _timer;
        private readonly StackPanel _players = new(), _ownerControls = new();
        private readonly Note _status = new(""), _chat = new("");
        private readonly ChoiceRow _hunter, _suit, _team, _map, _mode, _format;
        private readonly ChoiceRow _fire, _affinity, _freeze, _requireReady, _join;
        private readonly ChoiceRow _target, _moveTeam, _teamCount, _lockTeams;
        private readonly ChoiceRow[] _capacities = new ChoiceRow[4];
        private readonly StackPanel _custom = new();
        private readonly Note _layoutSummary = new("");
        private readonly FieldRow _name, _time, _goal;
        private readonly TextBox _chatEntry = new() { Watermark = "Message", MaxLength = ChatPacket.MaxTextBytes };
        private readonly UiMark _ready, _start, _apply;
        private readonly Image _preview = new() { Height = 110, Stretch = Stretch.UniformToFill };
        private readonly string[] _rooms;
        private readonly GameMode[] _modes = Enum.GetValues<GameMode>().Where(m => m >= GameMode.Battle && m <= GameMode.PrimeHunter).ToArray();
        private readonly MatchFormat[] _formats = Enum.GetValues<MatchFormat>();
        private readonly List<byte> _targetSlots = new();
        private MatchDefinition? _shownMatch;
        private ushort? _shownRevision;
        private int _chatRevision = -1, _rosterCount;
        private ushort? _shownRosterRevision;
        private double _nextPingRefresh;
        private SessionRules _shownRules;
        private bool _syncing, _suspended, _closed;
        private Bitmap? _bitmap;

        public LobbyScreen(IReadOnlyList<string> rooms)
        {
            _rooms = rooms.ToArray();
            Focusable = true;
            _hunter = new ChoiceRow("Hunter", Enumerable.Range(0, Hunters.Playable).Select(i => ((Hunter)i).ToString()).ToArray(), (int)NetSession.LocalHunter);
            _suit = new ChoiceRow("Suit", new[] { "1", "2", "3", "4" }, NetSession.LocalColor);
            _team = new ChoiceRow("Team", new[] { "Auto", "Team A", "Team B" });
            _name = new FieldRow("Name", NetSession.PlayerName); _name.Box.MaxLength = RosterPacket.MaxNameBytes;
            _hunter.Changed += (_, _) => Identify(); _suit.Changed += (_, _) => Identify();
            _team.Changed += (_, _) => { if (!_syncing) NetSession.SendLobbyCommand(LobbyCommandType.SetTeam, (byte)NetSession.LocalSlot, (sbyte)(_team.Index - 1)); };

            _map = new ChoiceRow("Map", _rooms.Select(r => Metadata.GetRoomByName(r).Item1?.InGameName ?? r).ToArray());
            _mode = new ChoiceRow("Mode", _modes.Select(m => m.ToString()).ToArray());
            _format = new ChoiceRow("Format", new[] { "Auto", "FFA", "1v1", "2v2", "3v3", "4v4", "2v2v2v2", "Custom" });
            _time = new FieldRow("Time limit (seconds)", "420", 85);
            _goal = new FieldRow("Point goal", "7", 85);
            _time.Box.TextChanged += (_, _) => RefreshDraft(); _goal.Box.TextChanged += (_, _) => RefreshDraft();
            _fire = Toggle("Friendly fire"); _affinity = Toggle("Affinity weapons"); _freeze = Toggle("Shadow freeze");
            _requireReady = Toggle("Require ready"); _join = Toggle("Join in progress");
            _lockTeams = new ChoiceRow("Team changes", new[] { "Open", "Locked" });
            _teamCount = new ChoiceRow("Teams", new[] { "2", "3", "4" });
            _custom.Children.Add(_teamCount);
            for (int team = 0; team < 4; team++)
            {
                _capacities[team] = new ChoiceRow($"Team {(char)('A' + team)} size", Enumerable.Range(1, 8).Select(n => n.ToString()).ToArray(), 1);
                _capacities[team].Changed += (_, _) => RefreshDraft();
                _custom.Children.Add(_capacities[team]);
            }
            _teamCount.Changed += (_, _) => RefreshDraft();
            _format.Changed += (_, _) => RefreshDraft(); _mode.Changed += (_, _) => RefreshDraft();
            _apply = ActionButton("Apply match settings", ApplyMatch);
            foreach (Control control in new Control[] { _map, _mode, _format, _custom, _layoutSummary, _time, _goal, _fire, _affinity, _freeze, _requireReady, _join, _lockTeams, _apply })
                _ownerControls.Children.Add(control);

            var left = new StackPanel { Spacing = 4, Margin = new Thickness(0, 0, 18, 0) };
            left.Children.Add(new Note("PLAYERS")); left.Children.Add(_players);
            foreach (Control control in new Control[] { _name, ActionButton("Update name", Identify), _hunter, _suit, _team }) left.Children.Add(control);
            _target = new ChoiceRow("Manage player", Array.Empty<string>());
            _moveTeam = new ChoiceRow("Move to team", new[] { "Auto", "Team A", "Team B" });
            var administration = new StackPanel();
            administration.Children.Add(new Note("OWNER ACTIONS")); administration.Children.Add(_target); administration.Children.Add(_moveTeam);
            administration.Children.Add(ActionButton("Move player", () => Admin(LobbyCommandType.SetTeam)));
            administration.Children.Add(ActionButton("Transfer ownership", () => Admin(LobbyCommandType.TransferOwner)));
            administration.Children.Add(ActionButton("Remove player", () => Admin(LobbyCommandType.KickPlayer)));
            left.Children.Add(administration);

            var right = new StackPanel { Spacing = 3 };
            right.Children.Add(_preview); right.Children.Add(_ownerControls);
            right.Children.Add(new Note("A time or point limit of 0 means unlimited."));
            var columns = new Grid { ColumnDefinitions = new ColumnDefinitions("*,*") };
            columns.Children.Add(new ScrollViewer { Content = left });
            var rightScroll = new ScrollViewer { Content = right }; Grid.SetColumn(rightScroll, 1); columns.Children.Add(rightScroll);
            var chatPanel = new StackPanel();
            chatPanel.Children.Add(new ScrollViewer { Content = _chat, Height = 62 });
            var chatInput = new DockPanel(); var send = ActionButton("Send", SendChat);
            DockPanel.SetDock(send, Dock.Right); chatInput.Children.Add(send); chatInput.Children.Add(_chatEntry); chatPanel.Children.Add(chatInput);
            _chatEntry.KeyDown += (_, e) => { if (e.Key == Key.Enter) { SendChat(); e.Handled = true; } };
            var footer = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 16, HorizontalAlignment = HorizontalAlignment.Center };
            footer.Children.Add(ActionButton("Leave", () => Leave("")));
            _ready = ActionButton("Ready", () => NetSession.SendLobbyCommand(LobbyCommandType.SetReady,
                ready: !NetSession.SlotLobbyReady[NetSession.LocalSlot]));
            _start = ActionButton("Start match", () => NetSession.SendLobbyCommand(LobbyCommandType.StartMatch));
            footer.Children.Add(_ready); footer.Children.Add(_start);
            var body = new Grid { RowDefinitions = new RowDefinitions("*,Auto,Auto,Auto") };
            body.Children.Add(columns); Grid.SetRow(chatPanel, 1); body.Children.Add(chatPanel);
            Grid.SetRow(_status, 2); body.Children.Add(_status); Grid.SetRow(footer, 3); body.Children.Add(footer);
            // The footer belongs inside this grid, unlike screens whose marks sit outside their well.
            var frame = new Grid { MaxWidth = 1060, Margin = new Thickness(UiLayout.WellGutter),
                RowDefinitions = new RowDefinitions("Auto,*") };
            frame.Children.Add(new Note("LOBBY") { FontSize = UiLayout.HeadingSize,
                HorizontalAlignment = HorizontalAlignment.Center, Margin = new Thickness(0, 0, 0, 12) });
            Grid.SetRow(body, 1); frame.Children.Add(body);
            Panel root = UiLayout.Backdrop(wash: UiLayout.BackdropWash.Standard);
            root.Children.Add(frame); Content = root;
            _timer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(1.0 / 30) };
            _timer.Tick += (_, _) => { Tick(); administration.IsVisible = NetSession.LocalIsLobbyOwner; };
            Refresh();
        }

        private static ChoiceRow Toggle(string label) => new(label, new[] { "Off", "On" });
        private static UiMark ActionButton(string label, Action action)
        {
            var button = new UiMark(label == "Leave" || label == "Remove player" ? UiMark.Shape.Cancel : UiMark.Shape.Accept, label) { Margin = new Thickness(2) };
            button.Click += (_, _) => action(); return button;
        }
        protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e)
        { base.OnAttachedToVisualTree(e); if (!_suspended && !_closed) _timer.Start(); }
        protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e)
        { _timer.Stop(); base.OnDetachedFromVisualTree(e); }
        public void Resume() { _suspended = false; _shownRevision = null; _timer.Start(); }
        public void Suspend() { _suspended = true; _timer.Stop(); }
        public void Leave(string reason)
        {
            if (_closed) return;
            _closed = true; _timer.Stop(); _bitmap?.Dispose();
            NetSession.Stop(); NetHostSession.Stop(); Closed?.Invoke(this, reason);
        }
        private void Tick()
        {
            if (_suspended || _closed) return;
            NetSession.Pump();
            if (NetSession.Refused || NetSession.SessionTimedOut || !NetSession.Active)
            { Leave(NetSession.Refused ? NetSession.RefusedReason.Describe("Server") : "The connection to the server was lost."); return; }
            Refresh();
            if (NetSession.ShouldLoadMatch)
            {
                Suspend();
                var match = NetSession.ActiveMatchDefinition!.Value;
                MatchRequested?.Invoke(this, new LaunchPlan { Kind = LaunchKind.Online,
                    Hunter = NetSession.LocalHunter, PlayerName = NetSession.PlayerName, RoomKey = match.RoomKey, Mode = match.Mode });
            }
        }
        private void Refresh()
        {
            if (NetSession.ServerSession is not { } session) return;
            _syncing = true;
            _hunter.Index = (int)NetSession.LocalHunter; _suit.Index = NetSession.LocalColor;
            var roster = NetSession.LobbyRoster();
            _rosterCount = roster.Count;
            // Roster pings update without a configuration revision.
            if (_shownRevision != session.Revision || _shownRosterRevision != roster.Revision || NetSession.Clock >= _nextPingRefresh)
            {
                _shownRevision = session.Revision; _shownRosterRevision = roster.Revision; _nextPingRefresh = NetSession.Clock + 1;
                byte selected = _target.Index < _targetSlots.Count ? _targetSlots[_target.Index] : byte.MaxValue;
                _players.Children.Clear(); _targetSlots.Clear();
                var names = new List<string>();
                for (int i = 0; i < roster.Count; i++)
                {
                    _players.Children.Add(new LobbyPlayerRow(roster, i, session.OwnerSlot));
                    if (roster.Slots[i] != NetSession.LocalSlot) { _targetSlots.Add(roster.Slots[i]); names.Add(roster.Names[i]); }
                }
                _target.SetItems(names, Math.Max(0, _targetSlots.IndexOf(selected)));
            }
            if (_shownMatch != session.Match || _shownRules != session.RuleFlags)
            {
                _shownMatch = session.Match; _shownRules = session.RuleFlags;
                _map.Index = Array.IndexOf(_rooms, session.Match.RoomKey);
                _mode.Index = Array.IndexOf(_modes, session.Match.Mode); _format.Index = Array.IndexOf(_formats, session.Match.Format);
                _time.Value = session.Match.TimeLimitSeconds.ToString(); _goal.Value = session.Match.PointGoal.ToString();
                _fire.Index = session.Match.FriendlyFire ? 1 : 0; _affinity.Index = session.Match.AffinityWeapons ? 1 : 0;
                _freeze.Index = session.Match.ShadowFreeze ? 1 : 0;
                _requireReady.Index = session.RequireReady ? 1 : 0; _join.Index = session.AllowJoinInProgress ? 1 : 0;
                _lockTeams.Index = session.LockTeams ? 1 : 0;
                TeamLayout layout = LobbyRules.ResolveTeamLayout(session.Match);
                _teamCount.Index = Math.Clamp(layout.TeamCount - 2, 0, 2);
                for (int team = 0; team < 4; team++) _capacities[team].Index = Math.Max(1, (int)layout.Capacity(team)) - 1;
                string[] teams = Enumerable.Range(0, layout.TeamCount).Select(team => $"Team {(char)('A' + team)}").Prepend("Auto").ToArray();
                _team.SetItems(teams, 0); _moveTeam.SetItems(teams, 0);
                _bitmap?.Dispose(); _bitmap = null; _preview.Source = null;
                string path = ThumbnailGenerator.PathFor(session.Match.RoomKey);
                try { if (File.Exists(path)) { _bitmap = new Bitmap(path); _preview.Source = _bitmap; } }
                catch (Exception) { /* A preview is optional; map validation belongs to the server. */ }
                _preview.IsVisible = _bitmap != null;
            }
            _ownerControls.IsEnabled = NetSession.CanEditLobby && !NetSession.LobbyCommandPending;
            _team.IsVisible = LobbyRules.TeamCount(session.Match) > 0;
            if (NetSession.LocalSlot >= 0) _team.Index = NetSession.SlotTeamIndex[NetSession.LocalSlot] + 1;
            _hunter.IsEnabled = _suit.IsEnabled = NetSession.IsInLobby && !NetSession.LobbyCommandPending;
            _team.IsEnabled = _hunter.IsEnabled && (!session.LockTeams || NetSession.LocalIsLobbyOwner);
            _moveTeam.IsVisible = _team.IsVisible;
            _ready.IsEnabled = NetSession.IsInLobby && !NetSession.LobbyCommandPending;
            _ready.Label = NetSession.LocalSlot >= 0 && NetSession.SlotLobbyReady[NetSession.LocalSlot] ? "Unready" : "Ready";
            var valid = LobbyRules.Validate(session.Match, roster, session.RequireReady, out string reason);
            _start.IsVisible = NetSession.LocalIsLobbyOwner;
            _start.IsEnabled = NetSession.CanEditLobby && valid == LobbyResultCode.Ok && !NetSession.LobbyCommandPending;
            _status.Text = NetSession.ConnectionLost ? "Connection lost, retrying..." : NetSession.LobbyMessage.Length > 0 ? NetSession.LobbyMessage
                : session.Phase == SessionPhase.Lobby ? reason : "Waiting for players to finish loading...";
            if (_chatRevision != NetChat.Revision)
            { _chatRevision = NetChat.Revision; _chat.Text = String.Join("\n", NetChat.History.TakeLast(8)); }
            _syncing = false;
            RefreshDraft();
        }
        private void Identify()
        {
            if (_syncing || !NetSession.IsInLobby) return;
            NetSession.PlayerName = String.IsNullOrWhiteSpace(_name.Value) ? "Player" : _name.Value.Trim();
            NetSession.LocalHunter = (Hunter)_hunter.Index; NetSession.LocalColor = _suit.Index;
            LauncherPrefs.PlayerName = NetSession.PlayerName; LauncherPrefs.LastHunter = NetSession.LocalHunter;
            LauncherPrefs.LastColor = NetSession.LocalColor; LauncherPrefs.Save(); NetSession.SendIdentify();
        }
        private void SendChat() { NetChat.Send(_chatEntry.Text ?? ""); _chatEntry.Text = ""; }
        private void Admin(LobbyCommandType type)
        {
            if (_target.Index < _targetSlots.Count)
                NetSession.SendLobbyCommand(type, _targetSlots[_target.Index], (sbyte)(_moveTeam.Index - 1));
        }
        private MatchDefinition DraftMatch()
        {
            int count = _teamCount.Index + 2;
            return new MatchDefinition { RoomKey = _rooms.Length > 0 ? _rooms[Math.Clamp(_map.Index, 0, _rooms.Length - 1)] : "",
                Mode = _modes[Math.Clamp(_mode.Index, 0, _modes.Length - 1)], Format = _formats[Math.Clamp(_format.Index, 0, _formats.Length - 1)],
                CustomTeams = new TeamLayout((byte)count, (byte)(_capacities[0].Index + 1), (byte)(_capacities[1].Index + 1),
                    count > 2 ? (byte)(_capacities[2].Index + 1) : (byte)0, count > 3 ? (byte)(_capacities[3].Index + 1) : (byte)0) };
        }
        private void RefreshDraft()
        {
            if (_syncing || _apply == null) return;
            MatchDefinition draft = DraftMatch();
            _custom.IsVisible = draft.Format == MatchFormat.Custom;
            for (int team = 0; team < 4; team++) _capacities[team].IsVisible = team < _teamCount.Index + 2;
            TeamLayout layout = LobbyRules.ResolveTeamLayout(draft);
            bool valid = LobbyRules.ValidateDefinition(draft, out string reason) == LobbyResultCode.Ok;
            if (valid && layout.TeamCount > 0 && (layout.TotalPlayers < _rosterCount
                || (LobbyRules.ExactTeams(draft) && layout.TotalPlayers > (NetSession.ServerSession?.MaxPlayers ?? 8))))
            { valid = false; reason = "The layout must fit the connected roster and server player limit."; }
            if (valid && (!ushort.TryParse(_time.Value, out _) || !ushort.TryParse(_goal.Value, out _)))
            { valid = false; reason = "Time and goal must be whole numbers from 0 to 65535."; }
            _layoutSummary.Text = !valid ? reason : layout.TeamCount == 0 ? "Free for all"
                : $"Layout: {layout} · Players required: {(LobbyRules.ExactTeams(draft) ? layout.TotalPlayers.ToString() : "flexible")}";
            _apply.IsEnabled = valid && NetSession.CanEditLobby && !NetSession.LobbyCommandPending;
            if (!valid) _start.IsEnabled = false;
        }
        private void ApplyMatch()
        {
            if (_rooms.Length == 0 || NetSession.ServerSession is not { } config) return;
            if (!ushort.TryParse(_time.Value, out ushort seconds) || !ushort.TryParse(_goal.Value, out ushort goal))
            { _status.Text = "Time and goal must be whole numbers from 0 to 65535."; return; }
            config.Match = DraftMatch() with {
                TimeLimitSeconds = seconds, PointGoal = goal, FriendlyFire = _fire.Index == 1,
                AffinityWeapons = _affinity.Index == 1, ShadowFreeze = _freeze.Index == 1 };
            config.RuleFlags = config.Match.Rules | (_requireReady.Index == 1 ? SessionRules.RequireReady : 0)
                | (_join.Index == 1 ? SessionRules.AllowJoinInProgress : 0)
                | (_lockTeams.Index == 1 ? SessionRules.LockTeams : 0);
            if (LobbyRules.ValidateDefinition(config.Match, out string reason) != LobbyResultCode.Ok) { _status.Text = reason; return; }
            NetSession.SendLobbyCommand(LobbyCommandType.UpdateMatch, configuration: config);
        }
    }
}
