# ARM11 audio saturation optimization — 2026-09-05

Continues [the sound-cache validation](Verification20260905SfxCachePerformance.md).
The sound-cache pass's pending ARM build, focused tests, full regression suite
and live Caverns run are complete. Its exact executable and asset pack are
preserved under `build/3ds-candidates/sfx-cache-c832e542/`.

## Measured target and implementation

The matched Dam sound-cache capture identifies music as the largest recurring
CPU phase: mean 2.3990 ms, p99 6.9955 ms, peak 10.4965 ms. World submission is
mean 2.2517 ms, p99 4.7071 ms, peak 4.7822 ms. Guard scene publication averages
0.5717 ms and GPU upload 0.0431 ms, with occasional topology-related spikes.
Frame-begin wait peaks at 0.0031 ms and previous GPU execution at 293 us.
This points to CPU synthesis and submission, rather than GPU saturation.

An optional opcode profiler further separates the music interpreter. Across
the Dam diagnostic, envelope mixing takes 2.822 seconds, general mixing 2.265,
resampling 1.907, ADPCM 1.550 and the pole filter 1.515. Per-command clock calls
add overhead, particularly to short commands; these are instrumented costs.

The retained change uses ARM's signed saturating instruction for the existing
16-bit clamp, via `__ssat(value, 16)` when `__ARM_FEATURE_SAT` is available.
The portable scalar fallback remains. Both 3DS models use the ARM11 path.
No arithmetic before the clamp, rounding, sample order, resampling rate,
voices, effects, simulation or renderer behavior is changed. The ARM control
object has no `ssat`; the candidate contains 22 instances, including fused
arithmetic-shift/clamp instructions. Disassemblies are preserved in the evidence
folder. The optimization is independent of the optional profiling hooks.

## Matched exact-output verification

Evidence folder: `build/host-tests/performance-followup/`.
Control executable: `46dcc984013976461283ca18e1e34996bd6fafe465a061a41b1085cb8d1a694a`.
Candidate executable: `fa6dffe84364580e73e60e60f40ee684cef6687b1fd27ca018e0aa2ebacc4a89`.
Exact asset pack: `7271d1db02833c29ce4b28111cfc3e56f6a03c2b5f9ae1510593669b9f61cf49`.

Both pairs use identical input files and recorded canonical retraces, with
all diagnostics enabled equally. Both checkpoint/retrace gates pass, now also
requiring equal generated PCM byte counts and FNV-1a 64-bit stream digests:

| Route | Generated PCM bytes | Matching digest |
| --- | ---: | --- |
| Dam | 6,435,584 | `1d505126fa091a5c` |
| Caverns | 4,416,000 | `3fb892d7b6dacc8c` |

These digests cover the complete generated stereo music stream. Original host
PCM and saved-state golden vectors also pass under ASan/UBSan. The host suite
checks disabled/enabled profiling produces the same nonzero output, expected
opcode counts and stream digest. The comparison test rejects missing or changed
PCM verification. A digest is bounded evidence, not exhaustive mathematical
verification of every possible audio command stream.

All captured non-timing gameplay and drawing fields match in both pairs;
only performance measurements and NDSP refill/silence counts differ
(`saturation-field-audit.json`). NDSP remains active with zero error. Dam's
prepared blocks / silent frames change from 3,247 / 56,828 to 3,235 / 50,581;
Caverns from 2,170 / 9,099 to 2,170 / 8,958. Generated PCM identity does not mean
identical speaker delivery timing: the device consumes its queue in real time,
and expensive diagnostics can cause underruns during fixed-cadence replay.

Dam retains 26 PP7 shots, two damaging guard hits, 1,174 guard-fire dispatches,
one opening-guard death, player damage and zero unknown AI commands. The authored
route ends at frame 4,370 with Bond dead, identically in both captures. This is
combat validation, not a completed mission.

| Deeply instrumented replay | Mean ms | p95 ms | p99 ms | Peak ms | >16.67 ms frames | Skipped intervals |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Dam control | 13.6578 | 22.1701 | 25.7117 | 35.3279 | 1,218 / 4,250 | 116 / 4,249 |
| Dam candidate | 13.3092 | 21.4501 | 24.8219 | 34.6725 | 1,010 / 4,250 | 102 / 4,249 |
| Caverns control | 11.0992 | 18.3185 | 19.6813 | 23.8219 | 364 / 2,880 | 0 / 2,879 |
| Caverns candidate | 10.7462 | 17.5161 | 18.8313 | 22.7252 | 233 / 2,880 | 0 / 2,879 |

Dam music mean falls 3.6327 → 3.2882 ms with diagnostics, and Caverns music
3.7081 → 3.3581 ms. In Dam, envelope work falls 2.824 → 2.142 seconds and general
mixing 2.265 → 1.806 seconds. ADPCM and resampling benefit from the same clamp;
pole-filter work remains effectively unchanged. Unchanged opcode call counts,
PCM and gameplay checkpoints support attribution to the instruction change.

Per-opcode clocks and PCM hashing add about 1.2 ms per frame to these probes.
Do not compare their absolute budget counts with the earlier sound-cache pairs
or interpret their frame rates as normal gameplay performance. Detailed profiling
remains opt-in through `probe-detail.cfg`; normal play does not hash PCM or call
these diagnostic clocks. All probes ran without concurrent builds or tests.

## Remaining tail

The sound-cache Dam frame 541 illustrates overlapping costs: 32.9663 ms total,
5.3113 music inside 13.7061 canonical simulation, 4.481 world inside 5.714 renderer,
1.9757 guard upload, 1.440 guard replacement, and 2.738 first-person publication.
It also starts a previously unused sound (2.5169 ms) and rebuilds guard topology.
Some replacement geometry work is outside the stable-topology `guard_scene`
timer; its actor-transform timer can therefore exceed `guard_scene`. Do not sum
nested categories or treat the listed subphases as complete accounting.

The next recurring target is CPU world submission in busy Dam views, reaching
about 4.7 ms. The largest isolated spikes still include first-use sounds and
guard topology replacement; reducing those requires separate first-use asset
preparation and replacement-transaction measurements, not hiding missed retraces
or dropping actors. This pass retains simulation, audio and drawing fidelity.

Azahar settings were read and left unchanged: New 3DS, CPU 100%, JIT enabled,
frame limit 100%, Vulkan. All performance results here are emulator measurements;
no physical hardware 60 fps lock is claimed.

## Low-overhead live Caverns check

The normal-profile 3,000-frame Caverns runs also pass the complete existing
comparison gate: actual retrace cadence, input, initial RNG, checkpoints and
end state match despite using the live scheduler. The cache-only `c832e542`
control (`caverns-live-final.result`) has 7 / 2,880 warm frames above budget,
mean 8.1036 ms, p95 13.4440, p99 15.2282 and peak 18.5565. The `fa6dffe8`
candidate (`caverns-saturation-live.result`) has 3 / 2,880 above budget,
mean 7.7720 ms, p95 12.6310, p99 14.5114 and peak 18.0259. Both have zero
repeated or skipped intervals among 2,879 submissions, with 293 us GPU peak.

This comparison includes the small inactive diagnostic-hook overhead added
since the cache-only binary. PCM hashing is disabled here; the instrumented
pairs above independently verify generated music PCM. It remains a bounded
representative scene with three work-budget misses, not a campaign 60 fps lock.

## Low-overhead matched Dam replay

The same control and candidate were then replayed with `probe_detail=0`.
Both retain the prescribed cadence and all recorded gameplay checkpoints;
`dam-normal-comparison.json` passes. The authored firefight verifier also passes.
This isolates the normal-cost instruction change without opcode clocks or PCM
hashing. All 4,370 presentations finish at the same route target and state.

| Dam, diagnostics off | Mean ms | p95 ms | p99 ms | Peak ms | >16.67 ms frames | Skipped intervals |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Control | 11.6845 | 19.0394 | 22.2120 | 32.1999 | 460 / 4,250 | 51 / 4,249 |
| Candidate | 11.3385 | 18.5354 | 21.3103 | 31.4393 | 376 / 4,250 | 42 / 4,249 |

Music mean is 2.4180 → 2.0735 ms (14.2% lower), p99 7.0499 → 5.9771 ms,
peak 10.5113 → 8.8989 ms. Neither repeats a submission interval; previous GPU
execution peaks at 293 us for control and 312 us for candidate. These still
miss the combat 60 fps budget substantially.

The final Caverns run has only three work-budget misses. Its worst frame,
1,101, spends 5.376 ms importing a previously missing guard texture via
`ensure_stage_guard_scene_texture`, plus 0.934 ms upload and 0.641 ms geometry
replacement; total work is 18.026 ms. Frames 2,343 and 2,039 combine music
(5.851 / 5.291 ms) with props ticks (5.411 / 5.142 ms). Deep profiling was off,
so these props spikes must not be attributed to a particular function without
replaying that live cadence with diagnostics. For this representative context,
the next concrete target is preparing required guard textures before their
first visible use while retaining the same images and bounded texture memory.

## Reproduction and delivery

Build with `scripts/build_3ds.sh -j4`. Run `scripts/test_port.sh`, including the
ASan/UBSan audio producer/cache tests and replay/comparison validation. The
candidate's ARM build log is `saturation-build.log`; its focused logs are
`saturation-audio-tests.log` and `saturation-timing-tests.log` in the evidence
folder. The initial sound-cache full suite is `full-suite.log`.

The preserved runner `manage_probe.py` installs a selected executable, the exact
rendering-audit pack, stage/input and optional recorded retraces. It verifies
installed hashes at collection; the final runs also verify the installed retrace
file and requested diagnostic mode. Generate cadence files with
`scripts/make_3ds_retrace_replay.py`; compare two collected results with
`scripts/analyze_3ds_frame_timing.py --compare BEFORE.result AFTER.result`.
Input and retrace hashes are retained in each capture's `.provenance.json`.

The cache-only candidate remains available. No physical console is deployed,
no commit is created, and the user's 12-hour `caffeinate` session remains running.

Final verification: the full `scripts/test_port.sh` suite passes, exit 0,
recorded in `saturation-full-suite.log`; `git diff --check` is clean. The tested
`fa6dffe8` executable, exact pack, SMDH and catalog are preserved in
`build/3ds-candidates/music-saturation-fa6dffe8/`. Matching executable/pack/SMDH
hashes are staged at `build/3ds-sd/3ds/goldeneye-3ds/` and installed in Azahar.
Temporary stage, input, retrace and detail overrides are removed; the original
configuration inventory is restored. Source changes and performance evidence
are archived with the candidate, including its toolchain image identity.

Normal-startup smoke test passed after override removal: the candidate reached
the original frontend cast credits in Azahar. The emulator was then stopped
and returned to idle; the keep-awake process was retained. This checks startup,
not frontend visual parity.
