#if MPHREAD_AVALONIA
using System;
using System.Globalization;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Media;

namespace MphRead.Mods.Launcher.Gui
{
    /// <summary>
    /// A label and the thing it names, in one small slab.
    ///
    /// The command bar's ends: what profile this is on the left, and on the
    /// right the one thing that is not a menu entry. They are not buttons --
    /// there is no lip and nothing to press on the profile one -- which is the
    /// point: the row reads as three faces between two pieces of information
    /// rather than five things to click.
    /// </summary>
    internal sealed class DeckChip : Control
    {
        private readonly string _key;
        private readonly string _value;

        public DeckChip(string key, string value)
        {
            _key = key.ToUpperInvariant();
            _value = value.ToUpperInvariant();
            IsHitTestVisible = false;
            Avalonia.Media.RenderOptions.SetEdgeMode(this, EdgeMode.Aliased);
        }

        private static FormattedText Key(string s)
        {
            return new FormattedText(s, CultureInfo.InvariantCulture,
                FlowDirection.LeftToRight, GuiTheme.Face(false), 10,
                GuiTheme.TextDimBrush);
        }

        private static FormattedText Value(string s)
        {
            return new FormattedText(s, CultureInfo.InvariantCulture,
                FlowDirection.LeftToRight,
                new Typeface(GuiTheme.PixelSemi, FontStyle.Normal, FontWeight.Normal),
                GuiTheme.PixelSize(17), GuiTheme.TextBrush);
        }

        protected override Size MeasureOverride(Size availableSize)
        {
            FormattedText k = Key(_key), v = Value(_value);
            return new Size(Math.Max(k.Width, v.Width) + 22,
                k.Height + v.Height + 18);
        }

        /// <summary>
        /// The keycap on a button's face: the reference's <c>.key</c>.
        ///
        /// A well sunk into the face with the key in it, at half the label's
        /// size, in the <i>body</i> face rather than the display one -- a
        /// keycap is read, not scanned, and "ESC" set in a pixel font at seven
        /// points is three smudges. Sunk rather than tinted: on brass a chip at
        /// a third opacity reads as a lighter brass rectangle, not as a hole.
        /// </summary>
        public static void DrawKey(DrawingContext context, string key, double labelSize,
            double x, double height)
        {
            if (key.Length == 0)
            {
                return;
            }
            double size = labelSize * 0.5;
            FormattedText text = DeckText.Run(key, Deck.Body(bold: true), size,
                GuiTheme.AccentBrush);
            double w = Math.Max(size * 1.5, text.Width + size * 0.7);
            double h = Math.Max(size * 1.5, text.Height);
            var box = new Rect(x, Math.Round((height - h) / 2), w, h);
            context.DrawRectangle(new SolidColorBrush(Color.FromArgb(107, 0, 0, 0)), null,
                new RoundedRect(box, size * 0.3));
            context.DrawText(text, new Point(
                Math.Round(box.X + (w - text.Width) / 2),
                Math.Round(box.Y + (h - text.Height) / 2)));
        }

        public override void Render(DrawingContext context)
        {
            double w = Bounds.Width, h = Bounds.Height - 4;
            FormattedText k = Key(_key), v = Value(_value);

            context.DrawRectangle(new SolidColorBrush(Color.FromArgb(210, 18, 21, 28)), null,
                new RoundedRect(new Rect(0, 0, w, h), 7));
            context.DrawRectangle(new SolidColorBrush(Color.FromArgb(128, 0, 0, 0)), null,
                new RoundedRect(new Rect(0, h, w, 4), 2));

            context.DrawText(k, new Point(Math.Round((w - k.Width) / 2), 5));
            // The value sits in its own well, the way the profile chip's does
            // on the screen this is from.
            double vy = Math.Round(k.Height + 7);
            context.DrawRectangle(new SolidColorBrush(GuiTheme.PanelLight), null,
                new RoundedRect(new Rect(5, vy - 2, w - 10, v.Height + 4), 4));
            context.DrawText(v, new Point(Math.Round((w - v.Width) / 2), vy));
        }
    }

    /// <summary>
    /// The one thing in the bar that is not a menu entry: a pixel heart that
    /// opens the support page.
    ///
    /// Drawn from rows rather than a path, each row centred on the same axis
    /// by construction -- a heart traced by hand comes out with one lobe wider
    /// than the other and nobody sees it until it is on a screenshot.
    /// </summary>
    internal sealed class DeckHeart : Control
    {
        public event EventHandler? Click;

        private readonly Tap _tap = new();

        /// <summary>y, x, width -- on a 12-wide grid, mirrored about x = 6.</summary>
        private static readonly (int Y, int X, int W)[] _rows =
        {
            (0, 2, 3), (0, 7, 3), (1, 1, 10), (2, 1, 10), (3, 1, 10),
            (4, 2, 8), (5, 3, 6), (6, 4, 4), (7, 5, 2)
        };

        public DeckHeart()
        {
            Focusable = true;
            Cursor = new Cursor(StandardCursorType.Hand);
            Avalonia.Media.RenderOptions.SetEdgeMode(this, EdgeMode.Aliased);
        }

        protected override Size MeasureOverride(Size availableSize) => new Size(52, 44);

        protected override void OnPointerEntered(PointerEventArgs e)
        {
            InvalidateVisual();
            base.OnPointerEntered(e);
        }

        protected override void OnPointerExited(PointerEventArgs e)
        {
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

        protected override void OnPointerReleased(PointerReleasedEventArgs e)
        {
            if (_tap.Release(e, this))
            {
                e.Handled = true;
                Click?.Invoke(this, EventArgs.Empty);
            }
            InvalidateVisual();
            base.OnPointerReleased(e);
        }

        public override void Render(DrawingContext context)
        {
            double w = Bounds.Width, h = Bounds.Height - 5;
            bool hot = IsPointerOver || IsFocused;

            context.DrawRectangle(new SolidColorBrush(
                hot ? Color.FromRgb(0x84, 0x42, 0x42) : Color.FromRgb(0x6b, 0x36, 0x36)), null,
                new RoundedRect(new Rect(0, 0, w, h), 8));
            context.DrawRectangle(new SolidColorBrush(Color.FromRgb(0x38, 0x1b, 0x1b)), null,
                new RoundedRect(new Rect(0, h, w, 5), 3));

            // 12 x 8 cells, centred, at whole-pixel scale so the heart stays a
            // pixel heart rather than a smoothed one.
            double cell = Math.Max(1, Math.Floor(Math.Min(w / 18.0, h / 12.0)));
            double ox = Math.Round((w - 12 * cell) / 2);
            double oy = Math.Round((h - 8 * cell) / 2);
            var ink = new SolidColorBrush(hot
                ? Color.FromRgb(0xff, 0xd0, 0xd0) : Color.FromRgb(0xe8, 0xa0, 0xa0));
            foreach ((int y, int x, int rw) in _rows)
            {
                context.FillRectangle(ink,
                    new Rect(ox + x * cell, oy + y * cell, rw * cell, cell));
            }
        }
    }
}
#endif
