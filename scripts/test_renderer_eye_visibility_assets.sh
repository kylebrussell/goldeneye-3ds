#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
# Optional: ./scripts/test_renderer_eye_visibility_assets.sh PACK BACKGROUND
# External inputs are copied individually; the shared build tree stays read-only.
pack_source=${1:-"${repo_dir}/build/3ds-assets/goldeneye.u.gepack"}
background_source=${2:-"${repo_dir}/build/u/assets/obseg/bg/bg_dam_all_p.bin"}
test_dir="${repo_dir}/build/host-tests/renderer-eye-visibility-assets"
mkdir -p "${test_dir}/inputs"
pack="${test_dir}/inputs/goldeneye.u.gepack"
background="${test_dir}/inputs/bg_dam_all_p.bin"
if [[ ! -f "${pack_source}" || ! -f "${background_source}" ]]; then
    echo "Private generated Dam background and 3DS pack are required (PACK BACKGROUND)." >&2
    exit 2
fi
cp "${pack_source}" "${pack}"
cp "${background_source}" "${background}"
cd "${repo_dir}"

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
    --route main --directions forward,left,right,back --frames 1 \
    --output "${test_dir}/main.geview" \
    --manifest "${test_dir}/main.json"
python3 "${repo_dir}/scripts/generate_dam_visual_tour.py" \
    --route objectives --directions forward,left,right,back --frames 1 \
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
    -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer
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
# Original guMtxF2L packs signed 16-bit halves with a C signed shift. Preserve
# its authored body while excluding only shift UB instrumentation in this TU.
cc "${camera_flags[@]}" -fno-sanitize=shift -c "${repo_dir}/src/libultra/gu/mtxutil.c" \
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
    -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer \
    -DVERSION_US -DLEFTOVERDEBUG -DGE_PORT_HOST_ABI \
    -DGE_PORT_BG_VISIBILITY_AVAILABLE \
    -DGE_ROUTE_VERTEX_CAPACITY="${scene_capacity}"U \
    -DGE_ROUTE_BATCH_CAPACITY="${scene_capacity}"U \
    -DGE_ROUTE_INITIAL_ROOM_CAPACITY="${initial_room_capacity}"U \
    -I "${repo_dir}/platform/3ds/include" -I "${test_dir}" \
    -I "${repo_dir}/port/include" -I "${repo_dir}/src" \
    -I "${repo_dir}/src/game" \
    "${repo_dir}/port/tools/ge_dam_renderer_visibility_probe.c" \
    "${repo_dir}/port/src/ge_dam_renderer_visibility.c" \
    "${repo_dir}/port/src/ge_renderer_bg_visibility.c" \
    "${repo_dir}/platform/3ds/source/ge_3ds_stereo.c" \
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
    -lm "${dead_strip[@]}" -o "${test_dir}/renderer-visibility-probe"

for route in main objectives; do
    "${test_dir}/renderer-visibility-probe" "${route}" "${background}" \
        "${pack}" "${test_dir}/${route}.geview" "${texture_capacity}" \
        > "${test_dir}/${route}-results.json"
done
# Confirm the geometry/texture pressure checks fail when deliberately underprovisioned.
if "${test_dir}/renderer-visibility-probe" main "${background}" "${pack}" \
        "${test_dir}/main.geview" 1 > /dev/null 2> "${test_dir}/negative-capacity.log"; then
    echo "private visibility probe accepted insufficient texture capacity" >&2
    exit 1
fi
if ! rg -q '^initial scene exceeds texture capacity$' "${test_dir}/negative-capacity.log"; then
    cat "${test_dir}/negative-capacity.log" >&2
    echo "negative capacity check failed for an unrelated reason" >&2
    exit 1
fi
# Removing only authored-preload protection must reproduce the original
# stationary mk13 unload/reload cycle, proving that this check detects it.
if "${test_dir}/renderer-visibility-probe" main "${background}" "${pack}" \
        "${test_dir}/main.geview" "${texture_capacity}" legacy-aging \
        > /dev/null 2> "${test_dir}/negative-preload-protection.log"; then
    echo "private visibility probe failed to detect authored preload churn" >&2
    exit 1
fi
if ! rg -q '^preload closure did not converge at route-main-02-mk13-forward;' \
        "${test_dir}/negative-preload-protection.log"; then
    cat "${test_dir}/negative-preload-protection.log" >&2
    echo "negative preload protection check failed for an unrelated reason" >&2
    exit 1
fi
python3 - "${test_dir}" "${repo_dir}" <<'PY'
import hashlib
import json
from pathlib import Path
import sys
root = Path(sys.argv[1])
repo = Path(sys.argv[2])
routes = [json.loads((root / f"{route}-results.json").read_text())
          for route in ("main", "objectives")]
if not sum(route["left_only_views"] for route in routes):
    raise SystemExit("No actual left-only authored room edge was exercised")
if not sum(route["right_only_views"] for route in routes):
    raise SystemExit("No actual right-only authored room edge was exercised")
if not sum(route["eye_only_installs"] for route in routes):
    raise SystemExit("No actual eye-only authored geometry installation was exercised")
for route in routes:
    if not route["frame_aging"] or not route["protect_preloads"]:
        raise SystemExit("positive evidence did not exercise frame aging and preload protection")
    if route["stationary_hold_checks"] != route["view_count"] * 6:
        raise SystemExit("not every authored camera held beyond the unload-age window")
    if route["released_preload_rooms"] == 0:
        raise SystemExit("moving cameras did not release authored preload protection")
source_paths = ("src/game/bg.c", "src/game/bondview2.c", "src/libultra/gu/mtxutil.c",
                "port/src/ge_original_bg_visibility.c", "port/src/ge_dam_renderer_visibility.c",
                "port/src/ge_dam_preload_queue.c", "port/src/ge_dam_dynamic_scene.c",
                "platform/3ds/source/ge_3ds_stereo.c",
                "port/tools/ge_dam_renderer_visibility_probe.c")
report = {"schema": 1, "evidence": "original-assets-host-visibility",
          "n64_pixel_comparison": False, "hardware_acceptance": False,
          "closure_policy": "age-every-install-frame; protect-current-authored-preloads; six-stationary-hold-frames",
          "sanitizers": ["address", "undefined (guMtxF2L shift excluded)"],
          "inputs_sha256": {file.name: hashlib.sha256(file.read_bytes()).hexdigest()
                            for file in (root / "inputs").iterdir()},
          "source_sha256": {path: hashlib.sha256((repo / path).read_bytes()).hexdigest()
                            for path in source_paths},
          "negative_controls": ["insufficient-texture-capacity", "authored-preload-churn"],
          "routes": routes}
(root / "report.json").write_text(json.dumps(report, indent=2, sort_keys=True) + "\n")
for route in routes:
    print(f"{route['route']}: {route['view_count']} views, "
          f"{route['isolation_checks']} publication/mono checks, "
          f"{route['left_only_views']}/{route['right_only_views']} left/right-only edges, "
          f"{route['eye_only_installs']} eye-only room installs; "
          f"peak {route['peak_vertices']}/{route['vertex_capacity']} vertices, "
          f"{route['peak_textures']}/{route['texture_capacity']} textures")
print(f"Private asset software evidence: {root / 'report.json'}")
PY
