#!/usr/bin/env python3
"""Extract exact services referenced by the canonical guard-damage closure."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
from pathlib import Path


FUNCTIONS = {
    "src/game/bondinv.c": ("bondinvHasGoldenGun",),
    "src/game/bondview2.c": (
        "trigger_watch_zoom",
        "bondviewGetWatchZoomFovy",
        "bondviewTriggerWatchZoom",
        "bondviewTriggerWatchZoomDefault",
        "bondviewKillCurrentPlayer",
    ),
    "src/boss.c": ("bossGetStageNum",),
    "src/game/cheat.c": ("cheatIsActive",),
    "src/game/options.c": (
        "cur_player_get_control_type",
        "reset_watch_item_is_actively_selected",
        "sub_GAME_7F0A69A8",
    ),
    "src/game/propobj.c": (
        "sub_GAME_7F0402B4", "objGetDestroyedLevel", "objGetWidth",
        "objDrop", "drop_inventory",
    ),
    "src/game/bgroomtrans.c": ("getRoomPositionByIndex",),
    "src/game/bg.c": ("get_room_data_float1",),
    "src/game/gunfire.c": (
        "increment_num_times_killed_MwtGC",
        "increment_num_deaths",
        "increment_num_suicides_display_MP",
    ),
    "src/joy.c": ("joyGetControllerCount", "joyRumblePakStart"),
    "src/game/language.c": ("langGet",),
    "src/game/lv.c": ("lvlGetCurrentStageToLoad",),
    "src/memp.c": ("nulled_mempLoopAllMemBanks", "mempAllocBytesInBank"),
}

DATA = {
    "src/game/lv.c": ("g_GlobalTimer", "g_CurrentStageToLoad"),
    "src/game/bondview.c": ("g_ExplodeTankOnDeathFlag",),
    "src/game/mpmenu.c": ("g_stopPlayFlag", "g_gameOverFlag"),
    "src/boss.c": ("g_StageNum",),
    "src/game/cheat.c": ("g_CheatPlayerTextRelated",),
    "src/game/bg.c": (
        "ptr_bgdata_room_fileposition_list", "g_BgPortals",
    ),
    "src/game/front.c": ("g_isBondKIA",),
    "src/game/gun.c": ("g_GunDeathCountFormat", "aSD_0"),
    "src/game/options.c": (
        "watch_item_is_actively_selected", "watch_screen_index",
        "mission_brief_index", "D_800409C8",
        "D_800409CC", "D_800409D8",
    ),
    "src/joy.c": (
        "g_ContData", "g_ContDataPtr", "g_ConnectedControllers",
        "g_ContPlaybackFunc",
        "g_ContRumblePakInitState",
        "g_ContRumblePakTimer60",
        "g_ContRumblePakCurrentState",
        "g_ContRumblePakTargetState",
    ),
    "src/game/language.c": ("g_LangBanks",),
    "src/memp.c": (
        "g_mempPools",
        "needmemallocation",
        "D_80024408",
        "D_8002440C",
        "D_80024410",
    ),
}


def load_helper(path: Path, name: str):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--arm-missing-only", action="store_true",
                        help="emit only providers not already owned by the 3DS link")
    args = parser.parse_args()
    hit = load_helper(args.repo / "scripts/extract_guard_bullet_hit_slice.py",
                      "guard_hit_extract")
    explosion = load_helper(
        args.repo / "scripts/extract_bond_move_explosion_slice.py",
        "guard_explosion_extract")
    functions = FUNCTIONS
    data = DATA
    if args.arm_missing_only:
        functions = {
            "src/game/bondinv.c": ("bondinvHasGoldenGun",),
            "src/game/bondview2.c": ("bondviewKillCurrentPlayer",),
            "src/game/cheat.c": ("cheatIsActive",),
            "src/game/options.c": ("cur_player_get_control_type",),
            "src/game/propobj.c": ("objGetWidth", "objDrop", "drop_inventory"),
            "src/game/bgroomtrans.c": ("getRoomPositionByIndex",),
            "src/game/gunfire.c": (
                "increment_num_times_killed_MwtGC",
                "increment_num_deaths",
                "increment_num_suicides_display_MP",
            ),
            "src/game/lv.c": ("lvlGetCurrentStageToLoad",),
        }
        data = {
            "src/game/mpmenu.c": ("g_stopPlayFlag", "g_gameOverFlag"),
            "src/game/cheat.c": ("g_CheatPlayerTextRelated",),
            "src/game/gun.c": ("g_GunDeathCountFormat", "aSD_0"),
            "src/game/front.c": ("g_isBondKIA",),
            "src/game/bondview.c": ("g_ExplodeTankOnDeathFlag",),
            "src/game/bg.c": ("ptr_bgdata_room_fileposition_list",),
            "src/memp.c": ("g_mempPools",),
        }
    paths = set(functions) | set(data)
    sources = {path: (args.repo / path).read_text() for path in paths}
    pieces = [
        "/* Generated mechanically from canonical guard-damage services. */",
        "#include <math.h>",
        "#include <stdio.h>",
        "#include <ultra64.h>",
        "#include <bondgame.h>",
        "#include <bondconstants.h>",
        "#include <bondtypes.h>",
        "typedef int PLAYERFLAG;",
        '#include "boss.h"',
        '#include "joy.h"',
        '#include "memp.h"',
        '#include "game/bg.h"',
        '#include "game/bgroomtrans.h"',
        '#include "game/bondinv.h"',
        '#include "game/bondview.h"',
        '#include "game/cheat.h"',
        '#include "game/front.h"',
        '#include "game/gun.h"',
        '#include "game/language.h"',
        '#include "game/loadobjectmodel.h"',
        '#include "game/lv.h"',
        '#include "game/model.h"',
        '#include "game/options.h"',
        '#include "game/player.h"',
        '#include "game/propobj.h"',
        '#include "game/stan.h"',
        '#include "assets/obseg/text/LgunE.h"',
        '#include "random.h"',
        "struct contdata {",
        "    struct contsample samples[CONTSAMPLE_LEN];",
        "    s32 curlast; s32 curstart; s32 nextlast; s32 nextsecondlast;",
        "    u16 buttonspressed[MAXCONTROLLERS]; s32 playbackcontcount;",
        "};",
        "#define CONTDATA_REGULAR 0",
        "#define CONTDATA_LEN 2",
        "#define RUNTIMEBITFLAG_EMBEDDED (1u << 6)",
        "#define RUNTIMEBITFLAG_HASPROJECTILE (1u << 7)",
        "#define objDetach ge_original_obj_detach_exact",
        "#define sub_GAME_7F057C14 ge_original_random_throw_transform_exact",
        "typedef enum { RUMBLEPAKINITSTATE_ERROR = -1,",
        "    RUMBLEPAKINITSTATE_NOT_READY, RUMBLEPAKINITSTATE_READY",
        "} RUMBLEPAKINITSTATE;",
        "typedef enum { RUMBLEPAKSTATE_OFF, RUMBLEPAKSTATE_ON,",
        "    RUMBLEPAKSTATE_UNKNOWN } RUMBLEPAKSTATE;",
        "extern bool bondinvHasInvItem(ITEM_IDS);",
        "extern f32 room_data_float1;",
        "extern void trigger_solo_watch_menu(s32);",
        "extern enum PROP getPropForHeldItem(ITEM_IDS);",
        "extern void bondinvDetermineEquippedItem(void);",
        "extern void embedmentFree(Embedment *);",
        "extern f32 getsubroty(Model *);",
        "extern void ge_original_obj_detach_exact(PropRecord *);",
        "extern void ge_original_random_throw_transform_exact(coord3d *, Mtxf *);",
        "extern f32 objGetWidth(ObjectRecord *);",
        "extern void sub_GAME_7F0402B4(PropRecord *, rgba_u8 *);",
        "extern s32 g_CurrentStageToLoad;",
        "",
    ]
    for path, names in data.items():
        for name in names:
            body = explosion.extract_data(sources[path], name)
            pieces.append(f"/* {name} sha256={hashlib.sha256(body.encode()).hexdigest()} */")
            pieces.append(body)
            pieces.append("")
    for path, names in functions.items():
        for name in names:
            body = hit.find_function(sources[path], name)
            pieces.append(f"/* {name} sha256={hashlib.sha256(body.encode()).hexdigest()} */")
            pieces.append(body)
            pieces.append("")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(pieces))
    print(f"generated {sum(map(len, functions.values()))} exact guard-damage support bodies -> {args.output}")


if __name__ == "__main__":
    main()
