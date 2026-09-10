#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir="${repo_dir}/build/host-tests/multiplayer-session"
mkdir -p "${build_dir}"
python3 "${repo_dir}/scripts/extract_multiplayer_session_slice.py" "${repo_dir}" "${build_dir}/session.c"
python3 "${repo_dir}/scripts/extract_multiplayer_rules_slice.py" "${repo_dir}" "${build_dir}/rules.c"
python3 "${repo_dir}/scripts/extract_multiplayer_rules_fixture.py" "${repo_dir}" "${build_dir}/support.c"
python3 - "${repo_dir}" "${build_dir}/state.c" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1]);sys.path.insert(0,str(root/'scripts'))
from extract_stage_pickup_slice import function_text
text=['#include <ultra64.h>','#include <bondtypes.h>','#include "game/player.h"',
      'extern s32 g_stopPlayFlag, g_ControlsLockedFlag;']
for file,names in (('lv.c',('lvlSetControlsLockedFlag','lvlGetControlsLockedFlag')),
                   ('mpmenu.c',('disablePlayerActionsWhenPausedOrInMpMenu',))):
    source=(root/'src/game'/file).read_text()
    text.extend(function_text(source,name) for name in names)
Path(sys.argv[2]).write_text('\n'.join(text))
PY
python3 "${repo_dir}/scripts/extract_multiplayer_watch_slice.py" "${repo_dir}" "${build_dir}/watch.c"
python3 "${repo_dir}/scripts/extract_multiplayer_watch_fixture.py" "${repo_dir}" "${build_dir}/watch-support.c"
if [[ $(uname -s) == Darwin ]]; then strip=(-Wl,-dead_strip); else strip=(-Wl,--gc-sections); fi
language=("${repo_dir}/port/src/ge_original_language_text.c")
for bank in LarchE LarkE LaztE LcaveE LarecE LcradE LcrypE LdamE LdepoE LdestE LjunE LlenE LpeteE LrunE LsevE LsevbE LsevxE LsevxbE LsiloE LstatE LtraE LgunE LmiscE LoptionsE LpropobjE LtitleE LmpweaponsE LmpmenuE; do
    language+=("${repo_dir}/assets/obseg/text/${bank}.c")
done
cc -std=gnu11 -O1 -g -fno-omit-frame-pointer -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -D_LANGUAGE_C -DGE_PORT_MS_INHERITS -fms-extensions \
    -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE -DGE_PORT_SETUP_DATA -DVERSION_US -DPLAYERFLAG=int \
    -DGE_PORT_LV_TIMER_SLICE \
    -I"${repo_dir}/port/include" -I"${repo_dir}/src" -I"${repo_dir}/src/game" -I"${repo_dir}" \
    -I"${repo_dir}/include/PR" -idirafter "${repo_dir}/include" -Wno-everything \
    "${build_dir}/rules.c" "${build_dir}/support.c" "${build_dir}/session.c" "${build_dir}/state.c" \
    "${repo_dir}/port/src/ge_original_multiplayer_rules.c" \
    "${repo_dir}/port/src/ge_original_multiplayer_session.c" \
    "${repo_dir}/port/src/ge_original_level.c" "${repo_dir}/src/game/lv.c" \
    "${repo_dir}/src/game/mp_weapon.c" "${repo_dir}/port/tests/test_ge_original_multiplayer_session.c" \
    "${build_dir}/watch.c" "${build_dir}/watch-support.c" \
    "${repo_dir}/port/src/ge_original_multiplayer_watch.c" \
    "${repo_dir}/port/src/ge_original_multiplayer_players.c" \
    "${repo_dir}/src/game/music_0D2720.c" "${repo_dir}/port/tests/multiplayer_watch_audio_resources.c" \
    "${language[@]}" "${strip[@]}" -lm -o "${build_dir}/test_session"
ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "${build_dir}/test_session"
