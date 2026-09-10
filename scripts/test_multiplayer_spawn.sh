#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir="${repo_dir}/build/host-tests/multiplayer-spawn"
pack=${1:-"${repo_dir}/build/host-tests/multiplayer-assets/multiplayer.gepack"}
mkdir -p "${build_dir}"
python3 "${repo_dir}/scripts/extract_multiplayer_players_slice.py" "${repo_dir}" "${build_dir}/players.c"
python3 "${repo_dir}/scripts/extract_multiplayer_spawn_fixture.py" "${repo_dir}" "${build_dir}/support.c"
python3 "${repo_dir}/scripts/extract_multiplayer_rules_slice.py" "${repo_dir}" "${build_dir}/rules.c"
flags=(-std=gnu11 -O1 -g -fno-omit-frame-pointer -ffunction-sections -fdata-sections -fsanitize=address,undefined
-D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA
-DGE_PORT_MS_INHERITS -DVERSION_US -DPLAYERFLAG=int -fms-extensions -Wno-everything
-I"${repo_dir}" -I"${repo_dir}/port/include" -I"${repo_dir}/src/game"
-idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src")
cc "${flags[@]}" -UPLAYERFLAG -DGE_PORT_BOND_INTRO_SPAWN_SLICE -c "${repo_dir}/src/game/bondview_r.c" -o "${build_dir}/spawn.o"
for source in stan stanintersection; do
    cc -std=gnu11 -O1 -g -fsanitize=address,undefined -Wno-everything -DGE_PORT_STAN_GEOMETRY_SLICE -DGE_PORT_BOND_MOVEMENT_SLICE \
        -I"${repo_dir}/port/include" -c "${repo_dir}/src/game/${source}.c" -o "${build_dir}/${source}.o"
done
sources=(
    port/src/ge_original_multiplayer_players.c port/src/ge_original_multiplayer_spawn.c
    port/src/ge_original_multiplayer_rules.c port/src/ge_original_player_spawn.c
    port/src/ge_original_bond_input_provider.c port/src/ge_original_input.c
    port/src/ge_libultra.c port/src/ge_services.c port/src/random_port.c src/joy.c
    port/src/ge_original_chr_obj_random.c port/src/ge_original_prop_state.c
    port/src/ge_original_prop_state_source.c port/src/ge_original_stage_setup.c
    port/src/ge_stage_assets.c port/src/ge_asset_pack.c port/src/ge_stan_collision.c
    port/src/ge_stan_native.c src/game/mp_weapon.c port/tests/test_ge_original_multiplayer_spawn.c
)
for i in "${!sources[@]}"; do sources[i]="${repo_dir}/${sources[i]}"; done
if [[ $(uname -s) == Darwin ]]; then strip=(-Wl,-dead_strip); else strip=(-Wl,--gc-sections); fi
cc "${flags[@]}" "${sources[@]}" "${build_dir}/players.c" "${build_dir}/support.c" "${build_dir}/rules.c" \
    "${build_dir}/spawn.o" "${build_dir}/stan.o" "${build_dir}/stanintersection.o" "${strip[@]}" -lm -o "${build_dir}/test"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "${build_dir}/test" "${pack}"
