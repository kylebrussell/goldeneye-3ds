#include "ge_original_level.h"

#include <stddef.h>

#include "game/frametiming.h"

extern int32_t g_ControlsLockedFlag;
extern int32_t g_ClockTimer;
extern float g_GlobalTimerDelta;
extern int32_t g_GlobalTimer;
extern int32_t D_80048380;
extern int32_t g_CurrentStageToLoad;
extern int32_t D_80048388;
extern int32_t D_80048390;
extern int32_t D_80048394;
extern float g_CurrentMultiPlayerSec;
extern float g_CurrentMultiPlayerMin;
extern int32_t D_800483B0;
extern float g_StageTimeSec;
extern int32_t D_800483B8;
extern float g_PowerOnTimeSec;

void lvlManageMpGameTimerSlice(void);

/* Canonical lv.c accessors used by campaign AI timer comparisons. */
float lvlGetCurrentMultiPlayerSec(void)
{
    return g_CurrentMultiPlayerSec;
}

float lvlGetCurrentMultiPlayerMin(void)
{
    return g_CurrentMultiPlayerMin;
}

static GeOriginalLevelProviders g_level_providers;

void ge_port_level_reset_tlb_entries(void)
{
    if (g_level_providers.reset_tlb_entries != NULL) {
        g_level_providers.reset_tlb_entries(g_level_providers.context);
    }
}

int32_t ge_port_level_is_paused(void)
{
    if (g_level_providers.is_paused == NULL) {
        return 0;
    }

    return g_level_providers.is_paused(g_level_providers.context);
}

uint16_t ge_port_level_buttons_pressed(void)
{
    if (g_level_providers.buttons_pressed == NULL) {
        return 0;
    }

    return g_level_providers.buttons_pressed(g_level_providers.context);
}

void ge_port_level_tick_subsystem(int32_t subsystem)
{
    if (g_level_providers.tick_subsystem != NULL) {
        g_level_providers.tick_subsystem(
            (GeOriginalLevelSubsystem)subsystem,
            g_level_providers.context);
    }
}

void ge_original_level_init(const GeOriginalLevelProviders *providers)
{
    if (providers == NULL) {
        g_level_providers.is_paused = NULL;
        g_level_providers.reset_tlb_entries = NULL;
        g_level_providers.context = NULL;
        g_level_providers.buttons_pressed = NULL;
        g_level_providers.tick_subsystem = NULL;
    } else {
        g_level_providers = *providers;
    }

    g_ControlsLockedFlag = 0;
    g_ClockTimer = 0;
    g_GlobalTimerDelta = 0.0f;
    g_GlobalTimer = 0;
    D_80048380 = 0;
    g_CurrentStageToLoad = 90;
    D_80048388 = 0;
    D_80048390 = 0;
    D_80048394 = 0;
    g_CurrentMultiPlayerSec = 0.0f;
    D_800483B0 = 0;
    g_StageTimeSec = 0.0f;
    D_800483B8 = 0;
    g_PowerOnTimeSec = 0.0f;
    speedgraphframes = 1;
}

void ge_original_level_set_controls_locked(int32_t locked)
{
    g_ControlsLockedFlag = locked;
}

void ge_original_level_set_stage(int32_t stage_id)
{
    g_CurrentStageToLoad = stage_id;
}

void ge_original_level_timer_tick(int32_t retrace_frames)
{
    speedgraphframes = retrace_frames;
    lvlManageMpGameTimerSlice();
}

void ge_original_level_tick(int32_t retrace_frames)
{
    ge_original_level_timer_tick(retrace_frames);
}

void ge_original_level_timer_snapshot(GeOriginalLevelTimerState *state)
{
    if (state == NULL) {
        return;
    }

    state->controls_locked = g_ControlsLockedFlag;
    state->clock_timer = g_ClockTimer;
    state->global_timer = g_GlobalTimer;
    state->active_frame_updates = D_80048380;
    state->global_timer_delta = g_GlobalTimerDelta;
    state->stage_id = g_CurrentStageToLoad;
    state->multiplayer_timer = D_80048394;
    state->multiplayer_seconds = g_CurrentMultiPlayerSec;
    state->idle_frames = D_80048390;
    state->idle_latched = D_80048388;
    state->active_stage_frames = D_800483B0;
    state->stage_seconds = g_StageTimeSec;
    state->power_on_frames = D_800483B8;
    state->power_on_seconds = g_PowerOnTimeSec;
}
