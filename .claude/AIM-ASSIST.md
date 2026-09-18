# Controller camera assistance

`feature/controller-aim-assist` starts at `feature/netcode-health-shot-consistency`
(`37d8fd8`). This is client camera input processing. Protocol 12, damage authority,
hit validation, lag compensation, collision volumes and projectile trajectories
are unchanged. The preview-worker process-exit repair is retained separately.

## Ownership and integration

`AimInputSourceTracker` tracks aiming independently from general prompt activity.
Mouse or touch/pen motion revokes controller assistance immediately. A meaningful
aim-stick deflection must persist for 120 ms to reclaim ownership after pointer
input. Keyboard movement neither claims nor revokes aim ownership.

`PlayerEntity.ApplyGamepadAim` passes controller deltas to `AimAssistWorld` before
the existing HUD and camera rotation functions. Its pure `AimAssist.Apply` core
has no network, collision, weapon-firing or damage API. Gameplay runs at a fixed
60 Hz; core checks also exercise 30 and 120 Hz integration. Aim-stick intent above
0.08 enables rotation; processed movement-stick intent above 0.20 enables half
strength. An untouched controller produces no rotation.

Device/context changes, focus loss, death, room changes, lifecycle changes,
spectating, menus, chat, results and the weapon wheel clear or disable tracking.
No player-facing enable/strength option is added.

## Visible targets and geometry

Candidates are active, spawned, alive opposing players from the local entity
presentation. Spectators, teammates, cloaked players and non-finite candidates
are rejected. No authority history, packet-position lookup, projectile lead or
muzzle-convergence helper is used. Cheap range/cone checks precede visibility
queries. Visibility includes room geometry and collision entities such as doors.
Storage is a bounded stack span; the normal assist path allocates no collections.

Body points derive from each hunter's `PlayerVolumes` and `MaxPickupHeight`.
The head band is the same geometry documented by `HitRig`, expressed using the
actual hunter height rather than its fixed test-fixture constant. Morph forms
use center mass and cannot receive head refinement.

Scores weight angular proximity 60%, retained target 15%, distance 10%, visibility
10% and recent angular motion 5%. Acquisition/release cones are 7/9 degrees, with
a 2.4-degree inner region. A challenger normally needs 1.30 times the incumbent
score; deliberate fast input relaxes retention. Far targets use a smaller cone.

Friction smoothly ranges from 1 to 0.62. Close range reduces assistance; medium
range receives the nominal profile; far range reduces rotation. Opposing input
attenuates both friction and correction independently per axis. Strong opposing
input wins completely. Rotation uses the visible angular error and filtered
angular velocity, with separate horizontal/vertical weights and a degrees-per-
second cap. It never modifies a projectile separately from the camera.

Weapon profiles distinguish tracking, standard, precision, projectile and splash
weapons. Scoped acquisition/release cones are 3.5/4.75 degrees and rotation is
weaker. Projectile and splash profiles do not attract the aim toward a predicted
future position.

Head refinement requires a retained torso for at least 120 ms, separate head LOS,
a head error below 1.5 degrees, an eligible weapon, distance above 5 units, and
upward input or a reticle already closer to the head than the torso. Blend ramps
gradually, is capped at 0.80, and drops immediately when the head is obscured or
the player aims downward. Heads never acquire targets independently.

## Local diagnostics

```
FruityPrime -gamepadassistdebug
FruityPrime -gamepadassistdebug -gamepadassistbaseline
FruityPrime -gamepadassisttelemetry ABSOLUTE_OUTPUT.json
```

The developer overlay shows ownership, device, target, score, distance, body/head
error and visibility, point/blend, friction, rotation strength, angular velocity,
raw/final deltas and correction. The baseline flag suppresses assisted output for
developer comparisons; it is not persisted in player settings.

Telemetry is opt-in, local and aggregate-only. Buckets record input arm, weapon,
distance, shots, observed hit events/damage, time on target, error, friction,
correction, angular motion, assist/head duration and target switches. JSON is
written at process exit, never from the frame loop. Combat hit counters are
authority-side observations; client predictions are not reported as confirmed
hits. Hit events per shot is not a competitive accuracy score: splash/continuous
weapons can generate multiple events and delayed impacts use the last shot's
weapon bucket. No telemetry service or new dependency is introduced.

## Validation

`-gamepadcheck` includes pure assist checks for no input, pointer takeover,
visibility, non-finite inputs, lifecycle resets, retention/challengers, friction,
opposing input, movement intent, head acquisition/refinement/occlusion and
integration-rate consistency. Controller runtime tests cover separate device
calibration, first-frame switching, immutable snapshots, reentrant callbacks,
atomic invalid-profile rejection, corrupt-library preservation, menu hysteresis,
haptic priorities, mapping replacement, layout identity and semantic prompts.
Headless Avalonia checks exercise explicit navigation neighbors, modal focus
containment, stable controller-settings focus and calibration Apply/cancel.

`-gamepadaimworldcheck "MP1 SANCTORUS"` is an optional asset-backed adapter test.
It uses the real headless engine, hunter rig and room collision to verify visible
acquisition, dead/spectator/team rejection and an occluded target inside assist
range. It requires the user's extracted game files; none are included in CI or
packages. The local run passed 9 assertions. The input/UI suite passed 279 checks,
including a 10,000-step zero-allocation check and a concurrent device-switch
regression. Each input frame retains the runtime captured with its state even
when Android publishes a different selected controller during that frame.

Hardware tests and controller-versus-mouse balance/playtesting are explicitly
deferred by the user (automated testing only). The numerical tuning is an initial
profile, not certified competitive balance. Still validate close/mid/far targets,
lateral/vertical motion, clustered enemies, cover transitions, rapid switches,
all weapons and hip/scoped aim with real players before balance claims.
