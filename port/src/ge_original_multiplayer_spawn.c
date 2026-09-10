#include "ge_original_multiplayer_spawn.h"
#include "ge_original_multiplayer_players.h"
#include "ge_original_multiplayer_rules.h"
#include "ge_original_player_body.h"
#include "ge_original_player_spawn_internal.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/ramromreplay.h"
#include <math.h>
#include <limits.h>
#include <string.h>

extern stagesetup g_CurrentSetup;
extern PadRecord *g_Startpad[16];
extern s32 startpadcount;
extern s32 bondviewGetRandomSpawnPadIndex(void);
extern void bondviewPlayerBeginLife(void);

static int selected_player(uint32_t slot)
{
    uint32_t count=ge_original_multiplayer_player_count(), i, j;
    if(count<2 || count>4 || slot>=count || getPlayerCount()!=(s32)count
            || get_cur_playernum()!=(s32)slot
            || g_CurrentPlayer!=ge_original_multiplayer_player(slot)
            || g_CurrentPlayer!=g_playerPointers[slot]
            || g_playerPerm!=&g_playerPlayerData[slot]
            || ge_original_spawn_player_get()!=g_CurrentPlayer) return 0;
    for(i=0;i<count;++i){
        if(!g_playerPointers[i] || g_playerPointers[i]!=ge_original_multiplayer_player(i)) return 0;
        for(j=0;j<i;++j)if(g_playerPointers[i]==g_playerPointers[j])return 0;
        if(g_playerPointers[i]->prop && (!isfinite(g_playerPointers[i]->prop->pos.x)
                || !isfinite(g_playerPointers[i]->prop->pos.y)
                || !isfinite(g_playerPointers[i]->prop->pos.z))) return 0;
    }
    return 1;
}

static int publish_setup(const GeOriginalMultiplayerSpawnProviders *providers)
{
    const GeOriginalStageSetupRuntime *stage;
    int i;
    if(!providers || !(stage=providers->stage) || !stage->loaded
            || !stage->setup || !stage->descriptor || !stage->descriptor->setup_key
            || strncmp(stage->descriptor->setup_key,"Ump_",4)!=0
            || !stage->pads_storage || !stage->intro_storage
            || stage->pad_count>INT32_MAX || stage->intro_word_count>INT32_MAX
            || stage->setup->pads!=stage->pads_storage
            || (const void *)stage->setup->intro!=stage->intro_storage
            || g_CurrentSetup.pads!=stage->pads_storage
            || (const void *)g_CurrentSetup.intro!=stage->intro_storage)
        return 0;
    if(ge_original_multiplayer_publish_spawns((const int32_t *)(const void *)stage->intro_storage,
            stage->intro_word_count,stage->pads_storage,stage->pad_count,
            (int32_t)check_ramrom_flags())!=GE_ORIGINAL_MP_OK) return 0;
    for(i=0;i<startpadcount;++i){
        GeOriginalStagePadPlacement pad;
        const int32_t index=(int32_t)(g_Startpad[i]-(PadRecord *)stage->pads_storage);
        if(!ge_original_stage_setup_pad_placement(stage,index,&pad)
                || !pad.has_stan || pad.room<0
                || !isfinite(pad.look[0]) || !isfinite(pad.look[1])
                || !isfinite(pad.look[2])) return 0;
    }
    return 1;
}

GeOriginalMultiplayerSpawnStatus ge_original_multiplayer_spawn_initial(
    uint32_t slot, const GeOriginalMultiplayerSpawnProviders *providers,
    GeOriginalPlayerViewState *view, GeOriginalMultiplayerSpawnResult *result)
{
    GeOriginalMultiplayerSpawnResult next={0};
    GeOriginalStagePadPlacement pad;
    const GeOriginalPlayerSpawnProviders *player;
    float height, floor;
    int selected;
    if(!providers || !view || !result || !providers->floor_y
            || !(player=providers->player) || !player->allocate_prop
            || !player->activate_prop || !player->enable_prop
            || !player->register_room || !player->deregister_room)
        return GE_ORIGINAL_MP_SPAWN_INVALID_ARGUMENT;
    if(!selected_player(slot) || g_CurrentPlayer->prop
            || !isfinite(g_CurrentPlayer->eyeheight) || g_CurrentPlayer->eyeheight<=0)
        return GE_ORIGINAL_MP_SPAWN_INVALID_PLAYER;
    if(!publish_setup(providers)) return GE_ORIGINAL_MP_SPAWN_INVALID_SETUP;
    /* The original signed cursor increments at most once per authored pad.
     * Fresh canonical players begin at random_byte; reject corrupt cursors
     * before entering its index arithmetic. */
    if(g_CurrentPlayer->field_29E0<0
            || g_CurrentPlayer->field_29E0>INT_MAX-startpadcount)
        return GE_ORIGINAL_MP_SPAWN_INVALID_PLAYER;
    height=get_player_mp_char_height((int32_t)slot);
    if(!isfinite(height) || height<=0) return GE_ORIGINAL_MP_SPAWN_INVALID_PLAYER;
    /* lv.c publishes character perspective before each original intro. */
    g_playerPerm->player_perspective_height=height;
    next.slot=slot;next.character_perspective=height;
    next.cursor_before=g_CurrentPlayer->field_29E0;
    /* Exact multiplayer choice from bondviewLoadSetupIntroSection. The
     * canonical selector owns cursor advance, proximity search and RNG. */
    selected=bondviewGetRandomSpawnPadIndex();
    if(selected<0 || selected>=startpadcount) return GE_ORIGINAL_MP_SPAWN_INVALID_SETUP;
    next.spawn_index=selected;
    next.pad_index=(int32_t)(g_Startpad[selected]-(PadRecord *)providers->stage->pads_storage);
    next.cursor_after=g_CurrentPlayer->field_29E0;
    if(!ge_original_stage_setup_pad_placement(providers->stage,next.pad_index,&pad))
        return GE_ORIGINAL_MP_SPAWN_INVALID_SETUP;
    floor=providers->floor_y(providers->context,pad.stan,pad.position[0],pad.position[2]);
    if(!isfinite(floor) || !isfinite(floor+g_CurrentPlayer->eyeheight))
        return GE_ORIGINAL_MP_SPAWN_INVALID_FLOOR;
    /* Exact post-selection position/yaw formulas from the original intro.
     * commit_existing owns the original collision/viewer/room assignments. */
    next.placement.position[0]=pad.position[0];
    next.placement.position[1]=g_CurrentPlayer->eyeheight+floor;
    next.placement.position[2]=pad.position[2];
    next.placement.floor_y=floor;
    next.placement.eye_height=g_CurrentPlayer->eyeheight;
    next.placement.look_angle_radians=M_TAU_F-atan2f(pad.look[0],pad.look[2]);
    next.placement.stan=pad.stan;next.placement.room=pad.room;
    ge_original_player_spawn_select(player,view);
    if(!ge_original_player_spawn_commit_existing(&next.placement))
        return GE_ORIGINAL_MP_SPAWN_COMMIT_FAILED;
    next.prop=g_CurrentPlayer->prop;next.placed=1;
    *result=next;
    return GE_ORIGINAL_MP_SPAWN_OK;
}

GeOriginalMultiplayerSpawnStatus ge_original_multiplayer_spawn_finalize(
    const GeOriginalMultiplayerSpawnProviders *providers,
    GeOriginalMultiplayerSpawnResult *result)
{
    if(!providers || !result || !result->placed || result->finalized)
        return GE_ORIGINAL_MP_SPAWN_INVALID_ARGUMENT;
    if(!selected_player(result->slot) || g_CurrentPlayer->prop!=result->prop)
        return GE_ORIGINAL_MP_SPAWN_INVALID_PLAYER;
    if(!publish_setup(providers)) return GE_ORIGINAL_MP_SPAWN_INVALID_SETUP;
    if(!providers->life_resources_ready || !g_CurrentPlayer->p_itemcur
            || g_CurrentPlayer->equipmaxitems<1
            || !providers->life_resources_ready(providers->context,result->slot))
        return GE_ORIGINAL_MP_SPAWN_LIFE_NOT_READY;
    bondviewPlayerBeginLife();
    if(!g_CurrentPlayer->bodyModel) return GE_ORIGINAL_MP_SPAWN_BODY_FAILED;
    result->finalized=1;
    return GE_ORIGINAL_MP_SPAWN_OK;
}
