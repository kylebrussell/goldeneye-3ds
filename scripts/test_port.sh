#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
if [[ -n "${GE_PORT_TEST_DIR:-}" ]]; then
    test_dir="${GE_PORT_TEST_DIR}"
    mkdir -p "${test_dir}"
else
    test_dir=$(mktemp -d)
    trap 'rm -rf "${test_dir}"' EXIT
fi

python3 "${repo_dir}/scripts/pack_3ds_assets.py" \
    --assets "${repo_dir}/port/tests/fixtures" \
    --source-sha1 0000000000000000000000000000000000000000 \
    --output "${test_dir}/fixtures.gepack"

"${repo_dir}/scripts/test_gun_update_exact.sh"
"${repo_dir}/scripts/test_original_gunbarrel.sh" "${repo_dir}"
"${repo_dir}/scripts/test_original_frontend_cast.sh" "${repo_dir}"
python3 "${repo_dir}/scripts/tests/test_frontend_cast_render_mode.py"
"${repo_dir}/scripts/test_original_frontend_statistics.sh" "${repo_dir}"
"${repo_dir}/scripts/test_original_frontend_cursor.sh" "${repo_dir}"
"${repo_dir}/scripts/test_original_watch_mission_abort.sh"
if [[ -f "${repo_dir}/build/3ds-assets/goldeneye.u.gepack" ]]; then
    "${repo_dir}/scripts/test_original_frontend_cast_model_live.sh" \
        "${repo_dir}"
    "${repo_dir}/scripts/test_original_stage_music.sh" "${repo_dir}"
fi

port_dead_strip=()
if [[ "$(uname -s)" == "Darwin" ]]; then
    port_dead_strip=(-Wl,-dead_strip)
else
    port_dead_strip=(-Wl,--gc-sections)
fi

cc -std=c11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
    -Wno-logical-not-parentheses -Wno-empty-body -Wno-unused-variable \
    -Wno-unused-parameter \
    -I "${repo_dir}/port/include" \
    -I "${repo_dir}/src" \
    "${repo_dir}/port/src/ge_asset_pack.c" \
    "${repo_dir}/port/src/ge_port.c" \
    "${repo_dir}/port/src/ge_libultra.c" \
    "${repo_dir}/port/src/ge_original_boss.c" \
    "${repo_dir}/port/src/ge_original_input.c" \
    "${repo_dir}/port/src/ge_original_level.c" \
    "${repo_dir}/port/src/random_port.c" \
    "${repo_dir}/src/joy.c" \
    "${repo_dir}/src/boss.c" \
    "${repo_dir}/src/game/lv.c" \
    "${repo_dir}/src/game/frametiming.c" \
    "${repo_dir}/src/game/quaternion.c" \
    "${repo_dir}/port/tests/test_ge_port.c" \
    -lm \
    "${port_dead_strip[@]}" \
    -DGE_PORT_BOSS_STAGE_SLICE -DGE_PORT_LV_STAGE_TICK_SLICE -DVERSION_US \
    -o "${test_dir}/test_ge_port"

"${test_dir}/test_ge_port" "${test_dir}/fixtures.gepack"

cc -std=c11 -Wall -Wextra -Werror \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_original_sfx_bank.c" \
    "${repo_dir}/port/tests/test_ge_original_sfx_bank.c" \
    -lm -o "${test_dir}/test_ge_original_sfx_bank"
"${test_dir}/test_ge_original_sfx_bank" \
    "${repo_dir}/assets/music/sfx.ctl" \
    "${repo_dir}/assets/music/sfx.tbl"

cc -std=c11 -Wall -Wextra -Werror \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_dam_environment.c" \
    "${repo_dir}/port/tests/test_ge_dam_environment.c" \
    -lm -o "${test_dir}/test_ge_dam_environment"
"${test_dir}/test_ge_dam_environment"

cc -std=c11 -Wall -Wextra -Werror \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_dam_environment.c" \
    "${repo_dir}/port/src/ge_dam_sky.c" \
    "${repo_dir}/port/tests/test_ge_dam_sky.c" \
    -lm -o "${test_dir}/test_ge_dam_sky"
"${test_dir}/test_ge_dam_sky"

cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion \
    -Wsign-conversion -Wshadow \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_visual_probe_tour.c" \
    "${repo_dir}/port/tests/test_ge_visual_probe_tour.c" \
    -lm -o "${test_dir}/test_ge_visual_probe_tour"
"${test_dir}/test_ge_visual_probe_tour"
python3 "${repo_dir}/scripts/tests/test_generate_dam_visual_tour.py"
python3 "${repo_dir}/scripts/tests/test_generate_dam_input_route.py"
python3 "${repo_dir}/scripts/tests/test_generate_dam_modem_route.py"
python3 "${repo_dir}/scripts/tests/test_generate_dam_end_to_end_route.py"
python3 "${repo_dir}/scripts/tests/test_verify_dam_end_to_end_result.py"
python3 "${repo_dir}/scripts/tests/test_generate_stage_visual_tour.py"
if [[ -f "${repo_dir}/build/u/assets/obseg/bg/bg_dam_all_p.bin" \
      && -f "${repo_dir}/build/3ds-assets/goldeneye.u.gepack" ]]; then
    "${repo_dir}/scripts/test_dam_route_capacity.sh"
    facility_stream_result=$(
        "${repo_dir}/scripts/probe_stage_stream.sh" \
            facility --all-connected | tail -n 1)
    if [[ "${facility_stream_result}" \
            != "room 70: ok -> 75 room, 37731 vertex, 3566 batch, 80 texture, 65 generation/0 failure" ]]; then
        printf 'Unexpected Facility stream result: %s\n' \
            "${facility_stream_result}" >&2
        exit 1
    fi
    printf 'Facility full connected-room stream capacity passed\n'
fi

cc -std=c11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
    -DGE_PORT_BOSS_STAGE_SLICE \
    -I "${repo_dir}/port/include" \
    -I "${repo_dir}/src" \
    "${repo_dir}/src/boss.c" \
    "${repo_dir}/port/src/ge_original_boss.c" \
    "${repo_dir}/port/tests/test_ge_original_boss.c" \
    "${port_dead_strip[@]}" \
    -o "${test_dir}/test_ge_original_boss"

"${test_dir}/test_ge_original_boss"

"${repo_dir}/scripts/test_facility_mission_flow.sh"

original_dam_setup_dir="${test_dir}/original-dam-setup"
mkdir -p "${original_dam_setup_dir}"
python3 "${repo_dir}/scripts/tests/test_door_collision_slice_exact.py" \
    "${repo_dir}"
python3 "${repo_dir}/scripts/extract_door_collision_slice.py" \
    "${repo_dir}" \
    "${original_dam_setup_dir}/ge_original_door_collision_slice.c"
python3 "${repo_dir}/scripts/tests/test_door_character_collision_slice_exact.py" \
    "${repo_dir}"
python3 "${repo_dir}/scripts/extract_door_character_collision_slice.py" \
    "${repo_dir}" \
    "${original_dam_setup_dir}/ge_original_door_character_collision_slice.c"

# Compile and link the exact generated mission setup. AIPARSE avoids unrelated
# runtime declarations; GE_PORT_SETUP_DATA keeps its diagnostic string tables
# out of this data-only translation unit.
cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-incompatible-pointer-types -Wno-missing-braces -Wno-pragma-pack \
    -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    -c "${repo_dir}/assets/obseg/setup/UsetupdamZ.c" \
    -o "${original_dam_setup_dir}/UsetupdamZ.o"

if nm -g "${original_dam_setup_dir}/UsetupdamZ.o" | rg -q '_ToString'; then
    echo "generated Dam setup unexpectedly exports AIPARSE string tables" >&2
    exit 1
fi

cc -std=gnu11 -Wall -Wextra -Wno-error -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    -c "${repo_dir}/port/src/ge_original_dam_setup.c" \
    -o "${original_dam_setup_dir}/adapter.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    -c "${repo_dir}/port/tests/test_ge_original_dam_setup.c" \
    -o "${original_dam_setup_dir}/test.o"

cc "${original_dam_setup_dir}/UsetupdamZ.o" \
    "${original_dam_setup_dir}/adapter.o" \
    "${original_dam_setup_dir}/test.o" \
    "${port_dead_strip[@]}" \
    -o "${original_dam_setup_dir}/test_ge_original_dam_setup"

"${original_dam_setup_dir}/test_ge_original_dam_setup"

# Start authored Dam stage AI list 0x1000 through the bounded, canonical chrai
# interpreter tranche. This executes its opening yield plus the authored
# objective-complete branch, with exact objective/tag/object-health bodies.
python3 "${repo_dir}/scripts/extract_dam_mission_object_state_slice.py" \
    "${repo_dir}" \
    "${original_dam_setup_dir}/ge_original_dam_mission_object_state_slice.c"
python3 "${repo_dir}/scripts/extract_dam_mission_hud_slice.py" \
    "${repo_dir}" \
    "${original_dam_setup_dir}/ge_original_dam_mission_hud_slice.c"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${original_dam_setup_dir}/ge_original_dam_mission_object_state_slice.c" \
    -o "${original_dam_setup_dir}/dam_mission_object_state.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${original_dam_setup_dir}/ge_original_dam_mission_hud_slice.c" \
    -o "${original_dam_setup_dir}/dam_mission_hud.o"

language_objects=()
for language_bank in LdamE LarkE LrunE LsevxE LsevE LsiloE LdestE \
        LsevxbE LsevbE LstatE LarchE LpeteE LdepoE LtraE LjunE LarecE \
        LcaveE LcradE LaztE LcrypE LgunE LmiscE LoptionsE LpropobjE LtitleE; do
    language_object="${original_dam_setup_dir}/${language_bank}.o"
    cc -std=gnu11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined -fno-omit-frame-pointer \
        -c "${repo_dir}/assets/obseg/text/${language_bank}.c" \
        -o "${language_object}"
    language_objects+=("${language_object}")
done

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-empty-body \
    -Wno-switch \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/src/ge_original_prop_state_source.c" \
    -o "${original_dam_setup_dir}/prop_state_source.o"

cc -std=gnu11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/src/ge_dam_setup_world_materializer.c" \
    -o "${original_dam_setup_dir}/world_materializer.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-missing-declarations -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}" -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/src/ge_original_dam_monitor.c" \
    -o "${original_dam_setup_dir}/dam_monitor.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-pragma-pack \
    -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function \
    -Wno-unused-but-set-variable -Wno-incompatible-pointer-types \
    -Wno-empty-body \
    -Wno-int-conversion -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_DAM_MISSION_FLOW_SLICE \
    -DGE_PORT_DAM_MISSION_SFX_SLICE \
    -DVERSION_US -DPLAYERFLAG=int \
    -I "${repo_dir}" -I "${repo_dir}/src/game" \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/src/game/chrai.c" \
    -o "${original_dam_setup_dir}/dam_mission_chrai.o"

cc -std=gnu11 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}" -I "${repo_dir}/src/game" \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/tests/ge_original_dam_mission_audio_fixture.c" \
    -o "${original_dam_setup_dir}/dam_mission_audio_fixture.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-pragma-pack \
    -Wno-unused-parameter -Wno-unused-variable -Wno-missing-braces \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_DAM_STAGE_AI_ALLOC_SLICE \
    -DPLAYERFLAG=int \
    -I "${repo_dir}" -I "${repo_dir}/src/game" \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/src/game/deb_loadallmodels.c" \
    -o "${original_dam_setup_dir}/dam_stage_ai_alloc.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DPLAYERFLAG=int \
    -I "${repo_dir}" -I "${repo_dir}/src/game" \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/src/ge_original_dam_mission_flow.c" \
    -o "${original_dam_setup_dir}/dam_mission_flow.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DPLAYERFLAG=int \
    -I "${repo_dir}" -I "${repo_dir}/src/game" \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/src/ge_original_dam_mission_stage_storage.c" \
    -o "${original_dam_setup_dir}/dam_mission_stage_storage.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DPLAYERFLAG=int \
    -I "${repo_dir}" -I "${repo_dir}/src/game" \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/tests/test_ge_original_dam_mission_flow.c" \
    -o "${original_dam_setup_dir}/dam_mission_test.o"

cc "${original_dam_setup_dir}/UsetupdamZ.o" \
    "${original_dam_setup_dir}/adapter.o" \
    "${original_dam_setup_dir}/dam_mission_chrai.o" \
    "${original_dam_setup_dir}/dam_stage_ai_alloc.o" \
    "${original_dam_setup_dir}/dam_mission_flow.o" \
    "${original_dam_setup_dir}/dam_mission_stage_storage.o" \
    "${original_dam_setup_dir}/dam_mission_object_state.o" \
    "${original_dam_setup_dir}/dam_mission_hud.o" \
    "${original_dam_setup_dir}/dam_mission_audio_fixture.o" \
    "${language_objects[@]}" \
    "${original_dam_setup_dir}/world_materializer.o" \
    "${original_dam_setup_dir}/prop_state_source.o" \
    "${original_dam_setup_dir}/dam_mission_test.o" \
    -fsanitize=address,undefined "${port_dead_strip[@]}" \
    -o "${original_dam_setup_dir}/test_ge_original_dam_mission_flow"

"${original_dam_setup_dir}/test_ge_original_dam_mission_flow"

# Exercise the configured 3DS MEMPOOL_STAGE path independently of the
# mission-flow fallback: exact vtxstore-sized allocations must not alias the
# later Dam background ChrRecord block.
cc -std=gnu11 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PLATFORM_3DS \
    -I "${repo_dir}" -I "${repo_dir}/src/game" \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    "${repo_dir}/port/src/ge_original_dam_mission_stage_storage.c" \
    "${repo_dir}/port/tests/test_ge_original_dam_stage_storage.c" \
    -o "${original_dam_setup_dir}/test_ge_original_dam_stage_storage"

"${original_dam_setup_dir}/test_ge_original_dam_stage_storage"

python3 "${repo_dir}/scripts/extract_dam_mission_flow_dependencies.py" \
    "${repo_dir}" "${original_dam_setup_dir}/dam_mission_dependencies.json"
cmp "${repo_dir}/docs/generated/dam_mission_flow_dependencies.json" \
    "${original_dam_setup_dir}/dam_mission_dependencies.json"

for mission_symbol in ai ailistFindById chraiGoToLabel chrHasStageFlag \
        alloc_false_GUARDdata_to_exec_global_action; do
    if ! nm -g "${original_dam_setup_dir}/test_ge_original_dam_mission_flow" \
            | rg -q "_?${mission_symbol}$"; then
        echo "Dam mission-flow slice did not retain ${mission_symbol}" >&2
        exit 1
    fi
done

# Compile the bounded original Dam setup-pad loader and intro spawn selection
# directly from prop.c and bondview_r.c. The generated setup's native pointers
# replace only the original setup-bank rebase boundary.
cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-empty-body \
    -Wno-switch -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -DGE_PORT_PROP_SETUP_PAD_SLICE \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    -c "${repo_dir}/src/game/prop.c" \
    -o "${original_dam_setup_dir}/prop_intro_slice.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-empty-body \
    -Wno-switch -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -DGE_PORT_BOND_INTRO_SPAWN_SLICE \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    -c "${repo_dir}/src/game/bondview_r.c" \
    -o "${original_dam_setup_dir}/bond_intro_slice.o"

cc -std=gnu11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -DGE_PORT_PROPDEF_SIZE_SLICE \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/src/game/loadobjectmodel.c" \
    -o "${original_dam_setup_dir}/propdef_size_slice.o"

cc -std=gnu11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/src/ge_dam_setup_world_materializer.c" \
    -o "${original_dam_setup_dir}/world_materializer.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-pointer-to-int-cast -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_BOND_CAMERA_SLICE \
    -DGE_PORT_BOND_PLAYER_SPAWN_SLICE -DAIPARSE \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    -c "${repo_dir}/src/game/bondview2.c" \
    -o "${original_dam_setup_dir}/bond_player_position_slice.o"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -ffunction-sections -fdata-sections \
    -I "${repo_dir}/port/include" \
    -c "${repo_dir}/port/src/ge_original_player_spawn.c" \
    -o "${original_dam_setup_dir}/player_spawn_adapter.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-empty-body \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    -c "${repo_dir}/port/tests/test_ge_original_dam_intro.c" \
    -o "${original_dam_setup_dir}/intro_test.o"

cc "${original_dam_setup_dir}/UsetupdamZ.o" \
    "${original_dam_setup_dir}/prop_intro_slice.o" \
    "${original_dam_setup_dir}/bond_intro_slice.o" \
    "${original_dam_setup_dir}/propdef_size_slice.o" \
    "${original_dam_setup_dir}/world_materializer.o" \
    "${original_dam_setup_dir}/bond_player_position_slice.o" \
    "${original_dam_setup_dir}/player_spawn_adapter.o" \
    "${original_dam_setup_dir}/intro_test.o" \
    -lm -fsanitize=address,undefined "${port_dead_strip[@]}" \
    -o "${original_dam_setup_dir}/test_ge_original_dam_intro"

"${original_dam_setup_dir}/test_ge_original_dam_intro"

for player_symbol in change_player_pos_to_target \
        ge_original_commit_intro_player_spawn_slice; do
    if ! nm -g "${original_dam_setup_dir}/test_ge_original_dam_intro" \
            | rg -q "_?${player_symbol}$"; then
        echo "original player spawn slice did not retain ${player_symbol}" >&2
        exit 1
    fi
done

for world_symbol in sizepropdef \
        ge_dam_setup_world_materialize_first_authored; do
    if ! nm -g "${original_dam_setup_dir}/test_ge_original_dam_intro" \
            | rg -q "_?${world_symbol}$"; then
        echo "Dam setup world bootstrap did not retain ${world_symbol}" >&2
        exit 1
    fi
done

# The normal Dam intro preloads the covert-modem projectile model. Compile the
# exact generated Pchrbug model on the host too, so ARM linkage cannot hide a
# stale root/skeleton/texture-table adapter.
cc -std=gnu11 -Wall -Wextra -Wno-error -Wno-unused-parameter \
    -Wno-missing-braces -Wno-incompatible-pointer-types \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/src/ge_original_bug_model.c" \
    -o "${original_dam_setup_dir}/bug_model.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/tests/test_ge_original_bug_model.c" \
    -o "${original_dam_setup_dir}/bug_model_test.o"

cc "${original_dam_setup_dir}/bug_model.o" \
    "${original_dam_setup_dir}/bug_model_test.o" \
    -fsanitize=address,undefined "${port_dead_strip[@]}" \
    -o "${original_dam_setup_dir}/test_ge_original_bug_model"

"${original_dam_setup_dir}/test_ge_original_bug_model"

# Feed the same three authored Dam records into the exact original prop pool,
# active list, enable flags and room-list insertion bodies.
cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter -Wno-empty-body \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/src/ge_original_prop_state_source.c" \
    -o "${original_dam_setup_dir}/prop_state_source.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/src/ge_original_prop_state.c" \
    -o "${original_dam_setup_dir}/prop_state.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-unused-variable -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/src/ge_original_default_object_source.c" \
    -o "${original_dam_setup_dir}/default_object_source.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/src/ge_original_default_object.c" \
    -o "${original_dam_setup_dir}/default_object.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/src/ge_original_model62.c" \
    -o "${original_dam_setup_dir}/model62.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/src/ge_original_model104.c" \
    -o "${original_dam_setup_dir}/model104.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/src/ge_original_model178.c" \
    -o "${original_dam_setup_dir}/model178.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/src/ge_original_door_source.c" \
    -o "${original_dam_setup_dir}/door_source.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/src/ge_original_door.c" \
    -o "${original_dam_setup_dir}/door.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/src/ge_original_door_runtime_source.c" \
    -o "${original_dam_setup_dir}/door_runtime_source.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-unused-variable -Wno-incompatible-pointer-types \
    -Wno-pointer-to-int-cast \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${original_dam_setup_dir}/ge_original_door_collision_slice.c" \
    -o "${original_dam_setup_dir}/door_collision_slice.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-unused-variable \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${original_dam_setup_dir}/ge_original_door_character_collision_slice.c" \
    -o "${original_dam_setup_dir}/door_character_collision_slice.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/src/ge_original_door_collision.c" \
    -o "${original_dam_setup_dir}/door_collision.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -DGE_PORT_MS_INHERITS -fms-extensions \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/src/ge_original_stage_prop_native.c" \
    -o "${original_dam_setup_dir}/stage_prop_native.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/src/ge_original_door_scene.c" \
    -o "${original_dam_setup_dir}/door_scene.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "${repo_dir}/port/include" \
    -c "${repo_dir}/port/src/ge_original_bond_input_provider.c" \
    -o "${original_dam_setup_dir}/bond_input_provider.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/src/ge_original_getposstan_source.c" \
    -o "${original_dam_setup_dir}/getposstan_source.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/src/ge_original_objinit_source.c" \
    -o "${original_dam_setup_dir}/objinit_source.o"

cc -std=gnu11 -Wall -Wextra -Wno-error -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/src/game/matrixmath.c" \
    -o "${original_dam_setup_dir}/default_object_matrixmath.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/tests/test_ge_original_prop_state.c" \
    -o "${original_dam_setup_dir}/prop_state_test.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-incompatible-pointer-types \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DVERSION_US -DBUGFIX_R0 \
    -Wno-empty-body \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/src/ge_original_covert_modem_object.c" \
    -o "${original_dam_setup_dir}/covert_modem_object.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DVERSION_US -DBUGFIX_R0 \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/tests/test_ge_original_covert_modem_object.c" \
    -o "${original_dam_setup_dir}/covert_modem_object_test.o"

cc "${original_dam_setup_dir}/UsetupdamZ.o" \
    "${original_dam_setup_dir}/prop_intro_slice.o" \
    "${original_dam_setup_dir}/world_materializer.o" \
    "${original_dam_setup_dir}/dam_monitor.o" \
    "${original_dam_setup_dir}/prop_state_source.o" \
    "${original_dam_setup_dir}/prop_state.o" \
    "${original_dam_setup_dir}/default_object_source.o" \
    "${original_dam_setup_dir}/default_object.o" \
    "${original_dam_setup_dir}/model62.o" \
    "${original_dam_setup_dir}/model104.o" \
    "${original_dam_setup_dir}/model178.o" \
    "${original_dam_setup_dir}/door_source.o" \
    "${original_dam_setup_dir}/door.o" \
    "${original_dam_setup_dir}/door_runtime_source.o" \
    "${original_dam_setup_dir}/door_collision_slice.o" \
    "${original_dam_setup_dir}/door_character_collision_slice.o" \
    "${original_dam_setup_dir}/door_collision.o" \
    "${original_dam_setup_dir}/stage_prop_native.o" \
    "${original_dam_setup_dir}/door_scene.o" \
    "${original_dam_setup_dir}/bond_input_provider.o" \
    "${original_dam_setup_dir}/getposstan_source.o" \
    "${original_dam_setup_dir}/objinit_source.o" \
    "${original_dam_setup_dir}/default_object_matrixmath.o" \
    "${original_dam_setup_dir}/prop_state_test.o" \
    -lm -fsanitize=address,undefined "${port_dead_strip[@]}" \
    -o "${original_dam_setup_dir}/test_ge_original_prop_state"

cc "${original_dam_setup_dir}/UsetupdamZ.o" \
    "${original_dam_setup_dir}/prop_intro_slice.o" \
    "${original_dam_setup_dir}/world_materializer.o" \
    "${original_dam_setup_dir}/prop_state_source.o" \
    "${original_dam_setup_dir}/prop_state.o" \
    "${original_dam_setup_dir}/default_object_source.o" \
    "${original_dam_setup_dir}/default_object.o" \
    "${original_dam_setup_dir}/model62.o" \
    "${original_dam_setup_dir}/model104.o" \
    "${original_dam_setup_dir}/model178.o" \
    "${original_dam_setup_dir}/door_source.o" \
    "${original_dam_setup_dir}/door.o" \
    "${original_dam_setup_dir}/door_runtime_source.o" \
    "${original_dam_setup_dir}/getposstan_source.o" \
    "${original_dam_setup_dir}/objinit_source.o" \
    "${original_dam_setup_dir}/default_object_matrixmath.o" \
    "${original_dam_setup_dir}/bug_model.o" \
    "${original_dam_setup_dir}/covert_modem_object.o" \
    "${original_dam_setup_dir}/covert_modem_object_test.o" \
    -lm -fsanitize=address,undefined "${port_dead_strip[@]}" \
    -o "${original_dam_setup_dir}/test_ge_original_covert_modem_object"

"${original_dam_setup_dir}/test_ge_original_covert_modem_object"

for covert_modem_symbol in \
        ge_original_covert_modem_object_create \
        ge_original_objInitPreallocatedSlice chrpropAllocate; do
    if ! nm -g "${original_dam_setup_dir}/test_ge_original_covert_modem_object" \
            | rg -q "_?${covert_modem_symbol}$"; then
        echo "original covert-modem slice did not retain ${covert_modem_symbol}" >&2
        exit 1
    fi
done

python3 "${repo_dir}/scripts/generate_first_person_model_inventory.py" --check
if [[ -f "${repo_dir}/baserom.u.z64" ]]; then
    python3 "${repo_dir}/scripts/extract_3ds_model62.py" \
        --rom "${repo_dir}/baserom.u.z64" \
        --output "${repo_dir}/build/3ds-models/chrwppksil"
    python3 "${repo_dir}/scripts/extract_3ds_first_person_pp7.py" \
        --rom "${repo_dir}/baserom.u.z64" \
        --output "${repo_dir}/build/3ds-models/first-person-pp7"
    python3 "${repo_dir}/scripts/extract_3ds_model104.py" \
        --rom "${repo_dir}/baserom.u.z64" \
        --output "${repo_dir}/build/3ds-models/window"
    python3 "${repo_dir}/scripts/extract_3ds_model178.py" \
        --rom "${repo_dir}/baserom.u.z64" \
        --output "${repo_dir}/build/3ds-models/damgatedoor"
    python3 "${repo_dir}/scripts/extract_3ds_dam_objective_models.py" \
        --rom "${repo_dir}/baserom.u.z64" \
        --output "${repo_dir}/build/3ds-models/dam-objectives"
fi
if [[ -f "${repo_dir}/build/3ds-models/first-person-pp7/GwppkZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GwppksilZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GbugZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/Gak47Z.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GremotemineZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GsniperrifleZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GtriggerZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GfistZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/Gmp5ksilZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GplastiqueZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GcameraZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GwatchmagnetattractZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GuziZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GwatchlaserZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GgrenadelaunchZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GgrenadeZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GtimedmineZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GbombcaseZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GmicrocameraZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GgoldeneyekeyZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/Gfnp90Z.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GrugerZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GspectreZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/Gm16Z.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GshotgunZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GautoshotZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/Gmp5kZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/Gtt33Z.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GskorpionZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GknifeZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GthrowknifeZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GgoldengunZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GsilverwppkZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GgoldwppkZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GlaserZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GrocketlaunchZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GproximitymineZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GtaserZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GflarepistolZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GpitongunZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/Csuit_lf_handZ.bin" \
      && -f "${repo_dir}/build/3ds-models/first-person-pp7/GjoypadZ.bin" ]]; then
    first_person_pack="${original_dam_setup_dir}/first-person-pp7.gepack"
    python3 "${repo_dir}/scripts/pack_3ds_assets.py" \
        --assets "${repo_dir}/port/tests/fixtures" \
        --source-sha1 abe01e4aeb033b6c0836819f549c791b26cfde83 \
        --extra-dir "converted/models/first-person-pp7=${repo_dir}/build/3ds-models/first-person-pp7" \
        --output "${first_person_pack}"
    cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion \
        -Wsign-conversion -Wshadow \
        -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
        -Wno-error -Wno-missing-declarations -Wno-c23-extensions \
        -I "${repo_dir}/port/include" -I "${repo_dir}/src" \
        -I "${repo_dir}/include/PR" -idirafter "${repo_dir}/include" \
        "${repo_dir}/port/src/ge_asset_pack.c" \
        "${repo_dir}/port/src/ge_original_first_person_assets.c" \
        "${repo_dir}/port/tests/test_ge_original_first_person_assets.c" \
        -o "${original_dam_setup_dir}/test_ge_original_first_person_assets"
    "${original_dam_setup_dir}/test_ge_original_first_person_assets" \
        "${first_person_pack}"
    cc -std=gnu11 -Wall -Wextra -Werror -Wno-error \
        -Wno-missing-declarations -Wno-visibility \
        -fsanitize=address,undefined -fno-omit-frame-pointer \
        -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
        -DGE_PORT_SETUP_DATA \
        -I "${repo_dir}/port/include" -I "${repo_dir}/src" \
        -I "${repo_dir}/include/PR" -idirafter "${repo_dir}/include" \
        "${repo_dir}/port/src/ge_asset_pack.c" \
        "${repo_dir}/port/src/ge_original_first_person_assets.c" \
        "${repo_dir}/port/src/ge_gbi_decoder.c" \
        "${repo_dir}/port/src/ge_gbi_matrix.c" \
        "${repo_dir}/port/src/ge_gbi_rsp.c" \
        "${repo_dir}/port/src/ge_gbi_traverse.c" \
        "${repo_dir}/port/src/ge_gbi_state.c" \
        "${repo_dir}/port/src/ge_gbi_vertex.c" \
        "${repo_dir}/port/src/ge_gbi_pipeline.c" \
        "${repo_dir}/port/src/ge_pica_material.c" \
        "${repo_dir}/port/src/ge_original_model_scene.c" \
        "${repo_dir}/port/tests/test_ge_original_first_person_capacity.c" \
        -lm -o "${original_dam_setup_dir}/test_ge_original_first_person_capacity"
    ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
        "${original_dam_setup_dir}/test_ge_original_first_person_capacity" \
        "${first_person_pack}"
fi
if [[ -f "${repo_dir}/build/3ds-models/chrwppksil/model.bin" ]]; then
    "${original_dam_setup_dir}/test_ge_original_prop_state" \
        "${repo_dir}/build/3ds-models/chrwppksil/model.bin" \
        "${repo_dir}/build/3ds-models/window/model.bin" \
        "${repo_dir}/build/3ds-models/damgatedoor/model.bin"
else
    "${original_dam_setup_dir}/test_ge_original_prop_state"
fi

for prop_symbol in chrpropAllocate chrpropActivate chrpropEnable \
        chrpropRegisterRoom \
        ge_original_domakedefaultobj_standard_prefix_slice \
        ge_original_getposstan_zero_radius_slice \
        ge_original_objInitPreallocatedSlice \
        ge_original_move_to_pad_slice \
        ge_original_bound_pad_scale_slice \
        ge_original_move_onscreen_to_pad_slice \
        ge_original_setup_door_slice \
        chrobjCollisionRelated \
        sub_GAME_7F03F540 \
        ge_original_door_collision_exact_slice \
        ge_original_door_collision_test \
        ge_original_door_scene_prepare \
        ge_original_door_chrUpdateCollisionBounds_exact \
        ge_original_door_chrGetChrWidthHeight_exact \
        ge_original_door_chrGetChrGround_exact \
        ge_original_room_object_at_position_slice; do
    if ! nm -g "${original_dam_setup_dir}/test_ge_original_prop_state" \
            | rg "_?${prop_symbol}$" >/dev/null; then
        echo "original prop-state slice did not retain ${prop_symbol}" >&2
        exit 1
    fi
done

# Materialize authored tag 5/4 targets into the exact native prop/tag state,
# execute ai_20's healthy second tick, then drive its exact tag-5-destroyed
# objective/HUD transition from the completed native damage-state boundary.
cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DPLAYERFLAG=int \
    -I "${repo_dir}" -I "${repo_dir}/src/game" \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/port/tests/test_ge_original_dam_mission_tags.c" \
    -o "${original_dam_setup_dir}/dam_mission_tags_test.o"

cc "${original_dam_setup_dir}/UsetupdamZ.o" \
    "${original_dam_setup_dir}/adapter.o" \
    "${original_dam_setup_dir}/prop_intro_slice.o" \
    "${original_dam_setup_dir}/world_materializer.o" \
    "${original_dam_setup_dir}/dam_monitor.o" \
    "${original_dam_setup_dir}/prop_state_source.o" \
    "${original_dam_setup_dir}/prop_state.o" \
    "${original_dam_setup_dir}/dam_mission_chrai.o" \
    "${original_dam_setup_dir}/dam_stage_ai_alloc.o" \
    "${original_dam_setup_dir}/dam_mission_flow.o" \
    "${original_dam_setup_dir}/dam_mission_stage_storage.o" \
    "${original_dam_setup_dir}/dam_mission_object_state.o" \
    "${original_dam_setup_dir}/dam_mission_hud.o" \
    "${original_dam_setup_dir}/dam_mission_audio_fixture.o" \
    "${language_objects[@]}" \
    "${original_dam_setup_dir}/dam_mission_tags_test.o" \
    -fsanitize=address,undefined "${port_dead_strip[@]}" \
    -o "${original_dam_setup_dir}/test_ge_original_dam_mission_tags"

"${original_dam_setup_dir}/test_ge_original_dam_mission_tags"

for mission_tag_symbol in objFindByTagId objIsHealthy weaponFindThrown \
        set_parent_cur_tag_entry ge_dam_setup_world_materialize_mission_tags \
        ge_original_dam_monitor_initialize \
        ge_original_dam_mission_hud_render_snapshot; do
    if ! nm -g "${original_dam_setup_dir}/test_ge_original_dam_mission_tags" \
            | rg -q "_?${mission_tag_symbol}$"; then
        echo "Dam mission tag integration did not retain ${mission_tag_symbol}" >&2
        exit 1
    fi
done

# Relocate the exact decompressed model-62 ROM resource into pointer-safe
# native ModelFileHeader, node, vertex, and instance storage.
if [[ -f "${repo_dir}/build/3ds-models/chrwppksil/model.bin" ]]; then
    cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
        -fsanitize=address,undefined -fno-omit-frame-pointer \
        -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
        -I "${repo_dir}/port/include" \
        -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
        -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
        "${repo_dir}/port/src/ge_original_model62.c" \
        "${repo_dir}/port/tests/test_ge_original_model62.c" \
        -lm -o "${original_dam_setup_dir}/test_ge_original_model62"
    "${original_dam_setup_dir}/test_ge_original_model62" \
        "${repo_dir}/build/3ds-models/chrwppksil/model.bin"
fi

if [[ -f "${repo_dir}/build/3ds-models/damgatedoor/model.bin" ]]; then
    cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
        -fsanitize=address,undefined -fno-omit-frame-pointer \
        -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
        -I "${repo_dir}/port/include" \
        -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
        -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
        "${repo_dir}/port/src/ge_original_model178.c" \
        "${repo_dir}/port/tests/test_ge_original_model178.c" \
        -lm -o "${original_dam_setup_dir}/test_ge_original_model178"
    "${original_dam_setup_dir}/test_ge_original_model178" \
        "${repo_dir}/build/3ds-models/damgatedoor/model.bin"
fi

if [[ -f "${repo_dir}/build/3ds-models/window/model.bin" ]]; then
    cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
        -fsanitize=address,undefined -fno-omit-frame-pointer \
        -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
        -I "${repo_dir}/port/include" \
        -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
        -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
        "${repo_dir}/port/src/ge_original_model104.c" \
        "${repo_dir}/port/tests/test_ge_original_model104.c" \
        -lm -o "${original_dam_setup_dir}/test_ge_original_model104"
    "${original_dam_setup_dir}/test_ge_original_model104" \
        "${repo_dir}/build/3ds-models/window/model.bin"
fi

# Relocate the two exact objective-owned prop models and pass both native
# instances through the unchanged successful objInit body under sanitizers.
if [[ -f "${repo_dir}/build/3ds-models/dam-objectives/modembox/model.bin" \
      && -f "${repo_dir}/build/3ds-models/dam-objectives/satdish/model.bin" ]]; then
    python3 "${repo_dir}/scripts/extract_dam_monitor_animation_slice.py" \
        "${repo_dir}" \
        "${original_dam_setup_dir}/ge_original_dam_monitor_animation_slice.c"
    cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
        -Wno-unused-variable -Wno-unused-but-set-variable -Wno-empty-body \
        -Wno-int-conversion -Wno-int-to-pointer-cast \
        -Wno-pointer-to-int-cast -Wno-missing-declarations \
        -Wno-sometimes-uninitialized \
        -ffunction-sections -fdata-sections \
        -fsanitize=address,undefined -fno-omit-frame-pointer \
        -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
        -DGE_PORT_SETUP_DATA \
        -I "${repo_dir}/port/include" \
        -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
        -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
        "${repo_dir}/port/src/ge_original_dam_objective_models.c" \
        "${repo_dir}/port/src/ge_original_dam_monitor.c" \
        "${original_dam_setup_dir}/ge_original_dam_monitor_animation_slice.c" \
        "${repo_dir}/port/tests/test_ge_original_dam_objective_models.c" \
        "${original_dam_setup_dir}/objinit_source.o" \
        -lm -fsanitize=address,undefined "${port_dead_strip[@]}" \
        -o "${original_dam_setup_dir}/test_ge_original_dam_objective_models"
    "${original_dam_setup_dir}/test_ge_original_dam_objective_models" \
        "${repo_dir}/build/3ds-models/dam-objectives/modembox/model.bin" \
        "${repo_dir}/build/3ds-models/dam-objectives/satdish/model.bin"
    for objective_model_symbol in \
            ge_original_modembox_model_relocate \
            ge_original_satdish_model_relocate \
            ge_original_process_monitor_animation_microcode \
            ge_original_dam_monitor_render_tick \
            ge_original_objInitPreallocatedSlice; do
        if ! nm -g \
                "${original_dam_setup_dir}/test_ge_original_dam_objective_models" \
                | rg -q "_?${objective_model_symbol}$"; then
            echo "Dam objective model test did not retain ${objective_model_symbol}" >&2
            exit 1
        fi
    done
fi

# Decode the authentic PP7-silenced, window and Dam gate-door Fast3D lists
# into the shared authored world-scene ABI.  This is the renderer handoff for
# the native object instances above; placement remains owned by their exact
# constructors.
if [[ -f "${repo_dir}/build/3ds-models/chrwppksil/model.bin" \
      && -f "${repo_dir}/build/3ds-models/window/model.bin" \
      && -f "${repo_dir}/build/3ds-models/damgatedoor/model.bin" ]]; then
    cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion \
        -Wsign-conversion -Wshadow \
        -fsanitize=address,undefined -fno-omit-frame-pointer \
        -I "${repo_dir}/port/include" \
        "${repo_dir}/port/src/ge_gbi_decoder.c" \
        "${repo_dir}/port/src/ge_gbi_matrix.c" \
        "${repo_dir}/port/src/ge_gbi_rsp.c" \
        "${repo_dir}/port/src/ge_gbi_traverse.c" \
        "${repo_dir}/port/src/ge_gbi_state.c" \
        "${repo_dir}/port/src/ge_gbi_vertex.c" \
        "${repo_dir}/port/src/ge_gbi_pipeline.c" \
        "${repo_dir}/port/src/ge_pica_material.c" \
        "${repo_dir}/port/src/ge_original_model_scene.c" \
        "${repo_dir}/port/tests/test_ge_original_model_scene.c" \
        -lm -o "${original_dam_setup_dir}/test_ge_original_model_scene"
    "${original_dam_setup_dir}/test_ge_original_model_scene" \
        "${repo_dir}/build/3ds-models/chrwppksil/model.bin" \
        "${repo_dir}/build/3ds-models/window/model.bin" \
        "${repo_dir}/build/3ds-models/damgatedoor/model.bin"
fi

cc -std=c11 -Wall -Wextra -Werror -pedantic -ffunction-sections -fdata-sections \
    -DGE_PORT_LV_STAGE_TICK_SLICE -DVERSION_US \
    -I "${repo_dir}/port/include" \
    -I "${repo_dir}/src" \
    "${repo_dir}/port/src/ge_original_level.c" \
    "${repo_dir}/src/game/lv.c" \
    "${repo_dir}/src/game/frametiming.c" \
    "${repo_dir}/port/tests/test_ge_original_level.c" \
    "${port_dead_strip[@]}" \
    -o "${test_dir}/test_ge_original_level"

"${test_dir}/test_ge_original_level"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_services.c" \
    "${repo_dir}/port/tests/test_ge_services.c" \
    -o "${test_dir}/test_ge_services"

"${test_dir}/test_ge_services"

cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_gbi_decoder.c" \
    "${repo_dir}/port/tests/test_ge_gbi_decoder.c" \
    -o "${test_dir}/test_ge_gbi_decoder"

"${test_dir}/test_ge_gbi_decoder"

cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_gbi_matrix.c" \
    "${repo_dir}/port/tests/test_ge_gbi_matrix.c" \
    -lm \
    -o "${test_dir}/test_ge_gbi_matrix"

"${test_dir}/test_ge_gbi_matrix"

cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_gbi_rsp.c" \
    "${repo_dir}/port/tests/test_ge_gbi_rsp.c" \
    -o "${test_dir}/test_ge_gbi_rsp"

"${test_dir}/test_ge_gbi_rsp"

cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion -Wsign-conversion -Wshadow \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_gbi_matrix.c" \
    "${repo_dir}/port/src/ge_gbi_state.c" \
    "${repo_dir}/port/src/ge_gbi_vertex.c" \
    "${repo_dir}/port/tests/test_ge_gbi_vertex.c" \
    -lm \
    -o "${test_dir}/test_ge_gbi_vertex"

"${test_dir}/test_ge_gbi_vertex"

cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion -Wsign-conversion -Wshadow \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_gbi_clip.c" \
    "${repo_dir}/port/tests/test_ge_gbi_clip.c" \
    -lm \
    -o "${test_dir}/test_ge_gbi_clip"

"${test_dir}/test_ge_gbi_clip"

original_camera_dir="${test_dir}/original-camera"
mkdir -p "${original_camera_dir}"

# Compile the exact decompiled camera matrix producer and libultra projection
# producer.  Function/data sections keep their unrelated dependency closure
# out of this bounded portable contract test.
cc -std=gnu11 -Wall -Wextra -Wno-error -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    -c "${repo_dir}/src/game/matrixmath.c" \
    -o "${original_camera_dir}/matrixmath.o"

cc -std=gnu11 -Wall -Wextra -Wno-error -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    -c "${repo_dir}/port/tests/ge_original_camera_test_producers.c" \
    -o "${original_camera_dir}/producers.o"

cc -std=gnu11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    -c "${repo_dir}/src/libultra/gu/perspective.c" \
    -o "${original_camera_dir}/perspective.o"

cc -std=gnu11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    -c "${repo_dir}/src/libultra/gu/mtxutil.c" \
    -o "${original_camera_dir}/mtxutil.o"

cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion -Wsign-conversion -Wshadow \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_dam_camera.c" \
    "${repo_dir}/port/tests/test_ge_dam_camera.c" \
    "${original_camera_dir}/matrixmath.o" \
    "${original_camera_dir}/producers.o" \
    "${original_camera_dir}/perspective.o" \
    "${original_camera_dir}/mtxutil.o" \
    -lm \
    "${port_dead_strip[@]}" \
    -o "${test_dir}/test_ge_dam_camera"

"${test_dir}/test_ge_dam_camera"

cc -std=c11 -Wall -Wextra -Werror -Wno-empty-body \
    -DGE_PORT_BG_CONNECTIVITY_SLICE \
    -I"${repo_dir}/port/include" \
    -I"${repo_dir}/src/game" \
    "${repo_dir}/port/tests/test_ge_dam_world.c" \
    "${repo_dir}/port/src/ge_dam_world.c" \
    "${repo_dir}/src/game/bg.c" \
    -o "${test_dir}/test_ge_dam_world"

(cd "${repo_dir}" && "${test_dir}/test_ge_dam_world")

cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion \
    -I"${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_dam_preload_queue.c" \
    "${repo_dir}/port/tests/test_ge_dam_preload_queue.c" \
    -o "${test_dir}/test_ge_dam_preload_queue"
"${test_dir}/test_ge_dam_preload_queue"

python3 "${repo_dir}/scripts/extract_bg_visibility_slice.py" \
    "${repo_dir}/src/game/bg.c" \
    "${test_dir}/ge_original_bg_visibility_slice.c"
cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-unused-variable -Wno-empty-body -Wno-incompatible-pointer-types \
    -Wno-return-type \
    -DVERSION_US -DLEFTOVERDEBUG -DGE_PORT_HOST_ABI \
    -I"${repo_dir}/port/include" \
    "${repo_dir}/port/tests/test_ge_original_bg_visibility.c" \
    "${repo_dir}/port/src/ge_dam_preload_queue.c" \
    "${repo_dir}/port/src/ge_original_bg_visibility.c" \
    "${test_dir}/ge_original_bg_visibility_slice.c" \
    -lm \
    -o "${test_dir}/test_ge_original_bg_visibility"
"${test_dir}/test_ge_original_bg_visibility"

if [[ -f "${repo_dir}/build/u/assets/obseg/bg/bg_dam_all_p.bin" \
      && -f "${repo_dir}/build/3ds-levels/dam/rooms/room135/point_table.bin" ]]; then
    cc -std=c11 -Wall -Wextra -Werror -Wno-empty-body \
        -DGE_PORT_BG_CONNECTIVITY_SLICE \
        -I"${repo_dir}/port/include" \
        -I"${repo_dir}/src/game" \
        "${repo_dir}/port/tests/test_ge_dam_world_scene.c" \
        "${repo_dir}/port/src/ge_dam_world.c" \
        "${repo_dir}/src/game/bg.c" \
        "${repo_dir}/port/src/ge_gbi_decoder.c" \
        "${repo_dir}/port/src/ge_gbi_matrix.c" \
        "${repo_dir}/port/src/ge_gbi_rsp.c" \
        "${repo_dir}/port/src/ge_gbi_traverse.c" \
        "${repo_dir}/port/src/ge_gbi_state.c" \
        "${repo_dir}/port/src/ge_gbi_vertex.c" \
        "${repo_dir}/port/src/ge_gbi_pipeline.c" \
        "${repo_dir}/port/src/ge_pica_material.c" \
        "${repo_dir}/port/src/ge_dam_room.c" \
        "${repo_dir}/port/src/ge_dam_camera.c" \
        "${repo_dir}/port/src/ge_draw_batch_visibility.c" \
        -lm \
        -o "${test_dir}/test_ge_dam_world_scene"
    (cd "${repo_dir}" && "${test_dir}/test_ge_dam_world_scene")
    if [[ -f "${repo_dir}/build/3ds-assets/goldeneye.u.gepack" ]]; then
        cc -std=c11 -Wall -Wextra -Werror -Wno-empty-body \
            -DGE_PORT_BG_CONNECTIVITY_SLICE \
            -I"${repo_dir}/port/include" \
            -I"${repo_dir}/src/game" \
            "${repo_dir}/port/tests/test_ge_dam_dynamic_scene.c" \
            "${repo_dir}/port/src/ge_dam_dynamic_scene.c" \
            "${repo_dir}/port/src/ge_stage_assets.c" \
            "${repo_dir}/port/src/ge_dam_preload_queue.c" \
            "${repo_dir}/port/src/ge_asset_pack.c" \
            "${repo_dir}/port/src/ge_dam_world.c" \
            "${repo_dir}/src/game/bg.c" \
            "${repo_dir}/port/src/ge_gbi_decoder.c" \
            "${repo_dir}/port/src/ge_gbi_matrix.c" \
            "${repo_dir}/port/src/ge_gbi_rsp.c" \
            "${repo_dir}/port/src/ge_gbi_traverse.c" \
            "${repo_dir}/port/src/ge_gbi_state.c" \
            "${repo_dir}/port/src/ge_gbi_vertex.c" \
            "${repo_dir}/port/src/ge_gbi_pipeline.c" \
            "${repo_dir}/port/src/ge_pica_material.c" \
            "${repo_dir}/port/src/ge_dam_room.c" \
            -lm \
            -o "${test_dir}/test_ge_dam_dynamic_scene"
        (cd "${repo_dir}" && "${test_dir}/test_ge_dam_dynamic_scene" \
            "build/u/assets/obseg/bg/bg_dam_all_p.bin" \
            "build/3ds-assets/goldeneye.u.gepack")
    fi
fi

# Retain the exact original bondview camera path while providing its player,
# room, dyn-allocation, and frustum side effects through a bounded harness.
cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-pointer-to-int-cast -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_BOND_CAMERA_SLICE \
    -DAIPARSE \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    -c "${repo_dir}/src/game/bondview2.c" \
    -o "${original_camera_dir}/bondview2-camera.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-pointer-to-int-cast -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_BOND_CAMERA_SLICE \
    -DAIPARSE \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    -c "${repo_dir}/port/src/ge_original_bond_camera.c" \
    -o "${original_camera_dir}/bond-camera-adapter.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-pointer-to-int-cast -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_BOND_CAMERA_SLICE \
    -DAIPARSE \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    -c "${repo_dir}/src/libultra/gu/lookatref.c" \
    -o "${original_camera_dir}/lookatref.o"

cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion \
    -Wsign-conversion -Wshadow -ffunction-sections -fdata-sections \
    -I "${repo_dir}/port/include" \
    -c "${repo_dir}/port/src/ge_dam_camera.c" \
    -o "${original_camera_dir}/dam-camera.o"

cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion \
    -Wsign-conversion -Wshadow -ffunction-sections -fdata-sections \
    -I "${repo_dir}/port/include" \
    -c "${repo_dir}/port/tests/test_ge_original_bond_camera.c" \
    -o "${original_camera_dir}/bond-camera-test.o"

cc "${original_camera_dir}/bondview2-camera.o" \
    "${original_camera_dir}/bond-camera-adapter.o" \
    "${original_camera_dir}/bond-camera-test.o" \
    "${original_camera_dir}/dam-camera.o" \
    "${original_camera_dir}/matrixmath.o" \
    "${original_camera_dir}/perspective.o" \
    "${original_camera_dir}/mtxutil.o" \
    "${original_camera_dir}/lookatref.o" \
    -lm \
    "${port_dead_strip[@]}" \
    -o "${original_camera_dir}/test_ge_original_bond_camera"

for camera_symbol in bondviewUpdateCurrentRoomPosition \
        store_BONDdata_curpos_to_previous bondviewUpdateCameraMatrices; do
    if ! nm -g "${original_camera_dir}/test_ge_original_bond_camera" \
            | rg -q "_?${camera_symbol}$"; then
        echo "original camera slice did not retain ${camera_symbol}" >&2
        exit 1
    fi
done
if nm -g "${original_camera_dir}/test_ge_original_bond_camera" \
        | rg -q '_?bondviewRenderDebugBondView$'; then
    echo "original camera slice retained unrelated bondview2 code" >&2
    exit 1
fi

"${original_camera_dir}/test_ge_original_bond_camera"

if [[ -f "${repo_dir}/build/u/assets/obseg/bg/bg_dam_all_p.bin" \
      && -f "${repo_dir}/build/3ds-levels/dam/rooms/room135/point_table.bin" ]]; then
    cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion \
        -Wsign-conversion -Wshadow -ffunction-sections -fdata-sections \
        -I "${repo_dir}/port/include" \
        -c "${repo_dir}/port/tests/write_ge_original_dam_spawn_camera.c" \
        -o "${original_camera_dir}/dam-spawn-camera-writer.o"
    cc "${original_camera_dir}/bondview2-camera.o" \
        "${original_camera_dir}/bond-camera-adapter.o" \
        "${original_camera_dir}/dam-spawn-camera-writer.o" \
        "${original_camera_dir}/matrixmath.o" \
        "${original_camera_dir}/perspective.o" \
        "${original_camera_dir}/mtxutil.o" \
        "${original_camera_dir}/lookatref.o" \
        -lm \
        "${port_dead_strip[@]}" \
        -o "${original_camera_dir}/write_dam_spawn_camera"
    (cd "${repo_dir}" && \
        "${original_camera_dir}/write_dam_spawn_camera" \
        "${original_camera_dir}/dam-spawn-view.bin")
    python3 "${repo_dir}/scripts/build_3ds_dam_room_bounds.py" \
        --background "${repo_dir}/build/u/assets/obseg/bg/bg_dam_all_p.bin" \
        --rooms "${repo_dir}/build/3ds-levels/dam/rooms" \
        --output "${original_camera_dir}/room_bounds.gebounds"
    (cd "${repo_dir}" && \
        "${test_dir}/test_ge_original_bg_visibility" \
        "${original_camera_dir}/dam-spawn-view.bin")
    python3 "${repo_dir}/scripts/extract_stage_environment_tables.py" \
        "${repo_dir}" \
        "${test_dir}/ge_original_stage_environment_tables.c"
    cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
        -Wno-unused-variable -Wno-empty-body -Wno-incompatible-pointer-types \
        -Wno-return-type -Wno-missing-braces \
        -DVERSION_US -DLEFTOVERDEBUG -DGE_PORT_HOST_ABI \
        -I"${repo_dir}/port/include" -I"${repo_dir}/src" \
        "${repo_dir}/port/tests/test_ge_dam_visibility_runtime.c" \
        "${repo_dir}/port/src/ge_dam_visibility_runtime.c" \
        "${repo_dir}/port/src/ge_stage_assets.c" \
        "${repo_dir}/port/src/ge_original_stage_environment.c" \
        "${test_dir}/ge_original_stage_environment_tables.c" \
        "${repo_dir}/port/src/ge_original_bg_visibility.c" \
        "${repo_dir}/port/src/ge_asset_pack.c" \
        "${test_dir}/ge_original_bg_visibility_slice.c" \
        -lm \
        -o "${test_dir}/test_ge_dam_visibility_runtime"
    dam_visibility_args=( \
        "build/u/assets/obseg/bg/bg_dam_all_p.bin" \
        "${original_camera_dir}/room_bounds.gebounds" \
        "${original_camera_dir}/dam-spawn-view.bin" )
    if [[ -f "${repo_dir}/build/3ds-assets/goldeneye.u.gepack" ]]; then
        dam_visibility_args+=("build/3ds-assets/goldeneye.u.gepack")
    fi
    (cd "${repo_dir}" && \
        "${test_dir}/test_ge_dam_visibility_runtime" \
        "${dam_visibility_args[@]}")
fi

cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_gbi_matrix.c" \
    "${repo_dir}/port/src/ge_gbi_state.c" \
    "${repo_dir}/port/tests/test_ge_gbi_state.c" \
    -o "${test_dir}/test_ge_gbi_state"

"${test_dir}/test_ge_gbi_state"

cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion -Wsign-conversion -Wshadow \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_gbi_matrix.c" \
    "${repo_dir}/port/src/ge_gbi_state.c" \
    "${repo_dir}/port/src/ge_pica_material.c" \
    "${repo_dir}/port/tests/test_ge_pica_material.c" \
    -o "${test_dir}/test_ge_pica_material"

"${test_dir}/test_ge_pica_material"

cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion \
    -Wsign-conversion -Wshadow \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_original_stage_monitor_surface.c" \
    "${repo_dir}/port/tests/test_ge_original_stage_monitor_surface.c" \
    -fsanitize=address,undefined \
    -o "${test_dir}/test_ge_original_stage_monitor_surface"

"${test_dir}/test_ge_original_stage_monitor_surface"

cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion -Wsign-conversion -Wshadow \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_pica_apply.c" \
    "${repo_dir}/port/tests/test_ge_pica_apply.c" \
    -o "${test_dir}/test_ge_pica_apply"

"${test_dir}/test_ge_pica_apply"

cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion -Wsign-conversion -Wshadow \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_gbi_decoder.c" \
    "${repo_dir}/port/src/ge_gbi_matrix.c" \
    "${repo_dir}/port/src/ge_gbi_rsp.c" \
    "${repo_dir}/port/src/ge_gbi_traverse.c" \
    "${repo_dir}/port/tests/test_ge_gbi_traverse.c" \
    -o "${test_dir}/test_ge_gbi_traverse"

"${test_dir}/test_ge_gbi_traverse"

cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion -Wsign-conversion -Wshadow \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_gbi_decoder.c" \
    "${repo_dir}/port/src/ge_gbi_matrix.c" \
    "${repo_dir}/port/src/ge_gbi_rsp.c" \
    "${repo_dir}/port/src/ge_gbi_traverse.c" \
    "${repo_dir}/port/src/ge_gbi_state.c" \
    "${repo_dir}/port/src/ge_gbi_vertex.c" \
    "${repo_dir}/port/src/ge_gbi_pipeline.c" \
    "${repo_dir}/port/tests/test_ge_gbi_pipeline.c" \
    -lm \
    -o "${test_dir}/test_ge_gbi_pipeline"

"${test_dir}/test_ge_gbi_pipeline"

cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion -Wsign-conversion -Wshadow \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_gbi_decoder.c" \
    "${repo_dir}/port/src/ge_gbi_matrix.c" \
    "${repo_dir}/port/src/ge_gbi_rsp.c" \
    "${repo_dir}/port/src/ge_gbi_traverse.c" \
    "${repo_dir}/port/src/ge_gbi_state.c" \
    "${repo_dir}/port/src/ge_gbi_vertex.c" \
    "${repo_dir}/port/src/ge_gbi_pipeline.c" \
    "${repo_dir}/port/tests/test_ge_gbi_dam_room1.c" \
    -lm \
    -o "${test_dir}/test_ge_gbi_dam_room1"

"${test_dir}/test_ge_gbi_dam_room1"
if [[ -f "${repo_dir}/build/3ds-models/dam-room1/point_table.bin" \
      && -f "${repo_dir}/build/3ds-models/dam-room1/primary_gdl.bin" ]]; then
    "${test_dir}/test_ge_gbi_dam_room1" \
        "${repo_dir}/build/3ds-models/dam-room1/point_table.bin" \
        "${repo_dir}/build/3ds-models/dam-room1/primary_gdl.bin"

    cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion -Wsign-conversion -Wshadow \
        -I "${repo_dir}/port/include" \
        "${repo_dir}/port/src/ge_gbi_decoder.c" \
        "${repo_dir}/port/src/ge_gbi_matrix.c" \
        "${repo_dir}/port/src/ge_gbi_rsp.c" \
        "${repo_dir}/port/src/ge_gbi_traverse.c" \
        "${repo_dir}/port/src/ge_gbi_state.c" \
        "${repo_dir}/port/src/ge_gbi_vertex.c" \
        "${repo_dir}/port/src/ge_gbi_pipeline.c" \
        "${repo_dir}/port/src/ge_pica_material.c" \
        "${repo_dir}/port/src/ge_dam_room.c" \
        "${repo_dir}/port/tests/test_ge_dam_room.c" \
        -lm \
        -o "${test_dir}/test_ge_dam_room"
    "${test_dir}/test_ge_dam_room" \
        "${repo_dir}/build/3ds-models/dam-room1"
fi

if [[ -f "${repo_dir}/build/3ds-levels/dam/collision/collision.gestan" \
        && -f "${repo_dir}/build/3ds-animations/bond/animation_data.bin" ]]; then
    "${repo_dir}/scripts/test_embedment_pool.sh"
    "${repo_dir}/scripts/test_covert_modem_fire.sh" \
        "${repo_dir}/build/3ds-levels/dam/collision/collision.gestan"
    cc -std=c11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections \
        -Wno-uninitialized -Wno-unused-variable -Wno-unused-parameter \
        -Wno-empty-body \
        -DGE_PORT_STAN_GEOMETRY_SLICE -DGE_PORT_BOND_MOVEMENT_SLICE \
        -I "${repo_dir}/port/include" \
        -c "${repo_dir}/src/game/stan.c" \
        -o "${test_dir}/stan_geometry_slice.o"
    cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter \
        -DGE_PORT_STAN_GEOMETRY_SLICE -DGE_PORT_BOND_MOVEMENT_SLICE \
        -I "${repo_dir}/port/include" \
        -c "${repo_dir}/src/game/stanintersection.c" \
        -o "${test_dir}/stanintersection_geometry_slice.o"
    cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion -Wsign-conversion -Wshadow \
        -DGE_PORT_STAN_GEOMETRY_SLICE \
        -I "${repo_dir}/port/include" \
        "${repo_dir}/port/src/ge_stan_collision.c" \
        "${repo_dir}/port/src/ge_stan_native.c" \
        "${repo_dir}/port/tests/test_ge_stan_collision.c" \
        "${test_dir}/stan_geometry_slice.o" \
        "${test_dir}/stanintersection_geometry_slice.o" \
        -lm \
        -o "${test_dir}/test_ge_stan_collision"
    "${test_dir}/test_ge_stan_collision" \
        "${repo_dir}/build/3ds-levels/dam/collision/collision.gestan"

    # Retain and exercise the exact Bond collision/fallback family against
    # authored Dam links and walls. This remains a diagnostic boundary until
    # bondhead/model root motion closes the normal MoveBond input path.
    cc -std=gnu11 -Wall -Wextra -Wno-error -Wno-unused-parameter \
        -Wno-switch \
        -Wno-unused-variable -Wno-empty-body -Wno-pointer-to-int-cast \
        -ffunction-sections -fdata-sections \
        -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
        -DGE_PORT_SETUP_DATA \
        -DGE_PORT_BOND_CAMERA_SLICE -DGE_PORT_BOND_MOVEMENT_SLICE \
        -I "${repo_dir}/port/include" -idirafter "${repo_dir}/include" \
        -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
        -iquote "${repo_dir}" \
        -c "${repo_dir}/port/src/ge_original_bond_movement_source.c" \
        -o "${test_dir}/bond_movement_source.o"
    cc -std=gnu11 -Wall -Wextra -Wno-error -Wno-unused-parameter \
        -Wno-unused-variable -Wno-empty-body -Wno-pointer-to-int-cast \
        -Wno-switch \
        -ffunction-sections -fdata-sections \
        -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
        -DGE_PORT_SETUP_DATA \
        -DGE_PORT_BOND_CAMERA_SLICE -DGE_PORT_BOND_MOVEMENT_SLICE \
        -I "${repo_dir}/port/include" -idirafter "${repo_dir}/include" \
        -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
        -iquote "${repo_dir}" \
        -c "${repo_dir}/port/src/ge_original_bond_movement.c" \
        -o "${test_dir}/bond_movement_adapter.o"
    cc -std=gnu11 -Wall -Wextra -Wno-error -Wno-unused-parameter \
        -Wno-unused-variable -Wno-empty-body -Wno-pointer-to-int-cast \
        -ffunction-sections -fdata-sections \
        -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
        -DGE_PORT_SETUP_DATA -DGE_PORT_BOND_HEAD_MOTION_SLICE \
        -I "${repo_dir}/port/include" -idirafter "${repo_dir}/include" \
        -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
        -iquote "${repo_dir}" \
        -c "${repo_dir}/port/src/ge_original_bond_head_source.c" \
        -o "${test_dir}/bond_head_source.o"
    cc -std=gnu11 -Wall -Wextra -Wno-error -Wno-unused-parameter \
        -Wno-unused-variable -Wno-empty-body -Wno-pointer-to-int-cast \
        -ffunction-sections -fdata-sections \
        -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
        -DGE_PORT_BOND_CAMERA_SLICE -DGE_PORT_BOND_PLAYER_SPAWN_SLICE \
        -I "${repo_dir}/port/include" -idirafter "${repo_dir}/include" \
        -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
        -iquote "${repo_dir}" -c "${repo_dir}/src/game/bondview2.c" \
        -o "${test_dir}/bond_player_position_for_movement.o"
    cc -std=gnu11 -Wall -Wextra -Wno-error -Wno-unused-parameter \
        -Wno-unused-variable -Wno-empty-body -Wno-pointer-to-int-cast \
        -Wno-switch -ffunction-sections -fdata-sections \
        -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
        -DGE_PORT_SETUP_DATA -DGE_PORT_BOND_INTRO_SPAWN_SLICE \
        -I "${repo_dir}/port/include" -idirafter "${repo_dir}/include" \
        -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
        -iquote "${repo_dir}" -c "${repo_dir}/src/game/bondview_r.c" \
        -o "${test_dir}/bond_player_state_for_movement.o"
    cc -std=gnu11 -Wall -Wextra -Wno-error -Wno-unused-parameter \
        -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
        -I "${repo_dir}/port/include" -idirafter "${repo_dir}/include" \
        -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
        -iquote "${repo_dir}" \
        "${repo_dir}/port/src/ge_stan_collision.c" \
        "${repo_dir}/port/src/ge_stan_native.c" \
        "${repo_dir}/port/src/ge_original_player_spawn.c" \
        "${repo_dir}/port/tests/test_ge_original_bond_collision.c" \
        "${test_dir}/stan_geometry_slice.o" \
        "${test_dir}/stanintersection_geometry_slice.o" \
        "${test_dir}/bond_movement_source.o" \
        "${test_dir}/bond_movement_adapter.o" \
        "${test_dir}/bond_head_source.o" \
        "${test_dir}/bond_player_position_for_movement.o" \
        "${test_dir}/bond_player_state_for_movement.o" \
        "${repo_dir}/port/src/ge_original_model_root_source.c" \
        "${repo_dir}/port/src/ge_original_animation_root.c" \
        -DGE_PORT_MODEL_ROOT_MOTION_SLICE -Wno-unused-value \
        -Wno-incompatible-pointer-types \
        -lm "${port_dead_strip[@]}" \
        -o "${test_dir}/test_ge_original_bond_collision"
    "${test_dir}/test_ge_original_bond_collision" \
        "${repo_dir}/build/3ds-levels/dam/collision/collision.gestan" \
        "${repo_dir}/build/3ds-animations/bond/animation_data.bin"
    for movement_symbol in bondviewTryMoveToStan \
            bondviewTrySimpleMovePlayerCollision \
            bondviewTryFractionMovePlayerCollision \
            bondviewTryEdgeMovePlayerCollision \
            bondviewTryEndHopPlayerCollision \
            bondviewCalcUpdatePlayerCollision \
            modelAnimReadRootMotionValue \
            sub_GAME_7F06D2E4 \
            bheadUpdatePos \
            ge_port_bond_movement_consume_head_root; do
        nm -g "${test_dir}/test_ge_original_bond_collision" \
            | grep -q "${movement_symbol}"
    done
fi

bhead_update_dir="${test_dir}/bond-head-update"
mkdir -p "${bhead_update_dir}"
bhead_update_common=(
    -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter
    -Wno-unused-variable -Wno-incompatible-pointer-types
    -Wno-pointer-to-int-cast -Wno-missing-braces
    -Wno-implicit-const-int-float-conversion
    -ffunction-sections -fdata-sections -D_LANGUAGE_C
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA
    -I "${repo_dir}" -I "${repo_dir}/port/include"
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR"
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}"
)
cc "${bhead_update_common[@]}" -DGE_PORT_BOND_HEAD_MOTION_SLICE \
    -DGE_PORT_BOND_HEAD_ANIMATION_SLICE -DGE_PORT_BOND_HEAD_UPDATE_SLICE \
    -c "${repo_dir}/port/src/ge_original_bond_head_source.c" \
    -o "${bhead_update_dir}/head.o"
cc "${bhead_update_common[@]}" \
    -c "${repo_dir}/port/src/ge_original_bond_head_update.c" \
    -o "${bhead_update_dir}/adapter.o"
cc "${bhead_update_common[@]}" \
    -c "${repo_dir}/src/game/matrixmath.c" \
    -o "${bhead_update_dir}/matrix.o"
cc "${bhead_update_common[@]}" \
    -c "${repo_dir}/src/game/matrixmath_misc.c" \
    -o "${bhead_update_dir}/matrix_misc.o"
cc "${bhead_update_common[@]}" \
    -c "${repo_dir}/port/tests/test_ge_original_bond_head_update.c" \
    -o "${bhead_update_dir}/test.o"
cc "${bhead_update_dir}"/*.o -lm "${port_dead_strip[@]}" \
    -o "${bhead_update_dir}/test_ge_original_bond_head_update"
"${bhead_update_dir}/test_ge_original_bond_head_update"
for bhead_update_symbol in bheadUpdate bheadUpdateRot \
        bheadGetBreathingValue ge_original_bond_head_update_tick \
        ge_original_bond_head_breathing_value vec3Lerp; do
    nm -g "${bhead_update_dir}/test_ge_original_bond_head_update" \
        | grep -q "${bhead_update_symbol}"
done

if [[ -f "${repo_dir}/build/3ds-animations/bond/animation_data.bin" ]]; then
    cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-value \
        -Wno-incompatible-pointer-types -ffunction-sections -fdata-sections \
        -D_LANGUAGE_C -DGE_PORT_MODEL_ROOT_MOTION_SLICE \
        -I "${repo_dir}/port/include" -idirafter "${repo_dir}/include" \
        -idirafter "${repo_dir}/include/PR" \
        "${repo_dir}/port/src/ge_original_model_root_source.c" \
        "${repo_dir}/port/src/ge_original_animation_root.c" \
        "${repo_dir}/port/tests/test_ge_original_animation_root.c" \
        -lm "${port_dead_strip[@]}" \
        -o "${test_dir}/test_ge_original_animation_root"
    "${test_dir}/test_ge_original_animation_root" \
        "${repo_dir}/build/3ds-animations/bond/animation_data.bin"
    for root_symbol in modelAnimReadRootMotionValue sub_GAME_7F06D2E4 \
            sub_GAME_7F06D3F4; do
        nm -g "${test_dir}/test_ge_original_animation_root" \
            | grep -q "${root_symbol}"
    done

    cc -std=gnu11 -Wall -Wextra -Wno-error -Wno-unused-parameter \
        -Wno-unused-variable -Wno-empty-body -Wno-incompatible-pointer-types \
        -Wno-int-conversion -Wno-unused-value -ffunction-sections \
        -fdata-sections -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES \
        -DAIPARSE -DGE_PORT_SETUP_DATA \
        -DGE_PORT_MODEL_ANIMATION_CLOCK_SLICE \
        -I "${repo_dir}" -I "${repo_dir}/port/include" \
        -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
        -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
        -c "${repo_dir}/port/src/ge_original_model_clock_source.c" \
        -o "${test_dir}/original_model_clock.o"
    for clock_symbol in modelSetAnimation modelSetAnimLooping \
            modelSetAnimEndFrame modelSetAnimSpeed modelTickAnim; do
        nm -g "${test_dir}/original_model_clock.o" | grep -q "${clock_symbol}"
    done

    cc -std=gnu11 -Wall -Wextra -Wno-error -Wno-unused-parameter \
        -Wno-incompatible-pointer-types -Wno-pointer-to-int-cast \
        -ffunction-sections -fdata-sections -D_LANGUAGE_C \
        -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
        -DGE_PORT_BOND_HEAD_MOTION_SLICE \
        -DGE_PORT_BOND_HEAD_ANIMATION_SLICE \
        -I "${repo_dir}" -I "${repo_dir}/port/include" \
        -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
        -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
        -c "${repo_dir}/port/src/ge_original_bond_head_source.c" \
        -o "${test_dir}/original_bond_head_animation.o"
    for head_symbol in bheadAdjustAnimation bheadUpdatePos \
            g_BondMoveAnimationSetup; do
        nm -g "${test_dir}/original_bond_head_animation.o" \
            | grep -q "${head_symbol}"
    done

    gait_dir="${test_dir}/player-gait"
    mkdir -p "${gait_dir}"
    python3 "${repo_dir}/scripts/extract_player_gait_model_slice.py" \
        "${repo_dir}/src/game/model.c" \
        "${repo_dir}/src/game/initBondDATAdefaults.c" \
        "${gait_dir}/ge_original_player_gait_model_slice.c"
    gait_common=(
        -std=gnu11 -Wall -Wextra -Wno-error -Wno-unused-parameter
        -Wno-unused-variable -Wno-empty-body -Wno-unused-value -Wno-switch
        -Wno-incompatible-pointer-types -Wno-pointer-to-int-cast
        -Wno-int-to-pointer-cast -Wno-int-conversion -Wno-missing-braces
        -Wno-missing-field-initializers -ffunction-sections -fdata-sections
        -fsanitize=address,undefined -fno-omit-frame-pointer
        -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE
        -DGE_PORT_SETUP_DATA -I "${repo_dir}" -I "${repo_dir}/port/include"
        -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR"
        -idirafter "${repo_dir}/src" -iquote "${repo_dir}"
    )
    cc "${gait_common[@]}" -DGE_PORT_MODEL_ROOT_MOTION_SLICE \
        -c "${repo_dir}/port/src/ge_original_model_root_source.c" \
        -o "${gait_dir}/model_root.o"
    cc "${gait_common[@]}" -DGE_PORT_MODEL_ROOT_MOTION_SLICE \
        -c "${repo_dir}/port/src/ge_original_animation_root.c" \
        -o "${gait_dir}/animation_root.o"
    cc "${gait_common[@]}" -DGE_PORT_MODEL_ANIMATION_CLOCK_SLICE \
        -c "${repo_dir}/port/src/ge_original_model_clock_source.c" \
        -o "${gait_dir}/model_clock.o"
    cc "${gait_common[@]}" -DGE_PORT_BOND_INTRO_SPAWN_SLICE \
        -c "${repo_dir}/src/game/bondview_r.c" \
        -o "${gait_dir}/spawn_player.o"
    cc "${gait_common[@]}" -DGE_PORT_BOND_HEAD_MOTION_SLICE \
        -DGE_PORT_BOND_HEAD_ANIMATION_SLICE \
        -c "${repo_dir}/port/src/ge_original_bond_head_source.c" \
        -o "${gait_dir}/bond_head_animation.o"
    gait_sources=(
        "${repo_dir}/port/src/ge_original_player_gait_data_source.c"
        "${gait_dir}/ge_original_player_gait_model_slice.c"
        "${repo_dir}/port/src/ge_original_player_gait.c"
        "${repo_dir}/port/src/ge_original_guard_animation_table.c"
        "${repo_dir}/src/game/matrixmath.c"
        "${repo_dir}/src/game/quaternion.c"
        "${repo_dir}/src/game/math_floor.c"
        "${repo_dir}/src/game/math_ceil.c"
        "${repo_dir}/src/game/math_unk_05A9E0.c"
        "${repo_dir}/port/tests/test_ge_original_player_gait.c"
    )
    gait_index=0
    for gait_source in "${gait_sources[@]}"; do
        cc "${gait_common[@]}" -c "${gait_source}" \
            -o "${gait_dir}/source_${gait_index}.o"
        gait_index=$((gait_index + 1))
    done
    cc "${gait_dir}"/*.o -lm -fsanitize=address,undefined \
        "${port_dead_strip[@]}" -o "${gait_dir}/test_ge_original_player_gait"
    "${gait_dir}/test_ge_original_player_gait" \
        "${repo_dir}/build/3ds-animations/bond/animation_data.bin" \
        "${repo_dir}/build/3ds-animations/bond/bond_eye_walk.entry.bin" \
        "${repo_dir}/build/3ds-animations/bond/sprinting.entry.bin" \
        "${repo_dir}/build/3ds-animations/bond/idle.entry.bin" \
        "${repo_dir}/build/3ds-animations/bond/animation_entries.bin"
    for gait_symbol in modelCalculateRwDataLen animInit modelSetAnimation \
            modelTickAnim subcalcpos process_01_group_heading \
            process_02_position modelBuildGroupMatrices sub_GAME_7F06DEC0 \
            ge_original_player_gait_create_bound \
            ge_original_player_gait_create_current_player \
            ge_original_player_gait_bind_bond_animations \
            ge_original_player_gait_calibrate_current_player_standing \
            ge_port_bond_animation_lookup bheadAdjustAnimation \
            ge_original_player_gait_tick_root; do
        nm -g "${gait_dir}/test_ge_original_player_gait" \
            | grep -q "${gait_symbol}"
    done

    gait_chain_sources=(
        "${repo_dir}/port/src/ge_stan_collision.c"
        "${repo_dir}/port/src/ge_stan_native.c"
        "${repo_dir}/port/src/ge_original_player_spawn.c"
        "${repo_dir}/port/tests/test_ge_original_bond_collision.c"
    )
    gait_chain_index=0
    for gait_chain_source in "${gait_chain_sources[@]}"; do
        cc "${gait_common[@]}" -DGE_PORT_STAN_GEOMETRY_SLICE \
            -DGE_TEST_PLAYER_GAIT_CHAIN -c "${gait_chain_source}" \
            -o "${gait_dir}/chain_${gait_chain_index}.o"
        gait_chain_index=$((gait_chain_index + 1))
    done
    cc "${gait_dir}/chain_0.o" "${gait_dir}/chain_1.o" \
        "${gait_dir}/chain_2.o" "${gait_dir}/chain_3.o" \
        "${gait_dir}/model_root.o" "${gait_dir}/animation_root.o" \
        "${gait_dir}/model_clock.o" \
        "${gait_dir}/bond_head_animation.o" \
        "${gait_dir}/source_0.o" "${gait_dir}/source_1.o" \
        "${gait_dir}/source_2.o" "${gait_dir}/source_3.o" \
        "${gait_dir}/source_4.o" "${gait_dir}/source_5.o" \
        "${gait_dir}/source_6.o" "${gait_dir}/source_7.o" \
        "${gait_dir}/source_8.o" \
        "${test_dir}/stan_geometry_slice.o" \
        "${test_dir}/stanintersection_geometry_slice.o" \
        "${test_dir}/bond_movement_source.o" \
        "${test_dir}/bond_movement_adapter.o" \
        "${test_dir}/bond_player_position_for_movement.o" \
        "${test_dir}/bond_player_state_for_movement.o" \
        -lm -fsanitize=address,undefined "${port_dead_strip[@]}" \
        -o "${gait_dir}/test_ge_original_player_gait_movement"
    "${gait_dir}/test_ge_original_player_gait_movement" \
        "${repo_dir}/build/3ds-levels/dam/collision/collision.gestan" \
        "${repo_dir}/build/3ds-animations/bond/animation_data.bin" \
        "${repo_dir}/build/3ds-animations/bond/bond_eye_walk.entry.bin" \
        "${repo_dir}/build/3ds-animations/bond/sprinting.entry.bin"
    for gait_chain_symbol in \
            ge_original_player_gait_current_player_movement_tick \
            ge_original_bond_root_motion_apply_current_player \
            bheadUpdatePos ge_port_bond_movement_consume_head_root; do
        nm -g "${gait_dir}/test_ge_original_player_gait_movement" \
            | grep -q "${gait_chain_symbol}"
    done
fi

cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion -Wsign-conversion -Wshadow \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_gbi_decoder.c" \
    "${repo_dir}/port/src/ge_gbi_matrix.c" \
    "${repo_dir}/port/src/ge_gbi_rsp.c" \
    "${repo_dir}/port/src/ge_gbi_traverse.c" \
    "${repo_dir}/port/src/ge_gbi_state.c" \
    "${repo_dir}/port/src/ge_gbi_vertex.c" \
    "${repo_dir}/port/src/ge_gbi_pipeline.c" \
    "${repo_dir}/port/src/ge_blotter_model.c" \
    "${repo_dir}/port/tests/test_ge_blotter_model.c" \
    -lm \
    -o "${test_dir}/test_ge_blotter_model"

"${test_dir}/test_ge_blotter_model"

cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion -Wsign-conversion -Wshadow \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_gbi_decoder.c" \
    "${repo_dir}/port/src/ge_gbi_matrix.c" \
    "${repo_dir}/port/src/ge_gbi_rsp.c" \
    "${repo_dir}/port/src/ge_gbi_traverse.c" \
    "${repo_dir}/port/src/ge_gbi_state.c" \
    "${repo_dir}/port/src/ge_gbi_vertex.c" \
    "${repo_dir}/port/src/ge_gbi_pipeline.c" \
    "${repo_dir}/port/tests/test_ge_gbi_rarewarelogo.c" \
    -lm \
    -o "${test_dir}/test_ge_gbi_rarewarelogo"

if [[ -f "${repo_dir}/build/u/ge007.u.z64" || -f "${repo_dir}/baserom.u.z64" ]]; then
    (cd "${repo_dir}" && "${test_dir}/test_ge_gbi_rarewarelogo")
fi

cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion -Wsign-conversion -Wshadow \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_gbi_decoder.c" \
    "${repo_dir}/port/src/ge_gbi_matrix.c" \
    "${repo_dir}/port/src/ge_gbi_rsp.c" \
    "${repo_dir}/port/src/ge_gbi_traverse.c" \
    "${repo_dir}/port/src/ge_gbi_state.c" \
    "${repo_dir}/port/src/ge_gbi_vertex.c" \
    "${repo_dir}/port/src/ge_gbi_pipeline.c" \
    "${repo_dir}/port/src/ge_blotter_model.c" \
    "${repo_dir}/port/tests/test_ge_gbi_blotter_model.c" \
    -lm \
    -o "${test_dir}/test_ge_gbi_blotter_model"

if [[ -f "${repo_dir}/baserom.u.z64" ]]; then
    python3 "${repo_dir}/scripts/extract_3ds_blotter_model.py" \
        --rom "${repo_dir}/baserom.u.z64" \
        --output "${repo_dir}/build/3ds-models/blotter1"
    "${test_dir}/test_ge_gbi_blotter_model" \
        "${repo_dir}/build/3ds-models/blotter1"
fi

cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_services.c" \
    "${repo_dir}/port/src/ge_libultra_services.c" \
    "${repo_dir}/port/tests/test_ge_libultra_services.c" \
    -o "${test_dir}/test_ge_libultra_services"

"${test_dir}/test_ge_libultra_services"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_services.c" \
    "${repo_dir}/port/src/ge_libultra_services.c" \
    "${repo_dir}/port/src/ge_retrace_scheduler.c" \
    "${repo_dir}/port/tests/test_ge_retrace_scheduler.c" \
    -o "${test_dir}/test_ge_retrace_scheduler"

"${test_dir}/test_ge_retrace_scheduler"

"${repo_dir}/port/tests/run_original_sched_smoke.sh"

cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion -Wsign-conversion -Wshadow \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_audio_output.c" \
    "${repo_dir}/port/src/ge_libultra_audio.c" \
    "${repo_dir}/port/tests/test_ge_audio_output.c" \
    -o "${test_dir}/test_ge_audio_output"

"${test_dir}/test_ge_audio_output"

cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion -Wsign-conversion -Wshadow \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_audio_output.c" \
    "${repo_dir}/port/src/ge_audio_refill.c" \
    "${repo_dir}/port/tests/test_ge_audio_refill.c" \
    -o "${test_dir}/test_ge_audio_refill"

"${test_dir}/test_ge_audio_refill"

"${repo_dir}/port/tests/run_original_audio_smoke.sh"
"${repo_dir}/port/tests/run_original_audio_producer.sh"
"${repo_dir}/port/tests/run_original_cseq_endian.sh"
"${repo_dir}/port/tests/run_original_music_bank.sh"
"${repo_dir}/scripts/test_gameplay_audio_lifecycle.sh"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_asset_pack.c" \
    "${repo_dir}/port/src/ge_texture_catalog.c" \
    "${repo_dir}/port/tests/test_ge_texture_catalog.c" \
    -o "${test_dir}/test_ge_texture_catalog"

if [[ -f "${repo_dir}/build/3ds-textures/catalog.gecat" \
      && -f "${repo_dir}/build/3ds-assets/goldeneye.u.gepack" ]]; then
    "${test_dir}/test_ge_texture_catalog" \
        "${repo_dir}/build/3ds-textures/catalog.gecat" \
        "${repo_dir}/build/3ds-assets/goldeneye.u.gepack"
else
    "${test_dir}/test_ge_texture_catalog"
fi

cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_asset_pack.c" \
    "${repo_dir}/port/src/ge_texture_catalog.c" \
    "${repo_dir}/port/src/ge_texture_cache.c" \
    "${repo_dir}/port/tests/test_ge_texture_cache.c" \
    -o "${test_dir}/test_ge_texture_cache"

if [[ -f "${repo_dir}/build/3ds-textures/catalog.gecat" \
      && -f "${repo_dir}/build/3ds-assets/goldeneye.u.gepack" ]]; then
    "${test_dir}/test_ge_texture_cache" \
        "${repo_dir}/build/3ds-textures/catalog.gecat" \
        "${repo_dir}/build/3ds-assets/goldeneye.u.gepack"

fi

cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -I"${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_texture_uv.c" \
    "${repo_dir}/port/tests/test_ge_texture_uv.c" \
    -lm -o "${test_dir}/test_ge_texture_uv"
"${test_dir}/test_ge_texture_uv"

python3 "${repo_dir}/scripts/tests/test_3ds_asset_pipeline.py"
python3 "${repo_dir}/scripts/tests/test_verify_3ds_asset_pack.py"

if [[ -f "${repo_dir}/assets/images/split/TARDETAIL.bin" ]]; then
    cc -std=c11 -Wall -Wextra -Werror -Wno-unused-parameter \
        -I "${repo_dir}/tools/mktex/src/libpdtex" \
        "${repo_dir}/port/tests/test_ge_texture_decode.c" \
        "${repo_dir}/tools/mktex/src/libpdtex/pdtex.c" \
        "${repo_dir}/tools/mktex/src/libpdtex/reader.c" \
        "${repo_dir}/tools/mktex/src/libpdtex/writer.c" \
        -lz -o "${test_dir}/test_ge_texture_decode"
    "${test_dir}/test_ge_texture_decode" \
        "${repo_dir}/assets/images/split/TARDETAIL.bin" \
        "${repo_dir}/assets/images/split/291.bin"
fi
python3 "${repo_dir}/scripts/tests/test_3ds_vertex_flush_spans.py"
python3 "${repo_dir}/scripts/tests/test_3ds_renderer_submission_cache.py"
python3 "${repo_dir}/scripts/tests/test_3ds_gbi_texture_rectangle_submission.py"
python3 "${repo_dir}/scripts/tests/test_extract_3ds_runtime_segments.py"
python3 "${repo_dir}/scripts/tests/test_convert_rareware_logo_3ds.py"
python3 "${repo_dir}/scripts/tests/test_extract_3ds_blotter_model.py"
python3 "${repo_dir}/scripts/tests/test_stage_3ds_pitem_models.py"
python3 "${repo_dir}/scripts/tests/test_extract_3ds_dam_rooms.py"
python3 "${repo_dir}/scripts/tests/test_build_3ds_dam_room_bounds.py"
python3 "${repo_dir}/scripts/tests/test_extract_3ds_facility_level.py"
if [[ -f "${repo_dir}/build/3ds-levels/facility/collision/collision.gestan" ]]; then
    "${repo_dir}/scripts/test_stage_assets.sh" "${repo_dir}" "${test_dir}/stage-assets"
    "${repo_dir}/scripts/test_stage_environment.sh" "${repo_dir}" "${test_dir}/stage-environment"
fi
if [[ -f "${repo_dir}/build/3ds-levels/facility/collision/collision.gestan" ]]; then
    cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion \
        -Wsign-conversion -Wshadow \
        -I "${repo_dir}/port/include" \
        "${repo_dir}/port/src/ge_stan_collision.c" \
        "${repo_dir}/port/tests/test_ge_facility_assets.c" \
        -lm -o "${test_dir}/test_ge_facility_assets"
    "${test_dir}/test_ge_facility_assets" \
        "${repo_dir}/build/3ds-levels/facility/collision/collision.gestan"
fi
python3 "${repo_dir}/scripts/tests/test_bondview_input_dependencies.py"
python3 "${repo_dir}/scripts/tests/test_bond_input_live_state_exact.py"
python3 "${repo_dir}/scripts/tests/test_bond_move_runtime_slice_exact.py"
python3 "${repo_dir}/scripts/tests/test_bond_move_live_call_order.py"
python3 "${repo_dir}/scripts/tests/test_original_stage_frame_order.py"
python3 "${repo_dir}/scripts/tests/test_live_autoaim_order.py"
python3 "${repo_dir}/scripts/tests/test_guard_overlay_direct_publish.py"
python3 "${repo_dir}/scripts/tests/test_runtime_overlay_frustum.py"
python3 "${repo_dir}/scripts/tests/test_live_guard_lighting_order.py"
python3 "${repo_dir}/scripts/tests/test_3ds_frame_pacer.py"
python3 "${repo_dir}/scripts/tests/test_bond_move_state_slice_exact.py"
python3 "${repo_dir}/scripts/tests/test_bond_move_collision_slice_exact.py" \
    "${repo_dir}"
python3 "${repo_dir}/scripts/tests/test_bond_move_non_tank_slice_exact.py" \
    "${repo_dir}"
python3 "${repo_dir}/scripts/tests/test_bond_move_explosion_slice_exact.py" \
    "${repo_dir}"
python3 "${repo_dir}/scripts/tests/test_move_model_tables_exact.py" \
    "${repo_dir}"
python3 "${repo_dir}/scripts/tests/test_animation_table_abi_exact.py" \
    "${repo_dir}"

animation_table_dir="${test_dir}/animation-table"
mkdir -p "${animation_table_dir}"
python3 "${repo_dir}/scripts/generate_animation_table_abi.py" \
    "${repo_dir}/assets/animationtable_data.c" \
    "${animation_table_dir}/animation_table.c"
cc -std=gnu11 -Wall -Wextra -Werror -fno-data-sections \
    -DGE_PORT_ANIMATION_TABLE_HOST_TEST \
    -D_LANGUAGE_C -I "${repo_dir}" -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -c "${animation_table_dir}/animation_table.c" \
    -o "${animation_table_dir}/animation_table.o"
cc -std=gnu11 -Wall -Wextra -Werror \
    -c "${repo_dir}/port/tests/test_ge_original_animation_table.c" \
    -o "${animation_table_dir}/test.o"
cc "${animation_table_dir}/animation_table.o" \
    "${animation_table_dir}/test.o" \
    -o "${animation_table_dir}/test_ge_original_animation_table"
"${animation_table_dir}/test_ge_original_animation_table"

bond_move_state_dir="${test_dir}/bond-move-state"
mkdir -p "${bond_move_state_dir}"
python3 "${repo_dir}/scripts/extract_bond_move_state_slice.py" \
    "${repo_dir}/src/game/bondview.c" \
    "${repo_dir}/src/game/bondview2.c" \
    "${repo_dir}/src/game/player.c" "${repo_dir}/src/game/lv.c" \
    "${repo_dir}/src/game/debugmenu_handler.c" \
    "${repo_dir}/src/game/model.c" "${repo_dir}/src/game/stan.c" \
    "${bond_move_state_dir}/state.c"
bond_move_state_common=(
    -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter
    -Wno-unused-variable -Wno-incompatible-pointer-types
    -Wno-pointer-to-int-cast -Wno-missing-braces -Wno-comment
    -ffunction-sections -fdata-sections -D_LANGUAGE_C
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA
    -DVERSION_US -I "${repo_dir}" -I "${repo_dir}/port/include"
    -I "${repo_dir}/src/game" -idirafter "${repo_dir}/include"
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src"
    -iquote "${repo_dir}"
)
cc "${bond_move_state_common[@]}" \
    -c "${bond_move_state_dir}/state.c" \
    -o "${bond_move_state_dir}/state.o"
cc "${bond_move_state_common[@]}" \
    -c "${repo_dir}/port/src/ge_original_bond_move_state.c" \
    -o "${bond_move_state_dir}/adapter.o"
cc "${bond_move_state_common[@]}" \
    -c "${repo_dir}/port/tests/test_ge_original_bond_move_state.c" \
    -o "${bond_move_state_dir}/test.o"
cc "${bond_move_state_dir}"/*.o "${port_dead_strip[@]}" \
    -o "${bond_move_state_dir}/test_ge_original_bond_move_state"
"${bond_move_state_dir}/test_ge_original_bond_move_state"

bond_move_collision_dir="${test_dir}/bond-move-collision"
mkdir -p "${bond_move_collision_dir}"
python3 "${repo_dir}/scripts/extract_bond_move_collision_slice.py" \
    "${repo_dir}" "${bond_move_collision_dir}/collision.c"
cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-unused-variable -Wno-incompatible-pointer-types \
    -Wno-pointer-to-int-cast -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DVERSION_US -DGE_PORT_MS_INHERITS \
    -fms-extensions -I "${repo_dir}/port/include" \
    -I "${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" -c "${bond_move_collision_dir}/collision.c" \
    -o "${bond_move_collision_dir}/collision.o"
cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -I "${repo_dir}/port/include" \
    -I "${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    -c "${repo_dir}/port/tests/test_ge_original_bond_move_collision.c" \
    -o "${bond_move_collision_dir}/test.o"
cc "${bond_move_collision_dir}/collision.o" \
    "${bond_move_collision_dir}/test.o" -lm -fsanitize=address,undefined \
    "${repo_dir}/port/src/ge_original_bond_input_provider.c" \
    -I "${repo_dir}/port/include" \
    "${port_dead_strip[@]}" \
    -o "${bond_move_collision_dir}/test_ge_original_bond_move_collision"
"${bond_move_collision_dir}/test_ge_original_bond_move_collision"

bond_move_non_tank_dir="${test_dir}/bond-move-non-tank"
mkdir -p "${bond_move_non_tank_dir}"
python3 "${repo_dir}/scripts/extract_bond_move_non_tank_slice.py" \
    "${repo_dir}" "${bond_move_non_tank_dir}/non_tank.c"
cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-unused-variable -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-pointer-to-int-cast \
    -Wno-implicit-const-int-float-conversion -Wno-empty-body -Wno-switch \
    -Wno-return-type -Wno-unused-value \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DVERSION_US -DBUGFIX_R0 \
    -DGE_PORT_MS_INHERITS -fms-extensions \
    -I "${repo_dir}" -I "${repo_dir}/port/include" -I "${repo_dir}/src/game" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${bond_move_non_tank_dir}/non_tank.c" \
    -o "${bond_move_non_tank_dir}/non_tank.o"
cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -I "${repo_dir}/port/include" \
    -I "${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    -c "${repo_dir}/port/tests/test_ge_original_bond_move_non_tank.c" \
    -o "${bond_move_non_tank_dir}/test.o"
cc "${bond_move_non_tank_dir}/non_tank.o" \
    "${bond_move_non_tank_dir}/test.o" \
    "${repo_dir}/port/src/ge_original_bond_input_provider.c" \
    -I "${repo_dir}/port/include" -lm -fsanitize=address,undefined \
    "${port_dead_strip[@]}" \
    -o "${bond_move_non_tank_dir}/test_ge_original_bond_move_non_tank"
"${bond_move_non_tank_dir}/test_ge_original_bond_move_non_tank"

bond_move_explosion_dir="${test_dir}/bond-move-explosion"
mkdir -p "${bond_move_explosion_dir}"
cc -std=gnu11 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES \
    -I "${repo_dir}" -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" \
    "${repo_dir}/port/src/ge_original_chr_obj_random.c" \
    "${repo_dir}/port/tests/test_ge_original_chr_obj_random.c" \
    -o "${bond_move_explosion_dir}/test_ge_original_chr_obj_random"
"${bond_move_explosion_dir}/test_ge_original_chr_obj_random"
python3 "${repo_dir}/scripts/extract_bond_move_explosion_slice.py" \
    "${repo_dir}" "${bond_move_explosion_dir}/explosion.c"
cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-unused-variable -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-pointer-to-int-cast \
    -Wno-int-to-pointer-cast -Wno-empty-body -Wno-missing-braces \
    -Wno-parentheses-equality -Wno-unused-value -Wno-pointer-bool-conversion \
    -Wno-missing-field-initializers -Wno-excess-initializers \
    -Wno-deprecated-declarations \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DVERSION_US -DBUGFIX_R0 \
    -DGE_PORT_MS_INHERITS -fms-extensions \
    -I "${repo_dir}" -I "${repo_dir}/port/include" \
    -I "${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" -c "${bond_move_explosion_dir}/explosion.c" \
    -o "${bond_move_explosion_dir}/explosion.o"
for explosion_symbol in maybe_detonate_object_and_its_children \
        chrlvExplosionDamage D_8002E648 explosion_animation_table; do
    nm -g "${bond_move_explosion_dir}/explosion.o" \
        | grep -q "${explosion_symbol}"
done

move_model_tables_dir="${test_dir}/move-model-tables"
mkdir -p "${move_model_tables_dir}"
python3 "${repo_dir}/scripts/generate_move_model_tables.py" \
    "${repo_dir}" "${move_model_tables_dir}/model_tables.c"
cc -std=gnu11 -Wall -Wextra -Werror \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DVERSION_US -DGE_PORT_MS_INHERITS \
    -fms-extensions -I "${repo_dir}" -I "${repo_dir}/port/include" \
    -I "${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" -c "${move_model_tables_dir}/model_tables.c" \
    -o "${move_model_tables_dir}/model_tables.o"
for model_table_symbol in PitemZ_entries c_item_entries; do
    nm -g "${move_model_tables_dir}/model_tables.o" \
        | grep "${model_table_symbol}" >/dev/null
done

# Compile the complete 0x2654-byte canonical bondviewProcessInput body. The
# first typed provider tranche resolves only normal-state/options reads; all
# gameplay side effects deliberately remain visible in the linker frontier.
bond_input_dir="${test_dir}/original-bond-input"
mkdir -p "${bond_input_dir}"
cc -std=c11 -Wall -Wextra -Werror -Wno-logical-not-parentheses \
    -Wno-empty-body -Wno-unused-variable -Wno-unused-parameter \
    -ffunction-sections -fdata-sections \
    -I "${repo_dir}/port/include" -I "${repo_dir}/src" \
    "${repo_dir}/port/src/ge_services.c" \
    "${repo_dir}/port/src/ge_libultra.c" \
    "${repo_dir}/port/src/ge_original_input.c" \
    "${repo_dir}/src/joy.c" \
    "${repo_dir}/port/tests/test_ge_original_bond_input_frame.c" \
    -lm "${port_dead_strip[@]}" \
    -o "${bond_input_dir}/raw-frame-test"
"${bond_input_dir}/raw-frame-test"
cc -std=c11 -Wall -Wextra -Werror -Wno-parentheses -Wno-empty-body \
    -Wno-unused-variable -Wno-unused-parameter \
    -I "${repo_dir}/port/include" -I "${repo_dir}/src" \
    -c "${repo_dir}/src/joy.c" -o "${bond_input_dir}/joy.o"
cc -std=c11 -Wall -Wextra -Werror -Wno-uninitialized \
    -Wno-unused-variable -Wno-unused-parameter -Wno-empty-body \
    -ffunction-sections -fdata-sections \
    -DGE_PORT_STAN_GEOMETRY_SLICE -DGE_PORT_BOND_MOVEMENT_SLICE \
    -I "${repo_dir}/port/include" \
    -c "${repo_dir}/src/game/stan.c" \
    -o "${bond_input_dir}/stan-geometry.o"
cc -std=c11 -Wall -Wextra -Werror -Wno-uninitialized \
    -Wno-unused-variable -Wno-unused-parameter -Wno-empty-body \
    -ffunction-sections -fdata-sections \
    -DGE_PORT_STAN_GEOMETRY_SLICE -DGE_PORT_STAN_DYNAMIC_PROP_COLLISION \
    -DGE_PORT_BOND_MOVEMENT_SLICE \
    -I "${repo_dir}/port/include" \
    -c "${repo_dir}/src/game/stan.c" \
    -o "${bond_input_dir}/stan-dynamic-geometry.o"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_original_bond_input_provider.c" \
    "${repo_dir}/port/tests/test_ge_original_bond_input_provider.c" \
    -o "${bond_input_dir}/provider-test"
"${bond_input_dir}/provider-test"

cc -std=gnu11 -Wall -Wextra -Wno-error -Wno-comment \
    -Wno-unused-parameter -Wno-unused-variable -Wno-empty-body \
    -Wno-incompatible-pointer-types -Wno-int-conversion \
    -Wno-pointer-to-int-cast -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_BOND_INPUT_FULL_SLICE -DVERSION_US \
    -I "${repo_dir}" -I "${repo_dir}/port/include" \
    -I "${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    -c "${repo_dir}/port/tests/ge_original_bond_input_source.c" \
    -o "${bond_input_dir}/body.o"
test "$(nm -g "${bond_input_dir}/body.o" | awk '$2 == "T" {print $3}' | wc -l | tr -d ' ')" = 1
nm -g "${bond_input_dir}/body.o" | grep -q "bondviewProcessInput"

cc -std=c11 -I "${repo_dir}/port/include" \
    -c "${repo_dir}/port/src/ge_original_bond_input_provider.c" \
    -o "${bond_input_dir}/provider.o"

python3 "${repo_dir}/scripts/extract_bond_input_live_state_slice.py" \
    "${repo_dir}" "${bond_input_dir}/live-state-source.c"
python3 "${repo_dir}/scripts/extract_bond_input_gun_data_slice.py" \
    "${repo_dir}" "${bond_input_dir}/gun-data-source.c"
cc -std=gnu11 -Wall -Wextra -Werror -Wno-missing-braces \
    -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DVERSION_US -DBUGFIX_R0 \
    -I "${repo_dir}" -I "${repo_dir}/port/include" \
    -I "${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    -c "${bond_input_dir}/gun-data-source.c" \
    -o "${bond_input_dir}/gun-data.o"
cc -std=gnu11 -Wall -Wextra -Werror -Wno-missing-braces \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DVERSION_US -DBUGFIX_R0 \
    -I "${repo_dir}" -I "${repo_dir}/port/include" \
    -I "${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    "${repo_dir}/port/tests/test_ge_original_bond_input_gun_data.c" \
    "${bond_input_dir}/gun-data.o" \
    -o "${bond_input_dir}/gun-data-test"
"${bond_input_dir}/gun-data-test"
cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment \
    -ffunction-sections -fdata-sections \
    -Wno-unused-parameter -Wno-unused-variable -Wno-empty-body \
    -Wno-incompatible-pointer-types -Wno-int-conversion \
    -Wno-pointer-to-int-cast -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES \
    -DAIPARSE -DGE_PORT_SETUP_DATA -DGE_PORT_BOND_INPUT_SPEED_HELPERS_SLICE \
    -DVERSION_US -I "${repo_dir}" -I "${repo_dir}/port/include" \
    -I "${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    -c "${repo_dir}/port/tests/ge_original_bond_speed_helpers_source.c" \
    -o "${bond_input_dir}/speed-helpers.o"
test "$(nm -g "${bond_input_dir}/speed-helpers.o" \
    | awk '$2 == "T" {print $3}' | wc -l | tr -d ' ')" = 6
cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment \
    -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DVERSION_US -DBUGFIX_R0 \
    -I "${repo_dir}" -I "${repo_dir}/port/include" \
    -I "${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    -c "${repo_dir}/port/tests/test_ge_original_bond_speed_helpers.c" \
    -o "${bond_input_dir}/speed-helper-test.o"
cc "${bond_input_dir}/speed-helpers.o" "${bond_input_dir}/provider.o" \
    "${bond_input_dir}/speed-helper-test.o" -lm \
    -o "${bond_input_dir}/speed-helper-test"
"${bond_input_dir}/speed-helper-test"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment \
    -Wno-unused-parameter -Wno-unused-variable -Wno-empty-body \
    -Wno-incompatible-pointer-types -Wno-int-conversion \
    -Wno-pointer-to-int-cast -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES \
    -DAIPARSE -DGE_PORT_SETUP_DATA -DGE_PORT_BOND_INPUT_WATCH_HELPERS_SLICE \
    -DVERSION_US -I "${repo_dir}" -I "${repo_dir}/port/include" \
    -I "${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    -c "${repo_dir}/port/tests/ge_original_bond_watch_helpers_source.c" \
    -o "${bond_input_dir}/watch-helpers.o"
test "$(nm -g "${bond_input_dir}/watch-helpers.o" \
    | awk '$2 == "T" {print $3}' | wc -l | tr -d ' ')" = 8
cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DVERSION_US \
    -I "${repo_dir}" -I "${repo_dir}/port/include" \
    -I "${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    -c "${repo_dir}/port/tests/test_ge_original_bond_watch_helpers.c" \
    -o "${bond_input_dir}/watch-helper-test.o"
cc "${bond_input_dir}/watch-helpers.o" "${bond_input_dir}/provider.o" \
    "${bond_input_dir}/watch-helper-test.o" -lm \
    -o "${bond_input_dir}/watch-helper-test"
"${bond_input_dir}/watch-helper-test"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment \
    -Wno-unused-parameter -Wno-unused-variable -Wno-empty-body \
    -Wno-incompatible-pointer-types -Wno-int-conversion \
    -Wno-pointer-to-int-cast -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES \
    -DAIPARSE -DGE_PORT_SETUP_DATA -DGE_PORT_BOND_INPUT_STATE_HELPERS_SLICE \
    -DVERSION_US -I "${repo_dir}" -I "${repo_dir}/port/include" \
    -I "${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    -c "${repo_dir}/port/tests/ge_original_bond_state_helpers_source.c" \
    -o "${bond_input_dir}/state-helpers.o"
test "$(nm -g "${bond_input_dir}/state-helpers.o" \
    | awk '$2 == "T" {print $3}' | wc -l | tr -d ' ')" = 11
cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DVERSION_US \
    -I "${repo_dir}" -I "${repo_dir}/port/include" \
    -I "${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    -c "${repo_dir}/port/tests/test_ge_original_bond_state_helpers.c" \
    -o "${bond_input_dir}/state-helper-test.o"
cc "${bond_input_dir}/state-helpers.o" "${bond_input_dir}/provider.o" \
    "${bond_input_dir}/state-helper-test.o" -lm \
    -o "${bond_input_dir}/state-helper-test"
"${bond_input_dir}/state-helper-test"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment \
    -ffunction-sections -fdata-sections \
    -Wno-unused-but-set-variable -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-empty-body -Wno-unused-variable \
    -Wno-return-type -Wno-switch -Wno-int-to-pointer-cast \
    -Wno-unused-value -Wno-sometimes-uninitialized -Wno-pointer-sign \
    -Wno-missing-braces -Wno-overflow -Wno-unused-parameter \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DVERSION_US -DBUGFIX_R0 \
    -I "${repo_dir}" -I "${repo_dir}/port/include" \
    -I "${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    -c "${bond_input_dir}/live-state-source.c" \
    -o "${bond_input_dir}/live-state.o"
test "$(nm -g "${bond_input_dir}/live-state.o" \
    | awk '$2 == "T" {print $3}' | wc -l | tr -d ' ')" = 124
cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DVERSION_US \
    -I "${repo_dir}" -I "${repo_dir}/port/include" \
    -I "${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    "${repo_dir}/port/tests/test_ge_original_bond_input_live_state.c" \
    "${bond_input_dir}/live-state.o" "${bond_input_dir}/provider.o" \
    "${bond_input_dir}/gun-data.o" "${bond_input_dir}/stan-geometry.o" \
    -lm "${port_dead_strip[@]}" \
    -o "${bond_input_dir}/live-state-test"
"${bond_input_dir}/live-state-test"

cc -std=c11 -Wall -Wextra -Werror -Wno-incompatible-pointer-types \
    -fno-builtin -I "${repo_dir}" -I "${repo_dir}/port/include" \
    -I "${repo_dir}/src/game" \
    -c "${repo_dir}/port/src/ge_original_math_angles.c" \
    -o "${bond_input_dir}/math-angles.o"
cc -std=c11 -Wall -Wextra -Werror -fno-builtin \
    -I "${repo_dir}/port/include" -I "${repo_dir}/src/game" \
    "${repo_dir}/port/tests/test_ge_original_angles.c" \
    "${bond_input_dir}/math-angles.o" -lm \
    -o "${bond_input_dir}/math-angle-test"
"${bond_input_dir}/math-angle-test"

cc -r "${bond_input_dir}/body.o" "${bond_input_dir}/provider.o" \
    "${bond_input_dir}/speed-helpers.o" "${bond_input_dir}/watch-helpers.o" \
    "${bond_input_dir}/state-helpers.o" "${bond_input_dir}/live-state.o" \
    "${bond_input_dir}/gun-data.o" "${bond_input_dir}/joy.o" \
    "${bond_input_dir}/stan-dynamic-geometry.o" \
    "${bond_input_dir}/math-angles.o" \
    -o "${bond_input_dir}/provider-bound.o"
for resolved_input_symbol in ge_original_bond_input_provider \
        cur_player_get_control_type cur_player_get_aim_control \
        get_cur_player_look_vertical_inverted getPlayerCount \
        get_cur_playernum lvlGetControlsLockedFlag \
        disablePlayerActionsWhenPausedOrInMpMenu g_CurrentPlayer \
        g_stopPlayFlag g_gameOverFlag g_bondviewForceDisarm \
        g_PlayerIsInTank g_PlayerTankProp g_BondCanEnterTank \
        g_ClockTimer g_GlobalTimerDelta viGetFovY \
        bondviewUpdateSpeedForwards bondviewUpdateSpeedSideways \
        bondviewCurrentPlayerUpdateSpeedVerta \
        bondviewCurrentPlayerUpdateSpeedTheta sub_GAME_7F080228 \
        bondviewTriggerWatchZoom bondviewUpdateWatchZoomIn \
        currentPlayerGetXAutoAimEnabledRedirect \
        currentPlayerGetYAutoAimEnabledRedirect \
        getCurrentPlayerWeaponId get_hands_firing_status \
        get_item_in_hand_or_watch_menu get_item_in_hand_zoom \
        camera_sniper_zoom_out camera_sniper_zoom_in \
        getCurrentPlayerNoise get_ammo_in_hands_magazine \
        get_ptr_item_statistics get_ammo_type_for_weapon \
        get_ammo_count_for_weapon bondwalkItemHasAmmo \
        bondwalkItemCheckBitflags \
        gunSetAimType gunSetSightVisible \
        sub_GAME_7F067F58 sub_GAME_7F067FBC \
        currentPlayerSetSwayTarget currentPlayerAdjustCrouchPos \
        bondviewGetIfCurrentPlayerDamageShowTime \
        bondviewGetVisibleToGuardsFlag bondviewYPositionRelated \
        bondviewGetPlayerDuckingHeightRelated bondviewGetCollisionRadius \
        getPlayerPointerIndex \
        trigger_remote_mine_detonation g_RemoteMineOwnerTriggerFlag \
        trigger_solo_watch_menu \
        gunTickGameplay gunTickHandState \
        used_to_load_1st_person_model_on_demand \
        chrCheckGuardsHeardSound \
        chrGetDistanceToBond chrlvAlertGuardToPlayerPosition \
        g_ChrSlots g_NumChrSlots \
        Gun_hand_without_item gunTickNoise gunCanUseWeapon \
        bondinvIncrementHeldTime getCurrentPlayerProp \
        currentPlayerGetHealth currentPlayerGetArmor \
        bondviewTankModelRotationRelated \
        joyGetStickX joyGetStickY \
        joyGetButtons chrlvStanPointPointIntersection stanResetHits \
        stanTestLineUnobstructed stanGetPositionYValue get_scenario \
        bondinvGetInvItem bondinvHasInvItem bondinvIsAliveWithFlag \
        bondinvSortInv bondinvInsertItem bondinvRemoveItem \
        bondinvGetNextAvailItem bondinvItemAvailable bondinvAddInvItem \
        bondinvRemoveItemByID \
        get_next_weapon_in_cycle_for_hand gunRequestHandWeaponChange \
        bondinvCycleForward bondinvCycleBackward \
        advance_through_inventory backstep_through_inventory \
        autoadvance_on_deplete_all_ammo give_cur_player_ammo \
        add_ammo_to_weapon ammo_related \
        g_EnterTankAudioState g_EnterTankCoord \
        g_TankDamagePenaltyTicks g_TankEnterBondHorizAngleDeg \
        g_TankEnterBondVertAngleDeg g_TankEnteringSitHeight \
        g_TankEnteringSitHeightRemain g_TankOrientationAngle \
        g_TankTurnSpeed g_TankTurretAngle \
        g_TankTurretOrientationAngleDeg \
        g_TankTurretOrientationAngleRad g_TankTurretTurn \
        g_TankTurretVerticalAngle g_TankTurretVerticalAngleRelated \
        tank_turret_turn_speed; do
    if nm -u "${bond_input_dir}/provider-bound.o" \
        | grep -q "${resolved_input_symbol}"; then
        echo "typed input provider failed to resolve ${resolved_input_symbol}" >&2
        exit 1
    fi
done
python3 "${repo_dir}/scripts/tests/test_bond_input_link_frontier.py" \
    --linked "${bond_input_dir}/provider-bound.o" \
    "${bond_input_dir}/body.o" \
    "${bond_input_dir}/provider.o" \
    "${bond_input_dir}/speed-helpers.o" \
    "${bond_input_dir}/watch-helpers.o" \
    "${bond_input_dir}/state-helpers.o" \
    "${bond_input_dir}/live-state.o" \
    "${bond_input_dir}/gun-data.o" \
    "${bond_input_dir}/joy.o" \
    "${bond_input_dir}/stan-dynamic-geometry.o" \
    "${bond_input_dir}/math-angles.o"
for authentic_input_side_effect in load_object_fill_header sndPlaySfx; do
    nm -u "${bond_input_dir}/provider-bound.o" \
        | grep -q "${authentic_input_side_effect}"
done
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -fno-omit-frame-pointer -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES \
    -I "${repo_dir}/port/include" -idirafter "${repo_dir}/include" \
    "${repo_dir}/port/src/ge_original_rom_copy.c" \
    "${repo_dir}/port/tests/test_ge_original_rom_copy.c" \
    -o "${test_dir}/test_ge_original_rom_copy"
"${test_dir}/test_ge_original_rom_copy"
if [[ -f "${repo_dir}/build/3ds-levels/dam/collision/collision.gestan" \
      && -f "${repo_dir}/build/3ds-animations/bond/animation_data.bin" \
      && -f "${repo_dir}/build/3ds-animations/bond/bond_eye_walk.entry.bin" \
      && -f "${repo_dir}/build/3ds-animations/bond/sprinting.entry.bin" \
      && -f "${repo_dir}/build/3ds-animations/bond/idle.entry.bin" ]]; then
    "${repo_dir}/scripts/test_bond_move_live_smoke.sh" \
        "${repo_dir}" "${test_dir}"
fi
"${repo_dir}/scripts/test_effect_buffers.sh"
"${repo_dir}/scripts/test_original_frontend_visuals.sh"
"${repo_dir}/scripts/test_bond_camera_live.sh"
"${repo_dir}/scripts/test_first_person_pose.sh"
"${repo_dir}/scripts/test_dam_guard_chr_scheduler.sh"
"${repo_dir}/scripts/test_stage_autogun_lifecycle.sh"
"${repo_dir}/scripts/test_dam_guard_bg_collision.sh"
"${repo_dir}/scripts/test_dam_projectile_explosion.sh"
"${repo_dir}/scripts/test_dam_guard_runtime_integration.sh"
"${repo_dir}/scripts/test_guard_attack_fire.sh"
"${repo_dir}/scripts/test_dam_mission_exit.sh"
"${repo_dir}/scripts/test_player_body.sh"
