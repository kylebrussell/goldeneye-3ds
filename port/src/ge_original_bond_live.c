#include "ge_original_bond_live.h"
#include "ge_original_bond_input_provider.h"
#include "ge_original_bond_move_state.h"
#include "ge_original_player_spawn_internal.h"
#include "ge_original_input.h"

#include <ultra64.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/player.h"
#include "game/gun.h"
#include "game/options.h"
#include <string.h>

void ge_original_bond_input_initialize_player_hands(
    void *right_buffer, void *left_buffer);
void bondinvReinitInv(void);
void bondviewProcessInput(s8 stick_x, s8 stick_y, u16 buttons,
                          u16 oldbuttons);
void MoveBond(s8 stick_x, s8 stick_y, u16 buttons, u16 oldbuttons);
void ge_port_bond_movement_publish(struct player *player);
extern struct player_data *g_playerPerm;
extern s32 g_ClockTimer;
extern s32 g_GlobalTimer;
extern f32 g_GlobalTimerDelta;
extern s32 g_PlayerIsInTank;
extern u32 watch_screen_index;
extern s32 mission_brief_index;
extern s32 lvlGetControlsLockedFlag(void);
extern GunModelFileRecord gitem_structs[];

static void ge_original_bond_live_set_timers(
    GeOriginalBondInputProvider *provider,
    int32_t clock_timer,
    int32_t global_timer,
    float global_timer_delta)
{
    provider->clock_timer = clock_timer;
    provider->global_timer = global_timer;
    provider->global_timer_delta = global_timer_delta;
    g_ClockTimer = clock_timer;
    g_GlobalTimer = global_timer;
    g_GlobalTimerDelta = global_timer_delta;
}

int ge_original_bond_live_initialize(void *right_hand_buffer,
                                     void *left_hand_buffer,
                                     GeOriginalBondLiveState *state)
{
    static InvItem inventory_slots[30];
    struct player *player = ge_original_spawn_player_get();
    if (state == NULL || player == NULL || right_hand_buffer == NULL
            || left_hand_buffer == NULL) return 0;
    if (!ge_original_bond_move_state_initialize_single_player()
            || g_playerPerm == NULL) return 0;
    ge_original_bond_input_bind_player(player, g_playerPerm);
    ge_original_bond_input_provider_reset_normal_dam();
    ge_original_bond_input_initialize_player_hands(
        right_hand_buffer, left_hand_buffer);
    memset(inventory_slots, 0, sizeof(inventory_slots));
    player->p_itemcur = inventory_slots;
    player->equipmaxitems =
        (s32)(sizeof(inventory_slots) / sizeof(inventory_slots[0]));
    bondinvReinitInv();
    if (!bondviewLoadSetupIntroLoadoutSlice(&state->loadout)) return 0;
    state->initialization_count++;
    state->initialized = 1;
    return 1;
}

int ge_original_bond_live_tick(GeOriginalBondLiveState *state,
                               int32_t clock_timer,
                               int32_t global_timer,
                               float global_timer_delta)
{
    GeOriginalBondInputFrame frame;
    GeOriginalBondInputProvider *provider;
    struct player *player;

    if (state == NULL || !state->initialized
            || (player = ge_original_spawn_player_get()) == NULL) return 0;
    provider = ge_original_bond_input_provider();
    ge_original_bond_live_set_timers(provider, clock_timer, global_timer,
                                     global_timer_delta);
    ge_original_input_read_bond_frame(player->prev_buttons_pressed, &frame);
    bondviewProcessInput(frame.stick_x, frame.stick_y,
                         frame.buttons, frame.oldbuttons);
    state->input_tick_count++;
    return 1;
}

int ge_original_bond_move_live_tick(GeOriginalBondLiveState *state,
                                    int32_t clock_timer,
                                    int32_t global_timer,
                                    float global_timer_delta)
{
    GeOriginalBondInputFrame frame;
    GeOriginalBondInputProvider *provider;
    struct player *player;

    if (state == NULL || !state->initialized
            || (player = ge_original_spawn_player_get()) == NULL) return 0;
    provider = ge_original_bond_input_provider();
    ge_original_bond_live_set_timers(provider, clock_timer, global_timer,
                                     global_timer_delta);
    ge_original_input_read_bond_frame(player->prev_buttons_pressed, &frame);
    MoveBond(frame.stick_x, frame.stick_y, frame.buttons, frame.oldbuttons);
    ge_port_bond_movement_publish(player);
    state->move_tick_count++;
    return 1;
}

int ge_original_bond_live_aim_snapshot(GeOriginalBondAimSnapshot *snapshot)
{
    struct player *player = ge_original_spawn_player_get();
    if (snapshot == NULL || player == NULL) return 0;
    snapshot->crosshair[0] = player->crosshair_angle.x;
    snapshot->crosshair[1] = player->crosshair_angle.y;
    snapshot->autoaim[0] = player->autoaimx;
    snapshot->autoaim[1] = player->autoaimy;
    snapshot->screen_left = player->c_screenleft;
    snapshot->screen_top = player->c_screentop;
    snapshot->screen_width = player->c_screenwidth;
    snapshot->screen_height = player->c_screenheight;
    snapshot->target_x = player->autoaim_target_x;
    snapshot->target_y = player->autoaim_target_y;
    snapshot->target_time_x = player->autoxaimtime60;
    snapshot->target_time_y = player->autoyaimtime60;
    return 1;
}

int ge_original_bond_live_motion_snapshot(
    GeOriginalBondMotionSnapshot *snapshot)
{
    struct player *player = ge_original_spawn_player_get();
    if (snapshot == NULL || player == NULL) return 0;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->speed_forwards = player->speedforwards;
    snapshot->speed_sideways = player->speedsideways;
    snapshot->speed_boost = player->speedboost;
    memcpy(snapshot->head_position, player->headpos.f,
        sizeof(snapshot->head_position));
    snapshot->stan_height = player->stanHeight;
    snapshot->eye_height = player->eyeheight;
    snapshot->controls_locked = lvlGetControlsLockedFlag();
    snapshot->watch_animation_state = player->watch_animation_state;
    snapshot->dead = player->bonddead;
    snapshot->camera_mode = player->cameramode;
    snapshot->in_tank = g_PlayerIsInTank;
    return 1;
}

int ge_original_bond_live_watch_objectives_visible(void)
{
    struct player *player = ge_original_spawn_player_get();
    return player != NULL
        && (player->watch_animation_state == WATCH_ANIMATION_0x5
            || player->watch_animation_state == WATCH_ANIMATION_0xc)
        && watch_screen_index == WATCH_INDEX_MISSION_BRIEFING
        && mission_brief_index == BRIEF_INDEX_OBJECTIVES;
}

int ge_original_bond_live_statistics_state(
    int32_t shot_register[GE_ORIGINAL_FRONTEND_SHOT_REGISTER_COUNT],
    int32_t *kill_count,
    GeOriginalFrontendHeldWeapon
        held[GE_ORIGINAL_FRONTEND_HELD_WEAPON_COUNT])
{
    struct player *player = ge_original_spawn_player_get();
    size_t index;
    if (player == NULL || g_playerPerm == NULL || shot_register == NULL
            || kill_count == NULL || held == NULL) return 0;
    for (index = 0U; index < GE_ORIGINAL_FRONTEND_SHOT_REGISTER_COUNT;
            ++index)
        shot_register[index] = g_playerPerm->shot_count[index];
    *kill_count = g_playerPerm->kill_count;
    for (index = 0U; index < GE_ORIGINAL_FRONTEND_HELD_WEAPON_COUNT;
            ++index) {
        held[index].weapon1 = player->gunheldarr[index].weapon1;
        held[index].weapon2 = player->gunheldarr[index].weapon2;
        held[index].total_time = player->gunheldarr[index].totaltime;
    }
    return 1;
}

int ge_original_bond_live_weapon_choice_text(
    int32_t weapon, uint16_t *text_id)
{
    if (text_id == NULL || weapon < ITEM_UNARMED || weapon > ITEM_JOYPAD)
        return 0;
    *text_id = gitem_structs[weapon].weapon_of_choice_text;
    return 1;
}
