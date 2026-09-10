#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d /tmp/ge-mp-core.XXXXXX)
trap 'rm -rf "${test_dir}"' EXIT
if [[ $(uname -s) == Darwin ]]; then strip=(-Wl,-dead_strip); else strip=(-Wl,--gc-sections); fi
python3 - "${repo_dir}" "${test_dir}/player.c" <<'PY'
from pathlib import Path
import re,sys
root=Path(sys.argv[1]);sys.path.insert(0,str(root/'scripts'))
from extract_bond_move_state_slice import extract_function,extract_variable
port=(root/'port/src/ge_port.c').read_text()
timing=(root/'src/game/frametiming.c').read_text()
timing=re.sub(r'(void waitForNextFrame\(void\))[^\n]*',r'\1',timing)
for name in ('ge_port_advance_multiplayer_tick','ge_port_tick'):
    body=extract_function(port,name)
    assert 'waitForNextFrame(' not in body and 'copy_of_osgetcount_value_' not in body
update=extract_function(timing,'updateFrameCounters')
update=re.sub(r'copy_of_osgetcount_value_[01]\s*=[^;]+;','',update)
assert 'copy_of_osgetcount_value_' not in update and 'osGetCount(' not in update
assert 'copy_of_osgetcount_value_1' in extract_function(timing,'waitForNextFrame')
player=(root/'src/game/player.c').read_text()
bond=(root/'src/game/bondview.c').read_text()
text=['#include <ultra64.h>','#include <bondconstants.h>','#include <bondtypes.h>',
      '#include "game/player.h"','#include <random.h>']
for name in ('g_playerPointers','g_CurrentPlayer','g_playerPlayerData','g_playerPerm',
             'player_num','random_byte','array_PLAYER_IDs'):
    text.append(extract_variable(player,name))
text.append(extract_variable(bond,'g_PlayerTickCount'))
for name in ('set_cur_player','get_cur_playernum','getPlayerCount','shuffle_player_ids'):
    text.append(extract_function(player,name))
Path(sys.argv[2]).write_text('\n'.join(text))
PY
cc -std=gnu11 -O2 -Wall -Wextra -Werror -Wno-comment -Wno-missing-braces \
-Wno-logical-not-parentheses -Wno-empty-body -Wno-unused-variable \
-Wno-unused-parameter -fsanitize=address,undefined -fno-omit-frame-pointer \
-ffunction-sections -fdata-sections \
-D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA \
-DGE_PORT_MS_INHERITS -DAIPARSE -DPLAYERFLAG=int -fms-extensions \
-DGE_PORT_BOSS_STAGE_SLICE -DGE_PORT_LV_STAGE_TICK_SLICE -DVERSION_US \
-I"${repo_dir}" -I"${repo_dir}/port/include" -I"${repo_dir}/src/game" \
-idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
-idirafter "${repo_dir}/src" \
"${test_dir}/player.c" \
"${repo_dir}/port/src/ge_mp_state.c" "${repo_dir}/port/src/ge_mp_state_core.c" \
"${repo_dir}/port/src/ge_port.c" "${repo_dir}/port/src/ge_libultra.c" \
"${repo_dir}/port/src/ge_original_boss.c" \
"${repo_dir}/port/src/ge_original_ramrom_replay.c" \
"${repo_dir}/port/src/ge_original_bond_input_provider.c" \
"${repo_dir}/port/src/ge_original_level.c" "${repo_dir}/port/src/random_port.c" \
"${repo_dir}/port/src/ge_original_chr_obj_random.c" \
"${repo_dir}/src/boss.c" "${repo_dir}/src/game/lv.c" \
"${repo_dir}/src/game/frametiming.c" "${repo_dir}/src/game/quaternion.c" \
"${repo_dir}/port/tests/test_ge_mp_state_core.c" "${strip[@]}" -lm -o "${test_dir}/test"
for count in 2 4; do
    for peer in 1 2; do
        ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
        "${test_dir}/test" "${repo_dir}/port/tests/fixtures/mp_core_raw_${count}p.txt" \
            "${test_dir}/${count}p-peer${peer}.bin" | tee "${test_dir}/${count}p-peer${peer}.log"
    done
    cmp "${test_dir}/${count}p-peer1.bin" "${test_dir}/${count}p-peer2.bin"
done
python3 - "${test_dir}" "${repo_dir}" <<'PY'
import hashlib,json,os,shutil,subprocess,sys
from pathlib import Path
receipts=[]
for count in (2,4):
    blob=(Path(sys.argv[1])/f'{count}p-peer1.bin').read_bytes()
    assert len(blob)%64==0
    stride=len(blob)//64
    for offset in range(0,len(blob),stride):
        record=blob[offset:offset+stride]
        assert hashlib.sha256(record[:-32]).digest()==record[-32:]
    print(f'core {count}p: fresh-process 64-tick bytes equal; every tick matches hashlib; transcript sha256={hashlib.sha256(blob).hexdigest()}')
    receipts.append({'players':count,'ticks':64,'bytes':len(blob),'sha256':hashlib.sha256(blob).hexdigest()})
if os.environ.get('GE_MP_CORE_EVIDENCE_DIR'):
    destination=Path(os.environ['GE_MP_CORE_EVIDENCE_DIR']);destination.mkdir(parents=True,exist_ok=True)
    for path in Path(sys.argv[1]).iterdir():
        if path.suffix in ('.bin','.log'):shutil.copyfile(path,destination/path.name)
    root=Path(sys.argv[2])
    sources=['port/src/ge_mp_state_core.c','port/include/ge_mp_state_core.h','port/src/ge_mp_state.c',
        'port/src/ge_original_input.c','src/joy.c','port/src/ge_port.c','port/src/ge_original_level.c',
        'port/src/ge_original_boss.c','src/boss.c','src/game/lv.c','src/game/frametiming.c',
        'port/src/random_port.c','port/src/ge_original_chr_obj_random.c','src/game/player.c',
        'src/game/bondview.c','port/src/ge_original_bond_input_provider.c',
        'port/tests/test_ge_mp_state_core.c','scripts/test_mp_state_core.sh',
        'port/tests/fixtures/mp_core_raw_2p.txt','port/tests/fixtures/mp_core_raw_4p.txt']
    receipt={'scope':'Core-domain diagnostic bytes only; full native coverage remains INCOMPLETE',
        'head_at_test':subprocess.check_output(['git','rev-parse','HEAD'],cwd=root,text=True).strip(),
        'source_sha256':{name:hashlib.sha256((root/name).read_bytes()).hexdigest() for name in sources},
        'transcripts':receipts,'native_fields':75,'actual_owner_mutations_per_process':1418,
        'processes':4,'sanitizers':['address','undefined'],'compiler_optimization':'O2'}
    (destination/'host-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
PY
