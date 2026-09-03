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

## Follow-up: localized articulated-prop topology publication

The live all-stage renderer now replaces only an articulated prop's model
parts when the canonical model changes node identities, part counts, or output
sizes. It uses the existing original model-instance enumeration, resident
matrix publication and shared model-component cache. Original object, AI,
animation, movement and weapon ticks are unchanged. The prior complete-scene
installer remains an explicit failure fallback; it is no longer the normal
response to a prop topology change.

The portable `ge_scene_part_replace` transaction validates the ordinary-part
prefix and trailing door/guard spans before updating geometry, metadata and
offsets. It retains room/neighbor/door/guard geometry without rereading assets,
then the runtime uploads the shifted overlay suffix. It still allocates/copies
the CPU scene when sizes change; this is not a zero-copy allocator. Empty door
sets now retain their insertion point, just as empty guard sets do. A completely
empty overlay also clears old door/guard segment bookkeeping.

Focused ASan/UBSan coverage passed repeated insert/grow/shrink/remove cycles,
same-size node switches, aliased source data/metadata, empty prefixes/tails,
capacity failure and invalid-layout rollback, with exact room/peer/door/guard
vertex and batch preservation. Evidence is in
`build/host-tests/prop-segment-replace-20260902.log`. Both complete host runs
passed (`prop-segment-full-20260902.log`, `prop-segment-final-20260902.log`),
including the source guard for empty door/guard insertion points. The final
ARM/3DSX build passed in `arm-prop-segment-final-20260902.log`.

The initial live report `dam-prop-segment-348fd814-no-transition.result` did
not hit a topology change and is not evidence for the new path. The final
build's repeat, `dam-prop-segment-94d4e71c-combat.result`, did:

- `articulated_publication=722,0,1,0`: one change, zero failures.
- `articulated_replacement=1,2671922,321,6`: command 321, six parts,
  2,671,922 ticks / 268,111,856 Hz = approximately **9.97 ms**.
- `stage_scene_install=1,1,...` and `overlay_refresh=1,0,0,0,...`:
  only the initial full installation, no subsequent full rebuild or failed
  door/guard/monitor refresh. All 52 guard topology replacements succeeded.

Earlier runs took 41 ms in the articulated full-rebuild frame and subsequently
up to 64 ms in guard publication. Neither spike recurred in this run. The new
path demonstrably removed the full-rebuild event, but different encounter/RNG
timing means the overall frame-tail distributions are not a controlled FPS
comparison. The final combat run presented 9,156 frames / 4,771 simulation
ticks, fired 29 PP7 shots with two guard hits, streamed 3,583 NDSP blocks, and
recorded zero guard-matrix/sight faults. It reached 12/160 route targets before
Bond died; it does **not** demonstrate end-to-end Dam completion. Post-warmup:
9,036 samples, 818 over 16 ms, 62 over 25 ms, six over 33 ms, one over 50 ms,
54 ms peak. The remaining largest cold first-person publication was 31 ms.

The same final binary also completed the 750-tick aim/look/fire sequence
(`dam-prop-segment-94d4e71c-aim.result`): 762 presented frames, 512 visible
canonical sight frames, zero sight failures. With no concurrent compilation,
616/642 post-warmup frames still exceeded 16 ms, nearly identical to the prior
aim repeat. Average measured work remains about 17 ms per presented frame;
aggregate simulation is 6,112 ms and render/GPU submission 5,837 ms across 762
frames (overlay work is included in simulation). **Sustained 60 FPS is not
established.** The next performance targets are active-aim simulation/guard
publication, render submission, and the cold first-person firing-layout path.
Do not redo existing material batching/delta/prepared caches, native float
conversion, room retention, or shared model-template retention.

Final executable SHA-256:
`94d4e71cf4651584b6523d64c8690ddabf993b30311c087b1ca4b2a7b1063085`.
Unchanged assets SHA-256:
`938536d47ee48aa275f97614886551889a5cbc7107726e6e433bd4ecd1fe3743`.
Build, hardware-stage and Azahar executable copies match; both staged asset
copies match. Probe routes/results remain private under `build/visual-probe`.
The active emulator probe configuration was moved there after verification,
restoring normal menu startup without changing saves or DSP configuration.

## Follow-up: first-use weapon hitch and active-frame costs

First-person layout changes now reuse matching immutable display-list parts
from the previous decoded layout, including their validated capacity queries,
vertices, materials and matrix indices. The new layout rebuilds its exact
cross-part transform map and still consumes current canonical hand matrices.
Keys include ROM resource identity, blob address/size, primary/secondary lists,
segment-4 offset and matrix count. No live matrix pointers are retained in the
component keys, and reuse cannot cross resources sharing a hand buffer.

The renderer preloads the selected weapon's complete authored texture table,
including inactive fire/reload-switch images. This moves texture import work
to initial weapon publication; it does not remove that work or guarantee
hitch-free weapon selection. It neither toggles model switches nor advances
gun state. First-person GPU publication also retains immutable RGBA between
layout changes, like its existing UV retention; every layout change still
republishes both, while pose updates write positions only.

World rendering first checks an actual source vertex with the exact scalar
clip transform. A visible point proves no unanimous outside plane exists,
avoiding the interval-bounds test in the common case. A failed point check
never rejects a draw: it continues through the existing bounds/full-vertex
path. Operation grouping, clip tangency, fail-open nonfinite behavior, room
visibility, draw ordering and authored material merging remain unchanged.
Guard room residency now uses an O(1) read of the same two fields previously
obtained through the full diagnostic snapshot. Retired/reused prop slots
remain invisible with room UINT8_MAX; canonical guard visibility stays owned
by chrTick.

Verification:

- Focused ASan/UBSan gun/modem suite passed, including 16 alternating authored
  SWITCH layouts compared against fresh decodes (source UV/color, eye/world,
  complete material batches), with actual component reuse asserted. Log:
  `build/host-tests/fp-components-20260902.log`.
- Early vertex + existing bounds/scalar composition matched all 20,000
  randomized reference decisions under ASan/UBSan, with tangency and invalid
  input checks. The full host run repeats this coverage.
- The complete host suite passed in `fp-opt-full-20260902.log`: all 42
  first-person resources' texture visitors matched their native authored
  tables, callback rejection was retained, and live/retired guard residency
  matched the full snapshot. Original gameplay, death, collision, gun/modem,
  sound, menus/watch, campaign and renderer coverage remain green.
- ARM/3DSX builds passed in `arm-fp-optimized-20260902.log` and
  `arm-fp-opt-final-20260902.log`.

Baseline profiling-only build `b6e0eae0...` reproduced the prior aiming cost
(`dam-fp-phases-b6e0eae0-aim.result`). The optimized run
`dam-fp-optimized-94a854c1-aim.result` and final build's repeat
`dam-fp-optimized-5258aa50-aim.result` both completed the identical 750-tick
aim/look/fire sequence, with no concurrent compilation:

| Measurement | Baseline | Final repeat |
| --- | ---: | ---: |
| Presented frames | 762 | 807 |
| Total measured frame work | 13,132 ms | 12,684 ms |
| Work per presented frame | 17.23 ms | 15.72 ms |
| Post-warmup frames over 16 ms | 617 / 642 | 205 / 687 |
| Post-warmup peak | 49 ms | 40 ms |
| First firing-layout publication | 31 ms | 7 ms |
| Guard matrix / sight failures | 0 / 0 | 0 / 0 |

The final noninitial first-person peak's four phase tick counts are
`1236979,151504,285442,159129` (cache, texture, UV, GPU vertex publication),
6.84 ms total at 268,111,856 Hz. Initial weapon publication is deliberately
excluded from that peak counter; totals still include it. The initial
profiling-only build used a peak including initial publication, so do not
compare those peak-phase rows directly. Component counters report 19 reused
parts and 21 decoded parts across the two layouts. Static-color retention
reduced aggregate GPU vertex publication from 115,696,639 to 14,727,989 ticks.
The early vertex check accepted 586,286 of 738,223 tested batches in the first
optimized aim run. The final repeat fired five PP7 shots and published 540
visible canonical sight frames; its additional displayed frames are not
additional simulation ticks. Azahar displayed 60 FPS during inspected aiming.

This is a repeatable improvement, **not sustained 60 FPS everywhere**. Nearly
30% of post-warmup aim samples still exceeded 16 ms. The remaining 40 ms frame
spent 27 ms in guard publication during a cold guard topology/component
transition. Warm actor/publication/render costs and those guard transitions
are the next measured performance targets. No hardware timing, all-level
completion or N64-reference audiovisual equivalence is claimed.

Final executable SHA-256:
`5258aa5099a954dcf8727471ab0563ecd6dbb232d8b7137546ad1fcd32ef77f3`.
Unchanged assets SHA-256:
`938536d47ee48aa275f97614886551889a5cbc7107726e6e433bd4ecd1fe3743`.
Build, hardware-stage and Azahar executable copies match, as do both staged
asset copies. Only source/tests/documentation belong in Git.

Final longer combat confirmation (`dam-fp-optimized-5258aa50-combat.result`):
8,200 presented / 3,964 simulation frames, 21 PP7 shots / four guard hits,
2,979 NDSP blocks, zero guard-matrix/sight faults, and 28 successful guard
topology replacements. The localized six-part command-321 prop replacement
still succeeded in 9.96 ms, with no full-scene rebuild after installation or
overlay-refresh failures. Post-warmup: 8,080 samples, 686 over 16 ms, 48 over
25 ms, four over 33 ms, **none over 50 ms**, 39 ms peak. First-person's
noninitial peak was 6.77 ms and its 19 shared parts were reused. The worst
remaining captured frame spent 24 ms in guard overlay publication. The input
route failed at Bond's death after 11/160 targets; it does not establish Dam
completion. Encounter timing differs from the prior combat runs, so these
tail counts are not a controlled overall FPS comparison. The active probe
configuration was moved to the private report directory after completion,
restoring normal startup. Saves/assets/DSP configuration were unchanged, and
no second Azahar process was launched.

## Guard texture residency, retained tail buffers, and material hashing

Continued from `d34df8de` without changing canonical gameplay bodies, authored
model relations, AI, animation, movement, collision, gun behavior, draw order,
or simulation scheduling. These changes are at the platform/publication boundary:

- Character texture enumeration reads the relocated tables of models already
  loaded for the stage's actual body/head choices. Scene installation imports
  hidden switch/LOD textures before play. Texture reconciliation includes and
  borrows these dependencies across room changes, but still releases obsolete
  room textures. It does not tick guards or consume RNG. Failed transactions
  retain the previous texture ownership; missing images remain reported.
- Model-scene texture visitation uses an exact 65,536-bit set instead of
  scanning every preceding draw batch. First-appearance order, eligibility,
  callback rejection and the complete 16-bit ID domain are unchanged.
- The final overlay segment can grow/shrink/clear inside retained CPU storage
  without copying resident rooms or earlier props. Allocations reserve at most
  25% of the overlay size, bounded by scene limits, not 25% of room geometry.
  If optional headroom allocation fails, exact-size allocation is retried.
  Validation and triangle-overflow checks precede all in-place mutation.
  Non-tail changes and growth beyond capacity retain the transactional path.
- Prepared materials retain 256 entries in two-way sets, with full material,
  texture pointer and fallback equality. Word-at-a-time hashing uses memcpy
  (no alignment/aliasing assumptions) and final mixing to distribute aligned
  pointers and authored enums. Hash collisions never establish equivalence.

The new `guard_refresh_peak` row reports the largest refresh after 120 rendered
frames: total, nested model-cache work, cache topology, cache vertex transform,
texture visitation/import, scene replacement, and actual texture import. Units
are system ticks at `runtime_profile_tick_hz` (268,111,856 in these runs); nested
phases are not additive. It excludes GPU upload and other overlay services.

Focused ASan/UBSan verification passed for all 71 character resources' texture
tables, hidden-texture borrow/import/abort/commit/capacity cases, full-domain
texture visitation, deliberate prepared-material collisions/eviction/key
mutation, and repeated guard-tail grow/shrink/clear transitions. Tail tests
compare the complete room vertex/batch prefix and pointer retention, including
overlapping input and invalid triangle-count rollback. Existing ordinary prop
insert/grow/shrink/clear and whole-overlay rollback cases still pass. The
gun/modem suite remains green (`guard-opt-gun-20260902.log`). Full host results
are in `guard-opt-full-20260902.log`; the final hashing helper was also compiled
directly from main.c and tested against real uncached material preparation.

No-concurrent-build Azahar aim/look/fire measurements (750 simulation ticks):

| Measurement | Profiling-only checkpoint | Final optimization |
| --- | ---: | ---: |
| Executable prefix | f99c8cb1 | 0797edaa |
| Presented frames | 807 | 836 |
| Measured frame work | 12,681 ms | 12,586 ms |
| Work per presented frame | 15.71 ms | 15.06 ms |
| Post-warmup frames over 16 ms | 199 / 687 | 113 / 716 |
| Post-warmup peak | 40 ms | 28 ms |
| Largest guard refresh | 22.87 ms | 12.12 ms |
| Texture import within that refresh | approximately 10.6 ms in isolated follow-up | 0 ms |
| Guard matrix / sight failures | 0 / 0 | 0 / 0 |

Private results: `dam-guard-phases-f99c8cb1-aim.result`,
`dam-guard-linear-46ae71c9-aim.result`, `dam-guard-resident-55d3e2a5-aim.result`,
`dam-guard-tail-25a71623-aim.result`, and `dam-guard-opt-0797edaa-aim.result`.
The intermediate seven-counter run isolated 10.55 ms of actual texture import.
The final cache reported 104,597 world preparation hits / 7,541 misses; pointer
placement affects conflicts, so miss counts alone are not an FPS measurement.
Scene-buffer replacement still costs about 6.05 ms on the cold growth that
exceeds reserved capacity; retained-buffer tests are not proof that every live
replacement avoids allocation. These results reduce hitches, not establish
sustained 60 FPS everywhere, hardware performance, audiovisual equivalence, or
mission completion.

Longer final-artifact combat replay (`dam-guard-opt-0797edaa-combat.result`):
9,627 presented / 5,131 simulation frames, 39 PP7 shots / six guard hits,
3,809 NDSP blocks, zero guard-matrix/sight failures, and 43 successful guard
topology replacements with no overlay-refresh failures. The localized
six-part command-321 prop replacement remained working (9.40 ms). Post-warmup:
9,507 samples, 837 over 16 ms, 43 over 25 ms, one over 33 ms, none over 50 ms,
41 ms peak. The largest guard refresh was 13.86 ms with zero texture-import
ticks; its cold scene replacement still took 6.82 ms. This encounter ran
longer and fired more shots than the prior checkpoint, so it is not a
controlled overall combat FPS comparison. Bond died after 13/160 scripted
targets; Dam completion remains unproven. The private probe configuration was
moved out of the virtual SD root afterward to restore ordinary menu startup.

Final full-suite repeat passed (`guard-opt-final-20260902.log`), including the
new texture and exact compiled material-cache tests. ARM/3DSX build passed in
`arm-material-wordhash-20260902.log`; the linked ELF retains MoveBond,
bondviewProcessInput, ge_original_gun_live_tick,
ge_original_stage_active_props_tick_exact and both new texture APIs.

Final executable SHA-256:
`0797edaa862003dfd1925a06c073519d62a176bf74c46ea58a04827154e0b975`.
Unchanged assets SHA-256:
`938536d47ee48aa275f97614886551889a5cbc7107726e6e433bd4ecd1fe3743`.
Build, hardware-stage and Azahar executable copies match; both staged asset
copies match. Saves and emulator DSP configuration were untouched. Only the
existing Azahar instance was used. Source/tests/docs are the public checkpoint;
ROM-derived assets, executable artifacts and local probe outputs stay private.
