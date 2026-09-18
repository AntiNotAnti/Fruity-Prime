# Integration UI and graphics acceptance

Implementation branch: `feature/integration-ui-graphics`, based on
`integration/all-branches` at `460a7ff`. Validation dates: 2026-09-16–17.

## Implemented

- Shared player-facing terminology and semantic copy checks; replay CLI aliases
  retain compatibility. Replay documentation is now NETWORK-REPLAYS.md.
- Shared focus/disabled treatment, wrapped tabs and eight Settings sections.
- Replay metadata/actions/confirmation, four playback pages and a persistent
  timeline. Desktop replay import uses the existing in-window file browser.
- Team-grouped lobby, host badge, readiness count, start-block reasons and
  confirmed player administration. Browser mode/state columns, sorting, filter,
  favorites, recent servers and explicit direct connection.
- Map Studio command groups, dirty title, inspector tabs and filtered Problems;
  four-stage setup with failure details and preview retry.
- Pixel through anisotropic filtering, mipmaps, FXAA, sharp sub-native scaling,
  50–200% render-scale stops for world rendering, graphics presets and optional color.
- HUD/UI/text scaling, safe zone, opacity, crosshair color/outline, high contrast,
  reduced whiteout/shake and alternative four-team palettes.

See [graphics quality](../render/GRAPHICS-QUALITY.md) for architecture, limits and
the later bloom/AO/lighting investigation. No protocol or replay-format rewrite,
second window, replacement renderer, proprietary assets or new NuGet dependency.

## Automated results

Host: Windows, NVIDIA RTX 5080, OpenGL 4.6, driver 610.88, .NET 9.0.318.

| Check | Result |
|---|---|
| Desktop Release | Build passes, zero warnings/errors |
| Dedicated server Release | Build passes, zero warnings/errors |
| Android Release | Build passes, 14 existing XML documentation warnings |
| UI copy | Zero regressions; parser positive/negative controls pass |
| UI matrix | 152 captures: 120 launcher/editor/settings/playback + 32 lobbies; zero layout/focus failures |
| Controller | 133 deterministic checks, including draft preset behavior |
| Pointer | 83 assertions |
| Replay format | 534 checks: v2/v3, recovery, CRC, metadata, malformed archives |
| Replay controls/camera | Timing/rates/pause/step, persistence/interpolation/corruption checks pass |
| Persistent lobby | 2,693 assertions, two rounds and host migration |
| Network lifecycle | 3,687 assertions, deterministic seed 8128 |
| Map pipeline | 126 checks |
| Frame timing and bright skins | All existing cases pass |
| ROM browser, extraction child and portable paths | Existing checks pass |
| Graphics/resource checks | Desktop/ES shaders, mipmaps, sampler states, pixel effects, HUD transform and repeated teardown pass |

The matrix covers 1280x720, 960x600, 800x400 and 1920x1080 through the desktop
surface scale. Synthetic populated browser/replay rows and a Map Studio project
avoid needing game assets for those layouts. Large text/UI scale and high
contrast have additional Settings captures. Focus checks include viewport and
scroll clipping; they do not prove every possible text overlap or pixel baseline.
Representative screenshots were visually reviewed. CI runs copy, controller and
the headless matrix, retaining its screenshots as an artifact.

### Rendered respawn checks

Debug renderer, Proving Ground, normal process teardown, all 14 poisoned-state
comparisons restored exact RGB output including cel execution in each run.

| Run | Deaths/respawns | Rendered frames | Checked backbuffers | Resize/scale/cel transitions | Wall time |
|---|---:|---:|---:|---|---:|
| 9 cycles | 10 | 4,935 | 3,106 | 18 / 18 / 18 | 11.9 s |
| 100 cycles | 111 | 51,780 | 31,872 | 200 / 200 / 200 | 80.4 s |
| 500 cycles | 556 | 257,880 | 158,317 | 1,000 / 1,000 / 1,000 | 385.2 s |
| 9 cycles with `-quality` | 10 | 4,935 | 3,106 | 18 / 27 / 18 | 15.9 s |

Every run had zero unexpected full-black frames, invariant failures and GL
errors. `-quality` includes 200% allocation transitions and enabled filtering,
FXAA, sharpening, color and accessibility preferences. Wall times are diagnostic
throughput, not controlled gameplay benchmarks; the quality smoke overlapped
other work. Windows logs live in ignored `artifacts/ui-enhancement/`; matrix
images are beside the desktop DLL under `artifacts/ui-enhancement/matrix/`.

## Remaining acceptance gates

### Highlight and controller integration (2026-09-17)

Merged player highlights through `384034b` and controller support through
`b481299`. The integration keeps its desktop ROM browser, extraction safeguards,
Mac user-data paths, and renderer teardown fixes. Visibility controls live under
HUD and accessibility; controller capture from a keyboard binding switches to
the separate Controller page. CI runs controller and UI checks on Windows and
both Mac architectures with distinct screenshot artifacts.

Local Release builds pass without warnings. The 177 controller checks, highlight
policy/preferences, frame timing, graphics resource checks, UI copy, ROM browser
and extraction regressions pass. The 152-layout matrix has zero failures; the
20-screen capture run also passes, and its visibility page was visually checked.
These checks do not certify physical controllers or Mac GPU rendering.

P0 cross-platform acceptance is **not complete**. This host has no Linux/Mesa/
XWayland environment. Issue #34 must remain open until its 9-, 100- and
500-cycle sequence passes there. Windows/NVIDIA results cannot substitute.

The following also remain manual/platform gates: Linux/X11/XWayland startup,
borderless fullscreen/alt-tab, physical controller hot-plug, real replay → menu →
replay, Map Studio playtest → editor, and visual four-team color checks across
HUD/radar/scoreboard/objectives/models on each target. The automated lobby test
does cover repeated lobby/match rounds; it is not a rendered OS-window cycle.
Android was built; neither device rendering nor touch behavior was exercised.

## Reproduction

```text
dotnet run --project tools/ui-copy-check/ui-copy-check.csproj -c Release
dotnet run --project src/MphRead/MphRead.csproj -c Release -- -gamepadcheck
dotnet run --project src/MphRead/MphRead.csproj -c Release -- -uimatrix OUTPUT
dotnet run --project tools/render-resource-check/render-resource-check.csproj -c Release
FruityPrime -respawnrendercheck "MP3 PROVING GROUND" -cycles 9 -timeout 180
FruityPrime -respawnrendercheck "MP3 PROVING GROUND" -cycles 100 -timeout 1800
FruityPrime -respawnrendercheck "MP3 PROVING GROUND" -cycles 500 -timeout 3600
FruityPrime -respawnrendercheck "MP3 PROVING GROUND" -cycles 9 -quality -timeout 180
```

Rendering requires extracted game assets and `paths.txt` beside the executable.
Use the Mesa compatibility-profile environment from RENDER-STABILITY.md on
Linux; do not count a partial, unsupported or nonzero-exit run as acceptance.
