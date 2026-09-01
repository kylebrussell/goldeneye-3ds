# Original Dam mission-flow slice

Dam's primary stage script is the authored `ai_20` byte stream in
`assets/obseg/setup/UsetupdamZ.c`. It is record 20 in `UsetupdamZ.ailists` and
has local AI-list ID `0x1000`. The port does not decode that stream with a new
interpreter. `ge_original_dam_mission_flow_begin` publishes the generated
setup's AI-list table, calls the original
`alloc_false_GUARDdata_to_exec_global_action` body from
`src/game/deb_loadallmodels.c`, and then calls the original `ailistFindById`
and `ai` bodies from `src/game/chrai.c`. The original allocator counts all
eight Dam lists whose IDs are at least `0x1000`, creates eight `ChrRecord`
background actors, and initializes their list, offset, return-list, character
number, and action fields. The port boundary supplies real, aligned
`MEMPOOL_STAGE` storage for exactly those eight records.

The linked canonical command tranche now also includes objective-bitfield
reads, tagged-object health, tracker attachment, and tracker stationary checks
in addition to `goto_next`, `goto_first`, `label`, `ai_sleep`/yield, and
`ai_list_end`. This executes the authored first mission tick:

1. The exact background-actor allocator selects the first actor for `ai_20`.
2. `label(0x2a)` advances the original program counter by two bytes.
3. `ai_sleep` advances it by one byte, writes `ChrRecord.ailist` and
   `ChrRecord.aioffset`, and returns from the original interpreter.
4. The resulting offset is exactly 3, which points at Dam's authored
   `if_objective_bitfield_is_set_on(0x00040000, 0x04)` command. Setup macros
   store this argument byte-reversed; the canonical interpreter's `ntohl`
   decodes it to runtime objective bit `0x00000400`.

The objective-complete test sets runtime bit `0x00000400` through the exact
`chrSetStageFlags` body. Original `ai` then takes ai_20's authored label `0x04`
branch and publishes yield offset 113.

The normal second-tick sanitizer test materializes setup command 290 (tag 5's
monitor, model 335, bound pad 10057) and command 292 (tag 4's standard prop,
model 70, bound pad 10058) into the shared original prop pool. Commands 289 and
291 are registered through the canonical tag-list link. With the objective
bitfield clear, original `ai` resolves both objects through `objFindByTagId`,
passes both through `objIsHealthy`, finds no attached or stationary tracker
through the exact attachment traversal and `weaponFindThrown`, and executes
the authored `goto_first(0x2a)` plus yield. The resulting program-counter
transition is exactly 3 -> 3. The portable sidecar supplies the authored
PropDef header state that GCC cannot represent through IDO's anonymous
`inherits` extension; the health decision remains the exact decompiled body.

The live 3DS bootstrap now performs that same materialization after its nine
already-supported Dam objects, then schedules `ai_20` once per simulation tick
through `ge_original_dam_mission_flow_tick`. Exact ROM models 335 and 70 are
relocated into native pointer-safe model graphs, resolved through the shared
model provider, and sent through the unchanged preallocated `objInit` and
`moveToPad` lifecycle. Their authored display lists and collision geometry are
published in the shared room/model overlay; only after that transaction
succeeds are the two tagged props activated and enabled.

The live thrown-modem path also restores the exact `initobjects` free-marker
loop for all 40 canonical `g_Embedments` slots. This is required by unchanged
`objTick` -> `objEmbed`: zero-initialized storage looks fully occupied to
`embedmentAllocate`, so a correctly thrown modem could hit tag 5 but never
become its weapon child. `scripts/generate_dam_modem_route.py` produces a
controller-only authored route that pulses normal Use input at the intervening
gates, cycles from the silenced PP7 to the modem, fires through the original
gun animation, and leaves a settle window for projectile/object and `ai_20`
ticks. `scripts/verify_dam_modem_result.py` requires both a successful
canonical throw and objective bit `0x00000100` from the live mission runtime;
it never writes mission state itself.

Focused sanitizer coverage sustains 120 healthy mission ticks at authored
offset 3. It also attaches an item-0x2f weapon prop to tag 5 and verifies the
unchanged child traversal, runtime objective bit `0x00000100`, exact `LdamE[8]`
HUD enqueue, sound `0x00e3`, tag-5 positional emitter binding, and return to the
authored yield loop. `AI_SfxPlay` and `AI_SfxEmitFromObject` remain unchanged
canonical command bodies; the port compile boundary merely retains them in the
Dam mission slice and routes their existing sound calls to the live SFX bank.

The destruction-path sanitizer test then marks tag 5 destroyed at the native
PropDef state boundary and re-enters original `ai` at offset 3. No objective
state is injected. The exact interpreter observes `objIsHealthy(tag5) ==
FALSE`, executes the authored `set_objective_bitfield` commands and
`text_print_top`, and reaches the terminal yield at offset 113. The canonical
runtime objective result is `0x00000a00` (authored arguments `0x00080000` and
`0x00020000`, decoded as `0x00000800` and `0x00000200`). The HUD contains the
exact `LdamE[14]` satellite-link failure message. Projectile/object damage is
not yet linked; the test begins at the completed damage-state boundary.

The modem body's authored switch child is currently published with its exact
initial visible state. Full monitor animation still requires materializing the
complete `MonitorObjRecord` controller prefix and routing
`monitorSetImageByNum`; the bounded tag materializer currently owns only the
ordinary object prefix. A live PP7-to-target destruction run also remains to
be captured as emulator evidence. The upper-HUD queue is canonical and live,
but its queued text still needs the platform font/rendering handoff before
players can see it.

Dam now also loads its exact packaged setup through the common setup/STAN
relationship ABI while retaining the dedicated canonical setup pointer used
by the intro and legacy Dam slices. The four authored alarm definitions at
commands 310, 312, 314, and 316 are constructed as native PitemZ model-1
objects, placed at bound pads 10070, 10071, 10072, and 10074, published into
their exact room lists, and included in the live model scene. The use-button
fallback traverses the depth-sorted onscreen list through the unchanged
`objTestForInteract` body, then enters the exact alarm branch of
`propobjInteract`: switch SFX, global alarm toggle, activation bit, and linked
switch publication retain original ordering.

The common objective registry is now bound to that same parsed Dam setup.
Objective zero resolves exact alarm tags 0--3; all four authored objective
menus evaluate without a runtime blocker through the original health,
inventory, stage-flag, and key-analyser providers. Twelve unrelated tag
targets remain deliberately registry-blocked because their props have not yet
been materialized; none is referenced by the four current objective criteria.
The exact status-change body is invoked once per displayed frame. Mission
`ai_24` still needs its retained `AI_IFObjectiveAllCompleted` call site before
the bungee exit can consume the now-live completion query.

The controller-only end-to-end route keeps the two different interaction
semantics distinct. Tags 6 and 7 receive ordinary Use-button pulses because
unchanged `ai_21` waits on `if_object_was_activated` for both authored standard
props before starting its ten-second countdown. Tags 0--3 receive ordinary
PP7 fire instead: objective zero contains four `ObjectiveDestroyObject`
criteria, while using an alarm only toggles the global alarm state. Route
generation validates those exact tag/prop relationships against the setup and
uses fresh semi-automatic PP7 trigger pulses at each alarm; it never writes
object health, activation flags, or objective registers.

The route now also completes the authored exit interaction instead of merely
arriving at pad 330.  It idles at the exact ai_24 bungee pad through the
script's 350-tick fallback, 60-tick fade, and three yielded frames, then emits
one fresh normal Fire edge.  ai_24 remains the sole owner of the bungee bit,
control/damage lock, falling velocity, fade, POSEND camera, objective check,
and title-stage request.  Because that title request exits the platform loop
before its ordinary fixed-frame probe deadline, the existing outer stage
transition boundary now flushes the final input-probe snapshot before
committing the requested stage.  Verification requires both the canonical
title request and a successful original mission-result/save mutation with no
persistence frontier.

`scripts/extract_dam_mission_flow_dependencies.py` regenerates the audited
manifest at `docs/generated/dam_mission_flow_dependencies.json`, including
source hashes, the complete authored command-name sequence, the verified
transition, and the next dependency frontier.
