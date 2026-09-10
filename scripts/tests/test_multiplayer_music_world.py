#!/usr/bin/env python3
"""Execute native music countdown at the actual shared world boundary."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
from extract_stage_pickup_slice import function_text
from extract_bond_move_non_tank_slice import generate
from extract_multiplayer_session_slice import render

music = (ROOT / 'src/game/mp_music.c').read_text()
generated = generate(ROOT)
for name in ('sub_GAME_7F0C1288', 'sub_GAME_7F0C12CC', 'reset_all_music_slots'):
    assert function_text(generated, name) == function_text(music, name)
assert function_text(render(ROOT), 'get_mission_state') == function_text(music, 'get_mission_state')
world = function_text((ROOT / 'port/src/ge_original_multiplayer_runtime.c').read_text(),
                      'ge_original_multiplayer_runtime_begin_world')
pre = function_text((ROOT / 'port/src/ge_original_stage_active_props.c').read_text(),
                    'ge_original_stage_active_props_pre_tick_exact')
assert world.count('reset_all_music_slots();') == 1
assert world.index('ge_original_stage_active_props_pre_tick_exact(') < world.index('reset_all_music_slots();')
assert world.index('ge_original_multiplayer_regen_tick_all(') < world.index('reset_all_music_slots();') < world.index('shuffle_player_ids();')
lv = (ROOT / 'src/game/lv.c').read_text()
assert 'chrpropTick();\n        reset_all_music_slots();\n        langTick();' in lv
music = music.replace(function_text(music, 'sub_GAME_7F0C0BF0'),
                      function_text(generated, 'sub_GAME_7F0C0BF0'))
with tempfile.TemporaryDirectory(prefix='ge-mp-music-world-') as tmp:
    build = Path(tmp)
    (build / 'music.c').write_text(music)
    (build / 'world.c').write_text('''#include "ge_original_multiplayer_runtime.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/mp_music.h"
#include "game/lv.h"
extern int ge_original_effect_runtime_pre_world(void);
extern void ge_original_dam_guard_all_chr_tick_exact(void);
extern void shuffle_player_ids(void);
extern GeOriginalStageActivePropStatus fixture_validate_live_binding(GeOriginalStageActiveProps *,int);
#define validate_live_binding fixture_validate_live_binding
''' + 's32 g_CurrentStageToLoad = LEVELID_CAVERNS;\n' +
        function_text(lv, 'lvlGetCurrentStageToLoad') + '\n' + pre + '\n' + world + '\n')
    for abi in ('native', 'short'):
        binary = build / abi
        command = ['cc', '-std=gnu11', '-O1', '-g', '-fno-omit-frame-pointer',
                   '-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                   '-fshort-enums' if abi == 'short' else '-fno-short-enums',
                   '-D_LANGUAGE_C', '-DGE_PORT_USE_ORIGINAL_TYPES', '-DGE_PORT_SETUP_DATA',
                   '-DVERSION_US', '-DAIPARSE', '-DGE_PORT_MS_INHERITS', '-DPLAYERFLAG=int',
                   '-fms-extensions', '-Wno-everything', '-I'+str(ROOT/'port/include'),
                   '-I'+str(ROOT/'src/game'), '-idirafter', str(ROOT/'src'),
                   '-idirafter', str(ROOT/'include'), '-idirafter', str(ROOT/'include/PR'),
                   str(build/'music.c'), str(build/'world.c'),
                   str(ROOT/'src/game/music_0D2720.c'), str(ROOT/'port/src/random_port.c'),
                   str(ROOT/'port/tests/test_ge_original_multiplayer_music_world.c'),
                   '-lm', '-o', str(binary)]
        subprocess.run(command, check=True)
        subprocess.run([str(binary)], check=True,
                       env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0', 'UBSAN_OPTIONS': 'halt_on_error=1'})
        # The execution oracle must reject omitted countdown and both wrong
        # placements; checking that source contains a call is insufficient.
        missing = world.replace('    reset_all_music_slots();\n', '')
        mutations = {
            'missing': missing,
            'before_ai': missing.replace(
                '    if (ge_original_stage_active_props_pre_tick_exact(',
                '    reset_all_music_slots();\n    if (ge_original_stage_active_props_pre_tick_exact('),
            'before_regen': missing.replace(
                '    regen = ge_original_multiplayer_regen_tick_all(',
                '    reset_all_music_slots();\n    regen = ge_original_multiplayer_regen_tick_all('),
        }
        for label, changed in mutations.items():
            assert changed != world
            mutant = build / (label + '.c')
            mutant.write_text((build/'world.c').read_text().replace(world, changed))
            mutant_binary = build / (abi + '-' + label)
            mutant_command = [str(mutant) if arg == str(build/'world.c') else
                              str(mutant_binary) if arg == str(binary) else arg for arg in command]
            subprocess.run(mutant_command, check=True)
            rejected = subprocess.run([str(mutant_binary)], capture_output=True,
                env={**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0', 'UBSAN_OPTIONS': 'halt_on_error=1'})
            assert rejected.returncode != 0 and b'assert' in rejected.stderr.lower(), label
        print(abi + ': missing, before-AI and before-regeneration countdown mutants rejected', flush=True)
