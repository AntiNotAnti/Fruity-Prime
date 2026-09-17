using System;
using System.Collections.Generic;
using System.Globalization;

namespace MphRead.Mods.Input
{
    // Independent of the engine so migration and calibration can be tested without a window.
    public static class GamepadOptions
    {
        public static float LeftInner = 0.2f, RightInner = 0.2f, LeftOuter, RightOuter;
        public static float LookX = 1, LookY = 1, TriggerThreshold = 0.60f, ActivityThreshold = 0.35f;
        public static bool InvertX, InvertY, Southpaw, Vibration = true;
        public static float VibrationStrength = 0.65f;
        public static GamepadCurve Curve = GamepadCurve.Classic;
        public static GamepadFamily GlyphStyle;

        public static void Load(IEnumerable<string> lines)
        {
            var values = new Dictionary<string, string>(StringComparer.Ordinal);
            foreach (var line in lines)
            {
                int split = line.IndexOf('=');
                if (split > 0) values[line[..split].Trim()] = line[(split + 1)..].Trim();
            }
            float Number(string key, float fallback, float min, float max)
                => values.TryGetValue(key, out var text) && float.TryParse(text, NumberStyles.Float,
                    CultureInfo.InvariantCulture, out var n) && float.IsFinite(n) ? Math.Clamp(n, min, max) : fallback;
            bool Flag(string key, bool fallback) => values.TryGetValue(key, out var text)
                && bool.TryParse(text, out var b) ? b : fallback;
            float legacyDead = Number("gamepad_deadzone", 0.2f, 0, 0.9f);
            float legacyLook = Number("gamepad_look", 1, 0.1f, 5);
            LeftInner = Number("gamepad_left_inner_deadzone", legacyDead, 0, 0.9f);
            RightInner = Number("gamepad_right_inner_deadzone", legacyDead, 0, 0.9f);
            LeftOuter = Number("gamepad_left_outer_deadzone", 0, 0, 0.5f);
            RightOuter = Number("gamepad_right_outer_deadzone", 0, 0, 0.5f);
            LookX = Number("gamepad_look_x", legacyLook, 0.1f, 5);
            LookY = Number("gamepad_look_y", legacyLook, 0.1f, 5);
            TriggerThreshold = Number("gamepad_trigger_threshold", 0.60f, 0.05f, 0.95f);
            ActivityThreshold = Number("gamepad_activity_threshold", 0.35f, 0.2f, 0.95f);
            VibrationStrength = Number("gamepad_vibration_strength", 0.65f, 0, 1);
            InvertX = Flag("gamepad_invert_x", false); InvertY = Flag("gamepad_invert_y", false);
            Southpaw = Flag("gamepad_southpaw", false); Vibration = Flag("gamepad_vibration", true);
            Curve = values.TryGetValue("gamepad_curve", out var c) && Enum.TryParse<GamepadCurve>(c, out var curve)
                && Enum.IsDefined(curve) ? curve : GamepadCurve.Classic;
            GlyphStyle = values.TryGetValue("gamepad_glyph_style", out var g) && Enum.TryParse<GamepadFamily>(g, out var glyph)
                && Enum.IsDefined(glyph) ? glyph : GamepadFamily.Unknown;
        }
        public static void Write(List<string> lines)
        {
            void Number(string key, float n) => lines.Add(key + "=" + n.ToString(CultureInfo.InvariantCulture));
            Number("gamepad_left_inner_deadzone", LeftInner); Number("gamepad_right_inner_deadzone", RightInner);
            Number("gamepad_left_outer_deadzone", LeftOuter); Number("gamepad_right_outer_deadzone", RightOuter);
            Number("gamepad_look_x", LookX); Number("gamepad_look_y", LookY);
            Number("gamepad_trigger_threshold", TriggerThreshold); Number("gamepad_activity_threshold", ActivityThreshold);
            Number("gamepad_vibration_strength", VibrationStrength);
            lines.Add("gamepad_invert_x=" + InvertX); lines.Add("gamepad_southpaw=" + Southpaw);
            lines.Add("gamepad_vibration=" + Vibration); lines.Add("gamepad_curve=" + Curve);
            lines.Add("gamepad_glyph_style=" + GlyphStyle);
        }
        public static void Reset() => Load(Array.Empty<string>());
    }
}
