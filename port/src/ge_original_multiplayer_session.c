#include "ge_original_multiplayer_session.h"
#include "ge_original_multiplayer_watch.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/front.h"
#include "game/mpmenu.h"
#include "game/lv.h"
#include <limits.h>
#include <math.h>
#include <string.h>

extern s32 g_stopPlayFlag, g_gameOverFlag, g_pausedFlag, g_MpTime, g_MpPoint;
extern struct mp_stage_playercount mp_player_counts[];
void ge_original_mp_session_decide_exact(s32,s32,s32,s32,uint32_t *,uint32_t *);

static struct player *session_players[4];
static int session_count;
static GeOriginalMultiplayerSessionState session;

static GeOriginalMultiplayerSessionStatus validate(int require_session)
{
    int i,j,count=getPlayerCount(),mode=get_scenario(),current=get_cur_playernum();
    int64_t kills=0;
    if (require_session && !session_count) return GE_ORIGINAL_MP_SESSION_NOT_INITIALIZED;
    if (count<2 || count>4 || current<0 || current>=count
            || mode<0 || mode>SCENARIO_2v1
            || count<mp_player_counts[mode].min || count>mp_player_counts[mode].max
            || (require_session && count!=session_count))
        return GE_ORIGINAL_MP_SESSION_INVALID_ROSTER;
    for(i=0;i<count;++i) {
        const struct player *p=g_playerPointers[i];
        if(!p || (require_session && p!=session_players[i]))
            return GE_ORIGINAL_MP_SESSION_INVALID_ROSTER;
        for(j=0;j<i;++j) if(p==g_playerPointers[j])
            return GE_ORIGINAL_MP_SESSION_INVALID_ROSTER;
        if(!isfinite(p->colourfadetimemax60)) return GE_ORIGINAL_MP_SESSION_INVALID_STATS;
        for(j=0;j<count;++j) {
            if(g_playerPlayerData[i].kill_counts[j]<0)
                return GE_ORIGINAL_MP_SESSION_INVALID_STATS;
            kills+=g_playerPlayerData[i].kill_counts[j];
        }
        if(g_playerPlayerData[i].killed_gg_owner_count<0
                || (mode==SCENARIO_YOLT && g_playerPlayerData[i].order_out_in_yolt>4)
                || (mode==SCENARIO_TLD && g_playerPlayerData[i].flag_counter<0))
            return GE_ORIGINAL_MP_SESSION_INVALID_STATS;
    }
    if(g_CurrentPlayer!=g_playerPointers[current]
            || g_playerPerm!=&g_playerPlayerData[current])
        return GE_ORIGINAL_MP_SESSION_INVALID_ROSTER;
    /* Canonical point calculations use signed 32-bit intermediates. */
    if(kills>INT32_MAX) return GE_ORIGINAL_MP_SESSION_INVALID_STATS;
    if(mode==SCENARIO_NORMAL || mode==SCENARIO_MWTGG || mode==SCENARIO_LTK) for(i=0;i<count;++i)
        if(kills+(int64_t)g_playerPlayerData[i].killed_gg_owner_count*(count-2)>INT32_MAX)
            return GE_ORIGINAL_MP_SESSION_INVALID_STATS;
    return GE_ORIGINAL_MP_SESSION_OK;
}

GeOriginalMultiplayerSessionStatus ge_original_multiplayer_session_reset(
    int32_t time_limit60,int32_t point_limit)
{
    GeOriginalMultiplayerSessionStatus status;
    int i;
    if(time_limit60<0 || point_limit<0) return GE_ORIGINAL_MP_SESSION_INVALID_ARGUMENT;
    status=validate(0);
    if(status!=GE_ORIGINAL_MP_SESSION_OK) return status;
    if(ge_original_multiplayer_watch_bind()!=GE_ORIGINAL_MP_WATCH_OK)
        return GE_ORIGINAL_MP_SESSION_INVALID_ROSTER;
    lvlSetControlsLockedFlag(0);
    mpwatchUnpauseGame();
    lvlSetMpTime(time_limit60);
    lvlSetMpPoint(point_limit);
    memset(&session,0,sizeof(session));
    session_count=getPlayerCount();
    for(i=0;i<session_count;++i)session_players[i]=g_playerPointers[i];
    return GE_ORIGINAL_MP_SESSION_OK;
}

GeOriginalMultiplayerSessionStatus ge_original_multiplayer_session_snapshot(
    GeOriginalMultiplayerSessionState *state)
{
    if(!state) return GE_ORIGINAL_MP_SESSION_INVALID_ARGUMENT;
    if(!session_count) return GE_ORIGINAL_MP_SESSION_NOT_INITIALIZED;
    *state=session;
    state->time_limit60=g_MpTime;
    state->point_limit=g_MpPoint;
    state->stop_play=g_stopPlayFlag;
    state->game_over=g_gameOverFlag;
    state->paused=g_pausedFlag;
    state->controls_locked=lvlGetControlsLockedFlag();
    return GE_ORIGINAL_MP_SESSION_OK;
}

GeOriginalMultiplayerSessionStatus ge_original_multiplayer_session_tick(
    int32_t tick60,int32_t elapsed60,GeOriginalMultiplayerSessionState *state)
{
    GeOriginalMultiplayerSessionStatus status;
    uint32_t results,freeze;
    if(!state || tick60<0 || elapsed60<0 || tick60>INT32_MAX-elapsed60)
        return GE_ORIGINAL_MP_SESSION_INVALID_ARGUMENT;
    status=validate(1);
    if(status!=GE_ORIGINAL_MP_SESSION_OK)return status;
    if(session.decision==GE_ORIGINAL_MP_END_RESULTS)
        return ge_original_multiplayer_session_snapshot(state);
    ge_original_mp_session_decide_exact(tick60,elapsed60,g_MpTime,g_MpPoint,&results,&freeze);
    session.freeze_causes|=freeze;
    if(freeze)mpwatchSetStopPlayFlag();
    if(results) {
        session.results_causes=results;
        session.end_reason=(results&GE_ORIGINAL_MP_SESSION_CAUSE_TIME)
            ?GE_ORIGINAL_MP_FRONTEND_END_TIME_LIMIT
            :(results&GE_ORIGINAL_MP_SESSION_CAUSE_POINT)
                ?GE_ORIGINAL_MP_FRONTEND_END_POINT_LIMIT
                :GE_ORIGINAL_MP_FRONTEND_END_ELIMINATION;
        session.decision=GE_ORIGINAL_MP_END_RESULTS;
        /* lv.c may execute several terminal calls in this same frame.
         * Retain each full awards/RNG/resource transition in source order. */
        for(uint32_t cause=GE_ORIGINAL_MP_SESSION_CAUSE_TIME;
                cause<=GE_ORIGINAL_MP_SESSION_CAUSE_ELIMINATION;cause<<=1)
            if((results&cause) && ge_original_multiplayer_watch_awards(0)!=GE_ORIGINAL_MP_WATCH_OK) {
                ge_original_multiplayer_session_snapshot(state);
                return GE_ORIGINAL_MP_SESSION_RESOURCE_FAILED;
            }
    } else session.decision=g_stopPlayFlag?GE_ORIGINAL_MP_END_FREEZE:GE_ORIGINAL_MP_END_RUNNING;
    return ge_original_multiplayer_session_snapshot(state);
}

GeOriginalMultiplayerSessionStatus ge_original_multiplayer_session_native_view(
    GeOriginalMultiplayerSessionNativeView *view)
{
    GeOriginalMultiplayerSessionStatus status;
    GeOriginalMultiplayerSessionNativeView next = {0};
    if (!view) return GE_ORIGINAL_MP_SESSION_INVALID_ARGUMENT;
    status = validate(1);
    if (status != GE_ORIGINAL_MP_SESSION_OK) return status;
    status = ge_original_multiplayer_session_snapshot(&next.state);
    if (status != GE_ORIGINAL_MP_SESSION_OK) return status;
    next.count = (uint32_t)session_count;
    for (unsigned i = 0; i < next.count; ++i) next.players[i] = session_players[i];
    *view = next;
    return GE_ORIGINAL_MP_SESSION_OK;
}

GeOriginalMultiplayerSessionStatus ge_original_multiplayer_session_abort(
    GeOriginalMultiplayerSessionState *state)
{
    GeOriginalMultiplayerSessionStatus status;
    if(!state)return GE_ORIGINAL_MP_SESSION_INVALID_ARGUMENT;
    status=validate(1);
    if(status!=GE_ORIGINAL_MP_SESSION_OK)return status;
    if(session.decision!=GE_ORIGINAL_MP_END_RESULTS) {
        session.decision=GE_ORIGINAL_MP_END_RESULTS;
        session.results_causes=GE_ORIGINAL_MP_SESSION_CAUSE_ABORT;
        session.end_reason=GE_ORIGINAL_MP_FRONTEND_END_ABORT;
        if(ge_original_multiplayer_watch_awards(0)!=GE_ORIGINAL_MP_WATCH_OK) {
            ge_original_multiplayer_session_snapshot(state);
            return GE_ORIGINAL_MP_SESSION_RESOURCE_FAILED;
        }
    }
    return ge_original_multiplayer_session_snapshot(state);
}
