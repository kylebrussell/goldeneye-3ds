#!/usr/bin/env python3
"""Extract the exact watch-page dispatcher and audit the full watch frontier."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


def function_text(source: str, name: str) -> str:
    match = re.search(
        rf"(?m)^[A-Za-z_][^\n;{{}}]*\b{name}\s*\([^;\n]*\)[^;{{}}]*\{{",
        source,
    )
    if match is None:
        raise ValueError(f"missing {name}")
    brace = source.index("{", match.start())
    depth = 0
    for position in range(brace, len(source)):
        if source[position] == "{":
            depth += 1
        elif source[position] == "}":
            depth -= 1
            if depth == 0:
                return source[match.start():position + 1]
    raise ValueError(f"unterminated {name}")


def calls(body: str) -> list[str]:
    ignored = {"if", "switch", "while", "for", "sizeof", "return"}
    return sorted({name for name in re.findall(r"\b([A-Za-z_]\w*)\s*\(", body)
                   if name not in ignored})


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--bondview-output", type=Path)
    args = parser.parse_args()

    options = (args.repo / "src/game/options.c").read_text()
    bondview = (args.repo / "src/game/bondview2.c").read_text()
    decoder_header = (args.repo / "port/include/ge_gbi_decoder.h").read_text()
    dispatch = function_text(options, "draw_watch_current_page").replace(
        "draw_watch_current_page", "ge_original_draw_watch_current_page_exact", 1
    )
    watch = function_text(bondview, "bondviewRenderWatch")
    page_names = [
        "draw_background_health_and_armor",
        "draw_watch_mission_status_page",
        "draw_watch_inventory_page",
        "draw_watch_control_options_page",
        "draw_watch_game_options_page",
        "draw_watch_mission_briefing_page",
    ]
    page_calls = {name: calls(function_text(options, name)) for name in page_names}

    preamble = r'''/* Generated from the unchanged US watch dispatcher. */
#include <stdint.h>

typedef int32_t s32;
typedef uint64_t Gfx;
typedef struct Mtx { uint8_t bytes[64]; } Mtx;
struct WatchVertex { uint8_t bytes[16]; };
struct GeOriginalWatchDispatchPlayer {
    struct WatchVertex buffer_for_watch_greenbackdrop_vertices[5];
};
struct GeOriginalWatchDispatchPlayer ge_original_watch_dispatch_storage;
struct GeOriginalWatchDispatchPlayer *g_CurrentPlayer =
    &ge_original_watch_dispatch_storage;

enum {
    FALSE = 0, TRUE = 1, PLAYER_1 = 0,
    WATCH_INDEX_MISSION_STATUS = 0,
    WATCH_INDEX_INVENTORY = 1,
    WATCH_INDEX_CONTROL_OPTIONS = 2,
    WATCH_INDEX_GAME_OPTIONS = 3,
    WATCH_INDEX_MISSION_BRIEFING = 4,
};
#define A_BUTTON UINT32_C(0x8000)
#define Z_TRIG UINT32_C(0x2000)

extern s32 watch_screen_index;
extern void set_page_rectangle_colors(s32, struct WatchVertex *);
extern void set_BONDdata_outside_watch_menu_flag(s32);
extern void sub_GAME_7F0BD8FC(s32);
extern uint32_t joyGetButtonsPressedThisFrame(s32, uint32_t);
extern void watch_play_beep_sound(void);
extern Gfx *draw_watch_mission_status_page(Gfx *, Mtx *);
extern Gfx *draw_watch_inventory_page(Gfx *, Mtx *);
extern Gfx *draw_watch_control_options_page(Gfx *, Mtx *);
extern Gfx *draw_watch_game_options_page(Gfx *, Mtx *);
extern Gfx *draw_watch_mission_briefing_page(Gfx *, Mtx *);
extern Gfx *draw_background_health_and_armor_transitioning(Gfx *, Mtx *);
'''
    args.output.write_text(preamble + "\n" + dispatch + "\n")
    if args.bondview_output is not None:
        retained_watch = watch.replace(
            "bondviewRenderWatch", "ge_original_bondview_render_watch_exact", 1
        )
        args.bondview_output.write_text(
            "/* Exact decomp body retained for dependency closure; not a stub. */\n"
            + retained_watch + "\n"
        )

    if args.report is not None:
        command_kinds = set(re.findall(r"GE_GBI_COMMAND_[A-Z0-9_]+", decoder_header))
        missing_commands = [name for name in (
            "GE_GBI_COMMAND_FILL_RECTANGLE",
            "GE_GBI_COMMAND_TEXTURE_RECTANGLE",
            "GE_GBI_COMMAND_RDP_HALF_1",
            "GE_GBI_COMMAND_RDP_HALF_2",
        ) if name not in command_kinds]
        args.report.write_text(json.dumps({
            "bondviewRenderWatch_calls": calls(watch),
            "draw_watch_current_page_calls": calls(dispatch),
            "page_calls": page_calls,
            "missing_gbi_commands": missing_commands,
        }, indent=2, sort_keys=True) + "\n")
    print(f"generated exact watch dispatcher -> {args.output}")


if __name__ == "__main__":
    main()
