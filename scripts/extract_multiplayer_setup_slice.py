#!/usr/bin/env python3
"""Retain original front.c multiplayer input/state with explicit resource edges."""
from pathlib import Path
import argparse
import hashlib
from extract_stage_pickup_slice import function_text
from extract_bond_move_explosion_slice import extract_data

PAGES=(('0E_mpoptions','0E_mpoptions'),('0f_mpcharsel','0F_mpcharsel'),
       ('10_mphandicap','10_mphandicap'),('11_mpcontrol','11_mpcontrols'),
       ('12_mpstage','12_mpstage'),('13_mpscenariosel','13_mpscenario'),
       ('14_mpteamsel','14_mpteams'))

def render(root):
    source=(root/'src/game/front.c').read_text()
    out='''/* Generated original multiplayer setup input/state. */
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <joy.h>
#include <random.h>
#include <music.h>
typedef struct OSViMode OSViMode;
#include <fr.h>
#include "game/front.h"
#include "game/player.h"
#include "game/mp_weapon.h"
#include "game/image.h"
#include "assets/image_externs.h"
#include "assets/oddtextures.h"
#include "assets/obseg/text/LtitleE.h"
#include "ge_original_multiplayer_setup.h"
#ifdef AIPARSE
enum {
#define IMAGE(NAME, ...) IMAGE_##NAME,
#include <assets/images.def>
#undef IMAGE
    GE_MP_SETUP_IMAGE_COUNT
};
#endif
extern s32 player_char[4],scenario,selected_num_players,selected_folder_num;
extern struct mp_stage_playercount mp_player_counts[];
extern struct mp_stage_setup multi_stage_setups[];
extern GAMEMODE gamemode;
extern s32 g_musicXTrack1Fade;
void ge_original_mp_setup_wallet_load(void);
void ge_original_mp_setup_wallet_clear(struct Model *);
void ge_original_mp_setup_wallet_switch(struct Model *,s32,s32);
void ge_original_mp_setup_texture_load(u32,s32);
s32 ge_original_mp_setup_stage_status(s32,s32,s32);
void ge_original_mp_setup_audio(GeOriginalMultiplayerSetupAudio,s32);
void ge_original_mp_setup_vi(s32,float,float);
float ge_original_mp_setup_delta(void);
s32 ge_original_mp_setup_seats(void);
#define joyGetControllerCount ge_original_mp_setup_seats
#define g_GlobalTimerDelta ge_original_mp_setup_delta()
#define getPlayer_c_screenleft() 0.0f
#define getPlayer_c_screentop() 0.0f
#define getPlayer_c_screenwidth() 440.0f
#define getPlayer_c_screenheight() 330.0f
#define load_walletbond ge_original_mp_setup_wallet_load
#define disable_all_switches ge_original_mp_setup_wallet_clear
#define set_item_visibility_in_objinstance ge_original_mp_setup_wallet_switch
#define texLoadFromTextureNum ge_original_mp_setup_texture_load
#define fileIsStageUnlockedAtDifficulty ge_original_mp_setup_stage_status
#define viSetFovY(v) ge_original_mp_setup_vi(0,v,0)
#define viSetAspect(v) ge_original_mp_setup_vi(1,v,0)
#define viSetZRange(a,b) ge_original_mp_setup_vi(2,a,b)
#define viSetUseZBuf(v) ge_original_mp_setup_vi(3,v,0)
#define sndApplyVolumeAllSfxSlot(v) ge_original_mp_setup_audio(GE_ORIGINAL_MP_SETUP_SFX_VOLUME,v)
#define musicTrack1ApplySeqpVol(v) ge_original_mp_setup_audio(GE_ORIGINAL_MP_SETUP_MUSIC_VOLUME,v)
#define musicTrack1Play(v) ge_original_mp_setup_audio(GE_ORIGINAL_MP_SETUP_MUSIC_PLAY,v)
#define sndPlaySfx(bank,id,state) ge_original_mp_setup_audio(GE_ORIGINAL_MP_SETUP_SFX_PLAY,id)
'''
    initialized=('current_menu','menu_update','maybe_prev_menu','g_MenuTimer',
        'tab_start_selected','tab_next_selected','tab_prev_selected','tab_start_highlight','tab_next_highlight','tab_prev_highlight',
        'selected_stage','briefingpage','selected_difficulty','cursor_h_pos','cursor_v_pos',
        'maybe_is_in_menu','walletinst','MP_stage_selected','game_length','aim_sight_adjustment',
        'num_chars_selectable_mp','unlock_stage_select','unlock_game_length','unlock_chars','unlock_weapon_select',
        'unlock_handicap','unlock_control_style','unlock_aim_sight','D_8002B560',
        'multi_game_lengths','mp_chr_setup','MP_handicap_table',
        'MP_controller_configuration_table','mp_sight_adjust_table','is_emulating_spectrum')
    for name in initialized:out+=extract_data(source,name)+'\n'
    for name in ('mp_char_cur_select_player','mp_char_prev_select_player','mp_char_select_scroll_offset',
                 'player_has_selected_char','size_mp_select_image_player','dword_CODE_bss_80069760',
                 'player_handicap','controlstyle_player'):
        out+=f's32 {name}[MAX_PLAYER_COUNT];\n'
    for name in ('current_mp_stage_highlighted','dword_CODE_bss_80069780','teamsize',
                 'highlight_players','highlight_scenario','highlight_gameselect','highlight_gamelength',
                 'highlight_character','highlight_weaponselect','highlight_health','highlight_controlstyle','highlight_aimadjustment'):
        out+=f's32 {name};\n'
    out+=extract_data((root/'assets/oddtextures.c').read_text(),'s_mpcharselimages')+'\n'
    out+='#define mpcharselimages s_mpcharselimages\n'
    names=('frontChangeMenu','frontCheckCursorOnStartTab','frontCheckCursorOnPreviousTab',
        'frontCheckCursorOnNextTab','frontSetCursorPositionToNextTab','setCursorPOSforMode','frontUpdateControlStickPosition',
        'check_if_mp_stage_unlocked','select_game_length','copy_aim_settings_to_playerdata',
        'advance_aim_settings_selection','unlock_all_mp_chars','get_player_mp_handicap',
        'get_mp_timelimit','get_mp_pointlimit','reset_mp_options_for_scenario','init_mp_options_for_scenario',
        'get_players_who_have_selected_mp_char','get_players_team_or_scenario_item_flag','set_players_team_or_scenario_item_flag')
    for name in names+tuple(name for init,interface in PAGES for name in ('init_menu'+init,'interface_menu'+interface)):
        body=function_text(source,name)
        out+='/* '+name+' source sha256 '+hashlib.sha256(body.encode()).hexdigest()+' */\n'+body+'\n'
    # Actual options setter, local name avoids interfering with unrelated
    # option ownership. Setup publication supplies the native font mode.
    out+='extern s32 j_text_trigger;\n'
    out+=function_text((root/'src/game/options.c').read_text(),'cur_player_set_control_type').replace(
        'cur_player_set_control_type','ge_original_mp_setup_control_type_exact')+'\n'
    return out
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('root',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args();a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(render(a.root))
