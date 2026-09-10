#include "ge_original_multiplayer_watch.h"
#include "ge_original_multiplayer_session.h"
#include "ge_original_multiplayer_players.h"
#include "ge_original_language_text.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/front.h"
#include "game/mpmenu.h"
#include "game/frametiming.h"
#include <limits.h>
#include <math.h>
#include <string.h>
extern s32 selected_num_players,who_paused,chevron_glow,chevron_glow_timer,alt_gameover_msg,alt_gameover_msg_timer;
extern s32 g_pausedFlag,array_favweapon[4][2],mission_state;
extern PLAYER_ID array_PLAYER_IDs[4];
extern void mpwatchMenuTick(void);
extern s32 getMissiontimer(void);
static struct {
    const struct player *player;
    const char *text[2];
    uint64_t lifetime;
    int32_t token[2];
} award_text[4];
static GeOriginalMultiplayerWatchStatus call_status;
/* This slice's language calls are exclusively the 17 authored LMPMENU awards.
 * Use the port's native pointer-table owner, never packed g_LangBanks offsets. */
u8 *ge_original_mp_watch_language(s32 id)
{
    const char *text=NULL;
    if(id>=(LMPMENU<<10) && id<=(LMPMENU<<10)+16)
        text=ge_original_language_text((uint16_t)id);
    if(!text)call_status=GE_ORIGINAL_MP_WATCH_RESOURCE_FAILED;
    return (u8 *)text;
}

static GeOriginalMultiplayerWatchStatus validate(int awards)
{
    int count=getPlayerCount(),current=get_cur_playernum();int64_t total=0;
    if(count<2 || count>4 || current<0 || current>=count
            || g_CurrentPlayer!=g_playerPointers[current] || g_playerPerm!=&g_playerPlayerData[current]
            || gamemode!=GAMEMODE_MULTI || get_selected_num_players()!=count)
        return GE_ORIGINAL_MP_WATCH_INVALID_ROSTER;
    unsigned seen=0;
    if(awards==0)for(unsigned i=0;i<4;++i) {
        unsigned id=(unsigned)array_PLAYER_IDs[i];
        if(id>=4 || (seen&(1U<<id)))return GE_ORIGINAL_MP_WATCH_INVALID_ROSTER;
        seen|=1U<<id;
    }
    if(awards==1 && (mission_state<MISSION_STATE_0 || mission_state>MISSION_STATE_6))
        return GE_ORIGINAL_MP_WATCH_INVALID_STATS;
    if(speedgraphframes<0 || getMissiontimer()<0 || getMissiontimer()==INT_MAX)
        return GE_ORIGINAL_MP_WATCH_INVALID_STATS;
    for(int i=0;i<count;++i) {
        const struct player *p=g_playerPointers[i];
        if(!p)return GE_ORIGINAL_MP_WATCH_INVALID_ROSTER;
        for(int j=0;j<i;++j)if(p==g_playerPointers[j])return GE_ORIGINAL_MP_WATCH_INVALID_ROSTER;
        if((int64_t)getMissiontimer()-p->lifestarttime60>INT_MAX)
            return GE_ORIGINAL_MP_WATCH_INVALID_STATS;
        if(awards==1) {
            for(int j=0;j<count;++j) {
                if(g_playerPlayerData[i].kill_counts[j]<0)return GE_ORIGINAL_MP_WATCH_INVALID_STATS;
                total+=g_playerPlayerData[i].kill_counts[j];
            }
            if(g_playerPlayerData[i].shot_count[SHOT_REGISTER_TOTAL]<0
                    || g_playerPlayerData[i].shot_count[SHOT_REGISTER_HEAD]<0
                    || !isfinite(g_playerPlayerData[i].distance_traveled / 100000.0f
                        / ((getMissiontimer()+1)/(3600.0f*60.0f)))
                    || !isfinite(g_playerPlayerData[i].distance_traveled)
                    || !isfinite(g_playerPlayerData[i].body_armor_pickups))
                return GE_ORIGINAL_MP_WATCH_INVALID_STATS;
        }
    }
    if(total>INT_MAX)return GE_ORIGINAL_MP_WATCH_INVALID_STATS;
    if((int64_t)chevron_glow_timer+speedgraphframes>INT_MAX
            || (int64_t)alt_gameover_msg_timer+speedgraphframes>INT_MAX)
        return GE_ORIGINAL_MP_WATCH_INVALID_STATS;
    return GE_ORIGINAL_MP_WATCH_OK;
}
GeOriginalMultiplayerWatchStatus ge_original_multiplayer_watch_bind(void)
{
    int count=getPlayerCount();
    if(count<2 || count>4)return GE_ORIGINAL_MP_WATCH_INVALID_ROSTER;
    for(int i=0;i<count;++i)if(!g_playerPointers[i])return GE_ORIGINAL_MP_WATCH_INVALID_ROSTER;
    /* These are the actual front.c fields selected by native MP menus. */
    gamemode=GAMEMODE_MULTI;selected_num_players=count;
    return GE_ORIGINAL_MP_WATCH_OK;
}
void ge_original_mp_watch_publish_award(s32 slot,s32 which,const char *text)
{
    GeOriginalMultiplayerInventoryStorage storage;
    if(slot<0 || slot>=getPlayerCount() || which<0 || which>1 || !text) {
        call_status=GE_ORIGINAL_MP_WATCH_RESOURCE_FAILED;return;
    }
    struct player *p=g_playerPointers[slot];
    award_text[slot].player=p;
    award_text[slot].lifetime=ge_original_multiplayer_inventory_storage((uint32_t)slot,&storage)?storage.lifetime:0;
    award_text[slot].text[which]=text;
#if UINTPTR_MAX > UINT32_MAX
    int32_t token=(int32_t)(0x50000001U+(unsigned)slot*2U+(unsigned)which);
#else
    int32_t token=(int32_t)(uintptr_t)text;
#endif
    award_text[slot].token[which]=token;
    if(which)p->ptr_text_second_mp_award=token;else p->ptr_text_first_mp_award=token;
}
int ge_original_multiplayer_watch_award_text(uint32_t slot,uint32_t which,const char **text)
{
    GeOriginalMultiplayerInventoryStorage storage;
    if(!text || slot>=(uint32_t)getPlayerCount() || slot>=4 || which>1 || !g_playerPointers[slot])return 0;
    const struct player *p=g_playerPointers[slot];
    int32_t token=which?p->ptr_text_second_mp_award:p->ptr_text_first_mp_award;
    if(!token){*text=NULL;return 1;}
    if(award_text[slot].player!=p || award_text[slot].token[which]!=token)return 0;
    if(ge_original_multiplayer_inventory_storage(slot,&storage)
            && storage.lifetime!=award_text[slot].lifetime)return 0;
    *text=award_text[slot].text[which];return *text!=NULL;
}
GeOriginalMultiplayerWatchStatus ge_original_multiplayer_watch_awards(int delay)
{
    GeOriginalMultiplayerWatchStatus status=validate(1);
    if(status!=GE_ORIGINAL_MP_WATCH_OK)return status;
    call_status=GE_ORIGINAL_MP_WATCH_OK;
    mpCalculateAwards(delay!=0);
    return call_status;
}
void ge_original_mp_watch_quit_exact(bool delay)
{
    GeOriginalMultiplayerSessionState state;
    if(delay || ge_original_multiplayer_session_abort(&state)!=GE_ORIGINAL_MP_SESSION_OK)
        call_status=GE_ORIGINAL_MP_WATCH_RESOURCE_FAILED;
}
GeOriginalMultiplayerWatchStatus ge_original_multiplayer_watch_tick(void)
{
    GeOriginalMultiplayerWatchStatus status=validate(0);
    if(status!=GE_ORIGINAL_MP_WATCH_OK)return status;
    call_status=GE_ORIGINAL_MP_WATCH_OK;mpwatchMenuTick();return call_status;
}
int ge_original_multiplayer_watch_snapshot(GeOriginalMultiplayerWatchState *state)
{
    if(!state || validate(2)!=GE_ORIGINAL_MP_WATCH_OK)return 0;
    state->who_paused=who_paused;state->chevron_glow=chevron_glow;state->chevron_timer=chevron_glow_timer;
    state->alternate_message=alt_gameover_msg;state->alternate_timer=alt_gameover_msg_timer;
    state->gamemode=gamemode;state->selected_count=selected_num_players;
    for(unsigned i=0;i<4;++i)for(unsigned hand=0;hand<2;++hand)
        state->favorite_weapon[i][hand]=array_favweapon[i][hand];
    return 1;
}
