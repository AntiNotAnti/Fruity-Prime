using System;
using System.IO;
using System.Text.Json;
using MphRead.Mods.Launcher;

namespace MphRead.Mods.Render
{
    public enum TextureQuality { Pixel, Bilinear, Trilinear, Anisotropic2, Anisotropic4, Anisotropic8, Anisotropic16 }
    public enum GraphicsPreset { Performance, Classic, Enhanced, Ultra, Custom, Cel }

    public sealed record VisualSettings
    {
        public TextureQuality Filtering { get; init; } = TextureQuality.Pixel;
        public GraphicsPreset Preset { get; init; } = GraphicsPreset.Custom;
        public bool Fxaa { get; init; }
        public bool SharpUpscaling { get; init; }
        public bool EnhancedColor { get; init; }
        public int HudScale { get; init; } = 100;
        public int UiScale { get; init; } = 100;
        public int TextScale { get; init; } = 100;
        public int SafeZone { get; init; }
        public int HudOpacity { get; init; } = 100;
        public int CrosshairColor { get; init; }
        public bool CrosshairOutline { get; init; } = true;
        public bool HighContrast { get; init; }
        public bool ReducedFlashes { get; init; }
        public bool ReducedShake { get; init; }
        public int TeamPalette { get; init; }

        public VisualSettings Validated() => this with
        {
            Filtering = Enum.IsDefined(Filtering) ? Filtering : TextureQuality.Pixel,
            Preset = Enum.IsDefined(Preset) ? Preset : GraphicsPreset.Custom,
            HudScale = Math.Clamp(HudScale, 70, 100), UiScale = Math.Clamp(UiScale, 75, 150),
            TextScale = Math.Clamp(TextScale, 85, 125), SafeZone = Math.Clamp(SafeZone, 0, 15),
            HudOpacity = Math.Clamp(HudOpacity, 30, 100), CrosshairColor = Math.Clamp(CrosshairColor, 0, 4),
            TeamPalette = Math.Clamp(TeamPalette, 0, 2)
        };
    }

    public static class VisualOptions
    {
        private static VisualSettings _current = new();
        public static event Action? Changed;
        public static VisualSettings Current
        {
            get => _current;
            set
            {
                var next = value.Validated();
                if (_current == next) return;
                _current = next; Mods.Multiplayer.TeamVisuals.ApplyPalette(); Changed?.Invoke();
            }
        }
        public static GraphicsPreset DisplayPreset
        {
            get
            {
                var expected = Preset(Current.Preset, Current);
                return expected.Scale == RenderOptions.ResolutionScale && expected.Lighting == RenderOptions.Lighting
                    && expected.Fog == RenderOptions.Fog && expected.Cel == RenderOptions.CelShading
                    && expected.Settings.Filtering == Current.Filtering && expected.Settings.Fxaa == Current.Fxaa
                    && expected.Settings.SharpUpscaling == Current.SharpUpscaling
                    && expected.Settings.EnhancedColor == Current.EnhancedColor ? Current.Preset : GraphicsPreset.Custom;
            }
        }

        private static bool _loaded;
        public static readonly int[] ScaleStops = { 50, 67, 75, 85, 100, 125, 150, 200 };
        public static readonly string[] FilterNames = { "Pixel", "Bilinear", "Trilinear", "Anisotropic 2x", "Anisotropic 4x", "Anisotropic 8x", "Anisotropic 16x" };
        public static readonly string[] PresetNames = { "Performance", "Classic", "Enhanced", "Ultra", "Custom", "Cel" };

        public static void LoadOnce(bool legacyFiltering)
        {
            if (_loaded) return;
            _loaded = true;
            Current = new() { Filtering = legacyFiltering ? TextureQuality.Bilinear : TextureQuality.Pixel };
            try
            {
                string path = Path.Combine(LauncherPrefs.Directory, "visuals.json");
                if (File.Exists(path) && new FileInfo(path).Length <= 16384)
                    Current = (JsonSerializer.Deserialize<VisualSettings>(File.ReadAllText(path)) ?? Current).Validated();
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException or JsonException)
            { Console.WriteLine("[graphics] Could not read visual preferences: " + ex.Message); }
        }

        public static void Save(VisualSettings settings)
        {
            settings = settings.Validated();
            Directory.CreateDirectory(LauncherPrefs.Directory);
            string path = Path.Combine(LauncherPrefs.Directory, "visuals.json");
            File.WriteAllText(path + ".tmp", JsonSerializer.Serialize(settings, new JsonSerializerOptions { WriteIndented = true }));
            File.Move(path + ".tmp", path, overwrite: true);
            Current = settings;
        }

        public static (int Scale, bool Lighting, bool Fog, bool Cel, VisualSettings Settings) Preset(GraphicsPreset preset, VisualSettings current) => preset switch
        {
            GraphicsPreset.Performance => (75, false, true, false, current with { Preset = preset, Filtering = TextureQuality.Bilinear, Fxaa = false, SharpUpscaling = false, EnhancedColor = false }),
            GraphicsPreset.Classic => (100, true, true, false, current with { Preset = preset, Filtering = TextureQuality.Pixel, Fxaa = false, SharpUpscaling = false, EnhancedColor = false }),
            GraphicsPreset.Enhanced => (100, true, true, false, current with { Preset = preset, Filtering = TextureQuality.Anisotropic8, Fxaa = true, SharpUpscaling = true, EnhancedColor = true }),
            GraphicsPreset.Ultra => (200, true, true, false, current with { Preset = preset, Filtering = TextureQuality.Anisotropic16, Fxaa = true, SharpUpscaling = true, EnhancedColor = true }),
            GraphicsPreset.Cel => (100, true, true, true, current with { Preset = preset, Filtering = TextureQuality.Trilinear, Fxaa = true, SharpUpscaling = false, EnhancedColor = false }),
            _ => (RenderOptions.ResolutionScale, RenderOptions.Lighting, RenderOptions.Fog, RenderOptions.CelShading, current)
        };
    }
}
