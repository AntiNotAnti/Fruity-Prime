using System;
using System.Linq;
using System.Globalization;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using Avalonia.VisualTree;
using MphRead.Entities;
using MphRead.Mods.Network;

namespace MphRead.Mods.Launcher.Gui
{
    /// <summary>
    /// What Escape shows during a match.
    ///
    /// The same shape as the front screen -- a column of words in the
    /// bottom-left corner over a dim line saying where you are -- because it
    /// is the front screen's job during a match, and a pause menu that looks
    /// like a different program is a pause menu that has to be read rather
    /// than glanced at. What differs is the backdrop: the scrim alone, so the
    /// match shows through. A networked match cannot be paused, and covering
    /// it with a photograph would be a lie about what the program is doing.
    ///
    /// The entries are not fewer than they were. Voting on a map, going
    /// fullscreen, spectating and recording are things you can only want
    /// *during* a match, so this is the one screen they can live on -- the
    /// list is shorter everywhere else precisely so it can be long here.
    ///
    /// A view rather than a window, because nothing shows it in one any
    /// more: the desktop pushes it onto <see cref="InGameMenu"/>'s stack,
    /// which is rendered into the game window itself, and Android pushes it
    /// onto <see cref="StartScreen"/>'s. One menu either way, so an entry
    /// added here turns up on both.
    ///
    /// It decides nothing itself. Every entry raises an event and the host
    /// acts on it: leaving a match is closing a window on one platform and
    /// swapping two views on the other, and neither belongs in a menu.
    /// </summary>
    internal sealed class PauseMenuView : UserControl
    {
        public event EventHandler? Resumed;
        public event EventHandler? SettingsRequested;
        public event EventHandler? LeaveRequested;
        public event EventHandler? QuitRequested;
        public event EventHandler? FullscreenRequested;
        public event EventHandler? SpectateRequested;
        public event EventHandler? RejoinRequested;
        public event EventHandler? RecordToggleRequested;
        public event EventHandler? VoteMapRequested;

        private readonly UiWord _resume;
        private readonly UiWord _voteYes;
        private readonly UiWord _voteNo;
        private readonly StackPanel _menu;

        private UiTabs? _replayTabs;
        internal void ShowReplaySection(string name) => _replayTabs?.Select(name);

        /// <param name="offerWindowMode">
        /// Show the fullscreen/windowed entry. False on a phone, which has one
        /// window, it is already the whole screen, and there is no F11.
        /// </param>
        public PauseMenuView(bool offerWindowMode, bool replayPreview = false)
        {
            Background = Brushes.Transparent;
            Focusable = true;

            var menu = new StackPanel { Spacing = 14 };
            _menu = menu;
            // Titles only. Every entry here used to say what it did twice --
            // "Quit", "Close FruityPrime" -- and the second saying is what
            // made a seven-line menu tall enough to be cut off by the window
            // it is drawn over.
            _resume = Add(menu, "Resume", () => Resumed?.Invoke(this, EventArgs.Empty));
            _voteYes = Add(menu, "Accept map vote", () => AnswerVote(true));
            _voteNo = Add(menu, "Deny map vote", () => AnswerVote(false));
            RefreshVote();
            var voteTimer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(200) };
            voteTimer.Tick += (_, _) => RefreshVote();
            AttachedToVisualTree += (_, _) => voteTimer.Start();
            DetachedFromVisualTree += (_, _) => voteTimer.Stop();
            if (!(DemoPlayback.IsActive || replayPreview) && NetSession.Active)
            {
                // Offered whenever there is a server to ask, rather than only
                // when a vote could pass right now: the reasons it cannot --
                // somebody else's vote is running, the room is still cooling
                // down -- are things the player wants told to them, and an
                // entry that quietly disappears tells them nothing.
                Add(menu, "Vote map", () => VoteMapRequested?.Invoke(this, EventArgs.Empty));
            }
            if (!(DemoPlayback.IsActive || replayPreview))
            {
                if (SpectatorMode.IsSpectating)
                {
                    Add(menu, "Rejoin match",
                        () => RejoinRequested?.Invoke(this, EventArgs.Empty));
                }
                else if (SpectatorMode.CanSpectate)
                {
                    Add(menu, "Spectate", () => SpectateRequested?.Invoke(this, EventArgs.Empty));
                }
            }
            if (offerWindowMode)
            {
                var window = new UiWord(WindowLabel());
                window.Click += (_, _) =>
                {
                    FullscreenRequested?.Invoke(this, EventArgs.Empty);
                    // The game thread does it on the next frame; reflect it
                    // here straight away so the label is not a lie for 16
                    // milliseconds.
                    window.Text = WindowMode.IsFullscreen ? "Windowed" : "Fullscreen";
                };
                menu.Children.Add(window);
            }
            UiTabs? replayTabs = null;
            Control? replayTime = null;
            if ((DemoPlayback.IsActive || replayPreview))
            {
                var playback = new StackPanel { Spacing = UiMetrics.Spacing };
                var camera = new StackPanel { Spacing = UiMetrics.Spacing };
                var events = new StackPanel { Spacing = UiMetrics.Spacing };
                var clip = new StackPanel { Spacing = UiMetrics.Spacing };
                var sections = new[] { playback, camera, events, clip };
                replayTabs = new UiTabs(new[] { "Playback", "Camera", "Events", "Clip" });
                var pages = new Panel();
                for (int i = 0; i < sections.Length; i++)
                {
                    sections[i].IsVisible = i == 0;
                    pages.Children.Add(sections[i]);
                }
                replayTabs.Changed += (_, _) =>
                {
                    for (int i = 0; i < sections.Length; i++) sections[i].IsVisible = i == replayTabs.Index;
                };
                menu.Children.Add(pages);
                Add(playback, ReplayController.IsPaused ? "Play replay" : "Pause replay", () =>
                { ReplayController.TogglePause(); Resumed?.Invoke(this, EventArgs.Empty); });
                var rate = new ComboBox { ItemsSource = new[] { "0.25x speed", "0.5x speed", "1x speed", "2x speed", "4x speed" },
                    SelectedIndex = Array.IndexOf(ReplayController.Rates, ReplayController.PlaybackRate), Width = 240 };
                rate.SelectionChanged += (_, _) => { if (rate.SelectedIndex >= 0) ReplayController.SetPlaybackRate(ReplayController.Rates[rate.SelectedIndex]); };
                playback.Children.Add(rate);
                Add(playback, "Previous frame", () => { ReplayController.Seek(ReplayController.CurrentFrame > 0 ? ReplayController.CurrentFrame - 1 : 0, false); Resumed?.Invoke(this, EventArgs.Empty); });
                Add(playback, "Restart replay", () => { ReplayController.Restart(); Resumed?.Invoke(this, EventArgs.Empty); });
                Add(playback, "Next frame", () => { ReplayController.StepForward(); Resumed?.Invoke(this, EventArgs.Empty); });
                Add(camera, "Previous player", () => { SpectatorMode.CyclePrevious(); Resumed?.Invoke(this, EventArgs.Empty); });
                Add(camera, "Next player", () => { SpectatorMode.CycleNext(); Resumed?.Invoke(this, EventArgs.Empty); });
                var timeline = new Slider { Minimum = 0, Maximum = Math.Max(1, ReplayController.DurationFrames),
                    Value = ReplayController.CurrentFrame, Width = 320 };
                var selectedTime = new TextBlock { Text = Replay.ReplayHud.Time((uint)timeline.Value) + " / " + Replay.ReplayHud.Time(ReplayController.DurationFrames), Foreground = GuiTheme.TextDimBrush };
                timeline.PropertyChanged += (_, e) => { if (e.Property == RangeBase.ValueProperty) selectedTime.Text = Replay.ReplayHud.Time((uint)timeline.Value) + " / " + Replay.ReplayHud.Time(ReplayController.DurationFrames); };
                timeline.PointerReleased += (_, _) => { ReplayController.Seek((uint)timeline.Value); Resumed?.Invoke(this, EventArgs.Empty); };
                var timePanel = new StackPanel { Spacing = 4 };
                timePanel.Children.Add(selectedTime);
                timePanel.Children.Add(timeline);
                replayTime = timePanel;
                Replay.ReplayCamera.EnsureTrack();
                var profile = new ComboBox { ItemsSource = new[] { "Faithful camera (default)", "Presentation camera" }, SelectedIndex = (int)Replay.ReplayCamera.Profile, Width = 280 };
                var track = new CheckBox { Content = "Play camera track by replay frame", IsChecked = Replay.ReplayCamera.PlayTrack,
                    IsEnabled = Replay.ReplayCamera.Profile == Replay.ReplayPresentationProfile.Presentation };
                profile.SelectionChanged += (_, _) =>
                {
                    if (profile.SelectedIndex < 0) return;
                    Replay.ReplayCamera.SetProfile((Replay.ReplayPresentationProfile)profile.SelectedIndex);
                    track.IsEnabled = profile.SelectedIndex == 1;
                    track.IsChecked = Replay.ReplayCamera.PlayTrack;
                };
                track.IsCheckedChanged += (_, _) => Replay.ReplayCamera.PlayTrack = track.IsChecked == true;
                camera.Children.Add(profile);
                camera.Children.Add(new TextBlock { Text = "Presentation smooths chase camera only; gameplay is unchanged.", Foreground = GuiTheme.TextDimBrush, TextWrapping = TextWrapping.Wrap, MaxWidth = 320 });
                camera.Children.Add(track);
                var cameraModes = new ComboBox { ItemsSource = new[] { "First person", "Third-person chase", "Free camera", "Orbit camera" }, SelectedIndex = (int)Replay.ReplayCamera.Mode, Width = 240 };
                cameraModes.SelectionChanged += (_, _) => { if (cameraModes.SelectedIndex >= 0) Replay.ReplayCamera.SetMode((Replay.ReplayCameraMode)cameraModes.SelectedIndex); };
                camera.Children.Add(cameraModes);
                void CameraSlider(string label, float value, double min, double max, Action<float> set)
                {
                    camera.Children.Add(new TextBlock { Text = label, Foreground = GuiTheme.TextDimBrush });
                    var slider = new Slider { Minimum = min, Maximum = max, Value = value, Width = 240 };
                    slider.PropertyChanged += (_, e) => { if (e.Property == RangeBase.ValueProperty) set((float)slider.Value); };
                    camera.Children.Add(slider);
                }
                CameraSlider("Camera distance", Replay.ReplayCamera.Distance, 1, 20, n => Replay.ReplayCamera.Distance = n);
                CameraSlider("Camera height", Replay.ReplayCamera.Height, 0.5, 8, n => Replay.ReplayCamera.Height = n);
                CameraSlider("Camera field of view", Replay.ReplayCamera.FieldOfView, 40, 120, n => Replay.ReplayCamera.FieldOfView = n);
                var players = new ComboBox { ItemsSource = GameState.Nicknames.Select((name, slot) => $"{slot + 1}: {name}").ToArray(), SelectedIndex = PlayerEntity.MainPlayerIndex, Width = 240 };
                players.SelectionChanged += (_, _) => SpectatorMode.Watch(players.SelectedIndex);
                camera.Children.Add(players);
                var director = new CheckBox { Content = "Follow kill/objective events", IsChecked = Replay.ReplayCamera.Director };
                director.IsCheckedChanged += (_, _) => Replay.ReplayCamera.Director = director.IsChecked == true;
                camera.Children.Add(director);
                Add(events, "Watch last killer", () => { Replay.ReplayCamera.WatchEvent(false); Resumed?.Invoke(this, EventArgs.Empty); });
                Add(events, "Watch last victim", () => { Replay.ReplayCamera.WatchEvent(true); Resumed?.Invoke(this, EventArgs.Empty); });
                var lookAt = new ComboBox { ItemsSource = new[] { "Keyframe: recorded orientation" }.Concat(GameState.Nicknames.Select((name, slot) => $"Look at {slot + 1}: {name}")).ToArray(), SelectedIndex = Replay.ReplayCamera.LookAtSlot + 1, Width = 280 };
                lookAt.SelectionChanged += (_, _) => Replay.ReplayCamera.LookAtSlot = lookAt.SelectedIndex - 1;
                camera.Children.Add(lookAt);
                camera.Children.Add(new TextBlock { Text = $"Camera keyframes: {Replay.ReplayCamera.KeyframeCount}/64" + (Replay.ReplayCamera.TrackError == null ? "" : " — " + Replay.ReplayCamera.TrackError), Foreground = GuiTheme.TextDimBrush, TextWrapping = TextWrapping.Wrap, MaxWidth = 320 });
                Add(camera, "Remove keyframe at current frame", () => { Replay.ReplayCamera.RemoveKeyframe(); Resumed?.Invoke(this, EventArgs.Empty); });
                Add(camera, "Save camera keyframe", () => { Replay.ReplayCamera.Bookmark(); Resumed?.Invoke(this, EventArgs.Empty); });
                Add(camera, "Preview next camera keyframe", () => { Replay.ReplayCamera.RestoreBookmark(); Resumed?.Invoke(this, EventArgs.Empty); });
                if (DemoPlayback.Events.Count > 0)
                {
                    var filters = new ComboBox { ItemsSource = new[] { "All events", "Kills", "Deaths", "Objectives", "Score" }, SelectedIndex = 0, Width = 240 };
                    filters.SelectionChanged += (_, _) => ReplayController.EventFilter = filters.SelectedIndex switch
                    { 1 => ReplayEventType.Kill, 2 => ReplayEventType.PlayerDeath, 3 => ReplayEventType.Objective, 4 => ReplayEventType.ScoreChanged, _ => null };
                    events.Children.Add(filters);
                    Add(events, "Previous event", () => { ReplayController.JumpEvent(false); Resumed?.Invoke(this, EventArgs.Empty); });
                    Add(events, "Next event", () => { ReplayController.JumpEvent(true); Resumed?.Invoke(this, EventArgs.Empty); });
                }
                Add(clip, "Mark clip start", () => { ReplayController.MarkIn(); Resumed?.Invoke(this, EventArgs.Empty); });
                Add(clip, "Mark clip end", () => { ReplayController.MarkOut(); Resumed?.Invoke(this, EventArgs.Empty); });
                Add(clip, "Save selected replay clip", () =>
                {
                    try { Chat.ChatBox.System($"Replay clip saved: {ReplayController.SaveSelection()}"); }
                    catch (Exception ex) { Chat.ChatBox.System("Could not save replay clip: " + ex.Message); }
                    Resumed?.Invoke(this, EventArgs.Empty);
                });
                Add(playback, "Go to selected time", () =>
                { ReplayController.Seek((uint)timeline.Value); Resumed?.Invoke(this, EventArgs.Empty); });
            }
            if (!(DemoPlayback.IsActive || replayPreview) && NetSession.Active)
            {
                if (DemoClip.Active) Add(menu, $"Save last {DemoClip.Seconds} seconds", () =>
                {
                    string? path = DemoClip.Save();
                    Chat.ChatBox.System(path == null ? "Not enough replay history to save a clip yet." : DemoClip.IsSaving ? "Saving replay…" : "Saved replay clip");
                    Resumed?.Invoke(this, EventArgs.Empty);
                });
                Add(menu, DemoRecorder.IsRecording ? "Stop replay recording" : "Record replay",
                    () => RecordToggleRequested?.Invoke(this, EventArgs.Empty));
            }
            Add(menu, "Settings", () => SettingsRequested?.Invoke(this, EventArgs.Empty));
            Add(menu, (DemoPlayback.IsActive || replayPreview) ? "Exit replay" : "Leave match", () => LeaveRequested?.Invoke(this, EventArgs.Empty));
            Add(menu, "Quit", () => QuitRequested?.Invoke(this, EventArgs.Empty));

            // Centred, like every other screen behind the front one. Each
            // word is centred in the column rather than the column being
            // centred with the words left-aligned inside it: a ragged edge
            // down the middle of the frame is the thing that makes a centred
            // menu look like an accident.
            foreach (Control child in menu.Children)
            {
                child.HorizontalAlignment = HorizontalAlignment.Center;
            }
            // Shrunk to fit rather than scrolled. The host is the game window
            // and the game window is whatever size the player dragged it to;
            // a scrollbar's answer to that is a menu with its top and bottom
            // cut off, which is what "the menu is always bitten" was. There is
            // nothing here to reflow -- eight words in a column stay eight
            // words in a column, just smaller.
            _scaler = new LayoutTransformControl
            {
                Child = (DemoPlayback.IsActive || replayPreview) ? null : menu,
                HorizontalAlignment = HorizontalAlignment.Center,
                VerticalAlignment = VerticalAlignment.Center
            };
            // The menu and nothing else. It carried a "paused" heading and a
            // line saying which match you were in, and both were dropped: the
            // first says what the player has just done, with the match frozen
            // behind it saying the same thing, and the second names a match
            // they are looking straight at. Neither is something anybody
            // pressed Escape to find out. Every other screen keeps its
            // heading, because on every other screen the heading is the only
            // thing that says where you are.
            //
            // No pair of marks either, and that is deliberate. Every entry
            // here is an action; there is no question being asked, so there
            // is no yes and no to answer it with -- and Resume as a tick in
            // the corner while it is also the first word of the menu is one
            // action drawn twice.

            if ((DemoPlayback.IsActive || replayPreview))
            {
                var replayPage = UiLayout.Backdrop(overGame: true);
                var frame = new Grid { RowDefinitions = new RowDefinitions("Auto,Auto,*"),
                    Margin = new Thickness(20), MaxWidth = 540 };
                _replayTabs = replayTabs;
                frame.Children.Add(replayTabs!);
                Grid.SetRow(replayTime!, 1); frame.Children.Add(replayTime!);
                var scroll = new ScrollViewer { Content = menu, Margin = new Thickness(0, 12, 0, 0),
                    VerticalScrollBarVisibility = ScrollBarVisibility.Auto };
                Grid.SetRow(scroll, 2); frame.Children.Add(scroll);
                replayPage.Children.Add(frame);
                Content = replayPage;
            }
            else Content = UiLayout.Page(overGame: true, UiLayout.WellShort, "",
                strip: null, body: _scaler, centreBody: true);
            SizeChanged += (_, e) => FitToHost(e.NewSize.Height);
        }

        private readonly LayoutTransformControl _scaler;

        /// <summary>
        /// What the column needs at full size: eight words, their spacing, and
        /// the corner it is anchored in.
        /// </summary>
        private double NeededHeight
        {
            get
            {
                int count = 0;
                foreach (Control child in _menu.Children) if (child.IsVisible) count++;
                return count * 26 + Math.Max(0, count - 1) * 14
                    + UiLayout.WellTop + UiLayout.WellBottom + 70;
            }
        }

        internal void RefreshVote()
        {
            bool visible = MapVote.Active && !MapVote.Answered && !DemoPlayback.IsActive;
            if (_voteYes.IsVisible == visible && _voteNo.IsVisible == visible) return;
            bool refocus = !visible && (_voteYes.IsFocused || _voteNo.IsFocused);
            _voteYes.IsVisible = _voteNo.IsVisible = visible;
            if (refocus) _resume.Focus();
            if (_scaler != null) FitToHost(Bounds.Height);
        }

        private void AnswerVote(bool yes)
        {
            MapVote.Cast(yes);
            RefreshVote();
            Resumed?.Invoke(this, EventArgs.Empty);
        }

        /// <summary>
        /// Fit the column to the height it has been given, down to half size.
        /// Below that there is no menu either way, and a window that short is
        /// not one anybody is playing in.
        /// </summary>
        private void FitToHost(double height)
        {
            if (DemoPlayback.IsActive || height <= 0)
            {
                return;
            }
            double scale = Math.Clamp(height / NeededHeight, 0.35, 1);
            if (_scaler.LayoutTransform is ScaleTransform current
                && Math.Abs(current.ScaleY - scale) < 0.001)
            {
                return;
            }
            _scaler.LayoutTransform = scale >= 1 ? null : new ScaleTransform(scale, scale);
        }

        /// <summary>
        /// Somebody who just asked for this is looking at a short list and
        /// expects the top entry to be the one already chosen.
        /// </summary>
        public void FocusResume()
        {
            Dispatcher.UIThread.Post(() => _resume.Focus(), DispatcherPriority.Background);
        }

        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (e.Key == Key.Escape)
            {
                // Android has no InGameMenu ancestor to handle semantic Back.
                // Resume also performs the host's view/input lifecycle cleanup.
                Resumed?.Invoke(this, EventArgs.Empty);
                e.Handled = true;
                return;
            }
            base.OnKeyDown(e);
        }

        private static string WindowLabel()
        {
            return WindowMode.IsFullscreen ? "Windowed" : "Fullscreen";
        }

        private static UiWord Add(StackPanel menu, string text, Action action)
        {
            var word = new UiWord(text);
            word.Click += (_, _) => action();
            menu.Children.Add(word);
            return word;
        }
    }
}
