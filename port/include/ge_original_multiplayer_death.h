#ifndef GE_ORIGINAL_MULTIPLAYER_DEATH_H
#define GE_ORIGINAL_MULTIPLAYER_DEATH_H
#include <stdint.h>

/* Per-match scheduling metadata. Gameplay state remains in canonical players. */
typedef struct GeOriginalMultiplayerDeathState {
    void *players[4];
    uint64_t pre_tick[4], post_tick[4];
    uint8_t count, pre_valid, post_valid;
} GeOriginalMultiplayerDeathState;

/* Binds the current canonical 2..4-player roster. Reset after roster creation. */
int ge_original_multiplayer_death_reset(GeOriginalMultiplayerDeathState *state);
/* For EVERY selected simulation player, before MoveBond: original screen fade
 * update then character fade update. One pre/post pair per monotonically
 * contiguous tick; duplicate, missing-phase and changed-roster calls fail. */
int ge_original_multiplayer_death_pre_move(
    GeOriginalMultiplayerDeathState *state, uint64_t tick);
/* After that player's gun update, while the shared dynamic frame arena is
 * open: original multiplayer death/blood/animation-completion state. This
 * consumes real blood decode allocations, never draws or calls respawn.
 * The rules owner checks original button/scenario eligibility afterwards.
 * Local/stereo drawing must not also run these state transitions. */
int ge_original_multiplayer_death_post_gun(
    GeOriginalMultiplayerDeathState *state, uint64_t tick);
#endif
