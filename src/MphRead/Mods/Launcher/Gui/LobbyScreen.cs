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
        private readonly StackPanel _ownerControls = new();
        private readonly Note _status = new(""), _notice = new(""), _header = new("");
        private readonly UiBadge _connection = new("Connected", UiTone.Good);
        private readonly LobbyContext? _context;
        private readonly LobbyRosterPanel _rosterPanel;
        private readonly LobbyMatchPanel _matchPanel;
        private readonly LobbyChatPanel _chatPanel = new();
        private readonly Grid _columns;
        private readonly ScrollViewer _rosterScroll, _matchScroll;
        private readonly UiTabs _sections = new(new[] { "Players", "Match", "Chat" });
        private readonly StackPanel _administration;
        private readonly Queue<string> _notices = new();
        private DateTime _noticeUntil;
        private bool _compactLayout, _dirty, _pendingApply, _wasLost, _identityPending;
        private byte? _lastOwner, _managedSlot;
        private readonly Note _managedName = new("Select another player above to manage them.");
        private ushort _editRevision;
        private MatchDefinition? _submittedMatch;
        private SessionRules _submittedRules;
        private DateTime _applyDeadline;

        private readonly Note _readiness = new(""), _startReason = new("");
        private readonly ChoiceRow _hunter, _suit, _team, _map, _mode, _format;
        private readonly ChoiceRow _fire, _affinity, _freeze, _opponentHealth, _requireReady, _join;
        private readonly ChoiceRow _target, _moveTeam, _teamCount, _lockTeams;
        private readonly ChoiceRow[] _capacities = new ChoiceRow[4];
        private readonly StackPanel _custom = new();
        private readonly Note _layoutSummary = new("");
        private readonly FieldRow _name, _time, _goal;
        private readonly UiMark _ready, _start, _apply, _updateName;
        private readonly Image _preview = new() { Height = 110, Stretch = Stretch.UniformToFill };
        private readonly string[] _rooms;
        private readonly GameMode[] _modes = Enum.GetValues<GameMode>().Where(m => m >= GameMode.Battle && m <= GameMode.PrimeHunter).ToArray();
        private readonly MatchFormat[] _formats = Enum.GetValues<MatchFormat>();
        private readonly List<byte> _targetSlots = new();
        private MatchDefinition? _shownMatch;
        private ushort? _shownRevision;
        private int _rosterCount;
        private uint? _shownRosterRevision;
        private double _nextPingRefresh;
        private SessionRules _shownRules;
        private bool _syncing, _suspended, _closed;
        private Bitmap? _bitmap;

        public LobbyScreen(IReadOnlyList<string> rooms, LobbyContext? context = null)
        {
            _context = context;
            _rooms = rooms.ToArray();
            Focusable = true;
            _hunter = new ChoiceRow("Hunter", Enumerable.Range(0, Hunters.Playable).Select(i => ((Hunter)i).ToString()).ToArray(), (int)NetSession.LocalHunter);
            _suit = new ChoiceRow("Suit", new[] { "1", "2", "3", "4" }, NetSession.LocalColor);
            _team = new ChoiceRow("Team", new[] { "Auto", "Team A", "Team B" });
            _name = new FieldRow("Name", NetSession.PlayerName); _name.Box.MaxLength = RosterPacket.MaxNameBytes;
            _hunter.Changed += (_, _) => Identify(); _suit.Changed += (_, _) => Identify();
            _team.Changed += (_, _) => {
                if (!_syncing && !NetSession.ConnectionLost && NetSession.IsInLobby)
                    _identityPending = NetSession.SendLobbyCommand(LobbyCommandType.SetTeam, (byte)NetSession.LocalSlot, (sbyte)(_team.Index - 1));
            };

            _map = new ChoiceRow("Map", _rooms.Select(r => Metadata.GetRoomByName(r).Item1?.InGameName ?? r).ToArray());
            _mode = new ChoiceRow("Mode", _modes.Select(UiText.Mode).ToArray());
            _format = new ChoiceRow("Format", _formats.Select(UiText.Format).ToArray());
            _time = new FieldRow("Time limit (seconds)", "420", 85);
            _goal = new FieldRow("Score limit", "7", 85);
            _time.Box.TextChanged += (_, _) => RefreshDraft(); _goal.Box.TextChanged += (_, _) => RefreshDraft();
            _fire = Toggle("Friendly fire"); _affinity = Toggle("Affinity weapons"); _freeze = Toggle("Shadow freeze");
            _opponentHealth = new ChoiceRow("Opponent health", new[] { "Visible", "Hidden" });
            _requireReady = Toggle("Require players to be ready"); _join = Toggle("Allow joining in progress");
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
            foreach (Control control in new Control[] { new Caption("Match"), _map, _mode, _format, new Caption("Teams"), _custom, _layoutSummary, new Caption("Rules"), _time, _goal, _fire, _affinity, _freeze, _opponentHealth, _requireReady, _join, _lockTeams, new Note("Use 0 for no time or score limit."), _apply })
                _ownerControls.Children.Add(control);

            _updateName = ActionButton("Update name", Identify);
            var identity = new StackPanel { Spacing = 3 };
            foreach (Control control in new Control[] { _name, _updateName, _hunter, _suit, _team, new Note("Changing your player or team clears Ready.") }) identity.Children.Add(control);
            _target = new ChoiceRow("Selected player", Array.Empty<string>());
            _moveTeam = new ChoiceRow("Move to team", new[] { "Auto", "Team A", "Team B" });
            _administration = new StackPanel();
            _administration.Children.Add(new Caption("Manage selected player"));
            _administration.Children.Add(_managedName); _administration.Children.Add(_moveTeam);
            _administration.Children.Add(ActionButton("Move player", () => Admin(LobbyCommandType.SetTeam)));
            _administration.Children.Add(ActionButton("Transfer host", () => ConfirmAdmin(LobbyCommandType.TransferOwner)));
            _administration.Children.Add(ActionButton("Remove player", () => ConfirmAdmin(LobbyCommandType.KickPlayer)));
            _rosterPanel = new LobbyRosterPanel(identity, _administration);
            _rosterPanel.PlayerSelected += slot => { _managedSlot = slot; int index = _targetSlots.IndexOf(slot); if (index >= 0) _target.Index = index; Refresh(); };
            _matchPanel = new LobbyMatchPanel(_preview, _ownerControls);
            _matchPanel.EditRequested += () => { _editRevision = NetSession.ServerSession?.Revision ?? 0; _dirty = false; Refresh(); };
            _matchPanel.CancelRequested += () => { _shownMatch = null; _dirty = false; Refresh(); };
            foreach (var choice in new[] { _map, _mode, _format, _fire, _affinity, _freeze, _opponentHealth, _requireReady, _join, _lockTeams, _teamCount }.Concat(_capacities))
                choice.Changed += (_, _) => MarkDirty();
            _time.Box.TextChanged += (_, _) => MarkDirty(); _goal.Box.TextChanged += (_, _) => MarkDirty();
            _columns = new Grid { ColumnDefinitions = new ColumnDefinitions("2*,3*"), RowDefinitions = new RowDefinitions("*,180") };
            _rosterScroll = new ScrollViewer { Content = _rosterPanel, Margin = new Thickness(0, 0, 12, 0) };
            _matchScroll = new ScrollViewer { Content = _matchPanel };
            _columns.Children.Add(_rosterScroll); Grid.SetColumn(_matchScroll, 1); _columns.Children.Add(_matchScroll);
            Grid.SetRow(_chatPanel, 1); Grid.SetColumnSpan(_chatPanel, 2); _columns.Children.Add(_chatPanel);
            _sections.Changed += (_, _) => LayoutSections();
            var footer = new StackPanel { Orientation = Orientation.Horizontal, Spacing = 16, HorizontalAlignment = HorizontalAlignment.Center };
            footer.Children.Add(ActionButton("Leave", ConfirmLeave));
            _ready = ActionButton("Ready", () => NetSession.SendLobbyCommand(LobbyCommandType.SetReady,
                ready: !NetSession.SlotLobbyReady[NetSession.LocalSlot]));
            _start = ActionButton("Start match", () => NetSession.SendLobbyCommand(LobbyCommandType.StartMatch));
            footer.Children.Add(_ready); footer.Children.Add(_start);
            var body = new Grid { RowDefinitions = new RowDefinitions("Auto,*,Auto,Auto,Auto,Auto,Auto") };
            body.Children.Add(_sections);
            Grid.SetRow(_columns, 1); body.Children.Add(_columns);
            Grid.SetRow(_notice, 2); body.Children.Add(_notice);
            Grid.SetRow(_status, 3); body.Children.Add(_status); Grid.SetRow(_readiness, 4); body.Children.Add(_readiness);
            Grid.SetRow(_startReason, 5); body.Children.Add(_startReason);
            Grid.SetRow(footer, 6); body.Children.Add(footer);
            var frame = new Grid { MaxWidth = 1440, Margin = new Thickness(20, 12), RowDefinitions = new RowDefinitions("Auto,*") };
            var heading = new Grid { ColumnDefinitions = new ColumnDefinitions("*,Auto") };
            _header.FontSize = 16; heading.Children.Add(_header); Grid.SetColumn(_connection, 1); heading.Children.Add(_connection);
            frame.Children.Add(heading); Grid.SetRow(body, 1); frame.Children.Add(body);
            Panel root = UiLayout.Backdrop(wash: UiLayout.BackdropWash.Standard);
            root.Children.Add(frame); Content = root;
            _timer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(1.0 / 30) };
            _timer.Tick += (_, _) => Tick();
            LayoutSections(); Refresh();
        }

        internal void ShowCaptureState(string sample)
        {
            Suspend();
            if (sample is "match-edit" or "match-validation-error")
            { _matchPanel.BeginEdit(); _sections.Index = 1; if (sample == "match-validation-error") _time.Value = "invalid"; }
            if (sample == "guest-custom") _sections.Index = 2;
            if (sample == "host-transfer" && NetSession.ServerSession is { } state)
            { state.OwnerSlot = (byte)NetSession.LocalSlot; state.Revision++; NetSession.ApplySessionState(state); _noticeUntil = DateTime.MinValue; Refresh(); }
        }
        internal void RefreshForCheck() => Refresh();

        private void MarkDirty() { if (!_syncing && _matchPanel.Editing) { _dirty = true; RefreshDraft(); } }
        private void Notify(string message)
        {
            if (string.IsNullOrEmpty(_notice.Text))
            { _notice.Text = message; _notice.IsVisible = true; _noticeUntil = DateTime.UtcNow.AddSeconds(4); return; }
            if (_notices.Count == 4) _notices.Dequeue();
            _notices.Enqueue(message);
        }
        private void LayoutSections()
        {
            _sections.IsVisible = _compactLayout;
            _columns.ColumnDefinitions = new ColumnDefinitions(_compactLayout ? "*,0" : "2*,3*");
            _columns.RowDefinitions = new RowDefinitions(_compactLayout ? "*,0" : "*,180");
            Grid.SetColumn(_matchScroll, _compactLayout ? 0 : 1);
            Grid.SetRow(_chatPanel, _compactLayout ? 0 : 1);
            Grid.SetColumnSpan(_chatPanel, _compactLayout ? 1 : 2);
            _rosterScroll.IsVisible = !_compactLayout || _sections.Index == 0;
            _matchScroll.IsVisible = !_compactLayout || _sections.Index == 1;
            _chatPanel.IsVisible = !_compactLayout || _sections.Index == 2;
        }
        protected override Size MeasureOverride(Size availableSize)
        {
            var surface = TopLevel.GetTopLevel(this)?.ClientSize ?? availableSize;
            bool compact = availableSize.Width < 720 || availableSize.Height < 550 || surface.Width < 720 || surface.Height < 550;
            if (_compactLayout != compact) { _compactLayout = compact; LayoutSections(); }
            return base.MeasureOverride(availableSize);
        }
        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (!e.Handled && e.Key == Key.Escape) { ConfirmLeave(); e.Handled = true; }
            base.OnKeyDown(e);
        }
        private void ConfirmLeave()
        {
            bool migrate = NetSession.LocalIsLobbyOwner && _rosterCount > 1;
            ConfirmScreen.Show(this, migrate ? "Leave this lobby? Another player will become the host." : "Leave this lobby and disconnect?", "Leave", () => Leave(""));
        }

        private static ChoiceRow Toggle(string label) => new(label, new[] { "Off", "On" });
        private static UiMark ActionButton(string label, Action action)
        {
            var button = new UiMark(label == "Leave" || label == "Remove player" ? UiMark.Shape.Cancel : UiMark.Shape.Accept, label) { Margin = new Thickness(2) };
            button.Click += (_, _) => action(); return button;
        }
        protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e)
        { base.OnAttachedToVisualTree(e); if (!_suspended && !_closed) _timer.Start();
            Dispatcher.UIThread.Post(() => _ready.Focus(), DispatcherPriority.Background); }
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
                    Hunter = NetSession.LocalHunter, PlayerName = NetSession.PlayerName, RoomKey = match.RoomKey, Mode = match.Mode, Lobby = _context });
            }
        }
        private void Refresh()
        {
            if (NetSession.ServerSession is not { } session) return;
            bool connected = NetSession.Active && !NetSession.ConnectionLost;
            bool owner = NetSession.LocalIsLobbyOwner;
            if (_lastOwner.HasValue && _lastOwner != session.OwnerSlot)
                Notify(owner ? "You are now the host." : "The host has changed.");
            _lastOwner = session.OwnerSlot;
            if (_wasLost && connected) Notify("Connection restored.");
            _wasLost = !connected;
            if (_matchPanel.Editing && _editRevision != session.Revision && !_pendingApply)
            { _shownMatch = null; _dirty = false; _editRevision = session.Revision; Notify("The lobby changed. Match settings reloaded; review them before applying."); }
            if (_pendingApply && !NetSession.LobbyCommandPending)
            {
                bool confirmed = NetSession.LobbyMessage.Length == 0 && _submittedMatch == session.Match && _submittedRules == session.RuleFlags;
                bool failed = NetSession.LobbyMessage.Length > 0 || DateTime.UtcNow >= _applyDeadline;
                if (confirmed || failed)
                {
                    _pendingApply = false; _dirty = false;
                    if (confirmed) { _matchPanel.CloseEditor(); Notify("Match settings updated. Players must ready again."); }
                    else { _shownMatch = null; Notify(NetSession.LobbyMessage.Length > 0 ? NetSession.LobbyMessage : "Updated match state has not arrived. Review the current settings before retrying."); }
                    _editRevision = session.Revision;
                }
            }
            _syncing = true;
            _hunter.Index = (int)NetSession.LocalHunter; _suit.Index = NetSession.LocalColor;
            var roster = NetSession.LobbyRoster();
            _rosterCount = roster.Count;
            if (_identityPending && NetSession.LocalSlot >= 0 && !NetSession.SlotLobbyReady[NetSession.LocalSlot])
            { _identityPending = false; Notify("Your ready status is clear. Ready again when you are set."); }
            // Roster pings update without a configuration revision.
            if (_shownRevision != session.Revision || _shownRosterRevision != roster.Revision || NetSession.Clock >= _nextPingRefresh)
            {
                _shownRevision = session.Revision; _shownRosterRevision = roster.Revision; _nextPingRefresh = NetSession.Clock + 1;
                byte selected = _target.Index >= 0 && _target.Index < _targetSlots.Count ? _targetSlots[_target.Index] : byte.MaxValue;
                _rosterPanel.Refresh(roster, session);
                _targetSlots.Clear();
                var names = new List<string>();
                for (int i = 0; i < roster.Count; i++)
                    if (roster.Slots[i] != NetSession.LocalSlot) { _targetSlots.Add(roster.Slots[i]); names.Add(roster.Names[i]); }
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
                _opponentHealth.Index = session.Match.HideOpponentHealth ? 1 : 0;
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
            _matchPanel.Refresh(session, owner, connected && NetSession.CanEditLobby, NetSession.LobbyCommandPending || _pendingApply);
            _ownerControls.IsEnabled = connected && NetSession.CanEditLobby && !NetSession.LobbyCommandPending;
            _administration.IsVisible = owner;
            _administration.IsEnabled = connected && NetSession.CanEditLobby && !NetSession.LobbyCommandPending && _managedSlot.HasValue && _targetSlots.Contains(_managedSlot.Value);
            _managedName.Text = _administration.IsEnabled ? _target.Value : "Select another player above to manage them.";
            _name.IsEnabled = _updateName.IsEnabled = connected && NetSession.IsInLobby && !NetSession.LobbyCommandPending;
            _team.IsVisible = LobbyRules.TeamCount(session.Match) > 0;
            if (NetSession.LocalSlot >= 0) _team.Index = NetSession.SlotTeamIndex[NetSession.LocalSlot] + 1;
            _hunter.IsEnabled = _suit.IsEnabled = connected && NetSession.IsInLobby && !NetSession.LobbyCommandPending;
            _team.IsEnabled = _hunter.IsEnabled && (!session.LockTeams || NetSession.LocalIsLobbyOwner);
            _moveTeam.IsVisible = _team.IsVisible;
            _ready.IsEnabled = connected && NetSession.LocalSlot >= 0 && NetSession.IsInLobby && !NetSession.LobbyCommandPending;
            _ready.Label = NetSession.LocalSlot >= 0 && NetSession.SlotLobbyReady[NetSession.LocalSlot] ? "Cancel ready" : "Ready";
            var valid = LobbyRules.Validate(session.Match, roster, session.RequireReady, out string reason);
            _start.IsVisible = NetSession.LocalIsLobbyOwner;
            _start.IsEnabled = connected && !_matchPanel.Editing && NetSession.CanEditLobby && valid == LobbyResultCode.Ok && !NetSession.LobbyCommandPending;
            int ready = 0;
            for (int i = 0; i < roster.Count; i++) if (roster.LobbyReady[i]) ready++;
            _readiness.Text = $"{ready}/{roster.Count} players ready";
            _start.Label = NetSession.LobbyCommandPending ? "Waiting for server…" : $"Start Match ({ready}/{roster.Count})";
            int localPing = Enumerable.Range(0, roster.Count).Where(i => roster.Slots[i] == NetSession.LocalSlot).Select(i => (int)roster.Pings[i]).FirstOrDefault();
            _header.Text = $"{_context?.ServerName ?? "Lobby"} · {roster.Count}/{session.MaxPlayers} players · {UiText.Phase(session.Phase)} · {localPing} ms\n{_context?.Endpoint ?? "Connected server"}";
            _connection.Set(connected ? "CONNECTED" : "RECONNECTING", connected ? UiTone.Good : UiTone.Warning);
            _startReason.IsVisible = NetSession.LocalIsLobbyOwner;
            _startReason.Text = NetSession.LobbyCommandPending ? "Waiting for the server to confirm changes…"
                : !connected ? "Reconnecting. Actions are paused until the server responds."
                : _matchPanel.Editing ? "Apply or cancel your match changes before starting."
                : !NetSession.IsInLobby ? $"Loading {UiText.Map(session.Match.RoomKey)}…" : reason;
            _status.Text = NetSession.ConnectionLost ? "Connection lost, retrying…" : NetSession.LobbyMessage.Length > 0 ? NetSession.LobbyMessage
                : "";
            _status.IsVisible = _status.Text.Length > 0;
            _startReason.IsVisible = _startReason.Text.Length > 0;
            _chatPanel.Refresh(connected);
            if (DateTime.UtcNow >= _noticeUntil)
            {
                _notice.Text = _notices.Count > 0 ? _notices.Dequeue() : "";
                _noticeUntil = DateTime.UtcNow.AddSeconds(4);
            }
            _notice.IsVisible = !string.IsNullOrEmpty(_notice.Text);
            _syncing = false;
            RefreshDraft();
        }
        private void Identify()
        {
            if (_syncing || !NetSession.IsInLobby || NetSession.ConnectionLost || NetSession.LobbyCommandPending) return;
            NetSession.PlayerName = String.IsNullOrWhiteSpace(_name.Value) ? "Player" : _name.Value.Trim();
            NetSession.LocalHunter = (Hunter)_hunter.Index; NetSession.LocalColor = _suit.Index;
            LauncherPrefs.PlayerName = NetSession.PlayerName; LauncherPrefs.LastHunter = NetSession.LocalHunter;
            LauncherPrefs.LastColor = NetSession.LocalColor; LauncherPrefs.Save(); NetSession.SendIdentify(); _identityPending = true;

        }
        private void ConfirmAdmin(LobbyCommandType type)
        {
            if (!_administration.IsEnabled || _target.Index < 0 || _target.Index >= _targetSlots.Count) return;
            byte slot = _targetSlots[_target.Index];
            uint revision = NetSession.LobbyRoster().Revision;
            string action = type == LobbyCommandType.KickPlayer ? "Remove player" : "Transfer host";
            ConfirmScreen.Show(this, type == LobbyCommandType.KickPlayer
                ? $"Remove “{_target.Value}” from this lobby?" : $"Make “{_target.Value}” the host?", action, () =>
                {
                    if (NetSession.LobbyRoster().Revision != revision || !NetSession.CanEditLobby || NetSession.ConnectionLost || NetSession.LobbyCommandPending)
                    { Notify("The lobby changed. Select the player again."); return; }
                    NetSession.SendLobbyCommand(type, slot);
                });
        }

        private void Admin(LobbyCommandType type)
        {
            if (_administration.IsEnabled && NetSession.CanEditLobby && !NetSession.ConnectionLost && !NetSession.LobbyCommandPending && _target.Index >= 0 && _target.Index < _targetSlots.Count)
                NetSession.SendLobbyCommand(type, _targetSlots[_target.Index], (sbyte)(_moveTeam.Index - 1));
        }
        private MatchDefinition DraftMatch()
        {
            int count = _teamCount.Index + 2;
            return new MatchDefinition { RoomKey = _rooms.Length > 0 ? _rooms[Math.Clamp(_map.Index, 0, _rooms.Length - 1)] : "",
                Mode = _modes[Math.Clamp(_mode.Index, 0, _modes.Length - 1)], Format = _formats[Math.Clamp(_format.Index, 0, _formats.Length - 1)],
                CustomTeams = _formats[Math.Clamp(_format.Index, 0, _formats.Length - 1)] != MatchFormat.Custom ? NetSession.ServerSession?.Match.CustomTeams ?? default : new TeamLayout((byte)count, (byte)(_capacities[0].Index + 1), (byte)(_capacities[1].Index + 1),
                    count > 2 ? (byte)(_capacities[2].Index + 1) : (byte)0, count > 3 ? (byte)(_capacities[3].Index + 1) : (byte)0) };
        }
        private void RefreshDraft()
        {
            if (_syncing || _apply == null || _matchPanel == null || !_matchPanel.Editing) return;
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
            if (valid && NetSession.ServerSession is { } current)
            {
                var complete = draft with { TimeLimitSeconds = ushort.Parse(_time.Value), PointGoal = ushort.Parse(_goal.Value),
                    FriendlyFire = _fire.Index == 1, AffinityWeapons = _affinity.Index == 1,
                    ShadowFreeze = _freeze.Index == 1, HideOpponentHealth = _opponentHealth.Index == 1 };
                var rules = complete.Rules | (_requireReady.Index == 1 ? SessionRules.RequireReady : 0)
                    | (_join.Index == 1 ? SessionRules.AllowJoinInProgress : 0) | (_lockTeams.Index == 1 ? SessionRules.LockTeams : 0);
                _dirty = complete != current.Match || rules != current.RuleFlags;
            }
            _lockTeams.IsVisible = LobbyRules.TeamCount(draft) > 0;
            _apply.Label = _pendingApply ? "Applying…" : "Apply Changes";
            _apply.IsEnabled = !_pendingApply && _dirty && valid && !NetSession.ConnectionLost && NetSession.CanEditLobby && !NetSession.LobbyCommandPending;
            if (!valid) { _start.IsEnabled = false; _startReason.Text = reason; }
        }
        private void ApplyMatch()
        {
            if (!_apply.IsEnabled || _rooms.Length == 0 || NetSession.ServerSession is not { } config) return;
            if (config.Revision != _editRevision) { _shownMatch = null; _dirty = false; Notify("The lobby changed. Review the latest settings."); Refresh(); return; }
            if (!ushort.TryParse(_time.Value, out ushort seconds) || !ushort.TryParse(_goal.Value, out ushort goal))
            { _status.Text = "Time and goal must be whole numbers from 0 to 65535."; return; }
            config.Match = DraftMatch() with {
                TimeLimitSeconds = seconds, PointGoal = goal, FriendlyFire = _fire.Index == 1,
                AffinityWeapons = _affinity.Index == 1, ShadowFreeze = _freeze.Index == 1,
                HideOpponentHealth = _opponentHealth.Index == 1 };
            config.RuleFlags = config.Match.Rules | (_requireReady.Index == 1 ? SessionRules.RequireReady : 0)
                | (_join.Index == 1 ? SessionRules.AllowJoinInProgress : 0)
                | (_lockTeams.Index == 1 ? SessionRules.LockTeams : 0);
            if (LobbyRules.ValidateDefinition(config.Match, out string reason) != LobbyResultCode.Ok) { _status.Text = reason; return; }
            NetSession.SendLobbyCommand(LobbyCommandType.UpdateMatch, configuration: config);
            _pendingApply = NetSession.LobbyCommandPending; _submittedMatch = config.Match;
            _submittedRules = config.RuleFlags; _applyDeadline = DateTime.UtcNow.AddSeconds(10);
            _apply.Label = "Applying…"; Refresh();
        }
    }
}
