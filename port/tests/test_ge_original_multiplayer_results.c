#define main ge_mp_rules_fixture_main
#define langGet ge_mp_rules_fixture_lang
#include "test_ge_original_multiplayer_rules.c"
#undef langGet
#undef main
#include "ge_original_multiplayer_results.h"
#include "assets/obseg/text/LmpmenuE.h"
extern s32 ge_original_mp_result_rank_exact(s32);
extern void write_playerrank_to_buffer(char *,s32);
u8 *langGet(s32 id)
{
    if(id==getStringID(LMPMENU,MPMENU_STR_11_RANK1ST))return (u8 *)"1";
    if(id==getStringID(LMPMENU,MPMENU_STR_12_RANK2ND))return (u8 *)"2";
    if(id==getStringID(LMPMENU,MPMENU_STR_13_RANK3RD))return (u8 *)"3";
    if(id==getStringID(LMPMENU,MPMENU_STR_14_RANK4TH))return (u8 *)"4";
    assert(!"ranking requested another label");return NULL;
}
static GeOriginalMultiplayerFrontendResult checked(int expected_mask,
    GeOriginalMultiplayerFrontendEndReason reason)
{
    GeOriginalMultiplayerFrontendResult out;
    struct player_data before[4];
    struct player *selected=g_CurrentPlayer;
    struct player_data *permissions=g_playerPerm;
    const uint32_t saved_rng=rng;
    const int saved_current=player_num;
    memcpy(before,g_playerPlayerData,sizeof(before));
    memset(&out,0xff,sizeof(out));
    assert(ge_original_multiplayer_results_snapshot(UINT64_C(0x12345678abcdef00),reason,&out)==GE_ORIGINAL_MP_RESULTS_OK);
    assert(out.request_id==UINT64_C(0x12345678abcdef00) && out.player_count==num_players);
    assert(out.end_reason==reason && out.aborted==(reason==GE_ORIGINAL_MP_FRONTEND_END_ABORT));
    assert(out.winner_mask==expected_mask);
    for(int i=0;i<num_players;++i){
        char reference[32];int score,kills=0,deaths=0;
        assert(ge_original_multiplayer_score(i,&score)==GE_ORIGINAL_MP_OK && out.scores[i]==score);
        write_playerrank_to_buffer(reference,i);
        assert(ge_original_mp_result_rank_exact(i)==reference[0]-'0');
        assert(!!(out.winner_mask&(1u<<i))==(!out.aborted && reference[0]=='1'));
        for(int j=0;j<num_players;++j){if(i!=j)kills+=before[i].kill_counts[j];deaths+=before[j].kill_counts[i];}
        assert(out.kills[i]==kills && out.deaths[i]==deaths);
    }
    for(int i=num_players;i<4;++i)assert(out.scores[i]==0 && out.kills[i]==0 && out.deaths[i]==0);
    assert(memcmp(before,g_playerPlayerData,sizeof(before))==0);
    assert(rng==saved_rng && player_num==saved_current && g_CurrentPlayer==selected && g_playerPerm==permissions);
    return out;
}
static void invalid(void)
{
    GeOriginalMultiplayerFrontendResult out,old;
    setup(2,SCENARIO_NORMAL);memset(&out,0x5a,sizeof(out));old=out;
    assert(ge_original_multiplayer_results_snapshot(0,GE_ORIGINAL_MP_FRONTEND_END_ABORT,&out)==GE_ORIGINAL_MP_RESULTS_INVALID_ARGUMENT);
    assert(ge_original_multiplayer_results_snapshot(1,GE_ORIGINAL_MP_FRONTEND_END_NONE,&out)==GE_ORIGINAL_MP_RESULTS_INVALID_ARGUMENT);
    assert(ge_original_multiplayer_results_snapshot(1,GE_ORIGINAL_MP_FRONTEND_END_ELIMINATION,&out)==GE_ORIGINAL_MP_RESULTS_INVALID_ARGUMENT);
    g_playerPointers[1]=NULL;
    assert(ge_original_multiplayer_results_snapshot(1,GE_ORIGINAL_MP_FRONTEND_END_ABORT,&out)==GE_ORIGINAL_MP_RESULTS_INVALID_ROSTER);
    g_playerPointers[1]=g_playerPointers[0];
    assert(ge_original_multiplayer_results_snapshot(1,GE_ORIGINAL_MP_FRONTEND_END_ABORT,&out)==GE_ORIGINAL_MP_RESULTS_INVALID_ROSTER);
    g_playerPointers[1]=&players[1];g_playerPlayerData[0].kill_counts[1]=-1;
    assert(ge_original_multiplayer_results_snapshot(1,GE_ORIGINAL_MP_FRONTEND_END_ABORT,&out)==GE_ORIGINAL_MP_RESULTS_INVALID_STATS);
    g_playerPlayerData[0].kill_counts[1]=INT_MAX;g_playerPlayerData[1].kill_counts[0]=1;
    assert(ge_original_multiplayer_results_snapshot(1,GE_ORIGINAL_MP_FRONTEND_END_ABORT,&out)==GE_ORIGINAL_MP_RESULTS_INVALID_STATS);
    assert(memcmp(&out,&old,sizeof(out))==0);
    setup(4,SCENARIO_MWTGG);g_playerPlayerData[0].killed_gg_owner_count=INT_MAX;
    assert(ge_original_multiplayer_results_snapshot(1,GE_ORIGINAL_MP_FRONTEND_END_ABORT,&out)==GE_ORIGINAL_MP_RESULTS_INVALID_STATS);
    assert(memcmp(&out,&old,sizeof(out))==0);
}
int main(void)
{
    GeOriginalMultiplayerFrontendResult out;
    setup(2,SCENARIO_NORMAL);checked(3,GE_ORIGINAL_MP_FRONTEND_END_TIME_LIMIT);
    setup(4,SCENARIO_NORMAL);
    g_playerPlayerData[0].kill_counts[1]=4;g_playerPlayerData[0].kill_counts[0]=1;
    g_playerPlayerData[1].kill_counts[0]=2;g_playerPlayerData[2].kill_counts[3]=3;
    g_playerPlayerData[3].kill_counts[3]=5;
    out=checked(5,GE_ORIGINAL_MP_FRONTEND_END_POINT_LIMIT);
    assert(out.scores[0]==3 && out.scores[3]==-5 && out.kills[0]==4 && out.deaths[0]==3);
    checked(0,GE_ORIGINAL_MP_FRONTEND_END_ABORT);
    setup(2,SCENARIO_NORMAL);g_playerPlayerData[0].kill_counts[0]=3;g_playerPlayerData[1].kill_counts[1]=1;
    out=checked(2,GE_ORIGINAL_MP_FRONTEND_END_TIME_LIMIT);assert(out.scores[0]==-3 && out.scores[1]==-1);
    g_playerPlayerData[0].kill_counts[0]=INT_MAX;g_playerPlayerData[1].kill_counts[1]=0;
    out=checked(2,GE_ORIGINAL_MP_FRONTEND_END_TIME_LIMIT);assert(out.scores[0]==-INT_MAX);
    setup(4,SCENARIO_MWTGG);g_playerPlayerData[0].kill_counts[1]=1;g_playerPlayerData[0].killed_gg_owner_count=1;g_playerPlayerData[2].kill_counts[3]=3;
    out=checked(5,GE_ORIGINAL_MP_FRONTEND_END_POINT_LIMIT);assert(out.scores[0]==3 && out.kills[0]==1);
    setup(2,SCENARIO_MWTGG);g_playerPlayerData[0].kill_counts[1]=1;g_playerPlayerData[0].killed_gg_owner_count=1;g_playerPlayerData[1].kill_counts[0]=1;
    out=checked(3,GE_ORIGINAL_MP_FRONTEND_END_POINT_LIMIT);assert(out.scores[0]==1);
    setup(4,SCENARIO_TLD);g_playerPlayerData[0].flag_counter=123;g_playerPlayerData[1].flag_counter=124;g_playerPlayerData[2].flag_counter=124;
    checked(6,GE_ORIGINAL_MP_FRONTEND_END_TIME_LIMIT);
    setup(2,SCENARIO_TLD);g_playerPlayerData[0].flag_counter=77;g_playerPlayerData[1].flag_counter=77;
    checked(3,GE_ORIGINAL_MP_FRONTEND_END_TIME_LIMIT);
    setup(4,SCENARIO_YOLT);g_playerPlayerData[1].order_out_in_yolt=4;g_playerPlayerData[2].order_out_in_yolt=3;g_playerPlayerData[3].order_out_in_yolt=2;
    out=checked(1,GE_ORIGINAL_MP_FRONTEND_END_ELIMINATION);assert(out.scores[0]==4 && out.scores[1]==0);
    for(int i=0;i<4;++i)g_playerPlayerData[i].order_out_in_yolt=2;
    checked(15,GE_ORIGINAL_MP_FRONTEND_END_ELIMINATION);
    setup(2,SCENARIO_YOLT);g_playerPlayerData[1].order_out_in_yolt=2;
    out=checked(1,GE_ORIGINAL_MP_FRONTEND_END_ELIMINATION);assert(out.scores[0]==4 && out.scores[1]==2);
    setup(4,SCENARIO_2v2);g_playerPlayerData[0].kill_counts[2]=4;g_playerPlayerData[1].kill_counts[0]=1;g_playerPlayerData[1].kill_counts[1]=1;g_playerPlayerData[2].kill_counts[0]=2;
    out=checked(15,GE_ORIGINAL_MP_FRONTEND_END_TIME_LIMIT);assert(out.scores[0]==2 && out.kills[1]==1 && out.deaths[1]==1);
    g_playerPlayerData[0].kill_counts[2]=5;checked(3,GE_ORIGINAL_MP_FRONTEND_END_POINT_LIMIT);
    setup(4,SCENARIO_2v2);g_playerPlayerData[0].kill_counts[1]=3;g_playerPlayerData[2].kill_counts[2]=2;
    out=checked(12,GE_ORIGINAL_MP_FRONTEND_END_TIME_LIMIT);assert(out.scores[0]==-3 && out.scores[2]==-2);
    setup(4,SCENARIO_NORMAL);scenario=SCENARIO_3v1;for(int i=0;i<4;++i)g_playerPlayerData[i].have_token_or_goldengun=i==3;
    g_playerPlayerData[3].kill_counts[0]=3;g_playerPlayerData[0].kill_counts[3]=2;checked(8,GE_ORIGINAL_MP_FRONTEND_END_TIME_LIMIT);
    g_playerPlayerData[0].kill_counts[3]=4;checked(7,GE_ORIGINAL_MP_FRONTEND_END_POINT_LIMIT);
    setup(3,SCENARIO_2v1);g_playerPlayerData[0].kill_counts[2]=2;g_playerPlayerData[2].kill_counts[0]=1;checked(3,GE_ORIGINAL_MP_FRONTEND_END_POINT_LIMIT);
    setup(4,SCENARIO_LTK);g_playerPlayerData[3].kill_counts[1]=4;checked(8,GE_ORIGINAL_MP_FRONTEND_END_POINT_LIMIT);
    setup(2,SCENARIO_LTK);g_playerPlayerData[0].kill_counts[1]=1;checked(1,GE_ORIGINAL_MP_FRONTEND_END_POINT_LIMIT);
    invalid();
    puts("canonical results: normal/ties/suicides/2v2/3v1/2v1/GoldenGun/TLD/YOLT/LTK, exact rank labels, abort cause and immutable state passed");
    return 0;
}
