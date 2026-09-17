using System;
using System.Linq;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;
using Avalonia.Threading;
using MphRead.Mods.Input;

namespace MphRead.Mods.Launcher.Gui
{
    /// <summary>
    /// One rebindable pad button: what it does on the left, which button it is
    /// on to the right, click and press to change it.
    ///
    /// <see cref="KeyRow"/>'s shape, and none of its problem: a key arrives as
    /// a toolkit event that has to be translated into GLFW's enumeration,
    /// whereas a pad is already reduced to <see cref="GamepadState"/> by the
    /// time anything here could see it. So this does not listen for an event
    /// at all -- it watches the state, which is the same state the game reads
    /// and therefore cannot disagree with it about what was pressed.
    ///
    /// Watching starts only while a row is listening. The pad is polled on the
    /// desktop and evented on Android (see <c>MainActivity.DispatchKeyEvent</c>,
    /// which is what puts a pad press into that state while a match is not
    /// running), and neither should be happening for the sake of a settings
    /// screen nobody is currently rebinding on.
    /// </summary>
    internal sealed class PadRow : Control
    {
        private readonly PadAction _action;
        private readonly double _labelWidth;
        private bool _listening;
        private int _slot, _choice, _pickIndex;
        private bool _picking;
        private static readonly GamepadButtons[] PickButtons = Enum.GetValues<GamepadButtons>();
        private GamepadButtons _pending;
        private long _deviceRevision;
        private string? _conflict;
        private static readonly string[] Resolutions = { "Swap", "Replace", "Keep Both", "Cancel" };
        private bool _hot;
        private DispatcherTimer? _watch;

        /// <summary>
        /// What the pad already had held when listening began, so a button
        /// that is being held for some other reason -- or one still down from
        /// the press that opened this row -- is not read as the answer. Only a
        /// button that goes down from here counts.
        /// </summary>
        private GamepadButtons _baseline;

        public event EventHandler? Rebound;

        public PadRow(PadAction action, double labelWidth = 160)
        {
            _action = action;
            _labelWidth = labelWidth;
            Height = 32;
            Focusable = true;
            Cursor = new Cursor(StandardCursorType.Hand);
        }

        private Rect Box => new(_labelWidth, 2,
            Math.Max(60, Bounds.Width - _labelWidth - 4), 28);

        protected override void OnPointerPressed(PointerPressedEventArgs e)
        {
            Focus();
            if (_conflict != null)
            {
                var point = e.GetPosition(this);
                if (point.Y > 60) { _choice = Math.Clamp((int)(point.X / Math.Max(1, Bounds.Width / 4)), 0, 3); Resolve(); }
                e.Handled = true; return;
            }
            if (!_listening && Box.Contains(e.GetPosition(this)))
            {
                _slot = e.GetPosition(this).X < Box.Center.X ? 0 : 1;
                Listen();
            }
            e.Handled = true;
            base.OnPointerPressed(e);
        }

        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (_conflict != null)
            {
                if (e.Key == Key.Left) _choice = Math.Max(0, _choice - 1);
                if (e.Key == Key.Right) _choice = Math.Min(3, _choice + 1);
                if (e.Key == Key.Enter) Resolve();
                if (e.Key == Key.Escape) Done();
                e.Handled = true; InvalidateVisual(); return;
            }
            if (!_listening)
            {
                if (e.Key == Key.Left || e.Key == Key.Right)
                {
                    _slot = e.Key == Key.Left ? 0 : 1; InvalidateVisual(); e.Handled = true; return;
                }
                if (e.Key == Key.Enter || e.Key == Key.Space)
                {
                    Listen();
                    e.Handled = true;
                }
                base.OnKeyDown(e);
                return;
            }
            e.Handled = true;
            if (e.Key == Key.Escape)
            {
                Done();
                return;
            }
            if (e.Key == Key.Back || e.Key == Key.Delete)
            {
                // Unbinding is worth having: a pad with a broken bumper is
                // better with nothing on it than with something that fires by
                // itself, and GamepadInput reads None as "never held".
                PadBindings.SetSlot(_action, _slot, GamepadButtons.None);
                Done();
            }
        }

        private void Listen()
        {
            _listening = true;
            Height = 72;
            GamepadContexts.Capturing = true;
            _deviceRevision = GamepadManager.Snapshot.Revision;
            GamepadDesktop.PollForMenu();
            _baseline = GamepadInput.State.Buttons;
            _watch?.Stop();
            _watch = new DispatcherTimer(TimeSpan.FromMilliseconds(30),
                DispatcherPriority.Input, (_, _) => Check());
            _watch.Start();
            InvalidateVisual();
        }

        internal void Check()
        {
            if (!_listening)
            {
                return;
            }
            // Android fills the state from events and needs nothing here; the
            // desktop's pad is polled, and with no game window running there
            // is nothing else pumping GLFW. Both cases are inside this call.
            GamepadDesktop.PollForMenu();
            var snapshot = GamepadManager.Snapshot;
            if (!snapshot.State.Connected || snapshot.Revision != _deviceRevision) { Done(); return; }
            GamepadButtons pressed = snapshot.State.Buttons & ~_baseline;
            _baseline = snapshot.State.Buttons;
            if (pressed == 0) return;
            if (_conflict != null)
            {
                if ((pressed & GamepadButtons.DpadLeft) != 0) _choice = Math.Max(0, _choice - 1);
                if ((pressed & GamepadButtons.DpadRight) != 0) _choice = Math.Min(3, _choice + 1);
                if ((pressed & GamepadButtons.A) != 0) Resolve();
                else if ((pressed & GamepadButtons.B) != 0) Done();
                InvalidateVisual(); return;
            }
            if ((pressed & GamepadButtons.B) != 0) { Done(); return; }
            if (_picking)
            {
                if ((pressed & GamepadButtons.DpadLeft) != 0) _pickIndex = (_pickIndex + PickButtons.Length - 1) % PickButtons.Length;
                if ((pressed & GamepadButtons.DpadRight) != 0) _pickIndex = (_pickIndex + 1) % PickButtons.Length;
                if ((pressed & GamepadButtons.A) != 0) Choose(PickButtons[_pickIndex]);
                InvalidateVisual(); return;
            }
            // The picker makes even the capture commands themselves bindable.
            if ((pressed & GamepadButtons.Start) != 0) { _picking = true; _pickIndex = 1; InvalidateVisual(); return; }
            if ((pressed & GamepadButtons.Back) != 0) { PadBindings.SetSlot(_action, _slot, 0); Done(); return; }
            foreach (GamepadButtons button in PickButtons)
            {
                if (button == 0 || (pressed & button) == 0) continue;
                Choose(button); return;
            }
        }
        private void Choose(GamepadButtons button)
        {
            _picking = false;
            var conflicts = PadBindings.Conflicts(_action, button);
            if (conflicts.Count == 0) { PadBindings.SetSlot(_action, _slot, button); Done(); }
            else
            {
                _pending = button; _choice = 3;
                _conflict = PadBindings.ButtonName(button) + " is assigned to "
                    + string.Join(" / ", conflicts.Select(PadBindings.Name));
                Height = 108; this.BringIntoView(); InvalidateVisual();
            }
        }

        private void Resolve()
        {
            PadBindings.Assign(_action, _slot, _pending, Resolutions[_choice]);
            Done();
        }

        private void Done()
        {
            _listening = false;
            GamepadContexts.Capturing = false;
            _conflict = null; _picking = false; Height = 32;
            _watch?.Stop();
            _watch = null;
            InvalidateVisual();
            Rebound?.Invoke(this, EventArgs.Empty);
        }

        protected override void OnPointerEntered(PointerEventArgs e)
        {
            _hot = true;
            InvalidateVisual();
            base.OnPointerEntered(e);
        }

        protected override void OnPointerExited(PointerEventArgs e)
        {
            _hot = false;
            InvalidateVisual();
            base.OnPointerExited(e);
        }

        protected override void OnLostFocus(Avalonia.Interactivity.RoutedEventArgs e)
        {
            if (_listening)
            {
                Done();
            }
            base.OnLostFocus(e);
        }

        protected override void OnGotFocus(GotFocusEventArgs e)
        {
            InvalidateVisual();
            base.OnGotFocus(e);
        }

        protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e)
        {
            // The timer outlives the view otherwise, and it polls GLFW.
            _watch?.Stop();
            _watch = null;
            _listening = false;
            GamepadContexts.Capturing = false;
            _conflict = null; _picking = false; Height = 32;
            base.OnDetachedFromVisualTree(e);
        }

        public override void Render(DrawingContext context)
        {
            // See UiWord.Render: hit testing follows the drawing.
            context.FillRectangle(Brushes.Transparent,
                new Rect(0, 0, Bounds.Width, Bounds.Height));
            FormattedText label = TrackedText.Make(PadBindings.Name(_action), 12,
                bold: true, GuiTheme.TextBrush);
            context.DrawText(label, new Point(4, (32 - label.Height) / 2));

            Rect box = Box;
            context.DrawRectangle(GuiTheme.PanelLightBrush,
                new Pen(new SolidColorBrush(_listening ? GuiTheme.Warm
                    : IsFocused || _hot ? GuiTheme.Accent : GuiTheme.Edge), 1),
                new RoundedRect(box, 4));

            string text = _picking ? "< " + PadBindings.Describe(PickButtons[_pickIndex]) + " >  Accept / Back" : _listening
                ? "Press a button"
                : (_slot == 0 && IsFocused ? "> " : "") + "Primary: " + PadBindings.Describe(PadBindings.Slot(_action, 0))
                    + "    " + (_slot == 1 && IsFocused ? "> " : "") + "Secondary: " + PadBindings.Describe(PadBindings.Slot(_action, 1));
            if (_listening && _conflict == null)
            {
                var hint = TrackedText.Make($"{PadBindings.ButtonName(GamepadButtons.B)} cancel   "
                    + $"{PadBindings.ButtonName(GamepadButtons.Back)} clear   {PadBindings.ButtonName(GamepadButtons.Start)} choose button", 11, true, GuiTheme.TextDimBrush);
                hint.MaxTextWidth = Math.Max(20, Bounds.Width - 8); hint.MaxTextHeight = 28;
                context.DrawText(hint, new Point(4, 40));
            }
            if (_conflict != null)
            {
                var note = TrackedText.Make(_conflict, 11, true, GuiTheme.TextBrush);
                note.MaxTextWidth = Math.Max(20, Bounds.Width - 8);
                note.MaxTextHeight = 28; note.Trimming = TextTrimming.CharacterEllipsis;
                context.DrawText(note, new Point(4, 36));
                for (int i = 0; i < 4; i++)
                {
                    var option = TrackedText.Make((_choice == i ? "> " : "") + Resolutions[i], 12, true,
                        _choice == i ? GuiTheme.AccentBrush : GuiTheme.TextBrush);
                    context.DrawText(option, new Point(i * Bounds.Width / 4 + 4, 74));
                }
            }
            FormattedText value = TrackedText.Make(text, 12, bold: true,
                new SolidColorBrush(_listening ? GuiTheme.Warm : GuiTheme.Text));
            value.MaxTextWidth = Math.Max(20, box.Width - 12);
            value.MaxTextHeight = box.Height;
            value.Trimming = TextTrimming.CharacterEllipsis;
            context.DrawText(value, new Point(box.X + (box.Width - value.Width) / 2,
                box.Y + (box.Height - value.Height) / 2));
        }
    }
}
