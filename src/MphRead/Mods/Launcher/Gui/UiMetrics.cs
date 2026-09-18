using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;

namespace MphRead.Mods.Launcher.Gui
{
    internal static class UiMetrics
    {
        public static double TextFactor => Mods.Render.VisualOptions.Current.TextScale / 100.0;
        public const double Spacing = 8;
        public const double ControlHeight = 34;
        public const double CornerRadius = 4;
        public const double FocusOutline = 2;
        public const double SectionSpacing = 18;
        public const double BodyText = 15;
        public const double MetadataText = 12;
        public const double HeadingText = 20;
        public const double DisabledOpacity = 0.5;
        public static readonly TimeSpan TransitionDuration = TimeSpan.FromMilliseconds(120);

        public static void DrawFocus(DrawingContext context, Control control)
        {
            if (!control.IsFocused || !control.IsEffectivelyEnabled) return;
            context.DrawRectangle(null, new Pen(GuiTheme.WarmBrush, FocusOutline),
                new Rect(1, 1, Math.Max(0, control.Bounds.Width - 2), Math.Max(0, control.Bounds.Height - 2)),
                CornerRadius, CornerRadius);
        }
    }
}
