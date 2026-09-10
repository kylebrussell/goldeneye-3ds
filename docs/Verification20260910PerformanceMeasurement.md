# Reproducible CPU/GPU and loading measurements — 10 September 2026

This change prepares physical New Nintendo 3DS XL and isolated emulator
measurements. No emulator or hardware performance run was performed for this
measurement implementation. Start comparisons from the September 9 checkpoint
`d0c3e8d2869341b9fee832caf7848a9862690705` and include the native ADPCM-address
and signed-envelope corrections in `141fe1a4` on both sides. The latter fixes
an ARM32 pointer failure exposed by the campaign task's offline synthesizer.
Earlier silent/defective-audio results are historical evidence only.

## Buffered runtime evidence

Input-probe timing version 4 preserves the existing frame/detail/presentation
row formats and adds `frame_pipeline` version 1:

```text
frame_pipeline=frame,previous_gpu_valid,previous_gpu_us,previous_processing_us,slider,eyes,ndsp_active,music_rendered,capture_io,primary_source_frames,primary_zero_fill_frames
```

Rows are buffered in the existing allocated timing array and written only
when the probe finishes. These additions require 40 extra bytes per timing
record on the ARM ABI (180,000 bytes for 4,500 records). The observed slider
and actual eye count come from the same values used to render that frame.
The header records the effective world, first-person and HUD stereo settings.
NDSP activity, whether music synthesis ran, and cumulative primary audio
consumption are observed, not inferred from configuration files. The primary
zero-fill counter excludes the secondary music ring and cannot by itself
prove an audible dropout. Audible output still needs operator confirmation.

Citro3D 1.7.1-2, libctru 2.7.0-1 and devkitARM r68-1 were inspected in the
installed `devkitpro/devkitarm` image. `C3D_FrameBegin(0)` waits for the previous
queue; its completion callback updates the GPU timer. The installed ARM
disassembly confirms this ordering and that `C3D_GetDrawingTime()` returns
float milliseconds. The source is
[Citro3D renderqueue.c](https://github.com/devkitPro/citro3d/blob/v1.7.1/source/renderqueue.c).
The new rows retain float microseconds instead of truncating to integer
microseconds. A failed begin or invalid value is explicitly invalid.

GPU row N belongs to frame N−1. The analyzer now associates samples that way
and reports missing completion samples. Citro3D's GPU timer includes queue
transfers and completion callback latency; it is not PICA occupancy or
separate vertex/fragment timing. `C3D_GetProcessingTime()` describes the
preceding begin/end interval, including pacing inside that interval. It is
nested diagnostic context, never additional independent CPU work.

The existing whole-frame CPU work boundary is preserved: it excludes the
explicit presentation wait, starts after `aptMainLoop`/physical input/touch
servicing, and ends before final probe accounting. Start intervals and
submission VBlank deltas expose delays outside that work interval. Submission
cadence is not a direct optical scanout measurement. No extra GPU wait or
new pacing/simulation behavior was introduced.

`stage_load_ticks` measures entry into `start_stage_runtime` through readiness
to enter the gameplay loop. It includes stage construction and initial
decoding, not the earlier global graphics/pack mount or a complete app boot.
It is reported separately as `stage_load_ms`. All in-route room streaming,
asset imports, first-person changes and ordinary stalls remain inside frame
measurements. Frontend page/actor decode hitches remain in the separate
frontend probe result and must be reported alongside stage startup.

## Private requests and collection

`scripts/prepare_3ds_benchmark.py` creates an output directory containing
`configs/` and `request.json`. It does not launch an emulator, install files,
or manage another task's SD/config directory. The coordinator stages the
configs while preserving the original inventory and performs the run.

The request hashes the executable, complete pack, any loose `converted/`
assets, source tree/commit, exact inputs, recorded cadence, and every supplied
launch configuration. Clean requests disable deep/world profiling and omit
capture/frontend/tour configs. Stereo uses full default depth; the operator
must set the slider to maximum, which runtime telemetry then verifies.

Example, from the worktree root, using private generated assets:

```sh
python3 scripts/prepare_3ds_benchmark.py prepare \
  --output build/performance-requests/dam-cold \
  --input scripts/dam-authored-firefight.cfg --stage dam \
  --binary platform/3ds/goldeneye-3ds.3dsx --assets build/3ds-assets \
  --label dam-live --environment emulator \
  --environment-description 'Record exact Azahar version, New 3DS mode, native speed and host' \
  --toolchain 'devkitARM r68-1; libctru 2.7.0-1; citro3d 1.7.1-2'
```

Use an actual environment description before running. Use
`--environment physical-new3dsxl` for physical measurements, with launcher,
firmware, model and power state in the description. The binary must be built
from the recorded source; archive the build log/toolchain image identity.
`--cadence path/to/recorded.cfg` preserves exact recorded retraces. `--mode pcm`
creates a separate instrumented correctness request, never clean timing.

The default sample policy is declared before the run: **all actual frames
after 120 warmup frames, except the final frame used to observe the preceding
GPU queue**. This accommodates routes whose displayed and configured frame
counts differ. At least 3,600 measured frames plus the final guard are needed.
Explicit `--samples` is available when the completed frame count is known;
strict acceptance still requires exactly one trailing frame. Every
post-warmup CPU frame, including that final guard, and every presentation
interval remain reported and gated. Cropping a 4,500-frame Dam route at frame
3,720 cannot hide its known late combat cluster around frame 4,307.

The coordinator takes read-only snapshots before launch and after completion:

```sh
python3 scripts/prepare_3ds_benchmark.py snapshot \
  --request build/performance-requests/dam-cold/request.json \
  --installed /path/to/installed/goldeneye-3ds \
  --phase before --output build/performance-requests/dam-cold/before.json
# Coordinator runs the route exclusively, with audible music/SFX and max stereo.
python3 scripts/prepare_3ds_benchmark.py snapshot \
  --request build/performance-requests/dam-cold/request.json \
  --installed /path/to/installed/goldeneye-3ds \
  --phase after --output build/performance-requests/dam-cold/after.json
python3 scripts/prepare_3ds_benchmark.py collect \
  --request build/performance-requests/dam-cold/request.json \
  --before build/performance-requests/dam-cold/before.json \
  --after build/performance-requests/dam-cold/after.json \
  --result /path/to/installed/goldeneye-3ds/dam-input-probe.result \
  --output build/performance-requests/dam-cold/cold.result \
  --run-kind cold --audible-confirmed \
  --observations 'Describe actual reset/settings, audible music/SFX and observed issues'
python3 scripts/analyze_3ds_frame_timing.py \
  build/performance-requests/dam-cold/cold.result --require-benchmark
```

The `before` snapshot refuses a stale result. Both snapshots must match the
request's assets, executable and entire config inventory; the after snapshot
binds the result hash. Collection refuses altered results, reversed snapshot
ordering and output overwrites. It records operator observations, never
automatically asserts that audio was heard or that hardware was used.
Preserve request/snapshots/results/provenance together, and restore the
pre-existing installed config inventory after collection.

`--require-benchmark` applies the exact integer work test
`work_ticks * 60 > clock_hz`, complete retrace/submission checks, completed GPU
queue budget checks, effective audio/stereo checks and verified provenance.
It refuses capture I/O anywhere in the route, including warmup, rather than
dropping diagnostic stalls afterward. Frame start intervals are reported in
exact-clock statistics; normal display-period jitter is not independently
treated as a skipped VBlank. A nonzero exit means the requested acceptance
evidence or measured speed failed; inspect both gates for the reason.

## Coordinator run matrix and remaining physical acceptance

Run the audio-corrected baseline and candidate under the same instrumentation.
Keep the heavy Dam `afed049b` input/cadence lineage and established Dam history
separate. Preserve their original files byte-for-byte. Use cold process starts
and independent repeats, with no concurrent builds, timing tests or other
agent-owned emulator. Run separate deep/PCM pairs against the corrected
baseline; require matching recorded gameplay, cadence, draw/state/frustum
totals and complete music byte-count/hash. PCM/profile overhead is excluded
only by running a separate clean route, never by editing measured times.

The new `scripts/performance-probes/caverns-extended.cfg` keeps the established
seven-segment Caverns route's first 750 frames and extends its final idle
segment to 2,971 frames (3,721 total). Generate new audio-corrected live and
matched-cadence controls for it. Its 3,000-frame historical result cannot
meet the 3,600-frame requirement. This remains a bounded Caverns regression
route, not proof of demanding all-level traversal. Add busy stereo doorways,
streaming, guard combat and first-person weapon changes as representative
routes become reproducible. Report every cold and repeat result, including
failed gates, startup latency and late outliers.

On a physical New 3DS XL, archive exact console/launcher settings, verify
New 3DS speedup and the recorded `music_worker` enabled/fallback state, use
audible speakers or headphones, and keep the slider fully raised. Record
CPU phase tails, completed GPU queue tails, FrameBegin wait, start intervals,
VBlank deltas, NDSP/primary consumption and stage startup separately.
Inspect both eyes and listen throughout busy routes; include prolonged
play, repeated loads and sleep/resume as additional acceptance runs.
Use an external display observation when certifying visible presentation.
These tests and instrumentation do not establish sustained physical 60 fps,
stereo comfort, complete campaign fidelity or subjective audio quality.

## Validation

`scripts/tests/test_3ds_frame_timing_analysis.py` tests legacy compatibility,
integer work thresholds, fractional GPU thresholds, prior-frame association,
guard coverage, missing/invalid rows, full-route tail visibility, recorded
state comparison, and stereo/audio/provenance rejection. The private-request
test exercises clean/PCM configurations, default full-route policy, minimum
window bounds, byte-preserved inputs/cadence, same-size asset corruption,
unknown configs, stale results, pre/post binding and immutable output evidence.
Both are registered in `scripts/test_port.sh`. ARM build and full-regression
results are recorded by the performance task coordinator.

The performance worktree's final `scripts/build_3ds.sh -j4` and complete
`scripts/test_port.sh` both pass on frozen source `a3dd8464`, which includes
the measurement changes, hardened placement-prefix reuse, and audio dependency
`141fe1a4`. Tracked source hashes match before and after both runs. Executable
SHA-256 is `1b3fe1d9d7422597ab302805473373271784b67306627475675b8b3a2b6e3541`;
pack SHA-256 is `51468ccce09d5635ae357f7f562f557368d5fd0e54b0c1f9f08dbc385adb483f`.
Private build/source/log receipts and seven planned clean/PCM/live requests
are in the performance worktree's `build/performance-20260910/final/`.
The requests do not assert an actual run or audible output. The integration
coordinator must regenerate them for the integrated executable and verified
emulator/device environment, including the external emulator profile receipt.
