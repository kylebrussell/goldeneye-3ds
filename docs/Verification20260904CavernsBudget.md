# Caverns frame-budget checkpoint — 2026-09-04

The first representative target is the existing 750-tick Caverns opening
controller trace: idle, forward movement, strafing, turning, firing and
settling. It retains the original simulation, music, SFX, actor visibility
and asset pack. It stays near the starting room and is not a combat route
or mission completion. New 3DS XL remains the primary hardware target;
these measurements use Azahar and do not certify physical hardware FPS.

## Measurement

Input probes now buffer per-frame ARM11 tick counters in memory and write
them after the trace. `scripts/analyze_3ds_frame_timing.py` reports exact
60 Hz work-budget misses using `ticks * 60 > clock_hz`, rather than rounded
millisecond counters. The first 120 submissions are excluded from the warm measurement window
by the configurable analyzer cutoff.
Allocation is diagnostic-only and released on stage cleanup. A missing
timing buffer produces no timing rows; the analyzer rejects that result.

`frame_timing` columns are: presentation number; work, start interval,
frame-begin wait, music, canonical tick, guard scene, world submission,
renderer, props, actor vertices, guard GPU upload, guard replacement,
guard texture import, visibility publication, first-person publication,
guard commit, world flush ticks; simulation retraces; detailed-world-sample
flag. Several phases are nested, so their values must not be added together.
Start intervals measure CPU scheduling, not actual LCD scanout.
`frame_present_timing` adds submission number, explicit pacing wait ticks,
previous completed GPU duration in microseconds, and top-screen VBlank counter.
The wait is excluded from CPU work. Submission-counter differences establish
queue pacing, not a capture of physical LCD scanout.

Timing format version 2 records the actual retraces passed into canonical
frame accounting. Earlier experimental rows accidentally stored gameplay
**dispatch count** (always one), which cannot identify catch-up frames.
The analyzer reports their multi-retrace count as unknown. The route's
`simulation_frames` likewise counts gameplay dispatches, not elapsed retraces.

Detailed per-batch world timing is now opt-in: put `1` in
`sd:/3ds/goldeneye-3ds/world-profile.cfg` alongside an input probe. It adds
visible overhead every 16th frame. Normal gameplay and default input probes
avoid those per-batch clock calls. The lightweight frame counters still add
some measurement overhead.

## Retained changes

- Compile only the integer audio interpreter at `-O3`, with strict arithmetic
  and no fast-math change to canonical gameplay.
- Prove ADPCM predictor bounds once per encountered codebook predictor per
  command. A 32-bit accumulator is safe when every row's absolute coefficient
  sum is at most 63,487: `32768 * (2048 + 63487) < INT32_MAX`. Every partial
  sum fits as well. Other predictors retain the 64-bit path. Existing audio
  goldens pass; 4,096 new bounded/boundary cases match the pre-change scalar
  DMEM/state digest `b961c332`, including both signs and mixed fast/fallback
  predictors within a command. Logs: `audio-{reference,bounded}-test.log`
  under `build/host-tests/caverns-budget-pass/`.
- Snapshot active prop membership once per visibility-publication pass.
  The snapshot is local and discarded before canonical list mutation.
  Ordinary-object matrix/flag publication keeps its original operations;
  guard counters consume narrow observations instead of full AI/model
  snapshots. Character ONSCREEN and depth remain owned by original chrTick.
- Use the existing exact 256-entry normalized color table for world and
  first-person vertex publication. Every entry remains the original `i/255.0f`.
  This removes repeated component divisions on ARM11; both renderer flag modes
  and all 256 values are checked. The earlier host-only UV/color experiment
  had insufficient target evidence to promote this table; this pass measures it.
- Keep envelope ramp state in 32 bits, compare unsigned distance to the target
  before addition, and clamp before any overflowing addition could occur.
  A sanitizer differential test compares 16,016,000 steps with the previous
  64-bit implementation, including signed extrema and wrong-direction steps.
- Copy changed guard material batches directly into the two published buffers,
  then adjust offsets there. This eliminates temporary copies of large records;
  range validation and generation accounting remain in place.
- Pace gameplay GPU submission to at most one submission per top-screen VBlank.
  If a new interval has already elapsed, submit immediately; otherwise wait
  before ending the frame. This avoids bunching a short frame after a long one
  into the same display interval. Canonical elapsed-time retrace dispatch and
  the zero-retrace input/audio idle path remain intact. RAMROM bypasses the wait.

The Citro3D queue completes asynchronously and swaps its framebuffer on queue
completion; CPU work alone is therefore insufficient to claim displayed FPS.
See the [upstream render queue implementation](https://github.com/devkitPro/citro3d/blob/master/source/renderqueue.c).
The pacing test executes the actual block with a simulated clock and checks
short work, already-late work, multiple elapsed intervals, counter wrap and
RAMROM bypass.

A 32-slot cache of repeated authored UV pairs was tested and rejected: exact
mapping tests passed, but its small measured gain did not remove either spike.
It is absent from the final implementation.

## Optimization measurements

Before the pacing change, each short trace completed 750 gameplay dispatches
and reached `5353.534180,-2665.300537,-815.943909`. These are individual
emulator samples, not hardware timings.

| Candidate | Warm work misses / 630 | Warm peak | Detailed world sampling |
| --- | ---: | ---: | --- |
| Instrumented prior build | 20 | 19.25 ms | On |
| Audio `-O3` | 12 | 18.85 ms | On |
| Bounded ADPCM | 9 | 18.73 ms | On |
| Bounded ADPCM, lightweight timing | 3 | 18.72 ms | Off |
| Active-set publication | 2 | 17.81 ms | Off |
| Exact color lookup | 1 | 17.17 ms | Off |
| 32-bit envelope ramps | 1 | 17.05 ms | Off |
| Direct guard batch copies | 0 | 16.59 ms | Off |
| VBlank-paced candidate | 1 | 16.72 ms | Off |

The first three runs cannot be compared directly with later runs as an
unmodified-runtime FPS gain because detailed timing changes the workload.
Audio `-O3` reduces measured mean music work about 6%; bounded ADPCM removes
another roughly 2%. The active-set pass reduces visibility publication from
about 1.3 ms to 0.33 ms. Its two remaining spikes occur at frame 261 (guard
geometry publication) and frame 481 (first firing/first-person layout).
Color lookup reduced the former's GPU vertex upload from about 2.92 to
2.16 ms; direct batch copies reduced guard commit from about 0.84 to 0.52 ms.

Extending the unpaced candidate to 3,000 gameplay dispatches exposed 66 CPU
budget misses out of 2,880 warm frames, with a 22.08 ms peak. The same extended
paced trace had 22 misses and a 21.38 ms peak. These traces are not identical
simulation states after the pacing change: elapsed retrace grouping depends
on wall-clock scheduling, so the CPU difference must not all be attributed
to faster work. Both extended traces ended at
`5353.534180,-2665.283691,-764.409119`.

The paced candidate submitted exactly once in each of 2,879 adjacent warm
display intervals in that extended run, with no repeated or skipped interval.
Its maximum reported previous-frame GPU time was 293 microseconds in Azahar.
Live inspection of the moving opening view showed 60 FPS at 100% emulator
speed, whereas the unpaced build had shown 30 FPS in that view. This establishes
an emulator checkpoint, not a hardware guarantee.

## Final verification

Final executable SHA-256:
`fef1a3dfa7345f81d02eaaacdeafd6ab746483575b11ba8bee2216e537d211c4`.

The final 3,000-dispatch run (format version 2) recorded:

- 2,879 adjacent warm submission intervals, all exactly one VBlank apart.
- Live 59–60 FPS at 100% emulator speed; 60 FPS again in a fresh opening repeat.
- CPU work mean 8.86 ms, p95 14.34 ms, p99 16.77 ms, peak 21.63 ms;
  30 of 2,880 warm work samples over 16.67 ms, excluding explicit pacing wait.
- Seven warm dispatches consumed more than one elapsed canonical retrace.
  The elapsed-time scheduler remains authoritative; these are not dropped ticks.
- Previous completed GPU duration at most 293 microseconds in the emulator.
- Final position `5353.534180,-2665.283691,-764.409119`, room 62.

The final 750-dispatch repeat also had exactly one submission in all 629
adjacent warm display intervals. Mean CPU work was 7.45 ms and peak 16.72 ms,
with one budget miss and one multi-retrace dispatch. It reached
`5353.534180,-2665.301025,-814.812073`, matching the earlier paced short run;
the small endpoint difference from the unpaced trace is retained in evidence.

Evidence: `build/visual-probe/caverns-budget-final{750,3000}.result` and
`build/host-tests/caverns-budget-pass/final{750,3000}-timing.json`.
CPU headroom is still insufficient to promise all future frames below 16.67 ms;
the observed success is display submission cadence in this representative
emulator context. A full level, combat route and physical hardware remain
separate acceptance gates.

The full `scripts/test_port.sh` suite passed, including exact audio output,
16,016,000 differential ramp steps with ASan/UBSan, active-set membership and
matrix/flag equivalence, dynamic-scene batch publication, renderer vertex
publication, pacing behavior and timing-analysis validation. The first final
suite run found an obsolete structural assertion for the removed temporary
batch; it now follows the direct destination pointer while retaining pose-only
publication ordering. Logs are `final-full-suite{,-recheck}.log`; final ARM build
is `arm-final.log`, all under `build/host-tests/caverns-budget-pass/`.

 Cold startup still exceeds
the frame budget. The existing elevator/guard rendering defect remains visible;
this performance pass neither resolves it nor suppresses scene content.

Private trace files: `build/visual-probe/caverns-budget-*.result`.
Build, exact-output and analysis logs: `build/host-tests/caverns-budget-pass/`.


## Staging

The final executable and metadata are installed in Azahar and staged at
`build/3ds-sd/3ds/goldeneye-3ds/`, with a preserved copy in
`build/3ds-candidates/caverns-budget-fef1a3df/`. The prior `6f6d1a92` executable
remains in `build/3ds-candidates/visible-runs-6f6d1a92/`.
`deploy_3ds.sh --skip-build` validated the SD tree (`staging.log`), and
`git diff --check` passed. Asset pack SHA-256 is unchanged:
`ee769251742b72bcaa9a3d1586794246355bc995dc62637e9755dc276adfdeb7`.

Temporary stage/input overrides were moved to
`build/visual-probe/caverns-budget-final-used-*.cfg`. Normal startup reached
the original cast sequence, then emulation was stopped through the UI. This
boot check is not a frontend FPS acceptance test. Saves and DSP files were
not manually modified. No commit, push or physical-device deployment was made.

Follow-up: [Door occlusion and publication optimization](Verification20260904DoorOcclusion.md) fixes inherited door depth and measures the unchanged-door generation fast path.
