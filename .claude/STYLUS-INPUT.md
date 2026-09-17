# Desktop stylus and pointer input

Capture the input source, never erase the player's action. `ApplyStylusZone`
adds DS actions after binding resolution; it never clears Shoot or AltAttack.

## Ownership and timing

`PointerSample` carries device type, pointer identity, client-area position,
primary/contact/range state and optional pressure/tilt. `PointerDevice` determines
the DS gesture owner before player bindings run. The existing `Held`, `Pressed`,
`TakePressed`, `MenuHeld` and first-contact `_aimReady` rules remain in StylusZone.
`CapturingPointer` includes placement; `CapturingPrimaryButton` describes only a
contact owned by the DS zone. Placement separately consumes its primary input.

`PointerBindings` computes primary-button edges from the filtered source once per
simulation step. Every LMB-bound action uses that source, including future binds.
Keyboard, right/middle mouse buttons, wheel and additive gamepad actions remain
independent. An entire captured contact produces no gameplay press or release.
Native physical-mouse LMB is an independent source while a pen is in range.

Movement is filtered as one vector per rendered pointer sample and accumulated
until a simulation step consumes it. First-contact movement is excluded before
accumulation, including a touchdown picture with no simulation step. A catch-up
step cannot consume the same movement twice. Source changes, placement, pause,
chat, focus loss and a new contact reset the movement baseline as appropriate.
Normal mouse mode and Android retain the existing snapshot delta path, cached as
one pair per simulation step. Filtering is opt-in through Stylus mode, so fast
high-DPI mouse flicks are unchanged by default.

## Windows and fallback

`WindowsPenInput` observes the existing GLFW HWND using `SetWindowSubclass`.
It reads WM_POINTERDOWN/UPDATE/UP/ENTER/LEAVE with GetPointerType and
GetPointerPenInfo for PT_PEN, retaining identity until contact ends. Coordinates
are converted from screen pixels into the client coordinate space. Pressure and
tilt are recorded when supported, without gameplay mappings.

Mouse messages carrying Windows' promoted-pointer signature do not change the
physical-mouse primary state. All messages still reach GLFW so menus keep normal
mouse promotion. Capture loss, cancellation, focus loss and window destruction
release ownership; the callback remains rooted until teardown. API failures leave
the GLFW path available. No new window or dependency is introduced by gameplay.

Windows Ink must be enabled in a tablet driver for that driver to provide native
pen events. Drivers exposing only mouse input, macOS and Linux use the GLFW
`Unknown` sample. That fallback intentionally cannot distinguish a physical LMB
from a pen tip; keyboard/controller firing remains available during capture.
Future macOS/Linux backends can supply the same normalized sample. Android keeps
its existing per-finger ownership and never attaches the desktop observer.

Native contracts:
- [POINTER_INFO](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-pointer_info)
- [POINTER_PEN_INFO](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-pointer_pen_info)
- [Promoted mouse signature](https://learn.microsoft.com/en-us/windows/win32/tablet/system-events-and-mouse-messages)

## Settings and diagnostics

`stylus_mode` is the master (default false), `pointer_jump_guard` is the independent
reposition filter (default true), and `stylus_zone` retains the DS zone preference.
Old files with only `pointer_jump_guard` use its value for both master and filter;
an explicit `stylus_mode` wins in either file order. Turning the master off keeps
the child settings. Reset to defaults disables the master and zone. Android never
enables the desktop master or zone when importing desktop settings.

Debug logs record contact/capture transitions and firing binding sources. Capture
records include Contact, Region, Held, CapturingPointer, Aiming and jump count.
No coordinates are logged every frame. Vector jump count increments once per
rejected sample and the first rejection is logged.

## Verification

Run `dotnet run --project src/MphRead/MphRead.csproj -c Release -- -pointercheck`.
It needs no game assets, window or tablet. It covers keyboard Shoot/AltAttack,
all primary bindings, normal mouse edges, physical mouse beside a captured pen,
controller contribution, BEAM/MSL/WPN/SEL/ALT actions, sticky ownership, outside
contacts, placement, vector thresholds, high-refresh touchdown, pause/source
changes, promoted message signatures and settings migration/round-trip/reset.
The integration portion runs the real `PlayerEntity.ProcessInput` pass using
synthetic hardware snapshots and real player constructors over an inert scene.

Validated on Windows: 83 assertions; desktop win-x64 and osx-arm64 builds;
linux-x64 dedicated-server build; Android Release APK build (14 existing XML
comment warnings); hidden native GLFW window attach/destroy smoke test; rendered
Controls settings screenshot. The native smoke test validates observer lifecycle,
not pen delivery from an XP-Pen driver. No physical tablet or phone was available.

Hardware acceptance still needs XP-Pen absolute mode with Windows Ink on/off,
Wacom/Surface pen, touchpads, ordinary/high-DPI mouse and controller + stylus.
Critical reproducer: keep the pen down in Aim, drag, hold keyboard Shoot, and
confirm continuous firing with no shot caused by the tip. Also verify BEAM/MSL
selection, WPN cycling, SEL hold-and-release, and physical LMB while using a
native pen. Pressure and tilt need no gameplay verification.
