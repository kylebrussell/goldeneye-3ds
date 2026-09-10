#include "ge_original_multiplayer_death.h"
#include "ge_original_gun_frame_arena.h"
#include <math.h>
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"

extern s32 g_ClockTimer;
extern f32 g_GlobalTimerDelta;
void ge_original_multiplayer_death_pre_move_exact(void);
void ge_original_multiplayer_death_post_gun_exact(void);

int ge_original_multiplayer_death_reset(GeOriginalMultiplayerDeathState *state)
{
    GeOriginalMultiplayerDeathState next = {0};
    s32 count = getPlayerCount(), slot;
    if (!state || count < 2 || count > 4) return 0;
    next.count = (uint8_t)count;
    for (slot = 0; slot < count; ++slot) {
        s32 prior;
        if (!g_playerPointers[slot] || !g_playerPointers[slot]->prop) return 0;
        for (prior = 0; prior < slot; ++prior)
            if (g_playerPointers[slot] == g_playerPointers[prior]) return 0;
        next.players[slot] = g_playerPointers[slot];
    }
    *state = next;
    return 1;
}

static int current_slot(const GeOriginalMultiplayerDeathState *state)
{
    s32 slot = get_cur_playernum(), i;
    if (!state || state->count < 2 || state->count > 4
            || getPlayerCount() != state->count || slot < 0 || slot >= state->count
            || g_CurrentPlayer != state->players[slot] || !g_CurrentPlayer->prop
            || g_ClockTimer < 0 || !isfinite(g_GlobalTimerDelta)
            || g_GlobalTimerDelta < 0) return -1;
    for (i = 0; i < state->count; ++i)
        if (g_playerPointers[i] != state->players[i]) return -1;
    return slot;
}

int ge_original_multiplayer_death_pre_move(
    GeOriginalMultiplayerDeathState *state, uint64_t tick)
{
    int slot = current_slot(state);
    uint8_t bit;
    if (slot < 0) return 0;
    bit = (uint8_t)(1U << slot);
    if ((state->pre_valid & bit)
            && (state->pre_tick[slot] == UINT64_MAX
                || tick != state->pre_tick[slot] + 1
                || !(state->post_valid & bit)
                || state->post_tick[slot] != state->pre_tick[slot])) return 0;
    ge_original_multiplayer_death_pre_move_exact();
    state->pre_tick[slot] = tick;
    state->pre_valid |= bit;
    return 1;
}

int ge_original_multiplayer_death_post_gun(
    GeOriginalMultiplayerDeathState *state, uint64_t tick)
{
    GeOriginalDynFrameAudit audit;
    int slot = current_slot(state);
    uint8_t bit;
    if (slot < 0) return 0;
    bit = (uint8_t)(1U << slot);
    if (!(state->pre_valid & bit) || state->pre_tick[slot] != tick
            || ((state->post_valid & bit) && state->post_tick[slot] >= tick)
            || !ge_original_gun_frame_arena_active()
            || !ge_original_gun_frame_arena_audit(&audit)) return 0;
    /* die_blood_image_routine uses two unchecked original dynAllocate calls. */
    if (g_CurrentPlayer->cameramode != 1 && g_CurrentPlayer->bonddead
            && !g_CurrentPlayer->deathanimfinished
            && (g_CurrentPlayer->bonddead == 1 || !g_CurrentPlayer->redbloodfinished)
            && audit.capacity - audit.used < 2U * 80U * 96U) return 0;
    ge_original_multiplayer_death_post_gun_exact();
    state->post_tick[slot] = tick;
    state->post_valid |= bit;
    return 1;
}
