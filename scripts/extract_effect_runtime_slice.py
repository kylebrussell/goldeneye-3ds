#!/usr/bin/env python3
"""Retain native effect reset/update and alpha-pass retirement statements."""
from pathlib import Path
import argparse
import hashlib
import re
from extract_bond_move_explosion_slice import extract_function, extract_data
from extract_gun_casing_slice import scalar_declaration


def generate(repo):
    glass=(repo/'src/game/glass2.c').read_text()
    shards=(repo/'src/game/glass.c').read_text()
    explosion=(repo/'src/game/explosion.c').read_text()
    gun=(repo/'src/game/gunfire.c').read_text()
    objective=(repo/'src/game/objective_status2.c').read_text()
    init=(repo/'src/game/initactorpropstuff.c').read_text()
    lv=(repo/'src/game/lv.c').read_text()
    audio=extract_function((repo/'src/game/ejectedcartridges.c').read_text(),
                           'init_ejected_cartridges')
    audio_reset=re.search(r'    g_CasingSfxState = NULL;',audio).group(0)
    names=[(glass,n) for n in ('bullet_sparks_reset','bullet_moving_sparks_reset',
        'bullet_sparks_reset_all','bullet_sparks_update','bullet_moving_sparks_update',
        'bullet_sparks_update_all')]
    names += [(objective,n) for n in ('sub_GAME_7F057D44','sub_GAME_7F057D88')]
    names += [(gun,n) for n in ('update_bullet_casing','update_bullet_casings')]
    names += [(shards,'update_broken_windows'),(explosion,'explosionUpdateFlyingParticles'),
              (init,'initCasingPool')]
    # This closure is the current shipping US ABI. The original first
    # non-EU spark definitions are retained without changing their bodies.
    pieces=['''#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#ifndef PLAYERFLAG
typedef int PLAYERFLAG;
#endif
#include "game/player.h"
#include "game/bondview.h"
#include "game/gun.h"
#include "game/glass.h"
#include "game/explosion.h"
#include "game/chrai.h"
#include "game/matrixmath.h"
#include "random.h"
#include "snd.h"
#define BULLET_SPARKS_MAX 20
#define BULLET_MOVING_SPARKS_MAX 50
#if !defined(VERSION_US)
#error Effect runtime slice currently retains the verified US source closure
#endif
extern s_bullet_spark g_BulletSparkArray[20];
extern ALBank *g_musicSfxBufferPtr;
extern s_explosiontype g_ExplosionTypes[];
extern s_impacttype g_ImpactTypes[];
extern s32 g_ClockTimer;
extern f32 g_GlobalTimerDelta;
extern PropRecord **g_LastOnScreenProp, *g_OnScreenPropList[];
extern s32 getROOMID_isRendered(s32 room);
extern s32 bossGetStageNum(void);
static void ge_effect_sub_GAME_7F057D44(f32 *,f32 *,f32);
static void ge_effect_sub_GAME_7F057D88(f32 *,f32 *,f32);
''']
    for source,name in ((glass,'g_MovingBulletSparkArray'),
            ((repo/'src/game/gun.c').read_text(),'g_CasingSfxState'),
            (lv,'D_800483C0')):
        pieces.append(extract_data(source,name))
    pieces.append(scalar_declaration(gun,'g_CasingGravity'))
    for _,name in names:
        pieces.append('#define '+name+' ge_effect_'+name)
    for source,name in names:
        pieces.append(extract_function(source,name))
    allocation=extract_function((repo/'src/game/alloc_window_pieces.c').read_text(),
                                'alloc_shattered_window_pieces')
    allocation=allocation.replace('void alloc_shattered_window_pieces(void)',
        'void ge_original_effect_reset_shards_exact(void *storage, s32 count, s32 stage)')
    pieces += ['#define getPlayerCount() count', '#define lvlGetCurrentStageToLoad() stage',
        '#define mempAllocBytesInBank(bytes,bank) storage',allocation,
        '#undef getPlayerCount','#undef lvlGetCurrentStageToLoad','#undef mempAllocBytesInBank']
    er=extract_function(explosion,'explosionRenderPropExplosion')
    start=er.index('        temp_f10 = (s32)')
    retire=er[start:er.rindex('\n    }')]
    sr=extract_function(explosion,'explosionRenderPropSmoke')
    start=sr.index('    for (i = 0; i < SMOKE_PARTS_LEN; i++)')
    smoke=sr[start:sr.index('\n    gDPSetColorDither',start)]
    smoke=smoke.replace('gdl = explosionSmokeRenderPart(smoke, &smoke->parts[i], gdl, temp_s5);','(void)0;')
    pieces += ['''static void *ge_effect_render_context;
static int (*ge_effect_render_callback)(void *,void *,int);
static int ge_effect_render_failed;
static Gfx *ge_effect_retire_prop(Gfx *gdl, PropRecord *prop, s32 alpha)
{
    s32 i,temp_f10;
    if(ge_effect_render_failed)return gdl;
    if(ge_effect_render_callback && !ge_effect_render_callback(ge_effect_render_context,prop,alpha)) {
        ge_effect_render_failed=1;return gdl;
    }
    if(!alpha)return gdl;
    if(prop->type==PROP_TYPE_EXPLOSION) {
        struct Explosion *temp_s5=prop->explosion;
'''+retire+'''
    } else if(prop->type==PROP_TYPE_SMOKE) {
        struct Smoke *smoke=prop->smoke;
'''+smoke+'''
    }
    return gdl;
}
#define chrpropRender ge_effect_retire_prop
#define bgScissorCurrentPlayerViewDefault(gdl) (gdl)
''']
    render=extract_function((repo/'src/game/chrprop.c').read_text(),'chrpropsRenderPass')
    pieces.append(render.replace('chrpropsRenderPass(', 'ge_effect_retire_room(',1))
    impact=extract_function(explosion,'explosionRenderBulletImpactOnProp')
    impact=impact.replace('explosionRenderBulletImpactOnProp(', 'ge_effect_impact(',1)
    # Pure GPU command emission and texture/room-matrix publication have no
    # influence on the retained eligibility/retirement branch.
    impact=re.sub(r'\b(?:gSP\w+|gDP\w+|texSelect)\([^;]*;', '(void)0;', impact)
    impact=re.sub(r'gdl = applyRoomMatrixToDisplayList\([^;]*;', '(void)0;',impact)
    pieces.append(impact)
    pieces.append('''void ge_original_effect_reset_static_exact(void)
{
    bullet_sparks_reset_all();
    initCasingPool();
    D_800483C0 = 1;
}
void ge_original_effect_reset_audio_exact(void)
{
    /* init_ejected_cartridges, after both stage buffer allocations. */
'''+audio_reset+'''
}
void ge_original_effect_pre_world_exact(void)
{
    bullet_sparks_update_all();
    update_bullet_casings();
    update_broken_windows();
    explosionUpdateFlyingParticles();
}
int ge_original_effect_render_room_exact(s32 room,s32 pass,void *context,
    int (*callback)(void *,void *,int))
{
    /* Non-LEFTOVERDEBUG get_debug_do_draw_obj() is exactly 1. */
    ge_effect_render_context=context;ge_effect_render_callback=callback;
    ge_effect_render_failed=0;
    if(D_800483C0) (void)ge_effect_retire_room(NULL,room,pass);
    ge_effect_render_context=NULL;ge_effect_render_callback=NULL;
    return !ge_effect_render_failed;
}
void ge_original_effect_impact_semantic_exact(void *prop,int alpha)
{
    (void)ge_effect_impact(NULL,prop,alpha);
}
''')
    body='\n\n'.join(pieces)
    return '/* Retained effect closure SHA256 '+hashlib.sha256(body.encode()).hexdigest()+' */\n'+body+'\n'

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('repo',type=Path);p.add_argument('output',type=Path)
    args=p.parse_args();args.output.write_text(generate(args.repo))
