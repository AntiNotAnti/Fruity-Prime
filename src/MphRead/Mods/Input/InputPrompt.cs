namespace MphRead.Mods.Input
{
    public readonly record struct InputPrompt(GamepadButtons Button, string Label, GamepadButtons Modifier = 0)
    {
        public static InputPrompt For(UiAction action) => new(action switch {
            UiAction.Accept => GamepadButtons.A, UiAction.Back => GamepadButtons.B,
            UiAction.PreviousTab => GamepadButtons.LeftBumper, UiAction.NextTab => GamepadButtons.RightBumper,
            UiAction.PageUp => GamepadButtons.LeftTrigger, UiAction.PageDown => GamepadButtons.RightTrigger,
            UiAction.Up => GamepadButtons.DpadUp, UiAction.Down => GamepadButtons.DpadDown,
            UiAction.Left => GamepadButtons.DpadLeft, _ => GamepadButtons.DpadRight }, action.ToString());
        public static InputPrompt For(PadAction action) => new(PadBindings.Slot(action, 0), PadBindings.Name(action), PadBindings.Modifier(action, 0));
        public string Glyph => (Modifier == 0 ? "" : GamepadGlyphs.Resolve(Modifier) + " + ") + GamepadGlyphs.Resolve(Button);
        public override string ToString() => Glyph + ": " + Label;
    }
}
