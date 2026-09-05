# Newest Dam combat history and exact audio sample stores — 5 September 2026

> Reproducibility correction: a later forced rebuild found a stale visibility object with a mismatched context layout in the archived `cd98cb4b` executable. These are measurements of that historical binary, not acceptance results for a fully rebuilt checkout. See [the corrected baseline and build fix](Verification20260905WorldGuardSubmission.md).

This pass starts from GPU command candidate `704af8ad67f5395652679052e6529035176dfb836e162c2b1f588c4a0841be56`. The primary stress case is its latest independent live Dam combat history: 32 shots, three damaging guard hits, 828 guard-fire dispatches, one opening-guard death and Bond alive at 0.5 health. Its actual retrace cadence is preserved in `build/host-tests/live-dam/dam-final-live.cadence`, SHA-256 `afed049bb9d8d6a57dab39a64f22b85bdb2218b9f473855ba789f7424d5429ab`. The unchanged authored input has SHA-256 `ea3ea43e4de2b5eb74355bf213980452010217ee942a8dcdfcb86bb291c10fe6`.

## Attribution on the current candidate

The normal replay baseline has 377 work-budget misses in 4,380 samples after the unchanged 120-frame warmup. Of these, 222 lie in frames 251–713, 44 in 767–991, 18 in 1105–1143, and 50 in 4307–4459. Peak frame 185 is 22.1740 ms. The early sustained slow frames average 18.2637 ms work: canonical ticking 8.7599 ms (including music 4.4325 and props 2.7856), renderer 4.8007 ms, and 4.7031 ms outside those parents. The late cluster averages 17.3880 ms, including music 5.5530 ms.

A temporary diagnostic build combines disjoint outer boundaries, per-opcode music, guard phases and per-frame world attribution. Every one of 4,500 frames reconciles exactly in integer ticks after subtracting explicit presentation waiting. Its recorded gameplay and draw totals match; the strict comparison records only the expected deep-profile metadata and PCM-availability differences. Heavy diagnostic times select work to investigate and are never presented as production FPS.

The instrumented early cluster has overlay publication 3.6822 ms, including guard-cache build 1.5827 ms (vertex transforms 0.7546). World children are visibility 1.2451, material setup 0.9479 and submission 2.2508 ms. Music children include envelope mixing 1.1711, resampling 0.8912, mixing 0.8245, pole filtering 0.6918 and ADPCM 0.6016 ms. In the late cluster, envelope mixing rises to 1.5626 ms, ADPCM to 1.1581 and resampling to 1.0664. Peak frame 185 instead has 9.5487 ms instrumented overlay publication, including 3.1410 ms guard-cache build and 2.0154 ms vertex transforms. These nested children must not be summed again with their parents.

## Retained change

ARM disassembly shows each big-endian 16-bit audio sample store using two byte stores and a shift. The shared store helper now encodes big-endian bytes in a local 16-bit value and copies exactly two bytes with `memcpy`. GCC emits `rev16` and `strh` for the ARM build. There is no alignment assumption, type-punning, new command branch, DSP arithmetic change or buffer reordering. Compilers retain responsibility for implementing unaligned copies safely; the bytewise fallback remains for an unknown host byte order.

This differs from the rejected aligned-mixing experiment: it adds no alignment dispatch or duplicated mixing loop, and applies to the shared sample store across envelope mixing, mixer, resampling, ADPCM and pole filtering. It was selected after inspecting the current slow clusters and ARM instructions. All original GPU commands, geometry publication, material setup, gameplay and simulation scheduling remain unchanged. Diagnostic main-source changes are archived outside production.

`scripts/tests/test_audio_big_endian_store.py` executes the actual helper against independent byte stores for all 65,536 values at 15 offsets, then one million ordered overlapping writes. Native and portable fallback builds both pass ASan/UBSan. The existing envelope differential test passes 30,000 commands, including aliases, odd/even buffers, ramps, saturation, saved state and failures.

On the original 377 slow-frame IDs, normal work falls from 17.9435 to 17.6855 ms and music from 4.5109 to 4.2688 ms. Early-cluster music falls 4.4325→4.1973 ms; late-cluster music falls 5.5530→5.2453 ms. Rendering remains essentially unchanged. The gain is actual synthesis work saved, with the same timing interval and canonical retrace schedule.

## Validation

Normal timing runs restart Azahar, disable deep/world diagnostics and run with no concurrent builds or tests. Matched comparisons use the same binary-independent input, pack and actual retrace schedule. Separate deep captures check audio PCM; a fresh live-scheduler run checks scheduling without comparing different combat histories as though they were matched workloads.

One attempted repeat was collected before serialization finished because `status=complete` appears in the file header. It is retained as `stress-store-repeat-truncated.result` and excluded from all measurements. The collector now requires the final `renderer_peak_phase_ticks` record and verifies the declared frame count against actual records. The original baseline, diagnostic and first candidate captures all have complete records and footers.

The full `scripts/test_port.sh` regression suite passes. A clean final ARM recompilation of the restored production main reproduces executable SHA-256 `cd98cb4b4f6619f160742e2bf02b2999f79b01f4e0f732e8084ce1eea42c0c1d`. Both complete normal stress runs have 317 misses and zero skipped intervals, with p95 17.2557 ms and peak 21.9854 ms.

The matching deep stress captures produce exactly `51de3d1a25a3c0fd,6624000` for PCM hash and byte count. Per-opcode timing on the 377 baseline slow-frame IDs confirms that the shared helper saves work across several commands: ADPCM 0.7098→0.6557 ms, resampling 0.8866→0.8444, mixing 0.8166→0.7656, pole filtering 0.6860→0.6372, and envelope mixing 1.1616→1.1494. The measured result therefore comes mainly from the shared write path across synthesis, rather than envelope mixing alone.

### Normal matched timing

| Workload / build | Mean ms | p95 ms | p99 ms | Peak ms | Budget misses | Skipped intervals |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Newest Dam history baseline | 12.6199 | 17.5333 | 19.0753 | 22.1740 | 377 | 1 |
| Newest Dam history candidate first | 12.5054 | 17.2557 | 18.7815 | 21.9854 | 317 | 0 |
| Newest Dam history candidate repeat | 12.5054 | 17.2557 | 18.7814 | 21.9854 | 317 | 0 |
| Established Dam baseline | 10.5107 | 16.2094 | 18.9611 | 27.8027 | 189 | 1 |
| Established Dam candidate first | 10.4005 | 16.0367 | 18.7092 | 27.7660 | 173 | 0 |
| Established Dam candidate repeat | 10.4005 | 16.0367 | 18.7092 | 27.7660 | 173 | 0 |
| Caverns baseline | 7.5523 | 12.3186 | 13.7856 | 15.6724 | 0 | 0 |
| Caverns candidate first | 7.4370 | 12.0839 | 13.5511 | 15.4666 | 0 | 0 |
| Caverns candidate repeat | 7.4370 | 12.0839 | 13.5512 | 15.4666 | 0 | 0 |

Newest Dam has 4,380 post-warmup samples / 4,379 presentation intervals; established Dam has 4,250 / 4,249; Caverns has 2,880 / 2,879. Every normal comparison passes recorded gameplay and draw-state gates, with zero repeated intervals and zero runtime guard texture imports.

### Exact PCM comparisons

| Workload | PCM hash | Byte count |
| --- | --- | ---: |
| Newest Dam history | `51de3d1a25a3c0fd` | 6624000 |
| Established Dam | `1d505126fa091a5c` | 6435584 |
| Caverns | `3fb892d7b6dacc8c` | 4416000 |

All three matching deep comparisons pass recorded gameplay, draw calls/state/frustum and PCM gates. Byte counts are the interpreter’s output-byte counter, not sample counts.

### Independent live scheduler

The final production candidate completes the unchanged authored route in 4,346 displayed frames / 4,347 simulation ticks, reaching all 11 targets. It records 27 PP7 shots, four damaging guard hits, 985 guard-fire dispatches, one opening-guard death and player damage, with no unresolved AI opcodes. Bond dies in this run. The prior live history left him alive at 0.5 health, so these outcomes are different workloads: the raw live miss counts are not a matched speedup comparison.

After the unchanged 120-frame warmup, the 4,226 live samples average 10.8885 ms, with p95 16.9510, p99 18.7117 and peak 22.0882 ms. There are 235 work-budget misses and zero skipped or repeated intervals among 4,225 measured presentation intervals. The authored combat verifier passes; this is not a full-mission completion claim.

Its actual retraces are preserved as `build/host-tests/dam-stress/dam-store-live.cadence`, SHA-256 `4531967d59495e8e5e0a4924fd48d8558ed698b02f0255bec63cfe812fb4e50c`, padded only to the original unused 4,500-frame capacity. The heavier `afed049b` history remains the primary stress benchmark; this new history is additional evidence rather than a replacement that makes the benchmark easier.

## Staging and remaining work

Candidate `cd98cb4b4f6619f160742e2bf02b2999f79b01f4e0f732e8084ce1eea42c0c1d` is preserved with the source snapshot, exactness/build logs, complete captures, input/cadence files and build provenance under `build/3ds-candidates/audio-stores-cd98cb4b/`. The asset pack is unchanged at SHA-256 `7271d1db02833c29ce4b28111cfc3e56f6a03c2b5f9ae1510593669b9f61cf49`. The executable and pack are staged locally for SD and Azahar; no physical-device installation is performed.

Dam still has sustained work-budget misses. The remaining measured costs include early-cluster world submission, guard/overlay publication and canonical music/props work; frame 185 remains a distinct publication spike. Keep the heavier history for future optimization, and require matching command bytes, geometry, gameplay, sound and presentation evidence for subsequent changes. These bounded emulator runs do not establish New 3DS/Old 3DS hardware performance, campaign-wide 60 fps, mission correctness or full visual parity.

The original configuration inventory and its bytes were restored and verified. A CUA-observed normal startup progressed through the gunbarrel sequence to the General Arkady Ourumov cast presentation. This verifies launch progression only. Azahar was stopped afterward; Mac wake protection remains active. Final archive and staged-file hashes were verified, and `git diff --check` passes.
