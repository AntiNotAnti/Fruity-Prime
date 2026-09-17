using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using MphRead.Mods.Network;

namespace MphRead.Mods.Launcher.Gui
{
    internal sealed class LobbyPlayerRow : Border
    {
        public LobbyPlayerRow(RosterPacket roster, int index, byte owner)
        {
            int slot = roster.Slots[index];
            string team = UiText.Team(roster.Teams[index]);
            var lines = new StackPanel();
            var heading = new Grid { ColumnDefinitions = new("*,Auto,Auto") };
            heading.Children.Add(new TextBlock
            {
                Text = roster.Names[index],
                FontFamily = GuiTheme.Display, FontSize = 14 * UiMetrics.TextFactor,
                Foreground = roster.LobbyReady[index] ? GuiTheme.WarmBrush : GuiTheme.TextBrush,
                TextTrimming = TextTrimming.CharacterEllipsis
            });
            if (slot == owner)
            {
                var badge = new TextBlock { Text = "Host", Foreground = GuiTheme.WarmBrush,
                    FontSize = UiMetrics.MetadataText * UiMetrics.TextFactor, FontWeight = FontWeight.Bold, Margin = new Thickness(8, 0) };
                Grid.SetColumn(badge, 1); heading.Children.Add(badge);
            }
            var ready = new TextBlock { Text = UiText.Ready(roster.LobbyReady[index]),
                Foreground = roster.LobbyReady[index] ? GuiTheme.WarmBrush : GuiTheme.TextDimBrush, FontSize = UiMetrics.MetadataText * UiMetrics.TextFactor };
            Grid.SetColumn(ready, 2); heading.Children.Add(ready); lines.Children.Add(heading);
            lines.Children.Add(new Note($"{(Hunter)roster.Hunters[index]} · Suit {roster.Colors[index] + 1} · {team} · {roster.Pings[index]} ms"));
            if (roster.Teams[index] >= 0)
            {
                var color = Mods.Multiplayer.TeamVisuals.Get(roster.Teams[index]).Color;
                BorderBrush = new SolidColorBrush(Color.FromRgb(color.Red, color.Green, color.Blue));
                BorderThickness = new Thickness(3, 0, 0, 0);
            }
            Padding = new Thickness(5); Child = lines;
        }
    }
}
