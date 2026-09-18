using System;

namespace MphRead.Mods.Input
{
    // Observe uncalibrated normalized input; applying the result is an explicit UI action.
    public sealed class GamepadCalibration
    {
        private float _leftDrift, _rightDrift, _leftReach, _rightReach;
        private float _ltRest, _rtRest, _ltMax, _rtMax;
        private int _restSamples, _rangeSamples;
        public void Sample(GamepadState state, bool resting)
        {
            float left = MathF.Sqrt(state.LeftX * state.LeftX + state.LeftY * state.LeftY);
            float right = MathF.Sqrt(state.RightX * state.RightX + state.RightY * state.RightY);
            if (resting)
            {
                _restSamples++;
                _leftDrift = Math.Max(_leftDrift, left); _rightDrift = Math.Max(_rightDrift, right);
                _ltRest = Math.Max(_ltRest, state.LeftTrigger); _rtRest = Math.Max(_rtRest, state.RightTrigger);
            }
            else
            {
                _rangeSamples++;
                _leftReach = Math.Max(_leftReach, left); _rightReach = Math.Max(_rightReach, right);
                _ltMax = Math.Max(_ltMax, state.LeftTrigger); _rtMax = Math.Max(_rtMax, state.RightTrigger);
            }
        }
        public bool Valid => _restSamples >= 10 && _rangeSamples >= 10 && _leftDrift < .4f && _rightDrift < .4f
            && _leftReach >= .6f && _rightReach >= .6f;
        public string Summary => !Valid ? "Release both sticks during rest, then move both through their full range. Please retry."
            : $"Suggested dead zones: left {Math.Min(.9f, _leftDrift + .04f):0.00}, right {Math.Min(.9f, _rightDrift + .04f):0.00}. "
                + "Trigger ranges are adjusted only when a full press was measured.";
        public void Apply()
        {
            if (!Valid) throw new InvalidOperationException("Calibration is incomplete.");
            GamepadOptions.LeftInner = Math.Min(.9f, _leftDrift + .04f);
            GamepadOptions.RightInner = Math.Min(.9f, _rightDrift + .04f);
            GamepadOptions.LeftOuter = Math.Clamp(1 - _leftReach, 0, .4f);
            GamepadOptions.RightOuter = Math.Clamp(1 - _rightReach, 0, .4f);
            if (_ltMax - _ltRest >= .4f) { GamepadOptions.LeftTriggerMin = _ltRest; GamepadOptions.LeftTriggerMax = _ltMax; }
            if (_rtMax - _rtRest >= .4f) { GamepadOptions.RightTriggerMin = _rtRest; GamepadOptions.RightTriggerMax = _rtMax; }
            PadBindings.ApplyPreset("Custom");
        }
        public static float Trigger(float value, float min, float max)
            => Math.Clamp((GamepadAnalog.Finite(value, 0, 1) - min) / Math.Max(.1f, max - min), 0, 1);
    }
}
