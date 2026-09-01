#!/usr/bin/env python3
"""Extract unchanged on-foot MoveBond dependencies from the decomp sources.

The emitted translation unit is deliberately a source slice rather than a
second implementation: every function body is copied byte-for-byte from the
canonical decomp file and its digest is recorded beside the body.
"""

from __future__ import annotations

import argparse
import hashlib
import re
from pathlib import Path


BONDVIEW2_FUNCTIONS = (
    "bondviewMoveAnimationTick",
    "bondviewUpdatePlayerY",
    "bondviewUpdatePlayerCollisionPositionFields",
    "bondviewUpdatePlayerCollisionBounds",
    "MoveBond",
)

BONDVIEW_FUNCTIONS = (
    "currentPlayerGetCrouchPos",
    "currentPlayerSetCameraMode",
)

STAN_FUNCTIONS = (
    "stanGetTileOrderedPointWorldPos",
    "stanGetMoveBondCollisionTiles",
)


def extract_function(source: str, name: str) -> str:
    match = re.search(
        rf"(?m)^[A-Za-z_][^;\n]*\b{re.escape(name)}\s*\([^;]*?\)\s*\{{",
        source,
    )
    if match is None:
        raise SystemExit(f"function not found: {name}")

    start = match.start()
    brace = source.find("{", match.start(), match.end())
    depth = 0
    in_string = False
    in_char = False
    in_line_comment = False
    in_block_comment = False
    escaped = False
    index = brace

    while index < len(source):
        char = source[index]
        next_char = source[index + 1] if index + 1 < len(source) else ""

        if in_line_comment:
            if char == "\n":
                in_line_comment = False
        elif in_block_comment:
            if char == "*" and next_char == "/":
                in_block_comment = False
                index += 1
        elif in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
        elif in_char:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == "'":
                in_char = False
        elif char == "/" and next_char == "/":
            in_line_comment = True
            index += 1
        elif char == "/" and next_char == "*":
            in_block_comment = True
            index += 1
        elif char == '"':
            in_string = True
        elif char == "'":
            in_char = True
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
        index += 1

    raise SystemExit(f"unterminated function: {name}")


def digest(body: str) -> str:
    return hashlib.sha256(body.encode("utf-8")).hexdigest()


def extract_timing_constants(source: str) -> str:
    start = source.index("#if defined(VERSION_EU)\n    #define TANKUPDATEROTATION_SCALE")
    end = source.index("\n#endif", start) + len("\n#endif")
    return source[start:end]


def extract_define(source: str, name: str) -> str:
    match = re.search(rf"(?m)^\s*#define\s+{re.escape(name)}\b[^\n]*", source)
    if match is None:
        raise SystemExit(f"define not found: {name}")
    return match.group(0).lstrip()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("bondview2", type=Path)
    parser.add_argument("bondview", type=Path)
    parser.add_argument("stan", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    bondview2 = args.bondview2.read_text()
    bondview = args.bondview.read_text()
    stan = args.stan.read_text()
    bodies = [
        (name, extract_function(bondview2, name))
        for name in BONDVIEW2_FUNCTIONS
    ] + [
        (name, extract_function(bondview, name))
        for name in BONDVIEW_FUNCTIONS
    ] + [
        (name, extract_function(stan, name))
        for name in STAN_FUNCTIONS
    ]

    sections = [
        "/* Generated from unchanged canonical decomp function bodies. */",
        extract_define(bondview2, "BONDVIEW_HUD_MSG_TOP_BUFFER_LENGTH"),
        extract_define(bondview2, "BONDVIEW_HUD_MSG_BOTTOM_BUFFER_LENGTH"),
        '#include "ge_original_bond_movement_internal.h"',
        '#include "game/bondhead.h"',
        '#include "game/bondview_internal.h"',
        '#include "game/chrai.h"',
        '#include "game/chr_b.h"',
        '#include "game/chraction.h"',
        '#include "game/explosion.h"',
        '#include "game/debugmenu_handler.h"',
        '#include "game/gun.h"',
        '#include "game/initanitable.h"',
        '#include "game/lv.h"',
        '#include "game/loadobjectmodel.h"',
        '#include "game/matrixmath.h"',
        '#include "game/model.h"',
        '#include "game/player.h"',
        '#include "game/propobj.h"',
        '#include "music.h"',
        '#include "random.h"',
        "",
        "void bondviewGetTankCollisionBounds(struct rect4f *bounds,",
        "                                    coord3d *position, float angle);",
        "s32 currentPlayerGetCrouchPos(void);",
        "void currentPlayerSetCameraMode(s32 mode);",
        "extern StandTile *dword_CODE_bss_8007BA0C;",
        "extern u8 g_StanTileSpecialFlags[];",
        "extern f32 inv_level_scale;",
        "",
        "/* Exact regional timing constants from bondview2.c. */",
        extract_timing_constants(bondview2),
        extract_define(bondview2, "TANK_MAX_SPEED"),
        "",
    ]
    for name, body in bodies:
        frontier_only = name == "MoveBond" or name in STAN_FUNCTIONS
        if frontier_only:
            sections.append("#ifdef GE_PORT_EXACT_MOVEBOND_FRONTIER")
        sections.append(f"/* {name} sha256={digest(body)} */")
        sections.append(body)
        if frontier_only:
            sections.append("#endif /* GE_PORT_EXACT_MOVEBOND_FRONTIER */")
        sections.append("")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(sections))


if __name__ == "__main__":
    main()
