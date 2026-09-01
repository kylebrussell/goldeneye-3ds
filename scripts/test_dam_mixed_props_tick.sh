#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=$(mktemp -d /tmp/ge-dam-mixed-props.XXXXXX)
generated_c="${build_dir}/ge_original_dam_guard_chr_scheduler_slice.c"
trap 'rm -rf "${build_dir}"' EXIT

python3 - "${repo_dir}" "${generated_c}" <<'PY'
import importlib.util
import re
import sys
from pathlib import Path

repo = Path(sys.argv[1])
output = Path(sys.argv[2])
extractor_path = repo / "scripts/extract_dam_guard_chr_scheduler_slice.py"
spec = importlib.util.spec_from_file_location("scheduler", extractor_path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
body = module.function_text((repo / "src/game/chrprop.c").read_text(),
                            "propsTick")
output.write_text("\n".join([
    "/* Host-only unchanged canonical mixed-prop dispatcher slice. */",
    '#include "ge_original_dam_guard_chr_scheduler.h"',
    "#include <ultra64.h>",
    "#include <bondconstants.h>",
    "#include <bondtypes.h>",
    '#include "chrai.h"',
    '#include "chr.h"',
    '#include "player.h"',
    '#include "propobj.h"',
    "extern s32 ge_mixed_chr_tick_boundary(PropRecord *prop);",
    "extern s32 objTick(PropRecord *prop);",
    "extern s32 playerTick(PropRecord *prop);",
    "extern u8 explosionChrpropExplosionTick(PropRecord *prop);",
    "extern u8 explosionChrpropSmokeTick(PropRecord *prop);",
    "extern void handle_alarm_gas_timer_calldamage(void);",
    "extern void loop_set_sound_effect_all_slots(void);",
    "#define chrTick ge_mixed_chr_tick_boundary",
    "#define propsTick ge_original_dam_guard_props_tick_exact",
    body,
    "#undef propsTick",
    "#undef chrTick",
    "",
]))
print("generated unchanged mixed-prop propsTick dispatcher")
PY

dead_strip=(-Wl,--gc-sections)
if [[ "$(uname -s)" == "Darwin" ]]; then
    dead_strip=(-Wl,-dead_strip)
fi

cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-pragma-pack \
    -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function \
    -Wno-unused-but-set-variable -Wno-incompatible-pointer-types \
    -Wno-empty-body -Wno-int-conversion -Wno-pointer-to-int-cast \
    -Wno-int-to-pointer-cast -Wno-missing-braces \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DVERSION_US -DPLAYERFLAG=int \
    -DGE_PORT_MS_INHERITS -fms-extensions \
    -I "${repo_dir}" -I "${repo_dir}/src/game" \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    "${generated_c}" \
    "${repo_dir}/port/tests/test_ge_original_dam_mixed_props_tick.c" \
    "${repo_dir}/port/src/ge_original_stage_active_props.c" \
    "${repo_dir}/port/src/ge_dam_setup_world_materializer.c" \
    "${repo_dir}/port/src/ge_original_covert_modem_object.c" \
    "${repo_dir}/port/src/ge_original_bug_model.c" \
    "${repo_dir}/port/src/ge_original_objinit_source.c" \
    "${repo_dir}/assets/obseg/setup/UsetupdamZ.c" \
    -fsanitize=address,undefined "${dead_strip[@]}" -lm \
    -o "${build_dir}/test_ge_original_dam_mixed_props_tick"

"${build_dir}/test_ge_original_dam_mixed_props_tick"

python3 - "${repo_dir}" <<'PY'
import importlib.util
import re
import sys
from pathlib import Path

repo = Path(sys.argv[1])
extractor_path = repo / "scripts/extract_dam_guard_chr_scheduler_slice.py"
spec = importlib.util.spec_from_file_location("scheduler", extractor_path)
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
player = module.function_text(
    (repo / "src/game/bondview2.c").read_text(), "playerTick")
obj = module.function_text(
    (repo / "src/game/propobj.c").read_text(), "objTick")
assert "prop->flags &= ~PROPFLAG_ONSCREEN" in player
assert "MoveBond(" not in player
assert "obj->type == PROPDEF_DOOR" in obj
assert "RUNTIMEBITFLAG_HASPROJECTILE" in obj
assert "RUNTIMEBITFLAG_ISRETICK" in obj
assert re.search(r"RUNTIMEBITFLAG_ISRETICK;\s*return 3;", obj)
print("source audit: playerTick clears VIEWER onscreen without MoveBond; "
      "objTick owns door and projectile-retick branches")
PY
