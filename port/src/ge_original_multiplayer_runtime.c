#include "ge_original_multiplayer_runtime.h"
#include "ge_original_multiplayer_watch.h"
#include "ge_original_bond_input_provider.h"
#include "ge_original_door_interaction.h"
#include "ge_original_gameplay_services.h"
#include "ge_original_gun_live.h"
#include "ge_original_input.h"
#include "ge_original_multiplayer_players.h"
#include "ge_original_multiplayer_rules.h"
#include "ge_original_player_held_items.h"
#include "ge_original_effect_runtime.h"
#include "ge_original_prop_render_semantics.h"
#include "ge_original_stage_alarm_interaction.h"
#include "ge_original_stage_pickup.h"

#include <math.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/bondview.h"
#include "game/gun.h"
#include "game/options.h"
#include "game/mp_music.h"
#include "game/lv.h"

extern void chraiUpdateOnscreenPropCount(void);
extern void chrpropUpdateAutoaimTarget(void);
extern void attempt_reload_item_in_hand(GUNHAND hand);
extern TICKOP ge_original_stage_prop_pickup_exact(PropRecord *prop, bool immediate);
extern bool ge_original_mp_runtime_interaction_exact(void);
extern void ge_original_mp_runtime_mission_clock_exact(s32 clock_timer);
extern void ge_original_mp_runtime_watch_clock_exact(void);
static int interaction_failed;

static void play_sfx(void *context, uint32_t sound)
{
    (void)context;
    ge_original_gameplay_services_play_sfx(sound);
}

static int pickup(void *context, void *prop, int immediate)
{
    (void)context;
    return ge_original_stage_prop_pickup_exact(prop, immediate);
}

TICKOP ge_original_mp_runtime_interact_object(PropRecord *prop)
{
    const GeOriginalStageAlarmInteractionProviders providers = {
        .play_sfx = play_sfx, .pickup_by_player = pickup
    };
    GeOriginalStageAlarmInteractionResult result;
    if (ge_original_stage_object_interact_exact(prop, &providers, &result)
            != GE_ORIGINAL_STAGE_ALARM_INTERACTION_OK) {
        interaction_failed = 1;
        return TICKOP_NONE;
    }
    return (TICKOP)result.tick_operation;
}

TICKOP ge_original_mp_runtime_interact_door(PropRecord *prop)
{
    GeOriginalDoorInteractionResult result =
        ge_original_door_interaction_activate_native(prop);
    if (result != GE_ORIGINAL_DOOR_INTERACTION_ACTIVATED
            && result != GE_ORIGINAL_DOOR_INTERACTION_LOCKED) interaction_failed = 1;
    return TICKOP_NONE;
}

static int interact(void)
{
    interaction_failed = 0;
    /* bond_pressed_reload_activate is the original field_D0 getter. The
     * retained bond_interact_object executes the original mixed traversal
     * and propExecuteTickOperation, including pickup/free outcomes. */
    if (g_CurrentPlayer->field_D0 && ge_original_mp_runtime_interaction_exact()) {
        if (interaction_failed) return 0;
        attempt_reload_item_in_hand(GUNRIGHT);
        attempt_reload_item_in_hand(GUNLEFT);
    }
    if (interaction_failed) return 0;
    return 1;
}

GeOriginalMultiplayerRuntimeStatus ge_original_multiplayer_runtime_begin_world(
    GeOriginalStageActiveProps *active_props, int32_t tick60,
    GeOriginalMultiplayerRegenBatch *regeneration)
{
    GeOriginalMultiplayerRegenStatus regen;
    if (active_props == NULL || tick60 < 0 || regeneration == NULL
            || lvlGetCurrentStageToLoad() == LEVELID_TITLE)
        return GE_ORIGINAL_MP_RUNTIME_INVALID_ARGUMENT;
    if (!ge_original_effect_runtime_pre_world())
        return GE_ORIGINAL_MP_RUNTIME_WORLD_FAILED;
    if (ge_original_stage_active_props_pre_tick_exact(active_props)
            != GE_ORIGINAL_STAGE_ACTIVE_PROP_OK) return GE_ORIGINAL_MP_RUNTIME_WORLD_FAILED;
    regen = ge_original_multiplayer_regen_tick_all(tick60, regeneration);
    if (regen != GE_ORIGINAL_MP_REGEN_OK && regen != GE_ORIGINAL_MP_REGEN_IDLE)
        return GE_ORIGINAL_MP_RUNTIME_WORLD_FAILED;
    /* chrpropTick runs background AI and regeneration before the common
     * non-title lv.c slot countdown. The native helper observes unchanged
     * g_ClockTimer even when background AI was paused or already pending. */
    reset_all_music_slots();
    shuffle_player_ids();
    return GE_ORIGINAL_MP_RUNTIME_OK;
}

int ge_original_multiplayer_runtime_camera_fovy(float *fovy)
{
    if (fovy == NULL || g_CurrentPlayer == NULL || !isfinite(g_CurrentPlayer->fovy)
            || g_CurrentPlayer->fovy <= 0 || g_CurrentPlayer->fovy >= 180) return 0;
    *fovy = g_CurrentPlayer->fovy;
    return 1;
}

static int valid(const GeOriginalMultiplayerRuntimeFrame *frame)
{
    uint32_t slot;
    if (!frame || frame->tick == UINT64_MAX || frame->count < 2 || frame->count > 4
            || frame->local >= frame->count || !frame->death || !frame->hud || !frame->views
            || !frame->active_props || !frame->guards || !frame->select_player
            || !frame->produce_camera || !frame->sync_gun_resources
            || !frame->prepare_local_renderer || frame->clock_timer < 0
            || frame->global_timer < 0 || !isfinite(frame->global_timer_delta)
            || frame->global_timer_delta < 0
            || ge_original_multiplayer_player_count() != frame->count
            || !ge_original_gun_frame_arena_active()) return 0;
    for (slot = 0; slot < frame->count; ++slot)
        if (!frame->lives[slot] || !frame->lives[slot]->initialized) return 0;
    return 1;
}

GeOriginalMultiplayerRuntimeStatus ge_original_multiplayer_runtime_tick(
    GeOriginalMultiplayerRuntimeFrame *frame)
{
    uint32_t ordinal;
    if (!valid(frame)) {
        if (frame) frame->status = GE_ORIGINAL_MP_RUNTIME_INVALID_ARGUMENT;
        return GE_ORIGINAL_MP_RUNTIME_INVALID_ARGUMENT;
    }
#define FAIL(code) do { frame->status = (code); return (code); } while (0)
    frame->last_slot = UINT32_MAX;
    if (ge_original_multiplayer_views_begin_frame(frame->views, frame->count,
            frame->local, &frame->view_providers) != GE_ORIGINAL_MULTIPLAYER_VIEW_OK)
        FAIL(GE_ORIGINAL_MP_RUNTIME_VIEW_FAILED);
    if (!ge_original_multiplayer_hud_begin_frame(frame->hud, frame->tick, frame->count))
        FAIL(GE_ORIGINAL_MP_RUNTIME_HUD_FAILED);
    for (ordinal = 0; ordinal < frame->count; ++ordinal) {
        uint32_t slot = (uint32_t)get_nth_player_from_shuffled((s32)ordinal);
        GeOriginalBondCameraConfig config;
        GeOriginalBondCameraResult camera;
        frame->last_slot = slot;
        if (slot >= frame->count || !frame->select_player(frame->context, slot)
                || get_cur_playernum() != (s32)slot
                || g_CurrentPlayer != ge_original_multiplayer_player(slot))
            FAIL(GE_ORIGINAL_MP_RUNTIME_SELECT_FAILED);
        /* bondviewMovePlayerUpdateViewport establishes 60 before MoveBond;
         * watch/weapon zoom may then replace both the player and VI values. */
        g_CurrentPlayer->fovy = 60.0f;
        ge_original_bond_input_set_fov_y(60.0f);
        if (!ge_original_multiplayer_death_pre_move(frame->death, frame->tick))
            FAIL(GE_ORIGINAL_MP_RUNTIME_DEATH_FAILED);
        currentPlayerSetYAutoAimEnabled(cur_player_get_autoaim());
        currentPlayerSetXAutoAimEnabled(cur_player_get_autoaim());
        currentPlayerSetLookAheadSetting(cur_player_get_lookahead());
        gunSetGunAmmoVisible(GUNAMMOREASON_OPTION, cur_player_get_ammo_onscreen_setting());
        ge_original_player_apply_sight_option_exact();
        ge_original_mp_runtime_mission_clock_exact(frame->clock_timer);
        if (!ge_original_bond_move_live_tick(frame->lives[slot], frame->clock_timer,
                frame->global_timer, frame->global_timer_delta))
            FAIL(GE_ORIGINAL_MP_RUNTIME_MOVE_FAILED);
        ge_original_mp_runtime_watch_clock_exact();
        if(ge_original_multiplayer_watch_tick()!=GE_ORIGINAL_MP_WATCH_OK)
            FAIL(GE_ORIGINAL_MP_RUNTIME_WATCH_FAILED);
        if (!frame->produce_camera(frame->context, slot, &config, &camera)
                || ge_original_multiplayer_views_begin_shooter(frame->views,
                    slot, &config, &camera) != GE_ORIGINAL_MULTIPLAYER_VIEW_OK)
            FAIL(GE_ORIGINAL_MP_RUNTIME_VIEW_FAILED);
        if (ge_original_stage_active_props_tick_exact(frame->active_props)
                != GE_ORIGINAL_STAGE_ACTIVE_PROP_OK)
            FAIL(GE_ORIGINAL_MP_RUNTIME_WORLD_FAILED);
        chraiUpdateOnscreenPropCount();
        chrpropUpdateAutoaimTarget();
        if (ge_original_player_held_items_tick() != GE_ORIGINAL_PLAYER_HELD_ITEMS_OK)
            FAIL(GE_ORIGINAL_MP_RUNTIME_HELD_ITEMS_FAILED);
        if (!interact()) FAIL(GE_ORIGINAL_MP_RUNTIME_INTERACTION_FAILED);
        ge_original_stage_props_tick_player_exact();
        /* bgLevelRender is before first-person gun production. Its original
         * room/pass/node order owns render-side state, hit-list retirement
         * and third-person muzzle RNG for every canonical viewport. */
        if (!ge_original_prop_render_semantics_prepare(frame->guards, frame->tick, slot))
            FAIL(GE_ORIGINAL_MP_RUNTIME_BODY_FAILED);
        if (!frame->sync_gun_resources(frame->context, slot))
            FAIL(GE_ORIGINAL_MP_RUNTIME_GUN_RESOURCE_FAILED);
        if (!ge_original_gun_live_tick()) FAIL(GE_ORIGINAL_MP_RUNTIME_GUN_FAILED);
        if (slot == frame->local
                && !frame->prepare_local_renderer(frame->context, slot, &config, &camera))
            FAIL(GE_ORIGINAL_MP_RUNTIME_VIEW_FAILED);
        if (ge_original_stage_guard_runtime_update_matrices(frame->guards, camera.view)
                != GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK
                || !ge_original_stage_guard_runtime_prepare_muzzle_flashes(
                    frame->guards, frame->tick, slot))
            FAIL(GE_ORIGINAL_MP_RUNTIME_BODY_FAILED);
        if (!ge_original_multiplayer_hud_prepare_shooter(frame->hud, slot))
            FAIL(GE_ORIGINAL_MP_RUNTIME_HUD_FAILED);
        if (!ge_original_multiplayer_death_post_gun(frame->death, frame->tick))
            FAIL(GE_ORIGINAL_MP_RUNTIME_DEATH_FAILED);
        if (ge_original_multiplayer_can_respawn((int32_t)slot, ge_original_input_buttons())
                && ge_original_multiplayer_respawn((int32_t)slot, ge_original_input_buttons())
                    != GE_ORIGINAL_MP_OK)
            FAIL(GE_ORIGINAL_MP_RUNTIME_RESPAWN_FAILED);
        if (!ge_original_multiplayer_hud_capture_shooter(frame->hud, slot))
            FAIL(GE_ORIGINAL_MP_RUNTIME_HUD_FAILED);
        if (slot == frame->local
                && ge_original_multiplayer_views_capture_local(frame->views)
                    != GE_ORIGINAL_MULTIPLAYER_VIEW_OK)
            FAIL(GE_ORIGINAL_MP_RUNTIME_VIEW_FAILED);
    }
    if (ge_original_multiplayer_views_finish_local(frame->views)
            != GE_ORIGINAL_MULTIPLAYER_VIEW_OK) FAIL(GE_ORIGINAL_MP_RUNTIME_VIEW_FAILED);
    ge_original_gameplay_services_tick();
    frame->status = GE_ORIGINAL_MP_RUNTIME_OK;
    return frame->status;
#undef FAIL
}
