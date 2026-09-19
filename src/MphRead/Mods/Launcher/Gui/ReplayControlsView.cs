#if MPHREAD_AVALONIA
using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using Avalonia.VisualTree;
using MphRead.Mods.Network;
using MphRead.Mods.Replay;

namespace MphRead.Mods.Launcher.Gui
{
    /// <summary>
    /// Replay transport, navigation and clip-marking controls in the same deck
    /// language as the rest of the current launcher.
    ///
    /// Actions that need the recorded simulation to advance close both this
    /// sheet and the pause menu through <see cref="ResumeRequested"/>. Actions
    /// that only change metadata or presentation stay here so several can be
    /// adjusted together.
    /// </summary>
    internal sealed class ReplayControlsView : UserControl
    {
        public event EventHandler? Closed;
        public event EventHandler? ResumeRequested;

        private readonly TextBlock _status;
        private readonly DeckButton _playPause;
        private readonly DeckButton _first;
        private string _message = "";

        public ReplayControlsView()
        {
            Background = Brushes.Transparent;
            Focusable = true;

            var body = new StackPanel { Spacing = 9 };
            _status = new TextBlock
            {
                FontFamily = GuiTheme.Display,
                FontSize = 14,
                Foreground = GuiTheme.TextBrush,
                TextWrapping = TextWrapping.Wrap,
                TextAlignment = TextAlignment.Center,
                HorizontalAlignment = HorizontalAlignment.Stretch,
                Margin = new Thickness(0, 0, 0, 4)
            };
            body.Children.Add(_status);

            var grid = new Grid
            {
                ColumnDefinitions = new ColumnDefinitions("*,*")
            };
            body.Children.Add(grid);
            int index = 0;

            DeckButton AddAction(string text, Action action, bool resume = false,
                Deck.Face? face = null)
            {
                if (index % 2 == 0)
                {
                    grid.RowDefinitions.Add(new RowDefinition(GridLength.Auto));
                }
                var button = new DeckButton(text, face ?? Deck.Face.Slate,
                    sizeEms: 1.05, padXEms: 0.8, padYEms: 0.5, lip: 4)
                {
                    HorizontalAlignment = HorizontalAlignment.Stretch,
                    Margin = new Thickness(index % 2 == 0 ? 0 : 4,
                        index < 2 ? 0 : 4,
                        index % 2 == 0 ? 4 : 0, 0)
                };
                button.Click += (_, _) =>
                {
                    _message = "";
                    action();
                    Refresh();
                    if (resume)
                    {
                        ResumeRequested?.Invoke(this, EventArgs.Empty);
                    }
                };
                Grid.SetRow(button, index / 2);
                Grid.SetColumn(button, index % 2);
                grid.Children.Add(button);
                index++;
                return button;
            }

            _playPause = AddAction("PAUSE", ReplayController.TogglePause, resume: true,
                face: Deck.Face.Moss);
            _first = _playPause;
            AddAction("STEP FRAME", ReplayController.StepForward, resume: true);
            AddAction("-5 SECONDS", () => ReplayController.Seek(
                ReplayController.CurrentFrame > 300
                    ? ReplayController.CurrentFrame - 300 : 0, resume: false), resume: true);
            AddAction("+5 SECONDS", () => ReplayController.Seek(
                (uint)Math.Min((ulong)ReplayController.CurrentFrame + 300,
                    ReplayController.DurationFrames), resume: false), resume: true);
            AddAction("SLOWER", () => ReplayController.ChangeRate(-1));
            AddAction("FASTER", () => ReplayController.ChangeRate(1));
            AddAction("PREV EVENT", () => ReplayController.JumpEvent(false), resume: true);
            AddAction("NEXT EVENT", () => ReplayController.JumpEvent(true), resume: true);
            AddAction("PREV PLAYER", () =>
            {
                SpectatorMode.CyclePrevious();
                ReplayController.NoteInput();
            }, resume: true);
            AddAction("NEXT PLAYER", () =>
            {
                SpectatorMode.CycleNext();
                ReplayController.NoteInput();
            }, resume: true);
            AddAction("CAMERA MODE", CycleCamera, resume: true);
            AddAction("RESTART", ReplayController.Restart, resume: true);
            AddAction("MARK IN", ReplayController.MarkIn, face: Deck.Face.Brass);
            AddAction("MARK OUT", ReplayController.MarkOut, face: Deck.Face.Brass);
            AddAction("SAVE SELECTION", SaveSelection, face: Deck.Face.Moss);

            var shortcuts = new TextBlock
            {
                Text = "Keyboard: Space play/pause · ,/. step · [/] speed · ←/→ seek · "
                    + "1-8 player · F/C/O camera",
                FontFamily = GuiTheme.Display,
                FontSize = 11,
                Foreground = GuiTheme.TextDimBrush,
                TextWrapping = TextWrapping.Wrap,
                TextAlignment = TextAlignment.Center
            };
            body.Children.Add(shortcuts);

            var scroll = new ScrollViewer
            {
                Content = body,
                HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled,
                VerticalScrollBarVisibility = ScrollBarVisibility.Auto
            };
            var back = new UiMark(UiMark.Shape.Cancel, "back");
            back.Click += (_, _) => Closed?.Invoke(this, EventArgs.Empty);
            Content = UiLayout.Page(overGame: true, UiLayout.WellSettings,
                "replay controls", strip: null, body: scroll, no: back);
            Refresh();
        }

        private void CycleCamera()
        {
            ReplayCameraMode next = ReplayCamera.Mode switch
            {
                ReplayCameraMode.FirstPerson => ReplayCameraMode.Chase,
                ReplayCameraMode.Chase => ReplayCameraMode.Orbit,
                ReplayCameraMode.Orbit => ReplayCameraMode.Free,
                _ => ReplayCameraMode.FirstPerson
            };
            ReplayCamera.SetMode(next);
        }

        private void SaveSelection()
        {
            ReplayOpenResult result = ReplayController.SaveSelection();
            _message = result == ReplayOpenResult.Success
                ? "Selected clip saved to the replay library."
                : result == ReplayOpenResult.Empty
                    ? "Set both MARK IN and MARK OUT before saving."
                    : $"Could not save selection: {result}.";
        }

        private void Refresh()
        {
            _playPause.Text = ReplayController.IsPaused ? "PLAY" : "PAUSE";
            string marks = $"IN {Mark(ReplayController.ClipIn)}  ·  OUT {Mark(ReplayController.ClipOut)}";
            string mode = ReplayCamera.Mode.ToString();
            _status.Text = $"{ReplayController.State}  ·  {Time(ReplayController.CurrentFrame)} / "
                + $"{Time(ReplayController.DurationFrames)}  ·  {ReplayController.PlaybackRate:0.##}x\n"
                + $"{mode} camera  ·  {marks}"
                + (_message.Length == 0 ? "" : "\n" + _message);
        }

        private static string Mark(uint? frame) => frame.HasValue ? Time(frame.Value) : "--:--";
        private static string Time(uint frame) => $"{frame / 3600:00}:{frame / 60 % 60:00}";

        protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e)
        {
            base.OnAttachedToVisualTree(e);
            Dispatcher.UIThread.Post(() => _first.Focus(), DispatcherPriority.Background);
        }

        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (e.Key == Key.Escape)
            {
                Closed?.Invoke(this, EventArgs.Empty);
                e.Handled = true;
                return;
            }
            base.OnKeyDown(e);
        }
    }
}
#endif
