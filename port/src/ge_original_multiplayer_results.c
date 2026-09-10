#include "ge_original_multiplayer_results.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/front.h"
#include <limits.h>
#include <string.h>

extern struct mp_stage_playercount mp_player_counts[];
extern s32 ge_original_mp_result_rank_exact(s32 player);
extern void ge_original_mp_result_counts_exact(s32 player, s32 player_count,
                                               s32 *kills, s32 *deaths);

GeOriginalMultiplayerResultsStatus ge_original_multiplayer_results_snapshot(
    uint64_t request_id, GeOriginalMultiplayerFrontendEndReason end_reason,
    GeOriginalMultiplayerFrontendResult *result)
{
    GeOriginalMultiplayerFrontendResult next={0};
    const int count=getPlayerCount(), scenario=get_scenario();
    int64_t total=0;
    int i,j;
    if(!result || !request_id || end_reason<=GE_ORIGINAL_MP_FRONTEND_END_NONE
            || end_reason>GE_ORIGINAL_MP_FRONTEND_END_ELIMINATION
            || (end_reason==GE_ORIGINAL_MP_FRONTEND_END_ELIMINATION
                && scenario!=SCENARIO_YOLT))
        return GE_ORIGINAL_MP_RESULTS_INVALID_ARGUMENT;
    if(count<2 || count>4 || scenario<SCENARIO_NORMAL || scenario>SCENARIO_2v1
            || count<mp_player_counts[scenario].min
            || count>mp_player_counts[scenario].max)
        return GE_ORIGINAL_MP_RESULTS_INVALID_ROSTER;
    for(i=0;i<count;++i){
        if(!g_playerPointers[i]) return GE_ORIGINAL_MP_RESULTS_INVALID_ROSTER;
        for(j=0;j<i;++j)
            if(g_playerPointers[i]==g_playerPointers[j])
                return GE_ORIGINAL_MP_RESULTS_INVALID_ROSTER;
        for(j=0;j<count;++j){
            if(g_playerPlayerData[i].kill_counts[j]<0)
                return GE_ORIGINAL_MP_RESULTS_INVALID_STATS;
            total+=g_playerPlayerData[i].kill_counts[j];
        }
        if(g_playerPlayerData[i].killed_gg_owner_count<0
                || (scenario==SCENARIO_YOLT && g_playerPlayerData[i].order_out_in_yolt>MAX_PLAYER_COUNT)
                || (scenario==SCENARIO_TLD && g_playerPlayerData[i].flag_counter<0))
            return GE_ORIGINAL_MP_RESULTS_INVALID_STATS;
    }
    /* Canonical rankings/metrics use signed 32-bit accumulators. Bounding the
     * entire nonnegative kill matrix also bounds every row, column and team
     * partial sum, so their unchanged bodies cannot overflow. */
    if(total>INT32_MAX) return GE_ORIGINAL_MP_RESULTS_INVALID_STATS;
    if(scenario==SCENARIO_NORMAL || scenario==SCENARIO_MWTGG || scenario==SCENARIO_LTK)
        for(i=0;i<count;++i)
            if(total+(int64_t)g_playerPlayerData[i].killed_gg_owner_count*(count-2)>INT32_MAX)
                return GE_ORIGINAL_MP_RESULTS_INVALID_STATS;
    next.request_id=request_id;next.player_count=count;next.end_reason=end_reason;
    next.aborted=end_reason==GE_ORIGINAL_MP_FRONTEND_END_ABORT;
    for(i=0;i<count;++i){
        s32 kills,deaths,rank;
        if(ge_original_multiplayer_score(i,&next.scores[i])!=GE_ORIGINAL_MP_OK)
            return GE_ORIGINAL_MP_RESULTS_SCORE_FAILED;
        ge_original_mp_result_counts_exact(i,count,&kills,&deaths);
        next.kills[i]=kills;next.deaths[i]=deaths;
        rank=ge_original_mp_result_rank_exact(i);
        if(rank<1 || rank>4) return GE_ORIGINAL_MP_RESULTS_SCORE_FAILED;
        if(!next.aborted && rank==1) next.winner_mask|=(uint8_t)(1U<<i);
    }
    *result=next;
    return GE_ORIGINAL_MP_RESULTS_OK;
}
