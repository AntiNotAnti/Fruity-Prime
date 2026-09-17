using System.Collections.Generic;
using Avalonia;
using Avalonia.Media;
using MphRead.Mods.Render;

namespace MphRead.Mods.Launcher.Gui
{
    /// <summary>
    /// The crosshair as it will actually be drawn, in the settings row that
    /// picks it.
    ///
    /// One point here is one pixel in the game, so the size stops read as what
    /// they are rather than as three words -- which is the whole reason the
    /// preview is worth the room it takes. The shapes come from
    /// <see cref="Crosshair"/>, the same table the renderer draws from, so the
    /// picture cannot drift away from the game.
    /// </summary>
    internal static class CrosshairPreview
    {
        public static void Draw(DrawingContext context, Rect area, CrosshairStyle style,
            CrosshairSize size, int? color = null, bool? outline = null)
        {
            context.DrawRectangle(GuiTheme.PanelBrush, new Pen(GuiTheme.EdgeBrush, 1),
                new RoundedRect(area, 4));
            double cx = area.X + area.Width / 2;
            double cy = area.Y + area.Height / 2;
            var rgb = Crosshair.Color(OpenTK.Mathematics.Vector3.One, color);
            var brush = new SolidColorBrush(Color.FromRgb((byte)(rgb.X * 255), (byte)(rgb.Y * 255), (byte)(rgb.Z * 255)));
            bool edged = outline ?? VisualOptions.Current.CrosshairOutline;
            float scale = Crosshair.ScaleOf(size);
            IReadOnlyList<CrosshairBar> bars = Crosshair.BarsOf(style, scale);
            for (int i = 0; i < bars.Count; i++)
            {
                // Whole pixels, and with Y up: the shapes are measured the way
                // the HUD is drawn, and a window measures Y down.
                (float left, float right, float bottom, float top) =
                    Crosshair.EdgesOf(bars[i]);
                if (edged) context.FillRectangle(Brushes.Black, new Rect(cx + left - 1, cy - top - 1, right - left + 2, top - bottom + 2));
                context.FillRectangle(brush, new Rect(
                    cx + left, cy - top, right - left, top - bottom));
            }
            (float radius, float thickness) = Crosshair.RingOf(style, scale);
            if (thickness > 0)
            {
                if (edged) context.DrawEllipse(null, new Pen(Brushes.Black, thickness + 2), new Point(cx, cy), radius, radius);
                context.DrawEllipse(null, new Pen(brush, thickness),
                    new Point(cx, cy), radius, radius);
            }
        }
    }
}
