#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=$(mktemp -d /tmp/ge-texture-hit-type.XXXXXX)
trap 'rm -rf "${build_dir}"' EXIT
python3 - "${repo_dir}" <<'PY'
import re,sys
from pathlib import Path
root=Path(sys.argv[1]); source=(root/'src/game/image.c').read_text()
prefix=re.search(r's32 D_800492E4\[\]\s*=\s*\{([^}]+)\};',source)
assert prefix is not None and re.findall(r'\d+',prefix[1]) == ['0']*7
between=source[prefix.end():source.index('struct image_entry g_Textures[]')]
assert '//D:80049300' in between and ';' not in between
assert '//d:800492e4' in source[:prefix.start()].lower()
print('original image.c: seven zero predecessor words ending at original US g_Textures address')
PY
for abi in native short-enum; do
    abi_flags=(-fno-short-enums)
    if [[ ${abi} == short-enum ]]; then abi_flags=(-fshort-enums); fi
    cc "${abi_flags[@]}" -std=gnu11 -Wall -Wextra -Werror -Wno-comment \
        -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA -DAIPARSE \
        -I "${repo_dir}" -I "${repo_dir}/port/include" \
        -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
        -idirafter "${repo_dir}/src" \
        "${repo_dir}/port/src/ge_original_texture_hit_type.c" \
        "${repo_dir}/port/tests/test_ge_original_texture_hit_type.c" \
        -fsanitize=address,undefined -fno-sanitize-recover=all \
        -o "${build_dir}/test_texture_hit_type"
    "${build_dir}/test_texture_hit_type"
done
