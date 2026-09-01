#!/usr/bin/env python3
"""Extract the exact projectile background/STAN services for host tests."""

from __future__ import annotations

import argparse
from pathlib import Path

from extract_dam_guard_chr_scheduler_slice import function_text
from extract_dam_guard_chr_scheduler_support_slice import definition_text


TARGETS = (
    ("chrprop.c", "chraiUpdateOnscreenPropCount"),
    ("bg.c", "addToByteSetMaxSize15"),
    ("bg.c", "bgFindRoomsAlongSegment"),
    ("bg.c", "bgCopyVisibleRoomsToList"),
    ("bg.c", "bgTestBulletHitBackground"),
    ("bg.c", "get_room_data_float2"),
    ("stan.c", "stanFindTileBelowPos"),
)


def render(repo: Path) -> str:
    game = repo / "src/game"
    sources = {name: (game / name).read_text() for name, _ in TARGETS}
    return "\n\n".join((
        "/* Generated unchanged background/STAN services for focused tests. */",
        "#include <ultra64.h>",
        "#include <PR/gbi.h>",
        "#include <bondconstants.h>",
        "#include <bondtypes.h>",
        '#include "bg.h"',
        '#include "chrai.h"',
        '#include "stan.h"',
        "extern char list_visible_rooms_in_cur_global_vis_packet[0x98];",
        "extern s32 num_visible_rooms_in_cur_global_vis_packet;",
        "extern f32 room_data_float1;",
        "extern s32 sub_GAME_7F0B9F14(s32, coord3d *, coord3d *);",
        "extern bool bgTestRayIntersectsBbox(coord3d *, coord3d *, s32 *, s32 *);",
        "extern bool bgTestRayIntersectionInRoom(coord3d *, coord3d *, coord3d *, RoomVtxBatchBounds *, s32, struct HitThing *);",
        "extern bool check_if_imageID_is_light(s32);",
        "extern f32 getShortest2dDispToInfTripleEdge(StandTile *, s32, f32, f32);",
        "extern bool stanTileHasZeroArea(StandTile *);",
        "extern void getTileMidPoint(StandTile *, coord3d *);",
        "extern f32 level_scale;",
        "extern u8 list_of_tilesizes[];",
        "extern PropRecord *chrpropGetActiveTail(void);",
        definition_text(sources["bg.c"], r"^struct HitThingSub\s*\{"),
        *(function_text(sources[filename], name)
          for filename, name in TARGETS),
        "",
    ))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.write_text(render(args.repo))
    print(f"generated {len(TARGETS)} unchanged background/STAN test bodies")


if __name__ == "__main__":
    main()
