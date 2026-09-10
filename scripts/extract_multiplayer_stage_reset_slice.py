#!/usr/bin/env python3
"""Retain original pre-record intro assignments for verified native owners.

No state is defined here. Explicit missing/cinematic/provider-only names are
excluded; every retained statement is copied in source order, with no rewrite.
"""
from pathlib import Path
import argparse
import hashlib
import re
from extract_guard_bullet_hit_slice import find_function
from extract_bond_move_state_slice import extract_variable

OWNERS = {
    'bondview.c': '''g_bondviewForceDisarm cameraBufferToggle cameraFrameCounter1
cameraFrameCounter2 camera_80036438 credits_state credits_pointer
 g_SurroundBondWithExplosionsFlag g_PlayerIsInTank g_PlayerTankProp g_TankSfxState
 g_TankTurnSpeed g_TankOrientationAngle g_TankTurretVerticalAngle
 g_TankTurretVerticalAngleRelated g_TankTurretOrientationAngleRad
 g_TankTurretOrientationAngleDeg tank_turret_turn_speed g_TankTurretAngle
 g_TankTurretTurn g_ExplodeTankOnDeathFlag is_timer_active g_PlayerInvincible
 g_CameraMode g_CameraAfterCinema stop_time_flag watch_transition_time'''.split(),
    'bondview2.c': ['g_ForceBondMoveOffset','startpadcount','mission_timer','starting_weapon'],
    'front.c': ['g_isBondKIA'],
}
OMITTED = '''resolution g_WorldTankProp g_PlayerTankYOffset tank_turret_unused_angle
 g_BondCanEnterTank camera_fade_active camera_transition_timer intro_camera_index
 g_IntroSwirl ptr_random06cam_entry g_CurrentSetupIntroCamera g_SetupIntroCameraCount
 g_IntroAnimationIndex'''.split()
LOCALS = ['intro_record','set_starting_weapon','start_look_angle']
NAMES = [name for names in OWNERS.values() for name in names]+['watch_time_0']


def declarations(repo, external=True, exclude=()):
    lines=[]
    for path,names in OWNERS.items():
        source=(repo/'src/game'/path).read_text()
        for name in names:
            if name in exclude: continue
            declaration=extract_variable(source,name).split('=',1)[0].rstrip().rstrip(';')+';'
            lines.append(('extern ' if external else '')+declaration)
    if 'watch_time_0' not in exclude:
        source=(repo/'src/game/bondview2.c').read_text()
        start=source.index('#if defined(VERSION_JP) || defined(VERSION_EU)',
                           source.index('s32 mission_timer;'))
        end=source.index('#endif',start)+len('#endif')
        declaration=source[start:end]
        assert 'f32 watch_time_0;' in declaration and 's32 watch_time_0;' in declaration
        if external:
            declaration=re.sub(r'(?m)^(f32|s32) watch_time_0;',r'extern \1 watch_time_0;',declaration)
        lines.append(declaration)
    return '\n'.join(lines)+'\n'


def statements(repo):
    source=find_function((repo/'src/game/bondview_r.c').read_text(),'bondviewLoadSetupIntroSection')
    prefix=source[source.index('    intro_record = '):source.index('    if (intro_record != NULL)')]
    # Multiplayer arenas never enter the Cuba framebuffer allocation branch.
    # Its cameraFrameCounter1=1 belongs to that omitted branch, not the prefix.
    prefix, removed = re.subn(r'\n    if \(bossGetStageNum\(\) == LEVELID_CUBA\)\n    \{.*?\n    \}\n', '', prefix, flags=re.S)
    assert removed == 1
    lines=[];seen=set()
    for line in prefix.splitlines():
        match=re.match(r'\s*(\w+)(?:\.[\w.]+)?(?:\[[^]]+\])?\s*=\s*.*;',line)
        if match:
            name=match.group(1)
            assert name in NAMES+OMITTED+LOCALS,name
            if name in NAMES: lines.append(line);seen.add(name)
    assert seen==set(NAMES),(set(NAMES)-seen)
    return '\n'.join(lines)

PREAMBLE='''#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <snd.h>
#ifndef PLAYERFLAG
typedef int PLAYERFLAG;
#endif
#include "game/bondview.h"
'''

def generate(repo):
    body=statements(repo)
    return '/* Exact native intro subset SHA-256: '+hashlib.sha256(body.encode()).hexdigest()+' */\n'+PREAMBLE+declarations(repo)+'''\nvoid ge_original_multiplayer_stage_intro_live_prefix_exact(void)
{
#define FLOAT_INIT 0
'''+body+'\n#undef FLOAT_INIT\n}\n'

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('repo',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args();a.output.write_text(generate(a.repo))
