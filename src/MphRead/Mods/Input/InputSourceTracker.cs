using System;

namespace MphRead.Mods.Input
{
    public enum InputSource { KeyboardMouse, Gamepad, Touch }
    public static class InputSourceTracker
    {
        public static InputSource Current { get; private set; }
        private static long _changed;
        public static void Note(InputSource source) => Note(source, Environment.TickCount64);
        public static void Note(InputSource source, long milliseconds)
        {
            if (source == Current || milliseconds - _changed < 180) return;
            Current = source;
            _changed = milliseconds;
        }
    }
}
