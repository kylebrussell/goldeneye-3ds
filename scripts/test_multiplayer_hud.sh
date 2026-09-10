#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=$(mktemp -d /tmp/ge-multiplayer-hud.XXXXXX)
trap 'rm -rf "${build_dir}"' EXIT
python3 "${repo_dir}/scripts/extract_dam_mission_hud_slice.py" "${repo_dir}" "${build_dir}/hud.c"
python3 "${repo_dir}/scripts/extract_gun_sight_slice.py" "${repo_dir}" "${build_dir}/sight.c"
if [[ $(uname -s) == Darwin ]]; then
    dead_strip=(-Wl,-dead_strip)
else
    dead_strip=(-Wl,--gc-sections)
fi
for abi in native short-enum; do
    abi_flags=(-fno-short-enums)
    if [[ ${abi} == short-enum ]]; then abi_flags=(-fshort-enums); fi
    cc "${abi_flags[@]}" -std=gnu11 -Wall -Wextra -Werror \
        -Wno-comment -Wno-unused-parameter -Wno-pragma-pack \
        -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA \
        -DAIPARSE -DVERSION_US -DPLAYERFLAG=int -DGE_PORT_MS_INHERITS -fms-extensions \
        -ffunction-sections -fdata-sections "${dead_strip[@]}" \
        -I "${repo_dir}/port/include" -idirafter "${repo_dir}/src" \
        -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
        "${build_dir}/hud.c" "${build_dir}/sight.c" \
        "${repo_dir}/port/src/ge_original_multiplayer_hud.c" \
        "${repo_dir}/port/src/ge_original_gun_sight.c" \
        "${repo_dir}/port/src/ge_gbi_decoder.c" \
        "${repo_dir}/port/src/ge_gbi_matrix.c" \
        "${repo_dir}/port/src/ge_gbi_rsp.c" \
        "${repo_dir}/port/src/ge_gbi_traverse.c" \
        "${repo_dir}/port/src/ge_gbi_state.c" \
        "${repo_dir}/port/src/ge_gbi_vertex.c" \
        "${repo_dir}/port/src/ge_gbi_pipeline.c" \
        "${repo_dir}/port/src/ge_pica_material.c" \
        "${repo_dir}/port/tests/test_ge_original_multiplayer_hud.c" \
        -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -lm \
        -o "${build_dir}/test_multiplayer_hud"
    ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "${build_dir}/test_multiplayer_hud"
done
