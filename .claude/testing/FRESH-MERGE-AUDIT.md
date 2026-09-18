# Fresh branch and PR audit — 2026-09-16

The default branch is `master`, at `547ffba20519bbbdd09a379fc40f5fb1443cc99b`.
This audit covers every local branch, every branch in the AntiNotAnti fork,
and every open upstream PR, including third-party collision PR #28. The
combined destination is `integration/all-branches`. No default branch was merged.

## Branch coverage and PRs

Every local branch and every origin branch present at the final audit is an
ancestor of the integration assembly. This includes the previous audit branches:
their equivalent fixes are represented without duplicating older protocol fixtures.
Clean local feature/fix worktrees were fast-forwarded to their remote source tips.
`feature/stylus-touch-input` is an empty alias of master; the implementation is
`feature/stylus-pointer-input`. The earlier seven apparent pending edits were
line-ending-only differences, as confirmed by the completed stylus task.

| Branch | Upstream PR |
| --- | --- |
| fix/rom-picker-33 | [#35](https://github.com/liveteklol/Fruity-Prime/pull/35) |
| fix/lockjaw-wireframe-24 | [#36](https://github.com/liveteklol/Fruity-Prime/pull/36) |
| fix/portable-extracted-files | [#38](https://github.com/liveteklol/Fruity-Prime/pull/38) |
| fix/updater-release-number | [#39](https://github.com/liveteklol/Fruity-Prime/pull/39) |
| fix/spire-alt-collision-sim | [#40](https://github.com/liveteklol/Fruity-Prime/pull/40) |
| fix/form-reconciliation | [#41](https://github.com/liveteklol/Fruity-Prime/pull/41) |
| fix/shock-coil-phase | [#42](https://github.com/liveteklol/Fruity-Prime/pull/42) |
| fix/respawn-render-corruption | [#43](https://github.com/liveteklol/Fruity-Prime/pull/43) |
| feature/brightskins | [#44](https://github.com/liveteklol/Fruity-Prime/pull/44) |
| feature/controller-support | [#45](https://github.com/liveteklol/Fruity-Prime/pull/45) |
| fix/macos-native-packaging | [#46](https://github.com/liveteklol/Fruity-Prime/pull/46) |
| fix/network-lifecycle-stabilization | [#47](https://github.com/liveteklol/Fruity-Prime/pull/47) |
| feature/persistent-lobby | [#48](https://github.com/liveteklol/Fruity-Prime/pull/48) |
| feature/team-layouts | [#49](https://github.com/liveteklol/Fruity-Prime/pull/49) |
| feature/replay-system | [#50](https://github.com/liveteklol/Fruity-Prime/pull/50) |
| feature/stylus-pointer-input | [#51](https://github.com/liveteklol/Fruity-Prime/pull/51) |
| fix/alt-form-stability | [#52](https://github.com/liveteklol/Fruity-Prime/pull/52) |
| feature/map-studio | [#53](https://github.com/liveteklol/Fruity-Prime/pull/53) |

PRs #47–53 were created by this audit. GitHub reports all 19 open upstream PRs
mergeable against the current master. Local `git merge-tree` checks also pass
for every fork source tip against master. This is distinct from upstream CI
approval: fork workflows currently require a maintainer's approval.

The old fork [macOS PR #1](https://github.com/AntiNotAnti/Fruity-Prime/pull/1)
was closed as superseded by #46. Its history is included, but its obsolete
post-release workflow and signing files are excluded: that workflow would
overwrite the newer signed app bundles with flat packages.

Upstream remote branches without an open PR were inventoried, not proposed as
new contributions from this fork. Most are already in master. Divergent historical
refs (`animation2`, `bugfix-scroll-android`, `fix-map-rotation-crash`, `message-info`,
`net10`, `net5`, `temp`, `temp2`, `texgen-testing`) are retained unchanged. They are
upstream-owned historical/development lines, not the user's pending source branches.

## Overlap resolutions

- Retain the combined protocol 11 identity, team/resource tails, map transfer and
  replay bootstrap format; older source fixtures must not replace combined tests.
- Preserve lifecycle spawn guards, simulation-owned Spire collision, pre-network
  flick capture, pointer dispatch and replay controller context ordering.
- Preserve the latest canceled-load behavior on desktop, terminal and Android,
  including the Android early return before its success callback.
- Preserve macOS writable-map paths and the native signed-bundle release pipeline.
- Map Studio now incorporates the actual commits from
  [collision PR #28](https://github.com/liveteklol/Fruity-Prime/pull/28). The source
  update `6ce923c` and CRLF guard fix `4bb18b8` were pushed to its existing branch.
  Collision OBJ is applied by the shared compiler, included in fingerprints and
  packages, isolated from filesystem fallback, validated, and retained by Save As.
  Both Q3 conversion callers use the updated signature. Transformed coordinate
  bounds and missing/null dependency paths have regression coverage.
- Block Fort's recipe is retained as `.json.example`: its external PK3 is absent.
  This preserves the contribution without breaking every package build or claiming
  the complete map is shipped. No external game assets were downloaded.

Shared code means the focused PRs cannot honestly be described as disjoint or
safe to merge in arbitrary order. The integration branch contains the reviewed
conflict resolutions and is the prepared combined merge candidate. #49 includes
#48; #53 includes #28. Merging these dependencies first keeps later PR diffs focused.
Private audit branches have no independent changes left and do not need duplicate PRs.

## Fresh verification

- Windows desktop self-contained Release publish: passed.
- Windows dedicated-server self-contained Release publish: passed; startup refusal
  without game files and local directory query/registration checks passed.
- Android full Release APK build: passed, with 14 existing XML documentation warnings.
- Source Map Studio desktop build and Android Compile: passed, plus original map
  bundle cooking and dependency checks. Source map suite: 126 checks.
- Combined map pipeline: 126; map transfer: 26; lifecycle: 3,687; lobby: 2,693.
- Replay format: 534; replay timing/camera, frame timing and bright skins: passed.
- Pointer: 83; alt-form: 512; controller: 129; form reconciliation: 699.
- Firing phase, paths, ROM browser, setup, updater and synthetic platform checks: passed.
- Real OpenGL framebuffer teardown: three cycles passed. Spire simulation ownership
  and all eight release-version workflow checks passed.
- Repository asset guard and shipped-map dependency check: passed.

Final integration output is built from the integration worktree under
`artifacts/local-build/`.

## Combined CI follow-up

The first complete integration CI run caught two problems beyond the Windows
checks. Both were repaired before the final CI rerun:

- macOS bundle signing treats controller mapping files under `Contents/MacOS`
  as unsigned code. Packaging now moves the database and license to Resources;
  runtime lookup uses the resource root while preserving portable paths and
  user/environment overrides. Native tests cover placement and signature tampering;
  the production smoke test verifies both resource files. Controller checks now
  total 129. Packaging fixes were also backported to the #46 source branch.
- The UDP loss test could send Ready against the admission revision before the
  Identify revision arrived. It now waits for the acknowledged roster identity
  and matching revisions before introducing loss. The server's deliberate
  stale-command rejection remains unchanged. The correction was also applied
  to the lobby, team and replay source branches; replay additionally received
  the lobby source's existing empty-server restart repair.

The final CI status is linked from the integration PR. This document records the
local evidence and repaired CI findings, rather than treating a queued run as passed.

Native combined macOS execution/signing cannot be certified on this Windows host;
the native-only test script correctly refuses to run here. Prior native macOS
source CI is not certification of this combined tree. Physical devices, rendered
asset-backed gameplay and Internet multiplayer remain outside these checks.
The earlier baseline TEST PADS native exit issue and snapshot MTU limitation remain
documented in [MERGE-AUDIT.md](MERGE-AUDIT.md); this audit does not claim them resolved.
