#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
background="${repo_dir}/build/u/assets/obseg/bg/bg_dam_all_p.bin"
pack="${repo_dir}/build/3ds-assets/goldeneye.u.gepack"
report_dir="${repo_dir}/build/visual-probe"
report="${report_dir}/dam-route-capacity.json"
test_dir=$(mktemp -d)
trap 'rm -rf "${test_dir}"' EXIT

if [[ ! -f "${background}" || ! -f "${pack}" ]]; then
    echo "Dam background and 3DS asset pack are required" >&2
    exit 2
fi

read_capacity() {
    local name=$1
    sed -n "s/^#define ${name} \\([0-9][0-9]*\\)U$/\\1/p" \
        "${repo_dir}/platform/3ds/source/main.c"
}
texture_capacity=$(read_capacity DAM_SCENE_TEXTURE_CAPACITY)
scene_capacity=$(read_capacity DAM_SCENE_PROJECTED_VERTEX_CAPACITY)
initial_room_capacity=$(read_capacity DAM_WORLD_ROOM_LOAD_CAPACITY)
for capacity in "${texture_capacity}" "${scene_capacity}" \
        "${initial_room_capacity}"; do
    if [[ ! "${capacity}" =~ ^[0-9]+$ ]]; then
        echo "could not read configured Dam 3DS capacity" >&2
        exit 2
    fi
done

python3 "${repo_dir}/scripts/generate_dam_visual_tour.py" \
    --route main --directions forward --frames 1 \
    --output "${test_dir}/main.geview" \
    --manifest "${test_dir}/main.json"
python3 "${repo_dir}/scripts/generate_dam_visual_tour.py" \
    --route objectives --directions forward --frames 1 \
    --output "${test_dir}/objectives.geview" \
    --manifest "${test_dir}/objectives.json"

python3 "${repo_dir}/scripts/extract_bg_visibility_slice.py" \
    "${repo_dir}/src/game/bg.c" \
    "${test_dir}/ge_original_bg_visibility_slice.c"
python3 "${repo_dir}/scripts/extract_stage_environment_tables.py" \
    "${repo_dir}" \
    "${test_dir}/ge_original_stage_environment_tables.c"

camera_flags=(
    -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter
    -Wno-pointer-to-int-cast -ffunction-sections -fdata-sections
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_BOND_CAMERA_SLICE
    -DAIPARSE
    -I "${repo_dir}/port/include"
    -idirafter "${repo_dir}/include"
    -idirafter "${repo_dir}/include/PR"
    -idirafter "${repo_dir}/src"
    -iquote "${repo_dir}"
)
cc "${camera_flags[@]}" -c "${repo_dir}/src/game/bondview2.c" \
    -o "${test_dir}/bondview2-camera.o"
cc "${camera_flags[@]}" -c "${repo_dir}/port/src/ge_original_bond_camera.c" \
    -o "${test_dir}/bond-camera-adapter.o"
cc "${camera_flags[@]/-Werror/-Wno-error}" \
    -c "${repo_dir}/src/game/matrixmath.c" \
    -o "${test_dir}/matrixmath.o"
cc "${camera_flags[@]}" -c "${repo_dir}/src/libultra/gu/perspective.c" \
    -o "${test_dir}/perspective.o"
cc "${camera_flags[@]}" -c "${repo_dir}/src/libultra/gu/mtxutil.c" \
    -o "${test_dir}/mtxutil.o"
cc "${camera_flags[@]}" -c "${repo_dir}/src/libultra/gu/lookatref.c" \
    -o "${test_dir}/lookatref.o"

if [[ $(uname -s) == Darwin ]]; then
    dead_strip=(-Wl,-dead_strip)
else
    dead_strip=(-Wl,--gc-sections)
fi

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-unused-variable -Wno-empty-body -Wno-incompatible-pointer-types \
    -Wno-return-type -Wno-missing-braces -ffunction-sections -fdata-sections \
    -DVERSION_US -DLEFTOVERDEBUG -DGE_PORT_HOST_ABI \
    -DGE_PORT_BG_VISIBILITY_AVAILABLE \
    -DGE_ROUTE_VERTEX_CAPACITY="${scene_capacity}"U \
    -DGE_ROUTE_BATCH_CAPACITY="${scene_capacity}"U \
    -DGE_ROUTE_INITIAL_ROOM_CAPACITY="${initial_room_capacity}"U \
    -I "${repo_dir}/port/include" -I "${repo_dir}/src" \
    -I "${repo_dir}/src/game" \
    "${repo_dir}/port/tools/ge_dam_route_capacity_probe.c" \
    "${repo_dir}/port/src/ge_dam_dynamic_scene.c" \
    "${repo_dir}/port/src/ge_draw_batch_visibility.c" \
    "${repo_dir}/port/src/ge_dam_preload_queue.c" \
    "${repo_dir}/port/src/ge_asset_pack.c" \
    "${repo_dir}/port/src/ge_stage_assets.c" \
    "${repo_dir}/port/src/ge_dam_world.c" \
    "${repo_dir}/port/src/ge_dam_visibility_runtime.c" \
    "${repo_dir}/port/src/ge_original_stage_environment.c" \
    "${test_dir}/ge_original_stage_environment_tables.c" \
    "${repo_dir}/port/src/ge_original_bg_visibility.c" \
    "${test_dir}/ge_original_bg_visibility_slice.c" \
    "${repo_dir}/port/src/ge_gbi_decoder.c" \
    "${repo_dir}/port/src/ge_gbi_matrix.c" \
    "${repo_dir}/port/src/ge_gbi_rsp.c" \
    "${repo_dir}/port/src/ge_gbi_traverse.c" \
    "${repo_dir}/port/src/ge_gbi_state.c" \
    "${repo_dir}/port/src/ge_gbi_vertex.c" \
    "${repo_dir}/port/src/ge_gbi_pipeline.c" \
    "${repo_dir}/port/src/ge_pica_material.c" \
    "${repo_dir}/port/src/ge_dam_room.c" \
    "${test_dir}/bondview2-camera.o" \
    "${test_dir}/bond-camera-adapter.o" \
    "${test_dir}/matrixmath.o" \
    "${test_dir}/perspective.o" \
    "${test_dir}/mtxutil.o" \
    "${test_dir}/lookatref.o" \
    -lm "${dead_strip[@]}" -o "${test_dir}/route-capacity-probe"

main_result=$(cd "${repo_dir}" && "${test_dir}/route-capacity-probe" \
    main "${background}" "${pack}" "${test_dir}/main.geview" \
    "${texture_capacity}")
objective_result=$(cd "${repo_dir}" && "${test_dir}/route-capacity-probe" \
    objectives "${background}" "${pack}" "${test_dir}/objectives.geview" \
    "${texture_capacity}")

# The probe itself must reject a build whose configured GPU texture slots are
# below the measured authored pressure. This negative pass guards the failure
# contract independently of the currently generous production value.
if (cd "${repo_dir}" && "${test_dir}/route-capacity-probe" \
        main "${background}" "${pack}" "${test_dir}/main.geview" 1 \
        >/dev/null 2>&1); then
    echo "Dam route capacity probe accepted an insufficient texture limit" >&2
    exit 1
fi

mkdir -p "${report_dir}"
python3 - "${report}" "${main_result}" "${objective_result}" <<'PY'
import json
from pathlib import Path
import sys

output = Path(sys.argv[1])
routes = [json.loads(sys.argv[2]), json.loads(sys.argv[3])]
document = {"schema": 1, "stage": "Dam", "routes": routes}
output.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n")
for route in routes:
    if not route["capacity_ok"]:
        raise SystemExit(f"{route['route']} exceeds configured 3DS capacity")
    if route["peak_resident_rooms"] != len(route["resident_rooms"]):
        raise SystemExit(f"{route['route']} resident-room accounting diverged")
    if route["explicit_requests"] != len(route["explicit_rooms"]):
        raise SystemExit(f"{route['route']} explicit-room accounting diverged")
    if route["visibility_preloads_accepted"] != len(
            route["visibility_preload_rooms"]):
        raise SystemExit(f"{route['route']} visibility preload accounting diverged")
    if set(route["explicit_rooms"]) & set(route["visibility_preload_rooms"]):
        raise SystemExit(f"{route['route']} classified a preload twice")
    print(
        f"{route['route']}: {route['peak_resident_rooms']}/"
        f"{route['room_capacity']} rooms, "
        f"{route['peak_unique_rare_textures']}/"
        f"{route['texture_capacity']} Rare textures, "
        f"{route['visibility_preloads_accepted']} visibility preloads"
    )
PY

echo "Dam route capacity report: ${report}"
