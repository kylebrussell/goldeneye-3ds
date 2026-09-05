# SFX decoder tail latency — 5 September 2026

Repeated normal and audio-gated native emulator comparisons, the full regression suite, and the final ARM build pass. The verified candidate is preserved and staged locally; the next overlay investigation is separate.

## Scope and exactness

The retained candidate uses a bounded signed 32-bit ADPCM accumulator when an absolute-coefficient proof establishes that every partial sum fits; extreme books retain the original signed 64-bit path. Signed floor division, saturation, predictor changes, and sample order remain exact. The current predictor is retained only within one decode, in constant stack space. Output overlapping control storage disables predictor reuse. No sound is moved earlier, skipped, or rescheduled; the existing 96-entry / 2 MiB decoded PCM cache remains unchanged.

The decoder alone receives `-O3`, without fast math. At both `-O2` and `-O3`, ASan/UBSan differential testing passes 6,000 synthetic decodes (25,433 bounded and 25,567 wide books, all scales, saturation/history, predictor changes, aliasing, capacity and partial-output errors), and all 261 original sounds: 2,124,208 exact PCM samples, numeric-sample FNV digest `c7efc44fa56614af`. The reference restores the original 64-bit floor function and reloads the predictor book every block. Prepared world runs and guard attachment texture preparation are preserved.

## Selection evidence

The previous exact build is `efce164d5cffccfb45569364cb727de457da3a11cee71f6ee415f54686bdda7d`. Asset pack is `7271d1db02833c29ce4b28111cfc3e56f6a03c2b5f9ae1510593669b9f61cf49` throughout.

The 319 Dam frames selected by the previous diagnostics-off capture's actual 60 Hz work budget averaged 19.181 ms: canonical ticking 8.3342 ms, renderer 5.0345 ms, remainder 5.8123 ms. Music (4.1271 ms) and props (2.6193 ms) are children of canonical ticking; world rendering (3.9041 ms) is a child of the renderer. Guard publication, uploads, first-person work, firing, and SFX counters have nested scopes and must not be added to their parents.

A fresh deep Caverns baseline uses the same retrace cadence as the normal comparison (`7662e338…`); an older deep capture used a different cadence and cannot attribute these frame numbers. In that matched baseline, frames 2039 and 2343 each spend 3.521 ms decoding a cold sound. They are the only two normal work-budget misses. This identifies a bounded, exact inner-loop change with a direct tail benefit.

The aligned music-mixing experiment remains rejected. It preserved PCM but improved Dam mean work by only about 0.02 ms and left 31 skipped intervals unchanged. Its source and evidence are archived under `build/host-tests/tail-optimization/`; production `ge_audio_abi.c` is restored to the prior build.

## Measurement protocol

Each comparison starts a new Azahar process with cold process-owned caches, identical executable/asset/input/cadence hashes recorded in provenance, and no concurrent builds or tests. Normal comparisons disable deep and world profiling. Deep runs validate PCM and attribution separately; their overhead is not counted as production performance. Skip the first 120 displayed frames when reporting tails; Caverns measures 2,880 frames and Dam 4,250. The Dam route completes its authored firefight replay, not a successful mission completion. Matching recorded state/draw/audio fields does not prove every intermediate state or physical console performance.

## Normal replay results

Final SFX executable: `5e5b3c5a9aba9dee4c7e605ea40e43f20e8321011f7714fed9ab6e83748ad6ad`.

| Route / build | Mean work ms | p95 ms | p99 ms | Peak ms | Over 60 Hz budget | Skipped intervals |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Caverns previous | 7.7824 | 12.6136 | 14.4592 | 17.6436 | 2 / 2,880 | 0 / 2,879 |
| Caverns SFX first | 7.7365 | 12.5380 | 14.3030 | 16.0568 | 0 / 2,880 | 0 / 2,879 |
| Caverns SFX cold repeat | 7.7367 | 12.5192 | 14.3446 | 16.0129 | 0 / 2,880 | 0 / 2,879 |
| Dam previous | 11.0882 | 18.0080 | 20.8647 | 31.1167 | 319 / 4,250 | 31 / 4,249 |
| Dam SFX first | 11.0600 | 17.9308 | 20.7949 | 29.8349 | 311 / 4,250 | 30 / 4,249 |
| Dam SFX cold repeat | 11.0598 | 17.9309 | 20.8022 | 29.8722 | 311 / 4,250 | 31 / 4,249 |

Caverns frames 2039 and 2343 are 15.0702 / 15.8284 ms in the first final run, compared with 15.5347 / 16.2929 ms in the `-O2` decoder experiment and over 16.667 ms in the previous decoder. Normal first/repeat runs have identical recorded gameplay checkpoints and draw calls/state/frustum totals, with zero runtime guard texture imports. Dam retains 26 PP7 shots, two damaging guard hits, 1,174 guard fire dispatches, one original opening-guard death and player damage, and zero unresolved AI opcodes. The skipped-interval result on Dam is variable; no consistent presentation improvement is claimed there.

This establishes budget compliance in the bounded, fixed-cadence Caverns emulator context. It does not establish live-scheduler or physical New/Old 3DS 60 fps performance or all-level parity.

## Disjoint outer-frame attribution

A separate temporary diagnostic executable (`83232c5a…`) timestamps consecutive boundaries. The analysis selects the same 319 baseline slow Dam frame IDs and asserts for each frame that all boundary intervals minus the existing explicit presentation wait equal measured work exactly in integer ticks. It then subtracts the original canonical and renderer counters from their envelopes; their boundary overhead remains explicit. Wrapper clocks add small measurement overhead (outside scopes average 5.8284 ms versus baseline 5.8123 ms). Production source is restored after generating the diagnostic executable. This accounts for the existing measured work interval: `aptMainLoop`/physical input reading precede its start and some final probe accounting follows its end. Submission intervals are a separate pacing check.

| Exclusive remainder component | Mean ms on selected frames |
| --- | ---: |
| Overlay publication including final visibility/list update | 4.4024 |
| First-person publication including flush | 0.7700 |
| C3D frame-end submission | 0.3809 |
| NDSP audio pump | 0.1224 |
| World upload | 0.0704 |
| Input / probe setup | 0.0273 |
| Telemetry / tour | 0.0122 |
| Portable retrace advance | 0.0112 |
| Post-submit accounting | 0.0103 |
| Frame begin and previous GPU query | 0.0060 |
| Render-target setup | 0.0058 |
| Canonical boundary overhead | 0.0029 |
| Renderer boundary overhead | 0.0027 |
| Presentation bookkeeping excluding wait | 0.0027 |
| Frame setup | 0.0013 |

Explicit presentation waiting averages 0.0086 ms on these frames and is excluded from work. Frame begin/end wall time includes any waits inside those APIs; the explicit VBlank pacing loop is measured separately. Audio pumping is not the missing multi-millisecond contributor. Overlay publication is the next measured target; its nested guard-scene (2.0277 ms), upload (0.1972 ms), and visibility (0.2795 ms) counters are not added to the outer remainder again.

## Validation and staging

- `scripts/test_port.sh`: pass, including the new SFX differential test at `-O2` and `-O3`.
- `scripts/build_3ds.sh -j4`: pass; final rebuild SHA exactly matches the measured candidate.
- `git diff --check`: pass.
- Deep PCM gates: Dam `1d505126fa091a5c,6435584`; Caverns `3fb892d7b6dacc8c,4416000`, identical to the matched baseline along with recorded gameplay and draw fields.
- Preserved candidate: `build/3ds-candidates/sfx-decoder-5e5b3c5a/`, including binary, assets, catalog, source patch, changed-source archive, performance evidence and toolchain provenance. Staged in `build/3ds-sd/3ds/goldeneye-3ds/` and Azahar. The diagnostic outer-boundary source is evidence only. No physical deployment or commit is performed.


## Follow-on overlay investigation

A second temporary diagnostic (`c44e0a79…`) measures only the outer overlay pass, disabling nested wrapper accounting. On the same 319 baseline slow Dam frame IDs, guard publication averages 2.1733 ms, articulated-object publication 1.6072 ms, glass 0.2317 ms, visibility 0.1517 ms, doors 0.0138 ms, and monitors 0.0089 ms. These are children of the 4.40 ms overlay envelope, not additional frame costs. Articulated objects were selected ahead of glass.

The concrete repeated work is `resource_part_for_node`: every visited model node scans a contiguous resource-owned node array to discover its index, and per-part queries repeat that traversal. A candidate replaces only that membership scan with integer range and alignment checks and the exact array index. Model relations, traversal order, instance visibility, part metadata and failure behavior are retained. The candidate passed 266,240 differential cases but saved only about 0.006 ms mean Dam work and retained 311 budget misses. It is rejected and reverted; source and tests remain in the evidence directory only. The staged SFX build above remains the verified baseline.
