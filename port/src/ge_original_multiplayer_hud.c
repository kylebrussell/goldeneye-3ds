#include "ge_original_multiplayer_hud.h"
#include <ultra64.h>
#include <bondtypes.h>
#include "game/bondview.h"
#include "game/options.h"
#include <string.h>

extern struct player *g_playerPointers[4];
extern struct player *g_CurrentPlayer;
extern s32 getPlayerCount(void);
extern s32 get_cur_playernum(void);
extern u32 watch_screen_index;
extern s32 mission_brief_index;

static int selected(const GeOriginalMultiplayerHud *state, uint32_t slot)
{
    return state != NULL && state->initialized && !state->failed
        && slot < state->player_count && g_CurrentPlayer != NULL
        && g_CurrentPlayer == g_playerPointers[slot]
        && get_cur_playernum() == (s32)slot
        && getPlayerCount() == (s32)state->player_count;
}

int ge_original_multiplayer_hud_begin_frame(GeOriginalMultiplayerHud *state,
    uint64_t tick, uint32_t player_count)
{
    if (state == NULL || player_count < 2U || player_count > 4U) return 0;
    if (state->initialized && (tick <= state->tick || state->failed
            || state->captured != (1U << state->player_count) - 1U)) return 0;
    memset(state, 0, sizeof(*state));
    state->initialized = 1U;
    state->tick = tick;
    state->player_count = player_count;
    return 1;
}

int ge_original_multiplayer_hud_prepare_shooter(GeOriginalMultiplayerHud *state,
    uint32_t slot)
{
    GeOriginalMultiplayerHudSnapshot *snapshot;
    if (!selected(state, slot) || (state->prepared & (1U << slot)) != 0U
            || state->prepared != state->captured) return 0;
    snapshot = &state->players[slot];
    snapshot->watch_animation_state = g_CurrentPlayer->watch_animation_state;
    snapshot->menu_on = g_CurrentPlayer->mpmenuon;
    snapshot->watch_open = snapshot->watch_animation_state != 0;
    snapshot->watch_objectives_visible =
        (snapshot->watch_animation_state == 5 || snapshot->watch_animation_state == 12)
        && watch_screen_index == WATCH_INDEX_MISSION_BRIEFING
        && mission_brief_index == BRIEF_INDEX_OBJECTIVES;
    if (g_CurrentPlayer->cameramode != 1) {
        if (!ge_original_gameplay_hud_render_snapshot(&snapshot->gameplay)) return 0;
        ge_original_gameplay_hud_prepare_exact();
    } else {
        state->intro_camera |= (uint8_t)(1U << slot);
    }
    state->prepared |= (uint8_t)(1U << slot);
    return 1;
}

int ge_original_multiplayer_hud_capture_shooter(GeOriginalMultiplayerHud *state,
    uint32_t slot)
{
    GeOriginalMultiplayerHudSnapshot *snapshot;
    GeOriginalGameplayHudRenderSnapshot tail;
    if (!selected(state, slot) || (state->prepared & (1U << slot)) == 0U
            || (state->captured & (1U << slot)) != 0U) return 0;
    snapshot = &state->players[slot];
    /* Failure after a timer producer must not allow a retry to tick twice. */
    state->failed = 1U;
    ge_original_bottom_hud_tick();
    ge_original_bottom_hud_prepare_exact();
    if (!ge_original_bottom_hud_render_snapshot(&snapshot->bottom)) return 0;
    ge_original_dam_mission_hud_tick();
    if (!ge_original_dam_mission_hud_render_snapshot(&snapshot->upper)) return 0;
    if ((state->intro_camera & (1U << slot)) == 0U) {
        if (!ge_original_gun_sight_snapshot(&snapshot->sight)
                || !ge_original_gun_sight_build_draw(&snapshot->sight,
                    &snapshot->sight_draw, &snapshot->sight_visible)
                || !ge_original_gameplay_hud_render_snapshot(&tail)) return 0;
        /* Original gauge geometry was emitted before death/respawn. Ammo is
         * emitted after it. Do not re-evaluate the earlier gauge predicate. */
        memcpy(tail.armour, snapshot->gameplay.armour, sizeof(tail.armour));
        memcpy(tail.health, snapshot->gameplay.health, sizeof(tail.health));
        tail.gauges_visible = snapshot->gameplay.gauges_visible;
        snapshot->gameplay = tail;
    }
    ge_original_multiplayer_hud_fade_snapshot_exact(&snapshot->fade);
    state->captured |= (uint8_t)(1U << slot);
    state->failed = 0U;
    return 1;
}

int ge_original_multiplayer_hud_get(const GeOriginalMultiplayerHud *state,
    uint64_t tick, uint32_t slot, GeOriginalMultiplayerHudSnapshot *snapshot)
{
    if (state == NULL || snapshot == NULL || !state->initialized || state->failed
            || tick != state->tick || slot >= state->player_count
            || (state->captured & (1U << slot)) == 0U) return 0;
    *snapshot = state->players[slot];
    return 1;
}
