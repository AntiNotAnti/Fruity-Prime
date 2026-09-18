#if MPHREAD_AVALONIA
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using Avalonia.Threading;
using Avalonia.VisualTree;
using MphRead.Mods.Network;
using MphRead.Mods.Chat;
#if MPHREAD_SHELL
using Avalonia.Headless;
using Avalonia.Input;
#endif
using MphRead.Mods.Multiplayer;

namespace MphRead.Mods.Launcher.Gui
{
    /// <summary>
    /// Screenshots of the front screen, without a screen.
    ///
    /// The launcher is the one part of this program that could not be looked
    /// at from here: the game renders through GL and can be read back
    /// (ScreenCapture), but the launcher is Avalonia, and checking a change to
    /// it meant opening a window on a machine with a display and looking. On a
    /// headless box, or over SSH, or in CI, there was no way to see what a
    /// layout change had actually done -- which is how a control that moves
    /// under the pointer ships.
    ///
    /// Avalonia can measure, arrange and draw a control into a bitmap with no
    /// window involved, which is all a screenshot of a layout needs. So
    /// `-uishot DIR` builds each screen at a fixed size, renders it, and
    /// writes a PNG.
    ///
    /// What this does *not* prove: that a real window manager gives the window
    /// the size asked for, that the fonts on another machine are these ones,
    /// or that anything is clickable. It proves the layout -- which is what
    /// every report about this screen has been about.
    /// </summary>
    internal static class UiCapture
    {
        /// <summary>
        /// The size the screens are photographed at. Close to what the game
        /// window gives them at its own startup size, which is what they are
        /// laid out against (see UiSurface.Scale).
        /// </summary>
        internal static readonly (string Name, Size Size)[] MatrixSizes = {
            ("1280x720", new Size(1280, 720)), ("960x600", new Size(960, 600)),
            ("800x400", new Size(800, 400)), ("1920x1080", new Size(1920, 1080)) };
        private static readonly Size _windowSize = new Size(940, 560);

        public static int RunLobby(string directory)
        {
            if (!GuiLauncher.EnsureSetup()) return 1;
            Directory.CreateDirectory(directory);
            int written = 0;
            Dispatcher.UIThread.Invoke(() =>
            {
                List<string> rooms = RoomList();
                var state = new SessionStatePacket { MatchId = 1, AuthorityEpoch = 1, Policy = ServerSessionPolicy.Lobby, Phase = SessionPhase.Lobby,
                    MaxPlayers = 8, OwnerSlot = 0, Revision = 7, RuleFlags = SessionRules.RequireReady | SessionRules.AllowJoinInProgress,
                    Match = new MatchDefinition { RoomKey = rooms[0], Mode = GameMode.BattleTeams, Format = MatchFormat.FourVsFour,
                        TimeLimitSeconds = 600, PointGoal = 20, ShadowFreeze = true } };
                foreach (var (sample, format, layout, locked) in new[] {
                    ("ffa", MatchFormat.FreeForAll, new TeamLayout(2, 2, 2), false),
                    ("2v2", MatchFormat.TwoVsTwo, new TeamLayout(2, 2, 2), false),
                    ("3v3", MatchFormat.ThreeVsThree, new TeamLayout(2, 3, 3), false),
                    ("4v4", MatchFormat.FourVsFour, new TeamLayout(2, 4, 4), false),
                    ("2v2v2v2", MatchFormat.TwoVsTwoVsTwoVsTwo, new TeamLayout(4, 2, 2, 2, 2), false),
                    ("custom-4v2", MatchFormat.Custom, new TeamLayout(2, 4, 2), false),
                    ("custom-1v1v2v4", MatchFormat.Custom, new TeamLayout(4, 1, 1, 2, 4), false),
                    ("locked", MatchFormat.TwoVsTwoVsTwoVsTwo, new TeamLayout(4, 2, 2, 2, 2), true) })
                {
                    state.Revision++;
                    state.Match = state.Match with { Format = format, CustomTeams = layout, Mode = format == MatchFormat.FreeForAll ? GameMode.Battle : GameMode.BattleTeams };
                    state.RuleFlags = SessionRules.RequireReady | SessionRules.AllowJoinInProgress | (locked ? SessionRules.LockTeams : 0);
                    NetSession.ApplySessionState(state);
                    var roster = RosterPacket.Create(); roster.Count = (byte)layout.TotalPlayers; roster.Revision = state.Revision;
                    roster.MatchId = 1; roster.AuthorityEpoch = 1; roster.SessionRevision = state.Revision;
                    int[] counts = new int[4];
                    for (int i = 0; i < roster.Count; i++)
                    {
                        roster.Slots[i] = (byte)i; roster.Names[i] = i == 0 ? "Jarrett" : $"Player {i + 1}";
                        roster.Generations[i] = 1;
                        roster.Hunters[i] = (byte)(i % 7); roster.Colors[i] = (byte)(i % 4);
                        roster.Teams[i] = TeamRules.ChooseTeam(layout, counts); counts[roster.Teams[i]]++;
                        if (format == MatchFormat.FreeForAll) roster.Teams[i] = -1;
                        roster.LobbyReady[i] = i < 5; roster.Pings[i] = (ushort)(23 + 11 * i);
                    }
                    NetSession.ApplyRoster(roster);
                    foreach (var (name, size) in MatrixSizes)
                    {
                        var lobby = new LobbyScreen(rooms); lobby.Suspend();
                        if (Capture(lobby, Path.Combine(directory, $"lobby-{sample}-{name}.png"), size, surfaceScale: true, checkLayout: true)) written++;
                    }
                }
                NetSession.Stop();
            });
            Console.WriteLine($"[lobbyshot] wrote {written} layouts to {directory}");
            return written == 8 * MatrixSizes.Length ? 0 : 1;
        }

        internal static readonly (string Name, Size Size)[] OnlineSizes = MatrixSizes.Concat(new[] {
            ("phone-portrait", new Size(430, 860)), ("phone-landscape", new Size(860, 430)) }).ToArray();

        // Test-only state injection; these fixtures never open a socket or mutate the wire protocol.
        internal static void LobbyFixture(string sample, IReadOnlyList<string> rooms)
        {
            NetSession.Stop();
            bool guest = sample.StartsWith("guest") || sample == "host-transfer";
            typeof(NetSession).GetProperty(nameof(NetSession.Role))!.SetValue(null, NetRole.Client);
            typeof(NetSession).GetProperty(nameof(NetSession.LocalSlot))!.SetValue(null, guest ? 1 : 0);
            bool teams = sample.Contains("team") || sample.Contains("custom") || sample == "full-roster";
            bool custom = sample.Contains("custom");
            int count = sample == "single-player" ? 1 : sample == "full-roster" ? 8 : 4;
            var state = new SessionStatePacket { AuthorityEpoch = 1, MatchId = 1, Revision = 1,
                Policy = ServerSessionPolicy.Lobby, Phase = SessionPhase.Lobby, OwnerSlot = 0, MaxPlayers = 8,
                RuleFlags = SessionRules.RequireReady | SessionRules.AllowJoinInProgress,
                Match = new MatchDefinition { RoomKey = rooms[0], Mode = teams ? GameMode.BattleTeams : GameMode.Battle,
                    Format = custom ? MatchFormat.Custom : teams ? MatchFormat.FourVsFour : MatchFormat.FreeForAll,
                    CustomTeams = new TeamLayout(4, 1, 1, 2, 4), TimeLimitSeconds = 420, PointGoal = 7 } };
            NetSession.ApplySessionState(state);
            var roster = RosterPacket.Create(); roster.Count = (byte)count; roster.Revision = 1;
            roster.MatchId = 1; roster.AuthorityEpoch = 1; roster.SessionRevision = 1;
            for (int i = 0; i < count; i++)
            {
                roster.Slots[i] = (byte)i; roster.Names[i] = i == NetSession.LocalSlot ? "Your player" : $"Player {i + 1}";
                roster.Generations[i] = 1; roster.Hunters[i] = (byte)i; roster.Colors[i] = (byte)(i % 4);
                roster.Teams[i] = (sbyte)(teams ? i % (custom ? 4 : 2) : -1);
                roster.Pings[i] = (ushort)(24 + i * 17);
                roster.LobbyReady[i] = sample == "all-ready" || (sample != "not-ready" && i < 2);
            }
            NetSession.ApplyRoster(roster);
            NetChat.Clear();
            for (int i = 0; i < 24; i++) NetChat.Remember(new ChatPacket {
                Kind = i % 6 == 0 ? ChatPacket.KindSystem : ChatPacket.KindSay, Name = "Player 2",
                Text = i % 6 == 0 ? "A player joined the lobby." : $"Message {i + 1}: Ready for another match?" });
            if (sample == "connection-lost") typeof(NetSession).GetProperty(nameof(NetSession.ConnectionLost))!.SetValue(null, true);
            if (sample == "command-pending") NetSession.SendLobbyCommand(LobbyCommandType.SetReady, ready: true);
        }

        private static int RunOnlineAndLobbyStates(string directory)
        {
            int failures = 0, written = 0;
            Dispatcher.UIThread.Invoke(() =>
            {
                var rooms = RoomList();
                foreach (string sample in new[] { "guest-ffa", "host-ffa", "guest-team", "host-team", "guest-custom", "host-custom",
                    "not-ready", "all-ready", "command-pending", "connection-lost", "full-roster", "single-player", "match-edit", "match-validation-error", "host-transfer" })
                foreach (var (name, size) in OnlineSizes)
                {
                    LobbyFixture(sample, rooms);
                    var lobby = new LobbyScreen(rooms, new LobbyContext("Friday night lobby", "play.example:27888"));
                    lobby.ShowCaptureState(sample);
                    if (Capture(lobby, Path.Combine(directory, $"lobby-state-{sample}-{name}.png"), size, surfaceScale: true, checkLayout: true)) written++; else failures++;
                    NetSession.Stop();
                }
                foreach (string sample in new[] { "lobby", "starting", "in-match", "full", "offline", "wrong-protocol", "legacy", "high-ping", "no-servers", "directory-failure" })
                foreach (var (name, size) in OnlineSizes)
                {
                    var status = new ServerStatus { Online = sample != "offline", LobbyEnabled = sample != "legacy", Legacy = sample == "legacy",
                        Protocol = (byte)(sample == "wrong-protocol" ? 1 : NetConfig.ProtocolVersion),
                        Phase = sample == "starting" ? SessionPhase.Starting : sample == "in-match" ? SessionPhase.InMatch : SessionPhase.Lobby,
                        AllowJoinInProgress = true, Players = sample == "full" ? 8 : 3, MaxPlayers = 8,
                        RoomKey = rooms[0], Mode = GameMode.Battle, Format = MatchFormat.FreeForAll, Latency = sample == "high-ping" ? 320 : 28 };
                    var browser = new PlayScreen(new MenuSettings(), rooms, captureOnly: true);
                    browser.ShowCaptureServers(sample is "no-servers" or "directory-failure" ? Array.Empty<(string, ServerStatus)>() : new[] { ("Friday night lobby", status) });
                    browser.ShowCaptureOnlineState(sample);
                    if (Capture(browser, Path.Combine(directory, $"online-{sample}-{name}.png"), size, surfaceScale: true, checkLayout: true)) written++; else failures++;
                }
            });
            Console.WriteLine($"[online-lobby-matrix] {written} states rendered; {failures} failures.");
            return failures;
        }

        public static int RunReplay(string directory, string replay)
        {
            if (!GuiLauncher.EnsureSetup()) return 1;
            Directory.CreateDirectory(directory);
            if (!DemoPlayback.Join(replay)) { Console.WriteLine(DemoPlayback.LastError); return 1; }
            int written = 0;
            try
            {
                Dispatcher.UIThread.Invoke(() =>
                {
                    foreach (var (name, size) in new[] { ("desktop", new Size(1280, 720)), ("phone", new Size(960, 540)), ("short", new Size(800, 400)) })
                    {
                        if (Capture(new PauseMenuView(offerWindowMode: true), Path.Combine(directory, "replay-controls-" + name + ".png"), size)) written++;
                        if (Capture(new PlayScreen(new MenuSettings(), RoomList(), PlayScreen.Face.Clips), Path.Combine(directory, "replay-library-" + name + ".png"), size)) written++;
                    }
                });
            }
            finally { DemoPlayback.Stop(); NetSession.Stop(); }
            Console.WriteLine($"[replayshot] {written} layouts written to {directory}");
            return written == 6 ? 0 : 1;
        }

        public static int RunMatrix(string directory)
        {
            if (!GuiLauncher.EnsureSetup()) return 1;
            Directory.CreateDirectory(directory);
            int failed = 0, written = 0;
            Dispatcher.UIThread.Invoke(() =>
            {
                foreach (var (suffix, size) in MatrixSizes)
                {
                    foreach (var (name, view, _) in Screens(new MenuSettings(), RoomList()))
                    {
                        if (name == "pausemenu-small" || name == "serverbrowser") continue;
                        if (name == "play-online" && view is PlayScreen online)
                            online.ShowCaptureServers(_sampleServers.Select((sample, i) => (sample.Item1,
                                new ServerStatus { Online = true, RoomKey = sample.Item2, Mode = sample.Item3,
                                    Players = sample.Item4, MaxPlayers = 8, Latency = sample.Item5, Phase = (SessionPhase)(i % 4) })));
                        if (name == "play-clips" && view is PlayScreen library) library.ShowCaptureReplays(SampleReplays());
                        if (Capture(view, Path.Combine(directory, name + "-" + suffix + ".png"), size, surfaceScale: true, checkLayout: true)) written++; else failed++;
                    }
                    foreach (string section in new[] { "Display", "HUD and accessibility", "Audio", "Mouse and stylus", "Controller", "Replay", "Profile", "Advanced / Support" })
                    {
                        var settings = new SettingsView(new MenuSettings()); settings.ShowSection(section);
                        string name = section.Replace(" / ", "-").Replace(' ', '-').ToLowerInvariant();
                        if (Capture(settings, Path.Combine(directory, "settings-" + name + "-" + suffix + ".png"), size, surfaceScale: true, checkLayout: true)) written++; else failed++;
                    }
                    foreach (string section in new[] { "Playback", "Camera", "Events", "Clip" })
                    {
                        var replay = new PauseMenuView(true, replayPreview: true); replay.ShowReplaySection(section);
                        if (Capture(replay, Path.Combine(directory, "replay-" + section.ToLowerInvariant() + "-" + suffix + ".png"), size, surfaceScale: true, checkLayout: true)) written++; else failed++;
                    }
                    var originalVisual = Render.VisualOptions.Current;
                    try
                    {
                        Render.VisualOptions.Current = originalVisual with { TextScale = 125, UiScale = 150, HighContrast = true };
                        var accessible = new SettingsView(new MenuSettings()); accessible.ShowSection("HUD and accessibility");
                        if (Capture(accessible, Path.Combine(directory, "settings-accessibility-large-" + suffix + ".png"), size, surfaceScale: true, checkLayout: true)) written++; else failed++;
                    }
                    finally { Render.VisualOptions.Current = originalVisual; }
                    var target = new TextBox { Text = "Player name", MaxLength = 32 };
                    var keyboard = new ControllerKeyboard(target, () => { }, captureOnly: true);
                    if (Capture(keyboard.NavigationRoot, Path.Combine(directory, "controller-keyboard-" + suffix + ".png"), size, surfaceScale: true, checkLayout: true)) written++; else failed++;
                    keyboard.Close(false);
                    var studio = new MapStudioScreen(); studio.LoadCaptureSample();
                    if (Capture(studio, Path.Combine(directory, "map-studio-" + suffix + ".png"), size, surfaceScale: true, checkLayout: true)) written++; else failed++;
                }
            });
            failed += RunLobby(directory);
            failed += RunOnlineAndLobbyStates(directory);
            Console.WriteLine($"[uimatrix] {written} launcher layouts plus lobby matrix; {failed} failures.");
            return failed == 0 ? 0 : 1;
        }

        public static int Run(string directory, bool browserOnly = false)
        {
            if (!GuiLauncher.EnsureSetup())
            {
                Console.WriteLine("[uishot] no Avalonia backend on this machine; nothing captured");
                return 1;
            }
            Directory.CreateDirectory(directory);
            // The front screen's Share button only exists where something can
            // receive a file, which today is Android alone -- so without a
            // stand-in the one corner this tool was made to check could never
            // be photographed as a phone draws it. Same reason as SampleDemos
            // below, and it is still only offered when real logs exist.
            Mods.LogShare.Current ??= new CaptureLogShare();
            int written = 0;
            bool browserPassed = true;
            // On the toolkit's own thread, and drained afterwards: the views
            // post work to the dispatcher as they are built (the front screen
            // focuses its first control that way), and a render before that
            // has run is a picture of a half-built screen.
            Dispatcher.UIThread.Invoke(() =>
            {
                if (!browserOnly)
                {
                    var settings = new MenuSettings();
                    List<string> rooms = RoomList();
                    foreach ((string name, Control view, Size size) in Screens(settings, rooms))
                    {
                        string path = Path.Combine(directory, $"{name}.png");
                        if (Capture(view, path, size))
                        {
                            written++;
                            Console.WriteLine($"[uishot] {path}");
                        }
                    }
                    var visibility = new SettingsView(settings);
                    string visibilityPath = Path.Combine(directory, "settings-visibility.png");
                    if (Capture(visibility, visibilityPath, _windowSize, afterLayout: visibility.ShowVisibilitySettings))
                    {
                        written++;
                        Console.WriteLine($"[uishot] {visibilityPath}");
                    }
                }
#if MPHREAD_SHELL
                browserPassed = CaptureRomBrowser(directory, ref written);
#endif
            });
            Console.WriteLine($"[uishot] {written} screen(s) written to {directory}");
            return written > 0 && browserPassed ? 0 : 1;
        }

#if MPHREAD_SHELL
        /// <summary>
        /// Exercise the actual setup button and browser controls with headless
        /// pointer/key input. No game data is needed and confirmation is not
        /// pressed, so the extractor never starts.
        /// </summary>
        private static bool CaptureRomBrowser(string output, ref int written)
        {
            string sample = Path.Combine(Path.GetTempPath(),
                "FruityPrime-RomBrowser-" + Guid.NewGuid().ToString("N"));
            string oldDirectory = LauncherPrefs.LastRomDirectory;
            string oldPrefsDirectory = LauncherPrefs.Directory;
            Window? window = null;
            try
            {
                var v = OpenTK.Windowing.GraphicsLibraryFramework.Keys.V;
                if (!Shell.IsRomPasteShortcut(v, control: true, command: false,
                        alt: false, macOS: false)
                    || !Shell.IsRomPasteShortcut(v, control: false, command: true,
                        alt: false, macOS: true)
                    || Shell.IsRomPasteShortcut(v, control: true, command: false,
                        alt: true, macOS: false)
                    || Shell.IsRomPasteShortcut(v, control: false, command: true,
                        alt: true, macOS: true))
                {
                    throw new InvalidOperationException("The desktop paste shortcut was misidentified.");
                }
                string games = Path.Combine(sample, "Games");
                string transient = Path.Combine(sample, "Gone");
                Directory.CreateDirectory(games);
                Directory.CreateDirectory(transient);
                File.WriteAllBytes(Path.Combine(sample, "PRIME.NDS"), Array.Empty<byte>());
                File.WriteAllBytes(Path.Combine(sample, "ignore.txt"), Array.Empty<byte>());
                File.WriteAllBytes(Path.Combine(games, "hunters.nds"), Array.Empty<byte>());
                LauncherPrefs.LastRomDirectory = sample;
                var setup = new SetupScreen();
                window = new Window
                {
                    Width = _windowSize.Width,
                    Height = _windowSize.Height,
                    Background = GuiTheme.PanelBrush,
                    RequestedThemeVariant = Avalonia.Styling.ThemeVariant.Dark,
                    SystemDecorations = SystemDecorations.None,
                    ShowInTaskbar = false,
                    ShowActivated = false,
                    WindowStartupLocation = WindowStartupLocation.Manual,
                    Position = new PixelPoint(-4000, -4000),
                    Content = setup
                };
                window.Show();
                Drain(window, _windowSize);
                Click(window, Find<UiMark>(window,
                    mark => mark.Label == "Choose .nds file"));
                Drain(window, _windowSize);
                if (setup.Content is not RomFileBrowser browser
                    || Find<UiListRow>(window, row => row.Choice is RomFileEntry
                        { Name: "PRIME.NDS" }) == null)
                {
                    throw new InvalidOperationException("The setup button did not show ROM rows.");
                }
                if (Find<UiListRow>(window, row => row.Choice is RomFileEntry
                    { Name: "ignore.txt" }) != null)
                {
                    throw new InvalidOperationException("Unrelated files reached the browser.");
                }
                SaveBrowserShot(window, output, "rom-browser", _windowSize, ref written);

                // The row remains on screen when a drive/folder disappears.
                // It must be usable again when that path comes back.
                Directory.Delete(transient);
                Click(window, Find<UiListRow>(window, row => row.Choice is RomFileEntry
                    { Name: "Gone" }));
                Drain(window, _windowSize);
                if (Find<Note>(window, note => note.Text?.StartsWith(
                    "This folder could not be opened:", StringComparison.Ordinal) == true
                    && note.IsVisible) == null)
                {
                    throw new InvalidOperationException("A removed folder had no inline error.");
                }
                Directory.CreateDirectory(transient);
                File.WriteAllBytes(Path.Combine(transient, "return.nds"), Array.Empty<byte>());
                Click(window, Find<UiListRow>(window, row => row.Choice is RomFileEntry
                    { Name: "Gone" }));
                Drain(window, _windowSize);
                if (Find<UiListRow>(window, row => row.Choice is RomFileEntry
                    { Name: "return.nds" }) == null)
                {
                    throw new InvalidOperationException("A restored folder could not be retried.");
                }
                window.KeyPress(Key.Escape, RawInputModifiers.None, PhysicalKey.None, "");
                window.KeyRelease(Key.Escape, RawInputModifiers.None, PhysicalKey.None, "");
                Drain(window, _windowSize);

                Click(window, Find<UiListRow>(window, row => row.Choice is RomFileEntry
                    { Name: "Games" }));
                Drain(window, _windowSize);
                if (Find<UiListRow>(window, row => row.Choice is RomFileEntry
                    { Name: "hunters.nds" }) == null)
                {
                    throw new InvalidOperationException("Directory activation did not navigate.");
                }
                Click(window, Find<UiListRow>(window, row => row.Choice is RomFileEntry
                    { Name: "hunters.nds" }));
                Drain(window, _windowSize);
                if (Find<UiMark>(window, mark => mark.Label == "use file")?.IsEnabled != true)
                {
                    throw new InvalidOperationException("ROM activation did not select the file.");
                }
                if (browser.PastePath("unfocused"))
                {
                    throw new InvalidOperationException("Paste reached an unfocused path field.");
                }
                window.KeyPress(Key.Up, RawInputModifiers.None, PhysicalKey.None, "");
                window.KeyRelease(Key.Up, RawInputModifiers.None, PhysicalKey.None, "");
                Drain(window, _windowSize);
                if (Find<UiMark>(window, mark => mark.Label == "use file")?.IsEnabled != false)
                {
                    throw new InvalidOperationException("The parent row left a ROM selected.");
                }
                Click(window, Find<UiListRow>(window, row => row.Choice is RomFileEntry
                    { Name: "hunters.nds" }));
                Drain(window, _windowSize);

                window.KeyPress(Key.Escape, RawInputModifiers.None, PhysicalKey.None, "");
                window.KeyRelease(Key.Escape, RawInputModifiers.None, PhysicalKey.None, "");
                Drain(window, _windowSize);
                if (Find<UiListRow>(window, row => row.Choice is RomFileEntry
                    { Name: "PRIME.NDS" }) == null)
                {
                    throw new InvalidOperationException("Escape did not navigate to the parent.");
                }

                TextBox path = Find<TextBox>(window, _ => true)
                    ?? throw new InvalidOperationException("The path field is missing.");
                path.Text = Path.Combine(sample, "ignore.txt");
                path.Focus();
                window.KeyPress(Key.Enter, RawInputModifiers.None, PhysicalKey.None, "");
                window.KeyRelease(Key.Enter, RawInputModifiers.None, PhysicalKey.None, "");
                Drain(window, _windowSize);
                if (Find<Note>(window, note => note.Text == "Choose a .nds file."
                    && note.IsVisible) == null)
                {
                    throw new InvalidOperationException("An unrelated path had no inline error.");
                }
                path.Text = "selected text";
                path.SelectAll();
                if (!browser.PastePath("") || path.Text != "selected text")
                {
                    throw new InvalidOperationException("Empty clipboard text changed the path.");
                }
                path.SelectAll();
                path.Focus();
                if (!browser.PastePath(Path.Combine(sample, "PRIME.NDS"))
                    || path.Text != Path.Combine(sample, "PRIME.NDS"))
                {
                    throw new InvalidOperationException("Pasted text did not reach the path field.");
                }
                window.KeyPress(Key.Enter, RawInputModifiers.None, PhysicalKey.None, "");
                window.KeyRelease(Key.Enter, RawInputModifiers.None, PhysicalKey.None, "");
                Drain(window, _windowSize);
                if (Find<UiMark>(window, mark => mark.Label == "use file")?.IsEnabled != true)
                {
                    throw new InvalidOperationException("Enter did not accept a pasted ROM path.");
                }

                var large = new Size(1280, 720);
                window.Width = large.Width;
                window.Height = large.Height;
                Drain(window, large);
                SaveBrowserShot(window, output, "rom-browser-large", large, ref written);
                Click(window, Find<UiMark>(window, mark => mark.Label == "cancel"));
                Drain(window, large);
                if (ReferenceEquals(setup.Content, browser))
                {
                    throw new InvalidOperationException("Cancel did not return to setup.");
                }

                // Confirmation is checked on a detached browser so the fake
                // file is never passed to the real cartridge extractor.
                var standalone = new RomFileBrowser(sample);
                string? chosen = null;
                standalone.Selected += path => chosen = path;
                window.Content = standalone;
                Drain(window, large);
                Click(window, Find<UiListRow>(window, row => row.Choice is RomFileEntry
                    { Name: "PRIME.NDS" }));
                Drain(window, large);
                Click(window, Find<UiMark>(window, mark => mark.Label == "use file"));
                if (chosen != Path.Combine(sample, "PRIME.NDS"))
                {
                    throw new InvalidOperationException("Confirmation did not return the ROM path.");
                }

                LauncherPrefs.Directory = sample;
                LauncherPrefs.LastRomDirectory = games;
                LauncherPrefs.Save();
                LauncherPrefs.LastRomDirectory = "";
                LauncherPrefs.Load();
                if (LauncherPrefs.LastRomDirectory != games)
                {
                    throw new InvalidOperationException("The last ROM folder was not persisted.");
                }
                Console.WriteLine("[uishot] ROM browser navigation, selection, path and cancel passed");
                return true;
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[uishot] ROM browser smoke failed: {ex.Message}");
                return false;
            }
            finally
            {
                window?.Close();
                LauncherPrefs.LastRomDirectory = oldDirectory;
                LauncherPrefs.Directory = oldPrefsDirectory;
                string tempRoot = Path.GetFullPath(Path.GetTempPath());
                string fullSample = Path.GetFullPath(sample);
                if (fullSample.StartsWith(tempRoot.TrimEnd(Path.DirectorySeparatorChar)
                        + Path.DirectorySeparatorChar, OperatingSystem.IsWindows()
                            ? StringComparison.OrdinalIgnoreCase : StringComparison.Ordinal)
                    && Path.GetFileName(fullSample).StartsWith(
                        "FruityPrime-RomBrowser-", StringComparison.Ordinal)
                    && Directory.Exists(fullSample))
                {
                    Directory.Delete(fullSample, recursive: true);
                }
            }
        }

        private static T? Find<T>(Control root, Func<T, bool> match) where T : Control
        {
            foreach (Visual visual in root.GetVisualDescendants())
            {
                if (visual is T control && match(control))
                {
                    return control;
                }
            }
            return null;
        }

        private static void Click(Window window, Control? control)
        {
            if (control == null)
            {
                throw new InvalidOperationException("A browser control could not be found.");
            }
            Point? point = control.TranslatePoint(
                new Point(control.Bounds.Width / 2, control.Bounds.Height / 2), window);
            if (point == null)
            {
                throw new InvalidOperationException("A browser control has no screen position.");
            }
            window.MouseMove(point.Value, RawInputModifiers.None);
            window.MouseDown(point.Value, MouseButton.Left,
                RawInputModifiers.LeftMouseButton);
            window.MouseUp(point.Value, MouseButton.Left, RawInputModifiers.None);
        }

        private static void Drain(Window window, Size size)
        {
            for (int i = 0; i < 8; i++)
            {
                Dispatcher.UIThread.RunJobs();
            }
            window.Measure(size);
            window.Arrange(new Rect(size));
            Dispatcher.UIThread.RunJobs();
        }

        private static void SaveBrowserShot(Window window, string output,
            string name, Size size, ref int written)
        {
            string path = Path.Combine(output, name + ".png");
            using var bitmap = new RenderTargetBitmap(
                new PixelSize((int)size.Width, (int)size.Height), new Vector(96, 96));
            bitmap.Render(window);
            bitmap.Save(path);
            written++;
            Console.WriteLine($"[uishot] {path}");
        }
#endif

        private static List<string> RoomList()
        {
            var rooms = new List<string>();
            try
            {
                foreach (RoomMetadata meta in Metadata.RoomMetadata.Values)
                {
                    if (meta.Multiplayer)
                    {
                        rooms.Add(meta.Name);
                    }
                }
            }
            catch (Exception)
            {
                // No game files here. The screens still lay out; the map rows
                // are simply empty, which is itself worth being able to see.
            }
            rooms.Sort(StringComparer.OrdinalIgnoreCase);
            return rooms;
        }

        private static IEnumerable<(string, Control, Size)> Screens(MenuSettings settings,
            IReadOnlyList<string> rooms)
        {
            yield return ("start", new StartScreen(settings, rooms), _windowSize);
            // Every face of the one screen that replaced seven. They share a
            // layout and nothing else -- the list, the settings beside it and
            // the word on the tick are different on each -- so one picture of
            // it would prove nothing about the other three.
            yield return ("play-online",
                new PlayScreen(settings, rooms, PlayScreen.Face.Online, captureOnly: true), _windowSize);
            yield return ("play-offline",
                new PlayScreen(settings, rooms, PlayScreen.Face.Offline), _windowSize);
            yield return ("play-story",
                new PlayScreen(settings, rooms, PlayScreen.Face.Story), _windowSize);
            yield return ("play-clips",
                new PlayScreen(settings, rooms, PlayScreen.Face.Clips), _windowSize);
            yield return ("play-vote",
                new PlayScreen(settings, rooms, PlayScreen.Face.Vote, overGame: true),
                _windowSize);
            // Both faces of creating a server, and the map list it opens.
            // The dedicated one is a separate picture because the rows it
            // hides and the warning it raises are the whole difference between
            // the two, and neither shows on the other.
            yield return ("create-server", new CreateServerScreen(rooms), _windowSize);
            var dedicated = new CreateServerScreen(rooms);
            dedicated.ShowDedicated();
            yield return ("create-server-dedicated", dedicated, _windowSize);
            yield return ("create-server-maps",
                new MapRotationPicker(rooms, Array.Empty<string>()), _windowSize);
            yield return ("create-server-hosts", new HostPicker(Fleet(), asking: false),
                _windowSize);
            yield return ("settings", new SettingsView(settings), _windowSize);
            var credits = new SettingsView(settings);
            credits.ShowSection("Profile");
            yield return ("settings-player", credits, _windowSize);
            yield return ("setup", new SetupScreen(), _windowSize);
            yield return ("confirm",
                new ConfirmScreen($"Quit {Mods.Branding.Name}?"), _windowSize);
            yield return ("pausemenu", new PauseMenuView(offerWindowMode: true), _windowSize);
            // Deliberately shorter than the menu's own content, and shorter
            // than the game window is now allowed to be. The pause menu is
            // laid over the game window, so its host is whatever size the
            // player dragged that to, and entries drawn off the bottom edge
            // are a player who cannot leave the match. This is the check that
            // the column shrinks to carry them.
            yield return ("pausemenu-small", new PauseMenuView(offerWindowMode: true),
                new Size(560, 320));
            yield return ("serverbrowser", ServerList(), _windowSize);
        }

        /// <summary>
        /// The fleet as the host picker draws it, without asking the network:
        /// one that will run a match, one too old to say so, and one with no
        /// directory at all. The three states are the whole point of the
        /// screen, and a capture that queried the real directory would
        /// photograph whichever of them happened to be true that morning.
        /// </summary>
        private static List<HostCandidate> Fleet()
        {
            return new List<HostCandidate>
            {
                new() { Label = "net.livetek.fr", Host = "net.livetek.fr", Port = 27889,
                    Answered = true, CanHost = true, Latency = 3 },
                new() { Label = "Fruity Prime - West Europe", Host = "20.16.135.109",
                    Port = 27889, Answered = true, CanHost = null, Latency = 39 },
                new() { Label = "Fruity Prime - Japan", Host = "13.78.14.98", Port = 27889,
                    Answered = false, CanHost = null, Latency = -1 }
            };
        }

        /// <summary>
        /// Somewhere for the Share button to point while it is being
        /// photographed. Nothing is built and nothing is sent: a capture has
        /// nobody to press it.
        /// </summary>
        private sealed class CaptureLogShare : Mods.ILogShare
        {
            public string StagingPath(string fileName) =>
                Path.Combine(Path.GetTempPath(), fileName);

            public bool Share(string path, string subject, out string error)
            {
                error = "there is nothing to share to on this platform";
                return false;
            }
        }

        /// <summary>
        /// The browser's table, at the width the panel gives it, with rows
        /// standing in for servers that are not up.
        ///
        /// Built here rather than reached through the play screen because that
        /// one only fills in when a directory answers -- and the fault this is
        /// for (a map name wrapping onto the row below, headings running into
        /// each other) is a property of the columns and the width, not of any
        /// real server. Both widths are drawn, so a narrow row is checked too.
        /// </summary>
        private static Control ServerList()
        {
            var stack = new StackPanel { Spacing = 18, Margin = new Thickness(12) };
            foreach (double width in new[] { 600.0, 400.0 })
            {
                var list = new StackPanel { Spacing = 2, Width = width };
                list.Children.Add(new ServerHeader());
                foreach ((string name, string room, GameMode mode, int players, int ping) in _sampleServers)
                {
                    var row = new ServerRow(name, "203.0.113.7:27888");
                    row.SetStatus(new ServerStatus
                    {
                        Online = true,
                        RoomKey = room,
                        Mode = mode,
                        Players = players,
                        MaxPlayers = 8,
                        Latency = ping
                    });
                    list.Children.Add(row);
                }
                stack.Children.Add(list);
            }
            return stack;
        }

        private static DemoRecording[] SampleReplays()
        {
            var date = new DateTime(2026, 9, 16, 18, 30, 0);
            return new[] {
                new DemoRecording("proving-ground.fpdemo", "MP3 PROVING GROUND", date, 12000000,
                    new ReplayMetadata { RoomKey = "MP3 PROVING GROUND", Mode = GameMode.BattleTeams,
                        Type = ReplayType.FullMatch, Integrity = ReplayIntegrity.Healthy,
                        Players = new[] { new ReplayPlayerInfo(0, 0, 0, "Player one"), new ReplayPlayerInfo(1, 1, 1, "Player two") } }, duration: 36000),
                new DemoRecording("last-round.fpdemo", "MP7 PROCESSOR CORE", date, 800000,
                    new ReplayMetadata { Mode = GameMode.PrimeHunter, Type = ReplayType.Clip, Integrity = ReplayIntegrity.Unknown }, duration: 3600),
                new DemoRecording("interrupted.fpdemo.part", "MP2 HARVESTER", date, 2400000,
                    compatibility: ReplayOpenResult.Truncated)
            };
        }

        private static readonly (string, string, GameMode, int, int)[] _sampleServers =
        {
            ("net.livetek.fr", "MP3 PROVING GROUND", GameMode.Battle, 3, 41),
            ("A very long server name indeed", "MP7 PROCESSOR CORE", GameMode.PrimeHunter, 8, 152),
            ("lan", "MP2 HARVESTER", GameMode.Bounty, 1, 2)
        };

        /// <summary>
        /// Render one screen.
        ///
        /// Through a real <see cref="Window"/>, not by laying the control out
        /// on its own. Avalonia resolves styles through the visual tree's
        /// style host, and a control with no window above it has none: it
        /// measures, arranges and renders perfectly happily and comes out a
        /// flat rectangle of the background colour, which is exactly what the
        /// first attempt at this produced. The window is what connects the
        /// tree to the Application's styles.
        ///
        /// It is shown, because a window that has never been shown has no
        /// layout pass behind it -- but shown *off the side of the display*
        /// and without taking focus, so a capture run does not steal the
        /// pointer or flash a window per screen.
        /// </summary>
        private static bool CheckLayout(Window window, Control view, string name)
        {
            bool passed = true;
            // Exercise the same focus/scroll path as controller navigation. Offscreen
            // scroll content is valid only when focusing it brings it into view.
            var controls = view.GetVisualDescendants().OfType<Control>().Where(c => c.Focusable
                && c.IsEffectivelyVisible && c.IsEffectivelyEnabled && c.IsHitTestVisible
                && c is not UserControl && c is not ScrollViewer && c.Bounds.Width > 0 && c.Bounds.Height > 0).ToArray();
            foreach (var control in controls)
            {
                FocusNavigator.Focus(control);
                window.UpdateLayout(); Dispatcher.UIThread.RunJobs(); window.UpdateLayout();
                var point = control.TranslatePoint(new Point(control.Bounds.Width / 2, control.Bounds.Height / 2), window);
                var edge = control.TranslatePoint(new Point(control.Bounds.Width, control.Bounds.Height), window);
                if (point == null || point.Value.X < 0 || point.Value.Y < 0
                    || point.Value.X > window.Bounds.Width || point.Value.Y > window.Bounds.Height
                    || edge == null || edge.Value.X > window.Bounds.Width + 1 || edge.Value.Y > window.Bounds.Height + 1)
                {
                    Console.WriteLine($"[layout] {name}: unreachable {control.GetType().Name} ({point})"); passed = false;
                }
                foreach (var scroll in control.GetVisualAncestors().OfType<ScrollViewer>())
                {
                    var within = control.TranslatePoint(new Point(control.Bounds.Width / 2, control.Bounds.Height / 2), scroll);
                    if (within != null && (within.Value.Y < -1 || within.Value.Y > scroll.Bounds.Height + 1))
                    { Console.WriteLine($"[layout] {name}: {control.GetType().Name} remains clipped by its scroller"); passed = false; }
                }
            }
            if (controls.Length == 0) { Console.WriteLine($"[layout] {name}: no focusable controls"); passed = false; }
            return passed;
        }

        internal static bool Capture(Control view, string path, Size size, bool surfaceScale = false,
            bool checkLayout = false, Action? afterLayout = null)
        {
            Window? window = null;
            try
            {
                window = new Window
                {
                    Width = size.Width,
                    Height = size.Height,
                    Background = GuiTheme.PanelBrush,
                    RequestedThemeVariant = Avalonia.Styling.ThemeVariant.Dark,
                    SystemDecorations = SystemDecorations.None,
                    ShowInTaskbar = false,
                    ShowActivated = false,
                    WindowStartupLocation = WindowStartupLocation.Manual,
                    Position = new PixelPoint(-4000, -4000),
                    Content = surfaceScale ? new LayoutTransformControl {
                        LayoutTransform = new Avalonia.Media.ScaleTransform(UiLayout.Factor(size.Width, size.Height), UiLayout.Factor(size.Width, size.Height)),
                        HorizontalAlignment = Avalonia.Layout.HorizontalAlignment.Stretch,
                        VerticalAlignment = Avalonia.Layout.VerticalAlignment.Stretch, Child = view } : view
                };
                window.Show();
                // The views post work to the dispatcher as they are built --
                // the front screen focuses its first control that way, and the
                // map picker loads its pictures -- and a render before that has
                // run is a picture of a half-built screen. Several passes,
                // because one job can queue another.
                for (int i = 0; i < 8; i++)
                {
                    Dispatcher.UIThread.RunJobs();
                }
                window.Measure(size);
                window.Arrange(new Rect(size));
                Dispatcher.UIThread.RunJobs();
                afterLayout?.Invoke();
                window.UpdateLayout();
                Dispatcher.UIThread.RunJobs();
                using var bitmap = new RenderTargetBitmap(
                    new PixelSize((int)size.Width, (int)size.Height),
                    new Vector(96, 96));
                bitmap.Render(window);
                bitmap.Save(path);
                return !checkLayout || CheckLayout(window, view, Path.GetFileNameWithoutExtension(path));
            }
            catch (Exception ex)
            {
                Console.WriteLine($"[shot] {Path.GetFileName(path)} could not be rendered: {ex.Message}");
                return false;
            }
            finally
            {
                window?.Close();
            }
        }

    }
}
#endif
