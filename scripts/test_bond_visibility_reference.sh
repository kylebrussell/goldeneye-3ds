#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT
dead_strip=(-Wl,--gc-sections)
if [[ $(uname -s) == Darwin ]]; then dead_strip=(-Wl,-dead_strip); fi
common=(
    -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable
    -Wno-incompatible-pointer-types -Wno-missing-braces
    -ffunction-sections -fdata-sections -fshort-enums
    -fsanitize=address,undefined -fno-omit-frame-pointer
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE
    -DGE_PORT_SETUP_DATA -DVERSION_US -DBUGFIX_R0
    -DGE_PORT_MS_INHERITS -fms-extensions
    -I "$repo_dir" -I "$repo_dir/port/include"
    -I "$repo_dir/src/game" -idirafter "$repo_dir/include"
    -idirafter "$repo_dir/include/PR" -idirafter "$repo_dir/src"
)
cc "${common[@]}" -c "$repo_dir/port/src/ge_original_bond_live.c" -o "$test_dir/live.o"
cc "${common[@]}" -c "$repo_dir/port/tests/test_ge_original_bond_visibility_reference.c" -o "$test_dir/test.o"
cc -fsanitize=address,undefined "${dead_strip[@]}" "$test_dir/live.o" "$test_dir/test.o" -o "$test_dir/test"
"$test_dir/test"
