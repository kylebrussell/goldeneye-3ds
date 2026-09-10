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
5. After all slots, `finish_local` selects the local player, restores its camera
   frustum and invokes the provider that restores the saved local canonical
   visibility. Both eye draws use the captured scene and shader eye shifts.
   Neither eye invokes camera publication, prop ticks or model simulation.

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

The final restore does not invent local character flags or rebuild hit lists.
Every later gun dispatch must again follow its own canonical prop pass. The
saved local room publication and renderer eye-room union are separate; the
renderer-only union must never feed canonical `chrTick` or shooting.

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
  selected camera before the simulated prop boundary, final local restoration,
  symmetric WORLD eye shifts and no canonical camera changes during either
  eye. Missing capture, duplicate slots and provider failures are rejected.
  A source audit checks the original prop/interaction/gun ordering.

Both ABI variants run with ASan/UBSan. Only the original `guMtxF2L` translation
unit excludes its existing signed-shift instrumentation, matching the camera
regression convention. The presentation fixture does not test real character
animation, damage, weapon pickup/drop or a live match. Those remain covered by
the multiplayer owner's actor/weapon tests and integrated acceptance. No
emulator, hardware or timing result is claimed here.
