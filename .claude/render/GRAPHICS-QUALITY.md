# Graphics quality and accessibility

The existing world, cel, composite and HUD passes remain in place.
`Mods/Render/SceneQuality.cs` supplies sampler and composite uniforms through
small hooks in `Renderer.cs` and `RenderPassState.cs`. Desktop GLSL 1.20 and
Android ES 3.0 shaders implement the same operations; source hashes guard drift.

## Quality controls

Texture filtering offers Pixel, Bilinear, Trilinear and anisotropic 2x/4x/8x/16x.
Mipmaps are generated lazily for textures actually used by world materials and
invalidated when their base image changes. HUD atlases and render targets are
not mipmapped by this path. Anisotropy is capped to the advertised extension
limit, falling back to trilinear when unavailable. Pixel uses nearest sampling.

Render-scale stops are 50, 67, 75, 85, 100, 125, 150 and 200 percent. Legacy
25–49 percent configurations remain parseable. Scaled allocation arithmetic
uses a wide intermediate and never requests zero pixels. Above 100 percent the
scene is downsampled into the native window; HUD, menus and helmet remain native.
200 percent requires four times the native world pixel count and attachment
memory. The UI does not promise an FPS level for a given preset.

The existing composite samples optional FXAA from neighboring scene texels.
Sub-native scaling selects Bilinear or Sharp; Sharp adds a contrast-adaptive,
locally clamped unsharp filter. Enhanced color is an optional 8% saturation lift.
The disruption composite applies these same options before its existing effect.
HUD entry clears all quality flags so text and reticles are not postprocessed.

| Preset | Scale | Filtering | FXAA | Sharp | Color | Lighting | Cel |
|---|---:|---|---|---|---|---|---|
| Performance | 75% | Bilinear | Off | Off | Off | Off | Off |
| Classic | 100% | Pixel | Off | Off | Off | On | Off |
| Enhanced | 100% | Anisotropic 8x | On | On when sub-native | On | On | Off |
| Ultra | 200% | Anisotropic 16x | On | On when sub-native | On | On | Off |
| Cel | 100% | Trilinear | On | Off | Off | On | On |

Presets enable fog. Individual graphics edits select Custom; FPS cap and FOV
are independent. Accessibility preferences survive preset changes.

## Accessibility

`visuals.json` stores validated visual preferences beside `launcher.txt`. A
legacy filtering toggle migrates to Pixel/Bilinear only when the new file has
not been loaded. Reads are size-bounded; writes replace a temporary file.

- HUD scale: 70–100%; safe zone: 0–15; opacity: 30–100%. The centered HUD
  transform preserves native sampling. Aiming reticles and hit markers bypass
  the transform so dynamic aiming remains aligned with the world.
- UI scale: 75–150%, constrained by the existing minimum layout box.
  Text scale: 85–125%, including custom labels and Pro HUD numbers.
- Crosshair colors: health, white, cyan, yellow, magenta. The optional black
  outline and settings preview use the same bar/ring geometry.
- High contrast uses an opaque dark backdrop. Existing screens refresh after
  saving so returning to the front screen reflects the preference.
- Reduced flashes caps whiteout brightness while retaining its obscuration;
  it does not remove gameplay effects. Reduced shake applies 15% of the camera
  offset while preserving all original RNG calls and simulation timing.
- Two additional four-team palettes use blue/orange/pink/white or
  purple/gold/cyan/white. `TeamVisuals` feeds lobby accents and the existing
  shared objective/radar/HUD colors. Alternate palettes also enable eligible
  team body overrides; cloak, fade and status-color precedence are retained.

## Verification

`tools/render-resource-check` compiles/links desktop shaders, validates ES
hashes, and compiles ES sources where the desktop driver advertises ES3
compatibility. Synthetic FBO readbacks require visible output, changed pixels
for FXAA/sharpening/color, exact restoration when disabled, mipmap creation,
HUD opacity/scale and zero GL errors. It retains repeated cel-target teardown
checks. Desktop ES compilation is not Android device acceptance.

The existing respawn harness accepts `-quality` to enable all enhancements,
HUD adjustments and 100 → 50 → 200 → 100 percent transitions. The default
diagnostic remains comparable to earlier runs. See
[integration acceptance](../testing/UI-GRAPHICS-ACCEPTANCE.md) for actual results.

## Later effects assessed

Color grading is limited to the optional saturation operation above. Bloom
would need a reliable emissive mask and additional targets; applying it to
bright HUD/model colors would reduce gameplay clarity. Depth AO requires
depth reconstruction and separate Android/depth-fallback validation. Per-fragment
lighting changes the original vertex-lighting presentation, and projectile
lights need a bounded light budget. These remain later experiments, disabled
and unimplemented pending cross-driver acceptance of this baseline. No temporal
history, continuous UI animation or new render-pass architecture was introduced.
