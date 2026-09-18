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
        private long _shownBindings = -1, _profileRevision;
        private long _runtimeRevision = -1;
        private static readonly string[] Presets = { "Default", "Bumper Jumper", "Southpaw", "Classic", "Custom" };
        public GamepadSettingsPanel()
        {
            Spacing = 8;
            Reload();
            _timer = new DispatcherTimer(TimeSpan.FromMilliseconds(100), DispatcherPriority.Background,
                (_, _) => { if (IsEffectivelyVisible) {
                    if ((_profileRevision != GamepadProfiles.Revision || _runtimeRevision != GamepadManager.Snapshot.Revision)
                        && !GamepadContexts.Capturing) Reload();
                    RefreshDevices(); RefreshLabels(); } });
            AttachedToVisualTree += (_, _) => _timer.Start();
            DetachedFromVisualTree += (_, _) => _timer.Stop();
        }
        public void Reload()
        {
            var focused = FocusNavigator.Focused(this);
            string? focusedId = focused?.GetValue(ControllerNav.NavIdProperty);
            Children.Clear(); _deviceList = ""; _devices = null; _presetRow = null;
            _profileRevision = GamepadProfiles.Revision;
            _runtimeRevision = GamepadManager.Snapshot.Revision;
            RefreshDevices();
            Children.Add(new GamepadMonitor());
            Choice("controller.button_labels", "Button labels", new[] { "Automatic", "Xbox", "PlayStation", "Nintendo", "Generic" }, (int)GamepadOptions.GlyphStyle,
                i => GamepadOptions.GlyphStyle = (GamepadFamily)i);
            Choice("controller.control_layout", "Control layout", Presets, Array.IndexOf(Presets, PadBindings.Preset),
                i => { PadBindings.ApplyPreset(Presets[i]); RefreshLabels(); Dispatcher.UIThread.Post(Reload); });
            Children.Add(new TextBlock
            {
                Text = "Presets change button actions and swap sticks for Southpaw. Your aim calibration is retained.",
                FontSize = 11, Foreground = GuiTheme.TextDimBrush, TextWrapping = Avalonia.Media.TextWrapping.Wrap
            });
            Number("controller.horizontal_sensitivity", "Horizontal sensitivity", GamepadOptions.LookX, .1f, 5, v => GamepadOptions.LookX = v);
            Number("controller.vertical_sensitivity", "Vertical sensitivity", GamepadOptions.LookY, .1f, 5, v => GamepadOptions.LookY = v);
            Number("controller.scoped_horizontal_multiplier", "Scoped horizontal multiplier", GamepadOptions.ScopedX, .1f, 3, v => GamepadOptions.ScopedX = v);
            Number("controller.scoped_vertical_multiplier", "Scoped vertical multiplier", GamepadOptions.ScopedY, .1f, 3, v => GamepadOptions.ScopedY = v);
            Flag("controller.toggle_weapon_wheel", "Toggle weapon wheel", GamepadOptions.WheelToggle, v => GamepadOptions.WheelToggle = v);
            Number("controller.wheel_selection_threshold", "Wheel selection threshold", GamepadOptions.WheelThreshold, .1f, .95f, v => GamepadOptions.WheelThreshold = v);
            var modifiers = Enum.GetValues<GamepadButtons>();
            Choice("controller.modifier_for_new_bindings", "Modifier for new bindings", modifiers.Select(PadBindings.Describe).ToArray(),
                Array.IndexOf(modifiers, GamepadOptions.BindingModifier), i => GamepadOptions.BindingModifier = modifiers[i]);
            Children.Add(new Note("Choose a modifier before rebinding. Modifiers used in combinations are reserved during gameplay. Hold the modifier first, then press the action button."));
            string[] weapons = { "Volt Driver", "Battlehammer", "Imperialist", "Judicator", "Magmaul", "Shock Coil" };
            for (int position = 0; position < 6; position++)
            {
                int index = position;
                Choice("controller.wheel." + position, "Wheel position " + (position + 1), weapons, GamepadOptions.WheelOrder[position],
                    slot => { GamepadOptions.SetWheelSlot(index, slot); Dispatcher.UIThread.Post(Reload); });
            }
            Children.Add(new Note("Wheel positions run clockwise from the top. Choosing an occupied position swaps the two weapons."));
            Number("controller.left_inner_deadzone", "Left inner deadzone", GamepadOptions.LeftInner, 0, .9f, v => GamepadOptions.LeftInner = v);
            Number("controller.left_outer_deadzone", "Left outer deadzone", GamepadOptions.LeftOuter, 0, .5f, v => GamepadOptions.LeftOuter = v);
            Number("controller.right_inner_deadzone", "Right inner deadzone", GamepadOptions.RightInner, 0, .9f, v => GamepadOptions.RightInner = v);
            Number("controller.right_outer_deadzone", "Right outer deadzone", GamepadOptions.RightOuter, 0, .5f, v => GamepadOptions.RightOuter = v);
            Choice("controller.aim_curve", "Aim curve", Enum.GetNames<GamepadCurve>(), (int)GamepadOptions.Curve, i => GamepadOptions.Curve = (GamepadCurve)i);
            Flag("controller.invert_horizontal", "Invert horizontal", GamepadOptions.InvertX, v => GamepadOptions.InvertX = v);
            Flag("controller.invert_vertical", "Invert vertical", GamepadOptions.InvertY, v => GamepadOptions.InvertY = v);
            Flag("controller.southpaw", "Southpaw", GamepadOptions.Southpaw, v => GamepadOptions.Southpaw = v);
            Number("controller.trigger_actuation", "Trigger actuation", GamepadOptions.TriggerThreshold, .05f, .95f, v => GamepadOptions.TriggerThreshold = v);
            Number("controller.device_activity_threshold", "Device activity threshold", GamepadOptions.ActivityThreshold, .2f, .95f, v => GamepadOptions.ActivityThreshold = v);
            Flag("controller.vibration", "Vibration", GamepadOptions.Vibration, v => { GamepadOptions.Vibration = v; if (!v) GamepadHaptics.Stop(); });
            Number("controller.vibration_strength", "Vibration strength", GamepadOptions.VibrationStrength, 0, 1,
                v => { GamepadOptions.VibrationStrength = v; if (v <= 0) GamepadHaptics.Stop(); });
            Children.Add(new GamepadSetupPanel(() => Dispatcher.UIThread.Post(Reload)));
            Children.Add(new GamepadProfilePanel(() => Dispatcher.UIThread.Post(Reload)));
            if (focusedId != null)
            {
                TopLevel.GetTopLevel(this)?.UpdateLayout();
                FocusNavigator.Focus(ControllerNav.Find(this, focusedId));
            }
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
            ControllerNav.Identify(_devices, "controller.device");
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
        private void Number(string id, string label, float value, float min, float max, Action<float> changed)
        {
            var row = new SliderRow(label, (int)Math.Round(value * 100), v => (v / 100f).ToString("0.00"),
                labelWidth: 210, min: (int)(min * 100), max: (int)(max * 100), keyStep: 1);
            row.ValueChanged += (_, _) => { changed(row.Value / 100f); };
            ControllerNav.Identify(row, id);
            Children.Add(row);
        }
        private void Flag(string id, string label, bool value, Action<bool> changed)
        {
            var row = new ToggleRow(label, value);
            row.Changed += (_, _) => { changed(row.On); };
            ControllerNav.Identify(row, id);
            Children.Add(row);
        }
        private void Choice(string id, string label, string[] options, int selected, Action<int> changed)
        {
            var row = new ChoiceRow(label, options, selected);
            if (label == "Control layout") _presetRow = row;
            row.Changed += (_, _) => { if (_refreshing) return; changed(row.Index); };
            ControllerNav.Identify(row, id);
            Children.Add(row);
        }
    }
}
