# Canonical multiplayer initial placement

`ge_original_multiplayer_spawn_initial(slot, providers, view, result)` is a
portable stage-session boundary over the existing canonical multiplayer player
owner, rules spawn selector and original intro player commit. The caller first
creates the roster, configures the match, binds the source setup to STAN,
publishes that setup, and selects the target slot.

The provider carries the loaded source stage, existing player prop/room
providers, a floor-Y query, and a readiness callback used only by finalize.
The initial call checks current-player/permanent/publication ownership,
bounded intro/pad storage, finite pad directions and valid spawn cursors.
It republishes the validated authored spawn list, applies the character's
canonical perspective ratio, invokes `bondviewGetRandomSpawnPadIndex`, then
uses `ge_original_player_spawn_commit_existing`. The original selector owns
cursor increments, the nearby-player search and shared RNG fallback. No solo
player reset or alternate spawn distribution is introduced.

Both calls leave the requested slot selected. The resulting `view` is the
current spawn publication until the caller selects another publication.
The result records slot, authored pad index, spawn-list index, cursor before
and after, character perspective, position/floor/yaw/STAN/room and the allocated
player prop. A player that already has a prop cannot be initially spawned a
second time. Resource failure after selection may consume the canonical cursor
or RNG; the caller should abort that stage creation rather than retrying it as
if no work occurred.

The source `lv.c` ordering places each new player before `bondviewPlayerBeginLife`.
`initBONDdataforPlayer` initially sets eyeheight to 1.0. The intro uses that
current value for its collision position; BeginLife subsequently computes
`player_perspective_height * 185 - 10`. The adapter preserves this ordering.
It does not preapply the later character eyeheight to spawn coordinates.

After placement, the caller loads the canonical intro inventory and cuffs,
initializes distinct hand buffers, binds hand models and installs body/gait
providers. `ge_original_multiplayer_spawn_finalize(providers, result)` requires
an initialized inventory and a true readiness callback, then invokes only the
existing exact `bondviewPlayerBeginLife`. That function adds the fist, equips
the initial hands, resets per-life counters and constructs a missing body.
Finalize rejects a second invocation and reports missing body construction.
The adapter does not repeat intro loadout initialization or reset match stats,
team/token state, hand storage, roster ownership or RNG seeds.

`bash scripts/test_multiplayer_spawn.sh [MULTIPLAYER_PACK]` exercises every
canonical arena with two players and all seven four-player arenas with four.
It loads real setup and STAN assets and uses the original 600-prop allocator,
active list and room registration. An independently extracted original intro
selection block is the reference for pad, position, yaw, cursor and RNG state.
The fixture verifies nonoverlapping pads when no fallback RNG is needed,
character perspective including Jaws, separate BeginLife ordering, real
inventory insertion, and preservation of persistent stats/team/hand state.
Body/model storage and equip presentation are test resource boundaries; their
full runtime lifecycle has separate fixtures. Invalid intro termination,
current slot, cursor, missing life resources and repeated calls are rejected.

The runtime module needs the original-type, setup, AIPARSE and US ABI flags
used by the player and rules adapters. No new runtime generated source is
needed. The fixture passes ASan/UBSan; the adapter passes focused ARM
`-fshort-enums -Wall -Wextra -Werror` compilation.
