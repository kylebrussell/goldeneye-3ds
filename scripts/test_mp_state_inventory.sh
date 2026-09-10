#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir="${repo_dir}/build/host-tests/mp-state-inventory"
mkdir -p "${test_dir}"
python3 "${repo_dir}/scripts/extract_multiplayer_players_slice.py" "${repo_dir}" "${test_dir}/players.c"
python3 "${repo_dir}/scripts/extract_mp_state_inventory_fixture.py" "${repo_dir}" "${test_dir}/support.c"
flags=(-std=gnu11 -O1 -g -fno-omit-frame-pointer -ffunction-sections -fdata-sections -fsanitize=address,undefined
-D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA
-DGE_PORT_MS_INHERITS -DVERSION_US -DPLAYERFLAG=int -fms-extensions -Wno-everything
-I"${repo_dir}" -I"${repo_dir}/port/include" -I"${repo_dir}/src/game"
-idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src")
cc "${flags[@]}" -UPLAYERFLAG -DGE_PORT_BOND_INTRO_SPAWN_SLICE -c "${repo_dir}/src/game/bondview_r.c" -o "${test_dir}/spawn.o"
sources=(port/src/ge_mp_state_inventory.c port/src/ge_mp_state.c
port/src/ge_original_multiplayer_players.c port/src/ge_original_multiplayer_rules.c
port/src/ge_original_bond_input_provider.c port/src/ge_original_input.c
port/src/ge_libultra.c port/src/ge_services.c port/src/random_port.c src/joy.c
port/src/ge_original_chr_obj_random.c port/tests/test_ge_mp_state_inventory.c)
for i in "${!sources[@]}"; do sources[i]="${repo_dir}/${sources[i]}"; done
if [[ $(uname -s) == Darwin ]]; then strip=(-Wl,-dead_strip); else strip=(-Wl,--gc-sections); fi
cc "${flags[@]}" "${sources[@]}" "${test_dir}/players.c" "${test_dir}/support.c" \
    "${test_dir}/spawn.o" "${strip[@]}" -lm -o "${test_dir}/test"
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "${test_dir}/test"
