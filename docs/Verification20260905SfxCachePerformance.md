# Sound-cache performance validation — 2026-09-05

Continues the rendering audit using its exact asset pack
`7271d1db02833c29ce4b28111cfc3e56f6a03c2b5f9ae1510593669b9f61cf49`.
New 3DS XL is the primary target; these are Azahar measurements, not hardware
60 fps certification. Original 3DS compatibility and audio quality are retained.

## Retained change

Repeated sound starts reuse immutable decoded PCM in a 96-entry, 2 MiB cache.
Each voice retains its own position, pitch, volume, pan, loop state and owner.
Only unused entries may be evicted. Active voices pin samples; oversized samples
and a fully pinned cache use the original allocation path. Rebinding even the
same bank address invalidates cache identity while preserving active voices.
The byte limit covers cached PCM, excluding metadata and uncached voices.
Observed peak cached PCM was 284,384 bytes in Caverns and 564,640 in Dam.

## Matched evidence

Evidence is under `build/host-tests/performance-followup/`. Each capture has a
SHA-256 provenance sidecar for executable, exact pack, input and retrace file.
The comparison checks input identity, initial RNG, retrace sequence, route,
mission/objective/door state, endpoint and periodic player/RNG/guard checkpoints.
Matching checkpoints are bounded evidence, not proof of every intermediate state.

Recorded retraces feed the unchanged simulation for diagnostic replay. Live
play still follows elapsed time. The Dam cadence includes unused trailing ones
because its authored route ends at frame 4,370 before the 4,500 configured cap.
Both matched runs end at exactly that same frame and route target.

| Replay | >16.67 ms frames | p95 ms | p99 ms | Peak ms | Skipped submission intervals |
| --- | ---: | ---: | ---: | ---: | ---: |
| Caverns before | 73 / 2,880 | 15.9028 | 17.8581 | 22.0682 | 0 / 2,879 |
| Caverns cache | 47 / 2,880 | 15.6097 | 16.8785 | 20.8564 | 0 / 2,879 |
| Dam before | 784 / 4,250 | 20.4363 | 23.0193 | 34.0104 | 77 / 4,249 |
| Dam cache | 666 / 4,250 | 19.7845 | 22.9189 | 32.9663 | 72 / 4,249 |

Both comparison gates pass (`caverns-comparison.json`, `dam-comparison.json`).
Dam retains 26 PP7 shots, two damaging guard hits, 1,174 guard-fire dispatches,
one opening-guard death, player damage and zero unknown AI commands. The route
ends with Bond dead; this is a combat fixture, not a completed mission.

At Caverns frame 2,491, sound preparation falls from 8.7670 to 0.0047 ms and
whole-frame work from 22.0682 to 13.3497 ms. First-use decoding still costs time.

Before executable: `c99c49687a5b4b9b7471141d86dbe47bbeef0d0436e33df99bd08b6321a71641`.
Matched cache executable: `b9fd251cada7a5b06cf2be7b31be4429ce51f2cd320cdd4ad8706082940ee40d`.
Deep profiling is identical for these pairs and adds about 1.1 ms in Caverns.
Do not compare these counts directly with the rendering audit's 31-frame count.

## Normal profiling and live validation

Deep dispatch, allocation and sound-preparation profiling is now opt-in via
`probe-detail.cfg` containing `1`. Standard frame phases and checkpoints remain.
Disabled detail is reported as unavailable, not zero cost. The final sound-cache
binary is `c832e5422f25ea91b2a8f605323ab3d4c10e7687200b4f72206b54dc3e5cccf6`,
preserved with its pack and SMDH in `build/3ds-candidates/sfx-cache-c832e542/`.

The 3,000-frame live Caverns run (`caverns-live-final.result`) has 7 / 2,880 warm
frames above budget, mean 8.1036 ms, p95 13.4440, p99 15.2282 and peak 18.5565.
Its 2,879 submission intervals have no repeats or skips; previous GPU time peaks
at 293 us. Live cadence can alter gameplay, so this is a separate bounded check,
not an isolated speedup comparison against the earlier live baseline.

## Checks and remaining work

The ARM build and focused replay/analyzer tests pass. The audio lifecycle suite
uses ASan/UBSan, compares nonzero cold/warm stereo output with pitch/pan/volume,
and exercises overlapping voices, eviction pressure, pinned samples and bank
rebinding. The full `scripts/test_port.sh` suite passes (exit 0); its log is
`build/host-tests/performance-followup/full-suite.log`.

Dam's remaining music phase reaches 10.4965 ms; world submission reaches
4.7822 ms; guard publication reaches 3.8441 ms and upload 2.1475 ms. These peaks
occur on different frames. Canonical timing includes music and props; renderer
includes world, and actor-transform timers span publication paths. These
categories must not be added as independent totals. Follow-up will measure the
music interpreter's actual costs and retain only an exact-output improvement.
