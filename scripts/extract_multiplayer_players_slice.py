#!/usr/bin/env python3
"""Retain canonical initial/per-life player initialization with native storage."""
from pathlib import Path
import argparse
from extract_bond_move_state_slice import extract_function, extract_variable, digest

def generate(repo):
    player = (repo / 'src/game/player.c').read_text()
    bond = (repo / 'src/game/bondview.c').read_text()
    hand = (repo / 'src/game/unk_092E50.c').read_text()
    defaults = player[player.index('/* Define default'):player.index('//newfile per EU')]
    sections = ['/* Generated: original initializers, native allocation boundary. */',
        '#include <ultra64.h>', '#include <bondconstants.h>', '#include <bondtypes.h>',
        'typedef struct OSViMode OSViMode;', '#include <fr.h>',
        '#include "game/player.h"', '#include "game/bondview.h"',
        '#include "ge_original_multiplayer_players.h"',
        '#include "ge_original_bond_input_provider.h"',
        'extern s32 random_byte;',
        'extern s32 g_VisibleToGuardsFlag;', 'extern s32 obj_collision_flag;',
        'struct player *ge_original_multiplayer_player_storage(s32 slot);',
        '#define mempAllocBytesInBank(size, bank) ge_original_multiplayer_player_storage(player_num)',
        '#define get_player_control_style(slot) ge_original_multiplayer_control_style(slot)',
        '#define cur_player_set_control_type(type) ge_original_bond_input_set_control_style(type)',
        defaults]
    for src, name in [(hand, 'D_8003FDA0'), (player, 'random_byte'), (bond, 'obj_collision_flag')]:
        body = extract_variable(src, name)
        sections += [f'/* {name} sha256={digest(body)} */', body]
    for src, name in [(player, 'initBONDdataforPlayer'), (bond, 'init_player_BONDdata')]:
        body = extract_function(src, name)
        sections += [f'/* {name} sha256={digest(body)} */', body]
    return '\n\n'.join(sections) + '\n'

if __name__ == '__main__':
    p = argparse.ArgumentParser();p.add_argument('repo',type=Path);p.add_argument('output',type=Path)
    a=p.parse_args();a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(generate(a.repo))
