# Controllers

The shared gameplay path is unchanged: platform device -> normalized snapshot ->
`GamepadInput` -> existing `PlayerControls` -> gameplay/network intent. The
controller work does not change movement physics or the wire protocol.

## Devices and lifecycle

`GamepadManager` owns independent device states and publishes a coherent active
snapshot. Explicit selection in Settings wins; Automatic follows meaningful new
button presses or stick movement above the activity threshold. Resting drift
cannot steal the active controller. Mapped controllers have the default preference
until a controller is used. Input from different controllers is never combined.

Desktop enumerates all 16 GLFW slots, caching names, layouts and trigger calibration
per connection. IDs contain GUID, slot and connection generation. OpenTK lifecycle
events invalidate a reused slot immediately; polling also detects removals. Android
registers an InputDeviceListener in MainActivity and caches profiles by physical
input-device ID. Its key buttons and motion buttons are independent. Hat/stick
motion cannot erase key-held D-pad or trigger buttons. Available motion ranges,
not current nonzero axis values, choose the right stick and trigger axes.

Removal clears the active snapshot immediately. Device changes reset button edges;
held buttons must be released before producing a new gameplay action. The same
barrier protects menu/capture transitions, including menus opened and closed
between simulation steps. Focus loss clears input and stops vibration. Both focus
edges advance the context revision, so background polling cannot consume the
held-button barrier before focus returns. UI routers also track that revision:
Android keeps its launcher alive while hidden, and the Start press that reopens
the pause menu must not immediately act as Back. A fresh Back press resumes via
the pause view's host callback on both platforms.

Controller selection is for this session; disconnected device IDs are never
silently applied to a newly connected device. There is still one local player.

## Menus

`GamepadUiRouter` produces semantic navigation, Accept, Back, tabs and page actions.
The UI host routes these through existing controls and Avalonia focus. Disabled or
hidden controls are skipped. Directional focus scrolls into view. Controllers are
polled throughout the desktop launcher, not just during rebinding. The same router
runs on Android's shared launcher, settings and pause screens.

| Physical position | Menu action |
|---|---|
| D-pad / left stick | Navigate |
| South face | Accept |
| East face / Start | Back |
| LB / RB | Previous / next tab |
| LT / RT | Page up / down |

Navigation repeats after 300 ms, then every 90 ms, accelerating to 55 ms after
1.5 seconds. Accept/Back never repeat. Gameplay bindings do not change menu actions.
Text fields open an in-window controller keyboard on Accept. It supports case,
spaces, numbers, endpoint punctuation, deletion, Done and Cancel. Physical keyboard
and mouse handling stays on its existing path.

Results use the same repeat logic: left/right choose hunter, up/down choose map or
suit, Accept toggles ready, RB casts the map vote, and B/Start opens the menu.
During a match, open the pause menu to Accept or Deny the active map vote;
expired votes disappear and restore focus to Resume.

## Settings and bindings

Settings -> Controller exposes independent inner/outer deadzones, X/Y sensitivity,
X/Y inversion, Linear/Classic/Precision/Dynamic response curves, southpaw, trigger
actuation, controller activity threshold, family labels, vibration and intensity.
Classic remains the squared curve. Dynamic is a 1.5 exponent preset. Defaults keep
both inner deadzones at 0.20 and outer deadzones at zero to preserve existing feel.
Movement is quantized into eight angular sectors after radial processing, with a
shared magnitude threshold. Gameplay still receives digital movement.

Trigger processing is shared: default press 0.60 and release 0.45. The adjustable
press range is 0.05-0.95; release stays 0.15 below press, with a floor of 0.01.
A digital trigger key remains held independently of the analog latch.

Every action has independent Primary and Secondary slots. Left/right selects the
slot and Accept begins capture. The opening press is ignored until released.
During capture, B cancels, Back clears, and Start opens a button picker. The picker
makes every button bindable, including B, Back and Start themselves. Left/right
chooses a button (or unbound), then Accept assigns it. Conflicts offer Swap,
Replace, Keep Both or Cancel; existing bindings are not changed until a choice.
Swap is initially selected, so Accept confirms the reassignment; Cancel remains available. Swapping preserves
the other action's Primary/Secondary slot positions. Capture takes its baseline
from the host's normalized snapshot and reports disconnect/focus changes instead of silently
stopping. Binding rows never initialize GLFW or poll its event loop: the host
owns hardware polling, including while menus are open. Disconnect cancels capture. Presets are Default, Bumper Jumper,
Southpaw and Classic; manual edits set Custom. Preset changes retain focus and refresh all button rows;
calibration is retained. The live test at the top shows the active controller,
mapping source, sticks, trigger bars, pressed buttons and the actions they reach.
Keyboard bindings are independent of controller presets. Using a controller on a
keyboard action row opens its corresponding controller binding. A controller
press during keyboard capture binds that game action without replacing the
keyboard key. Keyboard-only actions explain where the controller sticks are set.

The default right stick click opens a native radial weapon wheel. The aim stick
selects among the six existing special-weapon slots, clockwise from the top, and
release equips the selected available weapon. A neutral stick leaves the current
weapon alone. The controller supplies a vector to the shared slot resolver; it
never moves the OS cursor. Mouse drag and touch/stylus arc paths remain available.

Existing `gamepad_deadzone`, `gamepad_look`, `gamepad_invert_y` and `pad_*` settings
load. Legacy deadzone/look populate both new axes unless explicit new values exist,
regardless of file ordering. Primary/secondary slots round-trip with empty slots
intact, and legacy flag sets are still written. Unknown controls.txt keys survive
saving. New values reject non-finite numbers and invalid enum/button values.

## Weapon shortcuts, profiles and setup

All weapon actions have controller rows: Power Beam, Missile, Volt Driver,
Battlehammer, Imperialist, Judicator, Magmaul, Shock Coil, Omega Cannon and the
existing affinity slot. Last equipped weapon returns to `PreviousWeapon` through
its normal gameplay keybind. It preserves inventory, ammo and existing cheat
rules. New actions start unassigned. Cycle previous weapon remains a separate action.

Choose **Modifier for new bindings** before capturing a slot to create a combination
such as LB + X. This choice affects subsequent edits, not existing bindings. A
modifier used by any saved combination is reserved during gameplay, so its ordinary
action is suppressed even when pressed alone. Hold the modifier first, then press
the action button. Completing a combination suppresses both component actions;
releasing its modifier first cannot leak the still-held action button. Primary and
Secondary each keep their own modifier, and conflicts compare complete combinations.
Menu navigation always uses the fixed physical layout. Capture commands remain B
cancel, Back clear and Start picker; use the picker to bind those buttons themselves.

`GamepadActions` resolves held/pressed action bits once per frame without allocations.
Device, focus, context and binding revisions reset its edge state and close a toggled
wheel. The optional wheel toggle opens on one press and equips on the next; hold is
the default. Six configurable positions swap in place, keeping a valid permutation
shared by selection and icon placement. The adjustable selection threshold controls
how far to push the aim stick. Scoped X/Y multipliers affect controller input only,
on top of the game's existing zoom/FOV scaling; both default to 1.

Named profiles contain only controller option/binding lines. `controller-profiles.json`
in the settings directory stores up to 32 profiles and their automatic assignments.
Save overwrites the same name; import creates a unique name without applying it.
Load applies a saved profile. The profile file field supplies the import/export path.
Writes use a temporary sibling file followed by replacement. Bounded JSON input
rejects unsupported versions and non-controller settings before modifying state.
Keyboard/mouse bindings are excluded. Profile loading is cached at startup; device
activation applies cached settings without disk access. Switching to an unassigned
device restores the prior manual settings. Desktop identity is platform + firmware
GUID, so identical controllers share an assignment; Android uses its descriptor.

Guided calibration measures 2.5 seconds of rest after a release delay, then seven
seconds of full stick/trigger travel. It proposes drift dead zones, outer reach and
trigger min/max; Apply is required. Incomplete or heavily deflected rest samples are
rejected. Digital-only or unsqueezed triggers retain their previous ranges. B, Escape
or Cancel stops calibration without applying it. Save settings or a named profile
after applying calibration to retain it.

The desktop mapping wizard records 20 physical controls using raw samples supplied
only while requested by the GLFW host. It captures buttons, hats, signed stick axes
and analog or digital triggers, waits for release between steps, and aborts on device
or focus changes. Escape/Cancel stops mapping. Apply saves a GUID-specific SDL mapping
in the user mapping file; the next host poll reloads it. Existing environment mappings
retain their documented precedence. Android uses the platform's device mapping and
shares the calibration/profile tools. Neither setup tool polls GLFW from a UI event.

`GamepadEnhancementChecks` covers new weapon keybinds, chord precedence/release order,
swaps, wheel toggling/reordering, calibration ranges, raw mapping generation and profile
import/export/automatic switching. Headless UI checks exercise direct weapon capture,
keyboard preservation, focus after wheel changes and setup cancellation. Physical
controller validation remains separate from these deterministic fixtures.

On 2026-09-17, `FruityPrime -gamepadcheck` passed 235 deterministic checks on the
integration branch, including the headless controller settings flow and direct weapon
capture. This validates normalized input and UI lifecycle behavior; it does not replace
the Mac Xbox Series X/S Bluetooth retest described below.

## Labels and haptics

Presentation is chosen from names, GLFW GUID vendor IDs or Android vendor IDs;
Settings can override it. PlayStation, Nintendo and Xbox labels describe physical
positions without changing normalized gameplay mappings. Prompt source changes
follow meaningful keyboard/mouse, touch or controller activity with a 180 ms
switch cooldown.

`IGamepadHaptics` isolates platform output. Local damage, explosions, successful
weapon fire, charged shots, boost, heavy landings and death emit restrained effects.
Disabled vibration and zero strength stop output. Disconnects, focus loss, menu
opening and process exit stop outstanding effects. No ordinary movement rumble.

Android uses the controller's own vibrator, not the phone's. Windows supports
XInput rumble only when exactly one GLFW XInput device and one native XInput index
can be paired unambiguously. Multiple XInput devices still work for input, but
rumble is unavailable rather than being sent to the wrong controller. GLFW has no
rumble API: Linux/macOS and desktop non-XInput haptics remain unsupported. No SDL3
migration or new native dependency is introduced.

## Mappings and diagnostics

`gamecontrollerdb.txt` and its zlib license ship beside portable desktop builds;
macOS app bundles place both in `Contents/Resources` so code signing seals them
as data. Runtime lookup uses that resource directory, followed by the writable
settings directory; ordinary Windows/Linux and unbundled macOS paths stay portable.
The bundled snapshot is mdqinc/SDL_GameControllerDB commit
`5a12daa568d19344f9b6e9286ef5929833b25c7c`. Update both files together from that
upstream repository when refreshing mappings. Load order is bundled -> settings
directory -> SDL_GAMECONTROLLERCONFIG, so user mappings override bundled ones.
Raw guessing remains the last fallback and is labelled unmapped.

### macOS Xbox Bluetooth firmware compatibility

The Xbox Series X/S report exposed an unsafe fallback: all six-axis devices used
Linux's interleaved layout (right stick 3/4), while the macOS Bluetooth profile
uses right stick 2/3 and RT/LT 4/5. RT therefore became camera input when the
firmware GUID lacked an exact mapping. Device-specific selection now keeps these
axes separate, including the raw fallback if the database cannot be loaded.

For otherwise unmapped Microsoft 045e:0b13/0b20 devices with the known six-axis,
15-or-more-button, one-hat shape, a compatibility mapping is registered for the
actual firmware GUID. Existing GLFW/bundled/user/environment mappings keep
priority. Other products and report shapes are not assigned this compatibility
mapping. The layout follows the macOS entries in the bundled
[SDL_GameControllerDB snapshot](https://github.com/mdqinc/SDL_GameControllerDB/blob/5a12daa568d19344f9b6e9286ef5929833b25c7c/gamecontrollerdb.txt).
GLFW's [Cocoa device enumeration](https://github.com/glfw/glfw/blob/3.4/src/cocoa_joystick.m)
includes the firmware version in its GUID, so an unknown firmware revision can
miss an otherwise equivalent database entry.

`GamepadPlatformChecks` tests raw macOS Bluetooth fixtures through the same reader
used in gameplay: every button, both sticks/triggers, diagonal hats, unfamiliar
firmware, profile guards, all presets and conflict slot preservation. The live
monitor provides a hardware retest: hold RT alone and confirm the RT bar fills,
both stick dots stay centered, and Default reports Fire / alt attack.


```
FruityPrime -gamepad -verbose -seconds 30
FruityPrime -gamepadcheck
FruityPrime -gamepadcheck -shots OUTPUT_DIRECTORY
```

For an installed Mac app, run the executable inside the bundle (no sudo):

```sh
"/Applications/Fruity Prime.app/Contents/MacOS/FruityPrime" -gamepad -verbose -seconds 10
```

Neither diagnostic needs ROM assets. The probe resolves actions from the current
PadBindings, including secondary buttons, and verbose output lists device ID,
family, mapping/active status, capabilities, axes, triggers, buttons and processed
stick values. The deterministic check includes real headless Avalonia controls,
focus/scrolling, text entry, binding capture, persistence, device lifecycle, input
merging, trigger hysteresis, repeat timing, wheel sectors and haptics dispatch.
Lifecycle regressions include background polling followed by focus regain,
focus/menu transitions between ticks, and B/Start on the Android-hosted pause view.

## Validation limits

The user reported Xbox Series X/S Bluetooth failures on macOS using the integration
UI branch. Local Windows builds and synthetic macOS HID fixtures validate the
repair; the actual Mac/controller retest remains pending. No physical controller
or Android device is attached to the development machine. Synthetic checks are
not hardware certification. In particular, test
USB/Bluetooth reconnects, raw mappings, Android motion profiles and actual vibration
on real devices before calling the hardware matrix complete.

Required manual matrix: Windows Xbox USB/Bluetooth, DualSense USB/Bluetooth,
DualShock 4, Switch Pro and generic mapped/unmapped pads; Linux Xbox, DualSense and
generic pads; macOS Xbox Series X/S Bluetooth; Android Xbox/DualSense Bluetooth and USB-C/generic pads; Steam Deck
where available. For each, exercise launcher -> settings/rebinding -> host/join ->
match -> pause/settings -> map vote/results -> launcher -> quit. Disconnect while
holding a trigger or D-pad, reconnect, connect a second pad, switch activity and
repeat with an explicit selection. Confirm keyboard/mouse/touch remain usable.
# Controller runtime and camera assistance

The controller/aim-assist branch adds per-device `GamepadRuntimeConfig` ownership:
options, calibration and bindings are resolved before processing that device's
first sample. `GamepadOptions` and `PadBindings` remain compatibility views of the
active runtime. Manager consumers receive immutable value copies; added/removed/
active callbacks run after the manager lock is released. Explicit selection also
prevents another device from taking over prompt ownership.

Profiles are validated and built before replacement. Invalid or oversized profile
libraries remain untouched on disk and produce a recoverable status. Control
layout identity no longer changes when sensitivity, deadzones, curves, vibration,
glyphs, trigger thresholds or scoped multipliers change. Settings focus restores
by a stable navigation ID, not translated text or row index.

Calibration uses median center offsets, a 99th-percentile rest radius and
2nd/98th-percentile axis limits. Full directional coverage is required. Applying
measurements remains explicit; focus/device changes cancel setup. Centering and
range normalization happen before radial deadzones. The monitor shows raw and
processed positions, deadzone rings, raw/calibrated triggers and actuation marks.
Trigger travel that was not demonstrated retains its previous calibration.

Menu triggers have independent 0.45/0.30 hysteresis. Spectator controllers use
LB/RB for previous/next player, Y for view, Back for scoreboard, Start for the
existing menu action, sticks for free movement/look and LT/RT for descent/ascent.
Replay keeps its existing playback actions. UI footer prompts draw family-aware
geometry and semantic prompts follow rebound action combinations.

Haptics prioritize damage/explosion/death over charge/boost/landing over fire,
with independent cooldowns. Focus loss, menus and active-device changes stop
feedback. SDL override saves replace matching GUID/platform entries atomically;
setup includes a reset action. Auxiliary gyro/touch data has a snapshot foundation
but no backend or gyro-aim capability is advertised.

See [AIM-ASSIST.md](AIM-ASSIST.md) for aiming rules, diagnostics and test limits.
Historical implementation notes follow; the changes above supersede their global
runtime, extrema-only calibration and settings-change preset behavior.
