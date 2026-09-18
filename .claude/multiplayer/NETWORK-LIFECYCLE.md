Protocol-12 updates: [authoritative HUD health, hidden HP, shot identity, lifecycle and claim validation](NETWORK-HEALTH-SHOTS.md). This section supersedes conflicting historical behavior below.

# Network lifecycle (protocol 8)

Integration note: this is the original branch design. The combined build uses
protocol 10 and a larger snapshot budget; see [INTEGRATION.md](../INTEGRATION.md).

The simulation authority owns health, death, spawn, scores and life allocation.
Clients predict hit feedback and nonlethal damage. A remote lethal prediction
stops at 1 HP and clears the engine's forced-death flag. Its hit claim retains
the full original damage. Deterministic local self/environment deaths remain
immediate; prediction and replay preserve authoritative score arrays.

This document supersedes protocol-7 lifecycle and stream-reset descriptions in
the prediction, hit-claim, smoothing and unlagged histories.

## Identity and ordering

Every snapshot, intent, claim and verdict names its match and authority epoch.
A slot has a nonzero ushort `SlotGeneration`, allocated by the session owner on
occupant change, and a nonzero ushort `LifeId`, allocated only by the simulation
on spawn. Zero means unassigned/waiting. Occupant generations survive match
rotation on the server; life counters restart within the new match.

The authority epoch is a ulong, seeded from UTC ticks when the server starts
and incremented on authority handover. This separates restarted processes as
well as authority changes. The server clock must not move backwards across a
restart for an existing client to accept its new epoch; a fresh connection can
join regardless. Epoch changes clear occupants, presentation histories,
predictions, claims and packet ordering, then recover from the new roster and
snapshot. Hosted authority handover sends a seed snapshot under the new epoch.
A client re-announces itself to recover its admission after an epoch change.

Match, life and occupant counters use 16-bit serial-number comparison, skipping
zero. Frame and roster revision ordering use 32-bit serial comparison. These
assume a delayed packet spans less than half the counter space. Reordering never
rebases a stream based on a time or frame-gap threshold. Identities are checked
before advancing the relevant frame/event baseline. Client datagrams must come
from the configured server endpoint; demo injection is explicitly separate.

The lifecycle tracker records Empty, WaitingToSpawn, Alive, Dead and Spectating.
Death is a tombstone for the current life, including across spectator state:
only a newer life can become alive again. Invalid transitions are logged and
ignored. A client cannot allocate a life through the engine's normal Spawn.

## Reset boundaries

`NetPlayerLifecycle.SetOccupant` releases the old slot and clears its input,
score, damage, prediction, claim, smoothing and rewind state. Authoritative spawn
clears those per-life histories and held controls before simulation continues.
`NetPlayerBridge.BeginRemoteLife` resets presentation, prediction and damage
baselines, spawns once, and applies the authority's position, facing and health.
Clients apply a newly received life before composing their next outgoing intent.

Intents and claims must match the current nonzero life. Claims are checked both
at receipt and after their grace window, before any deduplication credit or
health mutation. Verdicts carry the shooter's life; outgoing claims retain the
victim's identity. Projectile entities retain the match, epoch and firing
occupant/life, so an old missile cannot be claimed as a new life's shot.

Rewind samples and smoothing entries carry occupant/life identity. A new life
invalidates the slot's samples before recording its first new position. No
interpolation or rewind uses the previous life's body. Match changes clear
cached gameplay state immediately; room readiness follows the requested match
through the actual load, including a second rotation during loading. Gameplay
is not stamped or published for the new match while still in the previous room.

## Wire layout

Protocol 8 is incompatible with older clients and demos. Hello uses the existing
version-refusal response. All desktop, Android, server and test consumers share
`NetProtocol.cs`. Sizes below exclude the one-byte packet type.

| Record | Bytes | Identity layout |
| --- | ---: | --- |
| MatchState | 103 | existing match at 13; epoch at 95 |
| Roster header / entry | 15 / 23 | match 1, epoch 3, revision 11; entry generation 21 |
| Welcome | 17 | slot 0, client ID 1, match 5, epoch 7, generation 15 |
| Authority grant | 13 | slot 0, match 1, epoch 3, generation 11 |
| MatchEnd | 10 | match 0, epoch 2 |
| Snapshot header | 23 | match 13, epoch 15 |
| PlayerState | 174 | generation 64, life 66, newest damage event 68, history 70 |
| Intent / extended | 88 / 92 | match 74, epoch 76, generation 84, life 86 |
| HitClaim | 49 | match 31, epoch 33, shooter generation/life 41/43, victim 45/47 |
| Verdict header / entry | 15 / 3 | match 1, epoch 3, shooter generation/life 11/13 |

An eight-player snapshot is 1,416 bytes including its type, below the 1,440-byte
transport bound and a standard Ethernet IPv4 UDP payload. Fixed original fields
retain their offsets; mandatory identity trailers precede optional intent data.

Each PlayerState repeats the last four exact damage events. An event is 26
bytes: ushort event ID, victim life, attacker life and generation; byte victim
and attacker slots; ushort damage; byte beam and flags; three direction floats.
The containing state and header identify the victim generation and stream.
Events replay in order at most once within that life. Join/respawn establishes a
baseline instead of playing historical hits. Feedback from an attacker who has
changed identity is not credited to the new occupant. Authoritative health and
scores remain absolute state. More than four missed events can lose feedback;
this bounded redundant history is not reliable delivery of every effect.

## Diagnostics and fault injection

`netdbg` includes transition, spawn/death, stale-life, wrong-generation,
invalid-resurrection, cross-match/authority, old-intent/claim/damage counters.
Lifecycle logs include slot, generation, life, local and authority frames,
health/spawn state and pending prediction/debit/last-authority-health details.
Lethal-held, confirmed and denied counters track the settlement of predictions;
a confirmed hit claim does not itself play a death or award a kill.

`-deathprediction` and `-nodeathprediction` remain accepted for old scripts;
neither enables remote death prediction. Use `-nohitprediction` for the control
without local hit prediction.

```
-netlag 400 -netjitter 80 -netloss .02 -netreorder .05 -netduplicate .02 -netseed 8128
```

`netlag` is added round-trip delay (half in each direction); jitter is an added
uniform per-direction delay. `-netlag 400:80` remains supported. Probabilities
accept fractions, percentages greater than 1, or an explicit percent suffix;
use `1%` for one percent and `1` for certainty. Jitter alone preserves FIFO;
explicit reorder delays selected packets past their successors. Inbound and
outbound queues are bounded and use separate seeded random streams. A seed
reproduces the fault schedule for the same ordered traffic and enqueue times;
live OS scheduling and gameplay are not made deterministic by it.

## Verification

```
dotnet run --project tools/nettest/nettest.csproj -c Release -- --lifecycle
dotnet build src/MphRead/MphRead.csproj -c Release
dotnet build src/MphRead/MphRead.csproj -c Release -p:MphReadServer=true
```

The asset-free regression executable references production code, exercises a
real UDP hosted-server admission/refusal, and checks packet codecs, stale
alive/death, occupant replacement, reordered frames, authority restart, match
rotation, prediction denial/self-death flags and score protection, claims,
projectile identity, exact damage replay, and history boundaries. The seeded
400 ms RTT / 80 ms jitter / 2% loss / 5% reorder / 2% duplicate stream crosses
40 lives over 3,600 snapshots. CI runs this suite alongside platform builds.

These tests do not render or play a real match. A visual Missile/Magmaul duel,
void-death/respawn, death animation/kill-feed counts, and the Android gameplay
smoke test still require extracted game assets and the existing hitrig/netcheck
workflows. Do not describe the synthetic packet stream as a passed visual duel.

The 2026-09-17 run of `--lifecycle` passed 3,687 deterministic assertions with
seed 8128. The companion `--health-shots` run passed 2,967,742 health/input
assertions; both suites are asset-free and are run serially when sharing the
same build output.
