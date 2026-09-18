using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;

namespace MphRead.Mods.Launcher.Gui
{
    internal sealed class UiBadge : Border
    {
        private readonly TextBlock _label = new() { FontFamily = GuiTheme.Display, FontSize = 12, FontWeight = FontWeight.Bold };
        public UiBadge(string text, UiTone tone = UiTone.Neutral)
        {
            Padding = new Thickness(8, 4); CornerRadius = new CornerRadius(4);
            Background = GuiTheme.PanelLightBrush; Child = _label; Set(text, tone);
        }
        public void Set(string text, UiTone tone) { _label.Text = text; _label.Foreground = Brush(tone); }
        internal static IBrush Brush(UiTone tone) => tone switch
        { UiTone.Good => GuiTheme.GoodBrush, UiTone.Warning => GuiTheme.WarmBrush, UiTone.Bad => GuiTheme.BadBrush, _ => GuiTheme.TextDimBrush };
    }
}
