using System;
using System.IO;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Media.Imaging;
using MphRead.Mods.Network;

namespace MphRead.Mods.Launcher.Gui
{
    internal sealed class ServerDetailsPanel : StackPanel
    {
        private readonly Note _title = new("Select a server"), _details = new(""), _reason = new("");
        private readonly UiBadge _badge = new("Checking…");
        private readonly Image _preview = new() { Height = 100, Stretch = Stretch.UniformToFill, IsVisible = false };
        private Bitmap? _bitmap;
        private string _room = "";
        internal readonly UiMark Join = new(UiMark.Shape.Accept, "Join Lobby");
        internal readonly UiMark Cancel = new(UiMark.Shape.Cancel, "Cancel Join") { IsVisible = false };
        internal bool CanJoin { get; private set; }
        public ServerDetailsPanel()
        {
            Spacing = 6; Margin = new Thickness(8);
            Children.Add(_title); Children.Add(_badge); Children.Add(_preview); Children.Add(_details);
            Children.Add(Join); Children.Add(Cancel); Children.Add(_reason);
            Join.IsEnabled = false;
        }
        public void Show(string name, string endpoint, ServerStatus? status, bool joining = false)
        {
            _title.Text = name;
            string room = status?.RoomKey ?? "";
            if (room != _room)
            {
                _room = room; _preview.Source = null; _bitmap?.Dispose(); _bitmap = null;
                try
                {
                    string path = ThumbnailGenerator.PathFor(room);
                    if (room.Length > 0 && File.Exists(path))
                    { using var stream = new MemoryStream(File.ReadAllBytes(path)); _bitmap = new Bitmap(stream); _preview.Source = _bitmap; }
                }
                catch (Exception) { /* The preview is optional; server state remains usable. */ }
                _preview.IsVisible = _bitmap != null;
            }
            var presentation = status.HasValue ? ServerPresentation.From(status.Value) : default;
            CanJoin = status.HasValue && presentation.CanJoin;
            _badge.Set(joining ? "Connecting…" : status.HasValue ? presentation.State : "Checking…", status.HasValue ? presentation.Tone : UiTone.Neutral);
            _details.Text = endpoint + (status is { } s ? $"\n{UiText.Map(s.RoomKey)} · {UiText.Mode(s.Mode)}\n{UiText.Format(s.Format)}\n{s.Players}/{(s.MaxPlayers > 0 ? s.MaxPlayers.ToString() : "?")} players · {(s.Latency >= 0 ? s.Latency + " ms" : "ping unavailable")}" : "");
            Join.Label = joining ? "Joining…" : status.HasValue ? presentation.JoinLabel : "Join Lobby";
            Join.IsEnabled = !joining && CanJoin; Cancel.IsVisible = joining;
            _reason.Text = joining ? "Contacting the server. You can cancel without leaving the browser." : status.HasValue ? presentation.Reason : "Checking server availability…";
        }
        protected override void OnDetachedFromVisualTree(VisualTreeAttachmentEventArgs e)
        { _preview.Source = null; _bitmap?.Dispose(); _bitmap = null; _room = ""; base.OnDetachedFromVisualTree(e); }
        public void Feedback(string text) => _reason.Text = text;
    }
}
