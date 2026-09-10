#!/usr/bin/env python3
"""Source-owned helper closure for the real multiplayer gun producer fixture."""
from pathlib import Path
import argparse
from extract_player_gait_model_slice import find_function
from extract_bond_move_explosion_slice import extract_data


def render(repo):
    gun = (repo / "src/game/gun.c").read_text()
    camera = (repo / "src/game/bondview.c").read_text()
    stats = (repo / "assets/obseg/gun/gunWeaponStats.inc.c").read_text()
    out = '''/* Generated canonical gun helpers and PP7 stats. Resource owners are external. */
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/bondview.h"
#include "game/gun.h"
#include "game/model.h"
#include "ge_original_bond_input_internal.h"
extern GunModelFileRecord gitem_structs[];
extern ModelFileHeader cartridge_header;
#undef MODELSKELETON
#define MODELSKELETON(NAME, NUMJOINTS, SKELSIZE) \\
    ModelSkeleton SKELETON(NAME) = {NUMJOINTS, 0, JOINTLIST(NAME), SKELSIZE, 0};
#include "assets/embedded/skeletons/standard_gun.inc.c"
#include "assets/obseg/gun/wppk/ModelFileHeader.inc.c"
#include "assets/obseg/gun/wppksil/ModelFileHeader.inc.c"
'''
    out += stats[:stats.index("WeaponStats")]
    for name in ("default_weaponstats", "wppk_stats", "wppksil_stats"):
        out += "\n" + extract_data(stats, name)
    out += "\n" + extract_data(gun, "sniperMeleeKeyframes2")
    for name in ("getCurrentPlayerWeaponId", "Gun_hand_without_item",
                 "get_item_in_hand_or_watch_menu", "get_ptr_item_statistics",
                 "get_ptr_weapon_model_header_line", "bondwalkItemCheckBitflags",
                 "gunSetHorizontalOffset", "sub_GAME_7F05DCB8"):
        out += "\n" + find_function(gun, name)
    for name in ("camGetWorldToScreenMtxf", "getPlayer_c_screenwidth",
                 "getPlayer_c_screenheight", "getPlayer_c_screenleft", "getPlayer_c_screentop"):
        out += "\n" + find_function(camera, name)
    out += "\n" + extract_data((repo / "src/game/unk_092E50.c").read_text(), "D_8003FDA0")
    return out + "\n"


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(render(args.repo))
