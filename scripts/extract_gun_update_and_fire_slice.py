#!/usr/bin/env python3
"""Extract the unchanged canonical gunUpdateAndFire both-hands tranche."""

from __future__ import annotations

import argparse
import hashlib
import re
from pathlib import Path


FUNCTIONS = (
    "gunUpdateAndFire",
    "gunUpdateAndFireBothHands",
    "CapBeamLengthAndDecideIfRendered",
    "gunCreateBeamForHand",
)


def extract_function(source: str, name: str) -> str:
    match = re.search(
        rf"(?m)^[A-Za-z_][^\n;{{}}]*\b{re.escape(name)}\s*\([^;\n]*\)[^;{{}}]*\{{",
        source,
    )
    if match is None:
        raise ValueError(f"missing {name}")
    brace = source.index("{", match.start())
    depth = 0
    state = "code"
    escaped = False
    pos = brace
    while pos < len(source):
        char = source[pos]
        nxt = source[pos + 1] if pos + 1 < len(source) else ""
        if state == "line":
            if char == "\n":
                state = "code"
        elif state == "block":
            if char == "*" and nxt == "/":
                state = "code"
                pos += 1
        elif state in ("string", "char"):
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif (state == "string" and char == '"') or (
                state == "char" and char == "'"
            ):
                state = "code"
        elif char == "/" and nxt == "/":
            state = "line"
            pos += 1
        elif char == "/" and nxt == "*":
            state = "block"
            pos += 1
        elif char == '"':
            state = "string"
        elif char == "'":
            state = "char"
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[match.start() : pos + 1]
        pos += 1
    raise ValueError(f"unterminated {name}")


def generate(repo: Path) -> str:
    source = (repo / "src/game/gunfire.c").read_text()
    bodies = [extract_function(source, name) for name in FUNCTIONS]
    digest = hashlib.sha256("\n\n".join(bodies).encode()).hexdigest()
    gun_body = bodies[0]
    gun_body = gun_body.replace(
        "        model = (Model *) (&hand->field_B68);",
        "#if defined(GE_PORT_GUN_HOST_MODEL_ABI)\n"
        "        model = ge_original_gun_host_model(handnum);\n"
        "#else\n"
        "        model = (Model *) (&hand->field_B68);\n"
        "#endif",
    ).replace(
        "        modelInit(model, mdlhdr, (s32 *) (&hand->modeldatas));",
        "#if defined(GE_PORT_GUN_HOST_MODEL_ABI)\n"
        "        modelInit(model, mdlhdr, "
        "ge_original_gun_host_model_rwdata(handnum));\n"
        "#else\n"
        "        modelInit(model, mdlhdr, (s32 *) (&hand->modeldatas));\n"
        "#endif",
    ).replace(
        "        hand->mtxlist = rwmtx;",
        "        hand->mtxlist = rwmtx;\n"
        "#if defined(GE_PORT_GUN_HOST_MODEL_ABI)\n"
        "        ge_original_gun_host_model_set_render_pos(handnum, rwmtx);\n"
        "#endif",
    )
    if gun_body == bodies[0]:
        raise ValueError("missing canonical embedded gun model ABI sites")
    bodies[0] = gun_body
    preamble = [
        "/* Generated mechanically from src/game/gunfire.c; do not edit. */",
        f'/* Canonical body SHA-256: {digest} */',
        "#include <math.h>",
        '#include "include/math.h"',
        "#include <ultra64.h>",
        "#include <PR/gu.h>",
        '#include "include/limits.h"',
        '#include "gbi_extension.h"',
        "#include <bondconstants.h>",
        "#include <bondtypes.h>",
        "typedef int PLAYERFLAG;",
        '#include "game/bondview.h"',
        '#include "game/dyn.h"',
        '#include "game/gun.h"',
        '#include "game/matrixmath.h"',
        '#include "game/model.h"',
        '#include "game/options.h"',
        '#include "random.h"',
        '#include "ge_original_bond_input_internal.h"',
        "#define GUN_SPRING_DAMP 0.95f",
        "#define GUN_SPRING_SCALE 0.050000012f",
        "extern coord3d D_80035C40;",
        "extern coord3d D_80035C4C;",
        "extern coord3d D_80035C58;",
        "extern coord3d D_80035C64;",
        "extern coord3d D_80035C70;",
        "extern coord3d D_80035C7C;",
        "extern coord3d D_80035C88;",
        "extern ModelSkeleton skeleton_gun_revolver;",
        "extern ModelSkeleton skeleton_gun_kf7;",
        "extern void coord3dCatmullRomInterp(coord3d *, coord3d *, coord3d *, coord3d *, f32, coord3d *);",
        "extern f32 gunSetHorizontalOffset(GUNHAND);",
        "extern ModelFileHeader *get_ptr_weapon_model_header_line(ITEM_IDS);",
        "extern void guRotateF(f32 [4][4], f32, f32, f32, f32);",
        "extern void sub_GAME_7F05C614(void);",
        "extern f32 get_value_if_watch_is_on_hand_or_not(GUNHAND);",
        "extern void sub_GAME_7F05E6B4(GUNHAND, s32);",
        "extern void sub_GAME_7F05E83C(GUNHAND);",
        "extern void sub_GAME_7F05E978(Model *, s32);",
        "extern void sub_GAME_7F05EA94(Model *, s32);",
        "extern void bondviewSelectCuff(Model *, ModelFileHeader *, s32);",
        "extern void gunCreateBeamForHand(GUNHAND);",
        "extern void sub_GAME_7F068508(GUNHAND, f32);",
        "extern void gunSpawnGLGrenade(GUNHAND);",
        "extern void generate_player_thrown_grenade(GUNHAND);",
        "extern void gunFireTankShell(s32);",
        "extern void generate_player_thrown_knife(GUNHAND);",
        "extern void generate_player_thrown_object(s32);",
        "extern void gunUpdateAttachedRocket(s32);",
        "extern s32 get_itemtype_in_hand(GUNHAND);",
        "extern f32 bondviewGetPlayerStanHeight(struct player *);",
        "extern void ge_original_generate_player_thrown_object_exact(s32);",
        "extern Mtxf *ge_original_gun_current_player_view_to_world(void);",
        "extern Model *ge_original_gun_host_model(GUNHAND);",
        "extern s32 *ge_original_gun_host_model_rwdata(GUNHAND);",
        "extern void ge_original_gun_host_model_set_render_pos(GUNHAND, void *);",
        "#define currentPlayerGetViewToWorldMtxf ge_original_gun_current_player_view_to_world",
        "#define generate_player_thrown_object ge_original_generate_player_thrown_object_exact",
        "#define gunUpdateAndFire ge_original_gun_update_and_fire_exact",
        "#define gunUpdateAndFireBothHands ge_original_gun_update_and_fire_both_hands_exact",
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
    print(f"generated {len(FUNCTIONS)} exact gun fire bodies -> {args.output}")


if __name__ == "__main__":
    main()
