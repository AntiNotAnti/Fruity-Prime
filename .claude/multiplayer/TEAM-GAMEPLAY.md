# Team gameplay

`GameState.TeamCount` is configured from the frozen match before room setup.
FFA keeps slot-based scoring identities with `Teams=false`; team modes use
2–4 authoritative team indices. `TeamRules.AreAllies` requires an active team
mode and valid equal indices. Original A/B skins remain available; C/D keep
normal hunter suits and use distinct HUD, radar and objective colors.

`GameStateTeams.UpdateStandings` orders by team result first, then individual
result within the team. Equal team results stay tied; a deterministic team
index ordering only groups the displayed rows. Individual scores cannot move
a losing team above a winning team. Tied results use an overview camera and
explicit result labels. Survival counts distinct surviving teams, preserves
the winning `-1` time sentinel and only the authority ends a network match.

Node/Defender neutral ownership is `NodeDefenseEntity.NoTeam == -1`, separate
from all eight FFA identities. Occupancy, clearing and capture credit cover
all eight slots. Bounty bases are enumerated through `EntityType.FlagBase`;
the former First Hunt bomb lookup hid every base from scoring/HUD callers.
Original Team Bounty map turrets exempt Team A only in two-team matches;
with more teams they treat all teams as possible targets.

## Regression

`-netlobbytest` includes asset-free `TeamGameplayTest` scoring checks: slots
0/7, 1/6, 2/5, 3/4; 2/3/4-way ties; Bounty tie breakers; Survival eliminating
A while B/C/D remain; empty results and FFA slot-seven ranking.

`-maptest "ROOM" -players 8 -seconds 10 -mode MODE -teamprobe -bots` runs a
rendered tour with that exact four-team assignment, then probes initialized
entities before cleanup. It verifies allied/enemy damage, friendly fire,
score aggregation, and mode-specific Survival/Node/Defender/Bounty behavior.
The output reports `TEAMPROBE` check/failure counts and `MAPFAIL` failures.
MapAudit selects the configured 2/3/4-player entity layer, clamping 5–8 to 4.

Use `MP3 PROVING GROUND` for BattleTeams, SurvivalTeams, NodesTeams and
DefenderTeams. BountyTeams needs an original map with its objectives, such as
`AD1 TRANSFER LOCK BT`; small MP maps can have empty Team Bounty layers.
These local checks do not establish Internet behavior, Android rendering,
map route balance, or all-map objective support.
