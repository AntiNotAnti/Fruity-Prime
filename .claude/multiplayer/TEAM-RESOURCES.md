# Team layouts and world resources

For the combined lifecycle/lobby wire format, packet-size caveat and current
verification scope, see [INTEGRATION.md](../INTEGRATION.md).

The persistent lobby's authoritative definition resolves exact team capacities
and a `MatchWorldProfile` before the loading barrier. Exact formats use total
capacity; flexible FFA/Auto use the server's configured player limit. The
profile never follows a client's observed roster size. The entity layer is
2, 3 or 4 players, with all larger configurations using the four-player layer.
Low/Standard/High resource profiles correspond to 2/3–4/5–8 players.

`Mods/Multiplayer/MapResourceRules.cs` is the map-specific resource table. It
references health entity IDs in the player's own extracted map files; no
cartridge assets or copied position tables ship in source. High population
adds authored health locations from other layers only when the selected layer
does not already contain the ID or health within one world unit. Existing
entity IDs cannot be replaced. Parent-dependent or disabled source entities
are excluded. High health respawns are capped at ten seconds. Maps absent from
the table, including custom maps, retain their authored resources.

Transfer Lock BT's Battle layers have no health. Low/Standard profiles borrow
that map's corresponding two/four-player Bounty health locations. This is a
separate correction from high-population supplementation.

## Authority

Health still uses ordinary `ItemSpawnEntity` and `ItemInstanceEntity`. The
authority owns collection, cooldown and respawn. Protocol 9 snapshots append
the individual/team clocks and a bounded health-spawner state section:
match ID, count, and entity ID/availability/active/cooldown/spawn count per entry.
Every snapshot repeats full state, so packet loss, reconnect and late joining
can recover without a second control channel. Old-match, malformed, duplicate-ID
and out-of-order state is rejected before it can alter the current world.

Replicas construct health from the same frozen map profile and adopt its
availability from snapshots. They do not independently consume or respawn
spawner-owned health. Ownerless drops retain their existing behavior. All eight
individual/team clocks are synchronized, including Defender and Survival.
At most 56 health spawners fit with eight players and clocks under the existing
1024-byte datagram limit; an oversized custom map fails loading explicitly.

## Repeatable validation

Run commands beside a valid `paths.txt` with the user's extracted game assets:

```text
dotnet FruityPrime.dll -netlobbytest
dotnet FruityPrime.dll -resourceaudit -noupdate
dotnet FruityPrime.dll -healthsimtest "MP1 SANCTORUS" -noupdate
dotnet FruityPrime.dll -maptest "MP3 PROVING GROUND" -mode BattleTeams -players 8 -seconds 10 -teamprobe -noupdate
dotnet FruityPrime.dll -lobbyshot OUTPUT_DIRECTORY
```

`-resourceaudit` checks the ten required Battle layouts (including three-player
FFA) plus eight objective-mode scenarios on every available multiplayer map.
It prints selected layer/profile, health counts/units, respawn intervals,
greatest nearest-health distances from spawns/objectives, objective count, and
a deterministic health fingerprint. It rejects duplicate IDs, overlapping
supplemental health, and inconsistent reconstruction. Distances are straight
lines, not navigation paths. Missing native objective layers are reported;
adding teams does not create map objectives.

`-healthsimtest ROOM` loads an eight-player high-profile authority, consumes a
real health pickup, advances until it respawns, and loads a fresh replica. The
replica must agree on spawners, hide consumed health, display its authoritative
respawn, and prevent client-side pickup of it. This exercises real entities and
snapshot encoding/decoding; it does not simulate UDP loss itself.

`-teamprobe` uses A={0,7}, B={1,6}, C={2,5}, D={3,4} in a rendered map run.
It checks actual allied/enemy damage and friendly fire, scoring/standings,
Survival elimination, upper-slot Nodes capture, Defender contention, and
Bounty pickup/delivery when the map contains those objectives.

## Acceptance recorded in this branch

The health lifecycle probe passed all 30 locally available maps (27 cartridge
maps and three custom maps). The resource audit passed deterministic construction and duplicate checks for
540 map/layout combinations. Of these, 12 original layers have no player spawns
and 104 objective-mode combinations lack native objectives; those combinations
are not evidence of playable maps. The two-client local
dedicated test passed on TEST ARENA, including a client with 100 ms added RTT
and 20 ms jitter; both reported the same scores at the shared 30-second sample.
These checks establish metadata determinism and simulation behavior, not
competitive balance or Internet behavior.

The four-team rendered probes passed Battle, Survival, Nodes and Defender on
Proving Ground, plus Bounty pickup/delivery on Transfer Lock BT. The lobby
regression passed 2,604 assertions and 18 lobby captures covered six layouts
at three sizes. Desktop and Windows dedicated-server builds passed without
warnings; the Android Debug APK built with 14 existing XML documentation
warnings and no errors.

The remaining acceptance work is human playtesting of health routes, safe
loops and navigation reachability, plus rendered Android and Internet sessions.
Android compilation is checked separately; a successful APK build does not
prove phone rendering, controls or multiple-round network behavior.

## Health quantities from the asset audit

Counts below include the profile corrections, not dropped health.

| Map | FFA 2 | FFA 4 | FFA 8 | 2v2 |
|---|---:|---:|---:|---:|
| AD1 TRANSFER LOCK BT | 3 | 7 | 10 | 7 |
| AD1 TRANSFER LOCK DM | 3 | 3 | 5 | 3 |
| AD2 ALINOS PERCH | 3 | 3 | 7 | 3 |
| AD2 MAGMA VENTS | 5 | 6 | 8 | 6 |
| CTF1 FAULT LINE - EXPANDED | 3 | 7 | 8 | 7 |
| CTF1_FAULT LINE | 2 | 5 | 5 | 5 |
| DUST2 | 7 | 7 | 7 | 7 |
| E3 FIRST HUNT | 3 | 4 | 4 | 4 |
| Gorea Prison | 3 | 3 | 3 | 3 |
| MP1 SANCTORUS | 2 | 4 | 4 | 4 |
| MP10 OVERLOAD | 2 | 4 | 5 | 4 |
| MP11 BREAKTHROUGH | 2 | 4 | 4 | 4 |
| MP12 SIC TRANSIT | 2 | 3 | 3 | 3 |
| MP13 ACCELERATOR | 7 | 7 | 8 | 7 |
| MP14 OUTER REACH | 2 | 3 | 3 | 3 |
| MP2 HARVESTER | 2 | 4 | 8 | 4 |
| MP3 PROVING GROUND | 4 | 4 | 4 | 4 |
| MP4 HIGHGROUND | 3 | 5 | 6 | 5 |
| MP4 HIGHGROUND - EXPANDED | 4 | 4 | 9 | 6 |
| MP5 FUEL SLUICE | 3 | 4 | 6 | 4 |
| MP6 HEADSHOT | 6 | 6 | 12 | 6 |
| MP7 PROCESSOR CORE | 2 | 3 | 5 | 3 |
| MP8 FIRE CONTROL | 3 | 4 | 5 | 4 |
| MP9 CRYOCHASM | 3 | 5 | 6 | 5 |
| TEST ARENA | 2 | 2 | 2 | 2 |
| TEST PADS | 2 | 2 | 2 | 2 |
| UNIT 3 VESPER STARPORT | 3 | 3 | 8 | 3 |
| UNIT 4 ARCTERRA BASE | 2 | 4 | 4 | 4 |
| UNIT1 ALINOS LANDFALL | 6 | 6 | 6 | 6 |
| UNIT2 LANDING BAY | 3 | 4 | 6 | 4 |
