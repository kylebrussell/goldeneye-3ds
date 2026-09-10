#!/usr/bin/env python3
"""Exact input/weapon/camera support for held-item combat acceptance."""
from pathlib import Path
import argparse
from extract_guard_bullet_hit_slice import find_function
from extract_bond_move_explosion_slice import extract_data

FUNCTIONS = {
    'gun.c': ('get_hands_firing_status', 'getCurrentPlayerWeaponId',
              'get_item_in_hand_or_watch_menu', 'get_ptr_item_statistics',
              'bondwalkItemCheckBitflags'),
    'bondview.c': ('camGetWorldToScreenMtxf', 'currentPlayerGetViewToWorldMtxf',
                   'getPlayer_c_screenwidth', 'getPlayer_c_screenheight',
                   'getPlayer_c_perspaspect', 'transformAndNormalizeByLength2Dto3D'),
    'bondview2.c': ('getCurrentPlayerProp', 'bondviewGetCurrentPlayersPosition',
                   'bondviewGetPlayerDuckingHeightRelated'),
    'gunfire.c': ('bullet_path_from_screen_center', 'inc_curplayer_hitcount_with_weapon',
                  'gunSetSightVisible'),
    'propobj.c': ('trigger_remote_mine_detonation',),
    'chrprop.c': tuple('chrpropSumMatrix' + sign + axis
                      for sign in ('Pos', 'Neg') for axis in ('X', 'Y', 'Z')),
}

def generate(repo):
    pieces = ['#include <math.h>', '#include <ultra64.h>', '#include <bondconstants.h>',
        '#include <bondtypes.h>', '#include <music.h>', '#include <snd.h>', '#include <random.h>',
        '#include "game/bondview.h"', '#include "game/gun.h"', '#include "game/propobj.h"',
        '#include "game/player.h"', '#define SCREEN_HEIGHT_272 272',
        '#define SCREEN_HEIGHT_240 240',
        '#include "ge_original_bond_input_internal.h"',
        '#define currentPlayerGetViewToWorldMtxf ge_original_gun_current_player_view_to_world',
        '#define bullet_path_from_screen_center ge_original_bullet_path_from_screen_center_exact',
        '#define g_GunScreenAspectRatio (4.0f / 3.0f)',
        'extern s32 g_RemoteMineOwnerTriggerFlag;',
        'extern GunModelFileRecord gitem_structs[];',
    ]
    stats = (repo/'assets/obseg/gun/gunWeaponStats.inc.c').read_text()
    pieces += [stats[:stats.index('WeaponStats')],
               extract_data(stats, 'default_weaponstats'),
               extract_data(stats, 'shotgun_stats'),
               extract_data(stats, 'autoshot_stats'),
               (repo/'assets/obseg/gun/fist/gunWeaponStat.inc.c').read_text()]
    for path, names in FUNCTIONS.items():
        source = (repo/'src/game'/path).read_text()
        pieces += [find_function(source, name) for name in names]
    return '\n\n'.join(pieces) + '\n'

if __name__ == '__main__':
    p=argparse.ArgumentParser();p.add_argument('repo',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args();a.output.write_text(generate(a.repo))
