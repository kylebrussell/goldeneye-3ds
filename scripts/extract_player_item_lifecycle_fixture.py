#!/usr/bin/env python3
"""Build the host fixture from unchanged canonical hand/drop/free bodies.

Resource services (projectile storage, lighting, rooms, sound) are supplied by
its real-model test harness. No replacement attachment or drop algorithm.
"""
import argparse
import importlib.util
from pathlib import Path


def render(repo):
    spec = importlib.util.spec_from_file_location(
        'parser', repo / 'scripts/extract_dam_guard_chr_scheduler_slice.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    get = module.function_text
    files = {name: (repo / path).read_text() for name, path in {
        'player': 'src/game/player.c', 'gun': 'src/game/gun.c',
        'object': 'src/game/propobj.c', 'prop': 'src/game/chrprop.c',
        'chr': 'src/game/chr.c', 'action': 'src/game/chraction.c',
        'model': 'src/game/model.c', 'objective': 'src/game/objective_status2.c',
        'services': 'port/src/ge_original_gameplay_services.c'}.items()}
    pieces = ['''/* Generated exact host player-item lifecycle fixture. */
#pragma GCC diagnostic ignored "-Wswitch"
#define PLAYERFLAG int
#define RUNTIMEBITFLAG_HASPROJECTILE (1u << 7)
#define RUNTIMEBITFLAG_EMBEDDED (1u << 6)
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <math.h>
#include "game/propobj.h"
#include "game/chrai.h"
#include "game/chraction.h"
#include "game/chr.h"
#include "game/model.h"
#include "game/player.h"
#include "game/gun.h"
#include "game/bondinv.h"
#include "game/bondview.h"
#include "game/matrixmath.h"
#include "game/stan.h"
#include "game/bg.h"
#include "game/loadobjectmodel.h"
#include "snd.h"
#include "random.h"
#include "ge_original_gameplay_services.h"
#include "ge_original_guard_grenade_object.h"
extern struct player *g_CurrentPlayer;
extern coord3d ZeroCoord;
extern union ModelRwData *ge_original_stage_guard_model_node_rw_exact(Model *, ModelNode *);
#define modelGetNodeRwData ge_original_stage_guard_model_node_rw_exact
extern void sub_GAME_7F0402B4(PropRecord *, rgba_u8 *);
extern void projectileFree(Projectile *);
extern bool bondinvHasInvItem(ITEM_IDS);
extern void embedmentFree(Embedment *);
extern void explosionClearBulletImpactRoomByFlag(PropRecord *, s8);
extern void sub_GAME_7F057C14(coord3d *, Mtxf *);
extern void mtxLoadRandomRotation(Mtxf *);
extern void sub_GAME_7F050DE8(Model *);
extern void lifecycle_deregister_rooms(PropRecord *);
extern void lifecycle_free_prop(PropRecord *);
static GeOriginalGameplayServiceStats ge_service_stats;
static void *ge_service_player_items_context;
static GeOriginalGameplayPlayerItemCreate ge_service_player_items_create;
#define chrpropDeregisterRooms lifecycle_deregister_rooms
#define chrpropFree lifecycle_free_prop
''']
    selected = {
        'services': ['ge_original_gameplay_services_bind_player_items',
            'ge_original_gameplay_services_unbind_player_items',
            'something_with_generating_object', 'chrSetWeaponFlag4'],
        'player': ['getPropForHeldItem', 'sub_GAME_7F09B368', 'sub_GAME_7F09B398'],
        'gun': ['getCurrentPlayerWeaponId'],
        'prop': ['chrpropDetach', 'chrpropDisable'],
        'chr': ['chrGetEquippedWeaponProp'],
        'action': ['chrSetFiring'],
        'model': ['clear_model_obj', 'clear_aircraft_model_obj', 'getsubmatrix'],
        'objective': ['mtxLoadRandomRotation', 'sub_GAME_7F057C14'],
        'object': ['weaponSetGunfireVisible', 'objGetWidth', 'objDetach', 'propobjSetDropped', 'objDrop',
            'objFreeEmbedmentOrProjectile', 'objFree', 'objFreePermanently', 'drop_inventory'],
    }
    for file, names in selected.items():
        pieces.extend(get(files[file], name) for name in names)
    return '\n\n'.join(pieces) + '\n'


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('repo', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.write_text(render(args.repo))
    print('Generated canonical player hand/drop/free fixture and actual factory dispatch')
