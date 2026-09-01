#!/usr/bin/env python3
"""Extract the exact collision/room helpers referenced by MoveBond."""

from __future__ import annotations

import argparse
import hashlib
import re
from pathlib import Path


FUNCTIONS = {
    "src/game/chr.c": (
        "chrUpdateCollisionBounds",
        "chrGetChrWidthHeight",
        "chrSetMoving",
    ),
    "src/game/bondview2.c": (
        "bondviewGetPropHeightRelatedValues",
        "bondviewGetTankCollisionBounds",
        "bondviewTestLineUnobstructed",
        "bondviewTankCollisionStatus",
        "bondviewCallTankCollisionStatus",
        "bondviewUpdateGuardTankFlagsRelated",
    ),
    "src/game/chrprop.c": (
        "chraiGetPropRoomIds",
        "chraiGetCollisionBounds",
        "chraiGetCollisionBoundsWithoutY",
        "chrpropTestPointInPolygon",
        "chrpropGetCollisionBounds",
        "roomGetProps",
        "chrpropBBOXGetYmin",
        "sub_GAME_7F03D058",
    ),
    "src/game/propobj.c": (
        "chrobjApplySpeed",
        "sub_GAME_7F04F244",
        "chrobjSeparatingAxisTheorem",
        "chrobjTestPolygonsTouchingOrOverlap2D",
        "chrobjTestPointPolygonCollision",
        "sub_GAME_7F04F218",
        "sub_GAME_7F0537B8",
        "sub_GAME_7F053894",
        "sub_GAME_7F0539B8",
        "chrobjSndCreatePostEvent",
        "chrobjSndCreatePostEventDefault",
    ),
    "src/game/loadobjectmodel.c": ("setupUpdateObjectRoomPosition",),
    "src/game/player.c": ("getPlayerCount",),
}

DATA = (("src/game/player.c", "g_playerPointers"),)

BODY_ORDER = (
    ("src/game/chr.c", "chrUpdateCollisionBounds"),
    ("src/game/chr.c", "chrGetChrWidthHeight"),
    ("src/game/chr.c", "chrSetMoving"),
    ("src/game/bondview2.c", "bondviewGetPropHeightRelatedValues"),
    ("src/game/bondview2.c", "bondviewGetTankCollisionBounds"),
    ("src/game/bondview2.c", "bondviewTestLineUnobstructed"),
    ("src/game/bondview2.c", "bondviewTankCollisionStatus"),
    ("src/game/bondview2.c", "bondviewCallTankCollisionStatus"),
    ("src/game/bondview2.c", "bondviewUpdateGuardTankFlagsRelated"),
    ("src/game/propobj.c", "sub_GAME_7F04F244"),
    ("src/game/chrprop.c", "chraiGetPropRoomIds"),
    ("src/game/chrprop.c", "chrpropTestPointInPolygon"),
    ("src/game/chrprop.c", "chrpropGetCollisionBounds"),
    ("src/game/chrprop.c", "roomGetProps"),
    ("src/game/chrprop.c", "chrpropBBOXGetYmin"),
    ("src/game/chrprop.c", "sub_GAME_7F03D058"),
    ("src/game/chrprop.c", "chraiGetCollisionBounds"),
    ("src/game/chrprop.c", "chraiGetCollisionBoundsWithoutY"),
    ("src/game/propobj.c", "chrobjApplySpeed"),
    ("src/game/propobj.c", "chrobjSeparatingAxisTheorem"),
    ("src/game/propobj.c", "chrobjTestPolygonsTouchingOrOverlap2D"),
    ("src/game/propobj.c", "chrobjTestPointPolygonCollision"),
    ("src/game/propobj.c", "sub_GAME_7F04F218"),
    ("src/game/loadobjectmodel.c", "setupUpdateObjectRoomPosition"),
    ("src/game/player.c", "getPlayerCount"),
    ("src/game/propobj.c", "sub_GAME_7F0537B8"),
    ("src/game/propobj.c", "sub_GAME_7F053894"),
    ("src/game/propobj.c", "sub_GAME_7F0539B8"),
    ("src/game/propobj.c", "chrobjSndCreatePostEvent"),
    ("src/game/propobj.c", "chrobjSndCreatePostEventDefault"),
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
    for pos in range(brace, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[match.start():pos + 1]
    raise ValueError(f"unterminated {name}")


def adapt_native_abi(body: str) -> str:
    """Name the native replacement for ObjectRecord's IDO-inherited state."""
    return (body
        .replace("obj->state", "ge_port_door_collision_object_state(obj)")
        .replace("&obj->ptr_allocated_collisiondata_block->polygon",
                 "(rect4f *)&obj->ptr_allocated_collisiondata_block->polygon"))


def extract_data(source: str, name: str) -> str:
    match = re.search(
        rf"(?m)^struct player \*{re.escape(name)}\[4\];$", source
    )
    if match is None:
        raise ValueError(f"missing data {name}")
    return match.group(0)


def digest(body: str) -> str:
    return hashlib.sha256(body.encode("utf-8")).hexdigest()


def generate(repo: Path) -> str:
    sources = {relative: (repo / relative).read_text() for relative in FUNCTIONS}
    sections = [
        "/* Generated mechanically from canonical GoldenEye collision bodies. */",
        "#include <limits.h>",
        "#include <stdint.h>",
        "#include <ultra64.h>",
        "#include <bondconstants.h>",
        "#include <bondtypes.h>",
        "typedef int PLAYERFLAG;",
        '#include "game/bondview.h"',
        '#include "game/chrai.h"',
        '#include "game/chr.h"',
        '#include "game/bg.h"',
        '#include "game/loadobjectmodel.h"',
        '#include "game/matrixmath.h"',
        '#include "game/player.h"',
        '#include "game/propobj.h"',
        '#include "game/stan.h"',
        '#include "snd.h"',
        '#include "ge_original_bond_input_internal.h"',
        '#include "ge_original_door_collision_internal.h"',
        "#undef g_playerPointers",
        "#undef getPlayerCount",
        "extern u32 num_obj_position_data_entries;",
        "extern coord3d g_TankModelPositionOffset;",
        "extern f32 g_TankTurretOrientationAngleRad;",
        "",
    ]
    for relative, name in DATA:
        canonical = extract_data(sources[relative], name)
        sections.append(f"/* {name} sha256={digest(canonical)} */")
        sections.append(canonical)
        sections.append("")
    previous = None
    for relative, name in BODY_ORDER:
        if relative != previous:
            sections.append(f"/* Exact bodies from {relative}. */")
            previous = relative
        canonical = extract_function(sources[relative], name)
        sections.append(f"/* {name} sha256={digest(canonical)} */")
        sections.append(adapt_native_abi(canonical))
        sections.append("")
    return "\n".join(sections)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    output = generate(args.repo)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(output)
    print(f"generated {len(BODY_ORDER)} exact MoveBond collision bodies -> {args.output}")


if __name__ == "__main__":
    main()
