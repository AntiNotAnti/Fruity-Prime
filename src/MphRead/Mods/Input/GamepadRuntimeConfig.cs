namespace MphRead.Mods.Input
{
    public sealed class GamepadRuntimeConfig
    {
        internal static GamepadRuntimeConfig Fallback = new();
        internal static GamepadRuntimeConfig Current = Fallback;
        public GamepadOptionState Options { get; }
        public PadBindingState Bindings { get; }
        public ControllerLayoutState Layout { get; }
        public GamepadRuntimeConfig() : this(new(), new()) { }
        private GamepadRuntimeConfig(GamepadOptionState options, PadBindingState bindings)
        { Options = options; Bindings = bindings; Layout = new(options, bindings); }
        public GamepadRuntimeConfig Clone() => new(Options.Clone(), Bindings.Clone());
    }
}
