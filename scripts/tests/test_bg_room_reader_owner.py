#!/usr/bin/env python3
"""Keep the gameplay visibility reader at the fixed native publication owner."""
from pathlib import Path
import subprocess
import sys
import tempfile

repo = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(repo / 'scripts'))
from extract_bg_visibility_slice import extract_function
from extract_dam_guard_ai_support_slice import FUNCTIONS
from extract_dam_guard_chr_scheduler_support_slice import FULL_PROPS_FUNCTIONS

assert ('bg', 'getROOMID_isRendered') not in FUNCTIONS
assert ('bg', 'bgGet2dBboxByRoomId') not in FULL_PROPS_FUNCTIONS
source = (repo / 'src/game/bg.c').read_text()
with tempfile.TemporaryDirectory() as directory:
    output = Path(directory) / 'visibility.c'
    subprocess.run([sys.executable, str(repo / 'scripts/extract_bg_visibility_slice.py'),
                    str(repo / 'src/game/bg.c'), str(output)], check=True)
    for name in ('getROOMID_isRendered', 'bgGet2dBboxByRoomId'):
        assert extract_function(output.read_text(), name, 0) == extract_function(source, name, 0)
        assert f'#define {name} ge_renderer_private_{name}' in \
               (repo / 'port/include/ge_renderer_bg_visibility_namespace.h').read_text()
assert '#define getROOMID_isRendered ge_renderer_private_getROOMID_isRendered' in \
       (repo / 'port/include/ge_renderer_bg_visibility_namespace.h').read_text()
assert 'extern s_bound_info ge_bg_visibility_draw_rooms' not in \
       (repo / 'scripts/extract_dam_guard_chr_scheduler_support_slice.py').read_text()
assert 'extern s_room_info ge_bg_visibility_room_info' not in \
       (repo / 'scripts/extract_dam_guard_ai_support_slice.py').read_text()
print('BG room reader: exact source body at canonical publication owner; private eye namespace separate')
