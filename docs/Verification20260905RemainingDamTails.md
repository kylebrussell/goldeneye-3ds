# Remaining Dam frame tails — 5 September 2026

This investigation starts from verified executable `3197c43670b04894071a686d3c928bbd6fd0000a0fb888d352324fd4d35a4814`. Its matched Dam firefight has 252 over-budget frames / 4,250 after the 120-frame warmup, 11 skipped presentation intervals / 4,249, and a 28.9298 ms peak. The matched Caverns route is below budget, with little headroom. Physical New/Old 3DS measurements and campaign-wide parity remain open.

## Accounting and selection

The diagnostic selection is exactly the 252 slow frame IDs from `tail-optimization/dam-artic-pose-repeat.result`. `remaining-dam/analyze_outer.py` joins those IDs to a separate instrumented replay. Fifteen consecutive outer boundaries sum exactly to the existing work interval in integer ticks, after subtracting explicit presentation waiting. Canonical and renderer envelopes are separated from their measured children, never added twice. The work interval excludes `aptMainLoop` and physical input polling before its start and final probe bookkeeping after its end; submission VBlank deltas separately measure pacing.

| Scope on selected frames | Mean ms |
| --- | ---: |
| Normal work | 18.7621 |
| Canonical tick, including music and props | 8.4517 |
| Renderer, including world | 5.1669 |
| Outside those two parents, original normal capture | 5.1435 |
| Outer overlay publication, separate diagnostic capture | 3.7160 |
| Outer first-person publication | 0.7898 |
| Frame end | 0.3784 |
| Audio pump | 0.1209 |
| World upload | 0.0700 |

The diagnostic remainder is 5.1640 ms; the small difference includes instrumentation. Explicit presentation waiting is 0.0035 ms and is excluded. Normal canonical children include music 4.1850 ms and props 2.6515 ms. Separate deep profiling measures props character work at 1.6542 ms and object work at 1.2494 ms, with additional timer overhead. Guard-cache build is 1.5740 ms, including 0.7530 ms vertex transforms, 0.2535 ms matrix quantization, 0.2032 ms signatures, 0.0908 ms topology and 0.0842 ms batch publication. These are nested counters, not additional outer costs.

Deep music opcode means are envelope mixing 1.1326 ms, resampling 0.8095 ms, mixing 0.7438 ms, pole filtering 0.6675 ms and ADPCM 0.5794 ms. Per-command clocks inflate small operations substantially; these figures guide investigation and are not normal FPS results.

## Rejected experiments

All experiments use cold emulator processes, the same authored input and fixed retrace sequence, the preserved pack, and no concurrent builds/tests. Normal timing runs disable deep and world profiling. The original matched Dam baseline is mean 10.9102 ms, p95 17.1000 ms, p99 19.9013 ms, peak 28.9298 ms, 252 budget misses and 11 skipped intervals.

| Experiment | Mean ms | p95 ms | p99 ms | Peak ms | Misses | Skipped intervals |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Zero-gain mixing elision | 10.9752 | 17.2478 | 20.1286 | 28.9253 | 270 | 13 |
| Bounded 32-bit pole filter | 10.9375 | 17.1614 | 19.9409 | 28.9156 | 254 | 12 |
| Two-layout first-person UV retention | 10.9219 | 17.1547 | 19.9359 | 28.4303 | 255 | 12 |
| UV retention plus exact shortened float hash | 10.9204 | 17.1435 | 19.9131 | 28.2138 | 252 | 12 |

The UV cache correctly reduced mapped first-person vertices from 92,196 to 3,480 and saved roughly 0.5–0.75 ms on several repeated firing-layout frames. It retained only two bounded layouts, revalidated actual atlas and normalization contexts, cleared on resource/texture replacement and teardown, and declined incomplete mappings. Its 2,400-publication sanitizer test compared entire GPU buffers and sentinels with uncached mapping through eviction, resource replacement, atlas/material changes, missing textures, per-vertex mapping failure and allocation failures. The float-hash change preserved two million bit patterns and chained hash seeds exactly by combining the four zero high-byte multiplications. Neither delivered a whole-run improvement sufficient to retain its source changes. Tests and source are archived as rejected evidence, not enabled in production.

The two audio experiments likewise passed differential PCM/state tests before measurement and remain reverted. Complete provenance, source variants and result files are under `build/host-tests/remaining-dam/`. Recorded Dam gameplay and draw totals match for these normal comparisons, including 26 player shots, two damaging guard hits, an opening-guard death and player damage. Such equality does not prove every intermediate state or full mission completion.

## Retained changes

The envelope mixer now finishes the ramp-changing prefix, including the sample that clamps the final ramp, then mixes the stable suffix without repeating ramp-state checks. It preserves source-before-output ordering, dry-left/right then wet-left/right writes, rounding, saturation and saved state. Thirty thousand differential commands (23,673 valid) cover odd/even offsets, overlapping buffers and saved-state aliases, ramp crossings, already stable gains, saturation and range/resolver failures under ASan/UBSan. The full port suite passed with this change. Envelope-only Dam first/repeat means are 10.9055/10.9058 ms; misses are 249/251 and skipped intervals remain 11. The benefit is small and the exact threshold count varies.

A further audit found that both guard and first-person scene caches bound detailed per-input clock callbacks unconditionally, even when deep profiling was disabled and during ordinary play. The candidate makes those callbacks obey the existing deep-profile opt-in. Guard binding occurs after reading the diagnostic flag; first-person binding follows cache initialization and receives the flag explicitly. Stage teardown resets the runtime and diagnostic state. Outer frame, guard, first-person and presentation measurements remain enabled, so saved clock calls remain part of a real reduction in measured work rather than a change to the work interval. Deep captures retain the detailed clocks for attribution. This changes instrumentation policy, not geometry or gameplay.

Final candidate executable: `1773e8ae4123368f8686eabfb15f838e1d9d24d825994c1eef050a19d89c22d3` (envelope suffix plus opt-in geometry diagnostics). Pack: `7271d1db02833c29ce4b28111cfc3e56f6a03c2b5f9ae1510593669b9f61cf49`. The final full `scripts/test_port.sh` run passes, including the envelope differential test and the diagnostic opt-in/reset guard. An initial new source-check harness failed to parse preprocessor branches in `main`; correcting that check resolved the failure without changing the measured executable. Final ARM rebuild matches the candidate hash.

On the same 252 original slow frame IDs, normal mean work falls from 18.7621 to 18.3555 ms. Guard publication falls from 2.1711 to 1.8804 ms and first-person publication from 0.7205 to 0.6358 ms. Canonical ticking falls from 8.4517 to 8.4296 ms, including music from 4.1850 to 4.1666 ms. Renderer timing stays essentially unchanged at 5.1669 versus 5.1621 ms. These are observationally related scopes; the child differences are not added to their parents. Normal first-person detailed profile fields are now zero as intended, while outer timing remains populated.

## Final normal matched comparisons

| Route/run | Mean ms | p95 ms | p99 ms | Peak ms | Misses / frames | Skipped / intervals |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| dam-opt-in-normal | 10.7489 | 16.7703 | 19.4508 | 28.5341 | 220 / 4250 | 4 / 4249 |
| dam-opt-in-repeat | 10.7486 | 16.7359 | 19.4212 | 28.5775 | 219 / 4250 | 4 / 4249 |
| caverns-opt-in-normal | 7.6023 | 12.3733 | 13.9442 | 15.8039 | 0 / 2880 | 0 / 2879 |
| caverns-opt-in-repeat | 7.6023 | 12.3735 | 13.9441 | 15.8039 | 0 / 2880 | 0 / 2879 |

All four runs have zero repeated VBlank intervals, zero runtime guard texture imports, matching recorded gameplay checkpoints and matching draw calls/state/frustum totals. Both cold Dam repeats satisfy the authored firefight verifier. These captures use the same fixed retrace histories as the preceding verified candidate and measure both cold first use and subsequent layout transitions. They do not establish 60 fps for Dam or physical hardware.

## Audio and regression gates

Deep captures retain detailed clocks and match the baseline's recorded gameplay and draw totals. Dam music PCM is `1d505126fa091a5c,6435584`; Caverns is `3fb892d7b6dacc8c,4416000`. These deep runs are exactness/diagnostic gates, not normal-performance evidence. The full port regression suite, including original audio goldens, all original SFX decode checks, renderer buffer tests, campaign/objective/menu checks and the new envelope differential test, passes. Final ARM build hash matches every final normal and deep capture. Rejected UV/float-hash source and temporary tests are absent from the active implementation.

## Separate live-scheduler combat check

`dam-opt-in-live.result` uses the same authored firefight input and final executable with `probe-retraces.cfg`, deep profiling and world profiling absent. It restarts the emulator with cold process caches. The natural scheduler completes all 11 route targets in 4,258 displayed frames; the verifier passes with 26 PP7 shots, four damaging guard hits, 1,100 guard fire dispatches, one opening-guard death, player damage/death and zero unresolved AI opcodes. This is a completed combat harness, not a survived mission or mission-completion test.

After the same 120-frame warmup: mean work 10.9469 ms, p95 17.5977 ms, p99 19.5331 ms, peak 23.6894 ms; 269 misses / 4,138 work samples. There are six skipped and zero repeated VBlank intervals / 4,137; 18 frames consume multiple retraces. This run follows a different timing/state history from the fixed-cadence captures and is reported on its own, with no matched-speedup claim. It confirms that live Dam combat is still not locked at 60 fps. The result file became available just after an initial collection check; the completed capture and provenance were subsequently verified, with no launch/gameplay failure or extended/warmed harness.

## Staging and remaining work

The verified executable, SMDH, exact existing pack/catalog, source patch, changed-source snapshot, all normal/deep/live evidence and Docker toolchain identity are preserved in `build/3ds-candidates/diagnostics-envelope-1773e8ae/`. The executable and assets are staged in `build/3ds-sd/3ds/goldeneye-3ds/` and Azahar. Original launch configuration is restored afterward. No commit, push or physical deployment is performed.

This pass retains only the exact envelope suffix and the correction to detailed geometry profiling. UV-layout retention, shortened float signatures, zero-gain elision and bounded pole filtering remain rejected evidence. It adds no new geometry cache or warmup maneuver. Shared production paths benefit, but measured coverage remains the two routes. Next performance work should prioritize the remaining music synthesis and guard/first-person publication costs using the corrected normal/deep distinction, and should retain cold transition costs and the separate live-scheduler acceptance gate. Physical New/Old 3DS measurements, sustained live 60 fps, other campaign routes and full rendering/objective/menu parity remain open.

Normal-launch smoke check: the staged executable with the exact original config inventory restored visibly renders the animated Rare logo and advances into the title/cast sequence. This is a startup check, not a new frontend-fidelity or hardware-FPS claim. Azahar is stopped afterward; caffeinate PID 33419 remains active.
