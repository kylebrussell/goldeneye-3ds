#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=$(mktemp -d /tmp/ge-facility-flow.XXXXXX)
trap 'rm -rf "${build_dir}"' EXIT

if [[ "$(uname -s)" == Darwin ]]; then
    dead_strip=(-Wl,-dead_strip)
else
    dead_strip=(-Wl,--gc-sections)
fi

common=(
    -std=gnu11 -Wall -Wextra -Werror
    -Wno-comment -Wno-pragma-pack -Wno-missing-braces -Wno-unused-parameter
    -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable
    -Wno-incompatible-pointer-types -Wno-empty-body -Wno-int-conversion
    -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast
    -ffunction-sections -fdata-sections
    -fsanitize=address,undefined -fno-omit-frame-pointer
    -fms-extensions -DGE_PORT_MS_INHERITS
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE
    -DGE_PORT_SETUP_DATA -DVERSION_US -DPLAYERFLAG=int
    -I "${repo_dir}" -I "${repo_dir}/src/game"
    -I "${repo_dir}/port/include"
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR"
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}"
)

python3 "${repo_dir}/scripts/tests/test_facility_mission_exit_contract.py"
python3 "${repo_dir}/scripts/extract_global_ai_lists.py" \
    "${repo_dir}/build/u/src/game/chraidata.o" \
    "${build_dir}/ge_original_global_ai.c"

cc "${common[@]}" -DGE_PORT_DAM_MISSION_FLOW_SLICE \
    -DGE_PORT_FACILITY_MISSION_FLOW_SLICE \
    -DGE_PORT_DAM_MISSION_SFX_SLICE \
    -c "${repo_dir}/src/game/chrai.c" -o "${build_dir}/chrai.o"
cc "${common[@]}" -DGE_PORT_DAM_MISSION_FLOW_SLICE \
    -DGE_PORT_FACILITY_MISSION_FLOW_SLICE \
    -DGE_PORT_DAM_MISSION_SFX_SLICE -E -P \
    "${repo_dir}/src/game/chrai.c" -o "${build_dir}/chrai.i"
python3 - "${build_dir}/chrai.i" <<'PY'
from pathlib import Path
import re
import sys

unit = Path(sys.argv[1]).read_text()
source = unit[unit.index("void ai("):]
actual = set(re.findall(r"case (AI_[A-Za-z0-9_]+):", source))
base = {
    "AI_GotoNext", "AI_GotoFirst", "AI_Label", "AI_Yield", "AI_EndList",
    "AI_IFKeyDropped", "AI_IFItemIsAttachedToObject",
    "AI_IFObjectNotDestroyed", "AI_SetObjectiveBitfield",
    "AI_IFObjectiveBitfieldHas", "AI_TextPrintTop", "AI_SfxPlay",
    "AI_SfxEmitFromObject",
}
facility = {
    "AI_SetChrAiList", "AI_PlayAnimation", "AI_IFImOnPatrolOrStopped",
    "AI_IFBondInRoomWithPad", "AI_IFBondHasItemEquipped", "AI_SetMyFlags2",
    "AI_UnsetMyFlags2", "AI_IFMyFlags2Has", "AI_MyTimerStart",
    "AI_IFMyTimerGreaterThanTicks", "AI_EndLevel", "AI_CameraSwitch",
    "AI_BondDisableControl", "AI_TRYTeleportingChrToPad",
    "AI_ScreenFadeToBlack", "AI_ScreenFadeFromBlack",
    "AI_IFScreenFadeCompleted", "AI_HideAllChrs", "AI_DoorOpenInstant",
    "AI_ChrRemoveItemInHand", "AI_BondEquipItemCinema",
    "AI_TriggerFadeAndExitLevelOnButtonPress", "AI_IFBondIsDead",
    "AI_BondDisableDamageAndPickups", "AI_BondHideWeapons",
    "AI_IFObjectiveAllCompleted",
}
assert len(base) == 13 and len(facility) == 26
assert actual == base | facility, (sorted(actual - base - facility),
                                   sorted(base | facility - actual))
assert "ge_original_global_ai_find" in unit
print("Facility interpreter boundary: 13 base + 26 exact Facility cases")
PY
cc "${common[@]}" -c "${repo_dir}/assets/obseg/setup/UsetuparkZ.c" \
    -o "${build_dir}/UsetuparkZ.o"
cc "${common[@]}" -c "${build_dir}/ge_original_global_ai.c" \
    -o "${build_dir}/ge_original_global_ai.o"
cc "${common[@]}" -c \
    "${repo_dir}/port/tests/test_ge_original_facility_mission_flow.c" \
    -o "${build_dir}/test.o"

cc "${build_dir}/chrai.o" "${build_dir}/UsetuparkZ.o" \
    "${build_dir}/ge_original_global_ai.o" "${build_dir}/test.o" \
    -fsanitize=address,undefined "${dead_strip[@]}" -lm \
    -o "${build_dir}/test_ge_original_facility_mission_flow"
"${build_dir}/test_ge_original_facility_mission_flow"

for symbol in ai ailistFindById ge_original_global_ai_find ai_37 ai_38 ai_47; do
    nm -g "${build_dir}/test_ge_original_facility_mission_flow" \
        | grep -E "_?${symbol}$" >/dev/null
done

printf 'Facility retained symbols: exact interpreter, global owner, ai_37/ai_38/ai_47\n'
