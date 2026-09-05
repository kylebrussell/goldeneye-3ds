# World submission and actor publication — 2026-09-04

Continues [the audio optimization pass](Verification20260904AudioOptimization.md).
Baseline executable: `525de37c942aa48238c426cdd3b2f9cd261ccce4652c1eea8d1bb7eee06f186a`.
Candidate executable: `6f6d1a923fc469ebba904868310451859b0e00f3bdc0d47d10c4a9b2eca9d9f9`.
Music/SFX, original simulation, portal visibility and hardware modes remain unchanged.

## Retained changes

Once an anchor batch passes the current-frame frustum check, consecutive
batches with byte-identical material, texture and coordinate space can join
its draw without further CPU frustum tests. The same room-membership and
contiguity checks still apply. A different material still requires an exact
frustum rejection before its geometry can sit inside the merged draw range.
There is no sorting or approximate visibility test.

The GPU may receive additional fully clipped trailing triangles under the
same state. Visible triangle order and material assignment remain unchanged;
this saves CPU work rather than reducing visible content or draw quality.
The existing per-frame cache and clipping functions remain unchanged.

Actor components retain their maximum decoded matrix index. Publication
checks that maximum against the current bank once per input instead of
checking every flattened triangle corner. Component topology already owns
immutable matrix indices; bank/count/layout changes still pass the existing
validation paths. Vertex arithmetic, matrix quantization, duplicate-source
reuse and canonical actor updates are unchanged.

Input probes now sample world submission every 16th rendered frame:
`world_submission_detail_ticks` contains visibility, material preparation /
application, draw submission, total world-pass ticks and sample count.
Detailed sampling is disabled during normal gameplay. Its clock calls add
measurement overhead, so the detailed totals are attribution rather than
unmodified-runtime FPS measurements.

## Verification

`scripts/tests/test_renderer_visible_runs.py` compiles the actual world merge
loop at `-O2` with ASan/UBSan. Across 90,197 generated visible anchors, it
compares with the prior walk: next draw boundary remains identical, every
visible batch retains its material/order, and extra trailing geometry must
be fully clipped. Visibility checks fall from 262,055 to 127,930 in this
fixture. It includes material/texture/projection changes, hidden rooms and
noncontiguous ranges. This is not a pixel comparison on hardware.

The existing model-cache fixture passes at `-O2` with ASan/UBSan, including
shared matrix templates, changed-peer publication, topology eviction,
120 shrink/grow/empty/reordered publications, dirty ranges and rejected
output preservation. Log: `build/host-tests/actor-submit-pass/model-max-test.log`.
The new merge test is included in `make test-3ds-port`.

## Rejected experiments

Exact float-hash folding and an additional prepared-material lookup cache
passed correctness tests but did not materially improve Caverns. An early
frustum-plane overflow proof and a bounds-first temporal hint also failed
to produce a useful measured improvement. They were removed; private
sources/results are retained under `build/host-tests/actor-submit-pass/`
and `build/visual-probe/actor-submit-*.result`.

## Dam short trace

The sampled pre-merge run (`actor-submit-detail-dam750.result`) versus the
merge candidate (`actor-submit-merge-dam750.result`) has 750 original ticks
in each run, with full music and SFX. This candidate preceded only the final
change that disables detailed sampling outside input probes.

- CPU frustum tests: 324,491 → 108,246.
- Sampled visibility ticks: 17,043,841 → 7,133,532 (58% lower).
- Total world-pass ticks: 378,721,437 → 295,193,246 (22% lower).
- Actor vertex-publication ticks: 19,011,280 → 15,745,843 (17% lower).
- Total measured work: 9,067 → 8,740 ms; >16 ms: 79 → 67 / 630.
- Warm peak: 29 → 25 ms; candidate has no warm frame above 25 ms.

The simulation consumes real retrace timing; encounters and endpoints can
change with CPU cost. These Dam runs have slightly different draw workloads,
so the percentages are measured region costs, not an identical-workload
whole-game speedup. A finite probe completing is not mission completion.

## Caverns short trace

The final candidate (`actor-submit-final-caverns750.result`) and audio
baseline (`actor-submit-baseline-caverns750.result`) complete the same 750
simulation ticks, with identical final position, 21 sound starts/decodes,
zero decode failures and healthy actor status. Main music is track 26 at
volume 26,212. Neither run completes the mission.

| Counter | Baseline | Candidate |
| --- | ---: | ---: |
| CPU frustum tests | 168,946 | 65,546 |
| World-pass ticks | 264,651,102 | 246,523,729 |
| Actor vertex-publication ticks | 76,720,048 | 61,750,554 |
| Total measured work (ms) | 7,219 | 7,071 |
| Post-warmup frames >16 ms | 46 / 630 | 32 / 630 |
| Post-warmup peak (ms) | 19 | 19 |
| World draw calls | 44,863 | 44,863 |

World submission falls about 7%, actor vertex publication about 20%, and
total measured work about 2%. Candidate sampled visibility/material/draw/
world totals are `4422739,3234457,8965390,24444516` ticks over 47 samples.
The candidate includes sampled timing overhead absent from this baseline.
Authored-batch counts increase from 141,810 to 148,263 because compatible
fully clipped trailing batches now count as submitted; visible draw calls
remain identical. These are single emulator runs, not hardware FPS claims.

## Extended Dam combat

The final candidate runs the existing `dam-end-to-end-lifetime-audio.cfg`
route with original music and SFX. `actor-submit-final-dam-combat.result`
ends in Bond's death at target 14/160, after 5,726 presentations and 5,727
simulation ticks. This is a failed mission, not a crash or completed route.
Actor status is healthy, scene-query/matrix and overlay-refresh failures
remain zero, and 689 sound starts/decodes have zero decode failures. NDSP
remains active and original music switches to death track 27, volume 31,127.

| Counter | Audio baseline | Candidate |
| --- | ---: | ---: |
| Presentations | 5,040 | 5,726 |
| Total measured work (ms) | 72,759 | 74,026 |
| Mean measured work per presentation (ms) | 14.44 | 12.93 |
| Post-warmup frames >16 ms | 1,680 / 4,920 (34.1%) | 1,118 / 5,606 (19.9%) |
| Post-warmup frames >25 ms | 19 | 46 |
| Post-warmup frames >33 ms | 1 | 1 |
| Post-warmup peak (ms) | 36 | 35 |

Baseline evidence: `fps-audio-final-dam-combat.result`. The route runs longer
and encounters differ, so this is not a controlled before/after benchmark.
The >25 ms tail did not improve in this run. Live candidate captures showed
60 and 59 FPS at two locations; this does not establish sustained 60 FPS.
No builds or host regression tests ran concurrently with these probes.

Remaining work includes long-tail simulation/actor costs and draw submission,
full-stage visual comparison, and measurements on physical New 3DS XL and
original 3DS. The shared runtime and original hardware mode selection are
retained; neither hardware FPS target has been certified.

## Build and staging

- Final ARM build passed: `build/host-tests/actor-submit-pass/arm-final.log`.
- `make test-3ds-port` passed: `build/host-tests/actor-submit-pass/full-suite.log`.
  The initial run found an obsolete source-name assertion after the profiling
  wrapper was introduced. Both related structural tests now follow the wrapper
  and retain their checks of exact clipping and projection/room ordering.
- `git diff --check` passed.
- Final candidate is installed in Azahar and staged at
  `build/3ds-sd/3ds/goldeneye-3ds/goldeneye-3ds.3dsx`, with a preserved copy in
  `build/3ds-candidates/visible-runs-6f6d1a92/`. The previous executable remains
  in `build/3ds-candidates/audio-525de37c/`.
- `deploy_3ds.sh --skip-build` validated the staged executable, metadata and
  unchanged asset pack. Log: `build/host-tests/actor-submit-pass/staging.log`.
  Asset SHA256: `ee769251742b72bcaa9a3d1586794246355bc995dc62637e9755dc276adfdeb7`.
- Temporary input/stage overrides were moved to
  `build/visual-probe/actor-submit-final-used-*.cfg`. Normal startup reached
  the original intro/cast sequence, then emulation was stopped through the UI.
  Saves and DSP files were not manually modified.

This is a validated SD staging tree, not a deployment or FPS verification on
physical hardware. No commit or push was made.
