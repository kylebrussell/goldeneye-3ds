#include "ge_original_multiplayer_setup.h"
#include "ge_original_multiplayer_players.h"
#include "ge_original_bond_input_provider.h"
#include "ge_original_input.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/front.h"
#include "game/mp_weapon.h"
#include "game/chrObjRandom.h"
#include <random.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

s32 g_musicXTrack1Fade,j_text_trigger;
extern s32 selected_num_players,player_char[4],scenario;
extern struct MP_handicap_menu MP_handicap_table[];
extern void init_player_BONDdata(void);
/* Real resource instance storage, with observable switch/texture/audio edges.
 * Gameplay/menu decisions are the retained source functions. */
static struct Model wallet;
static uint32_t switches[128],textures,audio_events,loads;
static int resource_fail,cradle_complete;
static unsigned seats,local_slot;
static GeOriginalInputPad pads[4][2];
static GeOriginalMultiplayerSetupSnapshot state;
static GeOriginalMultiplayerSetupLaunch launch;
static int wallet_load(void *context,void **model){assert(context==&wallet);++loads;*model=&wallet;return !resource_fail;}
static int wallet_clear(void *context,const void *model){assert(context==&wallet&&model==&wallet);memset(switches,0,sizeof(switches));return !resource_fail;}
static int wallet_switch(void *context,const void *model,int32_t index,int32_t value)
{assert(context==&wallet&&model==&wallet&&index>=0&&index<128);switches[index]=(uint32_t)value;return !resource_fail;}
static int texture_load(void *context,uint32_t texture){assert(context==&wallet&&texture<3000);++textures;return !resource_fail;}
static int stage_status(void *context,int32_t folder,int32_t mission,int32_t difficulty)
{assert(context==&wallet&&folder==2&&difficulty==DIFFICULTY_AGENT);return mission==SP_LEVEL_CRADLE&&cradle_complete?STAGESTATUS_COMPLETED:STAGESTATUS_LOCKED;}
static int audio_event(void *context,GeOriginalMultiplayerSetupAudio kind,int32_t value)
{assert(context==&wallet&&kind>=0&&kind<=GE_ORIGINAL_MP_SETUP_SFX_PLAY&&value>=0);++audio_events;return !resource_fail;}
static const GeOriginalMultiplayerSetupServices services={&wallet,wallet_load,wallet_clear,wallet_switch,texture_load,stage_status,audio_event};
static void snapshot(void){assert(ge_original_multiplayer_setup_snapshot(&state)==GE_ORIGINAL_MP_SETUP_OK);}
static GeOriginalMultiplayerSetupStatus frame(void)
{
    assert(ge_original_input_tick_pad_pairs(pads,seats));
    GeOriginalMultiplayerSetupStatus status=ge_original_multiplayer_setup_tick(1);
    assert(status==GE_ORIGINAL_MP_SETUP_OK||status==GE_ORIGINAL_MP_SETUP_LAUNCH_PENDING||status==GE_ORIGINAL_MP_SETUP_RETURN_TO_MODE);
    snapshot();return status;
}
static void neutral(void){memset(pads,0,sizeof(pads));(void)frame();}
static void press(unsigned slot,uint16_t button){neutral();pads[slot][0].buttons=button;(void)frame();memset(pads,0,sizeof(pads));}
static void confirm_all(void){neutral();for(unsigned i=0;i<seats;++i)pads[i][0].buttons=A_BUTTON;(void)frame();memset(pads,0,sizeof(pads));}
static void move_cursor(float x,float y)
{
    neutral();
    for(unsigned step=0;step<100;++step) {
        float dx=x-state.cursor[0],dy=y-state.cursor[1];
        if(fabsf(dx)<=3&&fabsf(dy)<=3)return;
        pads[0][0].stick_x=dx>3?80:dx<-3?-80:0;
        pads[0][0].stick_y=dy>3?-80:dy<-3?80:0;
        (void)frame();
    }
    assert(!"native cursor failed to reach target");
}
static void option(float y,int menu)
{
    neutral();assert(state.menu==MENU_MP_OPTIONS);
    move_cursor(200,y);press(0,A_BUTTON);assert(state.next_menu==menu);neutral();assert(state.menu==menu);
}
static void settings(void)
{
    option(228,MENU_MP_CHAR_SELECT);assert(textures==256);assert(state.character_count==8);
    for(unsigned i=0;i<seats;++i)assert(state.characters[i]==(int)i&&!state.ready[i]);
    press(0,R_JPAD);assert(state.current_character[0]==1&&state.scroll[0]==12);
    press(1,A_BUTTON);assert(state.ready[1]);
    press(0,A_BUTTON);assert(!state.ready[0]); /* Native duplicate-selection guard. */
    press(0,L_JPAD);for(int i=0;i<8;++i)neutral();assert(state.current_character[0]==0&&state.scroll[0]==0);
    confirm_all();assert(state.size[0]==1&&state.ready[0]);
    for(int n=0;n<9;++n){neutral();assert(state.next_menu==MENU_INVALID);}
    neutral();assert(state.next_menu==MENU_MP_OPTIONS);neutral();assert(state.menu==MENU_MP_OPTIONS);

    option(248,MENU_MP_HANDICAP);press(0,L_JPAD);assert(state.handicaps[0]==4);
    press(seats-1,R_JPAD);assert(state.handicaps[seats-1]==6);
    confirm_all();assert(state.next_menu==MENU_MP_OPTIONS);neutral();
    option(268,MENU_MP_CONTROL_STYLE);
    press(0,R_JPAD);press(1,R_JPAD);press(1,R_JPAD);assert(state.control_styles[0]==1&&state.control_styles[1]==2);
    confirm_all();neutral();
    move_cursor(200,290);press(0,A_BUTTON);assert(state.aim_setting==0);press(0,A_BUTTON);assert(state.aim_setting==1);
    assert(g_playerPlayerData[0].sight==1&&g_playerPlayerData[0].autoaim==0);
    move_cursor(200,190);press(0,A_BUTTON);assert(state.length==3);
    move_cursor(200,210);press(0,A_BUTTON);assert(state.weapon_set==12);
    option(168,MENU_MP_STAGE_SELECT);move_cursor(335,270);press(0,A_BUTTON);
    assert(state.stage_choice==6); /* All progression/lower-count arenas fail back to Stack. */
    neutral();option(168,MENU_MP_STAGE_SELECT);move_cursor(335,125);press(0,A_BUTTON);assert(state.stage_choice==3);neutral();
    option(148,MENU_MP_SCENARIO_SELECT);
    move_cursor(200,seats==4?250:230);press(0,A_BUTTON);
    if(seats==4) {
        assert(state.scenario==SCENARIO_2v2&&state.next_menu==MENU_MP_TEAMS);neutral();
        press(0,L_JPAD);assert(state.team_choice==2);press(0,A_BUTTON);
        assert(state.teams[0]==0&&state.teams[2]==0&&state.teams[1]==1&&state.teams[3]==1);
    } else {
        assert(state.scenario==SCENARIO_LTK&&state.weapon_set==1&&!state.enabled[4]);
    }
    neutral();assert(state.menu==MENU_MP_OPTIONS);
}
static void start_and_apply(void)
{
    option(168,MENU_MP_STAGE_SELECT);move_cursor(75,125);press(0,A_BUTTON);assert(state.stage_choice==0);neutral();
    uint64_t before=g_randomSeed;
    press(0,START_BUTTON);assert(state.reload_menu==MENU_RUN_STAGE&&g_randomSeed!=before);
    uint64_t after=g_randomSeed;assert(ge_original_multiplayer_setup_tick(1)==GE_ORIGINAL_MP_SETUP_LAUNCH_PENDING&&g_randomSeed==after);
    assert(ge_original_multiplayer_setup_take_launch(0x1020304050607080ULL,&launch)==GE_ORIGINAL_MP_SETUP_OK);
    assert(ge_original_multiplayer_setup_take_launch(2,&launch)==GE_ORIGINAL_MP_SETUP_STALE&&g_randomSeed==after);
    ge_original_multiplayer_setup_end();
    assert(ge_original_multiplayer_setup_snapshot(&state)==GE_ORIGINAL_MP_SETUP_UNBOUND);
    assert(ge_original_multiplayer_players_create(seats,local_slot,0x1234567890abcdefULL,0xabcdeffedcba9876ULL));
    assert(ge_original_multiplayer_configure(&launch.start.selection.config)==GE_ORIGINAL_MP_OK);
    struct player *selected=g_CurrentPlayer;uint64_t seed=g_randomSeed,chr_seed=g_chrObjRandomSeed;
    g_playerPlayerData[0].kill_counts[1]=17;
    assert(ge_original_multiplayer_setup_apply_launch(&launch)==GE_ORIGINAL_MP_SETUP_OK);
    assert(g_CurrentPlayer==selected&&seed==g_randomSeed&&chr_seed==g_chrObjRandomSeed&&g_playerPlayerData[0].kill_counts[1]==17);
    for(unsigned i=0;i<4;++i) {
        assert(g_playerPlayerData[i].handicap==(seats==2?200.0f:MP_handicap_table[launch.handicap_indices[i]].damage_modifier));
        assert(g_playerPlayerData[i].sight==1&&g_playerPlayerData[i].autoaim==0&&player_char[i]==launch.characters[i]);
        if(i<seats) {
            assert(ge_original_multiplayer_select_player(i));
            const GeOriginalBondInputProvider *p=ge_original_bond_input_provider();
            assert(p->control_type==launch.control_styles[i]);
            assert(g_CurrentPlayer->cur_player_control_type_0==launch.control_styles[i]);
            assert(g_CurrentPlayer->cur_player_control_type_1==launch.control_styles[i]);
            assert(g_CurrentPlayer->cur_player_control_type_2==(float)launch.control_styles[i]);
            assert(g_CurrentPlayer->neg_vspacing_for_control_type_entry==-10*launch.control_styles[i]);
            init_player_BONDdata();assert(ge_original_multiplayer_control_style((int32_t)i)==launch.control_styles[i]);
            assert(p->control_type==launch.control_styles[i]&&g_CurrentPlayer->cur_player_control_type_0==launch.control_styles[i]);
        }
    }
    GeOriginalMultiplayerSetupLaunch bad=launch;bad.control_styles[0]=8;
    struct player copy=*g_CurrentPlayer;struct player_data permanent[4];memcpy(permanent,g_playerPlayerData,sizeof(permanent));
    assert(ge_original_multiplayer_setup_apply_launch(&bad)==GE_ORIGINAL_MP_SETUP_INVALID);
    assert(!memcmp(&copy,g_CurrentPlayer,sizeof(copy))&&!memcmp(permanent,g_playerPlayerData,sizeof(permanent)));
    ge_original_multiplayer_players_destroy();
    assert(!ge_original_multiplayer_players_apply_control_styles(launch.control_styles,seats));
}
int main(int argc,char **argv)
{
    seats=argc>1?(unsigned)atoi(argv[1]):2;local_slot=argc>2?(unsigned)atoi(argv[2])%seats:0;
    assert(seats==2||seats==4);assert(ge_original_input_configure_players(seats));
    g_randomSeed=0x82341234aabcdeffULL;
    assert(ge_original_multiplayer_setup_begin(&services,2,220,165,seats,0)==GE_ORIGINAL_MP_SETUP_OK);
    neutral();assert(state.players==(int)seats&&state.menu==MENU_MP_OPTIONS&&loads==1&&audio_events==3);
    settings();start_and_apply();
    if(argc>3){FILE *f=fopen(argv[3],"wb");assert(f);fprintf(f,"%u %d %d %d %llx\n",seats,
        launch.start.selection.config.level_id,launch.start.selection.config.scenario,launch.start.selection.config.weapon_set,
        (unsigned long long)g_randomSeed);for(unsigned i=0;i<4;++i)fprintf(f,"%d %d %d %u\n",launch.characters[i],launch.handicap_indices[i],launch.control_styles[i],launch.teams[i]);assert(!fclose(f));}
    assert(ge_original_input_configure_players(seats));
    assert(ge_original_multiplayer_setup_begin(&services,2,220,165,seats,1)==GE_ORIGINAL_MP_SETUP_OK);neutral();
    assert(state.characters[0]==launch.characters[0]&&state.handicaps[0]==launch.handicap_indices[0]);
    press(0,B_BUTTON);assert(state.next_menu==MENU_MODE_SELECT);ge_original_multiplayer_setup_end();
    resource_fail=1;assert(ge_original_multiplayer_setup_begin(&services,2,220,165,seats,0)==GE_ORIGINAL_MP_SETUP_OK);
    memset(pads,0,sizeof(pads));assert(ge_original_input_tick_pad_pairs(pads,seats));
    assert(ge_original_multiplayer_setup_tick(1)==GE_ORIGINAL_MP_SETUP_RESOURCE_FAILED);
    assert(ge_original_multiplayer_setup_take_launch(9,&launch)==GE_ORIGINAL_MP_SETUP_RESOURCE_FAILED);ge_original_multiplayer_setup_end();
    printf("native setup %up: all seven source pages, raw-pad readiness/cursor, RNG Start, native all4 permissions and per-slot controls/respawn passed\n",seats);
    return 0;
}
