#define main ge_mp_rules_fixture_main
#include "test_ge_original_multiplayer_rules.c"
#undef main
#include "ge_original_multiplayer_session.h"
#include "ge_original_level.h"
#include "game/mpmenu.h"
s32 g_pausedFlag;
s32 speedgraphframes;
static s32 mission_state,mission_calls;
extern s32 g_ClockTimer,g_GlobalTimer,chevron_glow_timer,alt_gameover_msg_timer;
void set_missionstate(s32 state) {assert(state==0);mission_state=state;++mission_calls;}
static int32_t paused(void *context) {(void)context;return g_pausedFlag;}

static void start(int count,int mode,int time_limit,int point_limit)
{
    GeOriginalLevelProviders providers={0};
    providers.is_paused=paused;
    ge_original_level_init(&providers);
    setup(count,mode);
    for(int i=0;i<count;++i)players[i].mpmenuon=0;
    set_cur_player(count-1);
    g_stopPlayFlag=g_gameOverFlag=g_pausedFlag=1;
    ge_original_level_set_controls_locked(1);
    assert(ge_original_multiplayer_session_reset(time_limit,point_limit)==GE_ORIGINAL_MP_SESSION_OK);
    assert(!g_stopPlayFlag && !g_gameOverFlag && !g_pausedFlag);
    assert(!lvlGetControlsLockedFlag());
    mission_calls=0;mission_state=6;
}

static GeOriginalMultiplayerSessionState tick(int delta,int elapsed)
{
    GeOriginalMultiplayerSessionState out;
    struct player *selected=g_CurrentPlayer;
    struct player_data *perm=g_playerPerm;
    const uint32_t saved_rng=rng;
    const int selected_num=player_num;
    GeOriginalMultiplayerEndDecision reference;
    struct player_data before[4],expected[4];
    memcpy(before,g_playerPlayerData,sizeof(before));
    assert(ge_original_multiplayer_session_snapshot(&out)==GE_ORIGINAL_MP_SESSION_OK);
    reference=ge_original_multiplayer_end_tick(delta,elapsed,out.time_limit60,out.point_limit);
    memcpy(expected,g_playerPlayerData,sizeof(expected));
    memcpy(g_playerPlayerData,before,sizeof(before));
    assert(ge_original_multiplayer_session_tick(delta,elapsed,&out)==GE_ORIGINAL_MP_SESSION_OK);
    assert(out.decision==reference);
    assert(memcmp(g_playerPlayerData,expected,sizeof(expected))==0);
    assert(g_CurrentPlayer==selected && g_playerPerm==perm && player_num==selected_num && rng==saved_rng);
    return out;
}

static void point_round(int count)
{
    GeOriginalMultiplayerSessionState out;
    start(count,SCENARIO_NORMAL,0,1);
    /* Actual original damage -> kill accounting -> death, not a fake score. */
    set_cur_player(1);record_damage_kills(2.0f,1.0f,0.0f,0,1);
    assert(players[1].bonddead && g_playerPlayerData[0].kill_counts[1]==1);
    out=tick(1,10);
    assert(out.decision==GE_ORIGINAL_MP_END_FREEZE && out.stop_play);
    assert(out.freeze_causes==GE_ORIGINAL_MP_SESSION_CAUSE_POINT && !out.results_causes);
    assert(!out.controls_locked && !out.game_over && !out.paused && !mission_calls);
    assert(!disablePlayerActionsWhenPausedOrInMpMenu());
    ge_original_level_timer_tick(1);assert(g_ClockTimer==1 && g_GlobalTimer==1);
    players[1].redbloodfinished=1;
    out=tick(1,11);assert(out.decision==GE_ORIGINAL_MP_END_FREEZE);
    players[1].deathanimfinished=1;
    players[1].colourfadetimemax60=0;
    out=tick(1,12);assert(out.decision==GE_ORIGINAL_MP_END_FREEZE);
    finish_death(1);
    out=tick(1,13);
    assert(out.decision==GE_ORIGINAL_MP_END_RESULTS && out.game_over==1);
    assert(out.controls_locked && out.paused && out.stop_play);
    assert(out.end_reason==GE_ORIGINAL_MP_FRONTEND_END_POINT_LIMIT && mission_calls==1 && mission_state==0);
    assert(alt_gameover_msg_timer==20 && chevron_glow_timer==0);
    for(int i=0;i<count;++i) {
        assert(players[i].mpmenuon && players[i].mpmenumode==MENU_SCORES);
        assert(!players[i].ptr_text_first_mp_award && !players[i].ptr_text_second_mp_award);
    }
    /* Lock takes effect on the NEXT canonical timer boundary, not midway
     * through the already-produced frame clock. No invented five-second wait. */
    assert(g_ClockTimer==1);ge_original_level_timer_tick(1);assert(g_ClockTimer==0 && g_GlobalTimer==1);
    assert(ge_original_multiplayer_session_tick(0,14,&out)==GE_ORIGINAL_MP_SESSION_OK);
    assert(out.game_over==1 && mission_calls==1);
}

static void limits(void)
{
    GeOriginalMultiplayerSessionState out;
    start(2,SCENARIO_NORMAL,60,0);
    players[1].bonddead=1; /* Time limits do not wait for the death gates. */
    out=tick(1,58);assert(out.decision==GE_ORIGINAL_MP_END_RUNNING);
    out=tick(1,59);assert(out.game_over==1 && !out.stop_play);
    assert(out.end_reason==GE_ORIGINAL_MP_FRONTEND_END_TIME_LIMIT);
    start(4,SCENARIO_NORMAL,60,1);
    g_playerPlayerData[0].kill_counts[1]=1;
    out=tick(1,59);assert(out.results_causes==(GE_ORIGINAL_MP_SESSION_CAUSE_TIME|GE_ORIGINAL_MP_SESSION_CAUSE_POINT));
    assert(out.end_reason==GE_ORIGINAL_MP_FRONTEND_END_TIME_LIMIT && mission_calls==1);
    start(4,SCENARIO_YOLT,0,0);
    for(int i=1;i<4;++i){g_playerPlayerData[0].kill_counts[i]=2;players[i].bonddead=1;}
    out=tick(1,20);assert(out.decision==GE_ORIGINAL_MP_END_FREEZE);
    assert(out.freeze_causes==GE_ORIGINAL_MP_SESSION_CAUSE_ELIMINATION);
    for(int i=1;i<4;++i){finish_death(i);assert(g_playerPlayerData[i].order_out_in_yolt==2);}
    out=tick(1,21);assert(out.end_reason==GE_ORIGINAL_MP_FRONTEND_END_ELIMINATION && out.game_over==1);
    start(2,SCENARIO_YOLT,0,0);g_playerPlayerData[0].kill_counts[1]=2;players[1].bonddead=1;finish_death(1);
    out=tick(1,0);assert(out.results_causes==GE_ORIGINAL_MP_SESSION_CAUSE_ELIMINATION);
    start(4,SCENARIO_TLD,0,10);g_playerPlayerData[3].flag_counter=10;
    out=tick(1,0);assert(out.end_reason==GE_ORIGINAL_MP_FRONTEND_END_POINT_LIMIT);
    start(4,SCENARIO_2v2,0,1);g_playerPlayerData[0].kill_counts[2]=1;
    out=tick(1,0);assert(out.end_reason==GE_ORIGINAL_MP_FRONTEND_END_POINT_LIMIT);
    start(4,SCENARIO_MWTGG,0,2);g_playerPlayerData[2].killed_gg_owner_count=1;
    out=tick(1,0);assert(out.end_reason==GE_ORIGINAL_MP_FRONTEND_END_POINT_LIMIT);
    start(2,SCENARIO_NORMAL,0,1);g_playerPlayerData[0].kill_counts[0]=2;
    out=tick(1,0);assert(out.decision==GE_ORIGINAL_MP_END_RUNNING);
    start(2,SCENARIO_LTK,0,1);g_playerPlayerData[0].kill_counts[1]=1;
    out=tick(0,0);assert(out.decision==GE_ORIGINAL_MP_END_RUNNING);
    out=tick(1,0);assert(out.end_reason==GE_ORIGINAL_MP_FRONTEND_END_POINT_LIMIT);
    start(2,SCENARIO_NORMAL,0,0);
    assert(ge_original_multiplayer_session_abort(&out)==GE_ORIGINAL_MP_SESSION_OK);
    assert(out.game_over==1 && !out.stop_play && out.paused && out.controls_locked);
    assert(out.results_causes==GE_ORIGINAL_MP_SESSION_CAUSE_ABORT && out.end_reason==GE_ORIGINAL_MP_FRONTEND_END_ABORT);
    /* Reset preserves canonical player ownership and statistics. Fresh player
     * creation owns mpmenuon=FALSE; the global unpause does not clear it. */
    g_playerPlayerData[0].kill_counts[1]=9;
    assert(ge_original_multiplayer_session_reset(0,0)==GE_ORIGINAL_MP_SESSION_OK);
    assert(g_playerPlayerData[0].kill_counts[1]==9 && players[0].mpmenuon);
}

static void invalid(void)
{
    GeOriginalMultiplayerSessionState out,before;
    start(2,SCENARIO_NORMAL,0,1);memset(&out,0x5a,sizeof(out));before=out;
    assert(ge_original_multiplayer_session_tick(-1,0,&out)==GE_ORIGINAL_MP_SESSION_INVALID_ARGUMENT);
    assert(ge_original_multiplayer_session_tick(1,INT_MAX,&out)==GE_ORIGINAL_MP_SESSION_INVALID_ARGUMENT);
    g_playerPointers[1]=NULL;
    assert(ge_original_multiplayer_session_tick(1,0,&out)==GE_ORIGINAL_MP_SESSION_INVALID_ROSTER);
    g_playerPointers[1]=&players[1];g_CurrentPlayer=&players[0];
    assert(ge_original_multiplayer_session_tick(1,0,&out)==GE_ORIGINAL_MP_SESSION_INVALID_ROSTER);
    set_cur_player(1);players[0].colourfadetimemax60=NAN;
    assert(ge_original_multiplayer_session_tick(1,0,&out)==GE_ORIGINAL_MP_SESSION_INVALID_STATS);
    players[0].colourfadetimemax60=-1;g_playerPlayerData[0].kill_counts[1]=-1;
    assert(ge_original_multiplayer_session_tick(1,0,&out)==GE_ORIGINAL_MP_SESSION_INVALID_STATS);
    assert(!g_stopPlayFlag && !g_gameOverFlag && memcmp(&out,&before,sizeof(out))==0);
}

int main(void)
{
    point_round(2);point_round(4);limits();invalid();
    puts("canonical session: 2/4 damage -> stop-play -> completed death -> immediate results, original clock lock, time/point/YOLT/abort and simultaneous cause order passed");
    return 0;
}
