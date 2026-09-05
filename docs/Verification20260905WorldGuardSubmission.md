# Exact draw sequences, shading and reproducible builds — 5 September 2026

The final source candidate is `4cfeecee74683f887bb52e3a1be845f76dcaa930450ef581d57962efbe1adcf1`. The comparable, fully rebuilt baseline is `203bdf493ee299c818a9b18c562b9529fe1170f8364f6bd1876e20b837ba9469`. The heavier `afed049b` Dam history remains the primary workload. Its corrected normal candidate repeats have 253–256 budget misses against the rebuilt baseline's 313. One candidate run has one skipped interval; the other has zero. This is repeatable CPU work saved, not locked 60 fps.

## Reproducibility defect found and fixed

The initial pass measured 317→254 heavy Dam misses on incremental builds (`cd98cb4b`→`acafeb54`) and passed its host exactness checks, gameplay/draw gates, control repeats and audio checks. A subsequent visibility experiment forced its object to rebuild. Restoring the unchanged source then produced a different executable, exposing a stale object from a previously rejected early-bounds experiment.

That stale implementation expected an extra `safe_world_limit` field beyond the current `GeDrawBatchClipContext` layout. The old machine-code prefix is present in both initial executables and absent from the corrected pair. Timestamp-preserving restoration had left the experimental object newer than its restored source/header. Rebuilding only main was insufficient to establish source/binary reproducibility. The former binary's extra-field accesses violate the current layout; successful gameplay telemetry did not detect this ABI defect.

`scripts/build_3ds.sh` now passes `make -B`, rebuilding every object for probe and release builds. This intentionally trades incremental build time for a consistent source/header/object set. Shell syntax and argument forwarding were checked, and the entire target was rebuilt twice. Both forced builds reproduce `4cfeecee` exactly, also matching the individually corrected build. The baseline was rebuilt with the current visibility source/header and without the two new rendering optimizations, retaining the prior audio and single-GPU-command improvements.

Initial captures are preserved as historical evidence under `build/host-tests/world-guard/`, with `incremental-verification.md` and `reproducibility-audit.json`. They are not the final candidate's acceptance measurements. Earlier archive hashes identify real measured binaries, but do not establish a fully rebuilt checkout. This report supersedes those claims where they relied on incremental reproduction.

## Retained rendering changes

Separate temporary timers selected repeated glass shading and world submission work while distinguishing guard topology-publication spikes. No gameplay, audio, geometry, visibility rule or authored GPU command is relaxed.

1. `ge_3ds_shade_cache.h` reuses eight shading results within one immutable batch shading state. It keys authored RGBA/normal bytes and, when necessary, authored ST; generated ST replaces that dependency. Every batch and every door matrix transition resets the scratch cache. It copies only shader-written normal, texture, RGBA and generated-ST fields, preserving each vertex's position, clip and other bytes. No result survives a frame or camera/light-state change.
2. `ge_3ds_draw_arrays.c` directly writes the same 22 words for the eleven commands emitted by the pinned Citro3D DrawArrays function. It calls the original context updater first, retains original command order, primitive restart and vertex-cache clear, then sets the original DrawUsed flag. If the sequence will not fit, individual GPUCMD calls retain the original partial stream and panic point. There is no all-or-none replacement and no GPU command deletion. The prior exact single-command wrapper still handles other calls. The retained upstream oracle is [Citro3D 1.7.1 drawArrays.c](https://raw.githubusercontent.com/devkitPro/citro3d/v1.7.1/source/drawArrays.c).

The SDK dependency is explicit: the Makefile requires archive SHA-256 `3f34eff859c1f1e60ebe597b6df9cf5e5e74563d9e64ca01a7b3292fde63016a`. The private context declaration, original source and license are vendored unmodified, and a target static assertion checks the flags offset. Changing the SDK archive stops the build until this contract is reverified; an intentionally invalid hash was tested and rejected. This is a maintenance constraint, not a portable public-API replacement. See the pinned [context declaration](https://raw.githubusercontent.com/devkitPro/citro3d/v1.7.1/source/internal.h) and [license](https://raw.githubusercontent.com/devkitPro/citro3d/v1.7.1/LICENSE). The original updater has independent uniform, lighting and other state work, so it remains unconditional; examining its flags alone is not a sufficient reason to bypass it. See [updater source](https://raw.githubusercontent.com/devkitPro/citro3d/v1.7.1/source/base.c).

## Exactness gates

`test_3ds_draw_sequence.py` compares actual production encoding with retained upstream source for 50,000 complete buffers, offsets, context calls and flags, including 19,224 valid sequences and 30,776 matching partial/panic outcomes. It exercises variable context output, NULL buffers, exact capacity boundaries, dirty initial flags and full-width draw parameters under ASan/UBSan. The updater is modeled for those buffer tests; the linked production updater itself remains unchanged. Upstream oracle, header and license hashes are independently asserted.

`test_3ds_shade_cache.py` compares all processed-vertex bytes for 288,000 vertices, with 70,636 scalar calls avoided. Coverage includes lighting, normals, generated/authored ST, cache hits/eviction, changed matrix/light/look-at states, reset boundaries, unlit fields and invalid states. Source inputs are checked unchanged. The existing extracted windowed-door test now includes the actual cache header and still passes its 256 topology rebases, ownership and bounds cases. No assertions were removed. The full `scripts/test_port.sh` suite passes after correcting that extracted test's missing include path.


The final full `scripts/test_port.sh` run after restoration passes with exit status zero (`final-full-regression.status.json`). Final ARM disassembly verifies one original context-updater call, direct command-word stores, the individual-command fallback and the original DrawUsed flags write. The vendored SDK ABI and negative archive gate remain part of the build contract.

## Controlled measurements

All normal probes restart Azahar and run without concurrent builds/tests, with deep/world diagnostics disabled and the unchanged 120-frame warmup. Collection waits for the complete serialization footer, verifies actual frame counts, and records binary, input, cadence, stage and pack hashes. Recorded gameplay, draw-call/state/frustum totals and runtime guard texture imports are checked separately from timing. Matching telemetry is bounded evidence, not a proof of every intermediate state or pixel.

Asset pack: `7271d1db02833c29ce4b28111cfc3e56f6a03c2b5f9ae1510593669b9f61cf49`. Heavy Dam cadence: `afed049bb9d8d6a57dab39a64f22b85bdb2218b9f473855ba789f7424d5429ab`. Established Dam: `b731bbc5ddd95515de21c950cb67c41796aab8ee21607bd81332b8ee4f573bca`. Caverns: `7662e338e4dd3d17556b84e1cec7b8e43493259f8922e7d7458d33b2c4d57cdd`. Exact inputs and provenance are archived with the evidence.

| Workload/build | Mean ms | p95 ms | p99 ms | Peak ms | Budget misses | Skipped intervals |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Heavy Dam baseline | 12.4893 | 17.2659 | 18.7889 | 22.0235 | 313 | 0 |
| Heavy Dam candidate first | 12.2190 | 16.9316 | 18.4342 | 21.6305 | 256 | 1 |
| Heavy Dam candidate repeat | 12.2190 | 16.9282 | 18.4391 | 21.5476 | 253 | 0 |
| Established Dam baseline | 10.3753 | 16.0126 | 18.7169 | 27.5760 | 180 | 0 |
| Established Dam candidate first | 10.1862 | 15.6196 | 18.3283 | 27.1911 | 157 | 0 |
| Established Dam candidate repeat | 10.1861 | 15.6197 | 18.3282 | 27.1910 | 157 | 0 |
| Caverns baseline | 7.4238 | 12.1001 | 13.5062 | 15.4360 | 0 | 0 |
| Caverns candidate first | 7.3954 | 12.0489 | 13.3633 | 15.3811 | 0 | 0 |
| Caverns candidate repeat | 7.3954 | 12.0489 | 13.3633 | 15.3811 | 0 | 0 |

Heavy Dam has 4,380 post-warmup samples / 4,379 intervals; established Dam 4,250 / 4,249; Caverns 2,880 / 2,879. All six candidate runs match recorded gameplay and draw-call/state/frustum totals, with zero repeated intervals and zero runtime guard texture imports. Both Dam histories pass the authored combat verifier. Heavy Dam retains 32 shots, three hits, 828 guard-fire dispatches and Bond alive at 0.5 health; established Dam retains 26 shots, two hits and 1,174 dispatches. Each records one opening-guard death and no unresolved AI opcodes.

## Corrected executable PCM checks

All three final PCM comparisons pass along with recorded gameplay and draw-call/state/frustum totals:

| Workload | PCM hash | Bytes |
| --- | --- | ---: |
| Heavy Dam | `51de3d1a25a3c0fd` | 6624000 |
| Established Dam | `1d505126fa091a5c` | 6435584 |
| Caverns | `3fb892d7b6dacc8c` | 4416000 |

These use the final production executable with diagnostic flags enabled only for capture. They compare output to the preserved PCM oracles. The heavy oracle uses a different temporary profiling layout; its timing is not a performance comparator. All reported normal performance uses diagnostics-off runs of the fully rebuilt pair.

## Follow-up experiment and next measured bottleneck

The remaining world-visibility cost motivated explicit fixed-axis expansion of the existing common-plane loop. One million differential outcode cases and existing clip-boundary, coefficient-sign, overflow and containment tests passed. After correcting the stale object, the experiment and retained code have the same heavy-route mean 12.2190 ms, p95 16.9282 ms, 253 misses and zero skipped intervals. It provides no measured gain and is reverted; original visibility source/header bytes are retained. Its test, source variant and executable remain only in the evidence directory.

On the corrected candidate repeat, 186 remaining misses occur in frames 251–713 and 20 at/after frame 4307. For the early selected frames, work falls 18.2552→17.8757 ms and renderer work 4.7961→4.4587 ms. Music remains about 4.362 ms, guard-scene work 1.957 ms and props 2.837 ms. Late selected frames average 17.3310 ms, including music 5.3348 ms. Music is nested within canonical ticking and world is nested within rendering; these parents/children must not be summed twice.

The next sustained renderer-side bottleneck is guard-scene construction/publication: the early slow cluster spends about 1.96 ms there every measured frame, independently of the infrequent topology-replacement spike. Music synthesis is the larger cross-system cost at 4.36–5.33 ms on these clusters. Future work should isolate the guard transform/shade/publication children using the corrected build, and select a repeated operation with exact geometry tests; it should not infer that another persistent visibility cache or prewarm will solve the tail.

Frame 185 remains distinct: corrected repeat work is 21.5476 ms, with guard upload 2.1479 ms and replacement 1.5497 ms. The earlier detailed capture traced temporary replacement-buffer and topology-cache work, while the corrected production path still uses the same transaction. Scene publication capacity is already reserved; this is not evidence for missing texture prewarming. A replacement optimization must preserve failure/ownership behavior. No spike-specific transaction change is retained here.

## Final independent live scheduler

The corrected production candidate runs without replay, detail or world-profile configuration. It reaches all 11 route targets in 4,468 displayed frames / 4,469 simulation frames. The authored combat verifier passes: 29 PP7 shots, three damaging hits, 1,187 guard-fire dispatches, one opening-guard death and no unresolved AI opcodes. Bond dies in this harness. This is not a survived mission or mission-completion claim.

The 4,348 post-warmup work samples average 10.3600 ms, with p95 16.4070, p99 18.2294 and peak 22.3743 ms. There are 199 budget misses and zero skipped or repeated intervals among 4,347 measured presentation intervals. The cadence is preserved as `dam-clean-live.cadence`, SHA-256 `1074b03f932523230d0388d02a3777fadbe0cde2d713b863453b3b53e6890da2`, padded only to the original unused 4,500-frame capacity. Its different combat history makes its raw miss count unsuitable for an isolated speedup comparison. The heavier `afed049b` history remains the primary benchmark.

## Staging and limits

Candidate `4cfeecee` is packaged under `build/3ds-candidates/draw-sequence-4cfeecee/` with the unchanged pack, executable, source patch/snapshot, restored visibility source/header, tests, complete captures, rejected experiments, build/source hashes and toolchain provenance. Local SD and Azahar staging use that same binary and pack. No physical-console deployment, commit or push is performed.

The original launch-configuration inventory and bytes were restored. CUA-observed normal startup progressed through the gunbarrel sequence to the General Arkady Ourumov cast presentation; this verifies startup progression only. Azahar was stopped afterward, and Mac wake protection remains active. Final staged hashes and archive contents are verified, and `git diff --check` passes. Dam still exceeds its work budget, and one corrected heavy repeat skips an interval. These bounded emulator checks do not establish sustained live 60 fps, physical New/Old 3DS performance, full campaign coverage, mission correctness or N64 pixel parity. The guarded private SDK dependency must be reverified before an SDK upgrade.
