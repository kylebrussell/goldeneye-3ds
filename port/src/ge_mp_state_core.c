#include "ge_mp_state_core.h"

#include <string.h>
#include <ultra64.h>
#include "game/player.h"
#include "ge_original_bond_input_provider.h"

extern u64 g_randomSeed;
extern u64 g_chrObjRandomSeed;
extern s32 random_byte;
extern s32 lastFrameCounter;
extern s32 currentFrameCounter;
extern s32 speedgraphframes;
extern s32 previousFrameCounter;
extern s32 halfFrameCounter;
extern s32 isFrameCounterOdd;
extern s32 halfMinusPreviousCounter;
extern s32 frameDelay;
extern s32 g_ControlsLockedFlag;
extern s32 g_ClockTimer;
extern f32 g_GlobalTimerDelta;
extern s32 g_GlobalTimer;
extern s32 D_80048380;
extern s32 g_CurrentStageToLoad;
extern s32 D_80048388;
extern s32 D_8004838C;
extern s32 D_80048390;
extern s32 D_80048394;
extern s32 g_MpTime;
extern f32 g_CurrentMultiPlayerSec;
extern s32 D_800483A8;
extern f32 g_CurrentMultiPlayerMin;
extern s32 D_800483B0;
extern f32 g_StageTimeSec;
extern s32 D_800483B8;
extern f32 g_PowerOnTimeSec;
extern s32 g_StageNum;
extern s32 g_MainStageNum;
extern s32 g_BossIsDebugMenuOpen;
extern s32 g_DebugAndUpdateStageFlag;
extern s32 g_ShowMemUseFlag;
extern s32 g_ShowMemBarsFlag;
extern u32 g_CurentMMallocValue;
extern u32 g_CurentMaMallocValue;
extern s32 player_num;
extern s32 g_PlayerTickCount;
extern PLAYER_ID array_PLAYER_IDs[4];
extern u32 copy_of_osgetcount_value_0, copy_of_osgetcount_value_1;

static const GeMpFieldSchema ge_core_fields[] = {
#define GE_MP_CORE_SCHEMA(name, id, type, count, path) {id, GE_MP_STATE_##type, 0, count, count},
    GE_MP_CORE_FIELD_LIST(GE_MP_CORE_SCHEMA)
#undef GE_MP_CORE_SCHEMA
};
static const GeMpRecordSchema ge_core_schema = {
    GE_MP_CORE_KIND, GE_MP_CORE_SYSTEMS, ge_core_fields, GE_MP_CORE_FIELD_COUNT
};

const GeMpRecordSchema *ge_mp_state_core_schema(void) { return &ge_core_schema; }

GeMpStateStatus ge_mp_state_core_capture(GeMpCoreState *state,
    const GePortState *port_state, uint64_t actor_id, GeMpCoreBoundary boundary)
{
    size_t index = 0;
    uint32_t key = ge_mp_registry_key(GE_MP_CORE_KIND, GE_MP_CORE_CREATION_NAMESPACE, GE_MP_CORE_SLOT);
    if (state == NULL || port_state == NULL || (uint32_t)(actor_id >> 32) != key ||
        (uint32_t)actor_id == 0 || (boundary != GE_MP_CORE_BOUNDARY_UNCLASSIFIED &&
        boundary != GE_MP_CORE_BOUNDARY_COMMITTED_RAW)) return GE_MP_STATE_INVALID;
    memset(state, 0, sizeof(*state));
    state->values.rng = g_randomSeed;
    state->values.chr_rng = g_chrObjRandomSeed;
    state->values.random_byte = random_byte;
    state->values.lastframecounter = lastFrameCounter;
    state->values.currentframecounter = currentFrameCounter;
    state->values.speedgraphframes = speedgraphframes;
    state->values.previousframecounter = previousFrameCounter;
    state->values.halfframecounter = halfFrameCounter;
    state->values.isframecounterodd = isFrameCounterOdd;
    state->values.halfminuspreviouscounter = halfMinusPreviousCounter;
    state->values.framedelay = frameDelay;
    state->values.g_controlslockedflag = g_ControlsLockedFlag;
    state->values.g_clocktimer = g_ClockTimer;
    state->values.g_globaltimerdelta = g_GlobalTimerDelta;
    state->values.g_globaltimer = g_GlobalTimer;
    state->values.d_80048380 = D_80048380;
    state->values.g_currentstagetoload = g_CurrentStageToLoad;
    state->values.d_80048388 = D_80048388;
    state->values.d_8004838c = D_8004838C;
    state->values.d_80048390 = D_80048390;
    state->values.d_80048394 = D_80048394;
    state->values.g_mptime = g_MpTime;
    state->values.g_currentmultiplayersec = g_CurrentMultiPlayerSec;
    state->values.d_800483a8 = D_800483A8;
    state->values.g_currentmultiplayermin = g_CurrentMultiPlayerMin;
    state->values.d_800483b0 = D_800483B0;
    state->values.g_stagetimesec = g_StageTimeSec;
    state->values.d_800483b8 = D_800483B8;
    state->values.g_powerontimesec = g_PowerOnTimeSec;
    state->values.g_stagenum = g_StageNum;
    state->values.g_mainstagenum = g_MainStageNum;
    state->values.g_bossisdebugmenuopen = g_BossIsDebugMenuOpen;
    state->values.g_debugandupdatestageflag = g_DebugAndUpdateStageFlag;
    state->values.g_showmemuseflag = g_ShowMemUseFlag;
    state->values.g_showmembarsflag = g_ShowMemBarsFlag;
    state->values.g_curentmmallocvalue = g_CurentMMallocValue;
    state->values.g_curentmamallocvalue = g_CurentMaMallocValue;
    state->values.player_num = player_num;
    state->values.player_tick_count = g_PlayerTickCount;
    state->values.port_ticks = port_state->simulation_ticks;
    state->values.control_style = ge_original_bond_input_control_style();
    for (index = 0; index < 4; ++index) state->values.shuffled_order[index] = (int32_t)array_PLAYER_IDs[index];
    (void)ge_original_input_state_snapshot(&state->values.input);
    (void)ge_original_joy_state_snapshot(&state->values.joy);
    state->diagnostics.os_count_previous = copy_of_osgetcount_value_0;
    state->diagnostics.os_count_current = copy_of_osgetcount_value_1;
    state->diagnostics.omissions = GE_MP_CORE_OMIT_OTHER_NATIVE_DOMAINS |
        GE_MP_CORE_OMIT_LEVEL_PROVIDER_CONTEXT | GE_MP_CORE_OMIT_INPUT_PROVIDER_ROSTER |
        GE_MP_CORE_OMIT_MULTIPLAYER_DEATH_GUARDS;
    if (boundary != GE_MP_CORE_BOUNDARY_COMMITTED_RAW)
        state->diagnostics.omissions |= GE_MP_CORE_OMIT_NONCOMMITTED_CLOCK;
#if !defined(VERSION_US) || defined(BUGFIX_R1)
    state->diagnostics.omissions |= GE_MP_CORE_OMIT_BUILD_VARIANT;
#endif
    if (state->values.input.ramrom_controller_count || state->values.input.ramrom_replay_present ||
        state->values.input.ramrom_block_present || state->values.input.playback_kind == GE_ORIGINAL_INPUT_PLAYBACK_RAMROM)
        state->diagnostics.omissions |= GE_MP_CORE_OMIT_RAMROM_CONTENT;
    if (state->values.input.playback_kind == GE_ORIGINAL_INPUT_PLAYBACK_UNKNOWN)
        state->diagnostics.omissions |= GE_MP_CORE_OMIT_UNKNOWN_PLAYBACK;
    if (state->values.joy.record_present)
        state->diagnostics.omissions |= GE_MP_CORE_OMIT_RECORDING_CALLBACK;
    if (state->values.joy.selected_bank < 0)
        state->diagnostics.omissions |= GE_MP_CORE_OMIT_UNKNOWN_JOY_BANK;
    index = 0;
#define GE_MP_CORE_BIND(name, id, type, count, path) \
    state->fields[index++] = (GeMpStateField){id, GE_MP_STATE_##type, count, &state->values.path};
    GE_MP_CORE_FIELD_LIST(GE_MP_CORE_BIND)
#undef GE_MP_CORE_BIND
    state->record = (GeMpStateRecord){actor_id, GE_MP_CORE_KIND, state->fields, index};
    return GE_MP_STATE_INCOMPLETE;
}
