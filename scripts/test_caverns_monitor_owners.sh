#!/usr/bin/env bash

set -euo pipefail

repo_dir=${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
asset_pack=${2:-"${repo_dir}/build/host-tests/stage-assets/stages.gepack"}
test_dir="${repo_dir}/build/host-tests/caverns-monitor-owners"
mkdir -p "${test_dir}"

if [[ ! -f "${asset_pack}" ]]; then
    echo "missing campaign test asset pack: ${asset_pack}" >&2
    echo "run scripts/test_stage_assets.sh first" >&2
    exit 1
fi

dead_strip=(-Wl,--gc-sections)
if [[ "$(uname -s)" == Darwin ]]; then
    dead_strip=(-Wl,-dead_strip)
fi

cc -std=gnu11 -Wall -Wextra -Werror \
    -Wno-unused-parameter -Wno-unused-variable \
    -Wno-incompatible-pointer-types -Wno-int-conversion \
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -DVERSION_US \
    -fms-extensions -I"${repo_dir}" -I"${repo_dir}/port/include" \
    -I"${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    "${repo_dir}/port/tests/test_ge_original_caverns_monitor_owners.c" \
    "${repo_dir}/port/src/ge_original_stage_monitor.c" \
    "${repo_dir}/port/src/ge_original_stage_setup.c" \
    "${repo_dir}/port/src/ge_original_stage_prop_materializer.c" \
    "${repo_dir}/port/src/ge_original_stage_prop_native.c" \
    "${repo_dir}/port/src/ge_stage_assets.c" \
    "${repo_dir}/port/src/ge_asset_pack.c" \
    -lm "${dead_strip[@]}" \
    -o "${test_dir}/test_ge_original_caverns_monitor_owners"

"${test_dir}/test_ge_original_caverns_monitor_owners" "${asset_pack}"
