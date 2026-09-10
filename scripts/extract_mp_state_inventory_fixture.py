#!/usr/bin/env python3
"""Native inventory-owner fixture: original allocation/list/counter/scoring bodies."""
from pathlib import Path
import argparse
from extract_bond_move_state_slice import extract_variable
from extract_stage_pickup_slice import function_text

def render(root, include_shuffle=True):
    player=(root/'src/game/player.c').read_text()
    inv=(root/'src/game/bondinv.c').read_text()
    gun=(root/'src/game/gunfire.c').read_text()
    s=['''#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <stdio.h>
#include "game/player.h"
#include "game/bondinv.h"
#include "game/front.h"
#include "game/lv.h"
#include "game/language.h"
#include "game/ramromreplay.h"
ramromfilestructure *ptr_active_demofile;
s32 is_ramrom_flag,recording_ramrom_flag;
#include "assets/obseg/text/LgunE.h"
s32 g_VisibleToGuardsFlag;
s32 scenario;
extern InvItem *bondinvGetNextAvailItem(void);
extern InvItem *bondinvGetInvItem(ITEM_IDS);
extern InvItem *bondinvGetDualWeapon(ITEM_IDS,ITEM_IDS);
extern bool bondinvHasDualWeapon(ITEM_IDS,ITEM_IDS);
extern s32 bondinvHasInvItem(ITEM_IDS);
extern void bondinvSortInv(InvItem *);
extern void bondinvInsertItem(InvItem *);
extern void hudmsgBottomShow(char *);
static const char aSD[]="%s %d",aSD_0[]="%s %d",g_GunDeathCountFormat[]="%s %d %s";
''']
    for name in ('g_playerPointers','g_CurrentPlayer','g_playerPlayerData','g_playerPerm','player_num'):
        s.append(extract_variable(player,name))
    if include_shuffle:
        s.append(extract_variable(player,'array_PLAYER_IDs'))
    for source,names in ((player,('set_cur_player','get_cur_playernum','getPlayerCount','default_player_perspective_and_height')),
                         (inv,('bondinvReinitInv','bondinvGetNextAvailItem','bondinvGetInvItem','bondinvHasInvItem','bondinvSortInv','bondinvInsertItem','bondinvAddInvItem','bondinvGetDualWeapon','bondinvHasDualWeapon','bondinvAddDoublesInvItem','bondinvAddPropToInv','bondinvRemoveItem')),
                         (gun,('increment_num_kills_display_text_in_MP','increment_num_times_killed_MwtGC','increment_num_deaths','increment_num_suicides_display_MP')),
                         ((root/'src/game/front.c').read_text(),('get_scenario',)),
                         ((root/'src/game/mpmenu.c').read_text(),('get_points_for_mp_player',))):
        s.extend(function_text(source,name) for name in names)
    return '\n\n'.join(s)
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('root',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args();a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(render(a.root))
