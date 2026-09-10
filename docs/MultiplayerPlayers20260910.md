# Canonical multiplayer player and input owners

This change restores reusable native player ownership. It does **not** enable
multiplayer in `main.c`, and the wireless lobby remains a connection test.
The standalone fixtures are not complete authored matches.

## Canonical state and platform boundaries

`ge_original_multiplayer_players_create(count, local_slot, seed, chr_seed)` allocates
native-width storage for two to four original `struct player` objects before
publishing the roster. `initBONDdataforPlayer`, its original default hand, and
`init_player_BONDdata` are extracted from their canonical definitions. Only the
N64 byte-count allocator is replaced by the preallocated native player slot.
Permanent statistics, handicap defaults, random-byte initialization and the
original PRNG streams are shared canonical state. Both seed inputs are full
64-bit initial stream states agreed independently by all peers: `seed` binds
`g_randomSeed`, and `chr_seed` binds `g_chrObjRandomSeed`. They are assigned
directly without the original 32-bit setters' truncation or increment. Player
initialization consumes one gameplay draw for `random_byte`, then each original
player initializer increments that byte counter. Allocation failure and invalid
configuration preserve the prior session. Destroy restores its saved player,
permanent-data, both RNG streams and persistent control setting; consumers of
match props/models must close first. Match control styles do not overwrite the
prior solo preference after destruction.

Every player has a separate inventory and both hand buffers supplied by the
caller. `ge_original_bond_live_initialize_multiplayer` initializes those hands
and the original intro loadout without calling the solo roster reset.
`ge_original_player_spawn_commit_existing` places an already initialized slot
without clearing its canonical hands, inventory and per-life defaults.

`ge_original_multiplayer_select_player` selects the canonical current player,
permanent statistics and input slot. `ge_original_spawn_player_get` follows the
canonical current-player pointer while the roster is active. Temporary
`set_cur_player` switches inside damage attribution therefore also switch the
camera/movement and input-provider player pointers. All roster providers receive
the same timer values before movement; options and FOV remain per player.
`ge_original_multiplayer_select_local_player` restores the display player after
simulation. The spawn and movement `select` APIs preserve already initialized
per-slot publication state; gait selection validates that its native model
belongs to the selected canonical player. The original initial view is full 320 by 240 for each slot.

The native joy playback ring has eight virtual pads, with primary pad `p` and
secondary pad `p + count`. The N64 hardware build retains its four-pad ABI.
One `ge_original_input_tick_players` call publishes all players at the same tick;
selecting a slot never consumes another sample or generates an edge. Raw network
samples use `ge_original_input_tick_pad_pairs` to avoid applying a receiver's
sensitivity or rounding to another player's already quantized controller data.
The outgoing conversion boundary is `ge_original_input_sample_pads`.

Gun model/matrix publications and host-width embedded-model equivalents are
retained separately for every player. The dynamic frame arena remains shared
for the canonical frame lifetime. Multiplayer ticks reject an implicit frame:
the caller must open one explicit frame around all players and must not render
stale publications after recycling that arena. Solo implicit ticks remain valid.

`ge_original_stage_active_props_compose_players` validates and publishes the
whole player roster alongside sorted authored/dynamic props. One pre-tick token
covers each player's prop pass, rejects duplicate player passes, and completes
only when all players were serviced. Callers retain original shuffled player
order (`get_nth_player_from_shuffled(i)`, `i=0..N-1`) and publish each player's
canonical visibility/camera before its pass. This order is a caller precondition,
not checked by the duplicate mask: the first shuffled viewport unfreezes and
advances shared player animation poses. Active-list ownership is tracked
independently from its head, so canonical RETICK reordering cannot leave a
dangling list on close or allow a stale owner to clear a replacement's list.

## Verification

- `scripts/test_multiplayer_players.sh`: ASan/UBSan, original initial/per-life
  state for two/three/four players, independent health/hands/inventory/options,
  shared timers, native full views, temporary canonical player switches,
  simultaneous controller edges, exact raw network pad delivery, high-bit RNG
  seed preservation, and restoration of both RNG streams and control settings.
- `scripts/test_multiplayer_active_props.sh`: ASan/UBSan, solo/two/four roster
  construction, duplicate rejection without losing the prior list, empty arena
  support, canonical shuffled pass selection, RETICK cleanup, stale-owner
  rejection, one world pre-tick and one prop pass per player. Dispatch callbacks
  are observed doubles; this test does not substitute gameplay mechanics.
- `scripts/test_multiplayer_gun_ownership.sh`: ASan/UBSan, separate hand models,
  matrix publications and frame statistics for two/four players, using the
  extracted original `dynAllocate` within one shared frame. Implicit multiplayer
  ticks are rejected before the producer runs; solo implicit ticks still work. The producer
  is a resource-publication double; canonical shooting is tested separately.
- `scripts/test_multiplayer_gait.sh`: original animation/model matrix tests plus
  two/four interleaved player gaits, mismatched selection rejection and preserved
  other-player animation state.
- Existing input-provider, canonical frame-order and gun/frame-arena checks.
- ARM Makefile retains the new public multiplayer entry points with `-u`, so
  linking cannot hide unresolved new dependencies by removing unused functions.

## Required runtime integration

The main loop still constructs and ticks one `RuntimeFirstPersonModels`, gait,
movement publication and camera. It must own one set per player, construct the
selected multiplayer setup, run canonical shuffled simulation and prop passes,
then select the local view for both eyes. Full gameplay-state serialization,
state-hash coverage, agreed per-player controls/rules, actor ID lifetime,
start/results frontend flow, and real UDS matches remain separate work. The
transport's build/content/state digest fields alone cannot establish that these
owners are correctly wired or that the simulation is deterministic.
