#!/usr/bin/env python3
"""Extract canonical ray->player damage bodies for isolated combat acceptance.

The fixture supplies primitive model geometry and presentation sinks. Geometry,
damage modifiers, health/armor application, player switching and score updates
are copied from canonical sources, never reimplemented in the fixture.
"""
from pathlib import Path
import argparse
from extract_guard_bullet_hit_slice import find_function
from extract_bond_move_explosion_slice import extract_data

FUNCTIONS = {
    'player.c': ('set_cur_player', 'get_cur_playernum', 'getPlayerCount', 'getPlayerPointerIndex'),
    'model.c': ('getinstsize', 'modelFindNodeMtxIndex', 'modelFindNodeMtx', 'getsubmatrix',
                'getsubroty', 'modelTestRayIntersectsTransformedBBox', 'modelTestRayIntersectsNodeBBox'),
    'objecthandler.c': ('sub_GAME_7F06B120', 'probably_damage_detail_blood_effect_related', 'sub_GAME_7F06C010'),
    'propobj.c': ('projectileTestPropBoundingSphere',),
    'chrprop.c': ('chrpropAddBulletHit',),
    'chraction.c': ('get_distance_actor_to_position', 'chrlvAttackAnimationRelated7F026F30', 'handles_shot_actors'),
    'chr.c': ('chrTestHit', 'chrHandleBulletHit'),
    'bondview2.c': ('bondviewKillCurrentPlayer', 'record_damage_kills'),
    'gun.c': ('gunItemGetDestructionAmount',),
    'gunfire.c': ('increment_num_kills_display_text_in_MP', 'increment_num_deaths',
                  'increment_num_times_killed_MwtGC'),
}

def generate(repo):
    sections = ['/* Generated canonical multiplayer combat fixture. */',
        '#include <stdio.h>', '#include <math.h>', '#include <ultra64.h>',
        '#include <bondgame.h>', '#include <bondconstants.h>', '#include <bondtypes.h>',
        '#include <bondaicommands.h>', '#include <music.h>', '#include <snd.h>', '#include <random.h>',
        '#include <joy.h>', '#include "assets/obseg/text/LgunE.h"',
        '#include "assets/animationtable_data.h"']
    for name in ('bondview', 'bondinv', 'chraction', 'chr', 'gun', 'player', 'model',
                 'propobj', 'objecthandler', 'front', 'lv', 'matrixmath', 'image',
                 'tex', 'textrelated', 'options', 'explosion', 'language', 'chrobjdata'):
        sections.append(f'#include "game/{name}.h"')
    sections += ['extern ModelHitEntry *g_ModelHitFreeList;',
        'extern s32 g_stopPlayFlag, g_gameOverFlag;',
        'extern void trigger_solo_watch_menu(s32);',
        'extern bool propobjFindHit(Model *, ModelNode *, coord3d *, coord3d *, struct HitThing *, s32 *, ModelNode **);',
        'extern void chrCreateHitPuffs(PropRecord *, s32, coord3d *, coord3d *);',
        'extern void chrCreateBloodStain(Model *, s32, ModelNode *, coord3d *);',
        'extern void play_sound_for_shot_actor(ChrRecord *);',
        'extern void chrStopFiring(ChrRecord *);',
        'extern s32 chrlvAttackAnimationRelated7F026F30(ChrRecord *, f32 *);',
        'extern f32 get_distance_actor_to_position(ChrRecord *, coord3d *);',
        'extern void triggered_on_shot_hit(ChrRecord *, coord3d *, f32, s32, ITEM_IDS);',
        'extern void increment_num_deaths(void);', 'extern void increment_num_suicides_display_MP(void);',
        'extern void increment_num_times_killed_MwtGC(void);',
        'extern void hudMakeDamageSegments(f32 *, s32, s32, f32);',
        'extern bool modelTestRayIntersectsNodeBBox(Model *, ModelNode *, coord3d *, coord3d *);',
        'extern s32 sub_GAME_7F074CAC(Model *, ModelNode *, coord3d *, coord3d *);',
        'extern ModelHitEntry *sub_GAME_7F06B120(ModelHitEntry *, Model *);',
        'extern s32 sub_GAME_7F06C010(ModelHitEntry **, coord3d *, coord3d *, Model **, ModelNode **);',
        'extern s32 probably_damage_detail_blood_effect_related(ModelHitEntry **, coord3d *, coord3d *, Model **, ModelNode **);',
        'extern ModelFileHeader cartridge_header;', 'extern s16 metal_ricochet_SFX[3];',
        'extern char aSD[], g_GunDeathCountFormat[];']
    player = (repo/'src/game/player.c').read_text()
    for name in ('g_playerPointers', 'g_CurrentPlayer', 'g_playerPlayerData', 'g_playerPerm', 'player_num'):
        sections.append(extract_data(player, name))
    stats = (repo/'assets/obseg/gun/gunWeaponStats.inc.c').read_text()
    sections.append(stats[:stats.index('WeaponStats')])
    sections.append(extract_data(stats, 'wppk_stats'))
    sections.append(extract_data((repo/'src/game/chr.c').read_text(), 'D_80030078'))
    for path, names in FUNCTIONS.items():
        source = (repo/'src/game'/path).read_text()
        for name in names:
            body = find_function(source, name)
            body = body.replace('s16 mrs[3] = metal_ricochet_SFX;',
                's16 mrs[3] = {HIT_BULLET_METAL_A3_SFX, HIT_BULLET_METAL_A_SFX, HIT_BULLET_METAL_B_SFX};')
            sections.append(body)
    return '\n\n'.join(sections) + '\n'

if __name__ == '__main__':
    p=argparse.ArgumentParser();p.add_argument('repo',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args();a.output.write_text(generate(a.repo))
