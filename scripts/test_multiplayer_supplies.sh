#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir="${repo_dir}/build/host-tests/multiplayer-supplies"
rom_path=${1:-"${repo_dir}/baserom.u.z64"}
mkdir -p "${build_dir}/empty-assets"
python3 "${repo_dir}/scripts/extract_3ds_multiplayer_stages.py" --rom "${rom_path}" --check
python3 "${repo_dir}/scripts/pack_3ds_assets.py" --assets "${build_dir}/empty-assets" --rom "${rom_path}" \
    --extra-dir "converted/multiplayer=${repo_dir}/build/3ds-multiplayer" \
    --extra-dir "converted/models/pitem=${repo_dir}/build/3ds-models/pitem" \
    --output "${build_dir}/supplies.gepack"
python3 "${repo_dir}/scripts/generate_move_model_tables.py" "${repo_dir}" "${build_dir}/tables.c"
python3 "${repo_dir}/scripts/extract_multiplayer_rules_slice.py" "${repo_dir}" "${build_dir}/rules.c"
python3 "${repo_dir}/scripts/extract_stage_guard_actor_slice.py" "${repo_dir}" "${build_dir}/actor.c"
common=(-std=gnu11 -O1 -g -fno-omit-frame-pointer -ffunction-sections -fdata-sections -fsanitize=address,undefined)
if [[ "$(uname -s)" == Darwin ]]; then strip=(-Wl,-dead_strip); else strip=(-Wl,--gc-sections); fi
for source in stan stanintersection; do
    cc "${common[@]}" -Wno-everything -DGE_PORT_STAN_GEOMETRY_SLICE -DGE_PORT_BOND_MOVEMENT_SLICE \
        -I"${repo_dir}/port/include" -c "${repo_dir}/src/game/${source}.c" -o "${build_dir}/${source}.o"
done
sources=(
    port/tests/test_ge_original_multiplayer_supplies.c
    port/tests/ge_original_pitem_model_test_support.c
    port/src/ge_original_pitem_models.c
    port/src/ge_original_stage_setup.c
    port/src/ge_original_stage_prop_materializer.c
    port/src/ge_original_stage_prop_native.c
    port/src/ge_original_stage_items.c
    port/src/ge_original_stage_guard_actor.c
    port/src/ge_original_stage_supplies.c
    port/src/ge_original_stage_multiplayer_setup.c
    port/src/ge_original_default_object.c
    port/src/ge_original_default_object_source.c
    port/src/ge_original_getposstan_source.c
    port/src/ge_original_objinit_source.c
    port/src/ge_original_prop_state_source.c
    port/src/ge_original_multiplayer_rules.c
    port/src/ge_stage_assets.c
    port/src/ge_asset_pack.c
    port/src/ge_stan_collision.c
    port/src/ge_stan_native.c
    src/game/matrixmath.c
    src/game/mp_weapon.c
)
for i in "${!sources[@]}"; do sources[i]="${repo_dir}/${sources[i]}"; done
cc "${common[@]}" -Wno-everything -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -DVERSION_US -DPLAYERFLAG=int -fms-extensions \
    -I"${repo_dir}" -I"${repo_dir}/port/include" -I"${repo_dir}/src/game" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    "${sources[@]}" "${build_dir}/tables.c" "${build_dir}/rules.c" "${build_dir}/actor.c" \
    "${build_dir}/stan.o" "${build_dir}/stanintersection.o" -lm "${strip[@]}" \
    -o "${build_dir}/test_multiplayer_supplies"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "${build_dir}/test_multiplayer_supplies" "${build_dir}/supplies.gepack"
