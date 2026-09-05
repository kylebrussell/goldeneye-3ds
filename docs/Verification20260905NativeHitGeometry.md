# Native hit geometry and allocation-dependent gameplay — 5 September 2026

This correctness pass grew out of an attempt to move the music interpreter onto New 3DS core 2. The accepted incoming baseline is `e1d96c24`; experimental work and complete intermediate captures are preserved in `build/host-tests/canonical-costs/`. The old baseline remains archived separately.

## Finding

Adding sixteen unused bytes to `GeOriginalMusicRuntime`, with its original synchronous implementation and original main loop, changes a Dam bullet hit. This rules out the audio worker and runtime handoff as the cause of that divergence. Frame-by-frame diagnostic builds preserve the original replay until frame 856. At that frame, player position and view are identical, but bullet-hit handling consumes 32 random calls in the old layout and 31 in the padded layout.

Tracing the collision inputs identifies the assigned KF7 (`PchrkalashZ`). Its display lists use segment-5 vertex addresses beginning at blob offset `0x100`. The native provider already relocates that 100-vertex block into a separate array. The old hit adapter then adds the authored blob offset to that array again. It begins at the wrong vertices and eventually reads beyond the array. The resulting fake triangles depend on neighboring allocations. The captured ray hits one of those triangles in the original layout, producing a weapon hit; padding changes the surrounding bytes and removes that false hit.

This is a real correctness defect, not acceptable replay variability. The old combat history must remain evidence of the defect, not the expected result for a corrected collision implementation. Performance changes after this repair require a newly measured, corrected baseline and matching gameplay on that baseline. Do not claim savings by comparing the changed combat workload to `e1d96c24`.

## Repair

The native Pitem provider exposes the authored origin of its relocated vertex arrays. The collision adapter translates segment-5 byte offsets relative to that origin, while segment-4 addresses remain relative to the current vertex array. Encoded N64 vertices always use a 16-byte stride; native indexing uses `sizeof(Vertex)`, including the wider host representation.

Every vertex load and triangle index is checked before forming a native pointer. Unknown segments, missing segment-5 ownership, misaligned offsets, invalid loads and out-of-range references fail safely. Repeated-index triangles used as TRI4 padding are skipped as zero-area triangles; they cannot discard a previously valid hit merely because their unused slot lies outside the last load. Real triangle order, intersection arithmetic, hit positions, normals and durable vertex pointers remain canonical.

## Evidence

- An unused-padding-only binary reproduces the worker's original divergence with unchanged music logic.
- Dense checkpoints identify frame 856 as the first RNG/guard-state difference. Caller traces identify weapon versus flesh impact handling.
- Geometry traces show identical rays and valid initial vertices, followed by allocation-dependent out-of-bounds coordinates. The authored KF7's first vertex is at `0x100`; the old adapter starts reading at the equivalent of `0x200`.
- The corrected normal and padded builds match all 17 recorded checkpoints and draw/state/frustum totals in the 1,000-frame diagnostic prefix. Prefix runs intentionally end before the 11-target route is complete and are not performance acceptance runs.
- The focused test runs the actual generated collision body and canonical triangle routine with both 16-byte and 24-byte native vertices under ASan/UBSan. It covers segment origins, partial vertex-cache slots, TRI1/TRI4, empty padding, malformed addresses and durable hit pointers.
- An independent triangle-stream decoder compares 10,000 rays against the authored KF7 mesh for each native vertex size. It also checks the captured first divergent ray: the correct result is a weapon miss.
- The Pitem integration test verifies the real KF7 origin (`0x100`), vertex count (100) and rejection of an interior/unowned array pointer.
- The complete collision and final worker regression suites pass, including final padding-triangle coverage. Two forced final ARM builds reproduce `4446f3c8`. Both release repetitions of heavy Dam, established Dam and Caverns pass their corrected synchronous gameplay/draw gates.

## Larger optimization work

Earlier worker, immediate-join and worker-disabled captures preserve complete music PCM but encounter the same collision defect; their timings are not acceptance evidence. The corrected baseline is `5cc5a141`. Final worker `4446f3c8` is measured against that repaired baseline in [the music-worker acceptance report](Verification20260905MusicWorker.md). Heavy Dam falls from 166 budget misses to nine and six in the two final runs, with matching recorded gameplay and rendering fields.

The higher-cost opportunities remain music synthesis, repeated character/prop preparation and CPU world submission. Prior normal profiling places music at approximately 4.4 ms on the old sustained slow frames, props at 2.9 ms and world rendering at 3.4 ms (nested scopes, not independent totals). Those figures prioritize architectural work; they are not promised savings. Physical New 3DS XL profiling and Old 3DS fallback validation remain necessary for hardware performance claims.
