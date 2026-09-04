# Model publication hot-loop optimization

Continues `05475d80`. This is an incremental CPU optimization, not evidence
of sustained 60 FPS across the game or on New 3DS hardware.

## New emulator baseline

Azahar was initially available. The installed executable was updated from
`ade136f1` to the saved `3490fbf0` door-reflection checkpoint, with unchanged
`ee769251` assets. The existing extended Dam combat input trace ran without
concurrent builds. Evidence:
`build/visual-probe/optimization-3490-baseline-combat.result`.

- 4,680 presented frames, 4,681 original movement/actor ticks.
- 12/160 route targets before Bond's death; actor status remained healthy.
  This is not mission completion.
- 563/4,560 post-warmup frames exceeded 16 ms; two exceeded 25 ms.
  Post-warmup peak 30 ms; startup-inclusive peak 59 ms.
- Original SFX/NDSP remained active. World submission accounts for about
  13.3 seconds of CPU work over the run; first-person cache build about
  3.24 seconds. Timings overlap other reported regions and must not be added
  blindly to simulation totals.

The Mac locked before the controlled 750-frame Dam/Facility comparisons
could start. No new candidate emulator timing is available.

## Retained changes

First-person layouts already retain consecutive matrix-run ends. The hot
publication loop now selects/validates the matrix and checks unchanged-bone
reuse once per run, including when the bone is moving. Vertices still execute
in original order, with identical arithmetic and cross-input duplicate-source
dependencies. No new matrix, geometry, controller or animation is introduced.
There is no additional run-metadata allocation.

Native-width local counters collect duplicate transforms; the exact totals
are published once per input instead of updating shared 64-bit fields for
every reused vertex. Static-copy accounting is similarly accumulated per
run/batch range. The generic guard/prop/door model cache uses the same local
duplicate accounting and per-input batch-copy accounting. These counters do
not drive gameplay or select geometry.

Input reports now include `simulation_wait_audio_ticks`: accumulated
`C3D_FrameBegin` wait, music-render ticks, music-render calls and peak music
call ticks. The music call retains its original condition, location within
the simulation tick and failure cleanup. These measurements will distinguish
GPU backpressure and synthesis cost from actor/overlay work in the next trace.

## Verification and limits

- Full host suite: `build/host-tests/matrix-run-full.log`, exit 0. Includes
  96 mixed-bone frames checked against scalar output bytes and exact
  per-vertex counters, plus the existing firing/modem, retained-layout,
  guard/model, collision and campaign tests.
- After adding wait/music telemetry, original-frame-order, frame-pacing and
  first-person layout checks pass. Final ARM/3DSX build:
  `build/host-tests/arm-matrix-run-telemetry.log`, exit 0.
- Controlled host adapter comparison: baseline and candidate adapters both
  compile at `-O2` with ASan/UBSan, with the rest of the same existing host
  fixture retaining its usual flags. Each runs sequentially for 3,000
  iterations on 21 original PP7 parts / 1,764 vertices:

  | Publication case | Previous | Candidate |
  | --- | ---: | ---: |
  | All vertices moving (world-space output) | 58.487 us | 53.407 us |
  | Alternating output buffers, sparse bone | 64.361 us | 68.256 us |
  | Retained output, sparse bone | 19.096 us | 19.287 us |

  The roughly 9% improvement in the full-publication case is a host
  microbenchmark, not a measured whole-game or ARM frame-rate gain. Sparse
  updates are essentially flat; changing buffers is somewhat slower in this
  run. Geometry and accounting match. Evidence:
  `matrix-run-{baseline,candidate}-o2.log` and corresponding private driver
  scripts in `build/host-tests/`.

The first attempted whole-host `-O2` run failed the existing casing-finiteness
assertion with baseline adapter sources, before reaching this benchmark.
That broader optimization/fixture issue is unresolved, not caused or fixed by
these renderer changes. See `matrix-run-whole-host-o2-rejected.log`.
Earlier `matrix-run-baseline-focused.log` / `test_matrix_run_baseline.sh`
did not substitute sources inside the script's compile loop: they are not
valid A/B evidence. Only the corrected `*_o2.sh` drivers should be used.

## Rejected experiment

A three-point per-batch clip cache preserved every tested draw decision but
made actual authored geometry slower. It was removed before the ARM candidate
build. The all-21-stage / 64-heading host comparisons are in
`recent-vertex-all-stages.log`; the patch is saved privately as
`recent-vertex-rejected.patch`. Do not reintroduce it based on the favorable
synthetic repeated-point example. The existing frustum code is unchanged.

## Exact artifacts and next work

Candidate executable:
`29b97c6a701c5aa55273ff67be19b7e8fa0fccb539813e7801ddc22f03e8513e`.
Unchanged assets:
`ee769251742b72bcaa9a3d1586794246355bc995dc62637e9755dc276adfdeb7`.
Both are saved in `build/3ds-candidates/matrix-run-29b97c6a`.

Azahar remains on the combat-tested `3490fbf0` / `ee769251` baseline.
Hardware staging remains unchanged. Temporary `stage.cfg` and
`dam-input-probe.cfg` were moved to private evidence files; no test overrides
remain installed, and save/DSP settings were not changed manually.

Once unlocked, compare identical 750-frame movement traces on baseline and
candidate in Dam and Facility, then another stage; follow with the extended
combat trace and inspect model publication, frame tails and wait/music timing.
Do not promote this candidate as a 60-FPS improvement without those results.
The earlier Dam tower/windowed-door visual rechecks also remain pending.
