# New 3DS music interpreter worker — 5 September 2026

Release candidate `4446f3c8aa6eb993739dba371b44f7eb086be64d8856e9f85c915582fb45ed78` combines the native collision repair with a New 3DS core-2 audio interpreter. Two forced complete ARM builds reproduce this hash. The final full port regression suite passes; initial integration required updating the frame-order test to assert the split producer/completion boundary explicitly.

The synchronous comparison binary is `5cc5a141afbf75282bc832b81bc95cf38725f6ea414b105260aeea9f6a02ac82`. It includes the same collision repair. The previous `e1d96c24` heavy-Dam combat history is not used as an optimization baseline: [the native hit-geometry report](Verification20260905NativeHitGeometry.md) explains the doubled vertex offset and why it changed bullet hits when the music allocation grew.

## Ownership and timing

The main thread retains libaudio event processing and `alAudioFrame` command generation at the canonical point before MoveBond/props. Only the ABI interpreter and its private music-ring write run on the worker. Commands, DSP state, sample buffers and the ring remain runtime-owned. One job can be outstanding. Completion occurs before the canonical tick closes; the platform audio pump cannot consume the ring concurrently. Layer changes and runtime close also drain pending work before mutating or freeing producer/interpreter state.

The worker is attempted on New 3DS core 2 with a 32 KiB stack. A failed hardware query, original 3DS hardware or denied thread creation leaves execution synchronous. Frontend render/tick callers remain synchronous. The thread is joined and freed during final cleanup; stage music runtimes may be replaced safely while it is idle.

`music_timing_scope=main_prepare_and_join` explicitly identifies the release's music timing as main-thread preparation plus completion wait. Separate worker counters report submissions and aggregate execution/wait ticks. Worker execution overlaps the canonical tick and must not be added to whole-frame work. The whole-frame start/end timing and comparison gate are unchanged. The analyzer retains the scope distinction; older synchronous results default to `main_synchronous`.

## Tests

- The production worker runs 12,000 borrowed-job handoffs and 12 in-flight shutdowns under ASan/UBSan, then repeats under TSan. Output words, one execution per job, repeated shutdown, original-hardware fallback and thread-creation failure are checked.
- Tests execute the actual runtime begin/render/finish/tick bodies with delayed and failing completion, queue pressure, command bounds, exact 60 Hz phase wrap, synchronous fallback and idempotent completion. Producer execution stays on the caller; no borrowed commands or output are changed before completion.
- The stage-order test preserves mission AI, shuffle, producer, MoveBond, props and autoaim ordering, and asserts completion before the canonical tick ends.
- Collision regressions cover 238,952 synthetic rays per native vertex size, 10,000 independent authored-KF7 mesh comparisons per size and the captured divergent ray. Both 16-byte and 24-byte vertices pass ASan/UBSan.
- Complete regression/build logs, source snapshots and every capture are in `build/host-tests/canonical-costs/`.

## Matched release measurements

On the corrected heavy-Dam history (`afed049b`), synchronous work averages 11.2374 ms, p95 16.1463 ms, p99 17.8580 ms and peak 22.9490 ms, with 166 of 4,380 measured frames over 16.667 ms. The final worker release averages 9.8991 ms, p95 13.3944 ms, p99 15.3601 ms and peak 20.1236 ms, with nine misses. Both have zero skipped/repeated presentation intervals and matching recorded gameplay, draw calls/state/frustum totals, and no runtime guard texture imports. The earlier worker trial reproduces the nine misses.

This is a substantial improvement on that emulator workload, not a locked-60 result. The residual peaks include first-person model work and geometry uploads. Physical New 3DS XL CPU/GPU timing, actual launcher core permission, sustained audio stability and original 3DS performance remain to be measured. Completed audio captures, fallback, live scheduling and staging are recorded below.


All normal captures use 120 warmup frames, the same input and recorded retrace cadence, deep/world profiling disabled, the same asset pack, and cold emulator starts. Both release runs on every route pass recorded gameplay and draw/state/frustum gates with zero skipped or repeated presentation intervals. No runtime guard texture imports occur. Whole-frame budget is 16.667 ms; these are emulator measurements.

| Route | Synchronous mean / p95 / peak (ms) | Worker first mean / p95 / peak (ms) | Synchronous misses | Worker first / repeat misses |
| --- | --- | --- | --- | --- |
| Heavy Dam | 11.2374 / 16.1463 / 22.9490 | 9.8991 / 13.3944 / 20.1236 | 166 | 9 / 6 |
| Established Dam | 10.0590 / 15.1842 / 26.5130 | 8.9364 / 13.2890 / 22.2448 | 154 | 10 / 10 |
| Caverns | 7.2460 / 11.8105 / 15.0230 | 6.7902 / 10.7127 / 14.0766 | 0 | 0 / 0 |

Heavy Dam's repeat averages 9.8899 ms, p95 13.3807 ms, p99 15.3542 ms and peak 20.1454 ms. This variation is retained rather than selecting the better run. The corrected heavy route has 42 PP7 shots, four damaging guard hits and 696 guard fire dispatches; established Dam has 26 shots, two hits and 1,174 dispatches. Both complete eleven targets, the original opening-guard death and player damage with zero unresolved AI opcodes.

## Complete music comparisons

Each route has a new deep-profile capture of the corrected synchronous binary and the final worker. All three pairs pass recorded gameplay and draw/state/frustum gates, and match the complete music PCM hash and byte count. These instrumented captures are correctness evidence; their added synthesis profiling/hash overhead is excluded from the normal timing table.

| Route | Complete PCM hash | Bytes |
| --- | --- | ---: |
| Heavy Dam | `51de3d1a25a3c0fd` | 6,624,000 |
| Established Dam | `1d505126fa091a5c` | 6,435,584 |
| Caverns | `3fb892d7b6dacc8c` | 4,416,000 |

The worker protocol test was additionally extended after the full suite to exercise a failed hardware query explicitly. The updated test passes both ASan/UBSan and TSan; production source and release binary are unchanged.

## Original 3DS fallback

With Azahar stopped, its configuration was saved and only New 3DS mode/default flags were changed. The final release completed the Caverns control in original 3DS mode with `music_worker=0,0,0,0`, proving the synchronous branch was selected. Recorded gameplay and draw/state/frustum totals match the corrected synchronous control; no runtime guard texture imports occur. The original emulator configuration was restored byte-for-byte after collection. This verifies the software fallback on the emulator, not original 3DS CPU/GPU speed or memory behavior on a physical console. Configuration snapshots and assertions are preserved beside the capture.

## Independent live scheduler

`dam-worker-final-live.result` uses the final release and the full Dam input with no replay cadence or deep/world profiling. It completes 4,500 frames and the authored firefight verifier passes: 32 PP7 shots, four damaging guard hits, 956 guard fire dispatches, an opening-guard death, player damage and zero unresolved AI opcodes. After 120 warmup frames it averages 9.2007 ms, p95 13.2804 ms, p99 15.3342 ms and peak 21.7829 ms, with 11 budget misses and zero skipped/repeated presentation intervals. Its independently generated retrace history is preserved in `dam-worker-final-live.cadence`. Because it produces a different combat history, these figures are a live integration result, not an isolated speedup comparison against the fixed controls.

## Release and remaining work

The final `.3dsx` and `.smdh` are staged in `build/3ds-sd/3ds/goldeneye-3ds` and Azahar's local SD directory. The unchanged asset pack is `7271d1db02833c29ce4b28111cfc3e56f6a03c2b5f9ae1510593669b9f61cf49`. Original launch-config inventory was restored, normal Rareware-logo startup was visually observed, and the emulator was stopped. Candidate `build/3ds-candidates/music-worker-4446f3c8` preserves source, build provenance, reports and complete experimental/acceptance evidence. ROM-derived assets and local evidence archives remain outside Git.

This round accepts the collision repair and software audio-worker integration. It does not establish locked 60 fps, full mission completion, campaign parity or physical console performance. Higher-effort next work is physical New 3DS XL profiling, followed by first-person model preparation/upload spikes, persistent prop/character preparation and CPU world submission. Those changes need their own bounded ownership design and corrected replay/visual comparisons. Original 3DS performance and physical launcher core availability remain open.
