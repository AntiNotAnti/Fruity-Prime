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
