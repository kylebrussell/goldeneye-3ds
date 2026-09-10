#!/usr/bin/env python3
"""Native music producer order and source ownership at the MP stage boundary."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
from extract_stage_pickup_slice import function_text
from extract_multiplayer_regen_slice import block
from extract_multiplayer_session_slice import render

lv = (ROOT / 'src/game/lv.c').read_text()
music = (ROOT / 'src/game/mp_music.c').read_text()
session = (ROOT / 'port/src/ge_original_multiplayer_session.c').read_text()
generated = render(ROOT)
recovery, _ = block(lv, 'if (get_mission_state() == MISSION_STATE_6)')
helper = function_text(generated, 'ge_original_mp_session_music_exact')
assert helper[helper.index('{')+1:helper.rindex('}')].strip() == recovery.strip()
for name in ('get_mission_state', 'sub_GAME_7F0C1364'):
    assert function_text(generated, name) == function_text(music, name)
native_load = function_text(lv, 'lvlStageLoad')
assert native_load.index('sub_GAME_7F0C1364();') < native_load.index('init_guards(')
tick = function_text(session, 'ge_original_multiplayer_session_tick')
assert tick.index('status=validate(1);') < tick.index('ge_original_mp_session_music_exact();')
assert tick.index('ge_original_mp_session_music_exact();') < tick.index('ge_original_mp_session_decide_exact(')
assert 'reset_all_music_slots(' not in tick
main_source = (ROOT / 'platform/3ds/source/main.c').read_text()
# Main contains alternate preprocessor brace branches; select its unique
# definition region instead of counting braces from mutually exclusive code.
assert main_source.count('\nint main(void)\n{') == 1
main = main_source.split('\nint main(void)\n{', 1)[1]
start = main.index('if (!ge_port_start_stage(')
reset = main.index('ge_original_multiplayer_music_stage_reset();', start)
initialize = main.index('sub_GAME_7F0C11FC(selected_level_id);', reset)
guards = main.index('initialize_stage_guard_objects(', initialize)
lives = main.index('runtime_multiplayer_initialize_lives(', guards)
assert start < reset < initialize < guards < lives
print('multiplayer music: exact source reset/recovery and scoped native main initialization order passed')
