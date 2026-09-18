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

        private readonly Note _log = new("") { IsVisible = false };
        private readonly Note _stage = new("1. Choose ROM   →   2. Extract   →   3. Generate previews   →   4. Ready");
        private readonly ProgressRow _progress = new();
        private readonly UiMark _choose;
        private readonly UiMark _back;
        private readonly UiWord _previews;
        private readonly Control _setupPage;

        public SetupScreen()
        {
            Background = Brushes.Transparent;
            Focusable = true;

            var body = new StackPanel { Spacing = 8 };
            body.Children.Add(_stage);
            body.Children.Add(new Note(Mods.Branding.Name
                + " needs your own Metroid Prime Hunters cartridge dump.\n\n"
                + $"It extracts the game data it needs into {Mods.Branding.Name}'s data folder and leaves your ROM unchanged. "
                + $"No game data is included with {Mods.Branding.Name} or downloaded from the Internet."));
            if (GameFiles.InProcessSetup)
            {
                body.Children.Add(new Note("The unpacked files land in " + GameFiles.Root
                    + " -- this device's own folder for the app, which shows up over USB "
                    + "under Android/data. Files already copied there are found without "
                    + "picking anything.", GuiTheme.TextDim));
            }
            // Previews are rendered here, from the files that are here. A run
            // can be interrupted and files can arrive after one, so asking for
            // the missing ones has to be possible without setting up again.
            _previews = new UiWord("Render map previews", 15, colour: GuiTheme.TextDim);
            _previews.Click += async (_, _) => await RenderPreviews();
            body.Children.Add(_previews);
            _progress.IsVisible = false;
            body.Children.Add(_progress);
            body.Children.Add(new ScrollViewer
            {
                Height = 160,
                Content = _log,
                HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled
            });

            _choose = new UiMark(UiMark.Shape.Accept, "Choose .nds file");
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
            _setupPage = UiLayout.Page(overGame: false, UiLayout.WellSettings,
                "game files", strip: null, body: holder, no: _back, yes: _choose);
            Content = _setupPage;
            RefreshPreviewEntry();
        }

        protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e)
        {
            base.OnAttachedToVisualTree(e);
            Dispatcher.UIThread.Post(() => _choose.Focus(), DispatcherPriority.Background);
        }

        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (e.Key == Key.Escape && ReferenceEquals(Content, _setupPage)
                && _back.IsVisible)
            {
                Closed?.Invoke(this, EventArgs.Empty);
                e.Handled = true;
                return;
            }
            base.OnKeyDown(e);
        }

        private async Task ChooseRom()
        {
            if (OperatingSystem.IsAndroid())
            {
                await ChooseRomAndroidAsync();
                return;
            }
            ShowDesktopRomBrowser();
        }

        private void ShowDesktopRomBrowser()
        {
            var browser = new RomFileBrowser();
            browser.Cancelled += () =>
            {
                Content = _setupPage;
                _choose.Focus();
            };
            browser.Selected += async path =>
            {
                Content = _setupPage;
                string? directory = Path.GetDirectoryName(path);
                if (directory != null)
                {
                    LauncherPrefs.LastRomDirectory = directory;
                    LauncherPrefs.Save();
                }
                await InstallRomAsync(path);
            };
            Content = browser;
            browser.Focus();
        }

        private async Task ChooseRomAndroidAsync()
        {
            TopLevel? top = TopLevel.GetTopLevel(this);
            if (top?.StorageProvider == null)
            {
                _log.IsVisible = true;
                _log.Text = "The device's file picker is unavailable.";
                return;
            }
            var options = new FilePickerOpenOptions
            {
                Title = "Your Metroid Prime Hunters cartridge dump",
                AllowMultiple = false
            };
            IReadOnlyList<IStorageFile> picked;
            try
            {
                // Android filters by MIME type, and .nds has none. Keep its
                // platform document provider without a fabricated filter.
                picked = await top.StorageProvider.OpenFilePickerAsync(options);
            }
            catch (Exception ex)
            {
                _log.IsVisible = true;
                _log.Text = $"The file picker could not be opened: {ex.Message}";
                _progress.IsVisible = false;
                return;
            }
            if (picked.Count == 0)
            {
                return;
            }
            _choose.IsEnabled = false;
            _choose.Label = "Working…";
            string? path = picked[0].TryGetLocalPath();
            string? scratch = null;
            if (path == null)
            {
                // Android hands back a content:// document with no path behind
                // it. Copying is the only way to give the extractor a file, and
                // it is the player's own cartridge dump, so it is copied into
                // the app's directory and deleted afterwards.
                _log.Text = "Copying the file to this device…";
                try
                {
                    scratch = Path.Combine(GameFiles.Root, "picked.nds");
                    await using (Stream source = await picked[0].OpenReadAsync())
                    await using (var target = File.Create(scratch))
                    {
                        await source.CopyToAsync(target);
                    }
                    path = scratch;
                }
                catch (Exception ex)
                {
                    _log.IsVisible = true;
                    _log.Text = $"The file could not be read: {ex.Message}";
                    _progress.IsVisible = false;
                    TryDelete(scratch);
                    Ready();
                    return;
                }
            }
            await InstallRomAsync(path, scratch);
        }

        private async Task InstallRomAsync(string path, string? temporaryPath = null)
        {
            if (!File.Exists(path) || !String.Equals(Path.GetExtension(path), ".nds",
                StringComparison.OrdinalIgnoreCase))
            {
                _log.IsVisible = true;
                _log.Text = "This is not a supported .nds file. Choose a Metroid Prime Hunters cartridge dump.";
                _progress.IsVisible = false;
                TryDelete(temporaryPath);
                Ready();
                return;
            }
            _choose.IsEnabled = false;
            _choose.Label = "Working…";
            _log.Text = "";
            _log.IsVisible = false;
            _stage.Text = "2 of 4 • Extracting game data…";
            var progress = new SetupProgress();
            _progress.IsVisible = true;
            _progress.Set(0, "Starting");
            // Extraction takes minutes. Off the UI thread, or the screen stops
            // answering at the exact moment it is doing the one thing a fresh
            // install needs.
            bool ok = false;
            try
            {
                ok = await Task.Run(() => GameFiles.RunSetup(path, line =>
                    Dispatcher.UIThread.Post(() =>
                    {
                        _log.Text = Tail(_log.Text, line);
                        if (progress.Observe(line))
                        {
                            _progress.Set(progress.Fraction, "Extracting game data…");
                        }
                    })));
                if (ok)
                {
                    await RenderMissing(progress);
                }
            }
            catch (Exception ex)
            {
                ok = false;
                _log.Text = Tail(_log.Text, $"Setup failed: {ex.Message}");
            }
            finally
            {
                TryDelete(temporaryPath);
            }
            _stage.Text = ok ? "4 of 4 • Ready" : "Setup could not finish. See the details below, then try again.";
            _log.IsVisible = !ok;
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

        private static void TryDelete(string? path)
        {
            if (path == null)
            {
                return;
            }
            try
            {
                File.Delete(path);
            }
            catch (IOException)
            {
                // A copy left behind is untidy, not a setup failure.
            }
            catch (UnauthorizedAccessException)
            {
                // Same: do not hide the real extraction result.
            }
        }

        private void Ready()
        {
            _choose.IsEnabled = true;
            _choose.Label = "Choose .nds file";
        }

        private async Task RenderPreviews()
        {
            _previews.IsEnabled = false;
            _previews.Text = "Rendering…";
            try
            {
                await RenderMissing(null);
                _stage.Text = "4 of 4 • Ready";
            }
            catch (Exception ex)
            {
                _log.IsVisible = true;
                _log.Text = "Map previews could not be generated: " + ex.Message + "\nTry Render map previews again.";
            }
            finally
            {
                _previews.IsEnabled = true;
                _previews.Text = "Render map previews";
                RefreshPreviewEntry();
            }
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
            _stage.Text = "3 of 4 • Generating missing previews…";
            _log.Text = Tail(_log.Text, "Rendering map previews…");
            await ThumbnailHost.RenderMissingAsync(line => Dispatcher.UIThread.Post(() =>
            {
                _log.Text = Tail(_log.Text, line);
                if (progress != null && progress.Observe(line))
                {
                    _progress.Set(progress.Fraction, "Generating missing previews…");
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
