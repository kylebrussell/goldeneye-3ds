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
out.joinpath('support.c').write_text('\n'.join(pieces))
# The actual getter reads the separately namespaced native BG publication.
out.joinpath('visibility.c').write_text('''#include "ge_original_bg_visibility_internal.h"
extern s32 g_BgNumberOfRoomsDrawn;
extern s_bound_info dword_CODE_bss_8007FFA0[204];
void fixture_rooms(void) {
 g_BgNumberOfRoomsDrawn=3;
 dword_CODE_bss_8007FFA0[0].roomid=1;dword_CODE_bss_8007FFA0[0].unk1=257;
 dword_CODE_bss_8007FFA0[1].roomid=2;dword_CODE_bss_8007FFA0[1].unk1=-2;
 dword_CODE_bss_8007FFA0[2].roomid=3;dword_CODE_bss_8007FFA0[2].unk1=257;
}
''')
