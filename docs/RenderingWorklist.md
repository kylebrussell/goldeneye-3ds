# Rendering worklist

This pass independently checks the shared renderer against original commands,
SDK semantics and controlled tests. A fallback flag is evidence to inspect,
not proof of a visible defect. Full campaign parity and hardware 60 FPS remain
separate acceptance goals.

- [x] Preserve the current executable/assets and record baseline provenance.
- [x] Audit material modes in representative campaign backgrounds and identify
      actual affected commands, textures and rooms.
- [x] Test texture-edge alpha/coverage independently; fix transparent texels
      writing color/depth where the authored mode suppresses them.
- [x] Audit alpha-threshold boundary behavior against the original RDP contract.
- [x] Audit depth-mode distinctions against controlled tests and matched scenes;
      reject strict equality after the Facility sign regression and record
      PICA/N64 depth-tolerance limitations. The decal defect remains open.
- [x] Audit ordinary glass blending, transparent depth writes and state reset.
- [x] Trace inherited fog from the original parent lists through the native
      backend; fix demonstrated omissions without using raw child flags alone.
- [x] Audit converted texture alpha and quantify any lost fractional alpha
      before changing texture formats or memory requirements.
- [x] Add comparison gates that reject unlike gameplay states as isolated
      optimization evidence and retain frame-time distributions.
- [x] Run representative Caverns, Facility and Surface visual/streaming checks;
      record exact coverage and distinguish port inspection from N64 comparison.
- [x] Run focused sanitizer/renderer checks, full regressions and ARM build;
      resolve failures introduced by this pass.
- [x] Stage the verified candidate, restore normal launch configuration and
      publish findings, remaining defects and performance limits.

Evidence and the explicit remaining acceptance work are recorded in
[Verification20260904RenderingAudit.md](Verification20260904RenderingAudit.md).
Checked audit items mean the investigation was completed within that coverage;
they do not mean that the associated subsystem has reached full N64 parity.

## September 5 performance follow-up

- [x] Trace and prepare first-use guard attachment textures during loading;
      retain fixed texture capacity and transactional ownership.
- [x] Reject ineffective frustum experiments; reduce repeated world traversal
      with exact prepared runs and correct overlay-publication counts.
- [x] Compare final cold/repeat Caverns and demanding Dam routes with matched
      gameplay, draw counts and generated music PCM; run full regressions.

See [the measured results and limitations](Verification20260905TexturePreparationAndWorldRuns.md).
Dam retains 319 / 4250 work-budget misses and Caverns 2 / 2880; consistent
hardware 60 fps and full campaign parity remain open.


## September 5 SFX tail follow-up

- [x] Attribute the two remaining Caverns work-budget misses using the same
      retrace cadence, including cold SFX decode cost.
- [x] Implement bounded 32-bit decoding with wide and aliasing fallbacks;
      compare every original sample plus synthetic cases at both compiler levels.
- [x] Repeat cold-cache Caverns and Dam replays; match gameplay, draw and PCM gates.
- [x] Account for the Dam frame remainder with disjoint boundaries and exact
      per-frame tick reconciliation; select overlay publication for the next pass.
- [x] Finish full regressions and stage the verified decoder candidate.

[Decoder measurements](Verification20260905SfxDecoderTails.md) show Caverns at
0 / 2880 misses and 0 / 2879 skipped intervals in both final repeats. Dam remains
311 / 4250 misses with 30–31 skipped intervals. These are bounded emulator
results; physical hardware and full campaign acceptance remain open.


## Articulated publication follow-up

- [x] Attribute the outer frame remainder, then separate guard/articulated/glass
      publication and narrow the articulated cache, input and upload costs.
- [x] Reject the ineffective node-index experiment; preserve its evidence.
- [x] Publish only eye/world positions for proven pose-only articulated updates;
      retain full-copy fallback for scene installation and static changes.
- [x] Compare byte-exact publication and GPU buffers; repeat cold Dam/Caverns
      runs with matching gameplay, draw and music PCM gates.
- [x] Finish the combined full regression suite and final staging.

[Combined measurements](Verification20260905ArticulatedPublication.md) retain
zero Caverns misses and zero skipped intervals in both repeats. Dam improves
from the pre-SFX 319 misses / 31 skipped intervals to 252 / 11 in both combined
runs, with p95 work falling from 18.008 to 17.100 ms. Hardware 60 fps remains open.

### Remaining Dam tails and diagnostic overhead

- [x] Reconcile the 252 remaining slow Dam frame IDs with disjoint outer timing and nested music/props/guard profiles.
- [x] Test repeated first-person UV layout reuse against exact buffers and cold timing; reject it when whole-run pacing does not improve.
- [x] Preserve rejected zero-gain, pole-filter and float-hash experiments as evidence without retaining their source changes.
- [x] Split the envelope mixer's stable suffix with exact PCM/state/alias tests.
- [x] Make detailed guard and weapon geometry clocks obey the existing deep-profile opt-in; preserve outer timing and reset on stage teardown.
- [x] Pass full regressions, ARM build, two cold matched runs per route and both music PCM gates.
- [x] Run normal live-scheduler Dam combat separately from fixed-cadence speedup comparisons and report the remaining misses.
- [x] Preserve/stage candidate `1773e8ae` and restore the normal launch configuration.

Evidence and limitations: [remaining Dam tails](Verification20260905RemainingDamTails.md). Matched Dam: 219–220 misses and four skipped intervals; matched Caverns: zero misses/skips with a 15.8039 ms peak. Live Dam combat passes its route/combat verifier but still has 269 misses / 4,138 and six skipped intervals. Hardware and full-campaign 60 fps remain open.


### Live workload attribution and exact GPU command encoding

- [x] Preserve the previous independent live Dam input/retrace history and verify reproduced gameplay/draw totals.
- [x] Attribute sustained slow clusters and peak frames with opt-in outer, music, props, guard and world diagnostics.
- [x] Audit normal profiling costs while retaining timing and simulation semantics.
- [x] Specialize valid single-parameter GPU command encoding; preserve every Citro3D context update and draw command.
- [x] Pass 50,000 differential command-buffer cases with ASan/UBSan, full regressions and the ARM rebuild.
- [x] Repeat both Dam controls and Caverns with matched state/draw gates; pass all three audio PCM comparisons.
- [x] Run independent live Dam scheduling, preserve its new history and compare both binaries on that workload.
- [x] Archive/stage candidate `704af8ad`, restore the original configuration and verify normal startup.

[GPU command measurements](Verification20260905GpuCommandEncoding.md) show approximately 0.50 ms less work on the original sustained slow-frame cluster. The latest matched history improves from 515 to 377 misses and four to one skipped intervals; independent live Dam has 375 misses and one skipped interval. Dam and physical hardware 60 fps remain open.


### Newest Dam stress history and audio sample stores

- [x] Profile the current GPU-command candidate under the latest heavier Dam retrace history.
- [x] Reconcile outer timings and attribute sustained early/late clusters and worst frames with separate opt-in diagnostics.
- [x] Inspect remaining ARM sample-store instructions and implement the exact two-byte copy without alignment assumptions or new command dispatch.
- [x] Pass exhaustive value/address and ordered alias tests, all port regressions and a reproducible ARM rebuild.
- [x] Repeat matched newest Dam, established Dam and Caverns timings with identical recorded gameplay/draw state.
- [x] Pass exact PCM comparisons for all three workloads.
- [x] Complete independent live scheduling and preserve the resulting retrace history.
- [x] Stage/archive candidate `cd98cb4b`, restore normal configuration and check startup.

[Stress measurements](Verification20260905DamStressSampleStores.md): newest Dam improves from 377 to 317 misses, established Dam from 189 to 173, and Caverns retains zero; both repeats of all three controls have zero skipped/repeated intervals. Sustained Dam budget misses and physical hardware performance remain open.


### Exact draw sequences, shading and source/binary reproducibility

- [x] Attribute recurring world/guard/glass costs separately from topology-publication spikes.
- [x] Preserve exact shading bytes within immutable batches and reset on matrix/state transitions.
- [x] Preserve all eleven SDK draw commands, context updates and partial-buffer/error behavior.
- [x] Pin the private SDK ABI, vendor its source/license, and reject unverified archive changes.
- [x] Pass command/shading differential tests, windowed-door ownership checks and full regressions.
- [x] Detect the stale visibility object left by a rejected experiment; force complete rebuilds and verify matching hashes.
- [x] Rebuild a clean baseline so the new optimization is measured independently of that repair.
- [x] Repeat corrected heavy Dam, established Dam and Caverns controls, PCM and live scheduling checks.
- [x] Archive/stage the corrected candidate, restore normal configuration and verify startup.

Evidence: [draw sequences and reproducibility correction](Verification20260905WorldGuardSubmission.md). Initial `acafeb54` measurements include stale visibility code and are superseded by the corrected `4cfeecee` acceptance runs. Full campaign parity and hardware 60 fps remain open.


### Guard collection and exact vertex publication

- [x] Split recurring guard collection, topology, signatures, quantization, copies, transforms, batches and upload using temporary diagnostics.
- [x] Filter non-display-list nodes before resource searches without retaining indices or changing traversal.
- [x] Select the exact immutable segment-space publication path once per input.
- [x] Preserve empty-list/null-storage behavior, source fields, dirty ranges, ownership and failure semantics.
- [x] Pass byte-exact vertex/descriptor tests, sanitizer regressions, the full suite and two identical forced ARM builds.
- [x] Repeat heavy Dam, established Dam and Caverns with exact gameplay/draw gates; match all three complete PCM captures.
- [x] Finish independent live scheduling, preserve its cadence, archive/stage and verify normal startup.
- [x] Test a shared audio sample-load rewrite after profiling remaining music costs; reject its immaterial repeated timing gain, retain exhaustive read coverage and restore the verified production source.

Evidence: [guard construction measurements](Verification20260905GuardConstruction.md). Final candidate `e1d96c24` has 183 heavy-Dam misses in both repeats versus 253, established Dam 154 versus 157, and Caverns zero. Every normal matched run has zero skipped/repeated presentation intervals. Music/props remain substantial recurring costs; the frame-185 transaction is separate. Hardware and campaign-wide 60 fps remain open.

### Larger structural work and reliable comparisons

- [x] Attribute remaining recurring music, props and world costs before choosing another optimization.
- [x] Trace allocation-dependent replay divergence to the first changed gameplay operation.
- [x] Repair the doubled segment-5 offset in native weapon collision geometry; validate native strides, bounds, TRI4 padding and the authored KF7 mesh.
- [x] Establish a complete corrected Dam combat baseline instead of counting changed combat as a speedup.
- [x] Prototype a New 3DS core-2 music interpreter with synchronous Old 3DS/permission-failure fallback, main-thread libaudio production and explicit completion before audio consumption.
- [x] Measure a matched corrected Dam trial: 166 to 9 frame-budget misses, 11.2374 to 9.8987 ms mean work; preserve the gameplay/draw gate.
- [x] Complete release repeats, all control routes, exact PCM, actual emulator fallback, live scheduling and staging for the worker.
- [ ] Profile physical New 3DS XL CPU/GPU time, core availability, audio stability and thermally sustained pacing; characterize original 3DS separately.
- [ ] Attribute the remaining first-person and geometry-upload spikes on the corrected worker workload before changing buffer lifetimes or scheduling.
- [ ] Evaluate persistent character/prop preparation that removes repeated ownership/traversal work while preserving canonical tick order and exact published geometry.
- [ ] Evaluate a larger world-submission change that reduces CPU command/material work across rooms and levels, with bounded memory and exact render-state transitions.
- [ ] Expand corrected replay, mission-objective and visual acceptance across the single-player campaign; current representative routes do not establish campaign parity.

Collision evidence and the reason for replacing the old combat baseline are in
[Native hit geometry](Verification20260905NativeHitGeometry.md). Architectural
opportunities above are priorities, not guaranteed savings or a hardware 60 fps claim.

Final worker acceptance: [New 3DS music worker](Verification20260905MusicWorker.md), candidate `4446f3c8`. Heavy Dam misses fall from 166 to 9/6, established Dam from 154 to 10/10, and Caverns remains at zero. Physical hardware and campaign-wide acceptance remain open.
