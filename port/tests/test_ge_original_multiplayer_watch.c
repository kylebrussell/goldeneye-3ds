/* Real native roster/inventory/input/menu/awards/music-state fixture. Audio
 * rendering and localized text are explicit resources, not gameplay doubles. */
#define GE_MP_WATCH_FIXTURE
#define main ge_mp_session_fixture_main
#include "test_ge_mp_state_session.c"
#undef main
#include "ge_original_multiplayer_watch.h"
#include "ge_original_gameplay_services.h"
#include "ge_original_input.h"
#include <music.h>
#include <snd.h>
#include "game/mpmenu.h"
#include "game/mp_music.h"
#include "game/frametiming.h"
#include "boss.h"
extern unsigned fixture_music_intro_plays,fixture_music_stops[3];
extern s32 mission_state,who_paused,chevron_glow_timer,alt_gameover_msg_timer;
extern s32 array_favweapon[4][2];
extern PLAYER_ID array_PLAYER_IDs[4];
extern void fixture_reference_awards(bool);
static ALBank bank;
ALBank *g_musicSfxBufferPtr=&bank;
static s32 requested_stage;
static char text_resources[65536][8];
static unsigned active_count;
static GeOriginalInputPad rawpads[4][2];
u8 *langGet(s32 id) {assert(id>=0&&id<65536);snprintf(text_resources[id],8,"%04x",id);return (u8 *)text_resources[id];}
void bossSetLoadedStage(LEVELID stage) {assert(stage==LEVELID_TITLE);requested_stage=stage;}
static void start_watch(unsigned count,unsigned local)
{
    assert(ge_original_multiplayer_players_create(count,local,0x123456789abcdefULL,0x987654321fedcbaULL));
    active_count=count;requested_stage=0;fixture_mission_timer=600;
    GeOriginalMultiplayerConfig config={(int)count,SCENARIO_NORMAL,1,LEVELID_TEMPLE,{0,0,1,1}};
    assert(ge_original_multiplayer_configure(&config)==GE_ORIGINAL_MP_OK);
    assert(ge_original_multiplayer_session_reset(0,0)==GE_ORIGINAL_MP_SESSION_OK);
    for(unsigned i=0;i<4;++i)array_PLAYER_IDs[i]=(PLAYER_ID)(3-i);
    for(unsigned i=0;i<count;++i) {
        struct player *p=ge_original_multiplayer_player(i);
        assert(ge_original_multiplayer_player_bind_inventory(i));
        p->mpmenuon=0;p->colourfadetimemax60=-1;p->lifestarttime60=10;
        p->gunheldarr[0].totaltime=600;p->gunheldarr[0].weapon1=ITEM_WPPK+i;
        p->gunheldarr[0].weapon2=ITEM_UNARMED;
    }
    memset(rawpads,0,sizeof(rawpads));speedgraphframes=1;
    assert(ge_original_input_tick_pad_pairs(rawpads,count));
    sub_GAME_7F0C11FC(LEVELID_TEMPLE);assert(mission_state==MISSION_STATE_1);
}
static void watch_world(void)
{
    assert(ge_original_input_tick_pad_pairs(rawpads,active_count));
    for(unsigned ordinal=0;ordinal<active_count;++ordinal) {
        unsigned slot=get_nth_player_from_shuffled((PLAYER_ID)ordinal);
        assert(ge_original_multiplayer_select_player(slot));
        assert(ge_original_multiplayer_watch_tick()==GE_ORIGINAL_MP_WATCH_OK);
    }
}
static void press(unsigned slot,uint16_t button)
{
    memset(rawpads,0,sizeof(rawpads));watch_world();
    rawpads[slot][0].buttons=button;watch_world();
    memset(rawpads,0,sizeof(rawpads));
}
static void pause_and_death(void)
{
    const unsigned owner=active_count-1,other=0;
    press(owner,START_BUTTON);assert(g_playerPointers[owner]->mpmenuon);
    assert(g_playerPointers[owner]->mpmenumode==MENU_SCORES);
    press(owner,R_JPAD);assert(g_playerPointers[owner]->mpmenumode==MENU_PAUSE);
    press(owner,A_BUTTON);assert(g_pausedFlag&&g_ControlsLockedFlag&&who_paused==(s32)owner);
    press(other,START_BUTTON);press(other,R_JPAD);press(other,A_BUTTON);
    assert(g_pausedFlag&&g_ControlsLockedFlag&&who_paused==(s32)owner);
    press(owner,A_BUTTON);assert(!g_pausedFlag&&!g_ControlsLockedFlag);
    press(owner,A_BUTTON);assert(g_pausedFlag);press(other,B_BUTTON);assert(g_pausedFlag);
    press(owner,B_BUTTON);assert(!g_pausedFlag&&!g_ControlsLockedFlag&&!g_playerPointers[owner]->mpmenuon);
    press(other,START_BUTTON);g_playerPointers[other]->bonddead=1;watch_world();
    assert(!g_playerPointers[other]->mpmenuon&&!g_playerPointers[other]->healthdisplaytime);
    g_playerPointers[other]->bonddead=0;
    g_playerPointers[owner]->kills_this_life=3;watch_world();
    assert(g_playerPlayerData[owner].most_killed_one_life==3);
    assert(g_playerPlayerData[owner].longest_inning==590);
    /* Only first shuffled player owns global timer updates. */
    chevron_glow_timer=7;alt_gameover_msg_timer=9;speedgraphframes=2;watch_world();
    assert(chevron_glow_timer==9&&alt_gameover_msg_timer==11);speedgraphframes=1;
}
static void reference_awards(void)
{
    struct player *copy=malloc(active_count*sizeof(*copy));assert(copy);
    struct player_data permanent[4];GeOriginalMultiplayerWatchState expected;
    const char *expected_text[4][2];
    for(unsigned i=0;i<active_count;++i) {
        g_playerPlayerData[i].shot_count[SHOT_REGISTER_TOTAL]=37*i;
        g_playerPlayerData[i].shot_count[SHOT_REGISTER_HEAD]=i;
        g_playerPlayerData[i].distance_traveled=100000.0f*(i+1);
        g_playerPlayerData[i].body_armor_pickups=(float)(i*3);
        g_playerPlayerData[i].kill_counts[(i+1)%active_count]=i+2;
        if(i==0)g_playerPlayerData[i].kill_counts[i]=2;
        g_playerPlayerData[i].most_killed_one_time=i?2:4;
        copy[i]=*g_playerPointers[i];
    }
    memcpy(permanent,g_playerPlayerData,sizeof(permanent));
    uint64_t seed=g_randomSeed;
    fixture_reference_awards(FALSE);uint64_t expected_seed=g_randomSeed;
    assert(ge_original_multiplayer_watch_snapshot(&expected));
    for(unsigned i=0;i<active_count;++i)for(unsigned a=0;a<2;++a)
        assert(ge_original_multiplayer_watch_award_text(i,a,&expected_text[i][a]));
    assert(expected_text[0][0] && !strcmp(expected_text[0][0],"Quad Kill (Doh!)"));
    for(unsigned i=0;i<active_count;++i)*g_playerPointers[i]=copy[i];
    memcpy(g_playerPlayerData,permanent,sizeof(permanent));g_randomSeed=seed;
    assert(ge_original_multiplayer_session_reset(0,0)==GE_ORIGINAL_MP_SESSION_OK);
    unsigned intros=fixture_music_intro_plays;
    GeOriginalMultiplayerSessionState state;
    assert(ge_original_multiplayer_session_abort(&state)==GE_ORIGINAL_MP_SESSION_OK);
    if(g_randomSeed!=expected_seed||fixture_music_intro_plays!=intros+1)fprintf(stderr,"seeds %llx expected %llx intro %u expected %u\n",(unsigned long long)g_randomSeed,(unsigned long long)expected_seed,fixture_music_intro_plays,intros+1);
    assert(g_randomSeed==expected_seed&&fixture_music_intro_plays==intros+1);
    assert(state.end_reason==GE_ORIGINAL_MP_FRONTEND_END_ABORT);
    GeOriginalMultiplayerWatchState actual;assert(ge_original_multiplayer_watch_snapshot(&actual));
    assert(!memcmp(&expected,&actual,sizeof(expected)));
    for(unsigned i=0;i<active_count;++i)for(unsigned a=0;a<2;++a) {
        const char *text;assert(ge_original_multiplayer_watch_award_text(i,a,&text));assert(text==expected_text[i][a]);
        assert(array_favweapon[i][0]==ITEM_WPPK+(int)i&&array_favweapon[i][1]==ITEM_UNARMED);
    }
    assert(ge_original_multiplayer_session_abort(&state)==GE_ORIGINAL_MP_SESSION_OK);
    assert(g_randomSeed==expected_seed&&fixture_music_intro_plays==intros+1);free(copy);
}
static void delayed_results(void)
{
    assert(ge_original_multiplayer_watch_awards(1)==GE_ORIGINAL_MP_WATCH_OK);
    assert(g_gameOverFlag==300);speedgraphframes=59;
    for(int i=0;i<5;++i){watch_world();assert(g_gameOverFlag==300-(i+1)*59);}
    speedgraphframes=7;watch_world();assert(g_gameOverFlag==1);speedgraphframes=1;
    for(unsigned i=0;i<active_count;++i) {
        press(i,START_BUTTON);assert(g_playerPointers[i]->mpmenumode==MENU_FINISHED);
        assert(requested_stage==(i+1==active_count?LEVELID_TITLE:0));
    }
    press(0,B_BUTTON);assert(g_playerPointers[0]->mpmenumode==MENU_SCORES);
}
static void menu_quit(void)
{
    GeOriginalMultiplayerSessionState state;
    assert(ge_original_multiplayer_session_reset(0,0)==GE_ORIGINAL_MP_SESSION_OK);
    for(unsigned i=0;i<active_count;++i)g_playerPointers[i]->mpmenuon=0;
    press(0,START_BUTTON);press(0,R_JPAD);press(0,R_JPAD);
    assert(g_playerPointers[0]->mpmenumode==MENU_EXIT);
    press(0,A_BUTTON);assert(g_playerPointers[0]->mpmenumode==MENU_EXIT_CONFIRM&&!g_playerPointers[0]->mpquitconfirm);
    press(0,R_JPAD);assert(g_playerPointers[0]->mpquitconfirm==1);
    unsigned intros=fixture_music_intro_plays;uint64_t seed=g_randomSeed;
    ALSoundState *flagged=sndPlaySfx((struct ALBankAlt_s *)&bank,1,NULL);
    ALSoundState *keep=sndPlaySfx((struct ALBankAlt_s *)&bank,2,NULL);
    assert(flagged&&keep&&(flagged->unk3e&1));keep->unk3e=0x10;
    press(0,A_BUTTON);
    assert(ge_original_multiplayer_session_snapshot(&state)==GE_ORIGINAL_MP_SESSION_OK);
    assert(state.results_causes==GE_ORIGINAL_MP_SESSION_CAUSE_ABORT&&state.game_over==1);
    assert(fixture_music_intro_plays==intros+1&&seed!=g_randomSeed&&mission_state==MISSION_STATE_0);
    assert(sndGetPlayingState(flagged)==SOUND_STATE_NONE&&sndGetPlayingState(keep)!=SOUND_STATE_NONE);
    sndDeactivate(keep);
}

static void awards_cast_preflight(void)
{
    struct player *saved=malloc(active_count*sizeof(*saved));assert(saved);
    struct player_data baseline[4],changed[4];memcpy(baseline,g_playerPlayerData,sizeof(baseline));
    GeOriginalMultiplayerWatchState before,after;
    for(unsigned kind=0;kind<4;++kind)for(unsigned slot=0;slot<active_count && slot<3;++slot) {
        memcpy(g_playerPlayerData,baseline,sizeof(baseline));
        if(kind==0)g_playerPlayerData[slot].body_armor_pickups=2147483648.0f;
        else if(kind==1)g_playerPlayerData[slot].distance_traveled=1e20f;
        else {
            memset(g_playerPlayerData[slot].kill_counts,0,sizeof(g_playerPlayerData[slot].kill_counts));
            g_playerPlayerData[slot].kill_counts[(slot+1)%active_count]=21474837;
            g_playerPlayerData[slot].shot_count[SHOT_REGISTER_TOTAL]=kind==2?0:2147483647;
            if(kind==3)for(unsigned j=0;j<active_count;++j)g_playerPlayerData[j].kill_counts[slot]=0;
        }
        for(unsigned i=0;i<active_count;++i)saved[i]=*g_playerPointers[i];
        memcpy(changed,g_playerPlayerData,sizeof(changed));
        assert(ge_original_multiplayer_watch_snapshot(&before));
        GeOriginalGameplayServiceStats audio_before,audio_after;
        ge_original_gameplay_services_snapshot(&audio_before);
        uint64_t rng=g_randomSeed,chr_rng=g_chrObjRandomSeed;
        unsigned intros=fixture_music_intro_plays,stops=fixture_music_stops[0];s32 mission=mission_state;
        assert(ge_original_multiplayer_watch_awards(0)==GE_ORIGINAL_MP_WATCH_INVALID_STATS);
        assert(ge_original_multiplayer_watch_snapshot(&after));ge_original_gameplay_services_snapshot(&audio_after);
        assert(!memcmp(&before,&after,sizeof(before))&&!memcmp(&audio_before,&audio_after,sizeof(audio_before)));
        for(unsigned i=0;i<active_count;++i)assert(!memcmp(&saved[i],g_playerPointers[i],sizeof(saved[i])));
        assert(!memcmp(changed,g_playerPlayerData,sizeof(changed))&&rng==g_randomSeed&&chr_rng==g_chrObjRandomSeed
            &&intros==fixture_music_intro_plays&&stops==fixture_music_stops[0]&&mission==mission_state);
    }
    memcpy(g_playerPlayerData,baseline,sizeof(baseline));free(saved);
}

#ifndef GE_MP_WATCH_NO_MAIN
int main(int argc,char **argv)
{
    unsigned count=argc>1?(unsigned)atoi(argv[1]):2,local=argc>2?(unsigned)atoi(argv[2]):0;
    assert(count==2||count==4);start_watch(count,local%count);pause_and_death();awards_cast_preflight();reference_awards();delayed_results();menu_quit();
    GeOriginalMultiplayerWatchState state;assert(ge_original_multiplayer_watch_snapshot(&state));
    if(argc>3) {FILE *f=fopen(argv[3],"wb");assert(f);fprintf(f,"%u %u %u %016llx\n",count,fixture_music_intro_plays,fixture_music_stops[0],(unsigned long long)g_randomSeed);
        for(unsigned i=0;i<count;++i)for(unsigned a=0;a<2;++a){const char *text;assert(ge_original_multiplayer_watch_award_text(i,a,&text));fprintf(f,"%s\n",text?text:"none");}fclose(f);}
    int32_t previous_token=g_playerPointers[0]->ptr_text_first_mp_award;
    GeOriginalMultiplayerWatchState previous=state;
    ge_original_multiplayer_players_destroy();
    /* Prior menu history persists where native source owns it; fresh active
     * player fields/count and actual awards replace previous active results. */
    start_watch(count==2?4:2,0);assert(ge_original_multiplayer_watch_snapshot(&state));
    assert(state.selected_count==(count==2?4:2));
    assert(state.who_paused==previous.who_paused&&state.chevron_glow==previous.chevron_glow
        &&state.chevron_timer==previous.chevron_timer&&state.alternate_message==previous.alternate_message
        &&state.alternate_timer==previous.alternate_timer
        &&!memcmp(state.favorite_weapon,previous.favorite_weapon,sizeof(state.favorite_weapon)));
    assert(previous_token);g_playerPointers[0]->ptr_text_first_mp_award=previous_token;
    const char *stale_text;assert(!ge_original_multiplayer_watch_award_text(0,0,&stale_text));
    g_playerPointers[0]->ptr_text_first_mp_award=0;
    uint64_t unchanged=g_randomSeed;g_playerPlayerData[0].shot_count[SHOT_REGISTER_TOTAL]=-1;
    assert(ge_original_multiplayer_watch_awards(0)==GE_ORIGINAL_MP_WATCH_INVALID_STATS&&g_randomSeed==unchanged);
    g_playerPlayerData[0].shot_count[SHOT_REGISTER_TOTAL]=0;
    s32 old_mission=mission_state;mission_state=99;
    assert(ge_original_multiplayer_watch_awards(0)==GE_ORIGINAL_MP_WATCH_INVALID_STATS&&g_randomSeed==unchanged);
    mission_state=old_mission;
    array_PLAYER_IDs[0]=array_PLAYER_IDs[1];
    assert(ge_original_multiplayer_watch_tick()==GE_ORIGINAL_MP_WATCH_INVALID_ROSTER&&g_randomSeed==unchanged);
    ge_original_multiplayer_players_destroy();
    start_watch(count,local%count);for(unsigned i=0;i<count;++i){const char *text;assert(ge_original_multiplayer_watch_award_text(i,0,&text)&&!text);}
    ge_original_multiplayer_players_destroy();
    printf("native watch %up: raw-pad pause ownership/death menus, full source awards RNG/text/favorites, delayed results, all-player exit, music state/SFX flags and prior-session history passed\n",count);
    return 0;
}

#endif
