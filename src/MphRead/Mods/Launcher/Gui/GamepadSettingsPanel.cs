using System;
using System.Linq;
using Avalonia.Controls;
using Avalonia.Threading;
using Avalonia.VisualTree;
using MphRead.Mods.Input;

namespace MphRead.Mods.Launcher.Gui
{
    internal sealed class GamepadSettingsPanel : StackPanel
    {
        private readonly DispatcherTimer _timer;
        private string _deviceList = "";
        private ChoiceRow? _devices, _presetRow;
        private bool _refreshing;
        private GamepadFamily _shownFamily;
        private long _shownBindings = -1;
        private static readonly string[] Presets = { "Default", "Bumper Jumper", "Southpaw", "Classic", "Custom" };
        public GamepadSettingsPanel()
        {
            Spacing = 8;
            Reload();
            _timer = new DispatcherTimer(TimeSpan.FromMilliseconds(100), DispatcherPriority.Background,
                (_, _) => { if (IsEffectivelyVisible) { RefreshDevices(); RefreshLabels(); } });
            AttachedToVisualTree += (_, _) => _timer.Start();
            DetachedFromVisualTree += (_, _) => _timer.Stop();
        }
        public void Reload()
        {
            bool presetFocused = _presetRow?.IsFocused == true;
            Children.Clear(); _deviceList = ""; _devices = null; _presetRow = null;
            RefreshDevices();
            Children.Add(new GamepadMonitor());
            Choice("Button labels", new[] { "Automatic", "Xbox", "PlayStation", "Nintendo", "Generic" }, (int)GamepadOptions.GlyphStyle,
                i => GamepadOptions.GlyphStyle = (GamepadFamily)i);
            Choice("Preset", Presets, Array.IndexOf(Presets, PadBindings.Preset),
                i => { PadBindings.ApplyPreset(Presets[i]); RefreshLabels(); Dispatcher.UIThread.Post(Reload); });
            Children.Add(new TextBlock
            {
                Text = "Presets change button actions and swap sticks for Southpaw. Your aim calibration is retained.",
                FontSize = 11, Foreground = GuiTheme.TextDimBrush, TextWrapping = Avalonia.Media.TextWrapping.Wrap
            });
            Number("Horizontal sensitivity", GamepadOptions.LookX, .1f, 5, v => GamepadOptions.LookX = v);
            Number("Vertical sensitivity", GamepadOptions.LookY, .1f, 5, v => GamepadOptions.LookY = v);
            Number("Left inner deadzone", GamepadOptions.LeftInner, 0, .9f, v => GamepadOptions.LeftInner = v);
            Number("Left outer deadzone", GamepadOptions.LeftOuter, 0, .5f, v => GamepadOptions.LeftOuter = v);
            Number("Right inner deadzone", GamepadOptions.RightInner, 0, .9f, v => GamepadOptions.RightInner = v);
            Number("Right outer deadzone", GamepadOptions.RightOuter, 0, .5f, v => GamepadOptions.RightOuter = v);
            Choice("Aim curve", Enum.GetNames<GamepadCurve>(), (int)GamepadOptions.Curve, i => GamepadOptions.Curve = (GamepadCurve)i);
            Flag("Invert horizontal", GamepadOptions.InvertX, v => GamepadOptions.InvertX = v);
            Flag("Invert vertical", GamepadOptions.InvertY, v => GamepadOptions.InvertY = v);
            Flag("Southpaw", GamepadOptions.Southpaw, v => GamepadOptions.Southpaw = v);
            Number("Trigger actuation", GamepadOptions.TriggerThreshold, .05f, .95f, v => GamepadOptions.TriggerThreshold = v);
            Number("Device activity threshold", GamepadOptions.ActivityThreshold, .2f, .95f, v => GamepadOptions.ActivityThreshold = v);
            Flag("Vibration", GamepadOptions.Vibration, v => { GamepadOptions.Vibration = v; if (!v) GamepadHaptics.Stop(); });
            Number("Vibration strength", GamepadOptions.VibrationStrength, 0, 1,
                v => { GamepadOptions.VibrationStrength = v; if (v <= 0) GamepadHaptics.Stop(); });
            if (presetFocused) Dispatcher.UIThread.Post(() => FocusNavigator.Focus(_presetRow));
        }
        private void RefreshDevices()
        {
            var devices = GamepadManager.Devices;
            string signature = string.Join("|", devices.Select(d => d.DeviceId)) + GamepadManager.SelectedDeviceId;
            if (_devices != null && signature == _deviceList) return;
            _deviceList = signature;
            bool focused = _devices?.IsFocused == true;
            if (_devices != null) Children.Remove(_devices);
            string[] labels = new[] { "Automatic (last used)" }.Concat(devices.Select(d => d.Name)).ToArray();
            int selected = 0;
            for (int i = 0; i < devices.Count; i++) if (devices[i].DeviceId == GamepadManager.SelectedDeviceId) selected = i + 1;
            _devices = new ChoiceRow("Controller", labels, selected);
            _devices.Changed += (_, _) => GamepadManager.SelectDevice(_devices.Index == 0 ? null : devices[_devices.Index - 1].DeviceId);
            Children.Insert(0, _devices);
            if (focused) _devices.Focus();
        }
        internal void RefreshLabels()
        {
            if (_presetRow != null && _presetRow.Value != PadBindings.Preset)
            {
                _refreshing = true;
                _presetRow.Index = Math.Max(0, Array.IndexOf(Presets, PadBindings.Preset));
                _refreshing = false;
            }
            var family = GamepadOptions.GlyphStyle == GamepadFamily.Unknown
                ? GamepadManager.ActiveDevice?.Family ?? GamepadFamily.Generic : GamepadOptions.GlyphStyle;
            if (family == _shownFamily && _shownBindings == PadBindings.Revision) return;
            _shownFamily = family; _shownBindings = PadBindings.Revision;
            var settings = this.GetVisualAncestors().OfType<SettingsView>().FirstOrDefault();
            if (settings != null) foreach (var row in settings.GetVisualDescendants().OfType<PadRow>()) row.InvalidateVisual();
        }
        private void Number(string label, float value, float min, float max, Action<float> changed)
        {
            var row = new SliderRow(label, (int)Math.Round(value * 100), v => (v / 100f).ToString("0.00"),
                labelWidth: 210, min: (int)(min * 100), max: (int)(max * 100), keyStep: 1);
            row.ValueChanged += (_, _) => { changed(row.Value / 100f); PadBindings.ApplyPreset("Custom"); };
            Children.Add(row);
        }
        private void Flag(string label, bool value, Action<bool> changed)
        {
            var row = new ToggleRow(label, value);
            row.Changed += (_, _) => { changed(row.On); PadBindings.ApplyPreset("Custom"); };
            Children.Add(row);
        }
        private void Choice(string label, string[] options, int selected, Action<int> changed)
        {
            var row = new ChoiceRow(label, options, selected);
            if (label == "Preset") _presetRow = row;
            row.Changed += (_, _) => { if (_refreshing) return; changed(row.Index); if (label != "Preset") PadBindings.ApplyPreset("Custom"); };
            Children.Add(row);
        }
    }
}
