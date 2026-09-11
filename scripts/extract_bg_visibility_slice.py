#!/usr/bin/env python3
"""Extract the original US portal-visibility function closure from bg.c."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


FUNCTIONS = (
    ("getROOMID_isRendered", 0),
    ("bgGet2dBboxByRoomId", 0),
    ("sub_GAME_7F0B39BC", 0),
    ("bgResetPortalVisitCounts", 0),
    ("bgUpdateCurrentPlayerScreenMinMax", 0),
    ("bgGetRoomCenter", 0),
    ("sub_GAME_7F0B5168", 0),
    ("bgIsRoomOnScreen", 0),
    ("bgProjectRoomCoordToScreen", 0),
    ("sub_GAME_7F0B5528", 0),
    ("sub_GAME_7F0B5864", 0),
    ("bgRectIntersect", 0),
    ("bgRectOutersect", 0),
    ("bgResetPortalQueue", 0),
    ("bgIncrementRoomPortalVisitCount", 0),
    ("bgQueuePortalTraversal", 1),
    ("bgProcessNextQueuedPortal", 1),
    ("sub_GAME_7F0B7F84", 1),
    ("bgStackPush", 0),
    ("bgStackPop", 0),
    ("bgStackGetNthValueFromEnd", 0),
    ("parse_global_vis_command_list", 0),
    ("sub_GAME_7F0B8A24", 0),
    ("bgDetermineVisibleRooms", 0),
    ("sub_GAME_7F0B96CC", 0),
    ("sub_GAME_7F0B9990", 0),
    ("bgSwapConnectedRooms", 0),
    ("bgOrderPortal", 0),
    ("bgGetPortalBetweenRooms", 0),
    ("bgToggleDataPortalsContrlBytes1Bit1", 0),
    ("sub_GAME_7F0B9E04", 0),
    ("sub_GAME_7F0B9F14", 0),
)


def matching_brace(source: str, opening: int) -> int:
    depth = 0
    state = "code"
    index = opening
    while index < len(source):
        char = source[index]
        next_char = source[index + 1] if index + 1 < len(source) else ""
        if state == "code":
            if char == "/" and next_char == "*":
                state = "block"
                index += 2
                continue
            if char == "/" and next_char == "/":
                state = "line"
                index += 2
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
                    return index
        elif state == "block" and char == "*" and next_char == "/":
            state = "code"
            index += 2
            continue
        elif state == "line" and char == "\n":
            state = "code"
        elif state in ("string", "char"):
            if char == "\\":
                index += 2
                continue
            if (state == "string" and char == '"') or (
                state == "char" and char == "'"
            ):
                state = "code"
        index += 1
    raise ValueError("unterminated function body")


def extract_function(source: str, name: str, occurrence: int) -> str:
    pattern = re.compile(
        rf"(?m)^[A-Za-z_][^;\n]*\b{re.escape(name)}\s*\([^;]*?\)"
        rf"\s*(?://[^\n]*)?\s*\{{"
    )
    matches = list(pattern.finditer(source))
    if occurrence >= len(matches):
        raise ValueError(
            f"{name}: requested occurrence {occurrence}, found {len(matches)}"
        )
    match = matches[occurrence]
    opening = source.find("{", match.start(), match.end())
    closing = matching_brace(source, opening)
    return source[match.start() : closing + 1].rstrip()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    source = args.source.read_text(encoding="utf-8")
    bodies = [extract_function(source, name, occurrence)
              for name, occurrence in FUNCTIONS]
    state_declaration = "    static Unk80081600 dword_CODE_bss_80081600;"
    if sum(body.count(state_declaration) for body in bodies) != 1:
        raise ValueError("review original global-visibility interpreter state declaration")
    bodies = [body.replace(
        "#define osSyncPrintf(x)",
        "#define osSyncPrintf(...)"
    ).replace(
        "i = (s32) &D_800442FC[portalnum];",
        "#ifdef GE_PORT_HOST_ABI\n"
        "    i = portalnum;\n"
        "#else\n"
        "    i = (s32) &D_800442FC[portalnum];\n"
        "#endif",
    ).replace(
        "*((u8 *) i) = depth;",
        "#ifdef GE_PORT_HOST_ABI\n"
        "    D_800442FC[portalnum] = (u8)depth;\n"
        "#else\n"
        "    *((u8 *) i) = depth;\n"
        "#endif",
    ).replace(
        state_declaration,
        "    /* State retains its original lifetime in the named interpreter owner. */",
    ) for body in bodies]
    # bgLevelRender reaches this persistent native lake-depth adjustment before
    # sub_GAME_7F0B3C8C computes either pass order. Retain its entire pre-return
    # block; only the following GPU/scissor/traversal call belongs elsewhere.
    prefix = extract_function(source, "sub_GAME_7F0B8D78", 0)
    tail = "    return bgScissorCurrentPlayerViewDefault(sub_GAME_7F0B3C8C(arg0));"
    if prefix.count(tail) != 1:
        raise ValueError("Dam render-prefix source boundary changed")
    prefix = prefix[:prefix.index(tail)] + "}"
    prefix = prefix.replace("Gfx *sub_GAME_7F0B8D78(Gfx *arg0)",
                            "void ge_original_bg_visibility_render_prefix_exact(void)", 1)
    bodies.append(prefix)
    # Keep bgRoomVisibilityRelated's camera-to-floor portal walk verbatim.
    # The omitted prefix owns unload/budget/player services outside this slice.
    camera = extract_function(source, "bgRoomVisibilityRelated", 0)
    begin = "    for (depth = 0, maxdepth = 11; depth != maxdepth; depth++) {"
    end = "    g_BgCurrentRoom = room;\n    bgDetermineVisibleRooms();"
    if camera.count(begin) != 1 or camera.count(end) != 1:
        raise ValueError("camera-room portal walk source boundary changed")
    metric = extract_function(source, "sub_GAME_7F0B993C", 0)
    bodies.append(metric.replace("s32 sub_GAME_7F0B993C", "static s32 sub_GAME_7F0B993C", 1))
    bodies.append(
        "s32 ge_bg_visibility_resolve_camera_room(s32 room, coord3d *pos, coord3d *pos3)\n"
        "{\n    s32 portalnum, depth, maxdepth;\n    s32 lastportal = -1;\n"
        "    u8 D_800443C4[PORTMAX] = {0};\n"
        "    /* Original bgLoadStage metric classification, independent of door controls. */\n"
        "    for (portalnum = 0; g_BgPortals[portalnum].offset_portal != NULL; portalnum++)\n"
        "        D_800443C4[portalnum] = sub_GAME_7F0B993C(portalnum);\n"
        + camera[camera.index(begin):camera.index(end)]
        + "    return room;\n}"
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        "/* Generated from src/game/bg.c; do not edit. */\n"
        '#include "ge_original_bg_visibility_internal.h"\n'
        '#include "ge_mp_state_visibility_interpreter.inc"\n\n'
        + "\n\n".join(bodies)
        + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
