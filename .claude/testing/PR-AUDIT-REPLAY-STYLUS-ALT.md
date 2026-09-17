# Replay, stylus and alt-form audit — 2026-09-16

## Scope and isolation

Reviewed source heads:

- replay: `e049c50aa2491fa0ad6ffab9056a8240374dfe9c`
- stylus: `0cf92c00096f47d1485cb0195af32d10e26c6003`
- alt form: `656ad4d777cf9dcc2c43ea0c2f0f7e01ab2c1476`
- combined base: `3f52c969e54a99ea67c135dfb4de0e02578adb5b`

Every checkout created by this audit is private and rooted under
`C:/Users/Jarrett/Documents/Development/`:

| Directory | Private branch |
| --- | --- |
| `Fruity-Prime-audit-alt-0917` | `codex/pr-audit-alt-0917` |
| `Fruity-Prime-audit-replay-0917` | `codex/pr-audit-replay-0917` |
| `Fruity-Prime-audit-stylus-0917` | `codex/pr-audit-stylus-0917` |
| `Fruity-Prime-audit-replay-combined-0917` | `codex/pr-audit-replay-combined-0917` |

Existing checkouts and source branch pointers were not changed. Nothing was
pushed, merged to master, or posted externally. The live integration branch
advanced independently during this audit; these results describe the pinned
combined base plus the four fixes below, not those concurrent changes.

## Confirmed findings and fixes

### P1: Spire flick attack misses network capture

`NetHooks.AfterInput` records press history before `UpdateScene` runs
`ProcessAlt`. The old implementation generated Spire's one-shot AltAttack in
`ProcessAlt`; the following hardware input pass cleared it before it could be
recorded. The old test called gesture consumption before press capture, hiding
the real ordering failure. A new regression using the actual static input pass
failed both the outgoing press and local pre-simulation edge checks before repair.

Spire now prepares the edge at the end of binding/stylus resolution. Samus keeps
its aimed boost consumption during simulation. No protocol or network hook change.

- Source-head fix: `3789a4a5eda6e5632c5db80f276c1b6f0b600701`
- Combined equivalent: `db229b3aa2c2468b8a8b09a5ac017fde142323ac`
- Files: `src/MphRead/Entities/Players/PlayerInput.cs`,
  `src/MphRead/Mods/Input/PlayerEntityMouseFlick.cs`,
  `src/MphRead/Testing/TestAltForm.cs`,
  `.claude/multiplayer/NETWORK-DIAGNOSTICS.md`.
- Combined resolution: retain the combined form-reconciliation documentation;
  add only Spire input-timing documentation. Production changes apply unchanged.

### P1: combined snapshots make replay recording throw

Replay bootstrap validation required snapshots to end after player records.
Protocol 10 also requires team clocks and health-spawner state. A regression
fed a valid snapshot through the actual session receiver, captured the production
bootstrap, then constructed the replay writer: this threw
`InvalidDataException: Invalid bootstrap packet`. The existing format fixture
had no snapshot and missed the failure.

Validation now uses the production clock and health-tail validators, checks
unique/in-range player slots, and accepts the maximum eight-player/56-spawner
snapshot. Tests cover missing/truncated tails, nonfinite clocks, excessive health
counts, invalid health flags and invalid/duplicate slots. A capture/write/read/
playback round trip verifies restoration through normal lifecycle handlers.

- Combined-only fix: `d7920bdc341f21e6194cab30710125393246b8a5`
- Files: `src/MphRead/Mods/Network/ReplayFormatV3.cs`,
  `src/MphRead/Mods/Network/ReplayFormatCheck.cs`,
  `.claude/multiplayer/NETWORK-DEMOS.md`.
- Not applicable to the standalone replay wire layout.

### P1: replay cannot leave a recorded load barrier

The frozen gameplay path returned before pumping replay packets. A replay
starting in the persistent lobby's `Starting` phase could never read the later
`InMatch` packet. A synthetic file exercised the real private host step twice:
the pre-fix replay remained at frame zero and frozen. The fixed replay reaches
frame one/InMatch while the scene simulation counter stays zero.

- Source-head fix: `b9f619d3c3585258bd7056791d80b783ff049568`
- Combined equivalent: `69dab6eff5dc0fb89d4240309d530bc60d7f9102`
- Files: `src/MphRead/Renderer.cs`,
  `src/MphRead/Mods/Network/ReplayFormatCheck.cs`,
  `.claude/multiplayer/NETWORK-DEMOS.md`.
- Combined resolution: preserve the snapshot regression and give the synthetic
  session its required authority epoch and world profile. Runtime fix unchanged.

### P1: replay inherits stale controller context

The replay frame path bypassed the live input pass that refreshes controller
context. A previous paused game could leave `Current=Menu`, disabling stick look
and Start throughout replay. The regression reproduced the stale context before
repair. Replay now resolves context before sampling input, including while
paused, and excludes menu/text input from controller replay shortcuts. Tests
also cover menu suppression, held-Start suppression, and fresh Start after release.

- Combined-only fix: `259ffa5ef4324e4ff6f009f542c1985f4fb9a312`
- Files: `src/MphRead/Mods/Replay/ReplayInput.cs`, `src/MphRead/Renderer.cs`,
  `src/MphRead/Mods/Network/ReplayControlCheck.cs`,
  `.claude/multiplayer/NETWORK-DEMOS.md`.
- Depends on controller-support APIs absent from the standalone replay head.

## Application

For source branch updates use the two source-head commits separately. For the
combined tree use `d7920bd`, `db229b3`, `69dab6e`, `259ffa5` in that order.
Do not also apply the source equivalents to a tree receiving the combined fixes.
No production form helper or general network lifecycle code was changed.

## Verification

| Tree | Checks/builds |
| --- | --- |
| Standalone stylus, unchanged | Desktop Release; `-pointercheck`: 83 |
| Standalone alt fix | Desktop and win-x64 server Release, zero warnings/errors; `-altformcheck`: 106 on both; Android Debug, 14 documentation warnings, zero errors |
| Standalone replay fix | Desktop and win-x64 server Release, zero warnings/errors; `-replayformatcheck`: 521 on both; server `-replaycontrolcheck` passes; Android Debug, 14 documentation warnings, zero errors |
| Combined fixes | Desktop and win-x64 server Release, zero warnings/errors; Android Debug, 14 documentation warnings, zero errors |
| Combined deterministic suites | `-replayformatcheck`: 534; `-altformcheck`: 512; `-pointercheck`: 83; `-gamepadcheck`: 116; `-replaycontrolcheck` and `-frametimingcheck` pass; `tools/formtest`: 606 |

The combined server also passes replay format/control and alt-form checks.
Builds used .NET SDK 9.0.318, the supplied Android SDK and JDK 17 paths.
No game assets or recordings were committed.

## Independent form-helper review and limitations

Reviewed `1a2085f887c0ac36f21170201564ca24fd7ff42d` against
`fix/form-reconciliation`, including the complete helper and combined call sites.
No concern found: `_transitionActive` ends on an observed settled frame and
restarts a same-direction animation's timeout, while `_transitionSeen` retains
the separate latency-grace history. Reset still clears both. The fix is not
duplicated here; the main audit owns its integration and 699-check verification.
Our pinned combined tree retains the original 606-check helper.

No additional evidenced stylus defect was found. Source review and synthetic
checks do not certify Windows Ink/tablet delivery or Android/controller hardware.
No asset-backed replay, attack animation/damage, full-world clip equivalence,
rendered gameplay or Internet multiplayer validation was performed. The replay
branch's documented checkpoint and fidelity limitations remain. This is not an
unqualified assertion that every source branch is independently safe to merge;
the published form/lifecycle work remains part of the main audit's dependency plan.
