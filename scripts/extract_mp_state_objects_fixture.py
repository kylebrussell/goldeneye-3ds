#!/usr/bin/env python3
"""Object-state test: native owners/mutations, selected source pickup/regen paths.

No production gameplay substitute is emitted. The fixture preserves full native
free/drop/allocator bodies. Its named armor-pickup and regen helpers retain
source statement blocks; UI, other pickup variants, and the surrounding actor
scheduler are outside this bounded fixture. Model/room/audio resources are
explicit test dependencies, not replacements for object lifecycle operations.
"""
from pathlib import Path
import sys
repo=Path(sys.argv[1]);out=Path(sys.argv[2]);sys.path.insert(0,str(repo/'scripts'))
from extract_bond_move_explosion_slice import extract_data,extract_function,generate
from extract_bg_visibility_slice import matching_brace
prop=(repo/'src/game/propobj.c').read_text()
chrprop=(repo/'src/game/chrprop.c').read_text()
def block(source,marker):
    start=source.index(marker);end=matching_brace(source,source.index('{',start))
    return source[start:end+1]
pieces=['''#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "ge_original_prop_lifetime.h"
#include "game/player.h"
#include "game/chrai.h"
#include "game/propobj.h"
#include "game/model.h"
#include "game/bondview.h"
#include "game/explosion.h"
#include "snd.h"
#include "game/loadobjectmodel.h"
#include <assert.h>
#include <math.h>
extern PropRecord *g_FreeProps,*g_ActivePropsHead,*g_ActivePropsTail;
extern s32 g_ClockTimer;
extern void *g_musicSfxBufferPtr;
#define M_U16_MAX_VALUE_F 65536.0f
/* Original BITFLAG positions; AIPARSE suppresses the enum aliases. */
#define RUNTIMEBITFLAG_REMOVE (1u << 2)
#define RUNTIMEBITFLAG_TAGGED (1u << 4)
#define RUNTIMEBITFLAG_EMBEDDED (1u << 6)
#define RUNTIMEBITFLAG_HASPROJECTILE (1u << 7)
void objDetach(PropRecord *);
void objFreePermanently(ObjectRecord *,bool);
void sub_GAME_7F050DE8(Model *);
void sub_GAME_7F03E134(PropRecord *);
void chrobjSndCreatePostEventDefault(ALSoundState *,coord3d *);
void fixture_room_position(ObjectRecord *);
''']
for name in ['g_AmmoCrates','g_Projectiles']:
    pieces.append(extract_data(chrprop,name))
explosion=(repo/'src/game/explosion.c').read_text()
pieces.append(extract_data(explosion,'g_BulletImpactBuffer'))
pieces.append(extract_function(explosion,'explosionClearBulletImpactRoomByFlag'))
pieces.append(extract_function(generate(repo),'chrpropFree'))
for name in ['chrpropDelist','chrpropDisable','chrpropReparent','chrpropDetach',
             'propExecuteTickOperation','chrpropIsFarFromPlayers']:
    pieces.append(extract_function(chrprop,name))
for name in ['projectileFree','projectileReset','embedmentFree',
             'objFreeEmbedmentOrProjectile','projectileAllocate','sub_GAME_7F03FDA8',
             'propobjSetDropped','objDetach','objFree','objFreePermanently',
             'ammocrateAllocate','sub_GAME_7F050DE8','sub_GAME_7F04F244']:
    pieces.append(extract_function(prop,name))
pieces.append(extract_function((repo/'src/game/model.c').read_text(),'clear_model_obj'))
pieces.append(extract_function((repo/'src/game/bondview2.c').read_text(),'bondviewAddCurrentPlayerArmor'))
# Authoritative input: exact authored multiplayer Temple setup words.
setupsource=(repo/'assets/obseg/setup/Ump_setupdishZ.c').read_text()
setup=extract_data(setupsource[setupsource.index('s32 propDefs[] ='):],'propDefs')
pieces.append(setup.replace('propDefs','fixture_authored_words',1))
pieces.append('size_t fixture_authored_count(void) { return sizeof(fixture_authored_words)/sizeof(fixture_authored_words[0]); }')
# Native setup's fixed-point armor promotion statements, before resource setup.
setupsource=(repo/'src/game/prop.c').read_text()
start=setupsource.index('pdef_ba->initialamount =')
end=setupsource.index('domakedefaultobj(',start)
pieces.append('void fixture_armour_setup(BodyArmourRecord *pdef_ba) {\n'+setupsource[start:end]+'\n}')
# Retain guards + exact armor gameplay statements + native objFree dispatch.
pickup=extract_function(prop,'propPickupByPlayer')
prefix=pickup[:pickup.index('switch (obj->type)')]
prefix=prefix.replace('propPickupByPlayer','fixture_armour_pickup',1)
armor=block(pickup,'case PROPDEF_ARMOUR:')
armor=armor[armor.index('{')+1:armor.rfind('}')]
show=block(armor,'if (showstring)')
armor=armor.replace(show,'assert(!showstring);').replace('break;','')
tail=block(pickup,'if ((op == TICKOP_FREE)')
pieces.append(prefix+'assert(obj->type==PROPDEF_ARMOUR);\n'+armor+'\n'+tail+'\nassert(0); return TICKOP_NONE;\n}')
# Whole original object-regeneration conditional, including proximity gate,
# collision/destruction branches and armor refill. No forced completion bits.
regen=extract_function(chrprop,'chrpropTick')
pieces.append('''void fixture_object_regen(PropRecord *prop) {
 ObjectRecord *obj=prop->obj,*setupobj;
 bool skip_regen_sfx=FALSE,is_under_60;
 s32 cmdindex,pad;
'''+block(regen,'if (prop->timetoregen > 0)')+'\n}')
out.write_text('\n'.join(pieces))
