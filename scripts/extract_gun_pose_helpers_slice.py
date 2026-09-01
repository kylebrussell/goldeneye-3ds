#!/usr/bin/env python3
"""Extract canonical data and helpers consumed by gunUpdateAndFire."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

from extract_bond_input_gun_data_slice import find_initialized_global
from extract_gun_update_and_fire_slice import extract_function


FUNCTIONS = (
    ("src/game/gun.c", "sub_GAME_7F05C614"),
    ("src/game/gun.c", "get_itemtype_in_hand"),
    ("src/game/gun.c", "get_value_if_watch_is_on_hand_or_not"),
    ("src/game/gun.c", "sub_GAME_7F05E6B4"),
    ("src/game/gun.c", "sub_GAME_7F05E83C"),
    ("src/game/gun.c", "sub_GAME_7F05E978"),
    ("src/game/gun.c", "sub_GAME_7F05EA94"),
    ("src/game/matrixmath_misc.c", "coord3dCatmullRomInterp"),
    ("src/game/bondview2.c", "bondviewGetPlayerStanHeight"),
    ("src/game/bondview2.c", "bondviewSelectCuff"),
    ("src/game/file.c", "fileGetBondForCurrentFolder"),
    ("src/game/file2.c", "fileGetBondForFolder"),
    ("src/game/options.c", "cur_player_get_screen_setting"),
    ("src/game/options.c", "get_screen_ratio"),
    ("src/game/options.c", "cur_player_get_autoaim"),
    ("src/game/options.c", "cur_player_get_lookahead"),
    ("src/game/options.c", "cur_player_get_ammo_onscreen_setting"),
    ("src/game/options.c", "cur_player_get_sight_onscreen_control"),
    ("src/game/model.c", "modelUpdateNodeRelations"),
    ("src/game/model.c", "process_07_unknown"),
    ("src/game/model.c", "modelCalculateScaledRootToOriginDir"),
    ("src/game/model.c", "modelGetScaledRootToOriginDir"),
    ("src/game/dyn.c", "dynAllocate"),
)


def generate(repo: Path) -> str:
    sources: dict[str, str] = {}
    bodies: list[str] = []
    for relative, name in FUNCTIONS:
        source = sources.setdefault(relative, (repo / relative).read_text())
        bodies.append(extract_function(source, name))

    gun = sources["src/game/gun.c"]
    pose_globals = [
        find_initialized_global(gun, f"coord3d {name} =")
        for name in (
            "D_80035C40", "D_80035C4C", "D_80035C58", "D_80035C64",
            "D_80035C70", "D_80035C7C", "D_80035C88",
        )
    ]
    options = sources["src/game/options.c"]
    option_table = find_initialized_global(
        options, "struct game_options game_options_entries[] =")
    op07_table = find_initialized_global(
        (repo / "src/game/objecthandler.c").read_text(),
        "struct bondstruct_unk_op07_related D_800360C4[32] =",
    )
    gun_kf7_skeleton = (
        repo / "assets/embedded/skeletons/gun_kf7.inc.c"
    ).read_text().replace(
        "MODELSKELETON(gun_kf7, 7, 0x12)",
        "ModelSkeleton skeleton_gun_kf7 = {7, 0, jointlist_gun_kf7, 0x12, 0};",
    )
    gun_revolver_skeleton = (
        repo / "assets/embedded/skeletons/gun_revolver.inc.c"
    ).read_text().replace(
        "MODELSKELETON(gun_revolver, 7, 21)",
        "ModelSkeleton skeleton_gun_revolver = {7, 0, jointlist_gun_revolver, 21, 0};",
    )
    digest = hashlib.sha256(
        "\n\n".join((
            *pose_globals, option_table, op07_table,
            gun_kf7_skeleton, gun_revolver_skeleton, *bodies,
        )).encode()
    ).hexdigest()

    cuff_index = next(
        i for i, (_, name) in enumerate(FUNCTIONS)
        if name == "bondviewSelectCuff"
    )
    cuff_body = bodies[cuff_index]
    cuff_needle = "    offset = switchindex << 2;"
    if cuff_body.count(cuff_needle) != 1:
        raise ValueError("bondviewSelectCuff N64 pointer stride drift")
    bodies[cuff_index] = cuff_body.replace(
        cuff_needle,
        "#if defined(GE_PORT_GUN_HOST_MODEL_ABI)\n"
        "    offset = switchindex * sizeof(*switches);\n"
        "#else\n"
        "    offset = switchindex << 2;\n"
        "#endif",
    )

    preamble = [
        "/* Generated mechanically from canonical game sources; do not edit. */",
        f"/* Canonical data/body SHA-256: {digest} */",
        '#include "include/math.h"',
        "#include <ultra64.h>",
        '#include "include/limits.h"',
        "#include <macro.h>",
        "#include <bondconstants.h>",
        "#include <bondtypes.h>",
        "#ifndef PLAYERFLAG",
        "typedef int PLAYERFLAG;",
        "#endif",
        '#include "game/bondview.h"',
        '#include "game/dyn.h"',
        '#include "game/file.h"',
        '#include "game/gun.h"',
        '#include "game/matrixmath.h"',
        '#include "game/model.h"',
        '#include "game/options.h"',
        '#include "game/objecthandler.h"',
        '#include "assets/obseg/text/LoptionE.h"',
        '#include "ge_original_bond_input_internal.h"',
        "extern u8 *g_GfxMemPos;",
        "s32 selected_folder_num = FOLDER1;",
        "u8 fileGetBondForFolder(u32 folder);",
        "u32 cartridges_eject = 0;",
        "u32 g_gunDebKeyframeIndex = 0;",
        "extern Weapon1PTransformKeyframe sniperMeleeKeyframes2[11];",
        "#define DEB_KEYFRAMES sniperMeleeKeyframes2",
        *pose_globals,
        option_table,
        op07_table,
        gun_kf7_skeleton,
        gun_revolver_skeleton,
    ]
    return "\n".join(preamble) + "\n\n" + "\n\n".join(bodies) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    output = generate(args.repo)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(output)
    print(f"generated {len(FUNCTIONS)} exact gun pose helpers -> {args.output}")


if __name__ == "__main__":
    main()
