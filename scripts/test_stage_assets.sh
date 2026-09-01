#!/usr/bin/env bash

set -euo pipefail

repo_dir=${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
test_dir=${2:-"${repo_dir}/build/host-tests/stage-assets"}
mkdir -p "${test_dir}/empty-assets"

port_dead_strip=()
if [[ "$(uname -s)" == "Darwin" ]]; then
    port_dead_strip=(-Wl,-dead_strip)
else
    port_dead_strip=(-Wl,--gc-sections)
fi

python3 "${repo_dir}/scripts/stage_3ds_pitem_models.py" \
    --root "${repo_dir}" --output "${repo_dir}/build/3ds-models/pitem"
python3 "${repo_dir}/scripts/stage_3ds_character_models.py" \
    --root "${repo_dir}" --output "${repo_dir}/build/3ds-models/characters"

python3 "${repo_dir}/scripts/extract_3ds_solo_stages.py" \
    --output "${repo_dir}/build/3ds-levels"
python3 "${repo_dir}/scripts/extract_3ds_credits_stage.py" \
    --root "${repo_dir}" --output "${repo_dir}/build/3ds-levels/cuba"
python3 "${repo_dir}/scripts/generate_3ds_stage_registry.py" --check
python3 "${repo_dir}/scripts/extract_stage_guard_dynamic_spawn_slice.py" \
    "${repo_dir}" "${test_dir}/ge_original_stage_guard_dynamic_spawn_slice.c"
python3 -m unittest \
    "${repo_dir}/scripts/tests/test_3ds_solo_stage_pipeline.py" \
    "${repo_dir}/scripts/tests/test_stage_3ds_character_models.py"

stage_pack_args=(
    --extra-dir "converted/levels/dam=${repo_dir}/build/3ds-levels/dam"
    --extra-dir "converted/levels/facility=${repo_dir}/build/3ds-levels/facility"
    --extra-dir "converted/models/pitem=${repo_dir}/build/3ds-models/pitem"
    --extra-dir "converted/models/characters=${repo_dir}/build/3ds-models/characters"
)
stage_keys=(dam facility)
stage_keys+=(cuba)
stage_pack_args+=(
    --extra-dir "converted/levels/cuba=${repo_dir}/build/3ds-levels/cuba"
)
while IFS= read -r stage_key; do
    stage_keys+=("${stage_key}")
    stage_pack_args+=(
        --extra-dir "converted/levels/${stage_key}=${repo_dir}/build/3ds-levels/${stage_key}"
    )
done < <(python3 -c \
    'import json,sys; [print(s["runtime_key"]) for s in json.load(open(sys.argv[1]))["stages"]]' \
    "${repo_dir}/docs/generated/solo_stage_asset_inventory.json")

python3 "${repo_dir}/scripts/pack_3ds_assets.py" \
    --assets "${test_dir}/empty-assets" \
    --source-sha1 abe01e4aeb033b6c0836819f549c791b26cfde83 \
    --extra "converted/levels/dam/background.bin=${repo_dir}/build/u/assets/obseg/bg/bg_dam_all_p.bin" \
    --extra "converted/levels/dam/setup/setup.bin=${repo_dir}/build/u/assets/obseg/setup/UsetupdamZ.bin" \
    "${stage_pack_args[@]}" \
    --output "${test_dir}/stages.gepack"

cc -std=c11 -Wall -Wextra -Werror -Wno-empty-body \
    -DGE_PORT_BG_CONNECTIVITY_SLICE \
    -I"${repo_dir}/port/include" -I"${repo_dir}/src/game" \
    "${repo_dir}/port/tests/test_ge_stage_assets.c" \
    "${repo_dir}/port/src/ge_stage_assets.c" \
    "${repo_dir}/port/src/ge_stage_asset_resolver.c" \
    "${repo_dir}/port/src/ge_asset_pack.c" \
    "${repo_dir}/port/src/ge_dam_world.c" \
    "${repo_dir}/port/src/ge_stan_collision.c" \
    "${repo_dir}/src/game/bg.c" \
    -lm -o "${test_dir}/test_ge_stage_assets"

"${test_dir}/test_ge_stage_assets" "${test_dir}/stages.gepack"

cc -std=c11 -Wall -Wextra -Werror -Wno-empty-body \
    -DGE_PORT_BG_CONNECTIVITY_SLICE \
    -I"${repo_dir}/port/include" -I"${repo_dir}/src/game" \
    "${repo_dir}/port/tools/ge_stage_stream_probe.c" \
    "${repo_dir}/port/src/ge_stage_assets.c" \
    "${repo_dir}/port/src/ge_stage_asset_resolver.c" \
    "${repo_dir}/port/src/ge_dam_dynamic_scene.c" \
    "${repo_dir}/port/src/ge_dam_preload_queue.c" \
    "${repo_dir}/port/src/ge_asset_pack.c" \
    "${repo_dir}/port/src/ge_dam_world.c" \
    "${repo_dir}/port/src/ge_stan_collision.c" \
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
    -lm -o "${test_dir}/ge-stage-stream-probe"

for stage_key in "${stage_keys[@]}"; do
    "${test_dir}/ge-stage-stream-probe" "${stage_key}" \
        "${test_dir}/stages.gepack"
done

# Streets 20 is the first authored portal/logic room with a zero compressed
# point-table size.  The original loader marks it resident with vertices ==
# NULL; keep that exact path covered instead of requiring fabricated geometry.
"${test_dir}/ge-stage-stream-probe" streets \
    "${test_dir}/stages.gepack" --all-connected

cc -std=c11 -Wall -Wextra -Werror -Wno-empty-body \
    -DGE_PORT_BG_CONNECTIVITY_SLICE \
    -I"${repo_dir}/port/include" -I"${repo_dir}/src/game" \
    "${repo_dir}/port/tests/test_ge_stage_dynamic_scene_eviction.c" \
    "${repo_dir}/port/src/ge_stage_assets.c" \
    "${repo_dir}/port/src/ge_stage_asset_resolver.c" \
    "${repo_dir}/port/src/ge_dam_dynamic_scene.c" \
    "${repo_dir}/port/src/ge_dam_preload_queue.c" \
    "${repo_dir}/port/src/ge_asset_pack.c" \
    "${repo_dir}/port/src/ge_dam_world.c" \
    "${repo_dir}/port/src/ge_stan_collision.c" \
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
    -lm -o "${test_dir}/test_ge_stage_dynamic_scene_eviction"
"${test_dir}/test_ge_stage_dynamic_scene_eviction" \
    "${test_dir}/stages.gepack"

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
cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-unused-variable \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_STAN_GEOMETRY_SLICE \
    -DGE_PORT_MS_INHERITS -fms-extensions \
    -I"${repo_dir}/port/include" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    "${repo_dir}/port/tests/test_ge_original_facility_setup.c" \
    "${repo_dir}/port/src/ge_original_stage_setup.c" \
    "${repo_dir}/port/src/ge_original_stage_prop_materializer.c" \
    "${repo_dir}/port/src/ge_original_stage_prop_native.c" \
    "${repo_dir}/port/src/ge_stage_assets.c" \
    "${repo_dir}/port/src/ge_asset_pack.c" \
    "${repo_dir}/port/src/ge_stan_collision.c" \
    "${repo_dir}/port/src/ge_stan_native.c" \
    "${test_dir}/stan_geometry_slice.o" \
    "${test_dir}/stanintersection_geometry_slice.o" \
    -lm -o "${test_dir}/test_ge_original_facility_setup"

"${test_dir}/test_ge_original_facility_setup" "${test_dir}/stages.gepack"

# Retain the original campaign-wide background-AI allocation boundary.  Every
# setup AI list in the 0x1000 namespace must receive the same synthetic
# ChrRecord shape that chrlvAllChrTick consumes during live propsTick.
cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-pragma-pack \
    -Wno-unused-parameter -Wno-unused-variable -Wno-missing-braces \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_DAM_STAGE_AI_ALLOC_SLICE \
    -DPLAYERFLAG=int -DGE_PORT_MS_INHERITS -fms-extensions \
    -I "${repo_dir}" -I "${repo_dir}/src/game" \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    "${repo_dir}/port/tests/test_ge_original_stage_mission_runtime.c" \
    "${repo_dir}/port/src/ge_original_stage_mission_runtime.c" \
    "${repo_dir}/port/src/ge_original_dam_mission_stage_storage.c" \
    "${repo_dir}/port/src/ge_original_stage_setup.c" \
    "${repo_dir}/port/src/ge_stage_assets.c" \
    "${repo_dir}/port/src/ge_asset_pack.c" \
    "${repo_dir}/src/game/deb_loadallmodels.c" \
    "${port_dead_strip[@]}" \
    -o "${test_dir}/test_ge_original_stage_mission_runtime"

"${test_dir}/test_ge_original_stage_mission_runtime" \
    "${test_dir}/stages.gepack"

# Reproduce the objective/tag branches of proplvreset2 without writing native
# pointers into the setup's serialized 32-bit pointer slots.  The provider
# harness supplies already-live object definitions by authored command index.
cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -Wno-unused-variable \
    -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -fms-extensions \
    -I"${repo_dir}/port/include" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    "${repo_dir}/port/tests/test_ge_original_stage_objectives.c" \
    "${repo_dir}/port/src/ge_original_stage_objectives.c" \
    "${repo_dir}/port/src/ge_original_stage_setup.c" \
    "${repo_dir}/port/src/ge_stage_assets.c" \
    "${repo_dir}/port/src/ge_asset_pack.c" \
    "${port_dead_strip[@]}" \
    -o "${test_dir}/test_ge_original_stage_objectives"

"${test_dir}/test_ge_original_stage_objectives" \
    "${test_dir}/stages.gepack"

# Evaluate every campaign objective through the native registry, preserving
# original criterion ordering/status precedence and the enter/deposit/photo
# event lists. Missing live services are asserted as named blockers.
cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -fms-extensions \
    -I"${repo_dir}/port/include" -I"${repo_dir}/src/game" \
    -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    "${repo_dir}/port/tests/test_ge_original_stage_objective_runtime.c" \
    "${repo_dir}/port/src/ge_original_stage_objective_runtime.c" \
    "${repo_dir}/port/src/ge_original_stage_objective_live.c" \
    "${repo_dir}/port/src/ge_original_stage_objective_services.c" \
    "${repo_dir}/port/src/ge_original_stage_objective_photo_exact.c" \
    "${repo_dir}/port/src/ge_original_stage_objectives.c" \
    "${repo_dir}/port/src/ge_original_stage_setup.c" \
    "${repo_dir}/port/src/ge_stage_assets.c" \
    "${repo_dir}/port/src/ge_asset_pack.c" \
    "${port_dead_strip[@]}" -lm \
    -o "${test_dir}/test_ge_original_stage_objective_runtime"

"${test_dir}/test_ge_original_stage_objective_runtime" --photo-only

"${test_dir}/test_ge_original_stage_objective_runtime" \
    "${test_dir}/stages.gepack"

python3 "${repo_dir}/scripts/tests/test_stage_objective_photo_exact.py"

# Construct every authored CCTV and autogun through the exact serialized tail
# conversion and post-default-object setup ordering. Runtime capability masks
# prevent the unchanged objTick target/alarm/fire/SFX bodies from being made
# live with a partial service graph.
cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -fms-extensions \
    -I"${repo_dir}/port/include" -I"${repo_dir}/src/game" \
    -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    "${repo_dir}/port/tests/test_ge_original_stage_security.c" \
    "${repo_dir}/port/src/ge_original_stage_security.c" \
    "${repo_dir}/port/src/ge_original_stage_autogun_lifecycle.c" \
    "${repo_dir}/port/src/ge_original_stage_prop_native.c" \
    "${repo_dir}/port/src/ge_original_stage_prop_materializer.c" \
    "${repo_dir}/port/src/ge_original_stage_setup.c" \
    "${repo_dir}/port/src/ge_stage_assets.c" \
    "${repo_dir}/port/src/ge_asset_pack.c" \
    "${repo_dir}/src/game/matrixmath.c" \
    "${port_dead_strip[@]}" -lm \
    -o "${test_dir}/test_ge_original_stage_security"

"${test_dir}/test_ge_original_stage_security" \
    "${test_dir}/stages.gepack"

for symbol in ge_original_stage_security_construct \
        ge_original_stage_security_required_capabilities \
        ge_original_stage_security_dependency_audit \
        ge_original_stage_security_model_audit \
        ge_original_stage_security_status_name; do
    nm -g "${test_dir}/test_ge_original_stage_security" \
        | grep -E "_?${symbol}$" >/dev/null
done

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-unused-variable \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -fms-extensions \
    -I"${repo_dir}/port/include" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    "${repo_dir}/port/tests/test_ge_original_stage_special_objects.c" \
    "${repo_dir}/port/src/ge_original_stage_special_objects.c" \
    "${repo_dir}/port/src/ge_original_stage_safe_runtime.c" \
    "${repo_dir}/port/src/ge_original_stage_setup.c" \
    "${repo_dir}/port/src/ge_original_stage_prop_materializer.c" \
    "${repo_dir}/port/src/ge_original_stage_prop_native.c" \
    "${repo_dir}/port/src/ge_stage_assets.c" \
    "${repo_dir}/port/src/ge_asset_pack.c" \
    "${repo_dir}/port/src/ge_stan_collision.c" \
    "${repo_dir}/port/src/ge_stan_native.c" \
    "${test_dir}/stan_geometry_slice.o" \
    "${test_dir}/stanintersection_geometry_slice.o" \
    -lm "${port_dead_strip[@]}" \
    -o "${test_dir}/test_ge_original_stage_special_objects"

"${test_dir}/test_ge_original_stage_special_objects" \
    "${test_dir}/stages.gepack"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-unused-variable -fsanitize=address,undefined \
    -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -fms-extensions \
    -I"${repo_dir}/port/include" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    "${repo_dir}/port/tests/test_ge_original_dam_alarm_interaction.c" \
    "${repo_dir}/port/src/ge_original_stage_alarm_interaction.c" \
    "${repo_dir}/port/src/ge_original_stage_setup.c" \
    "${repo_dir}/port/src/ge_original_stage_prop_materializer.c" \
    "${repo_dir}/port/src/ge_original_stage_prop_native.c" \
    "${repo_dir}/port/src/ge_stage_assets.c" \
    "${repo_dir}/port/src/ge_asset_pack.c" \
    -lm "${port_dead_strip[@]}" \
    -o "${test_dir}/test_ge_original_dam_alarm_interaction"

"${test_dir}/test_ge_original_dam_alarm_interaction" \
    "${test_dir}/stages.gepack"

for symbol in ge_original_stage_object_interact_exact \
        ge_original_stage_alarm_interact_exact \
        ge_original_stage_alarm_interaction_status_name; do
    nm -g "${test_dir}/test_ge_original_dam_alarm_interaction" \
        | grep -E "_?${symbol}$" >/dev/null
done

for symbol in ge_original_stage_misc_construct_exact \
        ge_original_stage_misc_runtime_dependencies \
        ge_original_stage_safe_item_link_exact \
        ge_original_stage_safe_runtime_register_relation \
        ge_original_stage_safe_runtime_can_pickup \
        objCanPickupFromSafe \
        ge_original_stage_misc_status_name; do
    nm -g "${test_dir}/test_ge_original_stage_special_objects" \
        | grep -E "_?${symbol}$" >/dev/null
done

python3 "${repo_dir}/scripts/generate_move_model_tables.py" \
    "${repo_dir}" "${test_dir}/ge_original_move_model_tables.c"
python3 "${repo_dir}/scripts/extract_stage_guard_actor_slice.py" \
    "${repo_dir}" "${test_dir}/ge_original_stage_guard_actor_slice.c"
python3 "${repo_dir}/scripts/extract_global_ai_lists.py" \
    "${repo_dir}/build/u/src/game/chraidata.o" \
    "${test_dir}/ge_original_global_ai.c"
python3 "${repo_dir}/scripts/extract_dam_monitor_animation_slice.py" \
    "${repo_dir}" \
    "${test_dir}/ge_original_dam_monitor_animation_slice.c"

# Audit the exact relocated Pitem model hierarchies used by every authored
# CCTV/autogun before enabling their canonical render/tick dependency graph.
cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-unused-variable -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-pointer-to-int-cast \
    -Wno-int-to-pointer-cast \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -DVERSION_US \
    -fms-extensions -I"${repo_dir}" -I"${repo_dir}/port/include" \
    -I"${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    "${repo_dir}/port/tests/test_ge_original_stage_security_models.c" \
    "${repo_dir}/port/tests/ge_original_pitem_model_test_support.c" \
    "${repo_dir}/port/src/ge_original_pitem_models.c" \
    "${repo_dir}/port/src/ge_original_stage_security.c" \
    "${repo_dir}/port/src/ge_original_stage_prop_native.c" \
    "${repo_dir}/port/src/ge_original_stage_prop_materializer.c" \
    "${repo_dir}/port/src/ge_original_stage_setup.c" \
    "${repo_dir}/port/src/ge_stage_assets.c" \
    "${repo_dir}/port/src/ge_asset_pack.c" \
    "${repo_dir}/src/game/matrixmath.c" \
    "${test_dir}/ge_original_move_model_tables.c" \
    -lm "${port_dead_strip[@]}" \
    -o "${test_dir}/test_ge_original_stage_security_models"

"${test_dir}/test_ge_original_stage_security_models" \
    "${test_dir}/stages.gepack"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-unused-variable -Wno-unused-but-set-variable -Wno-empty-body \
    -Wno-int-conversion -Wno-pointer-to-int-cast \
    -Wno-incompatible-pointer-types \
    -Wno-int-to-pointer-cast -Wno-sometimes-uninitialized \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -DVERSION_US \
    -fms-extensions \
    -I"${repo_dir}" -I"${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -I"${repo_dir}/src/game" \
    "${repo_dir}/port/tests/test_ge_original_pitem_models.c" \
    "${repo_dir}/port/tests/ge_original_pitem_model_test_support.c" \
    "${repo_dir}/port/src/ge_original_pitem_models.c" \
    "${repo_dir}/port/src/ge_original_model_scene.c" \
    "${repo_dir}/port/src/ge_original_stage_setup.c" \
    "${repo_dir}/port/src/ge_original_stage_prop_materializer.c" \
    "${repo_dir}/port/src/ge_original_stage_prop_native.c" \
    "${repo_dir}/port/src/ge_original_stage_monitor.c" \
    "${repo_dir}/port/src/ge_original_stage_monitor_surface.c" \
    "${repo_dir}/port/src/ge_original_stage_interactive_objects.c" \
    "${repo_dir}/port/src/ge_original_stage_items.c" \
    "${repo_dir}/port/src/ge_original_stage_supplies.c" \
    "${repo_dir}/port/src/ge_original_stage_guard_actor.c" \
    "${test_dir}/ge_original_stage_guard_actor_slice.c" \
    "${repo_dir}/port/src/ge_original_door.c" \
    "${repo_dir}/port/src/ge_original_door_source.c" \
    "${repo_dir}/port/src/ge_original_door_runtime_source.c" \
    "${repo_dir}/port/src/ge_original_default_object.c" \
    "${repo_dir}/port/src/ge_original_default_object_source.c" \
    "${repo_dir}/port/src/ge_original_getposstan_source.c" \
    "${repo_dir}/port/src/ge_original_objinit_source.c" \
    "${repo_dir}/port/src/ge_original_prop_state_source.c" \
    "${repo_dir}/port/src/ge_stage_assets.c" \
    "${repo_dir}/port/src/ge_asset_pack.c" \
    "${repo_dir}/port/src/ge_stan_collision.c" \
    "${repo_dir}/port/src/ge_stan_native.c" \
    "${repo_dir}/port/src/ge_gbi_decoder.c" \
    "${repo_dir}/port/src/ge_gbi_matrix.c" \
    "${repo_dir}/port/src/ge_gbi_rsp.c" \
    "${repo_dir}/port/src/ge_gbi_traverse.c" \
    "${repo_dir}/port/src/ge_gbi_state.c" \
    "${repo_dir}/port/src/ge_gbi_vertex.c" \
    "${repo_dir}/port/src/ge_gbi_pipeline.c" \
    "${repo_dir}/port/src/ge_pica_material.c" \
    "${repo_dir}/port/src/ge_dam_room.c" \
    "${repo_dir}/src/game/matrixmath.c" \
    "${test_dir}/ge_original_move_model_tables.c" \
    "${test_dir}/ge_original_dam_monitor_animation_slice.c" \
    "${test_dir}/stan_geometry_slice.o" \
    "${test_dir}/stanintersection_geometry_slice.o" \
    -lm "${port_dead_strip[@]}" \
    -o "${test_dir}/test_ge_original_pitem_models"

"${test_dir}/test_ge_original_pitem_models" "${test_dir}/stages.gepack"

for symbol in ge_original_stage_interactive_expected_item_count \
        ge_original_stage_interactive_live_item_count \
        ge_original_stage_interactive_active_item \
        ge_original_stage_interactive_root_item_count \
        ge_original_stage_interactive_root_item \
        ge_original_stage_item_construct_standard_exact \
        ge_original_stage_item_construct_embedded_exact \
        ge_original_stage_item_construct_assigned_exact; do
    nm -g "${test_dir}/test_ge_original_pitem_models" \
        | grep -E "_?${symbol}$" >/dev/null
done

for symbol in ge_original_stage_supply_construct_exact \
        ge_original_stage_supply_status_name; do
    nm -g "${test_dir}/test_ge_original_pitem_models" \
        | grep -E "_?${symbol}$" >/dev/null
done

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-unused-variable -Wno-incompatible-pointer-types \
    -Wno-missing-braces -Wno-pointer-to-int-cast \
    -fsanitize=address,undefined \
    -fno-omit-frame-pointer -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES \
    -DAIPARSE -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -DVERSION_US \
    -fms-extensions -I"${repo_dir}" -I"${repo_dir}/port/include" \
    -I"${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    "${repo_dir}/port/src/ge_asset_pack.c" \
    "${repo_dir}/port/src/ge_original_character_models.c" \
    "${repo_dir}/port/src/ge_original_dam_guard_model.c" \
    "${repo_dir}/port/src/ge_original_dam_guard_weapon_model.c" \
    "${repo_dir}/port/src/ge_original_bug_model.c" \
    "${test_dir}/ge_original_move_model_tables.c" \
    "${repo_dir}/port/tests/test_ge_original_character_models.c" \
    -lm -o "${test_dir}/test_ge_original_character_models"

"${test_dir}/test_ge_original_character_models" "${test_dir}/stages.gepack"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-unused-variable -Wno-incompatible-pointer-types \
    -Wno-unused-function -Wno-missing-braces -fsanitize=address,undefined \
    -fno-omit-frame-pointer -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES \
    -DAIPARSE -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -DVERSION_US \
    -fms-extensions -I"${repo_dir}" -I"${repo_dir}/port/include" \
    -I"${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    "${repo_dir}/port/tests/test_ge_original_character_appearance.c" \
    "${repo_dir}/port/src/ge_original_character_appearance.c" \
    "${repo_dir}/port/src/ge_original_dam_guard_model.c" \
    "${repo_dir}/port/src/ge_original_dam_guard_weapon_model.c" \
    "${repo_dir}/port/src/ge_original_bug_model.c" \
    "${test_dir}/ge_original_move_model_tables.c" \
    -lm -o "${test_dir}/test_ge_original_character_appearance"

"${test_dir}/test_ge_original_character_appearance"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-unused-variable -Wno-incompatible-pointer-types \
    -Wno-missing-braces -fsanitize=address,undefined \
    -fno-omit-frame-pointer -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES \
    -DAIPARSE -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -DVERSION_US \
    -fms-extensions -I"${repo_dir}" -I"${repo_dir}/port/include" \
    -I"${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    "${repo_dir}/port/tests/test_ge_original_stage_guard_runtime.c" \
    "${test_dir}/ge_original_stage_guard_dynamic_spawn_slice.c" \
    "${repo_dir}/port/src/ge_original_stage_guard_runtime.c" \
    "${repo_dir}/port/src/ge_original_stage_guard_actor.c" \
    "${repo_dir}/port/src/ge_original_stage_active_props.c" \
    "${repo_dir}/port/src/ge_original_stage_interactive_objects.c" \
    "${test_dir}/ge_original_stage_guard_actor_slice.c" \
    "${test_dir}/ge_original_global_ai.c" \
    "${repo_dir}/port/src/ge_original_character_models.c" \
    "${repo_dir}/port/src/ge_original_model_scene.c" \
    "${repo_dir}/port/src/ge_original_stage_setup.c" \
    "${repo_dir}/port/src/ge_original_stage_prop_materializer.c" \
    "${repo_dir}/port/src/ge_original_stage_prop_native.c" \
    "${repo_dir}/port/src/ge_original_pitem_models.c" \
    "${repo_dir}/port/src/ge_original_objinit_source.c" \
    "${repo_dir}/port/src/ge_original_default_object.c" \
    "${repo_dir}/port/src/ge_original_getposstan_source.c" \
    "${repo_dir}/port/src/ge_original_prop_state_source.c" \
    "${repo_dir}/port/src/ge_stage_assets.c" \
    "${repo_dir}/port/src/ge_asset_pack.c" \
    "${repo_dir}/port/src/ge_stan_collision.c" \
    "${repo_dir}/port/src/ge_stan_native.c" \
    "${repo_dir}/port/src/ge_gbi_decoder.c" \
    "${repo_dir}/port/src/ge_gbi_matrix.c" \
    "${repo_dir}/port/src/ge_gbi_rsp.c" \
    "${repo_dir}/port/src/ge_gbi_traverse.c" \
    "${repo_dir}/port/src/ge_gbi_state.c" \
    "${repo_dir}/port/src/ge_gbi_vertex.c" \
    "${repo_dir}/port/src/ge_gbi_pipeline.c" \
    "${repo_dir}/port/src/ge_pica_material.c" \
    "${repo_dir}/port/src/ge_dam_room.c" \
    "${repo_dir}/src/game/matrixmath.c" \
    "${repo_dir}/port/src/ge_original_dam_guard_model.c" \
    "${repo_dir}/port/src/ge_original_dam_guard_weapon_model.c" \
    "${repo_dir}/port/src/ge_original_bug_model.c" \
    "${test_dir}/ge_original_move_model_tables.c" \
    "${test_dir}/stan_geometry_slice.o" \
    "${test_dir}/stanintersection_geometry_slice.o" \
    -lm "${port_dead_strip[@]}" \
    -o "${test_dir}/test_ge_original_stage_guard_runtime"

"${test_dir}/test_ge_original_stage_guard_runtime" \
    "${test_dir}/stages.gepack"

python3 - "${repo_dir}" \
        "${test_dir}/ge_original_stage_guard_actor_slice.c" <<'PY'
import importlib.util
import sys
from pathlib import Path

repo = Path(sys.argv[1])
generated = Path(sys.argv[2]).read_text()
spec = importlib.util.spec_from_file_location(
    "actor_extract", repo / "scripts/extract_stage_guard_actor_slice.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
assert generated == module.render(repo)
print("Stage guard actor exactness: unchanged constructor and model/health helpers retained")
PY

python3 - "${repo_dir}" "${test_dir}/ge_original_global_ai.c" <<'PY'
import importlib.util
import sys
from pathlib import Path

repo = Path(sys.argv[1])
generated = Path(sys.argv[2]).read_text()
spec = importlib.util.spec_from_file_location(
    "global_ai_extract", repo / "scripts/extract_global_ai_lists.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
records = module.parse_elf(repo / "build/u/src/game/chraidata.o")
assert generated == module.render(records)
assert len(records) == 18 and [record[1] for record in records] == list(range(18))
print("Global AI exactness: 18 matching decomp bytecode lists retained")
PY

for symbol in ge_original_stage_guard_actor_init_exact \
        ge_original_stage_guard_actor_construct_exact \
        ge_original_stage_guard_hat_apply_exact \
        ge_original_stage_guard_lighting_sample_exact \
        ge_original_stage_guard_lighting_step_exact \
        ge_original_stage_guard_runtime_actor \
        ge_original_stage_guard_runtime_bind_authored_hats \
        ge_original_stage_guard_runtime_update_lighting \
        ge_original_stage_guard_runtime_shadow \
        ge_original_global_ai_find; do
    nm -g "${test_dir}/test_ge_original_stage_guard_runtime" \
        | grep -E "_?${symbol}$" >/dev/null
done
