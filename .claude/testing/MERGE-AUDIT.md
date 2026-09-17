# Merge-readiness audit (2026-09-16)

Scope: the ten open upstream PRs from AntiNotAnti at audit start, plus local
branches that had no PR. Target is `liveteklol/Fruity-Prime:master` at `547ffba`.
The separate fork PR `AntiNotAnti/Fruity-Prime#1` is excluded. The actively
developing `fix/macos-native-packaging` branch became upstream PR #46 during
the audit and remains in scope. No PR or main/master branch was merged.

## Published PR corrections

These focused commits were fast-forward pushed to the existing fork branches,
updating their upstream PRs without rewriting history. Original local source
worktrees were not changed; their branches may now be behind their remote.

| PR | Fix commit | Evidence-backed correction |
| --- | --- | --- |
| #35 | `1ba7884` | Preserve managed entry point when setup is launched through dotnet; reject failed extraction even if old paths remain valid |
| #36 | None | No additional verified defect found; deterministic Lockjaw/frame checks pass |
| #38 | `f44d2e9` | Preserve legal equals signs in extracted-file paths and server copies |
| #39 | `29ea907` | Distinguish local builds from real 1.0.0; validate canonical bounded version components before creating a tag |
| #40 | None | No additional verified defect found; simulation-only Spire collision source probe passes |
| #41 | `1a2085f` | Give each new form animation its own timeout, retaining latency grace separately |
| #42 | None | No additional verified defect found; eight firing-phase regression cases pass |
| #43 | `38adfe5` | Release cel framebuffer and attachment reference when unloading a scene |
| #44 | `50b09d7` | Preserve bright-skin transparency when the Debug viewer disables textures |
| #45 | `742b856` | Suppress held buttons across focus/menu transitions; allow Android B/Start to resume pause |
| #46 (new during audit) | `ce86011` | Bind packaged OpenAL, permit displayless diagnostics, and test signed native bundle/resources |

New regression tests failed before their respective fixes and passed afterward.
Some fixes repair pre-existing callers or cleanup paths exercised by the PR,
not defects introduced by the PR itself.

Release caveat: already-installed clients retain the old 1.0.0 rejection.
Ship the parser fix in another supported version first, or require manual
installation of 1.0.0. This audit did not create tags or publish a release.

## Combined merge validation

`audit/upstream-pr-merge-check` (`b88f749`) in the sibling `Fruity-Prime-upstream-merge-check`
worktree contains only the original ten PRs and their audit fixes, plus a local
CRLF-tolerance correction to the asset guard. It does not contain unpublished
network/replay/stylus features or macOS PR #46.

Order used: #35, #36, #38, #39, #40, #41, #42, #43, #44, #45, then audit fixes.
This required real conflict resolutions; the individual PRs being mergeable
against the same base does not mean Git can merge them sequentially unaided.

- `ModEntry.cs`: retain every diagnostic dispatcher before/after setup as
  appropriate, plus both read-only Spire/form update-cleanup exemptions.
- `PlayerEntity.Spawn`: retain firing-phase reset, respawn visual reset and
  render diagnostics.
- `PlayerDraw.cs`: retain simulation-owned Spire collision positions and pass
  bright-skin color through drawing without restoring render-time position writes.
- Documentation index: retain both rendering topic entries.

These resolutions and the form fix received an independent second review.
The combined desktop Release, Windows x64 server Release, and Android Debug
builds passed; Android reports 14 existing XML-documentation warnings.
Combined form (699), controller (126), setup (6), updater (9), release-workflow
(8), portable-path (12), ROM-browser (22), firing-phase (8), bright-skin,
frame-timing and real-GL resource teardown checks passed. The asset/map guards
also pass in Git Bash after accounting for Windows CRLF allow-list lines.

`integration/all-branches` additionally contains the unpublished feature work
and cross-branch protocol/input/replay resolutions. See [INTEGRATION.md](../INTEGRATION.md).
The two branches intentionally are not interchangeable.

## Unpublished branch audit

Combined protocol-10 fixes in `edfe48a` (integration cherry-pick `6af9045`):

- Restart empty continuous servers in InMatch and discard old ballot/snapshot.
- Refuse stale session stream identities before they can reset newer gameplay.
- Apply delayed roster teams to already-active players without reinitializing them.
- Refuse duplicate player slots before a relay snapshot can mutate frame/life baselines.

The combined lifecycle suite passes 3,681 assertions, including maximum
1,875-byte snapshot decoding through the last player/resource, malformed tails,
unexpected sender endpoints, and the new regressions. Loopback lobby suite:
2,690 assertions. The integrated Spire asset-backed diagnostic also needed
current stream/generation/life stamps; its two asset-free identity regressions
failed before that repair and now pass. This does not replace its asset-backed run.

Replay/alt-form corrections:

| Branch | Source fix | Integration fix |
| --- | --- | --- |
| `fix/alt-form-stability` | `3789a4a` (pushed) | `db229b3`: capture Spire flick attack before outgoing input history |
| `feature/replay-system` | `b9f619d` (pushed) | `69dab6e`: pump recorded packets through the lobby load barrier |
| Replay + lifecycle/team resources | Combined only | `d7920bd`: validate complete snapshot tails when capturing/writing bootstrap |
| Replay + controller support | Combined only | `259ffa5`: resolve input context before replay controller sampling |

Combined replay format: 534 checks; alt-form: 512; pointer: 83; controller: 126;
form helper: 699. Replay-control and frame-timing checks also pass. No additional
evidenced issue was found in the standalone stylus branch. Detailed boundaries:
[replay/stylus/alt audit](PR-AUDIT-REPLAY-STYLUS-ALT.md).

Standalone network fixes were independently adapted to each source wire format,
tested, and fast-forward pushed:

| Branch | Source tip | Source checks |
| --- | --- | --- |
| `fix/network-lifecycle-stabilization` | `49f3a30` | 3,664 lifecycle assertions |
| `feature/persistent-lobby` | `d1a4c83` | 2,226 lobby assertions |
| `feature/team-layouts` | `4b2ed23` | 2,610 lobby/team assertions; includes the lobby restart fix |

Their desktop/server and Android builds passed. All three latest fork CI runs
also passed. The integration tree has equivalent production fixes and its own
combined-format regressions, not duplicated old-format source fixtures.

macOS source `ce86011` includes the concurrently developed source fix moving
bundled maps into signed Resources. Native ARM64 and x64 CI passed:
[run 35171566749](https://github.com/AntiNotAnti/Fruity-Prime/actions/runs/35171566749).
The audit verifies actual packaged OpenAL function binding, no-display startup,
missing-library/map failures, native architectures/signatures and resource tampering.
Merging it with updater PR #39 also required forwarding both version outputs
through the new resolve job; the local regression script checks that wiring.

All eight changed upstream PR branches and both replay/alt source branches
have passing latest fork build runs at their listed audit tips. These are not
claims that upstream required checks, approvals or live acceptance are complete.
The upstream pull-request workflow runs explicitly report `action_required`,
not a passing run (for example [#46's run](https://github.com/liveteklol/Fruity-Prime/actions/runs/35171570768)).
An upstream maintainer must approve those fork workflow runs before treating
the PRs as CI-cleared upstream. This audit did not bypass that gate.

`feature/map-studio` gained `86086f1` during this audit and is now included.
Its original 82 map-pipeline checks and fork CI pass. Source correction
`c10f18c` fixes project-relative BSP/texture precedence, multi-extension Windows
device names in packages, and closed zero-volume convex brushes. Its expanded
105-check suite includes undo/redo diagnostics and preservation of existing
installed files after rejected replacements; desktop/server/Android Release pass.
The merged editor was also captured and visually inspected at 1440x900 and
960x600 without assets. This is layout verification, not interactive acceptance.

Combined protocol **11** preserves lifecycle/team fields and adds mandatory map
negotiation. Connecting to an idle lobby does not download or require a running
map: CLI and GUI verify the selected map when loading starts. A real client
verifies twice across consecutive rounds (2,693 lobby assertions). The transfer
pump uses the live monotonic clock, so paused gameplay cannot leave packet
timestamps stale and falsely time out an active download. The new regression
failed before `6e4e185` and passes afterward (3,684 lifecycle assertions).
Source transfer correction `b5b6cf0` binds downloaded packages to offered room,
version and identity; recovers damaged local caches; normalizes hashes; contains
malformed JSON failures; and cancels on match changes. Its source UDP suite
passes 23 checks. Source package + transfer fixes are merged and pushed as
`41bfe26` on `feature/map-studio` (the original working checkout is preserved).
Its latest [fork CI run](https://github.com/AntiNotAnti/Fruity-Prime/actions/runs/35173117166)
passes at that exact source tip.

Combined transfer checks additionally stamp lifecycle fixtures and cancel on
authority changes (25 checks). Offers use the frozen lobby selection, not the
continuous-server rotation. Valid chunk traffic gives unloaded participants
a bounded loading grace; no traffic retains the old 15-second timeout and even
continuous requests cannot exceed 195 seconds from the original barrier start.
These boundary checks bring the lifecycle suite to 3,687 assertions.

Combined macOS correction `31e9743` separates writable user maps from immutable
bundled maps. Catalog precedence checks identity across roots; editor saves,
autosaves, asset/preview writes, download/install/cache directories and missing
import-texture outputs use Application Support. CLI catalog and package/build
commands follow the same rules (`02a760f`). Synthetic platform tests and 33 path
checks pass, along with the combined 105 map-pipeline checks. Native execution
of this newer Mac + Map Studio combination still needs a Mac runner/device.

Final cancellation review found that Starting can return to Lobby without a
new match ID. `dd01130` treats that as a return-to-lobby outcome, not a transfer
failure/disconnect, across desktop GUI, terminal and Android. It also adds the
map verification step to Android's separate scene-construction path. A live
loopback regression verifies cancellation preserves the session (26 map network
checks total); Android's device UI path is compile-reviewed, not device-tested.
`feature/stylus-touch-input` has no committed changes relative to the audited
base. The seven stylus edits that were uncommitted at audit start are outside
this commit/PR review and were not modified, stashed or reset by this audit.
Another audit task began during final handoff; any newer source work belongs
to that task's scope rather than this pinned verification boundary.

All private audit branches were renamed from the old tool-name prefix to
`audit/` at the user's request. No branch was deleted or force-pushed.

## Final verification boundary

Production-code boundary: `dd01130`, plus the final Android cancellation early
return that prevents a canceled load from firing its success callback.
Desktop Release, Windows x64 server
Release and Android Release builds pass. Android retains 14 XML-documentation
warnings. Both osx-arm64 and osx-x64 self-contained publishes pass from Windows;
these are compile/package checks, not native combined-build execution.
The regression results above and asset/map guards pass locally. The integration
and original-ten-PR merge-check branches remain local, with no new PR created
by this audit. Main/master and the original seven dirty files are untouched.

## Limits and merge cautions

- Local compilation and deterministic/loopback tests are not a certification
  of real Internet multiplayer, Android hardware, physical controllers/pens,
  or asset-backed rendered gameplay. Those acceptance runs remain necessary.
- The Map Studio source task documents an intermittent TEST PADS native
  process exit failure, also reproduced on its unchanged baseline. This audit
  did not reproduce or resolve that asset-backed crash; do not call it fixed.
- Combined snapshots can exceed a 1,500-byte MTU; WAN fragmentation behavior
  is unverified. Do not mix independently developed protocol-8/9 branch builds.
- GitHub CI is distinct from local results; check each PR's latest commit and
  required checks before merging, especially after a target-branch update.
- Do not blanket-resolve conflicts with either side. The prepared merge branch
  provides concrete resolutions, not authority to merge main automatically.
