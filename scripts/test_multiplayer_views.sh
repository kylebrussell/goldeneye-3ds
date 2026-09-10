#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "${test_dir}"' EXIT
if [[ $(uname -s) == Darwin ]]; then
    dead_strip=(-Wl,-dead_strip)
else
    dead_strip=(-Wl,--gc-sections)
fi

# Check the source that defines the adapter's caller contract. The fixture
# below instruments that boundary; it does not replace original actor ticks.
python3 - "${repo_dir}" <<'PY'
import sys
from pathlib import Path
sys.path.insert(0, str(Path(sys.argv[1]) / "scripts"))
from extract_dam_guard_chr_scheduler_slice import function_text
root = Path(sys.argv[1])
lv = function_text((root / "src/game/lv.c").read_text(), "lvlRender")
ordered = ["bgRoomVisibilityRelated();", "propsTick();",
           "chraiUpdateOnscreenPropCount();", "chrpropUpdateAutoaimTarget();",
           "chraiCheckUseHeldItems();", "propsTickPlayer();", "maybe_mp_interface(DL)"]
offsets = [lv.index(token) for token in ordered]
assert offsets == sorted(offsets)
interface = function_text((root / "src/game/bondview2.c").read_text(), "maybe_mp_interface")
assert interface.index("gunUpdateAndFireBothHands();") < interface.index("gunRenderFirstPersonGunModels(&gdl);")
player = function_text((root / "src/game/bondview2.c").read_text(), "playerTick")
assert "chrTick(prop)" in player
print("original lvlRender: mono visibility -> props/hit lists -> targeting/interaction -> gun dispatch")
PY

for abi in native short-enum; do
    abi_flags=(-fno-short-enums)
    if [[ ${abi} == short-enum ]]; then abi_flags=(-fshort-enums); fi
    flags=("${abi_flags[@]}" -std=gnu11 -Wall -Wextra -Werror
        -Wno-comment -Wno-unused-parameter -Wno-unused-variable
        -Wno-unused-but-set-variable -Wno-pointer-to-int-cast
        -Wno-missing-braces -ffunction-sections -fdata-sections
        -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer
        -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA
        -I "${repo_dir}/port/include" -I "${repo_dir}/platform/3ds/include"
        -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR"
        -idirafter "${repo_dir}/src" -iquote "${repo_dir}")
    for source in src/game/bondview2.c port/src/ge_original_bond_camera.c \
            src/game/matrixmath.c src/libultra/gu/perspective.c src/libultra/gu/lookatref.c; do
        cc "${flags[@]}" -DGE_PORT_BOND_CAMERA_SLICE \
            -c "${repo_dir}/${source}" -o "${test_dir}/$(basename "${source}").o"
    done
    # Only the original guMtxF2L signed halfword-packing shift is exempted.
    cc "${flags[@]}" -DGE_PORT_BOND_CAMERA_SLICE -fno-sanitize=shift \
        -c "${repo_dir}/src/libultra/gu/mtxutil.c" -o "${test_dir}/mtxutil.o"
    cc "${flags[@]}" \
        "${repo_dir}/port/src/ge_original_bond_camera_live.c" \
        "${repo_dir}/port/src/ge_original_multiplayer_views.c" \
        "${repo_dir}/platform/3ds/source/ge_3ds_stereo.c" \
        "${repo_dir}/port/tests/test_ge_original_multiplayer_views.c" \
        "${test_dir}"/*.o -lm "${dead_strip[@]}" -o "${test_dir}/multiplayer-views"
    ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 "${test_dir}/multiplayer-views"
done
