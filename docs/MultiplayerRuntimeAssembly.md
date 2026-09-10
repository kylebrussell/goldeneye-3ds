# Native multiplayer runtime assembly

The 3DS mode selector now hands the Multiplayer choice to the dedicated setup state machine. The copied start request loads the authored multiplayer arena, applies the original scenario and weapon-set preparation, creates distinct player/inventory/hand/gait owners, restores retained native stage-intro globals before spawn, places each player with the original spawn selector, and performs BeginLife after native world/body resources are available.

This is a diagnostic local assembly checkpoint. A complete two/four-player match has not yet been accepted, and this does not enable synchronized UDS gameplay.

## Local controls and lifecycle

Setup supports the original compatible player counts, arenas, scenarios, weapon sets, character selections and limits. Start always crosses the outer stage teardown/load boundary. During a diagnostic match, one console controls the displayed player; SELECT moves control to the next player, while every other slot receives neutral raw pads. SELECT+START explicitly ends the match. The score screen offers another setup or a return to the original mode-selection state. A loading failure returns to that menu after acknowledgement. Solo mission-result persistence is not bound for a multiplayer stage.

All raw pad pairs are committed together. One accepted publication advances one original 60 Hz timer tick; presentation delays never increase the canonical tick delta. The global timer boundary always selects slot zero, because its idle-button provider observes the selected controller. Original shuffled order selects the subsequent player passes.

`ge_original_multiplayer_runtime_begin_world` owns the all-character pre-tick, active-tail pickup regeneration and shuffle. The shared production/host runner owns the original mission/watch timer statements around MoveBond, canonical room visibility, props, auto-aim, held-item firing, mixed near-to-far object/door interaction and reload, pickups, guns, body matrices, flash RNG, HUD preparation, death/respawn and final HUD capture. Main supplies only resource bindings, camera construction and local scene capture. Session limits run before this world work; score-limit stop-play preserves world/death progression, while the terminal state enters results.

Each local scene is captured immediately after that player's pass. Later players may overwrite native matrix banks but do not reload or rebake the displayed world's renderer storage. Final local presentation restores only owned preview fields; canonical selection, frustum and background visibility remain those of the last shuffled player. Native camera-state comparisons across every local display choice verify this boundary. Diagnostic touch UI temporarily selects the displayed player and restores the canonical selection before accepting raw input. HUD queues and flash RNG advance in the simulation pass; both eye draws consume owned snapshots. Autogun beam vertices are captured with the local world scene. Both local first-person hands have independent cached scenes, texture owners and GPU vertex ranges. They are captured immediately after the local gun producer, before death/respawn can replace that player’s loadout; world scene capture follows the player pass. A failed hand publication disables that scene. Both eye draws read these owned publications without advancing gameplay.

Every gameplay stage now requires a complete native BG geometry owner before prop/player construction. Collision and light-fixture data cover every authored room independently of renderer residency. Held-item dispatch reads that owner’s native room table and binds its original light-break callback. Local scene capture reapplies persisted authored room colours, excluding actor overlays, and flushes the changed room prefix; remote passes never rewrite the saved local scene. Teardown unbinds BG/light callbacks after native world consumers close.

## Validation and remaining acceptance

Focused sanitizer checks for the constituent player/input, spawn, supplies, regeneration, session, combat, death, body, HUD, view and shared-loop owners are tracked in their individual documents and scripts. Root additionally ran the language, mission-exit, HUD, session and gun helper regressions while assembling this entrypoint. Actual two/four-player gun producer checks now execute the retained gun/model/RW/matrix/beam owners with real ROM weapon models, including distinct hands and cross-player storage persistence; preceding firing flags are fixture inputs, so this is still narrower than a complete match. Selected input providers refresh native stop-play, game-over, menu and lock gates at each live-input/MoveBond boundary; sanitizer checks cover freeze and release while ticks continue. The assembled loop, native BG/light closure, generated stage-intro reset, viewport timers and retained core/inventory/prop exports linked into the ARM ELF and 3DSX; integration performs the forced frozen build and independent aggregate checks. Main/bootstrap lifetime review and actual retained gun/world acceptance remain ongoing.

The native prop arena now has an explicit retirement event after all resource
consumers and the renderer close. Main binds E3/E5 before any allocation,
E1/E2/E4/E6 after player creation and E7 after BG load; each identity ends at
its native storage boundary. Released keys persist across restarts so a new
lifetime advances its generation. Native 2/4/2 lifecycle tests cover partial
loads and deliberately missed free callbacks, with faults preventing restart.
These ownership bindings do not produce a complete state digest.

Known work still required at this checkpoint:

- End-to-end held-item wall/light acceptance. Independent BG parser/query fixes now reject inconsistent vertex mapping, overlapping scene ranges and wrong origins/provenance, while invalid queries leave a valid owner usable. Native and short-enum real-asset geometry and E7 capture tests cover all 32 stage descriptors, but do not by themselves prove a complete match.
- Complete the native effect initialization/update paths and restore source render-side state order. Shard storage is currently absent, explosion/smoke part retirement belongs to the canonical BG alpha pass, and third-person flash RNG must follow native prop/room render order before the first-person gun producer. These are active gameplay blockers found by the source audit.
- Exercise actual two/four-player hit, pickup, death, respawn and results progression through the shared loop, including restart after different prior match histories. Producer-order fixtures alone do not satisfy this acceptance.
- Complete all native state export domains and stable-reference lifetime hooks before any protocol finish-tick digest is certified. Core/inventory domain work and the reviewed codec are insufficient as a full-state attestation.
- Connect the match protocol and UDS transport to the accepted native loop and implement the actual network lobby/start/result flow. The existing pre-start radio probe is not a synchronized match.

No emulator or shared SD/config changes were made by this worktree. Hardware frame-rate and radio acceptance remain separate integration tasks.
