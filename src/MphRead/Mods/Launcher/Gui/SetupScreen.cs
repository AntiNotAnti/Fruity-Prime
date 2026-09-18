using System;
using System.Collections.Generic;
using System.IO;
using System.Threading.Tasks;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Platform.Storage;
using Avalonia.Threading;

namespace MphRead.Mods.Launcher.Gui
{
    /// <summary>
    /// The one thing a fresh install needs before anything else can happen:
    /// the player's own cartridge dump.
    ///
    /// Shown *instead of* the front screen while there is nothing to load,
    /// rather than as one entry among five that are all refused. A menu whose
    /// entries do nothing is a program that looks broken; one screen asking
    /// for one file is a program waiting for you.
    /// </summary>
    internal sealed class SetupScreen : UserControl
    {
        /// <summary>Raised when the files are there, or when the player backed out.</summary>
        public event EventHandler? Closed;

        private readonly Note _log = new("", lines: 0);
        private readonly ProgressRow _progress = new();
        private readonly UiMark _choose;
        private readonly UiMark _back;
        private readonly UiWord _previews;

        /// <summary>
        /// The path, typed. The last resort, and on a Linux box with neither
        /// zenity nor kdialog the only one -- see
        /// <see cref="NativeFilePicker"/> for why the toolkit's own picker is
        /// not available to these screens. Hidden until that is the case:
        /// the screen has one thing to ask for and offers one way to answer.
        /// </summary>
        private readonly DeckField _typed = new("", widthEms: 26,
            watermark: "C:\\path\\to\\your.nds");
        private readonly StackPanel _typedRow;

        public SetupScreen()
        {
            Background = Brushes.Transparent;
            Focusable = true;

            var body = new StackPanel { Spacing = 8 };
            body.Children.Add(new Note(Mods.Branding.Name
                + " needs your own Metroid Prime Hunters cartridge dump. It unpacks what "
                + "it needs next to this program and leaves the file alone. No game data "
                + "is included in this download, and none is downloaded.", lines: 0));
            if (GameFiles.InProcessSetup)
            {
                body.Children.Add(new Note("The unpacked files land in " + GameFiles.Root
                    + " -- this device's own folder for the app, which shows up over USB "
                    + "under Android/data. Files already copied there are found without "
                    + "picking anything.", GuiTheme.TextDim, lines: 0));
            }
            // Previews are rendered here, from the files that are here. A run
            // can be interrupted and files can arrive after one, so asking for
            // the missing ones has to be possible without setting up again.
            _previews = new UiWord("Render map previews", 15, colour: GuiTheme.TextDim);
            _previews.Click += async (_, _) => await RenderPreviews();
            body.Children.Add(_previews);
            // The path, typed: the way in for a machine whose button opens
            // nothing. It is built either way and shown only then -- see
            // below.
            var typedGo = new UiWord("Use this file", 15, colour: GuiTheme.Accent);
            typedGo.Click += async (_, _) => await UseTypedPath();
            _typedRow = new StackPanel { Spacing = 6 };
            _typedRow.Children.Add(_typed);
            _typedRow.Children.Add(typedGo);
            // Hidden until it is the only way in, on every platform.
            //
            // The screen used to offer both at once on the reading that
            // somebody who knows where the file is would rather paste it than
            // walk a tree to it. That is one offer too many: a fresh install
            // has exactly one thing to do, and a text box beside the button
            // makes a player decide which half of the screen the answer is in
            // before they can give it. On a phone it is worse than redundant
            // -- the picker hands back a content:// document with no path
            // behind it (see RunSetup), so a typed path cannot name what the
            // button opens, and the keyboard covers half the screen to ask for
            // one.
            //
            // It is not gone: <see cref="ShowTyped"/> brings it back on a
            // machine with no dialog to open, which is a Linux box with
            // neither zenity nor kdialog and is the one case the row exists
            // for. See <see cref="NativeFilePicker"/>.
            _typedRow.IsVisible = false;
            _typed.Box.KeyDown += async (_, key) =>
            {
                if (key.Key == Key.Enter)
                {
                    key.Handled = true;
                    await UseTypedPath();
                }
            };
            body.Children.Add(_typedRow);
            _progress.IsVisible = false;
            body.Children.Add(_progress);
            body.Children.Add(new ScrollViewer
            {
                Height = 160,
                Content = _log,
                HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled
            });

            _choose = new UiMark(UiMark.Shape.Accept, "choose your .nds file");
            _choose.Click += async (_, _) => await ChooseRom();
            _back = new UiMark(UiMark.Shape.Cancel, "back")
            {
                // Nothing to go back to until there is something to play.
                IsVisible = GameFiles.Ready
            };
            _back.Click += (_, _) => Closed?.Invoke(this, EventArgs.Empty);

            var holder = new ScrollViewer
            {
                Content = body,
                HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled
            };
            Content = UiLayout.Page(overGame: false, UiLayout.WellSettings,
                "game files", strip: null, body: holder, no: _back, yes: _choose);
            RefreshPreviewEntry();
        }

        protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e)
        {
            base.OnAttachedToVisualTree(e);
            Dispatcher.UIThread.Post(() => _choose.Focus(), DispatcherPriority.Background);
        }

        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (e.Key == Key.Escape && _back.IsVisible)
            {
                Closed?.Invoke(this, EventArgs.Empty);
                e.Handled = true;
                return;
            }
            base.OnKeyDown(e);
        }

        /// <summary>Put the caret in the typed path, for when it is the only way in.</summary>
        private void ShowTyped()
        {
            // It is hidden until here: reaching this means the button has no
            // dialog behind it, typing is the only thing left, and the row has
            // to come back with the sentence that says so.
            _typedRow.IsVisible = true;
            Dispatcher.UIThread.Post(() => _typed.Box.Focus(), DispatcherPriority.Background);
        }

        /// <summary>
        /// The path the player typed, checked before it is acted on: a wrong
        /// path has to say so here rather than come back as an extractor error
        /// about a file it could not open.
        /// </summary>
        private async Task UseTypedPath()
        {
            string path = _typed.Value.Trim().Trim('"');
            if (path.Length == 0)
            {
                return;
            }
            path = Path.GetFullPath(path, ConsoleSetup.LaunchDirectory);
            if (!File.Exists(path))
            {
                _log.Text = $"There is no file at {path}.";
                return;
            }
            await RunSetup(path, null);
        }

        /// <summary>
        /// Ask for the cartridge dump, however this platform can be asked.
        ///
        /// Android has a real windowing backend and so a real
        /// <c>StorageProvider</c>. The desktop heads draw their screens with
        /// the headless one and have none, so they ask the operating system
        /// directly -- see <see cref="NativeFilePicker"/>, which is also where
        /// what happened before this is written down.
        /// </summary>
        private async Task ChooseRom()
        {
            TopLevel? top = TopLevel.GetTopLevel(this);
            if (top == null)
            {
                return;
            }
            if (!top.StorageProvider.CanOpen)
            {
                if (!NativeFilePicker.Available)
                {
                    _log.Text = "This desktop has no file dialog to open "
                        + "(install zenity or kdialog). Type the path instead.";
                    ShowTyped();
                    return;
                }
                string? chosen = await NativeFilePicker.OpenFile(
                    "Your Metroid Prime Hunters cartridge dump",
                    "Nintendo DS ROM", "nds");
                if (chosen != null)
                {
                    await RunSetup(chosen, null);
                }
                return;
            }
            var options = new FilePickerOpenOptions
            {
                Title = "Your Metroid Prime Hunters cartridge dump",
                AllowMultiple = false
            };
            if (!OperatingSystem.IsAndroid())
            {
                // Patterns are what Windows, Linux and the browser filter on.
                // Android filters by MIME type, and .nds has none -- inventing
                // one there produces a picker in which every file is refused.
                options.FileTypeFilter = new[]
                {
                    new FilePickerFileType("Nintendo DS ROM") { Patterns = new[] { "*.nds" } },
                    new FilePickerFileType("Every file") { Patterns = new[] { "*" } }
                };
            }
            IReadOnlyList<IStorageFile> picked =
                await top.StorageProvider.OpenFilePickerAsync(options);
            if (picked.Count == 0)
            {
                return;
            }
            await RunSetup(picked[0].TryGetLocalPath(), picked[0]);
        }

        /// <summary>
        /// Unpack the file that was picked, however it was picked.
        ///
        /// <paramref name="path"/> is a real path when there is one;
        /// <paramref name="file"/> is the toolkit's handle, which on Android
        /// is a content:// document with no path behind it and has to be
        /// copied before the extractor can read it.
        /// </summary>
        private async Task RunSetup(string? path, IStorageFile? file)
        {
            if (path == null && file == null)
            {
                return;
            }
            _choose.IsEnabled = false;
            _choose.Label = "working...";
            _log.Text = "";
            var progress = new SetupProgress();
            _progress.IsVisible = true;
            _progress.Set(0, "Starting");
            string? scratch = null;
            if (path == null && file != null)
            {
                // Android hands back a content:// document with no path behind
                // it. Copying is the only way to give the extractor a file, and
                // it is the player's own cartridge dump, so it is copied into
                // the app's directory and deleted afterwards.
                _log.Text = "Copying the file onto this device...";
                try
                {
                    scratch = Path.Combine(GameFiles.Root, "picked.nds");
                    await using (Stream source = await file.OpenReadAsync())
                    await using (var target = File.Create(scratch))
                    {
                        await source.CopyToAsync(target);
                    }
                    path = scratch;
                }
                catch (Exception ex)
                {
                    _log.Text = $"The file could not be read: {ex.Message}";
                    Ready();
                    return;
                }
            }
            if (path == null)
            {
                _log.Text = "That file could not be opened.";
                Ready();
                return;
            }
            string romPath = path;
            // Extraction takes minutes. Off the UI thread, or the screen stops
            // answering at the exact moment it is doing the one thing a fresh
            // install needs.
            bool ok = await Task.Run(() => GameFiles.RunSetup(romPath, line =>
                Dispatcher.UIThread.Post(() =>
                {
                    _log.Text = Tail(_log.Text, line);
                    if (progress.Observe(line))
                    {
                        _progress.Set(progress.Fraction, progress.Stage);
                    }
                })));
            if (scratch != null)
            {
                try
                {
                    File.Delete(scratch);
                }
                catch (IOException)
                {
                    // A copy left behind is untidy, not a failure worth saying.
                }
            }
            if (ok)
            {
                await RenderMissing(progress);
            }
            progress.Finish(ok);
            _progress.Set(progress.Fraction, progress.Stage);
            Ready();
            _log.Text = Tail(_log.Text, ok ? "Ready to play." : "Setup did not finish.");
            RefreshPreviewEntry();
            if (ok)
            {
                _progress.IsVisible = false;
                _back.IsVisible = true;
                Closed?.Invoke(this, EventArgs.Empty);
            }
        }

        private void Ready()
        {
            _choose.IsEnabled = true;
            _choose.Label = "choose your .nds file";
        }

        private async Task RenderPreviews()
        {
            _previews.IsEnabled = false;
            _previews.Text = "Rendering...";
            await RenderMissing(null);
            _previews.IsEnabled = true;
            _previews.Text = "Render map previews";
            RefreshPreviewEntry();
        }

        /// <summary>
        /// The pictures are made here, on this machine, from the files that
        /// were just unpacked. Nothing is downloaded and no picture ships with
        /// the program.
        /// </summary>
        private async Task RenderMissing(SetupProgress? progress)
        {
            if (!ThumbnailHost.CanRender)
            {
                return;
            }
            _log.Text = Tail(_log.Text, "Rendering map previews...");
            await ThumbnailHost.RenderMissingAsync(line => Dispatcher.UIThread.Post(() =>
            {
                _log.Text = Tail(_log.Text, line);
                if (progress != null && progress.Observe(line))
                {
                    _progress.Set(progress.Fraction, progress.Stage);
                }
            }));
        }

        /// <summary>How many previews are still to render, or nothing to say.</summary>
        private void RefreshPreviewEntry()
        {
            if (!GameFiles.Ready || !ThumbnailHost.CanRender)
            {
                _previews.IsVisible = false;
                return;
            }
            int missing = ThumbnailGenerator.MissingThumbnails().Count;
            _previews.IsVisible = missing > 0;
            _previews.IsEnabled = missing > 0;
        }

        /// <summary>Keep the last few lines; the extraction prints hundreds.</summary>
        private static string Tail(string? existing, string line)
        {
            string[] lines = ((existing ?? "") + "\n" + line)
                .Split('\n', StringSplitOptions.RemoveEmptyEntries);
            return String.Join("\n", lines[Math.Max(0, lines.Length - 8)..]);
        }
    }
}
