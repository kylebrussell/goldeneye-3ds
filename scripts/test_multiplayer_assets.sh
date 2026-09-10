#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir="${repo_dir}/build/host-tests/multiplayer-assets"
rom_path=${1:-"${repo_dir}/baserom.u.z64"}
mkdir -p "${build_dir}/empty-assets"
python3 "${repo_dir}/scripts/extract_3ds_multiplayer_stages.py" --rom "${rom_path}" --check
python3 "${repo_dir}/scripts/pack_3ds_assets.py" \
    --assets "${build_dir}/empty-assets" --rom "${rom_path}" \
    --extra-dir "converted/multiplayer=${repo_dir}/build/3ds-multiplayer" \
    --output "${build_dir}/multiplayer.gepack"
python3 "${repo_dir}/scripts/extract_multiplayer_rules_slice.py" "${repo_dir}" "${build_dir}/rules.c"
python3 "${repo_dir}/scripts/extract_multiplayer_rules_fixture.py" "${repo_dir}" "${build_dir}/support.c"
common=(-std=gnu11 -O1 -g -fno-omit-frame-pointer -ffunction-sections -fdata-sections -fsanitize=address,undefined)
cc "${common[@]}" -Wno-everything -DGE_PORT_BG_CONNECTIVITY_SLICE \
    -I"${repo_dir}/port/include" -I"${repo_dir}/src/game" \
    -c "${repo_dir}/src/game/bg.c" -o "${build_dir}/bg.o"
for source in stan stanintersection; do
    cc "${common[@]}" -Wno-everything -DGE_PORT_STAN_GEOMETRY_SLICE -DGE_PORT_BOND_MOVEMENT_SLICE \
        -I"${repo_dir}/port/include" -c "${repo_dir}/src/game/${source}.c" -o "${build_dir}/${source}.o"
done
cc "${common[@]}" -Wno-everything \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -DGE_PORT_MS_INHERITS -fms-extensions -DVERSION_US -DPLAYERFLAG=int \
    -I"${repo_dir}/port/include" -I"${repo_dir}/src" -I"${repo_dir}/src/game" -I"${repo_dir}" \
    -I"${repo_dir}/include/PR" -idirafter "${repo_dir}/include" \
    "${repo_dir}/port/tests/test_ge_multiplayer_assets.c" \
    "${repo_dir}/port/src/ge_stage_assets.c" "${repo_dir}/port/src/ge_stage_asset_resolver.c" \
    "${repo_dir}/port/src/ge_original_stage_setup.c" \
    "${repo_dir}/port/src/ge_asset_pack.c" "${repo_dir}/port/src/ge_dam_world.c" \
    "${repo_dir}/port/src/ge_stan_collision.c" "${repo_dir}/port/src/ge_stan_native.c" \
    "${repo_dir}/port/src/ge_original_multiplayer_rules.c" "${repo_dir}/src/game/mp_weapon.c" \
    "${build_dir}/rules.c" "${build_dir}/support.c" \
    "${build_dir}/bg.o" "${build_dir}/stan.o" "${build_dir}/stanintersection.o" \
    -lm -o "${build_dir}/test_multiplayer_assets"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "${build_dir}/test_multiplayer_assets" "${build_dir}/multiplayer.gepack"
