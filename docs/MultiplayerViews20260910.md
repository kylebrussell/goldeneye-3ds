# Multiplayer shooter cameras and local presentation

The live camera publisher previously had one two-entry matrix bank and one
projection for the entire roster. Publishing a second player overwrote the
first player's current and previous camera matrices. Each canonical roster slot
now owns its own bank. A separate bank preserves the standalone intro path.
`ge_original_bond_camera_restore_live_player()` refreshes the shared original
frustum globals after selecting a player, without rotating camera history.

## Original order and native integration

The authority is `src/game/lv.c: lvlRender`: select the next shuffled player,
publish room visibility, call `propsTick`, update onscreen counts and autoaim,
check held items and interaction/reload, call `propsTickPlayer`, and render the
world. Later `maybe_mp_interface` in `src/game/bondview2.c` calls
`gunUpdateAndFireBothHands` before rendering first-person guns.

`propsTick` dispatches player props to original `playerTick`, which reaches
`chrTick`. That pass owns shooter-relative `Model.render_pos`,
`PROPFLAG_ONSCREEN`, `PropRecord.zDepth`, `ChrRecord.field_20`, and the attached
held-weapon matrices/hit chains consumed by `chrTestHit`. Re-running `chrTick`
solely to prepare a draw would also advance actions, animation and other state.

The new `ge_original_multiplayer_views` adapter therefore leaves the exact
simulation calls in the outer multiplayer loop:

1. Begin the shared frame arena and canonical pre-tick once. Begin the view
   adapter with the roster count, local slot and providers.
2. In the original shuffled order, produce that player's mono camera after
   movement. `begin_shooter` selects the slot, publishes the supplied original
   camera result, and invokes the canonical visibility provider.
3. Run the one canonical prop pass for that slot, then targeting, interaction,
   pickups and gun update in the original order. This creates the correct body
   hit lists before shooting; the adapter contains no simulation calls.
4. During the local slot, call `capture_local` before beginning another slot.
   Its provider retains body/held-item matrices and builds the local scene into
   dedicated durable vertex/batch storage. Copy the first-person output too.
   `ge_original_stage_guard_runtime_set_local_player_view(runtime, player, 1)`
   suppresses the local body while retaining opponents. Body and held-item
   geometry use the WORLD stereo layer; local hands use FIRST_PERSON.
5. After all slots, `finish_local` preserves the last shuffled player’s native
   selection, frustum and background visibility. Its provider restores only
   owned local renderer data. Both eye draws use the captured scene and shader
   eye shifts. Neither eye invokes camera publication, prop ticks or model
   simulation. Choosing another local display must not alter the canonical
   state exported at the completed tick boundary.

`begin_shooter` accepts an already-produced config/result pair so the native
loop need not run the camera producer twice. Its visibility callback must use
that pair. The adapter copies the local pair and rejects duplicate slots,
missing local capture, premature completion and changed player ownership.
A failed provider invalidates the frame; do not retry a partially executed
canonical pass.

Capture must finish while matrices still belong to the local pass. Merely
saving pointers to `render_pos`, the shared dyn arena or a mutable scene buffer
is insufficient. Calling `update_matrices(local_view)` after the final remote
pass also is insufficient: its first path retains fresh transient matrices
that may still belong to the remote camera. Capture/build immediately during
the local pass instead. Subsequent remote simulation may change canonical
character state, as in the original shuffled render loop, but must not overwrite
the baked local geometry.

The final presentation callback must not select a player, restore native
frustum/room state, rebuild hit lists or mutate any gameplay owner. Completion
checks that the last shooter remains selected before and after the callback.
Every later gun dispatch follows its own canonical prop pass. The captured
local room publication and renderer eye-room union remain renderer-owned.

The multiplayer owner supplies the concrete main-loop providers and adds
`ge_original_multiplayer_views.o` to its build. This isolated change does not
modify `main.c`, active-prop orchestration or the player-body provider.

## Verification

- `scripts/test_bond_camera_live.sh`: ASan/UBSan with native and short enum
  layouts; 24 rotating four-player frames verify that each publication leaves
  every other slot's current, previous and projection matrices byte-identical.
  Local restoration leaves the player object and history pointers unchanged.
- `scripts/test_first_person_pose.sh`: the existing solo first-person camera
  and pose regression passes.
- `scripts/test_multiplayer_views.sh`: original camera/libultra and matrix
  producers plus instrumented caller-owned prop/capture boundaries; 216 frames
  across two, three and four players, every local slot and rotating/reversed
  order. Verifies copied local presentation and camera lifetime, correct
  selected camera before the simulated prop boundary, canonical last-shooter
  preservation, symmetric WORLD eye shifts and no canonical camera changes
  during either eye. Actual native player records, both generations of camera
  matrices, projection, selected owner and frustum compare identically across
  every local choice in one process; these raw test comparisons are not the
  portable network encoding. Missing capture, duplicate slots and provider failures are rejected.
  A source audit checks the original prop/interaction/gun ordering.

Both ABI variants run with ASan/UBSan. Only the original `guMtxF2L` translation
unit excludes its existing signed-shift instrumentation, matching the camera
regression convention. The presentation fixture does not test real character
animation, damage, weapon pickup/drop or a live match. Those remain covered by
the multiplayer owner's actor/weapon tests and integrated acceptance. No
emulator, hardware or timing result is claimed here.

## Canonical visibility is independent of renderer residency

The source audit resolves the remote-room concern without loading every room's
render geometry. `VISOP_PRELOAD_ROOM` and `VISOP_PRELOAD_ROOM_RANGE` use
`preload_ok` only to stop subsequent load requests. They do not gate the visible
room list or portal recursion. `room_loaded_mask` is the authored
`VISOP_DISABLE_ROOM`/`VISOP_DISABLE_ROOM_RANGE` bit, not renderer residency.
Original `posIsOnScreen` consumes the rendered-room flags, room screen bounds,
camera and fog; it does not consult the native dynamic scene.

Each shooter can use `ge_dam_visibility_runtime_run_with_providers` with live
door controls and a NULL preload callback. This runs the original room logic
while leaving the renderer queue and geometry untouched. Passing NULL for the
entire provider would instead use authored door controls and is incorrect when
a live door has changed.

The main-loop split should therefore be:

1. Produce the selected shooter's camera from the immutable world room origin,
   stage scales, viewport and near/far values. Publish camera and canonical
   visibility with live door controls and no preload callback. Do not call
   `stream_dam_renderer_camera` or remote `refresh_stage_*` scene functions.
2. Run its exact props/targeting/interaction/pickup/gun pass. Character hit-list
   publication follows that mono camera even when the local renderer has never
   loaded the remote room.
3. During local capture only, update the preview's local camera/result, run
   the renderer's own preload/eye-union/stream operations and build the durable
   local scene. Retain copies of the local camera and renderer visibility.
4. At finish, restore only the local preview fields from those owned copies.
   Preserve the last shuffled shooter’s canonical selection, frustum and full
   room/portal publication. Re-running native visibility for the device-local
   camera would make canonical state differ between peers. Diagnostic UI may
   inspect the displayed player temporarily, but must restore canonical
   selection before accepting raw input or executing another tick.

The full STAN data is already materialized and bound at stage initialization.
Despite its name, `materialize_stage_ordinary_resident` constructs setup model
instances without checking the dynamic scene's room residency. The audited
`dynamic_scene_is_resident` callsites and guard runtime `room_resident` service
control renderer scene sizing/building, matrix retention, or texture updates;
they are not the `chrTick`/`playerTick` visibility writer. Keep those renderer
operations local. This finding does not claim completion of unrelated
background triangle collision paths.

`scripts/test_multiplayer_visibility.sh` runs the unchanged original visibility
closure under ASan/UBSan with native and short enum layouts. Across 32 camera
and live-door configurations it verifies cold, partial and complete renderer
preload queues produce byte-identical room records and visible-room results
(only the load-request count differs). It then interleaves a remote camera and
door state and checks exact local restoration, unchanged queue bytes, retained
authored disable bits, and an unchanged room-load cycle. The runtime restore
entrypoint is exercised as well as the underlying original adapter. Existing
zero-portal Cradle and seven-point Depot fixture tests still pass.
