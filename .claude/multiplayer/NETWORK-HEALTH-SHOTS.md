# Authoritative health and shot consistency (protocol 12)

Work starts from `integration/all-branches` at `913908d`. The temporary HP-fix
branch was not used. Protocol 12 keeps packet sizes unchanged: SessionRules bit
6 hides opponent health, and verdict values 7–10 distinguish geometry, damage
limit, invalid launch-frame and zero-damage refusals. Protocol 11 rejects bit 6, so mixed
peers must fail admission rather than silently ignore the match rule.

## Health presentation

`NetHudHealth.Sample` returns the latest valid, current-generation/current-life
authority state on clients and entity health offline or on the authority. It
does not change prediction or entity state. `DrawOpponent` takes one sample for
both energy meters. Target-player meters and followed-player standard/Pro HUD
health use the same source. Local player health retains its existing behavior.
`NetHitPrediction.HealthFor` is unchanged.

The lobby owner sets **Opponent health: Visible / Hidden** through UpdateMatch.
`NetHudHealth.Visible` applies the synchronized rule to target cards, player and
turret scan meters, and followed-player health in both HUDs. Names, portraits,
teams, scores and hit feedback remain. Free spectators have no personal HP
readout. ReplayCapture already records SessionState in its bootstrap; replay
uses the original rule and grants no caster override. Developer logs remain
explicit diagnostics, not a player overlay.

## Reproductions and repairs

* A same-life intent marked not in play still set Shoot on an alive remote
  copy. ApplyIntent now consumes its history and clears gameplay controls
  without generating a release edge. A separate, life-reset respawn request
  preserves the dead player's Fire meaning. Spectator, ready and transport
  liveness still work. RecordPresses does not record dead gameplay edges.
* TryFireWeapon counted NoSpawn as fired. Every attempted invocation now records
  exactly one outcome; only Spawned increments NetDamage.Fired. The current
  engine's only NoSpawn return for a primary shot is insufficient ammo; its
  full projectile pool replaces the last slot rather than refusing a spawn.
* Fresh/reset damage arrays used attacker/beam zero. They now use NoSlot/NoBeam.
* Production missiles (8.48 s remaining), Magmaul shots (1.50 s) and Judicator
  shots (8.50 s) survived the actual Spawn method but failed CurrentProjectile.
  Projectiles now retain an internal launch key. Validation checks that key,
  match, epoch and occupant, rather than requiring the shooter still be in the
  firing life. Ricochet children inherit the original launch. Stamps are set
  inside Spawn, before immediate collision, including reuse of a live pool slot.
* Old-life flights are resolved by the authority. A client does not manufacture
  a new-life claim for an old projectile; old-life wire claims remain rejected.
  Rescued-flight suppression retains the original launch and victim identity
  across shooter respawn, preventing a second payment. It resets on victim life,
  occupant, room and authority changes. Damage events retain the firing life;
  replay attributes them only to the same slot occupant.
* Same-frame lethal claims traded, but claims a frame apart also traded because
  deaths were tracked after the entire batch. Arbitration now tracks each
  applied death and preserves the claim's launch world in the killing event.
  A known earlier claim finishes its grace before a later world is adjudicated.
  This is bounded arbitration of received evidence, not rollback of a verdict
  when an arbitrarily late packet arrives after its grace window.
  Waiting for earlier evidence is capped at twice the maximum grace window,
  so a stream of older claims cannot indefinitely block a pending verdict.
* A valid geometric claim against spawn protection was reported as applied
  despite doing no damage. It now returns an explicit damage-rule refusal,
  applies no frozen effect and does not prepay the authority's later flight.
  Final verdicts are remembered even if the current outbound packet is full;
  repeat requests do not count as new per-weapon claims or verdicts.

## Diagnostics

`-debuglog` enables shot stages keyed by epoch/match/slot/generation/life/launch
world frame: captured input, accepted intent, attempt outcome, projectile spawn,
collision, prediction, claim, verdict and authority hit. A ricochet or a
multi-projectile shot can share the same logical key. Input frame, InPlayState,
press level/age, authority frame and result are additional trace fields.
Damage publication/replay use victim identity plus damage EventId; the existing
damage wire record does not carry a launch frame, so replay does not invent one.

The weapon table separates attempts, spawns, local predicted hits, authority
hits, predictions, claims, rescues, refusals, damage, headshots and rewind/clamp
statistics. Continuous rows report spawn ticks, nonzero damage ticks, target
acquisition, ammo and predicted drain credit. Per-player log snapshots include
authoritative/entity HP, snapshot frame/age, pending predictions, debit, health
floor, last authority HP, last damage event and attacker.

Position counters measure snapshot restores and transitions into actual intent
fallback, once per slot/simulation frame. Network arrival intervals are stamped
on the transport worker; simulation intervals are measured separately. Playout
snaps are classified as occurring within 250 ms after an observed >50 ms local
frame or without one. This is timing attribution, not proof of a network cause.

A missing verdict expires explicitly as `unanswered`, with sends and reason in
the trace, and releases prediction. Packet loss cannot guarantee one of the
three authoritative outcomes reaches a client; a timeout is not falsely called
an authority refusal. The bounded outbox and existing claim retry policy remain.

## Automated checks

```
dotnet run --project tools/nettest/nettest.csproj -c Release -- --health-shots
dotnet run --project tools/nettest/nettest.csproj -c Release -- --lifecycle
dotnet run --project tools/continuous-phase-check/continuous-phase-check.csproj -c Release
FruityPrime.exe -netcombatcheck "MP1 SANCTORUS"
```

The first three need no game assets. The fourth uses locally extracted files
through paths.txt and the real headless engine; it never bundles those files.
The health/shot suite is included in Linux CI.

| Regression | Production path |
|---|---|
| OpponentHudUsesAuthorityHealth, OpponentHudCanHideHealth | snapshot/session packet parsing and HUD helper |
| AuthoritativeHealRaisesOpponentHud | actual prediction, Shock Coil debit, 30→80 authority heal |
| PredictionReconcileDoesNotFakeHeal | real claim refusal and unchanged authority HP |
| RespawnClearsPredictedHealth | lifecycle change resets debit/floor |
| DeadHeldFireDoesNotSpawnGhostShot | input fault matrix and actual engine, eight weapons |
| OldLifeShootPressIsRejected, RecoveredShootPressCannotCrossLife | lifecycle admission and ApplyIntent |
| DuplicateIntentDoesNotDuplicateShot, ReorderedIntentDoesNotDuplicateShot | admission ordering and redundant edges |
| FiredCounterRequiresActualSpawn | all diagnostic outcomes plus actual empty-ammo spawn refusal |
| DamageResetUsesNoAttackerSentinel | full and slot reset |
| ContinuousPhaseAgreesAcrossPeers | independent owner/authority/observer phase clocks and shared damage/ammo cadence |
| ProjectileLifecycleAcrossShooterDeath/Respawn | production spawn, real player Spawn, post-respawn damage, forged stamp and duplicate-payment checks |
| MutualKillOrdering | actual claim damage, ties and ±1 world frame, both arrival orders and 0/1/8-frame arrival gaps |
| InvalidHitClaimIsRefused | authority history and geometric gate at 0.5/1/1.5/2/2.5/4 units |

The input matrix has 6,912 weapon/profile combinations, with two independently
faulted delivery streams per combination: RTT 0/50/150/250/320/400 ms, jitter
0/20/40/80 ms, loss 0/1/2/5%, duplication 0/1/3%, reorder 0/1/3%, eight weapons.
It verifies production packet/input/lifecycle invariants, not 6,912 physics or
Internet sessions. Actual engine checks and process-level UDP runs are separate.

`tools/hitrig/run-windows.ps1` runs a staged runtime against private loopback
servers and two real clients. `-Suite smoke` exercises eight weapons plus duel
at baseline and severe faults. `-Suite press-age` A/B tests 150/250/320/400 ms,
0/40/80 ms jitter and 0/1/2/5% loss. Each profile and all raw reports are retained.
The feature-tour RESULT is not a hit-rig acceptance result: stationary weapon
rigs deliberately do not perform the tour's jumps, morphs and other actions.
`Completed` means both peers joined and emitted reports, not that their hit
agreement passed. Inspect hit counts, headshot agreement, claims and frame timing.

## Validation status

On 2026-09-17 the deterministic suites passed 2,967,742 health/input assertions,
3,687 lifecycle assertions, eight continuous-clock checks, and 93 actual-engine
combat assertions. The lobby suite passed 2,700 assertions, replay format passed
534, and replay timing/camera checks passed. Asset and map-package guards passed.
The engine suite also tests a stream of earlier claims against the arbitration
deadline and verifies duplicate requests do not inflate outcome counters.

[Latest CI run](https://github.com/AntiNotAnti/Fruity-Prime/actions/runs/35291447143)
passed all 13 jobs: Windows client/server, Apple Silicon and Intel Mac packages,
Linux client/server/ARM64 server, Android, lobby and controller/UI checks. Mac
packages passed the existing signing, executable, startup and compatibility
checks; this is not a manual Mac gameplay test. The local Android SDK was absent,
so Android validation used CI.

### Local process measurements

The full press-age matrix completed 96 separate server/two-client sessions,
20 simulation seconds per client, seed 8128. The 18 final eight-weapon/duel runs
completed at 30 seconds each. Severe faults mean 400 ms added RTT, up to 80 ms
jitter each way, 5% loss, 3% duplication and 3% reordering. The runner's reports
and numerical summaries are retained locally; compact source-controlled evidence
is in [health-shot-results](../testing/health-shot-results/).

| Press-age arm (48 runs each) | Off | On |
|---|---:|---:|
| Local predicted hits / claims | 88 | 86 |
| Authority damage events | 98 | 104 |
| Rescued claims | 5 | 2 |
| Already-resolved verdicts | 32 | 35 |
| Victim-down verdicts | 41 | 38 |
| Geometric/other refusals | 0 | 0 |
| Unanswered at expiration | 4 | 8 |
| Claim resends | 27 | 35 |
| Predicted headshots | 55 | 49 |
| Agreed / downgraded headshots | 24 / 13 | 28 / 10 |
| Rewinds sampled / clamped | 355 / 0 | 351 / 1 |
| Mean rewind (frames) | 24.19 | 25.03 |
| Repeated authority Imperialist spawn keys | 0 | 0 |

These are short runs with unequal shot exposure and imperfect prediction pairing.
They do not establish a regression-free improvement. **PressAgeEnabled remains
false.** The A/B binaries preceded the later ricochet, zero-damage refusal and
diagnostic refinements; press-age/input/rewind behavior was unchanged. The final
weapon sessions include those repairs. Numerical differences across these two
experiments must not be attributed solely to press age.

In the Samus Shock Coil runs, owner/authority ammo cadence agreed on all 1,420
baseline and 1,577 severe-fault common logical phases. Damage agreed on 1,412
and 1,538, respectively (99.4% and 97.5%). Including the observer, both quantities
agreed on 1,360/1,386 and 1,513/1,555 common phases. The logical phase, not the
acknowledged target-world frame, identifies a continuous firing tick. Comparing
only acknowledged world frames incorrectly conflates acquisition timing and
clock alignment. The remaining damage mismatch is **not claimed fixed**: phase
agreement alone does not prove equal target acquisition, ramp damage or contact.
The original 16-unit volley range could leave Sylux firing without contact; the
runner now holds Shock Coil at nine units so the drain scenario exercises hits.
Two subsequent 45-second Sylux sessions at the closer range produced 221 and 93
local predicted hits, with 94 and 89 health drained ahead of authority. Ammo
agreed on all 2,195 and 2,222 common phases; damage agreed on 2,173 and 2,216.
The severe run still expired 20 claims and recorded 92/110 predicted/authority
health removed. Those are measured limits, not a claim of perfect reconciliation.
The results CSV retains the earlier no-contact Sylux arm as a failed exposure
check; its zero damage is not evidence that drain synchronization passed.

All eight weapons produced authority hits in the final baseline and severe runs.
The input matrix and actual-engine tests verify the repaired not-in-play and
old-life trigger cases. Full three-process one-to-one fire-event equivalence is
not established by aggregate counts or repeated world-frame keys; continuous
and multi-projectile weapons can legitimately share those keys. No universal
zero-ghost-hit claim is made. Claims still expire under severe loss, and the
continuous-damage mismatch remains an explicit acceptance limitation.

LAN, Pi, Japan and other real-line checks were replaced by local injected faults
at the user's request. No Internet-play acceptance claim is made from loopback.
