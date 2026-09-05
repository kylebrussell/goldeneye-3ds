# Audio CPU optimization — 2026-09-04

Follow-up: [world submission and actor publication](Verification20260904VisibleRuns.md)
records the next optimization pass and supersedes this staged executable.

Continues [SinglePlayerParity.md](SinglePlayerParity.md), with nonzero music
volume and original SFX enabled. New 3DS XL remains the primary 60 FPS target;
these changes use the shared ARM11 path and retain original 3DS compatibility.
No physical hardware timing is claimed.

## Retained implementation

The ABI1 interpreter retains the original producer, 736-sample audio frames,
22050 Hz output, 30 Hz audio scheduling, voices, effects and command order.

- Resampling uses a 32-bit accumulator: the largest absolute coefficient-row
  sum is 34,117, so every partial sum is at most 1,117,945,856 in magnitude
  for signed 16-bit inputs. Source positions also fit after the existing
  output-range check. Contiguous DMEM taps avoid four separate history checks;
  all four samples are loaded before output is written, preserving overlap.
- ADPCM and resampling floor division use arithmetic right shifts. Compile-time
  assertions pin signed-shift behavior on the target, as already required by
  the envelope mixer and pole filter. Saturation and rounding remain exact.
- Envelope mixing reuses gains after both volume ramps stop. The dry and wet
  channel operations are explicit, retaining their source-read and output-write
  order even for overlapping or unaligned DMEM. Ramping samples still execute
  the original 64-bit ramp arithmetic; zero-length commands retain their state.

No renderer, gameplay timing, visibility, audio quality or hardware mode has
been changed by this optimization pass.

## Exact-output validation

The audio producer test now runs at `-O2` with ASan and UBSan. Existing PCM
golden vectors and atomic-error checks remain. The new deterministic fixtures
compare the complete DMEM and saved-state digest against the previous scalar
implementation (`build/host-tests/fps-audio-pass/baseline.c`):

| Fixture | Coverage | Digest |
| --- | --- | --- |
| Existing pitch sweep | All 65,536 pitches, range errors, history, overlap | `abfbe11a` |
| Full-scale resampling | All 64 rows × 16 sign patterns × 6 pitches, overlap/odd addresses | `41fa0db5` |
| Envelope mixing | 4,096 seeded init/continue, stationary/ramping, dry/wet, overlapping cases | `1f5e37d0` |
| ADPCM | 4,096 seeded commands, all scales and eight predictors, extreme codebooks/history | `d2ddec30` |

Private baseline and candidate sanitizer logs are under
`build/host-tests/fps-audio-pass/`. The final focused result is
`candidate-shift-test.log`; the ARM build is `arm-shift.log`.

## Experiments

The resampler-only Dam trace reduced music ticks from 811,621,046 to
689,857,832. Explicit channel mixing and settled-gain reuse reduced that to
580,085,963. A separate ADPCM experiment clamped in the accumulator domain
before narrowing, but produced 580,796,961 music ticks and no meaningful
whole-frame improvement. It was discarded in favor of arithmetic shifts.
Its trace is `fps-audio-rejected-clamp-dam750.result`, not the final build.

All comparisons use the existing move/look/fire trace and unchanged asset
pack, with no concurrent builds. The original simulation consumes real
retrace timing: encounters, damage, movement endpoints and SFX counts can
change when CPU cost changes. These are useful frame-tail measurements, not
identical-workload whole-game benchmarks. A completed input trace is not a
completed mission. Reported millisecond counters are integer instrumented
emulator work; they do not certify physical-hardware or sustained 60 FPS.

## Final short traces

Baseline executable: `babce586ae48dacd4505cff2096ddd96b21ba2b0e7ad560e6d8f3097a7dfc7a1`.
Candidate executable: `525de37c942aa48238c426cdd3b2f9cd261ccce4652c1eea8d1bb7eee06f186a`.
Asset pack: unchanged `ee769251742b72bcaa9a3d1586794246355bc995dc62637e9755dc276adfdeb7`.

| Stage | Total work, baseline → candidate | Music ticks, baseline → candidate | >16 ms, baseline → candidate | Warm peak |
| --- | ---: | ---: | ---: | ---: |
| Dam | 10,277 → 8,918 ms | 811,621,046 → 571,955,975 | 148 → 72 / 630 | 28 → 25 ms |
| Caverns | 7,880 → 7,199 ms | 676,409,564 → 500,614,204 | 78 → 45 / 630 | 22 → 19 ms |

Evidence: `build/visual-probe/fps-audio-{baseline,final}-{dam,caverns}750.result`.
All four runs complete 750 original movement/actor ticks and music-service
calls, with healthy actor status and zero sound decode failures. Main music
volume is 26,212. Caverns' endpoint and non-timing gameplay/draw/publication
counters match; its measured total work is 8.6% lower and music work 26.0%
lower. Dam's encounters/endpoints differ, so its 13.2% total-work reduction
is not a claim of identical-workload speedup; music work is 29.5% lower.
Neither final short trace has a post-warmup frame above 25 ms. Both still
have frames above the 60 FPS budget.

## Extended combat and remaining limit

`build/visual-probe/fps-audio-final-dam-combat.result` runs the existing
10,000-frame-maximum authored route. Bond dies after 5,040 presentations /
5,041 original movement, actor and music ticks, at target 13/160. This is
mission failure, not a crash or mission completion. Actor status is healthy;
759 sounds decode with zero failures, NDSP remains active, and death music
switches to track 27 at nonzero volume. Door/guard/monitor refreshes report
no errors. A live capture shows textured guards/world/HUD at 54 FPS.

There are 1,680/4,920 post-warmup frames above 16 ms, 19 above 25 ms, one
above 33 ms and none above 50 ms; the warm peak is 36 ms. The prior parity
combat run had 1,732/4,964 above 16 ms and 122 above 25 ms, but died at target
11/160 with fewer sound and world draw calls. The optimized run draws 506,215
world batches versus 365,710 and performs more guard publication. Do not
interpret their aggregate totals as an identical-workload speedup.

Music takes 3,756,385,407 ticks (14.0 seconds) across this run, compared with
5,265,143,680 in the prior 5,085-tick encounter. World submission is now
4,422,926,303 ticks (16.5 seconds); nested overlay work totals 9,542 ms.
These nested/overlapping regions must not be added to simulation totals.
The audio gains are retained, but sustained combat still misses 60 FPS.
Next targets are actor/scene publication and world submission in busy rooms,
followed by physical New 3DS XL and original 3DS measurements.

Azahar was configured for New 3DS, CPU clock 100%, JIT enabled and frame
limit 100%. Settings were read, not changed. Emulator timings still do not
substitute for hardware measurements.

## Delivery

`make test-3ds-port` passes after the final changes, exit 0:
`build/host-tests/fps-audio-pass/full-suite.log`. The final ARM build also
passes (`arm-shift.log`). The focused audio suite compiles at `-O2` with
ASan/UBSan and matches all four PCM/state digests.

The tested `525de37c` executable is saved under
`build/3ds-candidates/audio-525de37c/`, installed in Azahar, and staged at
`build/3ds-sd/3ds/goldeneye-3ds/goldeneye-3ds.3dsx`. The previous `babce586`
candidate remains available. Temporary stage/input overrides were moved to
`build/visual-probe/fps-audio-final-used-*.cfg`, restoring normal startup.
Assets, saves and DSP files were not manually changed. This is a staged
hardware build, not a physical deployment or FPS certification.

The final normal-boot UI recheck was blocked by the Mac locking after the
performance runs and regression suite finished. Override removal and all
staged hashes were verified; no lock or security setting was changed.
