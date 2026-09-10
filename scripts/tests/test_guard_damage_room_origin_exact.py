#!/usr/bin/env python3
"""Audit the damage consumer's sole borrowed native BG room-origin seam."""
from pathlib import Path
import subprocess
import importlib.util
import tempfile
ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('canonical_hit',ROOT/'scripts/extract_guard_bullet_hit_slice.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
with tempfile.TemporaryDirectory(prefix='ge-damage-origin-') as temporary:
    for arm in (False,True):
        output=Path(temporary)/('arm.c' if arm else 'full.c')
        command=['python3',str(ROOT/'scripts/extract_guard_damage_support_slice.py'),str(ROOT),str(output)]
        if arm:command.append('--arm-missing-only')
        subprocess.run(command,check=True,capture_output=True)
        text=output.read_text()
        expected='''#if defined(GE_PORT_NATIVE_BG_GEOMETRY)
    /* The original signature is mutable; every retained consumer only reads
     * this borrowed authored coordinate. Never populate a second room table. */
    return (struct coord3d *)ge_original_bg_geometry_room_origin(roomID);
#else
    return &ptr_bgdata_room_fileposition_list[roomID].pos;
#endif'''
        assert text.count(expected)==1
        original=(ROOT/'src/game/bgroomtrans.c').read_text()
        source_body=module.find_function(original,'getRoomPositionByIndex')
        generated_body=module.find_function(text,'getRoomPositionByIndex')
        legacy=generated_body.replace(expected,'return &ptr_bgdata_room_fileposition_list[roomID].pos;')
        assert ''.join(legacy.split())==''.join(source_body.split())
        # Neither a fabricated array nor a numeric coordinate fallback belongs
        # at this consumer. Actual bound/closed/range cases run with BG owner.
        assert text.count('ge_original_bg_geometry_room_origin(')==1
print('damage room origin: original fallback and single borrowed native owner seam verified')
