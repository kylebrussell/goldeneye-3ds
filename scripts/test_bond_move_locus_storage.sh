#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d /tmp/ge-move-locus.XXXXXX)
trap 'rm -rf "${test_dir}"' EXIT
python3 - "${repo_dir}" "${test_dir}" <<'PY'
from pathlib import Path
import re, sys
root, out = map(Path, sys.argv[1:])
sys.path.insert(0, str(root / 'scripts'))
from extract_bond_move_runtime_slice import extract_function
move = extract_function((root / 'src/game/bondview2.c').read_text(), 'MoveBond')
start = move.index('#if defined(GE_PORT_BOND_MOVEMENT_SLICE) || defined(GE_PORT_EXACT_MOVEBOND_FRONTIER)')
end = move.index('#endif', start) + len('#endif')
declaration = move[start:end]
stan = (root / 'src/game/stan.c').read_text()
prefix = '''#include "ge_original_stan_slice.h"
#include <assert.h>
#include <stdio.h>
/* Only the traversal is a seam: this regression executes the original
 * 64-byte clear against the exact local declaration from MoveBond. */
static s32 stanIsSpecialBit1Set(StandTile *tile, struct StandTileLocusCallbackRecord *record)
{ (void)tile; (void)record; return 0; }
static s32 stanCheckLinkedSpecialTileHardFloat(StandTile *tile,s32 p,f32 a,f32 b,f32 c,
    struct StandTileLocusCallbackRecord *record)
{ (void)tile; (void)p; (void)a; (void)b; (void)c; (void)record; return 0; }
static s32 sub_GAME_7F0B1DDC(StandTile **tile,f32 x,f32 z,f32 radius,
    standTileLocusCallback_A_t a,standTileLocusCallback_B_t b,standTileLocusCallback_C_t c,
    struct StandTileLocusCallbackRecord *record)
{
    (void)tile; (void)x; (void)z; (void)radius; (void)a; (void)b; (void)c;
    for (unsigned i=0;i<64;++i) assert(((unsigned char*)record)[i]==0);
    record->rooms=(s32*)(uintptr_t)1;record->count=7;return STAN_COLLISION_NONE;
}
struct move_bond_temp_struct { s32 unk00,unk04; };
'''
functions = '\n'.join(extract_function(stan, name) for name in ('stanTileDistanceRelated','stanGetLocusCount'))
body = '''
int main(void) {
DECLARATION
    StandTile *tile=NULL;
    assert(stanTileDistanceRelated(&tile,0,0,30,(struct StandTileLocusCallbackRecord*)&curLocus)==STAN_COLLISION_NONE);
    assert(stanGetLocusCount((struct StandTileLocusCallbackRecord*)&curLocus)==7);
    puts("MoveBond native locus scratch survives original 64-byte clear and typed callback");
    return 0;
}
'''
(out/'valid.c').write_text(prefix+functions+body.replace('DECLARATION',declaration))
(out/'old.c').write_text(prefix+functions+body.replace('DECLARATION','struct move_bond_temp_struct curLocus;'))
PY
flags=(-std=gnu11 -O1 -g -Wall -Wextra -Werror -fno-omit-frame-pointer
    -fsanitize=address,undefined -DGE_PORT_STAN_GEOMETRY_SLICE
    -DGE_PORT_EXACT_MOVEBOND_FRONTIER -I"${repo_dir}/port/include")
for abi in native short; do
    enum=(-fno-short-enums); if [[ $abi == short ]]; then enum=(-fshort-enums); fi
    cc "${flags[@]}" "${enum[@]}" "${test_dir}/valid.c" -o "${test_dir}/valid"
    "${test_dir}/valid"
done
cc "${flags[@]}" "${test_dir}/old.c" -o "${test_dir}/old"
if "${test_dir}/old" >"${test_dir}/negative.log" 2>&1; then
    echo "Old undersized locus declaration unexpectedly survived" >&2
    exit 1
fi
rg -q 'AddressSanitizer: stack-buffer-overflow' "${test_dir}/negative.log"
echo "Original undersized declaration fails the same sanitizer boundary"
