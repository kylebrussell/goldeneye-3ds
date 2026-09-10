#!/usr/bin/env python3
"""Retain canonical per-player held-item hitscan/melee dispatch and sight rule.

The original background and actor owners remain external. Native light Gfx
mutation is an explicit provider frontier, never an empty state replacement.
"""
from pathlib import Path
import argparse
import hashlib
from extract_guard_bullet_hit_slice import find_function

FUNCTIONS = (
    'chrpropRayIntersectsRoomBbox',
    'chrpropFindCloserBgHitInVisibleRooms',
    'chrpropFindFirstBgHitInConnectedRooms',
    'chrpropFindClosestBgHitRoom',
    'chraiDefaultWeaponFireHandler',
    'chraiFistAttackHandler',
    'chraiCheckUseHeldItem',
    'chraiCheckUseHeldItems',
)


def native_body(body, name):
    if name == 'chraiDefaultWeaponFireHandler':
        # These values are overwritten on every path that uses them. Explicit
        # defaults preserve that flag-correlated source behavior while making
        # GCC's native -Wmaybe-uninitialized analysis conclusive.
        body = body.replace('    f32 new_var;', '    f32 new_var = 300.0f;', 1)
        body = body.replace('    s32 besttexture;', '    s32 besttexture = -1;', 1)
        body = body.replace('    f32 negz;', '    f32 negz = 0.0f;', 1)
        # IDO matching-only uninitialized arithmetic has no gameplay output.
        matching = '''        new_var++;
        new_var--;

        if (new_var == new_var);'''
        assert body.count(matching) == 1
        body = body.replace(matching, '        /* Removed IDO matching-only uninitialized temporary reads. */')
        # On little-endian native bitfields, the original first byte's LOW
        # nibble is the named hitTexture member, not the native first byte.
        body = body.replace('((u8 *) (&g_Textures[bghit.texturenum]))[0] & 0xf',
                            'g_Textures[bghit.texturenum].hitTexture')
        raw = '(((0xf & ((u8 *) g_Textures)[besttexture * 8]) != 5) && ((((u8 *) g_Textures)[besttexture * 8] & 0xf) != 6))'
        assert body.count(raw) == 1
        body = body.replace(raw, '((g_Textures[besttexture].hitTexture != 5) && (g_Textures[besttexture].hitTexture != 6))')
        # STAN-only source path indexes the element before g_Textures. There
        # is no portable/native semantic value for those unrelated bytes.
        marker = '                recall_joy2_hits_edit_flag(shotdata.weapon, finalpos, besttexture);'
        assert body.count(marker) == 1
        body = body.replace(marker, '''                if (besttexture < 0)
                {
                    ge_original_player_held_items_stan_texture_frontier();
                    return;
                }

''' + marker)
        # Same reverse order, without forming a pointer before the array.
        body = body.replace('for (pp = g_LastOnScreenProp; (--pp) >= g_OnScreenPropList;)\n    {',
            'for (pp = g_LastOnScreenProp; pp > g_OnScreenPropList;)\n    {\n        --pp;')
    if name == 'chraiFistAttackHandler':
        body = body.replace('for (propptr = g_LastOnScreenProp - 1; propptr >= g_OnScreenPropList; propptr--)\n    {',
            'for (propptr = g_LastOnScreenProp; propptr > g_OnScreenPropList;)\n    {\n        --propptr;')
    return body


def generate(repo):
    source = (repo / 'src/game/chrprop.c').read_text()
    bodies = [find_function(source, name) for name in FUNCTIONS]
    viewport = find_function((repo / 'src/game/bondview2.c').read_text(),
                             'bondviewMovePlayerUpdateViewport')
    start = viewport.index('    gunSetSightVisible(')
    end = viewport.index('\n    );', start) + len('\n    );')
    sight = viewport[start:end]
    preamble = [
        '/* Generated from canonical held-item source; do not edit. */',
        '/* Canonical source SHA-256: ' + hashlib.sha256(('\n'.join(bodies)+sight).encode()).hexdigest() + ' */',
        '#include <math.h>', '#include <ultra64.h>', '#include <bondconstants.h>',
        '#include <bondtypes.h>', '#include <bondaicommands.h>',
        '#ifndef PLAYERFLAG', 'typedef int PLAYERFLAG;', '#endif',
        '#include <random.h>', '#include <snd.h>', '#include <music.h>',
    ]
    for header in ('bg', 'bondview', 'chraction', 'chr', 'gun', 'model', 'matrixmath',
                   'player', 'propobj', 'stan', 'image', 'tex', 'explosion', 'glass',
                   'lightfixture', 'options', 'objective_status'):
        preamble.append(f'#include "game/{header}.h"')
    preamble += [
        '#include "ge_original_bond_input_internal.h"',
        '#include "ge_original_player_held_items.h"',
        '#define M_U32_MAX_VALUE_F 4294967296.0f',
        '#define M_U16_MAX_VALUE_F 65536.0f',
        'extern PropRecord *g_OnScreenPropList[];', 'extern PropRecord **g_LastOnScreenProp;',
        'extern void ge_original_bullet_path_from_screen_center_exact(coord3d *, coord3d *, GUNHAND);',
        'extern Mtxf *ge_original_gun_current_player_view_to_world(void);',
        '#define bullet_path_from_screen_center ge_original_bullet_path_from_screen_center_exact',
        '#define currentPlayerGetViewToWorldMtxf ge_original_gun_current_player_view_to_world',
        'extern void ge_original_player_held_items_light_break_exact(Gfx *, u32, s32);',
        'extern void ge_original_player_held_items_stan_texture_frontier(void);',
        '#define lightFixtureBreak ge_original_player_held_items_light_break_exact',
        'extern void chrlvStanLineDirIntersection(coord3d *, coord3d *, coord3d *);',
        'extern void sub_GAME_7F04E9BC(PropRecord *, ShotData *);',
        'extern void gunFireTankShell(s32);',
        'extern void chraiDefaultWeaponFireHandler(s32);',
        'extern void objectiveTakePictureHandler(void);',
        'extern bool bgTestRayIntersectsBbox(coord3d *, coord3d *, s32 *, s32 *);',
        'extern s32 bgGetConnectedRooms(s32, s32 *, s32);',
        'extern s32 getMaxNumRooms(void);',
        'extern void objHit(ShotData *, BulletHit *);',
        'extern s32 getCurrentWeaponOrItem(void);',
        'extern void modelGetAxisExtents(Model *, f32 *, f32 *, s32);',
        'extern s32 currentPlayerGetCrouchPos(void);',
        '#define g_BgRoomInfo ge_bg_visibility_room_info',
        'extern s_room_info ge_bg_visibility_room_info[];',
        '#define g_MaxNumRooms ge_bg_visibility_max_rooms',
        'extern s32 ge_bg_visibility_max_rooms;',
        '#define getMaxNumRooms ge_original_held_get_max_rooms_exact',
        '#define getCurrentWeaponOrItem ge_original_held_current_weapon_exact',
        '#define currentPlayerGetCrouchPos ge_original_held_crouch_exact',
    ]
    support = [find_function((repo / 'src/game' / path).read_text(), name)
               for path, name in (('bg.c', 'getMaxNumRooms'),
                                  ('gun.c', 'getCurrentWeaponOrItem'),
                                  ('bondview.c', 'currentPlayerGetCrouchPos'))]
    return '\n'.join(preamble) + '\n\n' + '\n\n'.join(support) + '\n\n' + '\n\n'.join(
        native_body(body, name) for name, body in zip(FUNCTIONS, bodies)) + \
        '\n\nvoid ge_original_player_apply_sight_option_exact(void)\n{\n' + sight + '\n}\n'


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('repo', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(generate(args.repo))
    print(f'generated {len(FUNCTIONS)} held-item functions plus exact sight call -> {args.output}')
