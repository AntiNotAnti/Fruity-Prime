# Bright player skins

Display settings has a Visibility section with three **Player skins** choices:
Off, Textured, and Solid. Textured lifts dark lighting while keeping the real
suit textures; Solid uses the original flat identification color. **Player
outline** is independent: Off, Team color, or Bright red, with a thickness of
1–8 window pixels (default 4). Team outlines use red in FFA.

Both features are off by default and apply on Save/Apply without restarting.
`launcher.txt` keeps `bright_skins`, `bright_skin_style`, `player_outline`, and
`player_outline_width`. Old `bright_skins=true` preferences keep Solid mode and
do not enable outlines. These are client preferences; no server rules, packets,
replacement assets, or duplicate models are introduced.

`Mods/Render/BrightSkins.cs` owns eligibility and color policy. FFA reads the
player's resolved `Recolor`, then the representative palette color cached by
`HunterSuits`. That cache samples all six palettes, including the two team
palettes; the suit picker and collision resolver still offer four FFA suits.
Team play uses `Metadata.TeamColors[TeamIndex]`, the centralized 5-bit colors
already used by objectives and the HUD. Invalid identities fall back to a
neutral color.

Colors raise the strongest channel to 0.95. If luminance remains below 0.45,
the smallest blend toward white that reaches the floor reduces saturation
without rotating hue. Simply multiplying and clamping cannot bring saturated
blue to that floor. These are rendering tuning values, not protocol constants.

The normal and alternate body paths compute colors once per draw and forward them
through the existing mesh traversal, including Kanden segments and Spire's
alternate attack. Weavel's turret caches the owner's color for its body draw;
its separate ice model does not receive it. Guns, smoke, ice overlays, trails,
particles, projectiles, shadows, scan geometry and HUD draws keep their existing
paths. Neither visibility tests nor depth/culling state change.

The override is suppressed for the main player, single-player, death, damage
flash/palette override, and Double Damage. Cloak checks include both the flag
and current/target alpha: passive Trace/Prime Hunter cloak does not set that
flag, and expiration fades outlast it. Remote bodies remain eligible while the
local player spectates.

Both main shaders apply the RGB override before cel processing and fog.
Textured geometry keeps material and texture alpha with override alpha 1.
Untextured geometry replaces alpha in those shaders, so `ForMaterial` supplies
material alpha times entity alpha for that path (the vertex shaders emit alpha
1). The Debug viewer's texture toggle also selects that untextured branch, even for
a material with a texture, so color preparation reads the scene's toggle too.
Placeholder override semantics are unchanged. Textured skin mode uses
`mix(shadedRGB, texelRGB, 0.8) * 1.25`, clamped to the valid RGB range. It skips
cel shading's flat texture replacement for the body, retaining detail while
still passing through cel bands and fog. Its texels come from the player's
resolved suit/team palette. Desktop and ES fragment shaders stay synchronized.

## Outlines

`Mods/Render/PlayerOutlines.cs` adds an optional mask draw and edge composite.
It reuses tagged body render items and the scene's existing depth attachment.
The mask keeps material/texture alpha and normal culling, tests against world
depth, and never writes to depth. The composite paints only inside nonempty
mask pixels near an edge. It does not inflate geometry or expand an outline
onto pixels occupied by an occluding wall. Small or distant bodies can become
mostly outline at high thickness; reducing thickness keeps more interior detail.

Outline colors pass through fog. Compositing happens before the preview, HUD
models, and cel outline so those layers retain their normal priority. Outlines
share skin eligibility but are independent of the skin toggle; frozen bodies
and turrets additionally suppress outlines so ice overlays stay visible.

The mask texture, framebuffer, and shader are allocated lazily and reused;
off skips the pass entirely. Resizes and depth-attachment changes update the
target, pooled render items clear their tags, and scene disposal releases the
resources. The composite shader uses one shared algorithm for desktop and ES.
This optional outline has extra rendering cost; skin modes alone still add no
rendering pass.

## Checks

`-brightskinscheck` runs without game assets or GL. It checks eligibility,
cloak transitions, status precedence, material alpha handling, 4096 input
colors against the luminance floor, centralized team colors, fallback, and
preference round trips in a temporary directory, including legacy preference
compatibility, independent outline styles, and bounded thickness. `-uishot`
includes `settings-visibility.png` to inspect the controls after scrolling.

`-brightskinscheckassets` additionally requires the user's extracted files and
checks all seven multiplayer hunters: four resolved suits stay distinct after
normalization, repeat lookups agree, both team palettes are sampled, and invalid
recolors safely fall back. No proprietary data is included in these tests.

The automated checks do not replace visual acceptance on Windows and Android:
check every hunter in normal/alt form, four players sharing a hunter, teams,
turret, damage, freeze, Double Damage, active/passive cloak and fades, death and
respawn, spectators, cutouts, fog and cel shading, and wall occlusion. Use an
isolated build's `launcher.txt` to enable the preference for `-maptest`; the
headless command dispatcher loads launcher preferences too.
`-maptest` assigns alternating valid team indices in team modes, matching
launcher bot matches; FFA retains its existing per-player identities.

### Validation on Windows (2026-09-16)

- Release solution and Windows dedicated-server builds: no warnings or errors.
- Android Release APK build: succeeds with 14 XML documentation warnings in
  existing code. No Android device was connected for an ES gameplay run.
- Both brightskins checks pass, including all seven hunters' real palettes.
- Eight-player `MP3 PROVING GROUND` FFA (fog on, cel off) and team (fog and cel
  on) smoke runs complete with exit 0. Captures show bright body colors and
  orange/green team identity. The team run with the preference disabled has
  matching simulation totals (2885 frames, 7 deaths, 2818 effect particles).
- The harness's affliction sub-probes still report failures: burn in FFA and
  all three in the team run, also present in the corresponding disabled
  controls. These runs are rendering smoke coverage, not full status acceptance.
- The original baseline hit a native shutdown failure; subsequent smoke runs
  used `ALSOFT_DRIVERS=null` and exited normally. Launcher `-uishot` also completes.

The full visual matrix above remains pending, especially Android gameplay,
four simultaneous same-hunter players, spectator/cloak transitions, and every
hunter/status combination. Keep issue #32 open until that acceptance is done.

### Textured skins and outlines validation (2026-09-17)

- Release solution and Windows dedicated-server builds pass without warnings.
  Android Release APK builds with the same 14 existing XML documentation warnings.
- Asset-backed brightskins checks pass, including the new preference, outline
  color, independent toggle, invalid-value, and legacy compatibility cases.
- Eight-player, 26-second Proving Ground captures cover Textured skins with
  team outlines, fog and cel shading, plus unchanged skins with a 6-pixel red
  outline in FFA. Both runs exit normally and exercise six alternate forms.
  Captures retain armor texture detail and leave first-person equipment unchanged.
- The settings visibility capture confirms the new choices and thickness slider
  fit without overlap. The map-thumbnail capture subprocesses fail in this
  isolated build; the 18 launcher screen captures still complete.
- Affliction sub-probe failures remain as described above. These are rendering
  smoke runs, not full status acceptance. Native shutdown uses the null audio
  driver for the same baseline reason as the original checks.
- Android gameplay, the full visual matrix, live outline/cel/resize cycling,
  and a measured GPU cost comparison remain pending. Optional outlines add a
  body-mask pass and a fullscreen composite; they are not claimed to be free.
