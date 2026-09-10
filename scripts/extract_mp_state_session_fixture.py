#!/usr/bin/env python3
"""Exact native session, roster, inventory counters, death/fade fixture closure."""
from pathlib import Path
import argparse
from extract_mp_state_inventory_fixture import render as inventory
from extract_multiplayer_rules_slice import render as rules
from extract_stage_pickup_slice import function_text
from extract_bond_move_explosion_slice import extract_data

def render(root):
    result = inventory(root)
    rule = rules(root)
    # These same unchanged bodies/storage are already owned by inventory's
    # fixture slice; remove duplicate definitions rather than replace calls.
    rule = rule.replace(function_text((root/'src/game/mpmenu.c').read_text(),
                                    'get_points_for_mp_player'), '')
    for definition in ('ramromfilestructure *ptr_active_demofile;',
                       's32 is_ramrom_flag;', 's32 recording_ramrom_flag;'):
        rule = rule.replace(definition, '')
    result += '\n' + rule + '\n#include "game/model.h"\n#include <macro.h>\n'
    result += '\nextern u8 *g_GfxMemPos;\nextern s32 g_ControlsLockedFlag;\n'
    for file, names in (('lv.c', ('lvlSetControlsLockedFlag', 'lvlGetControlsLockedFlag')),
                        ('model.c', ('modelGetAnimFrame', 'modelGetAnimEndFrame')),
                        ('dyn.c', ('dynAllocate',))):
        source = (root/'src/game'/file).read_text()
        result += '\n'.join(function_text(source, name) for name in names) + '\n'
    arena=(root/'assets/obseg/setup/Ump_setupcrypZ.c').read_text()
    arena=arena[arena.index('PadRecord padlist[] ='):]
    for original,renamed in (('intro','fixture_intro'),('padlist','fixture_pads')):
        result += extract_data(arena,original).replace(original,renamed) + '\n'
    result += 'const size_t fixture_intro_words=sizeof(fixture_intro)/sizeof(s32);\n'
    result += 'const size_t fixture_pad_count=sizeof(fixture_pads)/sizeof(PadRecord);\n'
    return result

if __name__ == '__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('root',type=Path);parser.add_argument('output',type=Path)
    args=parser.parse_args();args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(render(args.root))
