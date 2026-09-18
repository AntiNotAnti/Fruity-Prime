using System;
using System.Collections.Generic;
using System.Globalization;
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
        private static readonly (string Label, GameMode Free, GameMode Team, bool TeamOnly, bool FfaOnly)[] _gameTypes =
        {
            ("Battle", GameMode.Battle, GameMode.BattleTeams, false, false),
            ("Survival", GameMode.Survival, GameMode.SurvivalTeams, false, false),
            ("Bounty", GameMode.Bounty, GameMode.BountyTeams, false, false),
            ("Defender", GameMode.Defender, GameMode.DefenderTeams, false, false),
            ("Nodes", GameMode.Nodes, GameMode.NodesTeams, false, false),
            ("Capture", GameMode.Capture, GameMode.Capture, true, false),
            ("Prime Hunter", GameMode.PrimeHunter, GameMode.PrimeHunter, false, true)
        };

        private static readonly (string Label, MatchFormat Format)[] _matchups =
        {
            ("FFA", MatchFormat.FreeForAll),
            ("Teams", MatchFormat.Auto),
            ("1v1", MatchFormat.OneVsOne),
            ("2v2", MatchFormat.TwoVsTwo),
            ("3v3", MatchFormat.ThreeVsThree),
            ("4v4", MatchFormat.FourVsFour),
            ("2v2v2v2", MatchFormat.TwoVsTwoVsTwoVsTwo),
            ("Custom", MatchFormat.Custom)
        };

        public event EventHandler<LaunchPlan>? MatchRequested;
        public event EventHandler<string>? Closed;

        private readonly DispatcherTimer _timer;
        private readonly Grid _root = new();
        private readonly Control _mainPage;
        private readonly StackPanel _players = new() { Spacing = 2 };
        private readonly StackPanel _ownerControls = new() { Spacing = 2 };
        private readonly Note _status = new(""), _chat = new("");
        private readonly ChoiceRow _hunter, _suit, _team, _mode, _format;
        private readonly ChoiceRow _target, _moveTeam;
        private readonly PickRow _map, _customTeams;
        private readonly ButtonToggleRow _fire, _affinity, _freeze, _requireReady, _join;
        private readonly ButtonToggleRow _lockTeams, _opponentHealth;
        private readonly Note _layoutSummary = new("");
        private readonly FieldRow _time, _goal;
        private readonly TextBox _chatEntry = new() { Watermark = "Message", MaxLength = ChatPacket.MaxTextBytes };
        private readonly UiMark _ready, _start, _apply;
        private readonly Image _preview = new() { Height = 104, Stretch = Stretch.UniformToFill };
        private readonly string[] _rooms;
        private readonly List<byte> _targetSlots = new();

        private MatchDefinition? _shownMatch;
        private ushort? _shownRevision;
        private uint? _shownRosterRevision;
        private int _chatRevision = -1, _rosterCount;
        private double _nextPingRefresh;
        private SessionRules _shownRules;
        private string _draftRoom = "";
        private TeamLayout _customLayout = new(2, 2, 2);
        private bool _syncing, _suspended, _closed;
        private Bitmap? _bitmap;

        public LobbyScreen(IReadOnlyList<string> rooms, MphRead.Mods.Launcher.LobbyContext? context = null)
        {
            _rooms = rooms.ToArray();
            Focusable = true;

            _hunter = new ChoiceRow("Hunter",
                Enumerable.Range(0, Hunters.Playable).Select(i => ((Hunter)i).ToString()).ToArray(),
                (int)NetSession.LocalHunter);
            _suit = new ChoiceRow("Suit", new[] { "1", "2", "3", "4" }, NetSession.LocalColor);
            _team = new ChoiceRow("Team", new[] { "Auto", "Team A", "Team B" });
            _hunter.Changed += (_, _) => Identify();
            _suit.Changed += (_, _) => Identify();
            _team.Changed += (_, _) =>
            {
                if (!_syncing && NetSession.LocalSlot >= 0)
                    NetSession.SendLobbyCommand(LobbyCommandType.SetTeam, (byte)NetSession.LocalSlot,
                        (sbyte)(_team.Index - 1));
            };

            _map = new PickRow("Map");
            _map.Clicked += (_, _) => OpenMapPicker();
            _mode = new ChoiceRow("Game type", _gameTypes.Select(m => m.Label).ToArray());
            _format = new ChoiceRow("Matchup", _matchups.Select(m => m.Label).ToArray());
            _mode.Changed += (_, _) => MatchChoiceChanged();
            _format.Changed += (_, _) => MatchChoiceChanged();

            _customTeams = new PickRow("Custom teams") { IsVisible = false };
            _customTeams.Clicked += (_, _) => OpenCustomTeams();

            _time = new FieldRow("Time limit (minutes)", "7", 80);
            _goal = new FieldRow("Point goal", "7", 80);
            _time.Box.TextChanged += (_, _) => RefreshDraft();
            _goal.Box.TextChanged += (_, _) => RefreshDraft();

            _fire = Toggle("Friendly fire");
            _affinity = Toggle("Affinity weapons");
            _freeze = Toggle("Shadow freeze");
            _requireReady = Toggle("Require ready");
            _join = Toggle("Join in progress");
            _lockTeams = Toggle("Lock teams");
            _opponentHealth = Toggle("Opponent health", on: true);
            foreach (ButtonToggleRow toggle in new[]
            {
                _fire, _affinity, _freeze, _opponentHealth, _requireReady, _join, _lockTeams
            })
                toggle.Changed += (_, _) => RefreshDraft();

            _apply = ActionButton("Apply", ApplyMatch);
            _ownerControls.Children.Add(_preview);
            _ownerControls.Children.Add(_map);
            _ownerControls.Children.Add(_mode);
            _ownerControls.Children.Add(_format);
            _ownerControls.Children.Add(_customTeams);

            var limits = new Grid
            {
                ColumnDefinitions = new ColumnDefinitions("*,*"),
                ColumnSpacing = 12
            };
            limits.Children.Add(_time);
            Grid.SetColumn(_goal, 1);
            limits.Children.Add(_goal);
            _ownerControls.Children.Add(limits);

            var toggles = new Grid
            {
                ColumnDefinitions = new ColumnDefinitions("*,*"),
                RowDefinitions = new RowDefinitions("Auto,Auto,Auto,Auto"),
                ColumnSpacing = 12,
                RowSpacing = 2
            };
            Control[] toggleRows =
            {
                _fire, _affinity, _freeze, _opponentHealth,
                _requireReady, _join, _lockTeams
            };
            for (int i = 0; i < toggleRows.Length; i++)
            {
                Grid.SetColumn(toggleRows[i], i % 2);
                Grid.SetRow(toggleRows[i], i / 2);
                toggles.Children.Add(toggleRows[i]);
            }
            _ownerControls.Children.Add(toggles);
            _ownerControls.Children.Add(_layoutSummary);
            _ownerControls.Children.Add(_apply);

            var left = new StackPanel { Spacing = 2, Margin = new Thickness(0, 0, 18, 0) };
            left.Children.Add(new Caption("Players"));
            left.Children.Add(_players);
            left.Children.Add(new Caption("Your player"));
            left.Children.Add(_hunter);
            left.Children.Add(_suit);
            left.Children.Add(_team);

            _target = new ChoiceRow("Manage player", Array.Empty<string>());
            _moveTeam = new ChoiceRow("Move to team", new[] { "Auto", "Team A", "Team B" });
            var administration = new StackPanel { Spacing = 2 };
            administration.Children.Add(new Caption("Owner actions"));
            administration.Children.Add(_target);
            administration.Children.Add(_moveTeam);
            var adminButtons = new StackPanel
            {
                Orientation = Orientation.Horizontal,
                Spacing = 7,
                HorizontalAlignment = HorizontalAlignment.Center
            };
            adminButtons.Children.Add(SmallButton("Move", Deck.Face.Blue,
                () => Admin(LobbyCommandType.SetTeam)));
            adminButtons.Children.Add(SmallButton("Transfer", Deck.Face.Brass,
                () => Admin(LobbyCommandType.TransferOwner)));
            adminButtons.Children.Add(SmallButton("Kick", Deck.Face.Rust,
                () => Admin(LobbyCommandType.KickPlayer)));
            administration.Children.Add(adminButtons);
            left.Children.Add(administration);

            var columns = new Grid
            {
                ColumnDefinitions = new ColumnDefinitions("0.82*,1.18*"),
                ColumnSpacing = 12
            };
            columns.Children.Add(left);
            Grid.SetColumn(_ownerControls, 1);
            columns.Children.Add(_ownerControls);

            _chat.MaxHeight = 48;
            var chatPanel = new Grid
            {
                RowDefinitions = new RowDefinitions("Auto,Auto"),
                RowSpacing = 3
            };
            chatPanel.Children.Add(_chat);
            var chatInput = new DockPanel();
            var send = ActionButton("Send", SendChat);
            DockPanel.SetDock(send, Dock.Right);
            chatInput.Children.Add(send);
            chatInput.Children.Add(_chatEntry);
            Grid.SetRow(chatInput, 1);
            chatPanel.Children.Add(chatInput);
            _chatEntry.KeyDown += (_, e) =>
            {
                if (e.Key == Key.Enter)
                {
                    SendChat();
                    e.Handled = true;
                }
            };

            var footer = new StackPanel
            {
                Orientation = Orientation.Horizontal,
                Spacing = 16,
                HorizontalAlignment = HorizontalAlignment.Center
            };
            footer.Children.Add(ActionButton("Leave", () => Leave("")));
            _ready = ActionButton("Ready", () =>
            {
                if (NetSession.LocalSlot >= 0)
                    NetSession.SendLobbyCommand(LobbyCommandType.SetReady,
                        ready: !NetSession.SlotLobbyReady[NetSession.LocalSlot]);
            });
            _start = ActionButton("Start match", () => NetSession.SendLobbyCommand(LobbyCommandType.StartMatch));
            footer.Children.Add(_ready);
            footer.Children.Add(_start);

            var body = new Grid
            {
                RowDefinitions = new RowDefinitions("*,Auto,Auto,Auto"),
                RowSpacing = 3
            };
            body.Children.Add(columns);
            Grid.SetRow(chatPanel, 1);
            body.Children.Add(chatPanel);
            Grid.SetRow(_status, 2);
            body.Children.Add(_status);
            Grid.SetRow(footer, 3);
            body.Children.Add(footer);

            var frame = new Grid
            {
                MaxWidth = 1100,
                Margin = new Thickness(UiLayout.WellGutter),
                RowDefinitions = new RowDefinitions("Auto,*")
            };
            string lobbyTitle = context?.ServerName is { Length: > 0 } serverName
                ? $"{serverName} LOBBY".ToUpperInvariant()
                : "LOBBY";
            frame.Children.Add(new Note(lobbyTitle)
            {
                FontSize = UiLayout.HeadingSize,
                HorizontalAlignment = HorizontalAlignment.Center,
                Margin = new Thickness(0, 0, 0, 8)
            });
            Grid.SetRow(body, 1);
            frame.Children.Add(body);

            Panel backdrop = UiLayout.Backdrop(wash: UiLayout.BackdropWash.Standard);
            backdrop.Children.Add(frame);
            _mainPage = backdrop;
            _root.Children.Add(_mainPage);
            Content = _root;

            _timer = new DispatcherTimer { Interval = TimeSpan.FromSeconds(1.0 / 30) };
            _timer.Tick += (_, _) =>
            {
                Tick();
                administration.IsVisible = NetSession.LocalIsLobbyOwner;
            };
            Refresh();
        }

        private static ButtonToggleRow Toggle(string label, bool on = false)
        {
            var row = new ButtonToggleRow(label, on);
            row.Changed += (_, _) => { };
            return row;
        }

        private static UiMark ActionButton(string label, Action action)
        {
            var button = new UiMark(
                label == "Leave" ? UiMark.Shape.Cancel : UiMark.Shape.Accept,
                label) { Margin = new Thickness(2) };
            button.Click += (_, _) => action();
            return button;
        }

        private static DeckButton SmallButton(string label, Deck.Face face, Action action)
        {
            var button = new DeckButton(label, face,
                sizeEms: 0.82, padXEms: 0.8, padYEms: 0.34, lip: 4);
            button.Click += (_, _) => action();
            return button;
        }

        protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e)
        {
            base.OnAttachedToVisualTree(e);
            if (!_suspended && !_closed) _timer.Start();
        }

        protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e)
        {
            _timer.Stop();
            base.OnDetachedFromVisualTree(e);
        }

        public void Resume()
        {
            _suspended = false;
            _shownRevision = null;
            _timer.Start();
        }

        public void Suspend()
        {
            _suspended = true;
            _timer.Stop();
        }

        public void Leave(string reason)
        {
            if (_closed) return;
            _closed = true;
            _timer.Stop();
            _bitmap?.Dispose();
            NetSession.Stop();
            NetHostSession.Stop();
            Closed?.Invoke(this, reason);
        }

        private void Tick()
        {
            if (_suspended || _closed) return;
            NetSession.Pump();
            if (NetSession.Refused || NetSession.SessionTimedOut || !NetSession.Active)
            {
                Leave(NetSession.Refused
                    ? NetSession.RefusedReason.Describe("Server")
                    : "The connection to the server was lost.");
                return;
            }
            Refresh();
            if (NetSession.ShouldLoadMatch)
            {
                Suspend();
                MatchDefinition match = NetSession.ActiveMatchDefinition!.Value;
                MatchRequested?.Invoke(this, new LaunchPlan
                {
                    Kind = LaunchKind.Online,
                    Hunter = NetSession.LocalHunter,
                    PlayerName = NetSession.PlayerName,
                    RoomKey = match.RoomKey,
                    Mode = match.Mode
                });
            }
        }

        private void Refresh()
        {
            if (NetSession.ServerSession is not { } session) return;
            _syncing = true;
            _hunter.Index = (int)NetSession.LocalHunter;
            _suit.Index = NetSession.LocalColor;
            RosterPacket roster = NetSession.LobbyRoster();
            _rosterCount = roster.Count;

            if (_shownRevision != session.Revision
                || _shownRosterRevision != roster.Revision
                || NetSession.Clock >= _nextPingRefresh)
            {
                _shownRevision = session.Revision;
                _shownRosterRevision = roster.Revision;
                _nextPingRefresh = NetSession.Clock + 1;
                byte selected = _target.Index < _targetSlots.Count
                    ? _targetSlots[_target.Index]
                    : byte.MaxValue;
                _players.Children.Clear();
                _targetSlots.Clear();
                var names = new List<string>();
                for (int i = 0; i < roster.Count; i++)
                {
                    _players.Children.Add(new LobbyPlayerRow(roster, i, session.OwnerSlot));
                    if (roster.Slots[i] != NetSession.LocalSlot)
                    {
                        _targetSlots.Add(roster.Slots[i]);
                        names.Add(roster.Names[i]);
                    }
                }
                _target.SetItems(names, Math.Max(0, _targetSlots.IndexOf(selected)));
            }

            if (_shownMatch != session.Match || _shownRules != session.RuleFlags)
            {
                _shownMatch = session.Match;
                _shownRules = session.RuleFlags;
                _draftRoom = session.Match.RoomKey;
                _map.Set(RoomName(_draftRoom));
                SetPreview(_draftRoom);
                _mode.Index = BaseModeIndex(session.Match.Mode);
                _format.Index = MatchupIndex(session.Match);
                _time.Value = Minutes(session.Match.TimeLimitSeconds);
                _goal.Value = session.Match.PointGoal.ToString(CultureInfo.InvariantCulture);
                _fire.On = session.Match.FriendlyFire;
                _affinity.On = session.Match.AffinityWeapons;
                _freeze.On = session.Match.ShadowFreeze;
                _opponentHealth.On = !session.Match.HideOpponentHealth;
                _requireReady.On = session.RequireReady;
                _join.On = session.AllowJoinInProgress;
                _lockTeams.On = session.LockTeams;

                TeamLayout layout = LobbyRules.ResolveTeamLayout(session.Match);
                _customLayout = session.Match.CustomTeams.IsValid
                    ? session.Match.CustomTeams
                    : layout.IsValid ? layout : new TeamLayout(2, 2, 2);
                _customTeams.Set(_customLayout.ToString());

                string[] teams = Enumerable.Range(0, layout.TeamCount)
                    .Select(team => $"Team {(char)('A' + team)}")
                    .Prepend("Auto").ToArray();
                _team.SetItems(teams, 0);
                _moveTeam.SetItems(teams, 0);
            }

            _ownerControls.IsEnabled = NetSession.CanEditLobby && !NetSession.LobbyCommandPending;
            TeamLayout activeLayout = LobbyRules.ResolveTeamLayout(session.Match);
            _team.IsVisible = activeLayout.TeamCount > 0;
            if (NetSession.LocalSlot >= 0)
                _team.Index = NetSession.SlotTeamIndex[NetSession.LocalSlot] + 1;
            _hunter.IsEnabled = _suit.IsEnabled = NetSession.IsInLobby && !NetSession.LobbyCommandPending;
            _team.IsEnabled = _hunter.IsEnabled && (!session.LockTeams || NetSession.LocalIsLobbyOwner);
            _moveTeam.IsVisible = _team.IsVisible;
            _ready.IsEnabled = NetSession.IsInLobby && !NetSession.LobbyCommandPending;
            _ready.Label = NetSession.LocalSlot >= 0 && NetSession.SlotLobbyReady[NetSession.LocalSlot]
                ? "Unready" : "Ready";

            LobbyResultCode valid = LobbyRules.Validate(session.Match, roster,
                session.RequireReady, out string reason);
            _start.IsVisible = NetSession.LocalIsLobbyOwner;
            _start.IsEnabled = NetSession.CanEditLobby
                && valid == LobbyResultCode.Ok
                && !NetSession.LobbyCommandPending;
            _status.Text = NetSession.ConnectionLost
                ? "Connection lost, retrying..."
                : NetSession.LobbyMessage.Length > 0
                    ? NetSession.LobbyMessage
                    : session.Phase == SessionPhase.Lobby
                        ? reason
                        : "Waiting for players to finish loading...";

            if (_chatRevision != NetChat.Revision)
            {
                _chatRevision = NetChat.Revision;
                _chat.Text = String.Join("\n", NetChat.History.TakeLast(4));
            }

            _syncing = false;
            RefreshDraft();
        }

        private void Identify()
        {
            if (_syncing || !NetSession.IsInLobby) return;
            NetSession.LocalHunter = (Hunter)_hunter.Index;
            NetSession.LocalColor = _suit.Index;
            LauncherPrefs.LastHunter = NetSession.LocalHunter;
            LauncherPrefs.LastColor = NetSession.LocalColor;
            LauncherPrefs.Save();
            NetSession.SendIdentify();
        }

        private void SendChat()
        {
            NetChat.Send(_chatEntry.Text ?? "");
            _chatEntry.Text = "";
        }

        private void Admin(LobbyCommandType type)
        {
            if (_target.Index < _targetSlots.Count)
                NetSession.SendLobbyCommand(type, _targetSlots[_target.Index],
                    (sbyte)(_moveTeam.Index - 1));
        }

        private void MatchChoiceChanged()
        {
            if (_syncing) return;
            _syncing = true;
            (string _, GameMode _, GameMode _, bool teamOnly, bool ffaOnly) = _gameTypes[_mode.Index];
            MatchFormat format = SelectedFormat();
            int target = _format.Index;
            if (ffaOnly && format != MatchFormat.FreeForAll)
                target = MatchupIndex(MatchFormat.FreeForAll);
            else if (teamOnly && (format == MatchFormat.FreeForAll
                || format == MatchFormat.TwoVsTwoVsTwoVsTwo))
                target = MatchupIndex(MatchFormat.Auto);
            if (target != _format.Index) _format.Index = target;
            _syncing = false;
            RefreshDraft();
        }

        private MatchDefinition DraftMatch()
        {
            MatchFormat format = SelectedFormat();
            var type = _gameTypes[Math.Clamp(_mode.Index, 0, _gameTypes.Length - 1)];
            bool teams = format != MatchFormat.FreeForAll;
            GameMode mode = type.FfaOnly ? type.Free
                : type.TeamOnly ? type.Team
                : teams ? type.Team : type.Free;
            return new MatchDefinition
            {
                RoomKey = _draftRoom,
                Mode = mode,
                Format = format,
                CustomTeams = _customLayout
            };
        }

        private void RefreshDraft()
        {
            if (_syncing || _apply == null) return;
            MatchDefinition draft = DraftMatch();
            _customTeams.IsVisible = draft.Format == MatchFormat.Custom;
            _lockTeams.IsVisible = GameState.IsTeamMode(draft.Mode);
            TeamLayout layout = LobbyRules.ResolveTeamLayout(draft);
            bool valid = LobbyRules.ValidateDefinition(draft, out string reason) == LobbyResultCode.Ok;

            if (valid && layout.TeamCount > 0
                && (layout.TotalPlayers < _rosterCount
                    || (LobbyRules.ExactTeams(draft)
                        && layout.TotalPlayers > (NetSession.ServerSession?.MaxPlayers ?? 8))))
            {
                valid = false;
                reason = "The matchup must fit the connected players and server limit.";
            }
            if (valid && !TryTimeSeconds(out _))
            {
                valid = false;
                reason = "Time must be minutes from 0 to 1092.25.";
            }
            if (valid && !ushort.TryParse(_goal.Value, NumberStyles.None,
                CultureInfo.InvariantCulture, out _))
            {
                valid = false;
                reason = "Point goal must be a whole number from 0 to 65535.";
            }

            _layoutSummary.Text = !valid
                ? reason
                : layout.TeamCount == 0
                    ? "Free for all"
                    : $"Teams: {layout} · "
                        + (LobbyRules.ExactTeams(draft)
                            ? $"{layout.TotalPlayers} players"
                            : "flexible roster");
            _customTeams.Set(_customLayout.ToString());
            _apply.IsEnabled = valid && NetSession.CanEditLobby && !NetSession.LobbyCommandPending;
            if (!valid) _start.IsEnabled = false;
        }

        private void ApplyMatch()
        {
            if (_rooms.Length == 0 || NetSession.ServerSession is not { } config) return;
            if (!TryTimeSeconds(out ushort seconds)
                || !ushort.TryParse(_goal.Value, NumberStyles.None,
                    CultureInfo.InvariantCulture, out ushort goal))
            {
                _status.Text = "Check the time in minutes and the point goal.";
                return;
            }

            config.Match = DraftMatch() with
            {
                TimeLimitSeconds = seconds,
                PointGoal = goal,
                FriendlyFire = _fire.On,
                AffinityWeapons = _affinity.On,
                ShadowFreeze = _freeze.On,
                HideOpponentHealth = !_opponentHealth.On
            };
            config.RuleFlags = config.Match.Rules
                | (_requireReady.On ? SessionRules.RequireReady : 0)
                | (_join.On ? SessionRules.AllowJoinInProgress : 0)
                | (_lockTeams.On ? SessionRules.LockTeams : 0);
            if (LobbyRules.ValidateDefinition(config.Match, out string reason) != LobbyResultCode.Ok)
            {
                _status.Text = reason;
                return;
            }
            NetSession.SendLobbyCommand(LobbyCommandType.UpdateMatch, configuration: config);
        }

        private void OpenMapPicker()
        {
            if (!NetSession.CanEditLobby || NetSession.LobbyCommandPending) return;
            IReadOnlyList<string> selected = String.IsNullOrWhiteSpace(_draftRoom)
                ? Array.Empty<string>()
                : new[] { _draftRoom };
            var picker = new MapRotationPicker(_rooms, selected, single: true);
            picker.Done += (_, picked) =>
            {
                if (picked.Count > 0)
                {
                    _draftRoom = picked[0];
                    _map.Set(RoomName(_draftRoom));
                    SetPreview(_draftRoom);
                    RefreshDraft();
                }
                ClosePage();
            };
            picker.Cancelled += (_, _) => ClosePage();
            OpenPage(picker);
        }

        private void OpenCustomTeams()
        {
            if (!NetSession.CanEditLobby || NetSession.LobbyCommandPending) return;
            var picker = new CustomTeamPicker(_customLayout,
                NetSession.ServerSession?.MaxPlayers ?? 8);
            picker.Done += (_, layout) =>
            {
                _customLayout = layout;
                _customTeams.Set(layout.ToString());
                RefreshDraft();
                ClosePage();
            };
            picker.Cancelled += (_, _) => ClosePage();
            OpenPage(picker);
        }

        private void OpenPage(Control page)
        {
            _root.Children.Clear();
            _root.Children.Add(page);
            Dispatcher.UIThread.Post(() => page.Focus(), DispatcherPriority.Background);
        }

        private void ClosePage()
        {
            _root.Children.Clear();
            _root.Children.Add(_mainPage);
            Dispatcher.UIThread.Post(() => _map.Focus(), DispatcherPriority.Background);
        }

        private void SetPreview(string room)
        {
            _preview.Source = null;
            _bitmap?.Dispose();
            _bitmap = null;
            try
            {
                string path = ThumbnailGenerator.PathFor(room);
                if (!String.IsNullOrWhiteSpace(room) && File.Exists(path))
                    _bitmap = new Bitmap(path);
            }
            catch (Exception)
            {
                // A thumbnail is presentation only; map validation belongs to the server.
            }
            _preview.Source = _bitmap;
            _preview.IsVisible = _bitmap != null;
        }

        private static string RoomName(string room)
        {
            return Metadata.GetRoomByName(room).Item1?.InGameName ?? room;
        }

        private static string Minutes(ushort seconds)
        {
            double minutes = seconds / 60.0;
            return minutes.ToString(minutes == Math.Truncate(minutes) ? "0" : "0.##",
                CultureInfo.InvariantCulture);
        }

        private bool TryTimeSeconds(out ushort seconds)
        {
            seconds = 0;
            if (!double.TryParse(_time.Value, NumberStyles.Float,
                    CultureInfo.InvariantCulture, out double minutes)
                || !Double.IsFinite(minutes) || minutes < 0
                || minutes * 60 > UInt16.MaxValue)
                return false;
            seconds = (ushort)Math.Round(minutes * 60, MidpointRounding.AwayFromZero);
            return true;
        }

        private MatchFormat SelectedFormat()
        {
            return _matchups[Math.Clamp(_format.Index, 0, _matchups.Length - 1)].Format;
        }

        private static int MatchupIndex(MatchFormat format)
        {
            int index = Array.FindIndex(_matchups, m => m.Format == format);
            return index < 0 ? 0 : index;
        }

        private static int MatchupIndex(MatchDefinition match)
        {
            if (match.Format == MatchFormat.Auto && !GameState.IsTeamMode(match.Mode))
                return MatchupIndex(MatchFormat.FreeForAll);
            return MatchupIndex(match.Format);
        }

        private static int BaseModeIndex(GameMode mode)
        {
            int index = Array.FindIndex(_gameTypes, m => m.Free == mode || m.Team == mode);
            return index < 0 ? 0 : index;
        }
    }

    /// <summary>
    /// Advanced team capacities live off the main lobby so the common match
    /// setup remains one screen. Only Custom opens this page.
    /// </summary>
    internal sealed class CustomTeamPicker : UserControl
    {
        public event EventHandler<TeamLayout>? Done;
        public event EventHandler? Cancelled;

        private readonly ChoiceRow _count;
        private readonly ChoiceRow[] _sizes = new ChoiceRow[4];
        private readonly Note _note = new("");
        private readonly UiMark _use;
        private readonly int _maxPlayers;

        public CustomTeamPicker(TeamLayout current, int maxPlayers)
        {
            _maxPlayers = Math.Clamp(maxPlayers, 2, 8);
            int count = current.IsValid ? current.TeamCount : 2;
            _count = new ChoiceRow("Teams", new[] { "2", "3", "4" }, count - 2);
            var form = new StackPanel { Spacing = 2 };
            form.Children.Add(_count);
            for (int team = 0; team < 4; team++)
            {
                int initial = current.IsValid && team < current.TeamCount
                    ? Math.Max(1, (int)current.Capacity(team)) - 1
                    : 1;
                _sizes[team] = new ChoiceRow($"Team {(char)('A' + team)} size",
                    Enumerable.Range(1, 8).Select(n => n.ToString()).ToArray(), initial);
                _sizes[team].Changed += (_, _) => Refresh();
                form.Children.Add(_sizes[team]);
            }
            form.Children.Add(_note);
            _count.Changed += (_, _) => Refresh();

            var back = new UiMark(UiMark.Shape.Cancel, "back");
            back.Click += (_, _) => Cancelled?.Invoke(this, EventArgs.Empty);
            _use = new UiMark(UiMark.Shape.Accept, "use teams");
            _use.Click += (_, _) =>
            {
                TeamLayout layout = Value();
                if (layout.IsValid && layout.TotalPlayers <= _maxPlayers)
                    Done?.Invoke(this, layout);
            };
            Content = UiLayout.Page(overGame: false, UiLayout.WellSettings,
                "custom teams", strip: null, body: form, no: back, yes: _use);
            Refresh();
        }

        private TeamLayout Value()
        {
            int count = _count.Index + 2;
            return new TeamLayout(
                (byte)count,
                (byte)(_sizes[0].Index + 1),
                (byte)(_sizes[1].Index + 1),
                count > 2 ? (byte)(_sizes[2].Index + 1) : (byte)0,
                count > 3 ? (byte)(_sizes[3].Index + 1) : (byte)0);
        }

        private void Refresh()
        {
            int count = _count.Index + 2;
            for (int team = 0; team < 4; team++)
                _sizes[team].IsVisible = team < count;
            TeamLayout layout = Value();
            bool valid = layout.IsValid && layout.TotalPlayers <= _maxPlayers;
            _note.Text = valid
                ? $"{layout} · {layout.TotalPlayers} player slots"
                : $"Custom teams must use no more than {_maxPlayers} player slots.";
            _note.Foreground = valid ? GuiTheme.TextDimBrush : GuiTheme.WarmBrush;
            _use.IsEnabled = valid;
        }
    }
}
