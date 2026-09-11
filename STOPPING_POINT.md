# Integration stopping point — 2026-09-10

Work stopped at the user's request after usage limits were reached. Earlier
plans do not authorize further feature work or investigations. Ready work is
committed; unfinished work and evidence limits are recorded below. No push,
deployment, parent-checkout edit or new emulator run occurred during this closure.

Branch: `codex/integration-acceptance-20260910`.
Worktree: `/Users/kyle/.codex/worktrees/1134/goldeneye-3ds`.
Final imported checkpoint before this handoff-only commit:
`3d0484fd` (full identity is its Git commit). The final branch HEAD is the commit
containing this file; the coordinator sends that full hash separately so this
document does not contain a self-referential commit hash. The production runtime
last changed at `4c57e81d`; `036b481a` adds host fixtures/tapes/provenance and
`3d0484fd` imports the multiplayer stopping-point document.

## Included work

All previous integrated campaign, renderer, asset, audio, input, multiplayer and
performance work remains in ancestry. Do not reapply the peer branches wholesale.
[Integration acceptance](docs/IntegrationAcceptance20260910.md) records the earlier
checkpoints. The final additions after frozen `d7e6b7d5` are:

| Integration commit | Upstream commit | Scope |
| --- | --- | --- |
| `d141790b` | coordinator | Bounded d7 host/ARM closure and music countdown source correction |
| `648be97f` | `e2799c1a` | ED physical Model header/node topology; open malformed-owner finding |
| `c607e7a3` | `93095089` | Native setup input, copied launch settings and kind0 v4 controls |
| `d64b6e38` | `16cb4ac6` | EF music fields/tables, stage-slot reset and death-music recovery |
| `3b8d905c` | coordinator | One ED full-pack hook and one setup hook in the aggregate |
| `e6b23d66` | `0ad30683` | Native stage player/shuffle reset and caller-context restoration |
| `8f80c01f` | `fde2ed19` | Real shuffle storage in focused fixture owners |
| `4c57e81d` | `774028e8` | MP countdown after pre-tick and regeneration, before shuffle; TITLE guard |
| `036b481a` | `a7c59ff5` | Corrected host bootstrap, frozen raw controller tapes and peer/restart audit |
| `3d0484fd` | `850187a7` | Multiplayer owner's final ready/open handoff |

Merge resolution preserved the combined linker retention list and existing
aggregate layout, without duplicate transport or exporter test groups. Existing
main input, stereo, renderer readiness and cleanup changes were retained.

## Validation and immutable evidence

All paths in this section are relative to this worktree. Private inputs and
evidence remain ignored files; Git commits alone do not contain them.

- **Latest common full host and forced ARM checkpoint: `d7e6b7d5`.**
  `build/integration/aggregate-host-evidence-d7e6b7d5/` records all 9,181 copied
  inputs, a fresh 366-unit native graph, 1,670 dependency identities and 52
  generated sources. Closure SHA256:
  `2135553bc3554c0ca6dae889470e745e82e7416c0e63c976df9370e599720f29`.
  The coordinator reverified all 18 closure and 431 native execution artifact
  hashes. `candidate-build-d7e6b7d5/` retains the forced ARM receipt, ELF,
  generated sources and 63-symbol audit. Its 3DSX SHA256 is
  `3836b23363415a0ed1bbe1528c78ededd28f1acaf2e6cb698ae6954ce4667f87`.
  This frozen fixture omitted main's music initializer/fade service and used
  different startup ordering; its Caves results do not establish exact main
  initialization or RNG-history agreement. Later corrections are excluded.
- `build/integration/music-countdown-scope-correction-d7e6b7d5.json` corrects
  the prior solo-only statement while preserving affected sealed files. Native
  `lv.c:1354` selects TITLE; the common non-title tail at 1375–1377 calls
  `chrpropTick`, countdown, then `langTick`. Solo and MP need their separate
  actual-world hooks. Correction SHA256:
  `31e7e42b55a7bb01488b4fd22183fe7235dfdbb49b809d1b43de3ba5e1376423`.
- **Merged focused checkpoint `3b8d905c`:**
  `build/integration/mp-ed-setup-music-3b8d905c/receipt.json` records passing
  setup, EF, session, main lifetime, kind0 and ED fixtures. Independent setup
  checks cover both enum ABIs, 2/3/4 participants, every local slot and 468
  malformed copied-launch cases with unchanged player/permanent/control/RNG
  state. Receipt SHA256:
  `1707fee7a2822456cd0284e571e0bc2e5b475e7a085e180e2547bdcc6bff8012`.
- **Merged focused checkpoint `4c57e81d`:**
  `build/integration/mp-countdown-reset-4c57e81d/receipt.json` preserves complete
  passing runtime/countdown, player reset, setup, inventory and kind0 transcripts.
  Both countdown ABIs reject omitted/before-AI/before-regeneration mutations.
  Original PTY handles became unavailable before final polling; final inventory
  and kind0 exit codes were not recovered, and no matching processes remained.
  This limitation is explicit. The native exactness command first missed its
  required repository argument; that failed invocation is retained. The corrected
  invocation exited 0. No broad rerun was made. Receipt SHA256:
  `0d9c52e4aee75ad8f50dba8a48c02360bba0b5cc998b21547431ce7c29d55cda`.
- **Upstream corrected replay checkpoint `a7c59ff5`:**
  `build/integration/mp-controls-replay-a7c59ff5/` copies and verifies 44 existing
  evidence files (87,736,177 bytes). Six independent-peer and three cross-enum
  comparisons pass using native/short 370-unit graphs. Raw cold/restart state
  differs: player-tick history, first warm body-animation publication and
  monotonic lifetimes remain visible and explicitly classified. This is bounded
  CPU evidence, not complete state equality. Copy receipt SHA256:
  `c328a9a1ab5fb731284a23b54ec0d52ccf16a2b184ca7596f09219dda61e9c5e`.
  No replay or full suite ran on the final merged source after this import.
  See [the multiplayer handoff](docs/MultiplayerStoppingPoint.md) for upstream
  build/fixture hashes and forced ARM scope at `774028e8`.
- **Latest packaged checkpoint remains `3760b2dd`:**
  `build/integration/candidate-3760b2dd/` has 672 inventoried files and 286 scoped
  references. Manifest SHA256:
  `d933211141bcadb35d4e9c4a37b69d14b787471ca8c125a06465c5346344a7c2`.
  It verifies normally and fails the completeness gate; all nine areas are
  partial. It contains earlier known limitations, compiled defaults and explicit
  external DSP prerequisites. The older `candidate-31fce6e5/` remains unchanged.
  No new package was made. `acceptance-next-d7e6b7d5-draft.json` is only a draft
  and does not describe final-HEAD acceptance.

## Excluded WIP and open findings

- **Solo countdown WIP, not imported:**
  `4360d85ce2a7f1b839c4daac9cf7ca54d069c1db` on
  `codex/solo-music-slot-tick`, worktree
  `/Users/kyle/.codex/worktrees/1ce8/goldeneye-3ds`.
  `docs/WIPSoloMusicCountdown.md` on that branch records its main hook and
  uncompiled/unwired C regression draft. It needs base `3b8d905c` plus released
  `774028e8` (already integrated here as `4c57e81d`). Do not also import its
  duplicate helper cherry-pick `6a0c7e9d`. No solo runtime/ARM acceptance exists
  for this WIP; only its source-order check and whitespace check passed.
- **ED duplicate-native-address finding remains unfixed.** Independent report:
  `build/integration/mp-header-topology-independent-review-e2799c1a/review.md`.
  In both ABIs a malformed owner can publish one physical node under two IDs;
  teardown accepts and changes it. Valid begin/add do not construct that state.
  Review receipt SHA256:
  `a15d9d51d894961fc5b2027be48d5684312ce2bb79d96a3ba9c08710f70903b6`.
  MP reports an analogous EC omission from source inspection only; its separate
  reproduction and repair were not started. Earlier valid-owner EC evidence
  does not establish this malformed-owner boundary.
- **Independent EF review is unfinished.** Source inspection only; no actionable
  defect or independent runtime/ARM closure. Copied status:
  `build/integration/ef-independent-review-paused-16cb4ac6/REVIEW-PAUSED.md`.
  Exact snapshot remains in the fidelity worktree at
  `/Users/kyle/.codex/worktrees/5c80/goldeneye-3ds/build/host-tests/music-state-review/snapshot-16cb4ac6`.
- Independent awards numerical repair review remains incomplete after an
  automatic reviewer flag. Neither that review nor the other flagged review
  was retried, reassigned or treated as passing.
- ED main binding, EE RW/RO/matrix/cache ownership, reserved F0 model controls,
  kind0 EF reference, visible native setup and its resource/main integration,
  actual UDS lobby/match and complete aggregate coverage remain incomplete.
  No additional renderer/EE/F0 WIP was begun by the MP owner.
- Campaign completion/reload, matched reference visuals, heard audio, physical
  New 3DS XL controls/stereo/radio and sustained 60fps remain unaccepted.
  The Mac-locked emulator route was not retried or bypassed. Existing A/B tools
  and asset receipts are integrated; final-source measurements remain pending.

## Coordination and resumption

All peer roots received the stop instruction and notified their subagents.
Integration's remaining ED reviewer was told to seal existing findings and idle;
the two previously flagged reviewers were not restarted. No test, compiler,
replay or emulator process is intentionally left running by this coordinator.
The parent task owns bringing this committed branch into its checkout while
preserving the parent's existing local work. No further feature follow-up is
scheduled. Resume only on a new user instruction, starting with this handoff,
the exact evidence scopes and the open findings above.

The parent coordinator reports that its checkout has already fast-forwarded to
`3d0484fd` on `codex/integrated-stopping-point-20260910`; it will fast-forward
the final handoff documentation next. Its original uncommitted work matched the
baseline exactly and is redundantly preserved in stash
`62dde08f4132eea5c536d4d474d685427a167a50`. Original `master` remains `849840cf`.
The user's untracked `platform/3ds/source/main.c.orig` remains unchanged,
SHA256 `25a8f597c4d660ada95dfeda0b664c3fefd8a29c25d90550f853287fe39ea593`.
These parent-preservation checks were performed by the parent coordinator;
this integration task did not edit or recheck that checkout.

## Physical hardware follow-up — 2026-09-11, Dam selection

User confirmed upright intro Bond on `60c22e2f`, but reported legs extending
outside the barrel, displaced mission-selection artwork, and a return to HBL
when starting Dam. Saved card log: `build/hardware-boot-20260911/dam-start-exit.log`.
It reaches gameplay geometry loading and then normal cleanup; it does not
identify the failing initializer. No peer tasks were restarted.

- Corrected wallet view translation from original `frontSetupMenuBackground`:
  eye and target both include the 190-unit Y offset, so the camera has no pitch.
- Added stencil coverage from the authored barrel-opening polygon to constrain
  Bond; restores stencil state before blood/fade and at frontend entry.
- Fixed solo fog loading before `bond_live_initialize` publishes the canonical
  player roster. `getPlayerCount()` returns zero before publication on a fresh
  launch, which the fog loader rejects. Multiplayer retains fog-before-effects
  ownership order. Solo player initialization failures now stop explicitly.
- Stage startup failure paths now persist their source location (geometry also
  records status and room) and use the existing START-to-exit error screen.

Validation: sanitized production wallet/stencil regression, Bond projection,
empty frontend draws, original gunbarrel contract, native and short-enum fog
selection/zero-player regression, MP fog state tests, and all 32 asset-pack BG
geometry descriptors in both host ABIs passed. These checks do not establish
physical rendering fidelity or successful Dam play; the next card build needs
that hardware confirmation. Forced ARM candidate build/installation receipts
are recorded separately under `build/releases` and `build/sd-deployment`.

## Physical hardware follow-up — tunnel geometry and stripes

On `62470422`, the user reached Dam gameplay and walked through the tunnel
exit into missing scenery (movement was not blocked). The renderer queued only
VISOP_PRELOAD requests and stereo preload requests, omitting the original
bgRenderRoomPrimary/Secondary demand load for ordinary portal-visible rooms.
The local renderer now queues unloaded members of its mono/stereo visibility
union as well as advance preloads. Canonical visibility membership is unchanged;
the existing bounded transaction imports those rooms.

Also fixed same-image wrap/filter changes in the differential material adapter:
Citro3D's inline setters modify only the CPU descriptor; C3D_TexBind marks the
texture registers dirty. Parameter changes now rebind even if the image pointer
is unchanged. Upstream implementation reference:
https://github.com/devkitPro/citro3d/blob/master/source/texture.c

New sanitized regressions execute the production demand loop and sampler
publication block, including visible-without-preload in mono, stereo-only
membership, deduplication, and same-handle wrap/filter changes. Existing room
stream transaction, UV and texture reconciliation checks passed; the authored
Dam stream probe imported its complete 15-room route without failures. Hardware
visual confirmation remains pending. Gameplay cleanup now records final room,
residency, pending requests and streaming failure counters in boot.log for the
next report. Previous card log saved under build/hardware-boot-20260911.

## Physical hardware follow-up — decal depth and Start/watch crash

On `b2377991`, the user reported intermittent tunnel wall/block visibility,
red/white stripes disappearing beneath concrete, and a crash after the watch
animation began when pressing Start. The card log's previous completed Dam run
reported 39 room installs and zero streaming failures; the last run has no exit
record. That log does not identify the crash or explain the intermittent walls.

- Authored Dam red/white stripe texture 18 uses secondary-list DECAL depth mode
  with depth writes disabled. The adapter now allows equal depth for decals,
  retaining strict comparison for other modes and the authored write mask.
  This addresses equality rejection against the receiver. RDP slope-dependent
  depth tolerance remains unsupported and continues to be reported as fallback.
- Guarded three empty HUD draw submissions in the bottom/mission HUD paths.
  This removes a known dangerous GPU submission pattern, but is not a confirmed
  explanation of this user's watch crash. Transition-only before/after player
  tick checkpoints now record watch animation state in boot.log.
- The original watch model offsets were checked against the ARM player layout;
  its animation is present in the native animation table. Neither observation
  establishes a successful complete pause animation on hardware.

Validation: native and short-enum sanitized fragment/decal contracts, sanitized
empty gameplay/front-end HUD submission checks, renderer submission cache,
world hardware regressions, PICA application mapping, and the watch dispatcher
frontier passed. The dispatcher check does not execute the full animated watch.
Disappearing tunnel blocks and seeing scenery through walls remain unresolved;
the next build is a hardware test candidate, not visual or pause acceptance.
Build and SD installation receipts are recorded separately under build/releases
and build/sd-deployment. No peer tasks were restarted.

## Physical hardware follow-up — tower/building missing surfaces

On `ffa40665`, photos show absent wall/floor sections at the first tower and
the building beyond the tunnel; railings, guards and some adjacent triangles
remain visible. The user says the omissions change mainly when moving, rather
than turning in place. The latest completed Dam run ends normally in room 112,
with 27 installs, zero failures, eight resident rooms and no pending requests.
No confirmed root cause or rendering fix is claimed in this follow-up.

Added a solo SELECT comparison cycle: 0 normal; 1 unmerged batches with existing
visibility; 2 unmerged resident geometry without CPU room/frustum rejection;
3 the same with face culling disabled. The bottom screen names the mode. Each
press saves the camera position, look/view matrix, slider, visible/resident room
lists and streaming status to render-audit.log. Restarting a stage restores
normal mode. Canonical visibility, residency, collision and AI are unchanged.
Modes 2/3 are diagnostic comparisons, not fidelity fixes, and may expose hidden
geometry or cost frame time. A same-position comparison is needed to select the
next fix; the saved camera enables a precise host replay instead of estimating
positions from photographs.

Depth mapping was cross-checked against upstream Citro3D Mtx_OrthoTilt and its
default DepthMap(true, -1, 0): this port's left-handed tilted 0..1 depth map
already yields near=1/far=0 with GREATER, so it was not reversed speculatively.
References: https://github.com/devkitPro/citro3d/blob/master/source/maths/mtx_orthotilt.c
and https://github.com/devkitPro/citro3d/blob/master/source/base.c .

## Hardware recovery — renderer comparison crash

The user reported a 3DS crash while using SELECT comparisons. Saved logs under
build/sd-deployment/render-switch-crash-20260911-132212 show modes 1, 2 and 3;
the final snapshot is mode 3 in room 135 with 1,356 batches/13,803 vertices,
no pending rooms, mono rendering, and successful streaming status. There is no
normal exit log. The preceding launcher discovery repair added a direct copy
at /3ds/GoldenEye.3dsx (and .smdh): future deployments must update both that
copy and the original /3ds/goldeneye-3ds/goldeneye-3ds.3dsx.

Removed all comparison-mode rendering overrides. The renderer_draw function
is byte-for-byte identical to ffa40665 again. SELECT only saves the camera/room
snapshot and previous completed command-buffer usage; it does not change
visibility, merging or culling. The submission-cache regression passes.

A likely crash mechanism is command-buffer exhaustion: the diagnostic
two-sided path called C3D_CullFace for every unmerged draw, marking the entire
effect state dirty even when unchanged. Upstream C3Di_EffectBind republishes
that state on the next draw; this application uses the default 256 KiB buffer.
This is a source-supported hypothesis, not a measured hardware overflow.
References: https://github.com/devkitPro/citro3d/blob/master/source/effect.c
and https://github.com/devkitPro/citro3d/blob/master/include/c3d/base.h .
Missing Dam geometry remains unresolved; do not ask the user to use the
removed comparison modes. Capturing at an actual missing surface can provide
the camera/room data needed for further diagnosis.

## Current SD deployment layout — one launcher entry

The user requested one launcher entry. The verified recovery build remains at
`/3ds/GoldenEye.3dsx` with `/3ds/GoldenEye.smdh`. Duplicate executable/metadata
files were backed up locally and removed from `/3ds/goldeneye-3ds/`; that folder
retains assets, saves, controls and logs. Future card deployments must update
only the direct launcher files and must not recreate a second executable in
the data folder. This supersedes the earlier two-entry deployment instruction.
Receipt: `build/sd-deployment/single-launcher-20260911-132557/deployment-receipt.json`.

## Dam capture follow-up — camera portal room and gate collision

Ten passive SELECT captures from the following run are preserved under
`build/hardware-captures/dam-20260911-133441/`. The run exited normally with
109 room installs and zero failures; recorded GPU command usage was below 19%.
Captures 3, 4, 6 and 7 select room 112 despite the eye sitting above its bounds;
nearby missing wall/floor surfaces belong to room 111, omitted from those lists.

Restored the exact eleven-step camera-to-floor portal walk from
`bgRoomVisibilityRelated` before `bgDetermineVisibleRooms`, including the original
geometry classification that excludes vertical portals. Solo live input supplies
the native portal STAN room and `pos3` reference rather than the collision room.
The renderer's isolated stereo passes receive the same reference. Zero-initialized
camera tours retain their existing explicit-room behavior. SELECT stays passive
and now records the actual reference, gate travel/collision bounds, truck movement
state and the last collision's setup command index, plus collision counters.

Real-asset replay reproduces all ten old visibility lists exactly. For the four
building captures, all 416 sampled floor-reference heights resolve 112 to 111
and include 111. Actual `pos3` was not logged: the sweep spans room 112's complete
authored Y range and is not a measured hardware floor reference. Detailed evidence
and input hashes are in the capture directory's `camera-room-replay.md`.

The tower omissions and truck-gate blockage are not proven fixed. User reports
the gate opens but Bond is blocked by the truck or an invisible barrier. Existing
data does not identify the blocking collision; the next SELECT capture while
pushing against it will distinguish the truck (setup 321), gates (267/268) and
static STAN. No speculative gate, vehicle AI or collision bypass was applied.
The new executable still needs physical retesting; build/deployment receipts
will be stored separately under `build/releases` and `build/sd-deployment`.
Validation passed: native live-reference selection with short enums and
ASan/UBSan, synthetic portal walks with short enums and ASan/UBSan, isolated
stereo visibility with ASan/UBSan, the BG ownership audit, submission-cache
and world-hardware regressions, and the 416-sample real-asset replay.

## Building failure on aead2325 — diagnostics, not a confirmed fix

New hardware logs and seven captures are backed up under
`build/hardware-captures/dam-gate-crash-20260911-160541/`. Despite the initial
gate description, the user clarified the failure happened inside the building
with prior visual issues. The installed executable hash matched aead2325.
The log records cleanup in room 111 with eight residents, 87 successful installs,
zero pending requests/failures and no stream/texture/allocation error; it ends
with the formerly generic `Application startup FAILED` message. No Luma dump
was present. All SELECT snapshots completed, including the new object diagnostics.

The only explicit solo-loop `result=1` break is background colour publication
failure. Native state retirement can also turn an exit into result 1 during
cleanup. These were not persistently distinguished. The new report API retains
all existing geometry validation and reports the first failed predicate, room,
batch/vertex, expected/actual provenance, and owner status. Main now writes that
detail and borrowed/dynamic scene counts before exiting, and logs cleanup-entry
result plus retirement failures separately. No speculative collision or rendering
bypass was added, and no crash root cause is claimed yet.

Software replay finds no substantial nearby geometry omitted by these seven
visible-room lists. Source-extracted renderer cache/merge/cull/fragment tests pass.
Real Dam residency replay passes 3,569 colour-publication checks per ABI under
native and short-enum ASan/UBSan, including explicit 10-to-8 eviction and repeated
overlay grow/shrink/reload transactions. This does not replay native gameplay
callbacks or borrowed preview pointers. Probe: `build/host-tests/dam-bg-residency-audit/`.

SELECT now additionally captures the already-uploaded world vertex buffer to
`render-<system-tick>.vertices.bin` and appends final projection matrices and
retained left-eye draw runs/material identifiers to `render-audit.log`. Format1
vertices are nine little-endian float32 values (XYZ, UV, RGBA), 36 bytes each;
metadata records count, stride, file-write success and run-cache validity.
These snapshots read existing state and do not change GPU submissions/culling.
This is a diagnostic build requiring another building run, not visual acceptance.
The detailed rejection-report tests pass native and short-enum ASan/UBSan;
vertex-flush and renderer submission/world regressions also pass. Native real-asset
geometry tests passed the 21 campaign stages before the broader harness stopped
at an optional asset missing from the current pack, so that entire harness is
not reported as passing. A separate probe passed all 45 possible 10-to-8 room
eviction combinations with the retained overlay.
