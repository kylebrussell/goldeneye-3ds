# Native 3DS verification checkpoint — 2026-09-02

The port is still not a demonstrated 1:1, sustained-60-FPS, end-to-end release.
This checkpoint resumes after the host reboot and preserves the frontend work
already present in the working tree. No ROM, extracted assets, firmware, saves,
or executable is included in source control.

## Changes and verification

- Model-scene publication exposes coalesced changed vertex/batch ranges. The
  live guard overlay copies and uploads those ranges and refreshes UVs on
  topology changes even when geometry counts stay the same. Existing cache
  reuse was already live; it is not a newly implemented character renderer.
- s15.16 matrix conversion uses native signed-32-bit truncation after the
  existing finite/range checks. 16,384 sampled elements and boundary values
  match the old signed-64-bit conversion bit-for-bit. ARM object inspection
  confirms removal of `__aeabi_f2lz` from the model-scene object.
- Static room batches cache world bounds at GPU publication time. Conservative
  interval transforms resolve wholly inside/outside batches; intersecting
  bounds fall back to the existing exact vertex test. Animated overlays keep
  the old path. No authored draw ordering, portal state, AI visibility, or
  geometry changes. Randomized sanitizer coverage plus 55,936 comparisons
  across real Dam geometry at 64 camera headings match exact decisions.
  The focused optimized host test measured 51 ms scalar versus 18 ms bounded
  over 256 iterations. This is not an emulator FPS claim.
- Canonical impact/smoke/scattered-explosion/flare image records are now bound
  to native pointers in the generated damage slice. Previously the extracted
  uninitialized `impactimages` pointer produced Azahar reads at `0x58/0x59`
  during wall impacts. The unchanged impact constructor now passes ASan/UBSan
  for all 20 authored impact types and their UV dimensions.
- Nintendo-logo lighting/reflection and authored material state changes from
  the preceding frontend pass remain preserved, with focused tests.

The full host suite passed after the renderer changes and again after the
integrated impact-table binding and matrix-failure diagnostics. The focused
stage ASan/UBSan suite also passed the injected nonfinite-matrix diagnostic
case. Final ARM/3DSX build passed. Local logs live in `build/host-tests/`:
`verification-20260902-final-integrated.log`,
`stage-matrix-diagnostic-20260902.log`, and `arm-final-20260902.log`.

## Emulator evidence and audio

Private reports live in `build/visual-probe/`. The 1,200-tick bounds run
(`dam-bounds-05c4f383.result`) completed all targets with healthy actor status.
It measured 23,473 ms of application work over 1,862 presented frames, with
539 of 1,742 post-warmup samples exceeding 16 ms. Stable 60 FPS is not proven.
Comparisons across these live runs are not controlled benchmarks: AI/RNG,
presented-frame counts and concurrent host verification varied.

Azahar HLE does **not** require a console DSP dump. Following devkitPro's
[audio setup notes](https://github.com/devkitPro/3ds-examples/blob/master/audio/README.md),
an empty file was installed only at the emulator's virtual
`sdmc:/3ds/dspfirm.cdc`. The hardware staging tree has no such file. Real
hardware still needs the component or Homebrew Menu's `hb:ndsp` handle.

`dam-hle-audio-01873f2c.result` records `ndsp=1,00000000,3374,103671`:
successful initialization and 3,374 prepared output blocks (the final field
is silence padding). The same run registered four PP7 guard hits and reached
Bond's death/report-ready transition with healthy actor status. This proves
NDSP delivery, not a listening comparison or complete audio fidelity.

## Highest-priority unresolved failure

Long firefights intermittently stop actor scheduling with
`GE_ORIGINAL_STAGE_GUARD_RUNTIME_MATRIX_UNAVAILABLE`. This is not an expected
death status. The second instrumented run reproduced it on chr 44 while
validating body matrices immediately after `runtime_retain_matrices` or the
renderer-only `subcalcmatrices` calculation. The diagnostic source line in
that exact a7180f2a binary was 1450; later edits change the line number.
Other runs completed the death transition, so a passing retry is not a fix.

The runtime now captures the failing source line, authored chr ID, matrix
index, whether the matrix was retained from the transient frame arena, and
all 16 values. Next: reproduce with this richer diagnostic and investigate
canonical matrix ownership/lifetime and unreferenced matrix slots. Do not
mask invalid matrices, skip actor scheduling, or replace the original pose.

The ordinary launch was restored by removing the temporary input-probe config.
Only one Azahar process was used. Azahar's separate-screen layout can still
create two legitimate screen windows; process count alone does not establish
window count. No layout setting was changed.

Final local executable SHA-256:
`26ea1a28fded0e5685eb6dcedd25e7c3a09f05c403c65c7622eea4564e49ccc2`.
Unchanged asset pack SHA-256:
`938536d47ee48aa275f97614886551889a5cbc7107726e6e433bd4ecd1fe3743`.
The code/data pair is byte-matched across the build, SD staging directory,
and Azahar virtual SD card. The last gameplay probe used 01873f2c; the final
executable differs only by null-argument guards on the diagnostic accessor.

End-to-end Dam objective completion/bungee, all-level gameplay, menu/attract
fidelity, audible music/SFX quality, and New 3DS XL performance still require
direct verification. Static world GPU buffers and the canonical shared prop
dispatcher were already live; do not replace them with duplicate systems.

## Follow-up: culled-model ownership and audio queue

The renderer's early visibility/residency/sphere-cull branches left body and
attached-model pointers in the shared transient frame arena. A subsequent
offscreen canonical tick need not publish new matrices, so arena reuse could
leave the renderer retaining unrelated bytes. This is independently
reproducible: the new test failed at the durable-body-pointer assertion before
the fix. Those branches now retain the original matrix bytes in the existing
instance-owned slots without recalculating a pose or marking it drawable.
The same ownership boundary covers attached hats and the nonresident player
body/weapon. No animation, movement, AI, or canonical scheduling bodies changed.

ASan/UBSan tests poison the old transient arrays after visibility, residency,
and sphere culls, then expose the models again. Body/weapon/hat/player cases
pass, including the existing 665 authored guard pairs across 21 stages and
300 authored hats. This fixes a real lifetime defect; it does **not** establish
that every observed matrix failure had that cause.

The platform PCM ring now transfers each block with at most two contiguous
copies instead of a modulo operation per stereo frame. Read/write/discard
cursors remain correct for arbitrary capacities; zero-fill and counters are
unchanged. 40,000 randomized operations across eight capacities match the old
scalar samples, cursors, and accounting under ASan/UBSan. ARM object inspection
confirms that `ge_audio_output.o` no longer imports `__aeabi_uidivmod`.
This is an instruction-cost reduction, not a measured whole-game FPS gain.

### Emulator results and exact remaining blocker

Private reports are in `build/visual-probe/`:

- `dam-matrix-lifetime-ec5ba27b.result`: 4,500 simulation/actor ticks,
  11/11 route targets, four PP7 guard hits, healthy actor status, no matrix
  failure, 3,471 NDSP blocks.
- `dam-matrix-lifetime-d6e5baf3.result`: 4,431 simulation/actor ticks,
  11/11 targets, three guard hits, Bond death/report-ready, healthy actor
  status, no matrix failure, 3,375 NDSP blocks.
- `dam-extended-lifetime-audio-70f0c57d.result`: **failed**, 12/160 authored
  targets and four rooms. At actor tick 4,421 the matrix check failed on chr 5,
  matrix 0, retained=1, all 16 floats NaN. The exact failure line in this
  artifact was 1508. Actor status became `8,0,0,1,1,0,14,0`; the route later
  reached Bond's death state. NDSP remained active with 3,689 blocks. This is
  not an end-to-end completion or a healthy death-flow pass.

The last run had 1,249 of 7,743 post-warmup samples above 16 ms, including a
229 ms peak. Host verification ran concurrently and the encounters vary with
AI/RNG; these runs are not a controlled before/after performance benchmark.

The final diagnostic additionally retains both camera matrices, model/root
and character-aim inputs at the failure instant, plus raw failing-matrix bits.
A sanitizer test verifies those inputs do not change when live state later
changes. Next: rerun the extended route with these diagnostics and distinguish
invalid canonical pose inputs/output from subsequent memory overwrites. Do
not hide the failure by dropping matrices, skipping actors, or clearing NaNs.

macOS locked before the next diagnostic run could start. The last gameplay
artifact tested was 70f0c57d; later changes only add failure diagnostics.
The temporary input-probe config was removed to restore ordinary menu launch.
Only one Azahar process was used. HLE's emulator-only empty DSP marker remains;
the hardware staging directory still has no firmware or empty marker.

Final executable SHA-256:
`46c751dea147f31ca7fa826a48bac32020cfb4f87304ec876ce06d482793bdba`.
Assets remain
`938536d47ee48aa275f97614886551889a5cbc7107726e6e433bd4ecd1fe3743`.
The build, SD staging directory, and Azahar installation are byte-matched.
The final full host suite, focused ASan/UBSan cases, and ARM/3DSX build passed.
Final verification logs: `build/host-tests/full-lifetime-checkpoint-20260902.log`
and `build/host-tests/arm-lifetime-checkpoint-20260902.log`.

## Follow-up: original guard quaternion blends

The remaining pose investigation found a restricted early adapter in
`ge_port_player_gait_build_group_quaternion`: it returned without writing any
matrix unless the model was the active Bond gait, and rejected auxiliary
matrix flags `0x100/0x200`. Canonical `process_02_position` also calls this
path for guards whenever `unk84` enables animation merging. This could retain
old transient arena bytes instead of the current blended pose.

That adapter is removed. The generator now extracts the entire original
`sub_GAME_7F06DB5C` body, including both auxiliary-matrix branches and the
original parent-dependent joint callback. The only adaptations are its public
symbol, pointer-sized locals/casts, and the correctly typed two-argument joint
callback. No pose sanitization, actor skipping, or replacement animation logic
was added.

Verification:

- A non-gait model with poisoned matrix slots failed the finite-matrix
  assertion before the change. It now passes with absent/base/model-node
  parents, all four auxiliary-flag combinations, exact primary transforms,
  and callback identity/count checks.
- A ROM-backed Russian soldier/Karl pair advances the original animation
  clock through walking, firing, injury, death, and one-handed firing records:
  200 frames, including 92 blended frames. All authored matrix slots are
  poisoned before each real `subcalcmatrices` call and must be rewritten to
  finite values. ASan/UBSan passes alongside the existing 34 cast actors,
  22 animation records, and 14 weapon models.
- Full host suite and ARM/3DSX build pass. Logs:
  `build/host-tests/quaternion-regression-before-20260902.log`,
  `quaternion-authored-regression-20260902.log`,
  `full-quaternion-20260902.log`, and `arm-quaternion-20260902.log`.

Azahar was unlocked. Two baseline retries of 46c751de ended in Bond death
without reproducing the intermittent matrix error (4,303 and 4,138 gameplay
ticks, 11/160 targets). The fixed build's report,
`build/visual-probe/dam-canonical-quaternion-08505980.result`, records 4,951
simulation/actor ticks, 13/160 targets, healthy actor status, no matrix failure,
35 PP7 shots/three guard hits, Bond death/report-ready, and 3,795 NDSP blocks.
It passed the prior failure area but did **not** complete Dam. The deterministic
poisoned-buffer tests establish the fixed defect; one successful live retry
alone would not establish absence of every possible matrix bug.

Performance remains incomplete: 1,126/8,910 post-warmup presented frames took
more than 16 ms; peak 229 ms. The slow-frame capture places 215 ms of that peak
inside overlay work, with full scene rebuilds near presented frames 1,979 and
1,983. One guard-topology replacement returned INVALID_ARGUMENT and fell back
to full rebuilding. Investigate that renderer transaction boundary next;
do not weaken validation or discard geometry. Host tests ran concurrently, so
this is not a controlled FPS comparison. Audio delivery remains verified,
not a listening fidelity comparison.

Executable SHA-256:
`08505980a5914d14c47cf3d7a9627a3715c298ee661655f75b00ae925401de8b`.
Assets remain
`938536d47ee48aa275f97614886551889a5cbc7107726e6e433bd4ecd1fe3743`.
The build, SD staging directory, and Azahar installation are byte-matched.
The temporary input-probe config was moved back into private probe output to
restore normal launch. No ROM-derived assets, firmware, or saves are committed.
