using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using MphRead.Mods.Chat;
using MphRead.Mods.Network;

namespace MphRead.Mods.Launcher.Gui
{
    internal sealed class LobbyChatPanel : Grid
    {
        private readonly StackPanel _messages = new() { Spacing = 4 };
        private readonly ScrollViewer _history;
        private readonly TextBox _entry = new() { Watermark = "Message the lobby", MaxLength = ChatPacket.MaxTextBytes };
        private readonly UiMark _send = new(UiMark.Shape.Accept, "Send");
        private int _revision = -1;
        public LobbyChatPanel()
        {
            RowDefinitions = new RowDefinitions("Auto,*,Auto");
            Children.Add(new Caption("Lobby chat"));
            _history = new ScrollViewer { Content = _messages, MinHeight = 32 };
            Grid.SetRow(_history, 1); Children.Add(_history);
            var input = new Grid { ColumnDefinitions = new ColumnDefinitions("*,Auto") };
            input.Children.Add(_entry); Grid.SetColumn(_send, 1); input.Children.Add(_send);
            Grid.SetRow(input, 2); Children.Add(input);
            _send.Click += (_, _) => Send();
            _entry.KeyDown += (_, e) =>
            {
                if (e.Key == Key.Enter) { Send(); e.Handled = true; }
                else if (e.Key == Key.Escape) { FocusNavigator.Focus(_send); e.Handled = true; }
            };
        }
        private void Send()
        {
            if (!_send.IsEnabled || string.IsNullOrWhiteSpace(_entry.Text)) return;
            NetChat.Send(_entry.Text); _entry.Text = "";
        }
        public void Refresh(bool connected)
        {
            _entry.IsEnabled = _send.IsEnabled = connected;
            if (_revision == NetChat.Revision) return;
            bool bottom = _revision < 0 || _history.Offset.Y >= _history.Extent.Height - _history.Viewport.Height - 8;
            var offset = _history.Offset;
            _revision = NetChat.Revision;
            _messages.Children.Clear();
            foreach (var message in NetChat.Entries)
                _messages.Children.Add(new TextBlock { Text = message.Text, TextWrapping = TextWrapping.Wrap,
                    FontSize = 13 * UiMetrics.TextFactor, FontFamily = GuiTheme.Display,
                    Foreground = message.System ? GuiTheme.WarmBrush : GuiTheme.TextBrush });
            Dispatcher.UIThread.Post(() => { if (bottom) _history.ScrollToEnd(); else _history.Offset = offset; }, DispatcherPriority.Loaded);
        }
    }
}
