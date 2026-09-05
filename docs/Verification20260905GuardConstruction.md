# Guard construction and exact segment publication — 5 September 2026

Candidate `e1d96c241985b2e18ff5e10173ea121d3a7a8e0143a647ef2131ba5f0f28798f` builds on verified `4cfeecee74683f887bb52e3a1be845f76dcaa930450ef581d57962efbe1adcf1`. All builds use the corrected `make -B` wrapper. The retained production candidate reproduces exactly after restoring the diagnostic source/header changes and rebuilding the complete ARM target. The earlier `203bdf49` baseline describes the preceding optimization pass; this pass compares against `4cfeecee` exclusively.

## Measured target

The corrected early heavy-Dam slow cluster spent 1.9569 ms in guard-scene construction/publication. Temporary, opt-in diagnostics now separate input collection, cache topology, publication signatures, matrix quantization, vertex/static copies, batch publication, commit and GPU upload. Frame-aligned comparison uses the same 253 baseline slow-frame IDs and reports the early subset separately. The capture's gameplay/draw/frustum state and complete PCM hash match.

| Early slow-cluster diagnostic phase | Baseline ms | Candidate ms |
| --- | ---: | ---: |
| Input collection | 0.6680 | 0.3024 |
| Model cache total | 1.7148 | 1.5939 |
| Topology | 0.0598 | 0.0597 |
| Publication signatures | 0.2151 | 0.2150 |
| Matrix quantization | 0.2673 | 0.2667 |
| Vertex copy/transform block | 0.8896 | 0.7683 |
| Static-copy sub-block | 0.0788 | 0.0798 |
| Batch publication | 0.0839 | 0.0830 |
| Scene commit | 0.0154 | 0.0152 |
| GPU upload | 0.1488 | 0.1466 |
| Topology replacement | 0 | 0 |

These are instrumented, nested times, not production frame times. In particular, the static-copy sub-block includes per-input clock overhead even when retained storage avoids the actual copy. The cache total includes its children; they must not be added again. Animated guard batches do not rebuild world AABBs in this path: their upload invalidates retained bounds and uses the original vertex-based test. Original UV/color data and topology ownership remain intact.

## Retained changes

`cache_publish_segment_vertices` selects the common immutable, identity-outer-transform path once per input. It reuses the existing exact XYZ/matrix duplicate map, calls the unchanged scalar quantized joint transform for unique vertices, and publishes exactly the same eye/world fields. The general path remains for mutable segment-4 vertices and outer placement. It adds no retained buffers, new signatures, affine assumptions, animation reuse or new invalidation state. Static source, color, texture, normal, clip and padding bytes remain owned by the existing publication logic.

Character input enumeration now checks whether a node is a display-list node before searching the body/head resource lists. Non-draw nodes could never return a scene part, so those two linear searches added no information. Relation updates, every node traversal, ordering and resource membership lookup for draw nodes remain unchanged. Count-only queries no longer construct unused descriptors. This is not the previously rejected persistent node-index experiment: it retains no index or state between calls.

The isolated segment-path trial reduced heavy Dam from 253 to 235 misses, mean 12.2190→12.1347 ms and p95 16.9282→16.7905 ms. Adding the enumeration filter produced 180 misses, mean 11.9199 ms and p95 16.3972 ms in the first combined run. That initial combined build (`9affeecf`) repeated the same result. Final candidate `e1d96c24` additionally guards the zero-vertex call site so empty display lists never form a pointer from null storage. A synthetic end-only list tests changed identity inputs with null buffers. Final repeated measurements follow below.

The distinct frame-185 transaction is preserved. In the baseline diagnostic capture it includes 1.3979 ms collection, 3.1722 ms model-cache work (1.2858 ms within the static-copy block), 2.1480 ms upload and 1.5055 ms replacement. Collection and vertex-path savings may help that frame, but this pass removes no replacement copies, alters no capacity or ownership behavior, and introduces no transaction shortcut.

## Exactness and validation method

`test_model_segment_publication.py` executes the actual helper against the original scalar/copy behavior for 377,724 vertices, including 250,816 duplicate copies, arbitrary quantized joint matrices, signed zeros, empty spans and full-buffer sentinels. Every byte of the complete destination buffers matches under ASan/UBSan, including fields the helper must leave untouched.

The existing model-scene regression exercises authentic PP7, window and gate geometry, two million-plus reused vertices, 40 original components, sparse/merged dirty ranges, changed inputs, retained topology, buffer changes, failure paths and 120 exact-size shrink/grow/empty transitions. It passes with the new path.

The character-provider test covers 71 original models, 3,031 nodes and 1,004 display lists. Bulk descriptors now compare every field, including node and model type, with the unchanged scalar walk. An extra scalar lookup independently verifies the final count, and short-buffer tests check partial output and unchanged failure counts. These tests pass under ASan/UBSan.

Normal timing comparisons restart Azahar, disable deep/world profiling and run without concurrent builds or tests. Warmup remains 120 frames. Input, retrace history and asset pack are unchanged; collection waits for the final footer and verifies actual frame counts and provenance. Matching gameplay/checkpoints and draw-call/state/frustum totals are required independently of timing. Exact PCM comparisons use separate captures, and a final natural-scheduler run is reported independently rather than treating a different combat history as an isolated speedup.

Primary heavy cadence remains `afed049bb9d8d6a57dab39a64f22b85bdb2218b9f473855ba789f7424d5429ab`; established Dam is `b731bbc5ddd95515de21c950cb67c41796aab8ee21607bd81332b8ee4f573bca`; Caverns is `7662e338e4dd3d17556b84e1cec7b8e43493259f8922e7d7458d33b2c4d57cdd`. Pack SHA-256 remains `7271d1db02833c29ce4b28111cfc3e56f6a03c2b5f9ae1510593669b9f61cf49`. Baselines, source snapshots, diagnostic rows, isolated trial, exactness logs and final captures are preserved in `build/host-tests/guard-phases/`.

The final full regression suite passes (exit status zero), including the empty-list case. A test-only initializer typo was corrected before the successful run; no assertions were removed. Two full ARM rebuilds match `e1d96c24` exactly. Temporary diagnostic fields and clocks are absent from production main, the model-cache header and the guard-runtime source. Final timing/PCM/live/staging captures are recorded below.


## Remaining production costs

On the same 253 baseline slow-frame IDs, normal work falls from 17.8170 to 17.3130 ms; guard construction falls from 1.8419 to 1.3678 ms. The fixed early subset (186 frames, IDs 251–713) falls from 17.8757 to 17.3608 ms overall and 1.9569 to 1.4477 ms in guard construction. This approximately 0.51 ms reduction agrees with the instrumented collection/vertex-path findings. The diagnostic candidate predates only the final zero-length call guard; final production performance and PCM are checked separately.

The final executable's remaining 183 slow frames average 17.6596 ms work. Their canonical tick averages 8.8596 ms, containing music at 4.4052 ms and props at 2.9071 ms; rendering averages 4.3434 ms, containing world at 3.3701 ms. Guard construction is now 1.3899 ms, upload 0.2524 ms and first-person work 0.7501 ms. Nested categories are not independent costs. These measurements make audio synthesis and canonical props stronger next targets than another topology-transaction shortcut.

Frame 185 remains a separate 20.7216 ms peak, down from 21.5476 ms. Upload remains 2.1479 ms; its included replacement work is 1.5055 ms. The normal guard-scene timer does not include all transaction work on this frame, so its 0.5397 ms must not be equated to the deeply instrumented cache total. This pass preserves transaction ownership, failure behavior and replacement copies.

## Final production timing

| Run | Mean ms | p95 ms | p99 ms | Peak ms | Misses |
| --- | ---: | ---: | ---: | ---: | ---: |
| stress-baseline | 12.2190 | 16.9282 | 18.4391 | 21.5476 | 253 |
| stress-final | 11.9243 | 16.4104 | 17.9370 | 20.7216 | 183 |
| stress-final-repeat | 11.9243 | 16.4103 | 17.9370 | 20.7216 | 183 |
| dam-baseline | 10.1861 | 15.6197 | 18.3282 | 27.1910 | 157 |
| dam-final | 10.0588 | 15.1590 | 17.7745 | 26.2590 | 154 |
| dam-final-repeat | 10.0589 | 15.1590 | 17.7745 | 26.2589 | 154 |
| caverns-baseline | 7.3954 | 12.0489 | 13.3633 | 15.3811 | 0 |
| caverns-final | 7.2460 | 11.8106 | 12.8847 | 15.0230 | 0 |
| caverns-final-repeat | 7.2460 | 11.8105 | 12.8848 | 15.0229 | 0 |

Every row has zero skipped or repeated submission intervals. Heavy Dam has 4,380 measured frames and 4,379 intervals, established Dam has 4,250 / 4,249, and Caverns 2,880 / 2,879. All six final runs match their baseline's recorded gameplay, draw state and frustum totals, with no runtime guard texture imports. Heavy Dam retains 32 PP7 shots, three damaging guard hits and 828 guard-fire dispatches; established Dam retains 26, two and 1,174 respectively. Each completes all authored targets, records one opening-guard death and player damage, and has zero unresolved AI opcodes.

The final bounded variant has 183 heavy misses in each repeat, rather than the intermediate build's 180. That is a 70-frame (27.7%) improvement over the corrected 253-miss reference. The 4,380-frame run still exceeds the work budget on 4.18% of measured frames. Its zero skipped presentation intervals do not turn those overruns into a hardware 60 fps guarantee.

## Final audio

All three separate final-binary deep captures pass the gameplay/draw gate and match the baseline PCM hash and byte count exactly:

- stress: `51de3d1a25a3c0fd,6624000`.
- dam: `1d505126fa091a5c,6435584`.
- caverns: `3fb892d7b6dacc8c,4416000`.

Instrumented audio runs are correctness evidence; their extra diagnostics are excluded from the normal timing table.

## Independent live scheduling and staging

The final normal-scheduler run completes all 11 authored targets in 4,468 displayed frames / 4,469 simulation ticks. It records 29 PP7 shots, three damaging hits, 1,187 guard-fire dispatches, one opening-guard death, player damage and Bond's death, with no unresolved AI opcodes. The verifier passes. Its 4,348 post-warmup samples have 177 work-budget misses, mean 10.2202 ms, p95 15.9141 ms, p99 17.7151 ms and peak 21.9255 ms. All 4,347 measured submission intervals have no skipped or repeated vblank. The generated cadence is preserved as `dam-final-live.cadence`, SHA-256 `1074b03f932523230d0388d02a3777fadbe0cde2d713b863453b3b53e6890da2`. This independent run is not used to replace the heavy `afed049b` workload or as an isolated speedup claim.

Candidate archive: `build/3ds-candidates/guard-publication-e1d96c24/`. It preserves the binary, unchanged pack/catalog, source patch and changed-source archive, complete performance evidence, report and forced-build/toolchain/source provenance. Local SD and Azahar staging use the same binary and pack. Physical hardware deployment, hardware 60 fps, exhaustive visual parity and full-campaign mission acceptance remain outside this evidence.

The original configuration inventory and bytes are restored. CUA screenshots confirm the intro circle fade and animated Boris/Xenia cast pages in normal startup. This is a startup smoke check, not a full frontend visual comparison. Azahar is stopped after the check; temporary wake protection remains active.

## Follow-up audio experiment: rejected

The largest remaining nested cost motivated a shared 16-bit audio-load trial (`e2dd3353`). Replacing the explicit big-endian byte expression with an exact two-byte `memcpy` and endian conversion reduced the ARM execute function from 2,391 to 2,353 instructions and folded more signed conversions into `revsh`. The baseline compiler already combines most loads; this was not a broad removal of byte accesses. Exhaustive native/portable load-store checks and original audio producer tests passed.

Two normal heavy runs each measured 11.9240 ms mean versus 11.9243, p95 16.4108 versus 16.4103 and 182 versus 183 misses. The approximately 0.0003 ms average change is not a material performance gain. The trial was rejected, its source/binary/assembly/results preserved in `build/host-tests/audio-loads/`, and the original sample loader restored. No final PCM or campaign claims are made for that rejected binary. The additional test coverage remains: all 65,536 values at 15 offsets are read independently from reference-encoded bytes, with signed/unsigned checks, read-only buffer checks and a million ordered overlapping load/store operations in native and portable modes.

A further full `make -B` rebuild after reverting the audio experiment reproduces `e1d96c24` exactly. Expanded load/store tests also pass against the restored source. The final staged executable therefore remains the binary that passed the full suite and all final timing/PCM/live checks above.
