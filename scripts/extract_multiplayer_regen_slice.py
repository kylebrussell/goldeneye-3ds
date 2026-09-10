#!/usr/bin/env python3
"""Retain canonical ordinary-root pickup regeneration; reject other owners in C."""
from pathlib import Path
import argparse
import hashlib
from extract_stage_pickup_slice import function_text


def block(source, marker):
    start = source.index(marker)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end], source[brace+1:end-1]


def render(repo):
    source = (repo / 'src/game/chrprop.c').read_text()
    tick = function_text(source, 'chrpropTick')
    regen, _ = block(tick, 'if (prop->timetoregen > 0)')
    # The public adapter rejects embedded and destroyed records before mutation.
    # Select the original ordinary object arm, retaining the original timer,
    # near-player decision, armour refill, and state restoration unchanged.
    owner, _ = block(regen, 'if ((obj->maxdamage == 0.0f)')
    destroy_start = regen.index(owner) + len(owner)
    destroyed, _ = block(regen[destroy_start:], 'else')
    root_start = owner.index('chrpropEnable(prop);')
    root_end = owner.index('\n                        }', root_start)
    root = owner[root_start:root_end]
    regen = regen.replace(owner + regen[destroy_start:destroy_start + regen[destroy_start:].index('else')] + destroyed, root)
    regen = regen.replace('chrobjSndCreatePostEventDefault(sndPlaySfx(g_musicSfxBufferPtr, OBJ_REGEN_SFX, NULL), &prop->pos);',
                          'providers->play_regen_sound(providers->context, prop->pos.f);')
    far = function_text(source, 'chrpropIsFarFromPlayers')
    return '''/* Generated selected original chrprop.c pickup lifecycle. */
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <math.h>
#include "game/player.h"
#include "game/propobj.h"
#include "ge_original_multiplayer_regen.h"
#define chrpropEnable(prop) providers->enable_prop(providers->context, prop)
#define sub_GAME_7F03E134(prop) providers->update_rooms(providers->context, prop)
#define chrpropIsFarFromPlayers ge_original_mp_regen_far_exact
''' + far + '''
void ge_original_mp_regen_tick_exact(PropRecord *prop, s32 g_ClockTimer,
    const GeOriginalMultiplayerRegenProviders *providers)
{
    ObjectRecord *obj = prop->obj;
    bool is_under_60, skip_regen_sfx = FALSE;
''' + regen + '\n}\n/* chrpropTick sha256 ' + hashlib.sha256(tick.encode()).hexdigest() + ' */\n'


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('repo', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--fixture', type=Path)
    a = p.parse_args()
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(render(a.repo))

    if a.fixture:
        source = (a.repo / 'src/game/chrprop.c').read_text()
        a.fixture.write_text("""#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include \"game/chrai.h\"
#include \"game/player.h\"
#include \"game/propobj.h\"
#define RUNTIMEBITFLAG_REMOVE (1u << 2)
extern void objDetach(PropRecord *);
extern void objFreeEmbedmentOrProjectile(PropRecord *);
extern PropRecord *getCurrentPlayerProp(void);
""" + '\n'.join(function_text(source,n) for n in
        ('chrpropEnable','chrpropDisable','propExecuteTickOperation')))
