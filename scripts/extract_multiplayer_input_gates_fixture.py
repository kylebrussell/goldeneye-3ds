#!/usr/bin/env python3
"""Original full-input dependencies for a focused gate/adapter regression.

Speed, aim option and selected player queries are original. Unvisited branches
trap; gun progression, camera presentation and STAN are observing boundaries.
This fixture does not claim full MoveBond physics or a world frame.
"""
from pathlib import Path
import argparse
from extract_guard_bullet_hit_slice import find_function
from extract_bond_input_live_state_slice import BONDVIEW_GLOBALS, BONDVIEW2_HOST_GLOBALS

EXACT = {
    'bondview2.c': ('sub_GAME_7F080228', 'sub_GAME_7F07FF74', 'bondviewUpdateSpeedSideways', 'bondviewUpdateSpeedForwards',
        'bondviewCurrentPlayerUpdateSpeedTheta', 'bondviewCurrentPlayerUpdateSpeedVerta',
        'bondviewGetIfCurrentPlayerDamageShowTime', 'bondviewGetVisibleToGuardsFlag',
        'currentPlayerGetXAutoAimEnabled', 'currentPlayerGetXAutoAimEnabledRedirect',
        'currentPlayerGetYAutoAimEnabled', 'currentPlayerGetYAutoAimEnabledRedirect'),
    'bondview.c': ('currentPlayerSetSwayTarget', 'currentPlayerAdjustCrouchPos'),
    'gun.c': ('getCurrentPlayerWeaponId', 'get_hands_firing_status'),
    'gunfire.c': ('gunSetAimType', 'gunSetSightVisible'),
}
TRAPS = '''add_ammo_to_weapon advance_through_inventory autoadvance_on_deplete_all_ammo
backstep_through_inventory bondinvAddInvItem bondinvIsAliveWithFlag bondinvRemoveItemByID
bondviewGetCollisionRadius bondviewTankModelRotationRelated bondviewYPositionRelated
camera_sniper_zoom_in camera_sniper_zoom_out chrCheckGuardsHeardSound
chrlvStanPointPointIntersection getCurrentPlayerNoise get_ammo_count_for_weapon
get_ammo_in_hands_magazine get_item_in_hand_zoom stanResetHits stanTestLineUnobstructed
trigger_remote_mine_detonation trigger_solo_watch_menu'''.split()
SINKS = {
    'gunTickGameplay': 'fixture_gun_trigger(triggerOn);',
    'bondwalkItemCheckBitflags': '(void)item; (void)mask; return 0;',
    'bondviewTriggerWatchZoom': '(void)zoominfovy;',
    'bondviewUpdateWatchZoomIn': '',
    'sub_GAME_7F067F58': '',
    'sub_GAME_7F067FBC': '',
    'get_scenario': 'return 0;',
}

def generate(repo):
    sources = {p.name: p.read_text() for p in (repo/'src/game').glob('*.c')}
    def body(name):
        if name == 'get_scenario': return 'MPSCENARIOS get_scenario(void) {}'
        for source in sources.values():
            if name+'(' in source:
                try: return find_function(source, name)
                except ValueError: pass
        raise ValueError(name)
    out=['#include <assert.h>', '#include <stdlib.h>', '#include <math.h>',
         '#include <ultra64.h>', '#include <bondtypes.h>', '#include <bondconstants.h>',
         'typedef int PLAYERFLAG;', '#include "game/bondview.h"',
         '#include "game/gun.h"', '#include "game/stan.h"', '#include "game/player.h"',
         '#include "ge_original_bond_input_internal.h"',
         'extern void fixture_gun_trigger(s32);', '#define FOV_Y_F 60.0f']
    out += list(BONDVIEW_GLOBALS)+list(BONDVIEW2_HOST_GLOBALS)
    for path,names in EXACT.items(): out += [find_function(sources[path],name) for name in names]
    for name in TRAPS + list(SINKS):
        sig=body(name).split('{',1)[0]
        statement=SINKS.get(name, f'assert(!"unexpected fixture boundary: {name}"); abort();')
        out += [sig+'{ '+statement+' }']
    return '\n\n'.join(out)+'\n'

if __name__ == '__main__':
    p=argparse.ArgumentParser();p.add_argument('repo',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args();a.output.write_text(generate(a.repo))
