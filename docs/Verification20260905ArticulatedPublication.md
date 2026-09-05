# Articulated publication after SFX decoding — 5 September 2026

Combined cold-repeat and audio-gated comparisons, byte-exact tests, the full regression suite, and the final ARM build pass. The SFX-only candidate remains preserved separately.

## Change

The articulated-object path now uses the model cache's existing `static_data_changed` contract. When every publication range is pose-only and the scene does not require a fresh copy, it copies only `processed.eye` and world positions, commits authored room IDs without rewriting immutable batch data, and uses the existing position-only GPU upload mode. Static/topology changes and fresh scene installations retain the complete copy and normal upload. There is no new allocation, geometry approximation, visibility change, draw merging, audio scheduling, or canonical-tick change.

This applies to the existing shared path for CCTV, autoguns, racks, vehicles, aircraft and tanks. It preserves guard attachment texture preparation, prepared world runs and the exact SFX decoder. The bounded Caverns route exercises no effective additional articulated work, so it is a regression gate rather than evidence of savings for every articulated model in the campaign.

## Why this work was selected

[The preceding SFX investigation](Verification20260905SfxDecoderTails.md) reconciles the previous 5.81 ms Dam remainder with consecutive boundary timestamps. Every selected frame sums exactly in integer ticks, with explicit presentation waiting excluded. This reconciles the existing work interval; `aptMainLoop` and physical `read_input` run before its start, and final probe accounting after its end is outside that interval. Frame-start intervals and submission VBlank deltas separately observe presentation pacing. It identified 4.40 ms of outer overlay publication, 0.77 ms first-person publication, 0.38 ms frame-end submission and 0.12 ms audio pumping. Children are never added to parents.

Using the same 319 diagnostics-off baseline slow-frame IDs, a second profile measured guard publication at 2.1733 ms and articulated-object publication at 1.6072 ms; glass was only 0.2317 ms. Two further diagnostic builds narrowed articulated work:

| Scope inside articulated publication | Mean ms |
| --- | ---: |
| Part count | 0.0137 |
| Object transform query | 0.0017 |
| Part getters | 0.0626 |
| Publication input construction | 0.0701 |
| Cache build, profiling its outer call | 0.4480 |
| Batch commit | 0.0992 |
| GPU upload | 0.1612 |

The remaining envelope includes full mesh/batch copies and bookkeeping. A separately instrumented cache measured 0.4828 ms including clock overhead: vertex transformation 0.3967 ms, signatures 0.0245 ms, matrix quantization 0.0206 ms, batch publication 0.0107 ms, topology 0.0075 ms and internal overhead. Those cache children are not additional costs outside cache build.

A constant-time node-membership experiment passed 266,240 sanitizer differential cases but saved only 0.006 ms mean Dam work and retained 311 work-budget misses. It was rejected and restored. Aligned audio mixing also stays rejected. Their source and results remain evidence only.

## Comparisons

All runs restart Azahar, giving cold process-owned caches; binary, input, pack and retrace-cadence hashes are checked by the collection tool. No builds or tests run alongside probes. The first 120 frames are omitted from tail statistics. Normal comparisons disable deep and world profiling. Audio hashes are collected separately under deep profiling.

Baseline executable: `efce164d5cffccfb45569364cb727de457da3a11cee71f6ee415f54686bdda7d`.
SFX-only executable: `5e5b3c5a9aba9dee4c7e605ea40e43f20e8321011f7714fed9ab6e83748ad6ad`.
Combined executable: `3197c43670b04894071a686d3c928bbd6fd0000a0fb888d352324fd4d35a4814`.
Asset pack throughout: `7271d1db02833c29ce4b28111cfc3e56f6a03c2b5f9ae1510593669b9f61cf49`.

| Dam build | Mean ms | p95 ms | p99 ms | Peak ms | Budget misses / 4,250 | Skipped intervals / 4,249 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Previous baseline | 11.0882 | 18.0080 | 20.8647 | 31.1167 | 319 | 31 |
| SFX-only repeat | 11.0598 | 17.9309 | 20.8022 | 29.8722 | 311 | 31 |
| Combined first | 10.9102 | 17.1000 | 19.9014 | 28.9298 | 252 | 11 |
| Combined cold repeat | 10.9102 | 17.1000 | 19.9013 | 28.9298 | 252 | 11 |

Caverns combined first: mean 7.7372 ms, p95 12.5197 ms, p99 14.3452 ms, peak 16.0134 ms; zero misses / 2,880 and zero skipped intervals / 2,879. Caverns combined cold repeat: mean 7.7370 ms, p95 12.5384 ms, p99 14.3034 ms, peak 16.0574 ms; again zero misses and zero skipped intervals. The preceding SFX-only build also has two cold repeats at zero misses and zero skipped intervals.

Selecting the same 319 original baseline slow-frame IDs in normal captures, mean work falls from 19.0644 ms (SFX-only) to 18.2044 ms (combined), and the disjoint remainder outside canonical ticking and rendering falls from 5.8117 to 4.9623 ms. This localizes about 0.8494 ms of the gain to the intended publication region without additional diagnostic overhead.

## Exactness and limits

The new test exercises the actual pose-copy helper across 12,000 pose/static/force-copy/empty publications, comparing every vertex byte and range sentinels under ASan/UBSan. The model-cache test now verifies that pose-only publication leaves all bytes outside eye/world positions unchanged. The actual GPU adapter test covers 240 overlay transitions, comparing full buffers, bounds, UVs, colors and empty/shrink/growth/fallback behavior; existing dynamic-scene tests cover room-only commits and generation/count preservation.

Recorded gameplay checkpoints and draw calls/state/frustum totals match in normal first runs, with zero runtime guard texture imports. Dam retains 26 PP7 shots, two damaging guard hits, 1,174 guard fire dispatches, one original opening-guard death and player damage, and zero unresolved AI opcodes. This authored replay terminates after the firefight; it is not proof of successful mission completion. Matching recorded fields cannot establish every intermediate state or all-level parity.

These are bounded fixed-cadence emulator results. Physical New/Old 3DS performance, live-scheduler 60 fps and full single-player parity remain open. Dam is still over budget in demanding frames; the next work should measure the remaining canonical music/props and guard-publication tails with the same non-overlapping accounting.


## Final validation and staging

- `scripts/test_port.sh`: pass, including the strengthened real model-cache pose contract, 12,000 pose-copy cases, GPU adapter comparisons, SFX tests at both optimization levels, and the existing campaign/objective/menu/audio tests.
- `scripts/build_3ds.sh -j4`: pass; rebuilt executable SHA matches all final captures.
- `git diff --check`: pass. Rejected aligned-mixing and node-index source changes are absent; all extra outer/overlay/articulated profiling is confined to archived diagnostic builds.
- Deep audio gates match: Dam music PCM `1d505126fa091a5c,6435584`; Caverns `3fb892d7b6dacc8c,4416000`, alongside recorded gameplay and draw equality. All original SFX samples also match the original wide decoder independently.
- Final candidate is preserved in `build/3ds-candidates/sfx-articulated-3197c436/` with executable, SMDH, exact asset pack/catalog, hashes, source patch, changed-source archive, toolchain identity and performance evidence. It is staged in `build/3ds-sd/3ds/goldeneye-3ds/` and Azahar; normal launch configuration is restored. No physical deployment or commit is performed.

Normal-launch smoke check: with probe/detail/world/retrace configs removed and the original config inventory restored, the staged executable renders the classification/copyright screen and advances into the title/cast sequence. This is a startup check, not a new fidelity or frontend-FPS claim. Azahar is stopped afterward; the existing caffeinate wake assertion remains active.
