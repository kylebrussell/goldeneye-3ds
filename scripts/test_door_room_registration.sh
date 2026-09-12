#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "$0")/.." && pwd)
pack=${1:?Pass the actual installed campaign pack}
fixture_dir="${repo_dir}/build/host-tests/stage-assets"
test_dir="${repo_dir}/build/host-tests/door-room-registration"
if [[ ! -f "${pack}" ]]; then
    echo "missing installed campaign pack: ${pack}" >&2
    exit 1
fi
for dependency in ge_original_stage_guard_actor_slice.c ge_original_move_model_tables.c \
        ge_original_dam_monitor_animation_slice.c stan_geometry_slice.o \
        stanintersection_geometry_slice.o; do
    if [[ ! -f "${fixture_dir}/${dependency}" ]]; then
        echo "missing host fixture dependency: ${fixture_dir}/${dependency}" >&2
        echo "run scripts/test_stage_assets.sh first" >&2
        exit 1
    fi
done
dead_strip=(-Wl,--gc-sections)
if [[ "$(uname -s)" == Darwin ]]; then
    dead_strip=(-Wl,-dead_strip)
fi
mkdir -p "${test_dir}"
# Reuse the existing real-model fixture dependencies; do not regenerate packs.
python3 "${repo_dir}/scripts/extract_door_collision_slice.py" "${repo_dir}" "${test_dir}/collision.c"
python3 - "${repo_dir}" "${test_dir}" <<'PYGEN'
from pathlib import Path
import sys
repo=Path(sys.argv[1]);out=Path(sys.argv[2]);sys.path.insert(0,str(repo/'scripts'))
from extract_door_collision_slice import extract_function
text='#include "game/chrai.h"\nextern PropRecord *g_ActivePropsHead,*g_ActivePropsTail,*g_FreeProps;\n'
for name in ('chrpropDelist','chrpropDisable','chrpropFree'):
    text+=extract_function((repo/'src/game/chrprop.c').read_text(),name)+'\n'
(out/'teardown.c').write_text(text)
PYGEN
for abi in native short; do
    enum_flag=-fno-short-enums
    if [[ ${abi} == short ]]; then enum_flag=-fshort-enums; fi
cc -std=gnu11 -w -Wno-unused-parameter \
    -Wno-unused-variable -Wno-unused-but-set-variable -Wno-empty-body \
    -Wno-int-conversion -Wno-pointer-to-int-cast \
    -Wno-incompatible-pointer-types \
    -Wno-int-to-pointer-cast -Wno-sometimes-uninitialized \
    -ffunction-sections -fdata-sections -fsanitize=address,undefined -fno-omit-frame-pointer "${enum_flag}" \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -DVERSION_US \
    -fms-extensions \
    -I"${repo_dir}" -I"${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -I"${repo_dir}/src/game" \
    "${repo_dir}/port/tests/test_ge_original_door_room_registration.c" \
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
    "${fixture_dir}/ge_original_stage_guard_actor_slice.c" \
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
    "${fixture_dir}/ge_original_move_model_tables.c" \
    "${fixture_dir}/ge_original_dam_monitor_animation_slice.c" \
    "${fixture_dir}/stan_geometry_slice.o" \
    "${fixture_dir}/stanintersection_geometry_slice.o" \
    -lm "${dead_strip[@]}" \
    "${test_dir}/collision.c" "${repo_dir}/port/src/ge_original_prop_state.c" "${test_dir}/teardown.c" -o "${test_dir}/room-test-${abi}"
    "${test_dir}/room-test-${abi}" "${pack}"
done
