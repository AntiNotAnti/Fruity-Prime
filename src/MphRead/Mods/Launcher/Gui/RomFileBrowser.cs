using System;
using System.IO;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;

namespace MphRead.Mods.Launcher.Gui
{
    /// <summary>The desktop ROM picker, drawn inside the existing game window.</summary>
    internal sealed class RomFileBrowser : UserControl
    {
        public event Action<string>? Selected;
        public event Action? Cancelled;

        private readonly RomFileBrowserModel _model;
        private readonly UiList _list = new();
        private readonly Note _directory = new("");
        private readonly Note _error = new("", GuiTheme.Bad);
        private readonly FieldRow _path = new("Path", "", boxWidth: 510);
        private readonly UiMark _use = new(UiMark.Shape.Accept, "use file");
        private readonly string _startingDirectory;
        private string? _selectedRom;
        private bool _submitted;
        private bool _rebuilding;
        private bool _settingPath;

        public RomFileBrowser(string? lastDirectory = null)
        {
            Background = Brushes.Transparent;
            Focusable = true;
            _model = new RomFileBrowserModel(lastDirectory ?? LauncherPrefs.LastRomDirectory);
            _startingDirectory = _model.CurrentDirectory;
            _list.SelectionChanged += (_, row) =>
            {
                if (_rebuilding)
                {
                    return;
                }
                if (row is UiListRow { Choice: RomFileEntry entry })
                {
                    SelectRow(entry);
                }
                else
                {
                    // The parent row is a navigation choice, not the ROM
                    // that happened to be highlighted before Up was pressed.
                    _selectedRom = null;
                    _use.IsEnabled = false;
                    SetPath(_model.CurrentDirectory);
                }
            };
            _list.Activated += (_, row) =>
            {
                if (row is UiListRow { Choice: RomFileEntry { Kind: RomFileEntryKind.Rom } })
                {
                    UseFile();
                }
            };
            _path.Box.KeyDown += (_, e) =>
            {
                if (e.Key == Key.Enter)
                {
                    OpenTypedPath();
                    e.Handled = true;
                }
            };
            _path.Box.TextChanging += (_, _) =>
            {
                if (!_settingPath)
                {
                    _selectedRom = null;
                    _use.IsEnabled = false;
                    _list.ClearSelection();
                }
            };
            var cancel = new UiMark(UiMark.Shape.Cancel, "cancel");
            cancel.Click += (_, _) => Cancel();
            _use.Click += (_, _) => UseFile();
            _use.IsEnabled = false;

            var body = new Grid { RowDefinitions = new RowDefinitions("Auto,*,Auto,Auto") };
            _directory.TextWrapping = TextWrapping.Wrap;
            Grid.SetRow(_directory, 0);
            body.Children.Add(_directory);
            Grid.SetRow(_list, 1);
            body.Children.Add(_list);
            Grid.SetRow(_path, 2);
            body.Children.Add(_path);
            Grid.SetRow(_error, 3);
            body.Children.Add(_error);
            Content = UiLayout.Page(overGame: false, UiLayout.WellSettings,
                "game files", strip: null, body: body, no: cancel, yes: _use);
            RebuildRows();
        }

        protected override void OnAttachedToVisualTree(VisualTreeAttachmentEventArgs e)
        {
            base.OnAttachedToVisualTree(e);
            Dispatcher.UIThread.Post(() => _list.FocusFirst(), DispatcherPriority.Background);
        }

        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (e.Key == Key.Escape)
            {
                Back();
                e.Handled = true;
                return;
            }
            if (e.Source is not TextBox && _list.HandleKey(e.Key))
            {
                e.Handled = true;
                return;
            }
            base.OnKeyDown(e);
        }

        private void RebuildRows()
        {
            _rebuilding = true;
            _selectedRom = null;
            _use.IsEnabled = false;
            _list.Clear();
            _directory.Text = _model.ShowingDrives ? "Available drives" : _model.CurrentDirectory;
            SetPath(_model.CurrentDirectory);
            if (!_model.ShowingDrives && (OperatingSystem.IsWindows()
                || Directory.GetParent(_model.CurrentDirectory) != null))
            {
                var up = new UiListRow("..", "parent folder");
                _list.Add(up);
                bool opening = false;
                up.Clicked += (_, _) =>
                {
                    if (!opening)
                    {
                        opening = true;
                        Dispatcher.UIThread.Post(() =>
                        {
                            try
                            {
                                OpenParent();
                            }
                            finally
                            {
                                opening = false;
                            }
                        });
                    }
                };
            }
            foreach (RomFileEntry entry in _model.Entries)
            {
                var row = new UiListRow(entry.Name,
                    entry.Kind == RomFileEntryKind.Directory ? "folder >" : ".nds")
                {
                    Choice = entry
                };
                _list.Add(row);
                bool opening = false;
                row.Clicked += (_, _) =>
                {
                    if (entry.Kind == RomFileEntryKind.Directory)
                    {
                        if (!opening)
                        {
                            opening = true;
                            // Enter raises Clicked and Activated on the old row.
                            // Navigate after both have finished dispatching.
                            Dispatcher.UIThread.Post(() =>
                            {
                                try
                                {
                                    OpenDirectory(entry.FullPath);
                                }
                                finally
                                {
                                    opening = false;
                                }
                            });
                        }
                    }
                    else
                    {
                        SelectRow(entry);
                    }
                };
            }
            if (_model.Entries.Count == 0)
            {
                _list.AddNote(_model.ShowingDrives ? "No available drives."
                    : "No folders or .nds files in this folder.");
            }
            _rebuilding = false;
            ShowError();
        }

        private void SelectRow(RomFileEntry entry)
        {
            _selectedRom = entry.Kind == RomFileEntryKind.Rom ? entry.FullPath : null;
            _use.IsEnabled = _selectedRom != null;
            SetPath(_selectedRom ?? _model.CurrentDirectory);
            _error.IsVisible = false;
        }

        private void OpenParent()
        {
            if (_model.GoUp())
            {
                RebuildRows();
                _list.FocusFirst();
            }
            else
            {
                ShowError();
            }
        }

        private void OpenDirectory(string path)
        {
            if (_model.NavigateTo(path))
            {
                RebuildRows();
                _list.FocusFirst();
            }
            else
            {
                ShowError();
            }
        }

        private void OpenTypedPath()
        {
            _selectedRom = null;
            _use.IsEnabled = false;
            string? full = _model.Resolve(_path.Value);
            if (full == null)
            {
                ShowError();
                return;
            }
            if (Directory.Exists(full))
            {
                OpenDirectory(full);
                return;
            }
            if (_model.TrySelectRom(full, out string? rom))
            {
                _selectedRom = rom;
                _use.IsEnabled = true;
                _list.ClearSelection();
                SetPath(rom!);
            }
            ShowError();
        }

        private void UseFile()
        {
            if (_submitted || _selectedRom == null)
            {
                return;
            }
            if (!_model.TrySelectRom(_selectedRom, out string? full))
            {
                ShowError();
                return;
            }
            _submitted = true;
            Selected?.Invoke(full!);
        }

        private void Back()
        {
            if (_model.ShowingDrives || SamePath(_model.CurrentDirectory, _startingDirectory)
                || (Directory.GetParent(_model.CurrentDirectory) == null
                    && !OperatingSystem.IsWindows()))
            {
                Cancel();
                return;
            }
            OpenParent();
        }

        private static bool SamePath(string a, string b) => String.Equals(
            Path.TrimEndingDirectorySeparator(a), Path.TrimEndingDirectorySeparator(b),
            OperatingSystem.IsWindows() ? StringComparison.OrdinalIgnoreCase
                : StringComparison.Ordinal);

        private void Cancel()
        {
            if (!_submitted)
            {
                _submitted = true;
                Cancelled?.Invoke();
            }
        }

        private void ShowError()
        {
            _error.Text = _model.Error ?? "";
            _error.IsVisible = _model.Error != null;
        }

        /// <summary>Insert desktop clipboard text at the path field's caret.</summary>
        internal bool PastePath(string? text)
        {
            if (!_path.Box.IsFocused)
            {
                return false;
            }
            if (!String.IsNullOrEmpty(text))
            {
                _path.Box.SelectedText = text;
            }
            else
            {
                _error.Text = text == null ? "Clipboard text could not be read."
                    : "Clipboard has no text to paste.";
                _error.IsVisible = true;
            }
            return true;
        }

        private void SetPath(string value)
        {
            _settingPath = true;
            try
            {
                _path.Value = value;
            }
            finally
            {
                _settingPath = false;
            }
        }
    }
}
