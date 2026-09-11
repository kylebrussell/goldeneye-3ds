#!/usr/bin/env bash
# Supply an existing private stage asset pack. No emulator/config/asset writes.
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
pack=${1:-"${repo_dir}/build/host-tests/stage-assets/stages.gepack"}
if [[ ! -f "${pack}" ]]; then
    echo "Campaign fixtures require a generated stage pack: $pack" >&2
    exit 2
fi
build_dir=$(mktemp -d /tmp/ge-campaign-acceptance.XXXXXX)
trap 'rm -rf "${build_dir}"' EXIT
if [[ $(uname -s) == Darwin ]]; then
    dead_strip=(-Wl,-dead_strip)
else
    dead_strip=(-Wl,--gc-sections)
fi
for enum_mode in native short; do
    enum_flags=(-fno-short-enums)
    if [[ "${enum_mode}" == short ]]; then enum_flags=(-fshort-enums); fi
    printf 'Campaign acceptance enum layout: %s\n' "${enum_mode}"
cc "${enum_flags[@]}" -std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-pragma-pack \
    -Wno-unused-parameter -Wno-missing-braces -Wno-pointer-sign \
    -Wno-incompatible-pointer-types -Wno-int-conversion \
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DVERSION_US \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -DPLAYERFLAG=int \
    -DBUGFIX_R0 -fms-extensions \
    -I"${repo_dir}/port/include" -I"${repo_dir}/src/game" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    "${repo_dir}/port/tests/test_ge_campaign_acceptance.c" \
    "${repo_dir}/port/src/ge_original_stage_objective_runtime.c" \
    "${repo_dir}/port/src/ge_original_stage_objective_live.c" \
    "${repo_dir}/port/src/ge_original_stage_objectives.c" \
    "${repo_dir}/port/src/ge_original_stage_setup.c" \
    "${repo_dir}/port/src/ge_stage_assets.c" \
    "${repo_dir}/port/src/ge_asset_pack.c" \
    "${repo_dir}/port/src/ge_original_mission_result.c" \
    "${repo_dir}/port/src/ge_original_dam_mission_exit_services.c" \
    "${repo_dir}/port/src/ge_original_dam_objective_status.c" \
    "${repo_dir}/port/src/ge_3ds_save_provider.c" \
    "${repo_dir}/port/src/random_port.c" \
    "${dead_strip[@]}" -lm -o "${build_dir}/test_ge_campaign_acceptance"
UBSAN_OPTIONS="${UBSAN_OPTIONS:+${UBSAN_OPTIONS}:}halt_on_error=1" \
    "${build_dir}/test_ge_campaign_acceptance" "${pack}"
done
