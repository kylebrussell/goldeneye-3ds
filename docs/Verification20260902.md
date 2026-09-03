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

## Follow-up: guard-tail and weapon-layout publication

The live all-stage scene installer did not initialize the insertion offsets
for an empty guard segment. When guards became visible it inserted their
geometry before ordinary props/doors, while subsequent publication assumed
the guards occupied the tail. The older Dam-only installer already preserved
that boundary. Both paths now retain the tail offset even with zero guards;
no validation or fallback behavior was weakened.

The regression first failed on the missing assignments. Sanitizer coverage
now repeats 24 empty/single/multiple/empty guard transitions while preserving
the ordinary-prop prefix, room data, batch indices, and segment boundaries.
The intermediate Azahar report
`build/visual-probe/dam-empty-guard-tail-9d910345.result` records 27 successful
topology replacements, zero replacement failures/full-rebuild fallbacks,
4,060 healthy actor ticks, and no matrix failure. Two full installs remained;
the initial report covered guard/door/monitor fallback counts but did not
report articulated-prop fallback counts. They must be distinguished before
attributing the remaining full rebuild to room residency alone.

First-person rendering now retains one previous immutable decoded layout.
Original weapon visibility switches can alternate between the two layouts
without decoding the same GBI repeatedly. Matrices, hand state, gameplay, and
output buffers are not retained in the spare slot. Resource identity joins
the cache key because the original hand buffers are reused between weapons.
Allocation failure simply leaves the uncached decode available. Layout
changes also invalidate/remap UVs and make newly referenced textures resident,
even if the weapon resource itself has not changed.

The exact gun/modem sanitizer test warms two authored PP7 SWITCH layouts and
alternates them 16 times using canonical model relations: 16 cache hits, no
new decodes. It compares renderer-consumed vertex fields and complete batches
against a freshly decoded cache each time. Decode-time clip/NDC/screen fields
are not compared across old/new poses because the projection-only first-person
renderer does not consume those template intermediates. The focused host
measurement was 273 us warm versus 4,157 us cold, not an emulator FPS claim.

The intermediate live report
`build/visual-probe/dam-weapon-layout-cache-843c74e4.result` records 11,587
first-person builds, two decoded layouts and 45 layout reuses. The previous
recurring roughly 30 ms first-person stalls no longer appear in its retained
slow-frame samples. Actor scheduling stayed healthy for 4,842 simulation
ticks; 23 PP7 shots/two guard hits and 3,635 NDSP blocks were recorded. Bond
died at 11/160 route targets: **the route failed, Dam was not completed**.
There were still 742/11,466 post-warmup samples over 16 ms and a 213 ms peak,
including 205 ms of overlay work during a full scene rebuild. These live runs
have differing AI/RNG, encounter duration, and host load; do not treat their
frame-tail counts as a controlled FPS comparison.

## Follow-up: avoid the duplicate prop/door sizing traversal

Room installation already queries each model to size its combined buffers.
It now passes that preflight to the writer instead of repeating the count
traversal. The write pass still traverses/validates all GBI commands and checks
every output bound. All list/vertex/batch/triangle/command counts must match
before publishing; failed or stale preflights cannot publish a scene. No
extra persistent geometry cache or authored state changes were introduced.
The output command-address fields are copied explicitly into the zeroed batch
to avoid copying indeterminate structure padding from traversal temporaries.

Focused ASan/UBSan coverage compares complete vertex/batch output for PP7,
window, and Dam gate models, checks aliased query/output variables, and
rejects failed/stale/overflowing queries, undersized output and invalid lists.
A bogus zero-sized query with null output pointers still hits the write
callback's bounds check. The focused host writer measured 95/14/52 us versus
164/27/93 us for the normal two-pass builder, respectively. This isolates
model publication only and does not establish the remaining room-hitch cost.

The full host suite and ARM/3DSX build pass for the combined changes. Logs:
`build/host-tests/preflighted-model-20260902.log`,
`full-renderer-publication-20260902.log`, and
`arm-renderer-publication-20260902.log`. Earlier focused logs include
`first-person-layout-regression-20260902.log` and
`full-empty-guard-tail-20260902.log`.

## Follow-up: retain room geometry during whole-overlay replacement

The next phase-timed report,
`build/visual-probe/dam-overlay-phases-4c4d585f.result`, did not reproduce the
mid-combat full rebuild: it had one initial install, no articulated-prop
failures/topology changes, 4,129 healthy actor ticks, and a 63 ms post-warmup
peak. It is **not** evidence that the intermittent full rebuild disappeared.
It did isolate initial installation: approximately 8.6 ms query, 9.3 ms prop
build, less than 0.1 ms guard build, **160.9 ms overlay transaction**, and
43.7 ms texture/metadata publication.

Inspection found that `ge_dam_dynamic_scene_set_overlay` always called the
resident-room asset loader and GBI builder, even though only overlay geometry
changed. It now shares the atomic overlay-segment writer, retaining the
already-decoded room prefix. The existing API's generation/update counters,
capacity checks, local-to-published batch rebasing, and failure atomicity are
preserved. Room loading/eviction transactions still own actual room decoding.
This removes redundant work; it does not skip room geometry or change original
model, AI, collision, or visibility behavior.

The focused regression fails on the old implementation when the test supplies
an asset pack with no entries after initial room loading. The fixed code
passes 13 overlay-only publications without needing that pack, including
shrink/clear/grow cycles and aliased old-overlay input. Every resident room
vertex and batch remains byte-identical, along with residency/age metadata.
Invalid batches, overflowing triangle totals, and total-scene vertex/batch
capacity failures preserve the published scene and pointers. Existing
24-cycle guard-tail and room-stream/eviction tests remain part of the suite.
Logs: `build/host-tests/whole-overlay-before-20260902.log` (expected failure)
and `whole-overlay-replacement-20260902.log` (ASan/UBSan pass).

Final live report: `build/visual-probe/dam-retained-room-overlay-264dec72.result`.
The full rebuild recurred, now with `articulated_publication=724,0,1,1`:
one articulated-prop topology change/fallback, not a guard-tail failure.
Its displayed frame took 51 ms (41 ms overlay work), versus 207 ms (196 ms
overlay work) in the earlier `dam-preflighted-room-3d33d0c4.result` run.
The last install's phases were 8.7/9.3/<0.1/**5.1**/1.1 ms, respectively.
Thus the room-preserving transaction is live; unrelated encounter timings
and aggregate FPS remain uncontrolled, not a general speedup measurement.

This run recorded 5,109 healthy simulation/actor ticks, zero matrix faults,
35 PP7 shots/five guard hits, 51 successful guard topology replacements with
zero failures, and 69 first-person layout reuses after two decodes. NDSP
remained active with 3,838 output blocks. It reached 14/160 route targets and
Bond death: **the route still failed and end-to-end Dam remains unverified**.
There were 1,078/8,974 post-warmup samples over 16 ms, nine over 33 ms, and a
68 ms peak. Stable 60 FPS is not achieved. Residual stalls include the first
weapon-layout decode (~39 ms), cold/changed guard publication (~56 ms overlay
work), and the still-global articulated-prop rebuild/upload (~41 ms).

Final full host suite and ARM build passed:
`build/host-tests/full-retained-room-overlay-20260902.log` and
`arm-retained-room-overlay-20260902.log`. The exact executable SHA-256 is
`264dec72082d775c014908665993444b00caadd01d4370f9d32b163b338dd6fb`;
asset pack remains
`938536d47ee48aa275f97614886551889a5cbc7107726e6e433bd4ecd1fe3743`.
Build, SD stage, and Azahar virtual SD copies are byte-matched. The temporary
input config was moved to `build/visual-probe/dam-publication-264dec72-used.cfg`
to restore ordinary menu launch. Only one Azahar process was used; no firmware
marker is present in the hardware staging tree. Audio output is verified, not
audible N64-reference fidelity.

Next: reduce remaining cold/changed-model publication and replace only the
affected articulated-prop segment, then validate player-driven objectives and
the Dam exit. A separate visible fidelity gap was also confirmed by source
inspection: the live HUD still draws the legacy synthetic crosshair from
`build_crosshair_from_gbi` unconditionally outside watch/credits. Replace that
with the canonical sight-rendering path/visibility state, not a recolored
placeholder. No crosshair change was made in this checkpoint.

## Follow-up: original sight and cold model-template publication

The legacy always-visible, firing-recolored crosshair has now been removed.
The live renderer executes the original `gunDrawSight`,
`display_image_at_position`, `draw_textured_rectangle`, and
`texSetRenderMode` bodies, with only native display-list pointer corrections.
It uses the authored `s_crosshairimage`/`CROSSHAIR1` image, player aim position,
gunsight suppression bits, multiplayer-menu check, widescreen sizing, clipping,
flipped texture coordinates, and environment alpha 110. The platform adapter
captures `texSelect`'s image binding instead of emulating N64 texture allocation.
The isolated capture inherits gameplay's bilinear filter. Source guards check
the retained bodies against the decomp; there is no replacement aiming logic.

The PICA translator previously lacked texture-times-environment RGB/alpha,
which the original `G_CC_FADEA` sight commands require. It now maps those
channels independently to texture-times-constant, preserving mixed
environment/primitive channels. Unrepresentable chained two-cycle modulation
remains explicitly flagged as a fallback; this is not full RDP equivalence.
The existing material enum values and asset format are unchanged.

First-person and shared model-component cold decodes now reuse their already
completed capacity query when recording matrix-index templates. Previously
each did count, count again, then write. They now do count then write with
the same command validation, storage bounds, and final count checks. Focused
ASan/UBSan tests compare all vertex/material/matrix-index output and reject
undersized matrix-index buffers, stale command totals, and invalid queries.
The PP7/window/gate test writer measured approximately 115/17/59 us against
184/30/115 us for the old two-pass writer in this host run. These are isolated
host decode measurements, not a promised gameplay FPS increase.

Both complete host runs passed, including the existing original stage,
guard/action/death, weapon/modem, sound, menu/watch/exit, and renderer tests:
`build/host-tests/canonical-sight-20260902.log` and
`canonical-sight-final-20260902.log`. Focused sight coverage also tests exact
GPU combine state/opacity, both aspect ratios, suppression, clipping, and
invalid floating-point state. Final ARM/3DSX build passes in
`arm-canonical-sight-final-20260902.log`. The linked ELF retains
`ge_original_gun_draw_sight_exact`, the rectangle body, sight snapshot,
preflighted matrix-template writer, and unchanged `MoveBond`.

The first combat report, `dam-canonical-sight-e2a0f1f6-combat.result`, recorded
9,618 presented / 5,221 healthy actor ticks, 37 PP7 shots / three guard hits,
zero guard-matrix faults, and 3,917 NDSP blocks. The canonical sight correctly
stayed suppressed throughout this hip-fire route. It reached 12/160 targets
and Bond died: **end-to-end Dam is still not demonstrated**. Of 9,498
post-warmup samples, 865 exceeded 16 ms, ten exceeded 33 ms, and two exceeded
50 ms, with a 68 ms peak. The cold weapon-layout frame spent 31 ms in
first-person publication (previous checkpoint: 39 ms); guard publication
still reached 56 ms, and the articulated full rebuild still cost 41 ms.
Encounter/RNG differences prevent treating these runs as a controlled FPS
comparison.

The final executable's dedicated aim/release/look/fire probe completed all
750 simulation ticks and five PP7 shots. Reports
`dam-canonical-sight-1cb7526a-aim.result` and `...-aim-repeat.result` each record
512 visible sight frames, zero sight failures and zero matrix faults. The
original red textured sight was directly inspected in Azahar. The repeat had
no concurrent builds, yet 616/642 post-warmup frames exceeded 16 ms; the UI
showed about 56 FPS during aiming. Thus the combat average does not establish
60 FPS during active aiming. Preserve this distinction in future reporting.

Final code SHA-256:
`1cb7526a465b2f7cffdf2121ceefc225fbd7675bc1d0438376008ceb7777e458`.
Assets remain:
`938536d47ee48aa275f97614886551889a5cbc7107726e6e433bd4ecd1fe3743`.
Build, hardware staging, and Azahar copies match. Neither the binaries nor
ROM-derived assets are committed.

Next priorities remain the shared guard/first-person publication and active
aiming frame budget, per-articulated-prop topology replacement instead of the
whole overlay rebuild, and player-driven Dam objective/exit verification.
Do not recreate the material delta cache, resident-room retention, canonical
movement/AI, or crosshair: these are already live. All-level completion,
N64-reference audiovisual fidelity, and New 3DS XL performance remain unproven.

Final sustained-run confirmation (`dam-canonical-sight-1cb7526a-combat.result`):
8,965 presented / 3,964 healthy actor ticks, 20 PP7 shots / two guard hits,
zero matrix/sight failures, 3,000 NDSP blocks, and 39 first-person topology
reuses after two decodes. This route also failed at Bond's death (11/160
targets). Its 8,845 post-warmup samples include 708 over 16 ms, seven over
33 ms, three over 50 ms, and a 75 ms peak. The worst guard-publication frame
spent 64 ms in overlays; the articulated rebuild remained 41 ms and cold
first-person publication remained 31 ms. The world audit additionally passes
with seven combiner diagnostic buckets (`canonical-sight-world-audit-20260902.log`).
The probe configuration was removed from the emulator's active location to
restore normal menu startup. Private copies of the input routes and reports
are retained in `build/visual-probe/`; saves and hardware DSP configuration
were not changed. No concurrent Azahar process was started.
