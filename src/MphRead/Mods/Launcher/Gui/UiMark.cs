using System;
using System.Globalization;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;

namespace MphRead.Mods.Launcher.Gui
{
    /// <summary>
    /// The two marks in the bottom corners: a cross on the left that means
    /// "no", a tick on the right that means "yes".
    ///
    /// One pair on every screen that asks anything, always in the same two
    /// places, so that leaving and committing stop being things to look for.
    /// The screens used to answer this five different ways -- "Back", "Launch",
    /// "Connect", "Start", the window's own close button -- which is five words
    /// for two actions.
    ///
    /// Drawn with two strokes rather than set as text: the tick and the cross
    /// are not in every font that might be substituted, and a missing glyph in
    /// the one control that means "go" is worse than a few lines of geometry.
    /// </summary>
    internal sealed class UiMark : Control
    {
        public enum Shape
        {
            Cancel,
            Accept,
            /// <summary>A plus: make a new one of something. Neither yes nor no.</summary>
            Add,
            /// <summary>An arrow into a tray: fetch something that is missing.</summary>
            Fetch
        }

        public static readonly StyledProperty<string> LabelProperty =
            AvaloniaProperty.Register<UiMark, string>(nameof(Label), "");

        public string Label
        {
            get => GetValue(LabelProperty);
            set => SetValue(LabelProperty, value);
        }

        public event EventHandler? Click;

        private readonly Shape _shape;
        private readonly Tap _tap = new();

        private const double Glyph = 22;
        private const double Gap = 12;
        private const double LabelSize = 15;

        static UiMark()
        {
            AffectsRender<UiMark>(LabelProperty, IsEnabledProperty);
            AffectsMeasure<UiMark>(LabelProperty);
        }

        public UiMark(Shape shape, string label)
        {
            _shape = shape;
            Label = label;
            Height = 34;
            Focusable = true;
            Cursor = new Cursor(StandardCursorType.Hand);
        }

        private FormattedText Caption(IBrush brush)
        {
            return new FormattedText(Label.ToUpperInvariant(), CultureInfo.InvariantCulture,
                FlowDirection.LeftToRight, GuiTheme.Face(bold: true), LabelSize, brush);
        }

        protected override Size MeasureOverride(Size availableSize)
        {
            double width = Glyph;
            if (Label.Length > 0)
            {
                width += Gap + Caption(GuiTheme.TextBrush).Width;
            }
            return new Size(Math.Min(width, availableSize.Width), Height);
        }

        protected override void OnPointerEntered(PointerEventArgs e)
        {
            InvalidateVisual();
            base.OnPointerEntered(e);
        }

        protected override void OnPointerExited(PointerEventArgs e)
        {
            _tap.Cancel();
            InvalidateVisual();
            base.OnPointerExited(e);
        }

        protected override void OnPointerPressed(PointerPressedEventArgs e)
        {
            _tap.Press(e, this);
            Focus();
            e.Pointer.Capture(this);
            e.Handled = true;
            InvalidateVisual();
            base.OnPointerPressed(e);
        }

        protected override void OnPointerMoved(PointerEventArgs e)
        {
            // A finger that has set off across the screen is scrolling
            // whatever this sits in, and is no longer pressing this. See Tap.
            _tap.Moved(e, this);
            base.OnPointerMoved(e);
        }

        protected override void OnPointerReleased(PointerReleasedEventArgs e)
        {
            // Tap decides: the press has to have landed here, stayed within a
            // finger's slop, and let go inside. Where the release landed, not
            // IsPointerOver -- a finger hovers nothing, so the pointer has
            // already left by the time it lets go.
            bool tapped = _tap.Release(e, this);
            InvalidateVisual();
            if (ReferenceEquals(e.Pointer.Captured, this))
            {
                e.Pointer.Capture(null);
            }
            base.OnPointerReleased(e);
            if (tapped && IsEnabled)
            {
                Click?.Invoke(this, EventArgs.Empty);
            }
        }

        protected override void OnPointerCaptureLost(PointerCaptureLostEventArgs e)
        {
            // The scroll gesture above has taken the pointer.
            _tap.Cancel();
            InvalidateVisual();
            base.OnPointerCaptureLost(e);
        }

        protected override void OnGotFocus(GotFocusEventArgs e)
        {
            InvalidateVisual();
            base.OnGotFocus(e);
        }

        protected override void OnLostFocus(RoutedEventArgs e)
        {
            InvalidateVisual();
            base.OnLostFocus(e);
        }

        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (e.Key == Key.Enter || e.Key == Key.Space)
            {
                e.Handled = true;
                if (IsEnabled)
                {
                    Click?.Invoke(this, EventArgs.Empty);
                }
                return;
            }
            base.OnKeyDown(e);
        }

        public override void Render(DrawingContext context)
        {
            context.FillRectangle(Brushes.Transparent,
                new Rect(0, 0, Bounds.Width, Bounds.Height));
            bool lit = (IsPointerOver || IsFocused) && IsEnabled;
            Color colour = !IsEnabled ? GuiTheme.Edge
                : lit ? GuiTheme.Accent
                : _shape == Shape.Accept ? GuiTheme.Text
                : _shape == Shape.Fetch ? GuiTheme.Warm : GuiTheme.TextDim;
            var pen = new Pen(new SolidColorBrush(colour), 2.4)
            {
                LineCap = PenLineCap.Round
            };
            double cy = Bounds.Height / 2;
            double half = Glyph / 2;
            if (_shape == Shape.Accept)
            {
                // A tick: down to the low point, then up and out past it.
                context.DrawLine(pen, new Point(half - 7, cy + 1), new Point(half - 2, cy + 6));
                context.DrawLine(pen, new Point(half - 2, cy + 6), new Point(half + 8, cy - 7));
            }
            else if (_shape == Shape.Add)
            {
                context.DrawLine(pen, new Point(half - 7, cy), new Point(half + 7, cy));
                context.DrawLine(pen, new Point(half, cy - 7), new Point(half, cy + 7));
            }
            else if (_shape == Shape.Fetch)
            {
                // Down the shaft, out to the two barbs, and a floor under it:
                // the arrow says which way, the floor says it lands here.
                context.DrawLine(pen, new Point(half, cy - 8), new Point(half, cy + 2));
                context.DrawLine(pen, new Point(half - 5, cy - 3), new Point(half, cy + 2));
                context.DrawLine(pen, new Point(half + 5, cy - 3), new Point(half, cy + 2));
                context.DrawLine(pen, new Point(half - 7, cy + 7), new Point(half + 7, cy + 7));
            }
            else
            {
                context.DrawLine(pen, new Point(half - 7, cy - 7), new Point(half + 7, cy + 7));
                context.DrawLine(pen, new Point(half + 7, cy - 7), new Point(half - 7, cy + 7));
            }
            if (Label.Length == 0)
            {
                return;
            }
            FormattedText caption = Caption(new SolidColorBrush(colour));
            context.DrawText(caption,
                new Point(Glyph + Gap, (Bounds.Height - caption.Height) / 2));
        }
    }
}
