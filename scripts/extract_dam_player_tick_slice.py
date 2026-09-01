#!/usr/bin/env python3
"""Extract GoldenEye's unchanged playerTick body for isolated Dam audit."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


def function_text(source: str, name: str) -> str:
    match = re.search(
        rf"(?m)^[A-Za-z_][^\n;={{}}]*\b{name}\s*\([^;]*\)\s*\{{",
        source,
    )
    if match is None:
        raise ValueError(f"missing function {name}")
    brace = source.index("{", match.start())
    depth = 0
    state = "code"
    pos = brace
    while pos < len(source):
        char = source[pos]
        nxt = source[pos + 1] if pos + 1 < len(source) else ""
        if state == "code":
            if char == "/" and nxt == "*":
                state = "block"
                pos += 2
                continue
            if char == "/" and nxt == "/":
                state = "line"
                pos += 2
                continue
            if char == '"':
                state = "string"
            elif char == "'":
                state = "char"
            elif char == "{":
                depth += 1
            elif char == "}":
                depth -= 1
                if depth == 0:
                    return source[match.start():pos + 1]
        elif state == "block" and char == "*" and nxt == "/":
            state = "code"
            pos += 2
            continue
        elif state == "line" and char == "\n":
            state = "code"
        elif state in ("string", "char"):
            if char == "\\":
                pos += 2
                continue
            if ((state == "string" and char == '"')
                    or (state == "char" and char == "'")):
                state = "code"
        pos += 1
    raise ValueError(f"unterminated function {name}")


def render(repo: Path) -> str:
    bondview2 = (repo / "src/game/bondview2.c").read_text()
    bondview = (repo / "src/game/bondview.c").read_text()
    player = (repo / "src/game/player.c").read_text()
    pointer_index = function_text(player, "getPlayerPointerIndex")
    crouch = function_text(bondview, "playerGetCrouchPos")
    deregister_room = function_text(bondview2, "bondviewDeregisterPlayerRoom")
    update_room = function_text(bondview2, "bondviewUpdatePlayerRoom")
    body = function_text(bondview2, "playerTick")
    return "\n".join((
        "/* Generated unchanged canonical single-player prop tick slice. */",
        '#include "ge_original_dam_player_tick.h"',
        "#include <ultra64.h>",
        '#include "include/math.h"',
        "#include <bondconstants.h>",
        "#include <bondtypes.h>",
        "#ifndef PLAYERFLAG",
        "typedef int PLAYERFLAG;",
        "#endif",
        '#include "game/bondview.h"',
        "#define BONDVIEW_HUD_MSG_TOP_BUFFER_LENGTH 0x97",
        "#define BONDVIEW_HUD_MSG_BOTTOM_BUFFER_LENGTH 0x65",
        '#include "game/bondview_internal.h"',
        '#include "game/chr.h"',
        '#include "game/chraction.h"',
        '#include "game/debugmenu_handler.h"',
        '#include "game/initanitable.h"',
        '#include "game/lv.h"',
        '#include "game/matrixmath.h"',
        '#include "game/model.h"',
        '#include "game/player.h"',
        '#include "random.h"',
        '#include "ge_original_dam_guard_chr_scheduler.h"',
        "",
        "#define getPlayerPointerIndex \\",
        "    ge_original_dam_player_pointer_index_exact",
        pointer_index,
        "#undef getPlayerPointerIndex",
        "#define playerGetCrouchPos ge_original_dam_player_crouch_pos_exact",
        crouch,
        "#undef playerGetCrouchPos",
        "#define bondviewDeregisterPlayerRoom \\",
        "    ge_original_dam_player_deregister_room_exact",
        deregister_room,
        "#undef bondviewDeregisterPlayerRoom",
        "#define chrDetectRooms ge_original_dam_guard_chr_detect_rooms_exact",
        "#define bondviewDeregisterPlayerRoom \\",
        "    ge_original_dam_player_deregister_room_exact",
        "#define bondviewUpdatePlayerRoom \\",
        "    ge_original_dam_player_update_room_exact",
        update_room,
        "#undef bondviewUpdatePlayerRoom",
        "#undef bondviewDeregisterPlayerRoom",
        "#undef chrDetectRooms",
        "",
        "#define getPlayerPointerIndex \\",
        "    ge_original_dam_player_pointer_index_exact",
        "#define playerGetCrouchPos ge_original_dam_player_crouch_pos_exact",
        "#define bondviewUpdatePlayerRoom \\",
        "    ge_original_dam_player_update_room_exact",
        "#define chrTick ge_original_dam_guard_chr_tick_exact",
        "#define playerTick ge_original_dam_player_tick_exact",
        body,
        "#undef playerTick",
        "#undef chrTick",
        "#undef bondviewUpdatePlayerRoom",
        "#undef playerGetCrouchPos",
        "#undef getPlayerPointerIndex",
        "",
    ))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(render(args.repo))
    print("generated unchanged canonical Dam playerTick")


if __name__ == "__main__":
    main()
