#include "ge_original_multiplayer_setup.h"
#include "ge_original_multiplayer_players.h"
#include "ge_original_bond_input_provider.h"
#include "ge_original_player_body.h"
#include "ge_original_input.h"
#include "ge_stage_assets.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/front.h"
#include "game/player.h"
#include "game/mp_weapon.h"
#include <math.h>
#include <string.h>

extern s32 selected_num_players,player_char[4],player_handicap[4],controlstyle_player[4];
extern s32 selected_folder_num,selected_stage,briefingpage,MP_stage_selected,game_length,aim_sight_adjustment;
extern s32 num_chars_selectable_mp,maybe_is_in_menu,scenario,current_mp_stage_highlighted,dword_CODE_bss_80069780,teamsize;
extern s32 highlight_players,highlight_scenario,highlight_gameselect,highlight_gamelength,highlight_character;
extern s32 highlight_weaponselect,highlight_health,highlight_controlstyle,highlight_aimadjustment;
extern s32 unlock_stage_select,unlock_game_length,unlock_chars,unlock_weapon_select,unlock_handicap,unlock_control_style,unlock_aim_sight;
extern s32 tab_start_selected,tab_next_selected,tab_prev_selected,tab_start_highlight,tab_next_highlight,tab_prev_highlight;
extern s32 mp_char_cur_select_player[4],mp_char_prev_select_player[4],mp_char_select_scroll_offset[4];
extern s32 player_has_selected_char[4],size_mp_select_image_player[4],dword_CODE_bss_80069760[4];
extern u32 D_8002B560;
extern f32 cursor_h_pos,cursor_v_pos;
extern MENU current_menu,menu_update,maybe_prev_menu;
extern GAMEMODE gamemode;
extern struct Model *walletinst[4];
extern struct mp_stage_setup multi_stage_setups[];
extern struct mp_stage_playercount mp_player_counts[];
extern struct MP_handicap_menu MP_handicap_table[];
extern struct MP_sight_aim_settings mp_sight_adjust_table[];
extern struct MP_game_length_settings multi_game_lengths[];
void ge_original_mp_setup_control_type_exact(int type);
void init_menu0E_mpoptions(void);void interface_menu0E_mpoptions(void);
void init_menu0f_mpcharsel(void);void interface_menu0F_mpcharsel(void);
void init_menu10_mphandicap(void);void interface_menu10_mphandicap(void);
void init_menu11_mpcontrol(void);void interface_menu11_mpcontrols(void);
void init_menu12_mpstage(void);void interface_menu12_mpstage(void);
void init_menu13_mpscenariosel(void);void interface_menu13_mpscenario(void);
void init_menu14_mpteamsel(void);void interface_menu14_mpteams(void);
void frontChangeMenu(MENU,s32);

static struct {
    GeOriginalMultiplayerSetupServices services;
    GeOriginalMultiplayerSetupStatus status;
    uint32_t seats;
    float delta,fov_y,aspect,znear,zfar;
    uint8_t active,use_zbuffer,taken;
} setup;
static int setup_page(int menu)
{return menu>=MENU_MP_OPTIONS && menu<=MENU_MP_TEAMS;}
static void resource(int success)
{if(!success)setup.status=GE_ORIGINAL_MP_SETUP_RESOURCE_FAILED;}
void ge_original_mp_setup_wallet_load(void)
{
    void *model=NULL;
    if(setup.status!=GE_ORIGINAL_MP_SETUP_OK)return;
    resource(setup.services.wallet_load(setup.services.context,&model)&&model!=NULL);
    if(setup.status==GE_ORIGINAL_MP_SETUP_OK)walletinst[0]=model;
}
void ge_original_mp_setup_wallet_clear(struct Model *model)
{
    if(setup.status==GE_ORIGINAL_MP_SETUP_OK)
        resource(model && model==walletinst[0] && setup.services.wallet_clear_switches(setup.services.context,model));
}
void ge_original_mp_setup_wallet_switch(struct Model *model,s32 index,s32 value)
{
    if(setup.status==GE_ORIGINAL_MP_SETUP_OK)
        resource(model && model==walletinst[0] && setup.services.wallet_set_switch(setup.services.context,model,index,value));
}
void ge_original_mp_setup_texture_load(u32 texture,s32 unused)
{
    (void)unused;
    if(setup.status==GE_ORIGINAL_MP_SETUP_OK)resource(setup.services.texture_load(setup.services.context,texture));
}
s32 ge_original_mp_setup_stage_status(s32 folder,s32 mission,s32 difficulty)
{
    if(setup.status!=GE_ORIGINAL_MP_SETUP_OK)return STAGESTATUS_LOCKED;
    int value=setup.services.stage_status(setup.services.context,folder,mission,difficulty);
    if(value<STAGESTATUS_LOCKED || value>STAGESTATUS_COMPLETED){resource(0);return STAGESTATUS_LOCKED;}
    return value;
}
void ge_original_mp_setup_audio(GeOriginalMultiplayerSetupAudio event,s32 value)
{
    if(setup.status==GE_ORIGINAL_MP_SETUP_OK)resource(setup.services.audio(setup.services.context,event,value));
}
void ge_original_mp_setup_vi(s32 mode,float value,float second)
{
    if(mode==0)setup.fov_y=value;
    else if(mode==1)setup.aspect=value;
    else if(mode==2){setup.znear=value;setup.zfar=second;}
    else setup.use_zbuffer=value!=0;
}
float ge_original_mp_setup_delta(void){return setup.delta;}
s32 ge_original_mp_setup_seats(void){return (s32)setup.seats;}
static int valid_native(int entering)
{
    if(gamemode!=GAMEMODE_MULTI || ((selected_num_players<2 || selected_num_players>4)
            && !(entering && maybe_prev_menu==MENU_MP_OPTIONS && selected_num_players==0))
        || scenario<SCENARIO_NORMAL || scenario>SCENARIO_2v1
        || MP_stage_selected<MP_STAGE_RANDOM || MP_stage_selected>=MP_STAGE_SELECTED_MAX
        || game_length<0 || game_length>=8 || aim_sight_adjustment<0 || aim_sight_adjustment>=4
        || (num_chars_selectable_mp!=8 && num_chars_selectable_mp!=33 && num_chars_selectable_mp!=64)
        || !isfinite(cursor_h_pos) || !isfinite(cursor_v_pos) || cursor_h_pos<20 || cursor_h_pos>420
        || cursor_v_pos<20 || cursor_v_pos>310 || getMPWeaponSet()<0 || getMPWeaponSet()>=14)return 0;
    for(unsigned i=0;i<4;++i) {
        if(player_char[i]<-1 || player_char[i]>=64 || player_handicap[i]<0 || player_handicap[i]>10
            || controlstyle_player[i]<0 || controlstyle_player[i]>7)return 0;
        if(current_menu==MENU_MP_CHAR_SELECT && (mp_char_cur_select_player[i]<-1 || mp_char_cur_select_player[i]>=64
            || mp_char_prev_select_player[i]<-1 || mp_char_prev_select_player[i]>=64
            || mp_char_select_scroll_offset[i]<0 || mp_char_select_scroll_offset[i]>=84
            || size_mp_select_image_player[i]<0 || size_mp_select_image_player[i]>11))return 0;
    }
    return 1;
}
static void enter(MENU menu)
{
    /* Same non-reload transition order as menu_init; the seven source exit
     * callbacks are empty. Reload/VI transitions remain caller-owned. */
    current_menu=menu;maybe_prev_menu=MENU_INVALID;
    switch(menu) {
    case MENU_MP_OPTIONS:init_menu0E_mpoptions();break;
    case MENU_MP_CHAR_SELECT:init_menu0f_mpcharsel();break;
    case MENU_MP_HANDICAP:init_menu10_mphandicap();break;
    case MENU_MP_CONTROL_STYLE:init_menu11_mpcontrol();break;
    case MENU_MP_STAGE_SELECT:init_menu12_mpstage();break;
    case MENU_MP_SCENARIO_SELECT:init_menu13_mpscenariosel();break;
    case MENU_MP_TEAMS:init_menu14_mpteamsel();break;
    default:setup.status=GE_ORIGINAL_MP_SETUP_INVALID;break;
    }
}
GeOriginalMultiplayerSetupStatus ge_original_multiplayer_setup_begin(
    const GeOriginalMultiplayerSetupServices *services,int32_t folder,float x,float y,
    uint32_t seats,int returning)
{
    if(setup.active || !services || !services->wallet_load || !services->wallet_clear_switches
        || !services->wallet_set_switch || !services->texture_load || !services->stage_status || !services->audio
        || folder<0 || folder>=4 || seats<2 || seats>4 || ge_original_input_configured_player_count()!=seats
        || ge_original_multiplayer_player_count()!=0 || !isfinite(x) || !isfinite(y)
        || x<20 || x>420 || y<20 || y>310 || (returning!=0 && returning!=1))return GE_ORIGINAL_MP_SETUP_INVALID;
    memset(&setup,0,sizeof(setup));setup.services=*services;setup.active=1;setup.seats=seats;
    setup.delta=1.0f;gamemode=GAMEMODE_MULTI;selected_folder_num=folder;
    cursor_h_pos=x;cursor_v_pos=y;if(returning)maybe_is_in_menu=TRUE;
    /* Source mode selection issues a non-reload MP_OPTIONS request. */
    menu_update=MENU_INVALID;frontChangeMenu(MENU_MP_OPTIONS,FALSE);
    return setup.status;
}
GeOriginalMultiplayerSetupStatus ge_original_multiplayer_setup_tick(float delta)
{
    if(!setup.active)return GE_ORIGINAL_MP_SETUP_UNBOUND;
    if(setup.status!=GE_ORIGINAL_MP_SETUP_OK)return setup.status;
    if(menu_update==MENU_RUN_STAGE)return GE_ORIGINAL_MP_SETUP_LAUNCH_PENDING;
    if(maybe_prev_menu==MENU_MODE_SELECT)return GE_ORIGINAL_MP_SETUP_RETURN_TO_MODE;
    if(!isfinite(delta)||delta<0||delta>16||ge_original_input_configured_player_count()!=setup.seats)
        return GE_ORIGINAL_MP_SETUP_INVALID;
    setup.delta=delta;
    /* Initializers also index the native scenario/stage and four-row tables.
     * Validate before their first resource or state mutation. Only the source
     * first-entry selected-count zero sentinel is permitted here. */
    if(!valid_native(1))return GE_ORIGINAL_MP_SETUP_INVALID;
    if(maybe_prev_menu>MENU_INVALID) {
        if(!setup_page(maybe_prev_menu))return GE_ORIGINAL_MP_SETUP_INVALID;
        enter(maybe_prev_menu);
    }
    if(setup.status!=GE_ORIGINAL_MP_SETUP_OK)return setup.status;
    if(!valid_native(0))return GE_ORIGINAL_MP_SETUP_INVALID;
    switch(current_menu) {
    case MENU_MP_OPTIONS:interface_menu0E_mpoptions();break;
    case MENU_MP_CHAR_SELECT:interface_menu0F_mpcharsel();break;
    case MENU_MP_HANDICAP:interface_menu10_mphandicap();break;
    case MENU_MP_CONTROL_STYLE:interface_menu11_mpcontrols();break;
    case MENU_MP_STAGE_SELECT:interface_menu12_mpstage();break;
    case MENU_MP_SCENARIO_SELECT:interface_menu13_mpscenario();break;
    case MENU_MP_TEAMS:interface_menu14_mpteams();break;
    default:return GE_ORIGINAL_MP_SETUP_INVALID;
    }
    if(setup.status!=GE_ORIGINAL_MP_SETUP_OK)return setup.status;
    if(!valid_native(0))return GE_ORIGINAL_MP_SETUP_INVALID;
    if(menu_update==MENU_RUN_STAGE)return GE_ORIGINAL_MP_SETUP_LAUNCH_PENDING;
    return maybe_prev_menu==MENU_MODE_SELECT?GE_ORIGINAL_MP_SETUP_RETURN_TO_MODE:GE_ORIGINAL_MP_SETUP_OK;
}
GeOriginalMultiplayerSetupStatus ge_original_multiplayer_setup_snapshot(GeOriginalMultiplayerSetupSnapshot *out)
{
    if(!out)return GE_ORIGINAL_MP_SETUP_INVALID;
    if(!setup.active)return GE_ORIGINAL_MP_SETUP_UNBOUND;
    if(setup.status!=GE_ORIGINAL_MP_SETUP_OK)return setup.status;
    memset(out,0,sizeof(*out));
    out->menu=current_menu;out->next_menu=maybe_prev_menu;out->reload_menu=menu_update;
    out->players=selected_num_players;out->stage_choice=MP_stage_selected;out->scenario=scenario;out->weapon_set=getMPWeaponSet();
    out->length=game_length;out->aim_setting=aim_sight_adjustment;out->character_count=num_chars_selectable_mp;
    out->stage_highlight=current_mp_stage_highlighted;out->scenario_highlight=dword_CODE_bss_80069780;
    out->team_choice=teamsize;out->team_blink=D_8002B560;
    for(unsigned i=0;i<4;++i) {
        out->characters[i]=player_char[i];out->handicaps[i]=player_handicap[i];out->control_styles[i]=controlstyle_player[i];
        out->teams[i]=g_playerPlayerData[i].have_token_or_goldengun;out->ready[i]=player_has_selected_char[i];
        out->current_character[i]=mp_char_cur_select_player[i];out->previous_character[i]=mp_char_prev_select_player[i];
        out->scroll[i]=mp_char_select_scroll_offset[i];out->size[i]=size_mp_select_image_player[i];
        out->stick_centered[i]=dword_CODE_bss_80069760[i];
    }
    int32_t highlights[]={highlight_players,highlight_scenario,highlight_gameselect,highlight_gamelength,
        highlight_weaponselect,highlight_character,highlight_health,highlight_controlstyle,highlight_aimadjustment};
    int32_t enabled[]={unlock_stage_select,unlock_game_length,unlock_chars,unlock_weapon_select,unlock_handicap,unlock_control_style,unlock_aim_sight};
    int32_t selected[]={tab_start_selected,tab_next_selected,tab_prev_selected};
    int32_t tabs[]={tab_start_highlight,tab_next_highlight,tab_prev_highlight};
    memcpy(out->option_highlights,highlights,sizeof(highlights));memcpy(out->enabled,enabled,sizeof(enabled));
    memcpy(out->tab_selected,selected,sizeof(selected));memcpy(out->tab_highlights,tabs,sizeof(tabs));
    out->cursor[0]=cursor_h_pos;out->cursor[1]=cursor_v_pos;out->fov_y=setup.fov_y;out->aspect=setup.aspect;
    out->znear=setup.znear;out->zfar=setup.zfar;out->use_zbuffer=setup.use_zbuffer;out->launch_taken=setup.taken;
    return GE_ORIGINAL_MP_SETUP_OK;
}
static int launch_valid(const GeOriginalMultiplayerSetupLaunch *launch)
{
    const GeOriginalMultiplayerFrontendStart *start=&launch->start;
    const GeOriginalMultiplayerConfig *config=&start->selection.config;
    if(!start->request_id || start->diagnostic_local!=1 || ge_original_multiplayer_validate(config)!=GE_ORIGINAL_MP_OK
        || launch->aim_setting<0 || launch->aim_setting>=4 || start->selection.limit_index<0 || start->selection.limit_index>=8
        || launch->sight!=mp_sight_adjust_table[launch->aim_setting].sight
        || launch->autoaim!=mp_sight_adjust_table[launch->aim_setting].autoaim
        || start->time_limit60!=multi_game_lengths[start->selection.limit_index].time
        || start->point_limit!=multi_game_lengths[start->selection.limit_index].points)return 0;
    if((config->scenario==SCENARIO_YOLT && start->selection.limit_index!=7)
        || (config->scenario!=SCENARIO_YOLT && start->selection.limit_index>6)
        || (config->scenario==SCENARIO_TLD && start->selection.limit_index>3)
        || (config->scenario==SCENARIO_MWTGG && config->weapon_set!=13))return 0;
    const GeStageAssetDescriptor *asset=ge_stage_asset_multiplayer((size_t)start->selection.arena_index);
    if(!asset || asset->level_id!=config->level_id)return 0;
    for(unsigned i=0;i<4;++i) {
        GeOriginalPlayerBodyCharacter c;
        if(launch->characters[i]<-1 || launch->characters[i]>=64 || launch->handicap_indices[i]<0 || launch->handicap_indices[i]>10
            || launch->control_styles[i]<0 || launch->control_styles[i]>7 || launch->teams[i]>1
            || !ge_original_player_body_character(launch->characters[i]<0?(int32_t)i:launch->characters[i],&c))return 0;
        if(i<(unsigned)config->player_count && (start->selection.character_ids[i]!=launch->characters[i]
            || config->teams[i]!=launch->teams[i] || start->characters[i].body!=c.body_id || start->characters[i].head!=c.head_id
            || start->characters[i].gender!=c.gender || start->characters[i].height!=c.perspective_height))return 0;
    }
    return 1;
}
GeOriginalMultiplayerSetupStatus ge_original_multiplayer_setup_take_launch(uint64_t id,GeOriginalMultiplayerSetupLaunch *out)
{
    if(!out || !id)return GE_ORIGINAL_MP_SETUP_INVALID;
    if(!setup.active)return GE_ORIGINAL_MP_SETUP_UNBOUND;
    if(setup.status!=GE_ORIGINAL_MP_SETUP_OK)return setup.status;
    if(menu_update!=MENU_RUN_STAGE || setup.taken)return GE_ORIGINAL_MP_SETUP_STALE;
    if(!valid_native(0))return GE_ORIGINAL_MP_SETUP_INVALID;
    GeOriginalMultiplayerSetupLaunch next={0};next.start.request_id=id;next.start.diagnostic_local=1;
    next.start.selection.config=(GeOriginalMultiplayerConfig){selected_num_players,scenario,getMPWeaponSet(),selected_stage,{0}};
    next.start.selection.arena_index=-1;
    for(unsigned i=0;i<GE_ORIGINAL_MP_ARENAS;++i) {
        const GeStageAssetDescriptor *asset=ge_stage_asset_multiplayer(i);
        if(asset && asset->level_id==selected_stage){next.start.selection.arena_index=(int32_t)i;break;}
    }
    next.start.selection.limit_index=game_length;next.start.time_limit60=multi_game_lengths[game_length].time;
    next.start.point_limit=multi_game_lengths[game_length].points;next.aim_setting=aim_sight_adjustment;
    next.sight=mp_sight_adjust_table[aim_sight_adjustment].sight;next.autoaim=mp_sight_adjust_table[aim_sight_adjustment].autoaim;
    for(unsigned i=0;i<4;++i) {
        next.characters[i]=player_char[i];next.handicap_indices[i]=player_handicap[i];next.control_styles[i]=controlstyle_player[i];
        int team=g_playerPlayerData[i].have_token_or_goldengun;
        if(team<0 || team>1)return GE_ORIGINAL_MP_SETUP_INVALID;
        next.teams[i]=(uint8_t)team;next.start.selection.config.teams[i]=next.teams[i];
        next.start.selection.character_ids[i]=player_char[i];
        if(i<(unsigned)selected_num_players && !ge_original_multiplayer_frontend_character(
            (size_t)(player_char[i]<0?(int32_t)i:player_char[i]),&next.start.characters[i]))return GE_ORIGINAL_MP_SETUP_INVALID;
    }
    if(!launch_valid(&next))return GE_ORIGINAL_MP_SETUP_INVALID;
    *out=next;setup.taken=1;return GE_ORIGINAL_MP_SETUP_OK;
}
GeOriginalMultiplayerSetupStatus ge_original_multiplayer_setup_apply_launch(const GeOriginalMultiplayerSetupLaunch *launch)
{
    if(!launch || !launch_valid(launch))return GE_ORIGINAL_MP_SETUP_INVALID;
    unsigned count=ge_original_multiplayer_player_count();
    if(count!=(unsigned)launch->start.selection.config.player_count || scenario!=launch->start.selection.config.scenario
        || getMPWeaponSet()!=launch->start.selection.config.weapon_set)return GE_ORIGINAL_MP_SETUP_STALE;
    int selected=get_cur_playernum();
    if(selected<0 || (unsigned)selected>=count || g_CurrentPlayer!=ge_original_multiplayer_player((unsigned)selected)
        || g_playerPerm!=&g_playerPlayerData[selected])
        return GE_ORIGINAL_MP_SETUP_STALE;
    if(!ge_original_multiplayer_players_apply_control_styles(launch->control_styles,count))return GE_ORIGINAL_MP_SETUP_STALE;
    for(unsigned i=0;i<4;++i) {
        (void)ge_original_player_body_set_character((int32_t)i,launch->characters[i]);
        g_playerPlayerData[i].handicap=scenario==SCENARIO_LTK?200.0f:MP_handicap_table[launch->handicap_indices[i]].damage_modifier;
        g_playerPlayerData[i].player_perspective_height=get_player_mp_char_height((int32_t)i);
        g_playerPlayerData[i].autoaim=launch->autoaim;g_playerPlayerData[i].sight=launch->sight;
        g_playerPlayerData[i].have_token_or_goldengun=launch->teams[i];
    }
    for(unsigned i=0;i<count;++i) {
        ge_original_multiplayer_select_player(i);
        ge_original_mp_setup_control_type_exact(launch->control_styles[i]);
    }
    ge_original_multiplayer_select_player((unsigned)selected);
    return GE_ORIGINAL_MP_SETUP_OK;
}
void ge_original_multiplayer_setup_end(void)
{
    if(!setup.active)return;
    walletinst[0]=NULL;
    memset(&setup,0,sizeof(setup));
}
