#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/ge-embedment-pool.XXXXXX")
trap 'rm -rf "${test_dir}"' EXIT

cc -std=gnu11 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -fms-extensions \
    -I"${repo_dir}" -I"${repo_dir}/src/game" \
    -I"${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    "${repo_dir}/port/src/ge_original_embedment_pool.c" \
    "${repo_dir}/port/tests/test_ge_original_embedment_pool.c" \
    -o "${test_dir}/test_ge_original_embedment_pool"

"${test_dir}/test_ge_original_embedment_pool"

python3 - "${repo_dir}" <<'PY'
import re
import sys
from pathlib import Path

repo = Path(sys.argv[1])
source = (repo / "src/game/initobjects.c").read_text()
match = re.search(
    r"for \(i = 0; i < EMBEDMENT_ARR_MAX; i\+\+\)\s*"
    r"\{\s*g_Embedments\[i\]\.flags = 1;\s*\}", source)
assert match is not None
port = (repo / "port/src/ge_original_embedment_pool.c").read_text()
assert "index < EMBEDMENT_ARR_MAX" in port
assert "g_Embedments[index].flags = EMBEDMENTFLAG_FREE" in port
print("embedment reset exactness: canonical initobjects free-marker loop retained")
PY
