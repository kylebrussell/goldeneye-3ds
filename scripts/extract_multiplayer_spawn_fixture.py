#!/usr/bin/env python3
"""Original player globals, intro reference, inventory and character fixture."""
from pathlib import Path
import argparse
from extract_bond_move_state_slice import extract_variable
from extract_bond_move_explosion_slice import extract_data
from extract_stage_pickup_slice import function_text
from extract_multiplayer_regen_slice import block


def render(repo):
    player=(repo/'src/game/player.c').read_text()
    bond=(repo/'src/game/bondview.c').read_text()
    bond2=(repo/'src/game/bondview2.c').read_text()
    front=(repo/'src/game/front.c').read_text()
    inventory=(repo/'src/game/bondinv.c').read_text()
    sections=['''/* Generated unchanged canonical initial-spawn fixture. */
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <math.h>
#include "game/player.h"
#include "game/bondview.h"
#include "game/bondinv.h"
#include "game/front.h"
#include "assets/obseg/text/LtitleE.h"
#include "assets/oddtextures.h"
#include "assets/animationtable_data.h"
#include "ge_original_player_spawn.h"
s32 g_VisibleToGuardsFlag;
extern s32 startpadcount;
extern PadRecord *g_Startpad[16];
extern s32 bondviewGetRandomSpawnPadIndex(void);
extern f32 bondviewYPositionRelated(StandTile *, f32, f32);
extern s32 bondinvHasInvItem(ITEM_IDS);
extern InvItem *bondinvGetNextAvailItem(void);
extern InvItem *bondinvGetInvItem(ITEM_IDS);
extern void bondinvSortInv(InvItem *);
extern void bondinvInsertItem(InvItem *);
''']
    for name in ('g_playerPointers','g_CurrentPlayer','g_playerPlayerData','g_playerPerm','player_num'):
        sections.append(extract_variable(player,name))
    for name in ('set_cur_player','get_cur_playernum','getPlayerCount','default_player_perspective_and_height'):
        sections.append(function_text(player,name))
    for name in ('g_bondviewBondDeathAnimations','g_bondviewBondDeathAnimationsCount'):
        sections.append(extract_variable(bond,name))
    sections += [extract_data(front,'mp_chr_setup'), 's32 player_char[4]={-1,-1,-1,-1};', function_text(front,'get_player_mp_char_height')]
    for name in ('bondinvReinitInv','bondinvGetNextAvailItem','bondinvGetInvItem','bondinvHasInvItem','bondinvSortInv','bondinvInsertItem','bondinvAddInvItem'):
        sections.append(function_text(inventory,name))
    sections.append(function_text(bond2,'change_player_pos_to_target'))
    intro=function_text((repo/'src/game/bondview_r.c').read_text(),'bondviewLoadSetupIntroSection')
    selection,_=block(intro,'if (startpadcount > 0)')
    sections.append('''void ge_mp_spawn_reference(GeOriginalPlayerSpawnConfig *out, s32 *selected)
{
    s32 rand_pad_index;
    coord3d start_pos={0};
    StandTile *start_stan;
    f32 stan_height, start_look_angle=0;
''' + selection + '''
    out->position[0]=start_pos.x;out->position[1]=start_pos.y;out->position[2]=start_pos.z;
    out->floor_y=stan_height;out->look_angle_radians=start_look_angle;
    out->eye_height=g_CurrentPlayer->eyeheight;out->stan=start_stan;
    *selected=rand_pad_index;
}
''')
    return '\n\n'.join(sections)


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('repo',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args();a.output.write_text(render(a.repo))
