#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
python3 "${repo_dir}/scripts/tests/test_multiplayer_music_boundary.py"
test_dir=$(mktemp -d /tmp/ge-state-music.XXXXXX)
trap 'rm -rf "${test_dir}"' EXIT
python3 - "${repo_dir}" "${test_dir}/music.c" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1]);sys.path.insert(0,str(root/'scripts'))
from extract_bond_move_non_tank_slice import generate,extract_function
source=(root/'src/game/mp_music.c').read_text()
fixed=extract_function(generate(root),'sub_GAME_7F0C0BF0')
source=source.replace(extract_function(source,'sub_GAME_7F0C0BF0'),fixed)
Path(sys.argv[2]).write_text(source)
Path(sys.argv[2]).with_name('tables.c').write_text(
    '#include "ge_mp_state_music.h"\n#include "'+str(root/'src/game/music_0D2720.c')+'"\n'+
    '_Static_assert(sizeof(music_setup_entries)/sizeof(music_setup_entries[0]) == GE_MP_MUSIC_SETUP_ROWS,"review music setup rows");\n'+
    '_Static_assert(sizeof(random_tracks)/sizeof(random_tracks[0]) == GE_MP_MUSIC_RANDOM_TRACKS,"review random tracks");\n')
PY
for abi in native short; do
    enum=(-fno-short-enums); if [[ $abi == short ]]; then enum=(-fshort-enums); fi
    cc "${enum[@]}" -std=gnu11 -O1 -g -fno-omit-frame-pointer \
        -fsanitize=address,undefined -fno-sanitize-recover=all \
        -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DVERSION_US \
        -DAIPARSE -DGE_PORT_MS_INHERITS -DPLAYERFLAG=int -fms-extensions -Wno-everything \
        -I"${repo_dir}/port/include" -I"${repo_dir}/src/game" \
        -idirafter "${repo_dir}/src" -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
        "${test_dir}/music.c" "${test_dir}/tables.c" \
        "${repo_dir}/port/src/random_port.c" "${repo_dir}/port/src/ge_mp_state.c" \
        "${repo_dir}/port/src/ge_mp_state_music.c" "${repo_dir}/port/tests/test_ge_mp_state_music.c" \
        -lm -o "${test_dir}/test-${abi}"
    for count in 2 4; do
        for peer in 1 2; do
            ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
                "${test_dir}/test-${abi}" "$count" > "${test_dir}/${abi}-${count}-${peer}.log"
        done
        cmp "${test_dir}/${abi}-${count}-1.log" "${test_dir}/${abi}-${count}-2.log"
        cat "${test_dir}/${abi}-${count}-1.log"
    done
done
for count in 2 4; do cmp "${test_dir}/native-${count}-1.log" "${test_dir}/short-${count}-1.log"; done
