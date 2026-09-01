#!/usr/bin/env python3
"""Extract the unchanged pad-spawn wrapper for focused host validation."""

from __future__ import annotations

import argparse
from pathlib import Path

from extract_dam_guard_ai_support_slice import function_text


def render(repo: Path) -> str:
    source = (repo / "src/game/chraction.c").read_text()
    chrprop_source = (repo / "src/game/chrprop.c").read_text()
    pieces = [
        "/* Generated from canonical decompiled sources; do not hand-edit. */",
        "#include <ultra64.h>",
        "#include <bondconstants.h>",
        "#include <bondtypes.h>",
        '#include "game/chraction.h"',
        '#include "ge_original_stage_setup.h"',
        "",
        "extern stagesetup g_CurrentSetup;",
        "extern PropRecord *g_ActivePropsHead;",
        "extern PropRecord *g_ActivePropsTail;",
        "extern PropRecord *g_FreeProps;",
        "extern PropRecord *chrSpawnAtCoord(s32 bodynum, s32 headnum,",
        "    coord3d *pos, StandTile *stan, f32 angle,",
        "    AIListRecord *ailist, s32 spawnflags);",
        "",
        function_text(source, "chrResolvePadId"),
        "",
        function_text(source, "chrSpawnAtPad"),
        "",
        function_text(chrprop_source, "chrpropActivateThisFrame"),
        "",
        function_text(chrprop_source, "chrpropFree"),
        "",
        function_text(chrprop_source, "chrpropDelist"),
        "",
    ]
    return "\n".join(pieces)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.write_text(render(args.repo))
    print(f"wrote exact dynamic guard pad-spawn slice to {args.output}")


if __name__ == "__main__":
    main()
