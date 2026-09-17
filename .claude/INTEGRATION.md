# All-branches integration

`integration/all-branches` combines the local branch commits present during the
2026-09-16 integration. Source branches are retained. The working checkout is
the sibling `Fruity-Prime-integration` worktree; the original checkout and its
seven uncommitted stylus-related edits were left untouched. Uncommitted edits
are not part of a branch merge. Nothing was pushed by this integration.

## Combined behavior

- Protocol **10** combines lifecycle identity/order validation with persistent
  lobbies, teams, authoritative resources and continuous-weapon firing phases.
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
warnings/errors. Android Debug built with 14 XML-documentation warnings and no errors.

Asset-free checks passed:

- `-netlobbytest`: 2,690 assertions, real loopback UDP, retries, two rounds,
  owner migration, teams, rebind and continuous rotation.
- `tools/nettest --lifecycle`: 3,660 assertions with seeded packet faults.
- `-replayformatcheck`: 520 checks, including production capture/bootstrap with
  sparse slots, generations, teams and independent revisions.
- `-altformcheck`: 510; `-pointercheck`: 83; `-gamepadcheck`: 116.
- `-frametimingcheck`, `-brightskinscheck`, `-replaycontrolcheck`.
- `tools/formtest`: 606; `tools/continuous-phase-check`: eight cases;
  `tools/pathstest`: 12; `tools/rombrowsercheck`: 22.

These checks do not establish rendered gameplay correctness, physical stylus
or controller behavior, Android device behavior, or real Internet multiplayer.
Asset-backed health simulation and all-map/rendered probes were not run.
Historical branch notes elsewhere retain their original protocol numbers and
assertion counts; this note describes the combined format and verification.
