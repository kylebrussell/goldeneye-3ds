#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=$(mktemp -d /tmp/ge-dam-projectile-explosion.XXXXXX)
generated_c="${build_dir}/ge_original_dam_projectile_explosion_slice.c"
trap 'rm -rf "${build_dir}"' EXIT

python3 "${repo_dir}/scripts/extract_dam_projectile_explosion_slice.py" \
    "${repo_dir}" "${generated_c}"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-pragma-pack \
    -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function \
    -Wno-unused-but-set-variable -Wno-incompatible-pointer-types \
    -Wno-empty-body -Wno-int-conversion -Wno-pointer-to-int-cast \
    -Wno-int-to-pointer-cast -Wno-missing-braces -Wno-sign-compare \
    -Wno-uninitialized -Wno-unused-value -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -fms-extensions \
    -DVERSION_US -DBUGFIX_R0 -DPLAYERFLAG=int \
    -I "${repo_dir}" -I "${repo_dir}/src/game" \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    "${generated_c}" \
    "${repo_dir}/port/tests/test_ge_original_dam_projectile_explosion.c" \
    -fsanitize=address,undefined -lm \
    -o "${build_dir}/test_ge_original_dam_projectile_explosion"

"${build_dir}/test_ge_original_dam_projectile_explosion"

python3 - "${repo_dir}" "${generated_c}" <<'PY'
import importlib.util
import sys
from pathlib import Path

repo = Path(sys.argv[1])
generated = Path(sys.argv[2]).read_text()
spec = importlib.util.spec_from_file_location(
    "projectile_explosion_extract",
    repo / "scripts/extract_dam_projectile_explosion_slice.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
for path, name in module.FUNCTIONS:
    source = (repo / path).read_text()
    assert module.extract_function(source, name) in generated
print(f"Dam projectile/explosion exactness: {len(module.FUNCTIONS)} unchanged bodies retained")
PY

for symbol in propExplode projectileFindCollidingProp \
        projectileTestObjectCollisionRecursive modelFindNextProjectileHitCandidate; do
    nm -g "${build_dir}/test_ge_original_dam_projectile_explosion" \
        | grep -E "_?${symbol}$" >/dev/null
done
