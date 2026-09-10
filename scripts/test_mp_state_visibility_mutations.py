#!/usr/bin/env python3
"""Prove targeted exporter omissions are detected by native-owner mutations."""
from pathlib import Path
import os
import subprocess
import tempfile

repo=Path(__file__).resolve().parent.parent
source=(repo/'port/src/ge_mp_state_visibility.c').read_text()
mutations={
    'lost_room_cycle': ('v->room_cycle=D_80044858;', 'v->room_cycle=0;'),
    'lost_retained_bbox': (
        'memcpy(v->interpreter_box,ge_bg_visibility_interpreter_state.unk0.f,sizeof(bbox2d));',
        'memset(v->interpreter_box,0,sizeof(v->interpreter_box));'),
    'unsigned_draw_depth': ('v->draw_depths[i]=room->unk1;', 'v->draw_depths[i]=(uint8_t)room->unk1;'),
    'unmatched_door_controls': ('|| portal->controlbytes1 != portal_controls[i]', '|| 0'),
    'active_traversal_capture': ('|| g_BgPortalQueueReadIndex != g_BgPortalQueueWriteIndex', '|| 0'),
}
with tempfile.TemporaryDirectory(prefix='ge-e9-mutations-') as tmp:
    for name,(old,new) in mutations.items():
        assert source.count(old)==1, name
        directory=Path(tmp)/name;directory.mkdir()
        variant=directory/'exporter.c';variant.write_text(source.replace(old,new))
        env=dict(os.environ,GE_VISIBILITY_TEST_DIR=str(directory),GE_VISIBILITY_EXPORTER_SOURCE=str(variant))
        result=subprocess.run(['bash',str(repo/'scripts/test_mp_state_visibility.sh')],
            env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
        if result.returncode==0 or 'Assertion' not in result.stdout or 'failed' not in result.stdout:
            raise RuntimeError(f'{name}: expected semantic assertion failure\n{result.stdout}')
        print(f'E9 mutation rejected: {name}')
