# Native multiplayer runtime assembly

The 3DS mode selector now hands the Multiplayer choice to the dedicated setup state machine. The copied start request loads the authored multiplayer arena, applies the original scenario and weapon-set preparation, creates distinct player/inventory/hand/gait owners, places each player with the original spawn selector, and performs BeginLife after native world/body resources are available.

This is a diagnostic local assembly checkpoint. A complete two/four-player match has not yet been accepted, and this does not enable synchronized UDS gameplay.

## Local controls and lifecycle

Setup supports the original compatible player counts, arenas, scenarios, weapon sets, character selections and limits. Start always crosses the outer stage teardown/load boundary. During a diagnostic match, one console controls the displayed player; SELECT moves control to the next player, while every other slot receives neutral raw pads. SELECT+START explicitly ends the match. The score screen offers another setup or a return to the original mode-selection state. A loading failure returns to that menu after acknowledgement. Solo mission-result persistence is not bound for a multiplayer stage.

All raw pad pairs are committed together. One accepted publication advances one original 60 Hz timer tick; presentation delays never increase the canonical tick delta. The global timer boundary always selects slot zero, because its idle-button provider observes the selected controller. Original shuffled order selects the subsequent player passes.

`ge_original_multiplayer_runtime_begin_world` owns the all-character pre-tick, active-tail pickup regeneration and shuffle. The shared production/host runner owns MoveBond, canonical room visibility, props, auto-aim, held-item firing, mixed near-to-far object/door interaction and reload, pickups, guns, body matrices, flash RNG, HUD preparation, death/respawn and final HUD capture. Main supplies only resource bindings, camera construction and local scene capture. Session limits run before this world work; score-limit stop-play preserves world/death progression, while the terminal state enters results.

Each local scene is captured immediately after that player's pass. Later players may overwrite native matrix banks but do not reload or rebake the displayed world's renderer storage. Restoring the local camera also restores canonical background visibility using captured portal controls without running gameplay again. HUD queues and flash RNG advance in the simulation pass; both eye draws consume owned snapshots. Autogun beam vertices are captured with the local scene. First-person and world GPU uploads occur at that capture boundary.

## Validation and remaining acceptance

Focused sanitizer checks for the constituent player/input, spawn, supplies, regeneration, session, combat, death, body, HUD, view and shared-loop owners are tracked in their individual documents and scripts. Root additionally ran the language, mission-exit, HUD, session and gun helper regressions while assembling this entrypoint. The assembled loop linked into the native ARM ELF and 3DSX; integration performs the forced frozen build and independent aggregate checks. Main/bootstrap lifetime review and actual retained gun/world acceptance remain ongoing.

Known work still required at this checkpoint:

- Publish complete native background collision triangles and texture provenance independently of renderer residency, and bind original light-fixture destruction. The old visibility-only background table cannot prove wall/light bullet hits.
- Synchronize stop-play/menu/control-lock values into the selected input provider before MoveBond; the source audit found that its original defaults otherwise survive a round freeze.
- Render the local left-hand first-person scene. Both native hands and world attachments exist, but the old scene consumer selects only hand zero.
- Exercise actual two/four-player hit, pickup, death, respawn and results progression through the shared loop, including restart after different prior match histories. Producer-order fixtures alone do not satisfy this acceptance.
- Complete all native state export domains and stable-reference lifetime hooks before any protocol finish-tick digest is certified. Core/inventory domain work and the reviewed codec are insufficient as a full-state attestation.
- Connect the match protocol and UDS transport to the accepted native loop and implement the actual network lobby/start/result flow. The existing pre-start radio probe is not a synchronized match.

No emulator or shared SD/config changes were made by this worktree. Hardware frame-rate and radio acceptance remain separate integration tasks.
