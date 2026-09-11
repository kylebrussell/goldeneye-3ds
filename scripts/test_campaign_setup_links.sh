#!/usr/bin/env bash
# Read existing private assets; write only disposable host test products.
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
pack=${1:-"${repo_dir}/build/host-tests/stage-assets/stages.gepack"}
test_dir=$(mktemp -d /tmp/ge-campaign-links.XXXXXX)
trap 'rm -rf "${test_dir}"' EXIT
python3 - "${repo_dir}" "${test_dir}/campaign_link_exact.inc" <<'PY'
import sys
from pathlib import Path
root=Path(sys.argv[1]); sys.path.insert(0,str(root/'scripts'))
from extract_guard_bullet_hit_slice import find_function as extract_function
obj=(root/'src/game/propobj.c').read_text()
chrprop=(root/'src/game/chrprop.c').read_text()
parts=[extract_function(obj,n) for n in ('objGetDestroyedLevel','objGetShotsTaken','objIsHealthy','objExplode','chrobjApplySpeed','updateDoorDisplacement','doorSetOpenState','doorActivate','doorActivateWrapper')]
parts += [extract_function(chrprop,n) for n in ('sub_GAME_7F03E6A0','doorIsPadlockFree')]
interactive=(root/'port/src/ge_original_stage_interactive_objects.c').read_text()
parts += [extract_function(interactive,n) for n in ('ge_interactive_set_blocker','ge_interactive_entry_for_record','ge_interactive_link_doors')]
parts.append(extract_function((root/'port/src/ge_original_door.c').read_text(),'ge_original_door_runtime_link_pair'))
Path(sys.argv[2]).write_text('\n\n'.join(parts))
PY
if [[ $(uname -s) == Darwin ]]; then strip=(-Wl,-dead_strip); else strip=(-Wl,--gc-sections); fi
for enum_mode in native short; do
    enum_flags=(-fno-short-enums)
    if [[ "${enum_mode}" == short ]]; then enum_flags=(-fshort-enums); fi
    printf 'Campaign setup links enum layout: %s\n' "${enum_mode}"
cc "${enum_flags[@]}" -std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-microsoft-anon-tag \
    -Wno-unused-variable -Wno-unused-parameter -Wno-incompatible-pointer-types -Wno-empty-body \
    -fsanitize=address,undefined -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA \
    -DGE_PORT_MS_INHERITS -DVERSION_US -fms-extensions \
    -I"${test_dir}" -I"${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    "${repo_dir}/port/tests/test_ge_campaign_setup_links.c" \
    "${repo_dir}/port/src/ge_original_stage_setup.c" \
    "${repo_dir}/port/src/ge_original_stage_switch_runtime.c" \
    "${repo_dir}/port/src/ge_original_stage_padlock_runtime.c" \
    "${repo_dir}/port/src/ge_original_stage_safe_runtime.c" \
    "${repo_dir}/port/src/ge_stage_assets.c" "${repo_dir}/port/src/ge_asset_pack.c" \
    "${strip[@]}" -lm -o "${test_dir}/test_links"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "${test_dir}/test_links" "${pack}"
done
