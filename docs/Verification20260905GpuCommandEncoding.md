# Live Dam clusters and exact GPU command encoding — 5 September 2026

This pass starts from verified candidate `1773e8ae4123368f8686eabfb15f838e1d9d24d825994c1eef050a19d89c22d3`. Its independent live Dam combat check completed 11/11 route targets but had 269 work-budget misses / 4,138 frames after warmup and six skipped presentation intervals. This investigation targets sustained missed-frame work, with the established Dam/Caverns controls retained.

## Preserving the live workload

The exact live capture and input are copied into `build/host-tests/live-dam/`. `scripts/make_3ds_retrace_replay.py` extracts every actual canonical retrace delta, padding only the unused tail to the original 4,500-frame harness limit. The preserved cadence SHA-256 is `031e98e6949372b22b05afb547dfd3bd456ffb0f074681feb603df5570912a54`; input is `ea3ea43e4de2b5eb74355bf213980452010217ee942a8dcdfcb86bb291c10fe6`. A cold replay completes at the same frame count and recorded gameplay checkpoints, with the same retrace deltas and draw calls/state/frustum totals. The strict comparison correctly flags the expected `probe_clock` mode difference; `live-replay-equivalence.json` records that exception explicitly rather than altering the original capture or weakening the normal comparison gate.

Candidate-versus-baseline speedup comparisons on this additional workload use the replay mode on both sides. A new independent live-scheduler run remains a separate acceptance check.

## Slow clusters and opt-in attribution

Of the original live run's 269 misses, 247 lie in frames 243–725. Those slow frames average 18.7525 ms work, including canonical ticking 8.6642 ms (music 4.2629 ms, props 2.7599 ms), rendering 5.3359 ms, and 4.7524 ms outside those parents. The opening transition cluster at 129–186 has 14 misses; the remaining eight misses are isolated or small later clusters. Peak frame 541 is 23.6894 ms in the original normal run.

A separate opt-in diagnostic build records consecutive outer boundaries plus music, props and guard children. Every one of 4,258 frames reconciles exactly in integer ticks after excluding explicit presentation waiting. On the 247-frame sustained cluster the instrumented outer overlay is 3.7192 ms and first-person publication 0.8130 ms. Music children include envelope mixing 1.1016 ms, resampling 0.8400 ms, mixing 0.8069 ms, pole filtering 0.6784 ms and ADPCM 0.5763 ms. Guard-cache build is 1.5891 ms, including 0.7619 ms vertex transforms, 0.2674 ms matrix quantization and 0.2159 ms signatures. Child clocks add overhead and are not summed again with their parents.

A further explicitly enabled world diagnostic samples every frame to avoid the normal 1/16 sampling pattern missing this cluster. It measures visibility 1.2512 ms, material setup 0.9715 ms and draw submission 2.6899 ms, within an instrumented 6.9754 ms world envelope. The residual includes traversal, merge decisions, counters and extra clocks. These heavy diagnostic numbers are not normal FPS measurements; they select draw submission as the largest measured world child. Temporary per-frame instrumentation is archived outside production source.

The normal-hot-path audit confirms that per-input guard/weapon, per-opcode audio, props and allocator diagnostic callbacks obey their opt-in flags. Per-batch world timers are also gated. Existing outer phase, presentation and coarse timing remain available; no simulation timing or required frame accounting is removed.

## Exact command-encoding change

Citro3D 1.7.1's array draw updates context, emits eleven single-parameter register writes and marks the draw used. The candidate retains that complete call path and command order. It specializes only libctru's general command encoder for a valid one-parameter write (including the API's zero-length-as-one behavior): the same parameter word, unchanged header, and offset increment. All multi-parameter, exhausted-buffer and invalid-buffer cases call the original library implementation. No private Citro3D context layout is accessed. Sources: [Citro3D array draw](https://raw.githubusercontent.com/devkitPro/citro3d/v1.7.1/source/drawArrays.c), [libctru encoder](https://raw.githubusercontent.com/devkitPro/libctru/v2.7.0/libctru/source/gpu/gpu.c).

The new host differential test executes the actual wrapper against an independent transcription of libctru 2.7.0. Fifty thousand cases cover complete buffer bytes and offsets, zero/one/multiple parameter counts, register masks and incremental headers, aliases, null parameters, 256-word chunking, padding and exact capacity boundaries. It observes 4,946 fast writes and 28,078 identical panic outcomes under ASan/UBSan. The wrapper preserves source-read-before-write ordering and delegates failure handling to libctru. Toolchain inspection confirms the installed library is libctru 2.7.0-1, with Citro3D 1.7.1-2.

The final ARM executable is `704af8ad67f5395652679052e6529035176dfb836e162c2b1f588c4a0841be56`. Full port regressions pass and a rebuild reproduces that hash. Linked ARM disassembly verifies that `C3D_DrawArrays` still calls `C3Di_UpdateContext`, followed by all eleven register writes through the wrapper. All gameplay, audio and renderer code from 1773e8ae is retained; this pass adds only the platform command wrapper and its link flag/test. No rejected UV, pole-filter, zero-gain, float-hash or frustum experiments are reintroduced.

On the same 247 sustained slow-frame IDs, replayed baseline work is 18.7673 ms versus 18.2637 ms in the initial candidate run; rendering is 5.3380 versus 4.8494 ms, including world 4.2027 versus 3.8289 ms. Canonical ticking and music remain essentially unchanged. This localizes the gain to real rendering work; it does not change the work interval or merely remove diagnostic sampling.

## Normal matched timing results

All normal captures restart Azahar, disable deep/world diagnostics, preserve the exact pack and input, and run without concurrent builds/tests. The first 120 frames are excluded from tail statistics.

| Workload / build | Mean ms | p95 ms | p99 ms | Peak ms | Misses / samples | Skipped / intervals |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Live-history replay baseline | 10.9522 | 17.6881 | 19.5536 | 23.5158 | 278 / 4138 | 5 / 4137 |
| Live-history replay candidate first | 10.7063 | 17.1789 | 19.0391 | 22.9563 | 239 / 4138 | 0 / 4137 |
| Live-history replay candidate repeat | 10.7063 | 17.1788 | 19.0392 | 22.9564 | 239 / 4138 | 0 / 4137 |
| Established Dam baseline | 10.7486 | 16.7359 | 19.4212 | 28.5775 | 219 / 4250 | 4 / 4249 |
| Established Dam candidate first | 10.5107 | 16.2080 | 18.9495 | 27.8469 | 192 / 4250 | 1 / 4249 |
| Established Dam candidate repeat | 10.5107 | 16.2094 | 18.9611 | 27.8027 | 189 / 4250 | 1 / 4249 |
| Caverns baseline | 7.6023 | 12.3735 | 13.9441 | 15.8039 | 0 / 2880 | 0 / 2879 |
| Caverns candidate first | 7.5522 | 12.3373 | 13.7334 | 15.7161 | 0 / 2880 | 0 / 2879 |
| Caverns candidate repeat | 7.5523 | 12.3186 | 13.7856 | 15.6724 | 0 / 2880 | 0 / 2879 |

Every final normal comparison has matching recorded gameplay and draw calls/state/frustum totals, no repeated VBlank intervals and zero runtime guard texture imports. Both Dam histories pass their authored firefight verifiers; the histories intentionally retain different hit/dispatch outcomes from one another. Matching telemetry is a bounded regression gate, not proof of every intermediate state or campaign-wide parity.

## Audio, regressions and independent live acceptance

All three deep audio comparisons pass the strict recorded gameplay/draw/PCM gate. These instrumented captures are separate from the normal timing table:

| Workload | PCM hash | Byte count |
| --- | --- | ---: |
| Established Dam | `1d505126fa091a5c` | 6435584 |
| First preserved live Dam history | `1cfff2cfe95da504` | 6270720 |
| Caverns | `3fb892d7b6dacc8c` | 4416000 |

`scripts/test_port.sh` passes, including the new command-buffer differential test. The final ARM rebuild reproduces the candidate hash. The test preserves libctru's license notice in `scripts/tests/licenses/libctru.txt`.

A fresh independent live-scheduler candidate run completes the unchanged 4,500-frame combat check with 11/11 targets reached: 32 PP7 shots, three damaging guard hits, 828 guard-fire dispatches, one opening-guard death and player damage. The player survives with 0.5 health. After 120 warmup frames, 4,380 samples average 12.6194 ms, with p95 17.5285, p99 19.0781 and peak 22.1866 ms. There are 375 work-budget misses, one skipped presentation interval and no repeated intervals. This is a bounded combat check, not full mission completion.

The prior live run killed the player and produced different combat counts. Its raw 269 misses cannot be compared directly with the new live run's 375 as a speedup or regression. To make that workload reproducible too, the new actual retrace history is preserved as `dam-final-live.cadence`, SHA-256 `afed049bb9d8d6a57dab39a64f22b85bdb2218b9f473855ba789f7424d5429ab`. The prior binary reproduces the new live gameplay and draw totals; the live-versus-replay check records only the expected clock-mode difference. A fresh baseline/candidate comparison then uses replay on both sides:

| Latest live history / build | Mean ms | p95 ms | p99 ms | Peak ms | Misses / samples | Skipped / intervals |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Baseline 1773e8ae | 12.9725 | 17.9410 | 19.5623 | 22.7166 | 515 / 4380 | 4 / 4379 |
| Candidate 704af8ad | 12.6199 | 17.5333 | 19.0753 | 22.1740 | 377 / 4380 | 1 / 4379 |

This additional pair passes the strict recorded-state/draw comparison and authored firefight verifier, with zero runtime guard texture imports. It confirms lower cost under the latest live combat history as well as the established controls.

## Candidate and remaining acceptance

Candidate files, hashes, build provenance, source snapshot and diagnostic evidence are preserved under `build/3ds-candidates/gpu-commands-704af8ad/`. The executable and unchanged asset pack are staged for SD and Azahar; the pack SHA-256 remains `7271d1db02833c29ce4b28111cfc3e56f6a03c2b5f9ae1510593669b9f61cf49`. Probe configuration was restored to the original inventory. A CUA-observed normal startup progressed to the “Starring 007 James Bond” cast sequence. This verifies launch/progression only, not frontend visual fidelity. Azahar was stopped afterward; Mac wake protection remains active.

Dam still exceeds the 16.667 ms work budget in sustained combat. Caverns retains zero misses in these bounded controls, but these emulator measurements do not establish physical New 3DS/Old 3DS performance, all-level coverage, full mission correctness or exact visual parity. The next investigation should target the remaining sustained-cluster costs using the preserved live histories and opt-in attribution, with command bytes, gameplay, audio and presentation gates retained.
