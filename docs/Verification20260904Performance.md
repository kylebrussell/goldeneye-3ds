# Model publication hot-loop optimization

Follow-up: [stage-coordinate correction and prepared visibility](Verification20260904StageScaleVisibility.md)
diagnoses the Cradle/Runway failures below and records the subsequent builds.

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

The Mac initially locked before the controlled 750-frame comparisons could
start. The user subsequently unlocked it and requested `caffeinate`; a
one-hour `caffeinate -di -t 3600` session kept this validation session awake.

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

At the initial locked checkpoint, Azahar remained on the combat-tested
`3490fbf0` / `ee769251` baseline. The following unlocked measurements supersede
that checkpoint's pending-validation status. Save/DSP settings were not
changed manually.

## Unlocked, controlled movement comparisons

The saved baseline `3490fbf0` and candidate `29b97c6a` ran the same
`stage-move-look-fire-750.cfg`, unchanged pack and Azahar settings, with no
concurrent builds. Each completed 750 original movement/actor ticks. In Dam
and Facility, non-timing gameplay, draw, topology, sound decode and publication
counters match exactly. End positions also match exactly.

| Stage | Total measured frame work, baseline → candidate | First-person vertex ticks, baseline → candidate | Post-warmup frames >16 ms |
| --- | --- | --- | --- |
| Dam | 7,634 → 7,498 ms | 105,737,231 → 71,600,950 | 7/630 → 7/630 |
| Facility | 5,628 → 5,505 ms | 105,589,956 → 71,393,534 | 0/630 → 0/630 |

First-person vertex processing is roughly 32% lower; total measured frame
work is only 1.8–2.2% lower. First-person full cache build is about 23% lower.
This does not eliminate the slow tail: Dam's post-warmup peak remains 19 ms;
Facility remains 14 ms. These short traces remain in the opening room and
are not mission completion or combat certification. The timing counters
measure instrumented work, not a guaranteed displayed FPS or hardware speed.

Evidence: `build/visual-probe/matrix-run-{3490,29b}-{dam,facility}750.result`.
The candidate's new wait counter records only 619,500 and 643,064 ticks
respectively over each entire trace. Music-render call counts are zero:
these runs exercise SFX/NDSP but do **not** measure active original music
synthesis. Do not interpret zero as free audio processing.

### Cradle is not valid world-performance evidence

The same trace in Cradle completed on both builds with identical player and
draw state, but both publish zero rendered world rooms and attempt 751 scene
installations; `overlay_refresh` records 750 door refresh failures. A live
baseline screenshot showed the gun and sky without the world. Thus its
2,518 → 2,394 ms aggregate and 10 → 9 ms post-warmup peak are **not** evidence
of a performant playable Cradle. This is a pre-existing shared visibility /
scene-installation gap requiring diagnosis, not a new optimization regression.
Evidence: `matrix-run-{3490,29b}-cradle750.result`.

### Runway also fails the world-rendering prerequisite

Baseline and candidate both report
`stage_scene_install=1,0,16,10,0,0,2,16,0,6` and zero world draw calls.
Here scene status 2 is `GE_ORIGINAL_MODEL_SCENE_INVALID_LAYOUT`, **not**
`GE_DAM_DYNAMIC_SCENE_ASSET_NOT_FOUND`; failure phase 6 is
`RUNTIME_STAGE_SCENE_INSTALL_DOOR_QUERY`. The door model query in
`install_stage_ordinary_object_scenes` rejects an input before completing the
aggregate overlay and world installation. Both execute all 750 movement and
actor ticks without a recorded actor error, but these are not valid playable
world benchmarks. The specific door/part and rejecting layout condition still
need to be isolated. Evidence: `matrix-run-{3490,29b}-runway750.result`.

## Extended candidate Dam combat trace

The existing 10,000-frame maximum / 160-target authored-route input trace ran
on `29b97c6a`, with no concurrent builds. Bond died after 4,704 presented
frames / 4,705 original movement and actor ticks, at target 11/160. This is
mission failure (`status=failed`), not a runtime crash or route completion.
Baseline died at target 12/160 after 4,680 presented frames, with different
guard/draw/publication workloads. Do not derive a controlled whole-run speedup
from these unequal encounters.

- Candidate: 613/4,584 post-warmup frames over 16 ms; two over 25 ms; none
  over 33 ms. Post-warmup peak 28 ms. Baseline: 563/4,560 over 16 ms and
  peak 30 ms. The fraction of slow frames did **not** improve in this pair.
- Candidate first-person vertex work: 383,493,675 ticks over the run, versus
  baseline 613,329,210. Workload differs, so the controlled 750-frame traces
  above are the evidence for the local optimization's gain.
- Candidate world submission: 4,244,700,833 ticks (about 15.83 seconds);
  aggregate guard cache build: 1,203,549,370 ticks (about 4.49 seconds).
  The 28 ms slow-frame record includes 20 ms simulation, of which 5 ms is
  nested overlay work, plus 1 ms camera, 1 ms first-person and 7 ms rendering
  (independently rounded counters are not exactly additive).
- Actor status remains healthy; zero guard matrix/publication/overlay growth
  errors. 592 original sounds decoded without decode failures; NDSP active,
  3,414 blocks prepared. Music-render calls remain zero and active music
  synthesis is not certified by this run.
- Live screenshots showed movement, changing PP7 ammunition, a textured
  guard and Dam ground/walls. They are not exhaustive visual comparisons.

Evidence: `build/visual-probe/matrix-run-29b-combat.result`.

## Final checkpoint and next priorities

Azahar has the tested `29b97c6a` executable / unchanged `ee769251` pack.
Hardware staging remains unchanged. Temporary input and stage overrides were
removed to private `matrix-run-final-used-*.cfg` evidence files, restoring
normal frontend boot. Save files and DSP settings were not changed manually.
The user-requested one-hour `caffeinate` session is left to expire normally;
no system lock/security settings were changed.

The local hot-loop improvement is verified in Dam and Facility, but sustained
60 FPS across levels remains unproven. Next priorities:

1. Isolate Runway's rejected door model part and exact layout invariant;
   repair its adapter using the authored model/matrix data, not by dropping
   the door or accepting invalid matrices.
2. Diagnose Cradle's zero rendered rooms through original global visibility
   and camera publication, then the empty-scene door-generation/rebuild loop.
3. Profile the unchanged combat-heavy guard/overlay and world submission
   peaks. The measured FrameBegin wait is tiny, so these traces do not
   support prioritizing GPU wait as the main CPU-frame bottleneck.
4. Repeat meaningful cross-stage movement/combat traces after the above
   prerequisites work; include active original music and actual New 3DS
   hardware before claiming target-wide 60 FPS.

The earlier Dam tower/windowed-door visual rechecks remain pending.
