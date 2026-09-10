#!/usr/bin/env python3
"""Native setup fixture's actual permanent/player tables and allocation helpers."""
from pathlib import Path
import argparse
from extract_stage_pickup_slice import function_text
from extract_bond_move_explosion_slice import extract_data

def render(root):
    player=(root/'src/game/player.c').read_text();front=(root/'src/game/front.c').read_text()
    out='''#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/front.h"
#include "game/ramromreplay.h"
ramromfilestructure *ptr_active_demofile;
s32 is_ramrom_flag,recording_ramrom_flag;
#include "assets/obseg/text/LtitleE.h"
#include "assets/oddtextures.h"
s32 g_VisibleToGuardsFlag;
PadRecord *g_Startpad[16];
s32 startpadcount;
'''
    for name in ('g_playerPointers','g_CurrentPlayer','g_playerPlayerData','g_playerPerm','player_num','array_PLAYER_IDs'):
        out+=extract_data(player,name)+'\n'
    for name in ('set_cur_player','get_cur_playernum','getPlayerCount','default_player_perspective_and_height'):
        out+=function_text(player,name)+'\n'
    for name in ('selected_num_players','selected_folder_num','gamemode','scenario','mp_player_counts','multi_stage_setups'):
        out+=extract_data(front,name)+'\n'
    out+=function_text(front,'get_selected_num_players')+'\n'
    out+=extract_data((root/'src/game/chraidata.c').read_text(),'setup_text_pointers').replace('setup_text_pointers','ge_mp_setup_text_pointers')+'\n'
    return out
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('root',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args();a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(render(a.root))
