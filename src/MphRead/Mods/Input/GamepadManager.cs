using System;
using System.Collections.Generic;

namespace MphRead.Mods.Input
{
    public enum GamepadFamily { Unknown, Xbox, PlayStation, Nintendo, Generic }
    [Flags]
    public enum GamepadCapabilities { None = 0, Rumble = 1, Gyro = 2, Touchpad = 4, AnalogTriggers = 8 }

    public sealed class GamepadDevice
    {
        public string DeviceId { get; init; } = "";
        public string Name { get; internal set; } = "";
        public GamepadFamily Family { get; internal set; }
        public GamepadCapabilities Capabilities { get; internal set; }
        public bool IsMapped { get; internal set; }
        public string Mapping { get; internal set; } = "";
        public GamepadState State { get; internal set; }
        internal bool LeftTriggerHeld, RightTriggerHeld;
    }

    public readonly record struct GamepadSnapshot(string? DeviceId, GamepadState State, long Revision);

    public static class GamepadManager
    {
        private static readonly object Gate = new();
        private static readonly List<GamepadDevice> Known = new();
        private static GamepadDevice? _active;
        private static string? _selected;
        private static bool _used;
        private static long _revision;
        private static GamepadSnapshot _snapshot = new(null, default, 0);
        public static GamepadSnapshot Snapshot { get { lock (Gate) return _snapshot; } }
        public static GamepadState ActiveState => Snapshot.State;
        public static GamepadDevice? ActiveDevice { get { lock (Gate) return _active; } }
        public static string? SelectedDeviceId { get { lock (Gate) return _selected; } }
        public static string? LastInputDevice { get; private set; }
        public static IReadOnlyList<GamepadDevice> Devices { get { lock (Gate) return Known.ToArray(); } }
        public static event Action<GamepadDevice>? DeviceAdded;
        public static event Action<GamepadDevice>? DeviceRemoved;
        public static event Action? ActiveChanged;

        private static GamepadDevice? Find(string id)
        {
            foreach (var device in Known) if (device.DeviceId == id) return device;
            return null;
        }

        private static void Activate(GamepadDevice? device)
        {
            if (_active == device) return;
            _active = device;
            _revision++;
            Publish();
            ActiveChanged?.Invoke();
        }
        private static void Publish() => _snapshot = new(_active?.DeviceId, _active?.State ?? default, _revision);

        public static void UpdateDevice(string id, GamepadState state, bool mapped,
            GamepadFamily family = GamepadFamily.Unknown,
            GamepadCapabilities capabilities = GamepadCapabilities.None, string? mapping = null)
        {
            lock (Gate)
            {
                var device = Find(id);
                bool added = device == null;
                if (device == null)
                {
                    device = new GamepadDevice { DeviceId = id };
                    Known.Add(device);
                }
                var previous = device.State;
                state.LeftX = GamepadAnalog.Finite(state.LeftX);
                state.LeftY = GamepadAnalog.Finite(state.LeftY);
                state.RightX = GamepadAnalog.Finite(state.RightX);
                state.RightY = GamepadAnalog.Finite(state.RightY);
                state.LeftTrigger = GamepadAnalog.Finite(state.LeftTrigger, 0, 1);
                state.RightTrigger = GamepadAnalog.Finite(state.RightTrigger, 0, 1);
                device.LeftTriggerHeld = GamepadAnalog.Trigger(state.LeftTrigger, device.LeftTriggerHeld, GamepadOptions.TriggerThreshold);
                device.RightTriggerHeld = GamepadAnalog.Trigger(state.RightTrigger, device.RightTriggerHeld, GamepadOptions.TriggerThreshold);
                if (device.LeftTriggerHeld) state.Buttons |= GamepadButtons.LeftTrigger;
                if (device.RightTriggerHeld) state.Buttons |= GamepadButtons.RightTrigger;
                state.Connected = true;
                device.State = state;
                string name = state.Name ?? "gamepad";
                if (added || device.Name != name || family != GamepadFamily.Unknown)
                    device.Family = family == GamepadFamily.Unknown ? GamepadGlyphs.Detect(name) : family;
                device.Name = name;
                device.IsMapped = mapped;
                device.Mapping = mapping ?? (mapped ? "Platform mapping" : "Unmapped fallback");
                device.Capabilities = capabilities;
                bool activity = (state.Buttons & ~previous.Buttons) != 0
                    || StickActivity(state.LeftX, state.LeftY, previous.LeftX, previous.LeftY)
                    || StickActivity(state.RightX, state.RightY, previous.RightX, previous.RightY);
                if (_selected == id || (_selected == null && (_active == null
                    || (!_used && mapped && !_active.IsMapped)))) Activate(device);
                if (activity)
                {
                    LastInputDevice = id;
                    InputSourceTracker.Note(InputSource.Gamepad);
                    if (_selected == null || _selected == id) { _used = true; Activate(device); }
                }
                Publish();
                if (added) DeviceAdded?.Invoke(device);
            }
        }

        private static bool StickActivity(float x, float y, float oldX, float oldY)
            => x * x + y * y > GamepadOptions.ActivityThreshold * GamepadOptions.ActivityThreshold
                && (Math.Abs(x - oldX) > 0.08f || Math.Abs(y - oldY) > 0.08f);

        public static void SelectDevice(string? id)
        {
            lock (Gate)
            {
                _selected = string.IsNullOrEmpty(id) ? null : id;
                _used = false;
                if (_selected != null) Activate(Find(_selected));
                else if (_active == null) Activate(Known.Find(d => d.IsMapped) ?? Known.Find(_ => true));
            }
        }
        public static void ClearDevice(string id)
        {
            lock (Gate)
            {
                var device = Find(id);
                if (device == null) return;
                device.State = new GamepadState { Connected = true, Name = device.Name };
                device.LeftTriggerHeld = device.RightTriggerHeld = false;
                if (_active == device) { _revision++; Publish(); }
            }
        }
        public static void RemoveDevice(string id)
        {
            lock (Gate)
            {
                var device = Find(id);
                if (device == null) return;
                device.State = default;
                Known.Remove(device);
                if (_selected == id) _selected = null;
                if (_active == device) { _used = false; Activate(null); }
                DeviceRemoved?.Invoke(device);
            }
        }
        public static void ClearAll()
        {
            lock (Gate) foreach (var device in Known) ClearDevice(device.DeviceId);
        }
    }
}
