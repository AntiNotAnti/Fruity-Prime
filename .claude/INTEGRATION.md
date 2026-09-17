# All-branches integration

`integration/all-branches` combines every local and fork source branch audited
on 2026-09-16, including upstream collision PR #28. Source branches are retained.
The working checkout is the sibling `Fruity-Prime-integration` worktree, and the
integration branch is published to origin. Seven missing upstream PRs (#47–53)
were opened. See [FRESH-MERGE-AUDIT.md](testing/FRESH-MERGE-AUDIT.md) for current
branch coverage, conflict resolutions, verification and remaining gates;
[MERGE-AUDIT.md](testing/MERGE-AUDIT.md) preserves the earlier audit boundary.

## Combined behavior

- Protocol **11** combines lifecycle identity/order validation with persistent
  lobbies, teams, authoritative resources, continuous-weapon firing phases and
  Map Studio's verified custom-map transfer. Map negotiation occurs at the load
  barrier, not while connecting to an idle lobby. Map packet IDs 32-35 remain
  separate from the lobby packet IDs starting at 36.
  All peers must use the integration build; old-protocol recordings are incompatible.
- Roster headers are 17 bytes, entries are 25 bytes. The 32-bit roster sequence
  is independent of the 16-bit lobby session revision. SessionState includes
  an authority epoch. Match identity advances before starting simulation or
  broadcasting rotation; lobby state establishes identity before the roster.
- Snapshots retain lifecycle fields, 64 bytes of team clocks and the bounded
  health-spawner tail. Maximum combined datagram: 1,875 bytes (eight players,
  56 spawners), with a 2,048-byte buffer bound. This can require IP fragmentation
  on a 1,500-byte-MTU network: the lifecycle branch's single-MTU packet-budget
  property is NOT preserved. Compression or splitting the resource update is
  future work; WAN behavior is not verified.
- Form reconciliation uses the elapsed-frame helper with ping grace and stalled
  animation recovery, alongside alt-form camera/flick fixes. Spire collision
  remains simulation-owned; bright skins do not restore render-time mutation.
- Controllers, stylus ownership and replay input suppression coexist. Playback
  is socket-free and preserves generations, teams and lobby revisions in its bootstrap.
- Fault simulation honors loss-setting changes after transport creation, so
  lobby retry checks exercise actual loss rather than a frozen setting.

## Verification

Desktop Release and Windows x64 dedicated-server Release built with zero
warnings/errors. Android Debug and Release built with 14 XML-documentation
warnings and no errors. Both macOS self-contained targets cross-published on
Windows; native execution of the final combined tree remains unverified.

Asset-free checks passed:

- `-netlobbytest`: 2,693 assertions, including map negotiation at consecutive load
  barriers, real loopback UDP, retries, two rounds,
  owner migration, teams, rebind and continuous rotation.
- `tools/nettest --lifecycle`: 3,687 assertions with seeded packet faults,
  including live connection timestamps while map downloads pause gameplay.
- `-replayformatcheck`: 534 checks, including production capture/bootstrap with
  sparse slots, generations, teams and independent revisions.
- `-altformcheck`: 512; `-pointercheck`: 83; `-gamepadcheck`: 129.
- `-frametimingcheck`, `-brightskinscheck`, `-replaycontrolcheck`.
- `tools/formtest`: 699; `tools/continuous-phase-check`: eight cases;
  `tools/pathstest`: 33; `tools/rombrowsercheck`: 22.
- `tools/mapcheck`: 126; `tools/mapcheck --network`: 26. Bundled maps stay
  read-only on macOS; editor/cache/download outputs use Application Support.
  The platform suite exercises synthetic Mac paths on Windows.
- Canceling a lobby start during map verification returns clients to the same
  lobby. Desktop, terminal and Android load paths preserve the connection;
  Android verifies the map before constructing its scene.
- `tools/setupcheck`: six; `tools/updatecheck`: nine; release-workflow: eight
  plus cross-job Android version-output checks; real-GL teardown: three cycles.
- macOS PR #46's native ARM64/x64 CI passed on audited source `ce86011`,
  including signed bundle, tamper, missing-library and displayless smoke tests.
  These source-branch results do not certify the later all-branches tree on Mac.

These checks do not establish rendered gameplay correctness, physical stylus
or controller behavior, Android device behavior, or real Internet multiplayer.
Asset-backed health simulation and all-map/rendered probes were not run.
Historical branch notes elsewhere retain their original protocol numbers and
assertion counts; this note describes the combined format and verification.
