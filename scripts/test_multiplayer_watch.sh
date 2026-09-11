#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=${GE_WATCH_TEST_DIR:-"${repo_dir}/build/host-tests/multiplayer-watch"}
mkdir -p "${test_dir}"
python3 "${repo_dir}/scripts/extract_multiplayer_players_slice.py" "${repo_dir}" "${test_dir}/players.c"
python3 "${repo_dir}/scripts/extract_mp_state_session_fixture.py" "${repo_dir}" "${test_dir}/support.c"
python3 "${repo_dir}/scripts/extract_multiplayer_session_slice.py" "${repo_dir}" "${test_dir}/session.c"
python3 "${repo_dir}/scripts/extract_multiplayer_death_slice.py" "${repo_dir}" "${test_dir}/death.c"
python3 "${repo_dir}/scripts/extract_multiplayer_watch_slice.py" "${repo_dir}" "${test_dir}/watch.c"
python3 "${repo_dir}/scripts/extract_multiplayer_watch_fixture.py" "${repo_dir}" "${test_dir}/watch-support.c"
flags=(-std=gnu11 -O1 -g -fno-omit-frame-pointer -ffunction-sections -fdata-sections -fsanitize=address,undefined
-D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA
-DGE_PORT_MS_INHERITS -DVERSION_US -DPLAYERFLAG=int -fms-extensions -Wno-everything
-DGE_PORT_BLOOD_DECODE_SLICE -DGE_MP_WATCH_REAL_SFX
-I"${repo_dir}" -I"${repo_dir}/port/include" -I"${repo_dir}/src/game"
-idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src")
sources=(port/src/ge_original_multiplayer_watch.c port/tests/multiplayer_watch_audio_resources.c src/game/music_0D2720.c src/game/frametiming.c
port/src/ge_original_sfx_bank.c port/src/ge_audio_output.c
port/src/ge_mp_state_session.c port/src/ge_mp_state_players.c port/src/ge_mp_state.c
port/src/ge_original_stage_active_props.c
port/src/ge_original_multiplayer_hud.c
port/src/ge_original_multiplayer_players.c port/src/ge_original_multiplayer_rules.c
port/src/ge_original_multiplayer_session.c port/src/ge_original_multiplayer_death.c
port/src/ge_original_gun_frame_arena.c src/game/blood_animation.c src/game/blood_decrypt.c src/game/mp_weapon.c
port/src/ge_original_bond_input_provider.c port/src/ge_original_input.c
port/src/ge_libultra.c port/src/ge_services.c port/src/random_port.c src/joy.c
port/src/ge_original_chr_obj_random.c)
sources+=(port/src/ge_original_language_text.c)
for bank in LarchE LarkE LaztE LcaveE LarecE LcradE LcrypE LdamE LdepoE LdestE LjunE LlenE LpeteE LrunE LsevE LsevbE LsevxE LsevxbE LsiloE LstatE LtraE LgunE LmiscE LoptionsE LpropobjE LtitleE LmpweaponsE LmpmenuE; do
    sources+=("assets/obseg/text/${bank}.c")
done
for i in "${!sources[@]}"; do sources[i]="${repo_dir}/${sources[i]}"; done
if [[ $(uname -s) == Darwin ]]; then strip=(-Wl,-dead_strip); else strip=(-Wl,--gc-sections); fi
for abi in native short; do
    enum=(-fno-short-enums);if [[ $abi == short ]]; then enum=(-fshort-enums); fi
    cc "${flags[@]}" "${enum[@]}" -UPLAYERFLAG -DGE_PORT_BOND_INTRO_SPAWN_SLICE -c "${repo_dir}/src/game/bondview_r.c" -o "${test_dir}/spawn-${abi}.o"
    cc "${flags[@]}" "${enum[@]}" -UPLAYERFLAG -c "${repo_dir}/port/src/ge_original_gameplay_services.c" -o "${test_dir}/services-${abi}.o"
    for fixture in watch; do
    cc "${flags[@]}" "${enum[@]}" "${sources[@]}" "${repo_dir}/port/tests/test_ge_original_multiplayer_${fixture}.c" "${test_dir}/players.c" "${test_dir}/support.c" \
        "${test_dir}/services-${abi}.o" "${test_dir}/session.c" "${test_dir}/watch.c" "${test_dir}/watch-support.c" "${test_dir}/death.c" "${test_dir}/spawn-${abi}.o" "${strip[@]}" -lm -o "${test_dir}/test-${abi}"
    for count in 2 4; do
        for peer in 0 1; do
            ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
                "${test_dir}/test-${abi}" "$count" "$peer" "${test_dir}/${abi}-${fixture}-${count}-${peer}.bin"
        done
        cmp "${test_dir}/${abi}-${fixture}-${count}-0.bin" "${test_dir}/${abi}-${fixture}-${count}-1.bin"
    done
    done
done
for fixture in watch; do
for count in 2 4; do cmp "${test_dir}/native-${fixture}-${count}-0.bin" "${test_dir}/short-${fixture}-${count}-0.bin"; done
done
