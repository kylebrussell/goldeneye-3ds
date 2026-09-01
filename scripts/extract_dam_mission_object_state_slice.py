#!/usr/bin/env python3
"""Extract exact objective/tag/object-health bodies needed by Dam ai_20."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


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


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    chraction = (args.repo / "src/game/chraction.c").read_text()
    objective = (args.repo / "src/game/objective_status.c").read_text()
    objective_links = (args.repo / "src/game/objective.c").read_text()
    propobj = (args.repo / "src/game/propobj.c").read_text()
    bodies = [
        extract_function(chraction, "chrSetStageFlags"),
        extract_function(chraction, "chrUnsetStageFlags"),
        extract_function(chraction, "chrHasStageFlag"),
        extract_function(objective, "sub_GAME_7F057080"),
        extract_function(objective, "objFindByTagId"),
        extract_function(objective_links, "set_parent_cur_tag_entry"),
        extract_function(propobj, "objGetDestroyedLevel"),
        extract_function(propobj, "objIsHealthy"),
        extract_function(propobj, "check_if_entry_is_collectable"),
        extract_function(propobj, "weaponFindThrown"),
    ]
    # These are source-faithful ABI adaptations only: the decomp declares the
    # tag-list head as u32* and relies on IDO's inherited-struct extension.
    bodies = [
        body.replace(
            "TagObjectRecord *tag = ptr_last_tag_entry_type16;",
            "TagObjectRecord *tag = (TagObjectRecord *)ptr_last_tag_entry_type16;",
        ).replace(
            "arg0->NextTag = ptr_last_tag_entry_type16;",
            "arg0->NextTag = (TagObjectRecord *)ptr_last_tag_entry_type16;",
        ).replace(
            "ptr_last_tag_entry_type16 = arg0;",
            "ptr_last_tag_entry_type16 = (u32 *)arg0;",
        ).replace(
            "    if (!(obj->state & PROPSTATE_DESTROYED))",
            "    u8 ge_port_state;\n"
            "    if (!ge_dam_setup_world_definition_header(\n"
            "            obj, NULL, &ge_port_state, NULL))\n"
            "        ge_port_state = ((PropDefHeaderRecord *)obj)->state;\n"
            "    if (!(ge_port_state & PROPSTATE_DESTROYED))",
        ).replace(
            "obj->runtime_bitflags & RUNTIMEBITFLAG_HASPROJECTILE",
            "((ObjectRecord *)obj)->runtime_bitflags & 0x00000080U",
        ).replace(
            "key = prop->obj;",
            "key = (KeyRecord *)prop->obj;",
        )
        for body in bodies
    ]
    output = """/* Generated exact Dam mission object-state slice. */
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/chraction.h"
#include "game/chrai.h"
#include "game/objective_status.h"
#include "game/propobj.h"
#include "ge_original_dam_world.h"

s32 objectiveregisters1 = 0;
u32 *ptr_last_tag_entry_type16 = NULL;

""" + "\n\n".join(bodies) + "\n"
    args.output.write_text(output)
    print(f"generated {len(bodies)} exact Dam mission state bodies -> {args.output}")


if __name__ == "__main__":
    main()
