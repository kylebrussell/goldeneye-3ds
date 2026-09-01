#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
collision_asset=${1:-"${repo_dir}/build/3ds-levels/dam/collision/collision.gestan"}
test_dir=$(mktemp -d)
trap 'rm -rf "${test_dir}"' EXIT

common_flags=(
    -std=c11 -Wall -Wextra -Werror -ffunction-sections -fdata-sections
    -fsanitize=address,undefined -fno-omit-frame-pointer
    -DGE_PORT_STAN_GEOMETRY_SLICE
    -I "${repo_dir}/port/include"
)

cc "${common_flags[@]}" -Wno-uninitialized -Wno-unused-variable \
    -Wno-unused-parameter -Wno-empty-body \
    -DGE_PORT_STAN_DYNAMIC_PROP_COLLISION \
    -DGE_PORT_BOND_MOVEMENT_SLICE \
    -c "${repo_dir}/src/game/stan.c" -o "${test_dir}/stan.o"

cc "${common_flags[@]}" -DGE_PORT_BOND_MOVEMENT_SLICE \
    -c "${repo_dir}/src/game/stanintersection.c" \
    -o "${test_dir}/stanintersection.o"

cc "${common_flags[@]}" -pedantic -Wconversion -Wsign-conversion -Wshadow \
    -DGE_PORT_STAN_DYNAMIC_PROP_COLLISION \
    "${repo_dir}/port/src/ge_stan_collision.c" \
    "${repo_dir}/port/src/ge_stan_native.c" \
    "${repo_dir}/port/tests/test_ge_stan_collision.c" \
    "${test_dir}/stan.o" "${test_dir}/stanintersection.o" \
    -lm -o "${test_dir}/test_guard_stan_dynamic_los"

"${test_dir}/test_guard_stan_dynamic_los" "${collision_asset}"
