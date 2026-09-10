#!/usr/bin/env python3
"""Native constructors/storage plus explicit audio/room resource fixture."""
from pathlib import Path
import sys
repo=Path(sys.argv[1]);out=Path(sys.argv[2]);sys.path.insert(0,str(repo/'scripts'))
from extract_bond_move_explosion_slice import extract_data,extract_function
explosion=(repo/'src/game/explosion.c').read_text();glass=(repo/'src/game/glass.c').read_text()
spark=(repo/'src/game/glass2.c').read_text();gun=(repo/'src/game/gun.c').read_text()
pieces=['''#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <limits.h>
#include <math.h>
typedef int PLAYERFLAG;
#include "game/glass.h"
#include "game/explosion.h"
#include "game/gun.h"
#include "game/image_bank.h"
#include "random.h"
#define BULLET_SPARKS_MAX 20
#define M_U32_MAX_VALUE_F 4294967296.0f
''']
pieces.append(glass[glass.index('#ifndef VERSION_EU'):glass.index('#define BULLET_SPARKS_MAX')])
for src,names in [(explosion,['g_ExplosionBuffer','g_SmokeBuffer','g_FlyingParticlesBuffer',
    'g_BulletImpactBuffer','g_NumExplosionEntries','g_NumSmokeEntries','g_NumParticleEntries',
    'max_particles','g_ExplosionTypes','g_ImpactTypes']),
    (glass,['SHATTERED_WINDOW_PIECES_BUFFER_LEN','ptr_shattered_window_pieces','g_NextShardNum']),
    (spark,['g_BulletSparkColors','g_BulletSparkArray']), (gun,['g_Casings'])]:
    for name in names:pieces.append(extract_data(src,name))
images=(repo/'src/game/image_bank.c').read_text()
for name in ['flareimage2','explosion_smokeimages','scattered_explosions']:
    pieces.append(extract_data(images,name))
for src,names in [(glass,['glassCreateShard']),(spark,['bullet_sparks_init','bullet_spark_create']),
                  (explosion,['explosionInitFlyingParticles'])]:
    for name in names:pieces.append(extract_function(src,name))
pieces.append(extract_function((repo/'src/game/propobj.c').read_text(),
    'projectileReset').replace('projectileReset(', 'fixture_projectile_reset(',1))
# Actual original gun producer statements, including their fallback gate.
# The surrounding STAN/camera/model/resource setup has already selected obj.
from extract_bg_visibility_slice import matching_brace
producer=extract_function(gun,'gunInitProjectileFromPlayer')
start=producer.index('if (usedfallback) {')
end=matching_brace(producer,producer.index('{',start))
pieces.append('void fixture_gun_projectile_fallback(ObjectRecord *obj, coord3d *targetpos, int usedfallback)\n{\n'
    +producer[start:end+1]+'\n}')
out.joinpath('support.c').write_text('\n'.join(pieces))
# The actual getter reads the separately namespaced native BG publication.
out.joinpath('visibility.c').write_text('''#include "ge_original_bg_visibility_internal.h"
extern s32 g_BgNumberOfRoomsDrawn;
extern s_bound_info dword_CODE_bss_8007FFA0[204];
void fixture_rooms(void) {
 for(unsigned i=0;i<MAXROOMCOUNT;++i)
  g_BgRoomInfo[i].room_rendered=(i>=1&&i<=3)||i==35;
 g_BgNumberOfRoomsDrawn=3;
 dword_CODE_bss_8007FFA0[0].roomid=1;dword_CODE_bss_8007FFA0[0].unk1=257;
 dword_CODE_bss_8007FFA0[1].roomid=2;dword_CODE_bss_8007FFA0[1].unk1=-2;
 dword_CODE_bss_8007FFA0[2].roomid=3;dword_CODE_bss_8007FFA0[2].unk1=257;
}
''')
# Independent original prefix + original depth loops as a differential oracle.
# GPU/resource operations are removed, while the full source control flow and
# both original room/depth loops remain. Its prop boundary records pass0/1;
# pass2 has no eligible props in this fixture.
import re
bg=(repo/'src/game/bg.c').read_text()
order=extract_function(bg,'sub_GAME_7F0B3C8C')
order=re.sub(r'\bgSP\w+\([^;]*;', '(void)0;',order)
def strip_graphics(match):
    return match.group(0) if 'chrpropsRenderPass(' in match.group(0) else '(void)0;'
order=re.sub(r'\bgdl\s*=[^;]*;',strip_graphics,order)
prefix=extract_function(bg,'sub_GAME_7F0B8D78')
levels=re.findall(r'\{\s*(LEVELID_\w+)\s*,',extract_data(bg,'levelinfotable'))
mp=extract_data((repo/'src/game/front.c').read_text(),'multi_stage_setups')
mp=re.sub(r'/\*.*?\*/|//[^\n]*','',mp,flags=re.S)
arenas=[x for x in re.findall(r'\bLEVELID_\w+',mp) if x!='LEVELID_NONE']
assert len(arenas)==11 and levels.index('LEVELID_DAM')==13
indices=[levels.index(x) for x in arenas]
out.joinpath('prefix_reference.c').write_text('''#include "ge_original_bg_visibility_internal.h"
typedef void Gfx;
static unsigned observed[32],observed_count;
#define get_debug_do_draw_obj() 1
#define get_debug_do_draw_bg() 1
#define sub_GAME_7F0BD8F0() 1
#define bgScissorCurrentPlayerViewDefault(gdl) (gdl)
static Gfx *chrpropsRenderPass(Gfx *gdl,s32 room,s32 pass) {
 if(pass!=2)observed[observed_count++]=(unsigned)(room*10+pass);
 return gdl;
}
'''+order+'\n'+prefix+'''
void fixture_render_rooms(int index) {
 levelentry_index=index;g_BgNumberOfRoomsDrawn=2;
 dword_CODE_bss_8007FFA0[0].roomid=35;dword_CODE_bss_8007FFA0[0].unk1=5;
 dword_CODE_bss_8007FFA0[1].roomid=1;dword_CODE_bss_8007FFA0[1].unk1=1;
}
void fixture_original_render(unsigned *sequence,size_t *count) {
 observed_count=0;sub_GAME_7F0B8D78(NULL);*count=observed_count;
 for(unsigned i=0;i<observed_count;++i)sequence[i]=observed[i];
}
int fixture_arena_index(unsigned slot) {
 static const int indices[]={'''+','.join(map(str,indices))+'''};
 return slot<11?indices[slot]:-1;
}
''')
