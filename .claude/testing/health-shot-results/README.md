# Local health and shot measurements — 2026-09-17

See [design and interpretation](../../multiplayer/NETWORK-HEALTH-SHOTS.md).

* `press-age-summary.csv/json`: 96 server/two-client sessions, 48 per arm,
  20 simulation seconds each, complete RTT/jitter/loss A/B matrix.
* `weapon-summary.csv/json`: 18 eight-weapon/duel sessions at 30 seconds, plus
  four Sylux Shock Coil sessions at 45 seconds. `root` distinguishes the
  original 16-unit volley setup from the corrected nine-unit Shock Coil setup.
  The original severe Sylux arm had no contacts; its completion is not acceptance.

Generated with `tools/hitrig/summarize-windows.py`. Missing fields mean that
the report emitted no such metric; they are not fabricated zero observations.
Client totals include both client processes, including their remote copies.
Server damage events come from its complete shot trace. Rewind statistics are
from its final periodic sample and can omit the last fraction of the run.
An authority event is not necessarily paired with a local prediction. Do not
interpret subtraction of aggregate hit counts as lost or phantom shots.

`completed` means both clients joined and produced reports. The existing
feature-tour RESULT also checks unrelated actions that these weapon scenarios
do not perform, so it is not a pass/fail test for this experiment. The deterministic
input and actual-engine suites supply the assertions; these files supply measured
network behavior, including its remaining disagreements.

Raw reports remain under the listed local `artifacts` paths. These committed
files contain numerical summaries only, with no cartridge assets or previews.
