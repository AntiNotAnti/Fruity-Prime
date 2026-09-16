#if MPHREAD_AVALONIA
using System;
using System.Globalization;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;

namespace MphRead.Mods.Launcher.Gui
{
    /// <summary>
    /// The name, set rather than drawn.
    ///
    /// <see cref="UiLayout.Wordmark"/> hands back the shipped PNG, which is
    /// the cherry and "Fruity PRIME" beside it. That mark is the program's,
    /// and it stays the program's -- the window icon and the release art are
    /// still it. What this is for is the front screen under the deck theme,
    /// where a smooth-edged bitmap sitting over a row of pixel-type buttons is
    /// the one thing on the screen from a different drawing.
    ///
    /// <para>
    /// Two lines, because the name is two words and stacking them is what
    /// gives a wordmark in a pixel face something to be: a wide single line
    /// of it is a caption. The outline is four hard offsets rather than a
    /// blur, for the same reason the buttons' edge is not a gradient -- a
    /// pixel face with a soft glow behind it is a pixel face that has been
    /// apologised for.
    /// </para>
    /// </summary>
    internal sealed class DeckWordmark : Control
    {
        private readonly double _size;

        /// <summary>How far the hard outline is thrown, in whole pixels.</summary>
        private const double Outline = 3;

        /// <summary>The shadow under it, which is the outline again, further down.</summary>
        private const double Drop = 9;

        public DeckWordmark(double size = 64)
        {
            _size = GuiTheme.PixelSize(size);
            IsHitTestVisible = false;
            Avalonia.Media.RenderOptions.SetEdgeMode(this, EdgeMode.Aliased);
        }

        private FormattedText Line(string text, IBrush brush)
        {
            return new FormattedText(text, CultureInfo.InvariantCulture,
                FlowDirection.LeftToRight,
                new Typeface(GuiTheme.PixelBold, FontStyle.Normal, FontWeight.Normal),
                _size, brush);
        }

        protected override Size MeasureOverride(Size availableSize)
        {
            FormattedText top = Line("FRUITY", GuiTheme.TextBrush);
            FormattedText bottom = Line("PRIME", GuiTheme.AccentBrush);
            return new Size(
                Math.Max(top.Width, bottom.Width) + Outline * 2,
                top.Height + bottom.Height * 0.88 + Outline * 2 + Drop);
        }

        public override void Render(DrawingContext context)
        {
            FormattedText top = Line("FRUITY", new SolidColorBrush(Color.FromRgb(0xf2, 0xed, 0xe2)));
            FormattedText bottom = Line("PRIME", GuiTheme.AccentBrush);
            double w = Bounds.Width;

            // Whole pixels, both lines, or the outline lands half on one row.
            double topX = Math.Round((w - top.Width) / 2);
            double bottomX = Math.Round((w - bottom.Width) / 2);
            double topY = Math.Round(Outline);
            double bottomY = Math.Round(topY + top.Height * 0.88);

            Draw(context, "FRUITY", top, topX, topY, _size);
            Draw(context, "PRIME", bottom, bottomX, bottomY, _size);
        }

        private static void Draw(DrawingContext context, string word,
            FormattedText text, double x, double y, double size)
        {
            var ink = new SolidColorBrush(GuiTheme.Ink);
            var shadow = new SolidColorBrush(Color.FromArgb(140, 0, 0, 0));

            // The drop first, then the outline, then the face over both.
            context.DrawText(Recolour(word, size, shadow), new Point(x, y + Drop));
            foreach ((double dx, double dy) in new[]
            {
                (-Outline, 0d), (Outline, 0d), (0d, -Outline), (0d, Outline),
                (-Outline, Outline), (Outline, Outline)
            })
            {
                context.DrawText(Recolour(word, size, ink), new Point(x + dx, y + dy));
            }
            context.DrawText(text, new Point(x, y));
        }

        /// <summary>
        /// A FormattedText carries its brush and hands back neither the string
        /// nor the size it was built from, so each outline pass builds its own
        /// rather than mutating one the next pass would inherit.
        /// </summary>
        private static FormattedText Recolour(string word, double size, IBrush brush)
        {
            return new FormattedText(word, CultureInfo.InvariantCulture,
                FlowDirection.LeftToRight,
                new Typeface(GuiTheme.PixelBold, FontStyle.Normal, FontWeight.Normal),
                size, brush);
        }
    }
}
#endif
