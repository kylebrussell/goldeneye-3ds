#!/usr/bin/env python3
"""Extract the exact moving-door overlap path and its geometry dependencies."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


FUNCTIONS = {
    "src/game/propobj.c": (
        "chrobjSeparatingAxisTheorem",
        "chrobjTestPolygonsTouchingOrOverlap2D",
        "chrobjTestPointPolygonCollision",
        "sub_GAME_7F0448A8",
        "sub_GAME_7F04F244",
    ),
    "src/game/chrprop.c": (
        "chraiGetPropRoomIds",
        "chraiGetCollisionBounds",
        "chrpropTestPointInPolygon",
        "chrpropGetCollisionBounds",
        "sub_GAME_7F03CFE8",
        "roomGetProps",
    ),
    "src/game/bondview2.c": (
        "bondviewGetPlayerStanHeight",
        "bondviewGetPlayerDuckingHeightRelated",
        "bondviewGetPropHeightRelatedValues",
        "bondviewGetCollisionRadius",
    ),
    "src/game/player.c": ("getPlayerPointerIndex",),
    "src/game/stan.c": (
        "stanGetSignedPointLineDistance",
        "distBetweenPoints2d",
        "stanPointProjectsOntoEdge",
    ),
}

RENAMES = {
    name: f"ge_door_collision_{name}"
    for names in FUNCTIONS.values() for name in names
}
RENAMES.update({
    "chrUpdateCollisionBounds": "ge_port_door_collision_chr_update_bounds",
    "chrGetChrWidthHeight": "ge_port_door_collision_chr_width_height",
    "chrGetChrGround": "ge_port_door_collision_chr_ground",
})

BODY_ORDER = (
    ("src/game/player.c", "getPlayerPointerIndex"),
    ("src/game/bondview2.c", "bondviewGetPlayerStanHeight"),
    ("src/game/bondview2.c", "bondviewGetPlayerDuckingHeightRelated"),
    ("src/game/bondview2.c", "bondviewGetPropHeightRelatedValues"),
    ("src/game/bondview2.c", "bondviewGetCollisionRadius"),
    ("src/game/stan.c", "stanGetSignedPointLineDistance"),
    ("src/game/stan.c", "distBetweenPoints2d"),
    ("src/game/stan.c", "stanPointProjectsOntoEdge"),
    ("src/game/chrprop.c", "chraiGetPropRoomIds"),
    ("src/game/chrprop.c", "chrpropTestPointInPolygon"),
    ("src/game/chrprop.c", "chrpropGetCollisionBounds"),
    ("src/game/chrprop.c", "sub_GAME_7F03CFE8"),
    ("src/game/chrprop.c", "roomGetProps"),
    ("src/game/propobj.c", "sub_GAME_7F04F244"),
    ("src/game/chrprop.c", "chraiGetCollisionBounds"),
    ("src/game/propobj.c", "chrobjSeparatingAxisTheorem"),
    ("src/game/propobj.c", "chrobjTestPolygonsTouchingOrOverlap2D"),
    ("src/game/propobj.c", "chrobjTestPointPolygonCollision"),
    ("src/game/propobj.c", "sub_GAME_7F0448A8"),
)


def extract_function(source: str, name: str) -> str:
    match = re.search(
        rf"(?m)^[A-Za-z_][^\n;{{}}]*\b{name}\s*\([^;\n]*\)[^;{{}}]*\{{",
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


def rename_identifiers(body: str) -> str:
    for original in sorted(RENAMES, key=len, reverse=True):
        body = re.sub(rf"\b{re.escape(original)}\b", RENAMES[original], body)
    return body


def adapt_native_abi(body: str) -> str:
    """Bridge IDO inherited fields and 32-bit tagged pointers explicitly."""
    body = (body
        .replace("obj->state", "ge_port_door_collision_object_state(obj)")
        .replace("prop->obj->type",
                 "ge_port_door_collision_object_type(prop->obj)")
        .replace("&obj->ptr_allocated_collisiondata_block->polygon",
                 "(rect4f *)&obj->ptr_allocated_collisiondata_block->polygon")
        .replace("(s32) temp_v0_2->model",
                 "ge_port_door_collision_character_flags(prop)"))
    # collision_data stores up to eight coord2d values starting at its rect4f
    # member.  The N64 compiler treats that member as the start of the backing
    # block; express that native layout without indexing past rect4f.points[4].
    return re.sub(r"\b([A-Za-z_]\w*)->points",
                  r"ge_port_door_collision_polygon_points(\1)", body)


def generate(repo: Path) -> str:
    sections = [
        "/* Generated mechanically from canonical GoldenEye collision bodies. */",
        "#include <stdint.h>",
        "#include <ultra64.h>",
        "#include <bondconstants.h>",
        "#include <bondtypes.h>",
        "typedef int PLAYERFLAG;",
        "#include \"game/bondview.h\"",
        "#include \"game/chrai.h\"",
        "#include \"game/chr.h\"",
        "#include \"game/player.h\"",
        "#include \"game/propobj.h\"",
        "#include \"game/stan.h\"",
        "#include \"ge_original_bond_input_internal.h\"",
        "#include \"ge_original_door_collision_internal.h\"",
        "#define ge_port_door_collision_polygon_points(POLYGON) \\",
        "    ((struct coord2d *)(void *)(POLYGON))",
        "extern u32 num_obj_position_data_entries;",
        "",
    ]
    count = 0
    # Dependency bodies precede their callers, preserving C11 declarations.
    sources = {relative: (repo / relative).read_text() for relative in FUNCTIONS}
    previous = None
    for relative, name in BODY_ORDER:
        if relative != previous:
            sections.append(f"/* Exact bodies from {relative}. */")
            previous = relative
        sections.extend((adapt_native_abi(rename_identifiers(
            extract_function(sources[relative], name))), ""))
        count += 1
    sections.extend((
        "s32 ge_original_door_collision_exact_slice(PropRecord *prop)",
        "{",
        "    return ge_door_collision_sub_GAME_7F0448A8(prop);",
        "}",
        "",
    ))
    return "\n".join(sections)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    output = generate(args.repo)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(output)
    print(f"generated {sum(map(len, FUNCTIONS.values()))} exact door collision bodies -> {args.output}")


if __name__ == "__main__":
    main()
