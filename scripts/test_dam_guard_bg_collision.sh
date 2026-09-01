#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=$(mktemp -d /tmp/ge-dam-bg-collision.XXXXXX)
generated_c="${build_dir}/ge_original_dam_guard_bg_collision_test_slice.c"
trap 'rm -rf "${build_dir}"' EXIT

python3 "${repo_dir}/scripts/extract_dam_guard_bg_collision_test_slice.py" \
    "${repo_dir}" "${generated_c}"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-pragma-pack \
    -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function \
    -Wno-unused-but-set-variable -Wno-incompatible-pointer-types \
    -Wno-empty-body -Wno-int-conversion -Wno-pointer-to-int-cast \
    -Wno-int-to-pointer-cast -Wno-uninitialized \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DVERSION_US -DPLAYERFLAG=int \
    -I "${repo_dir}" -I "${repo_dir}/src/game" \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    "${generated_c}" \
    "${repo_dir}/port/tests/test_ge_original_dam_guard_bg_collision.c" \
    -fsanitize=address,undefined -lm \
    -o "${build_dir}/test_ge_original_dam_guard_bg_collision"

"${build_dir}/test_ge_original_dam_guard_bg_collision"

python3 - "${repo_dir}" "${generated_c}" <<'PY'
import importlib.util
import sys
from pathlib import Path

repo = Path(sys.argv[1])
generated = Path(sys.argv[2]).read_text()
sys.path.insert(0, str(repo / "scripts"))
spec = importlib.util.spec_from_file_location(
    "bg_collision_extract",
    repo / "scripts/extract_dam_guard_bg_collision_test_slice.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
for filename, name in module.TARGETS:
    source = (repo / "src/game" / filename).read_text()
    assert module.function_text(source, name) in generated
support_spec = importlib.util.spec_from_file_location(
    "scheduler_support_extract",
    repo / "scripts/extract_dam_guard_chr_scheduler_support_slice.py")
support_module = importlib.util.module_from_spec(support_spec)
support_spec.loader.exec_module(support_module)
full_support = support_module.render(repo, True)
for source_key, name in (
    ("chrprop", "chraiUpdateOnscreenPropCount"),
    ("lightfixture", "check_if_imageID_is_light"),
    ("bg", "addToByteSetMaxSize15"),
    ("bg", "bgTestRayIntersectionInRoom"),
    ("bg", "bgFindRoomsAlongSegment"),
    ("bg", "bgCopyVisibleRoomsToList"),
    ("bg", "bgTestBulletHitBackground"),
    ("bg", "get_room_data_float2"),
    ("stan", "stanFindTileBelowPos"),
):
    source = (repo / "src/game" / support_module.SOURCE_FILES[source_key]).read_text()
    assert module.function_text(source, name) in full_support
assert "extern bool stanTileHasZeroArea(StandTile *tile);" in full_support
assert "extern void getTileMidPoint(StandTile *tile, coord3d *out);" in full_support
print("Dam scheduler visibility/background/STAN exactness: canonical STAN owner plus unchanged test bodies retained")
PY

printf 'Dam scheduler visibility/background/STAN: active sorting, stale-list removal, portal traversal, background hit and tile-below lookup\n'
