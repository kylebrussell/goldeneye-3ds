# Native multiplayer runtime assembly

The 3DS mode selector now hands the Multiplayer choice to the dedicated setup state machine. The copied start request loads the authored multiplayer arena, applies the original scenario and weapon-set preparation, creates distinct player/inventory/hand/gait owners, restores retained native stage-intro globals before spawn, places each player with the original spawn selector, and performs BeginLife after native world/body resources are available.

This is a diagnostic local assembly checkpoint. A complete two/four-player match has not yet been accepted, and this does not enable synchronized UDS gameplay.

## Local controls and lifecycle

Setup supports the original compatible player counts, arenas, scenarios, weapon sets, character selections and limits. Start always crosses the outer stage teardown/load boundary. During a diagnostic match, one console controls the displayed player; SELECT moves control to the next player, while every other slot receives neutral raw pads. SELECT+START explicitly ends the match. The score screen offers another setup or a return to the original mode-selection state. A loading failure returns to that menu after acknowledgement. Solo mission-result persistence is not bound for a multiplayer stage.

All raw pad pairs are committed together. One accepted publication advances one original 60 Hz frame; presentation delays never increase the canonical tick delta. Paused and controls-locked frames still consume every pad and advance `speedgraphframes`, while the original timer sets the gameplay clock/delta to zero. The global timer boundary preserves the prior frame’s last shuffled player, including the selected controller observed by idle-input and casing-audio consumers. Original shuffled order selects the subsequent player passes.

`ge_original_multiplayer_runtime_begin_world` owns native spark/casing/shard/particle updates, the all-character pre-tick, active-tail pickup regeneration and shuffle. The stage sky update precedes this boundary. The shared production/host runner owns the original mission/watch timer statements around MoveBond, canonical room visibility, props, auto-aim, held-item firing, mixed near-to-far object/door interaction and reload, pickups, native BG prop render state and flash RNG, guns, body matrices, HUD preparation, death/respawn and final HUD capture. Main supplies only resource bindings, camera construction and local scene capture. Session limits run before this world work; score-limit stop-play preserves world/death progression, while the terminal state computes native awards and enters the watch results pages. Results keep the same all-player loop running until every player reaches `MENU_FINISHED` and the native watch requests `LEVELID_TITLE`; only then does the outer stage boundary retire the match.

Each local scene is captured immediately after that player's pass. Later players may overwrite native matrix banks but do not reload or rebake the displayed world's renderer storage. Final local presentation restores only owned preview fields; canonical selection, frustum and background visibility remain those of the last shuffled player. Native camera-state comparisons across every local display choice verify this boundary. Diagnostic touch UI temporarily selects the displayed player and restores the canonical selection before accepting raw input. HUD queues and flash RNG advance in the simulation pass; both eye draws consume owned snapshots. Autogun beam vertices are captured with the local world scene. Both local first-person hands have independent cached scenes, texture owners and GPU vertex ranges. They are captured immediately after the local gun producer, before death/respawn can replace that player’s loadout; world scene capture follows the player pass. A failed hand publication disables that scene. Both eye draws read these owned publications without advancing gameplay.

Every gameplay stage now requires a complete native BG geometry owner before prop/player construction. Collision and light-fixture data cover every authored room independently of renderer residency. Held-item dispatch reads that owner’s native room table and binds its original light-break callback. Local scene capture reapplies persisted authored room colours, excluding actor overlays, and flushes the changed room prefix; remote passes never rewrite the saved local scene. Teardown unbinds BG/light callbacks after native world consumers close.

Native multiplayer watch text is captured during the displayed player's local
scene capture, while that player is still selected. The retained source lays
out the pause, scores, awards and continuation pages in the authored 320×240
screen and split viewport. Main centers that viewport on the 400×240 display
by translation, builds the real Bank Gothic glyphs once, and uploads owned
vertices into the existing frontend UI region while gameplay owns it. Both
eye draws consume those vertices after the ordinary HUD and before the final
fade, matching the original watch display order. This adds no linear vertex
allocation and does not change native VI, camera, selection, menu state or RNG.
Missing font or failed page capture fails the local scene publication. The
generated watch display slice is part of the production ARM build and the
page/immutability regression is included in `test_port.sh`. See
`MultiplayerWatchRender.md` for the source boundaries and CPU gallery evidence;
the ARM link and CPU layout inspection do not establish hardware rendering.

## Validation and remaining acceptance

Focused sanitizer checks for the constituent player/input, spawn, supplies, regeneration, session, combat, death, body, HUD, view and shared-loop owners are tracked in their individual documents and scripts. Root additionally ran the language, mission-exit, HUD, session and gun helper regressions while assembling this entrypoint. Actual two/four-player gun producer checks now execute the retained gun/model/RW/matrix/beam owners with real ROM weapon models, including distinct hands and cross-player storage persistence; preceding firing flags are fixture inputs, so this is still narrower than a complete match. Selected input providers refresh native stop-play, game-over, menu and lock gates at each live-input/MoveBond boundary; sanitizer checks cover freeze and release while ticks continue. The assembled loop, native BG/light closure, generated stage-intro reset, viewport timers and retained core/inventory/prop exports linked into the ARM ELF and 3DSX; integration performs the forced frozen build and independent aggregate checks. Main/bootstrap lifetime review and actual retained gun/world acceptance remain ongoing.

The native prop arena now has an explicit retirement event after all resource
consumers and the renderer close. Main binds E3/E5 before any allocation,
E1/E2/E4/E6 after player creation, E7 after BG load, EB after fog load
and E9 after visibility load. E8 begins only after both native effect and projectile resets. EA begins after
all stage definition containers exist, with namespace 0 for setup command IDs,
1 for authored guard weapon slots, 2 for all 30 player-item slots and 3 for all
20 native ammo-crate slots. It includes retained inactive definitions and ends
before the ordinary-object/guard runtime destroys their storage. E9 ends
before visibility closes; E8 ends after native consumers and the renderer,
before the effect storage observation is retired. Each identity ends at its
native storage boundary. Released keys persist within a match so reuse
advances its generation; a new match starts a fresh registry after clean
teardown, independent of prior local history. Native 2/4/2 lifecycle tests cover partial
loads and deliberately missed free callbacks, with faults preventing restart.
These ownership bindings do not produce a complete state digest.

Native effect initialization now owns the original count-dependent shard storage,
spark/casing resets and updates. Canonical prop rendering retains explosion/smoke
retirement, impact bits, monitor controllers, character statistics and native
model-node RNG in signed room-depth/pass order before the first-person gun
producer. Dam’s native lake depth prefix is retained; the eleven multiplayer
arenas keep their original ordering. Native fog is selected using the actual
roster before view setup. Focused both-ABI source tests cover these boundaries.

The fresh native host graph now builds directly from the production extraction
recipe with source/command/header hashes. Actual Temple two/four-player bootstrap passes, and fully populated Caves
runs 480 frames per roster through native pickup, PP7 selection, ten shots,
reloads and ten bullet impacts. Native player hits, death/respawn and complete
results progression remain acceptance work; these advances do not yet
establish the complete shared-loop contract.

Kind0, E8, E9, EA and EB capture/schema entrypoints are force-retained in the ARM link.
Unresolved native resource references, unowned transitions and retained
visibility room-cycle/parser-state initialization still prevent complete state
attestation. Host model callback tokens must resolve to stable callback roles;
table registration order is not canonical wire state.

Known work still required at this checkpoint:

- End-to-end held-item wall/light acceptance. Independent BG parser/query fixes now reject inconsistent vertex mapping, overlapping scene ranges and wrong origins/provenance, while invalid queries leave a valid owner usable. Native and short-enum real-asset geometry and E7 capture tests cover all 32 stage descriptors, but do not by themselves prove a complete match.
- Exercise actual two/four-player hit, pickup, death, respawn and results progression through the shared loop, including restart after different prior match histories. Producer-order fixtures alone do not satisfy this acceptance.
- Complete all native state export domains and stable-reference lifetime hooks before any protocol finish-tick digest is certified. Core/inventory domain work and the reviewed codec are insufficient as a full-state attestation.
- Connect the match protocol and UDS transport to the accepted native loop and implement the actual network lobby/start/result flow. The existing pre-start radio probe is not a synchronized match.

No emulator or shared SD/config changes were made by this worktree. Hardware frame-rate and radio acceptance remain separate integration tasks.
