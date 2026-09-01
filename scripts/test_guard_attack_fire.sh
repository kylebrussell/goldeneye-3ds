#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=$(mktemp -d /tmp/ge-guard-attack-fire.XXXXXX)
generated_c="${build_dir}/ge_original_guard_attack_fire_slice.c"
trap 'rm -rf "${build_dir}"' EXIT

python3 "${repo_dir}/scripts/extract_guard_attack_fire_slice.py" \
    "${repo_dir}" "${generated_c}"

if [[ "$(uname -s)" == Darwin ]]; then
    dead_strip=(-Wl,-dead_strip)
else
    dead_strip=(-Wl,--gc-sections)
fi

cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-pragma-pack \
    -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function \
    -Wno-unused-but-set-variable -Wno-incompatible-pointer-types \
    -Wno-empty-body -Wno-int-conversion -Wno-pointer-to-int-cast \
    -Wno-int-to-pointer-cast -Wno-missing-braces -Wno-sign-compare \
    -Wno-uninitialized -Wno-unused-value -ffunction-sections \
    -fdata-sections -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -fms-extensions \
    -DVERSION_US -DBUGFIX_R0 -DPLAYERFLAG=int \
    -I "${repo_dir}" -I "${repo_dir}/src/game" \
    -I "${repo_dir}/port/include" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" "${generated_c}" \
    "${repo_dir}/port/tests/test_ge_original_guard_attack_fire.c" \
    -fsanitize=address,undefined -lm "${dead_strip[@]}" \
    -o "${build_dir}/test_ge_original_guard_attack_fire"

"${build_dir}/test_ge_original_guard_attack_fire"

python3 - "${repo_dir}" "${generated_c}" <<'PY'
import importlib.util
import sys
from pathlib import Path

repo = Path(sys.argv[1])
generated = Path(sys.argv[2]).read_text()
sys.path.insert(0, str(repo / "scripts"))
spec = importlib.util.spec_from_file_location(
    "attack_extract", repo / "scripts/extract_guard_attack_fire_slice.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
from extract_dam_guard_chr_scheduler_slice import function_text

for path, functions in (
    (repo / "src/game/chraction.c", module.CHRACTION_FUNCTIONS),
    (repo / "src/game/bondview2.c", module.BONDVIEW_FUNCTIONS),
    (repo / "src/game/gun.c", module.GUN_FUNCTIONS),
):
    source = path.read_text()
    for name in functions:
        assert function_text(source, name) in generated, name
print("guard AK47 exactness: unchanged attack, hitscan, damage and weapon-stat bodies retained")
PY
