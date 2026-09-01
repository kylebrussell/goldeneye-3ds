#!/usr/bin/env python3
"""Extract the canonical mutable state used by the normal MoveBond path."""

from __future__ import annotations

import argparse
import hashlib
import re
from pathlib import Path


VARIABLES = {
    "bondview": (
        "g_bondviewForceDisarm",
        "g_PlayerIsInTank",
        "g_PlayerTankProp",
        "g_TankSfxState",
        "g_CameraMode",
        "g_bondviewBondDeathAnimations",
        "g_bondviewBondDeathAnimationsCount",
        "g_DefaultMoveBondOffset",
    ),
    "bondview2": ("g_ForceBondMoveOffset", "g_TankEngineSfxVolume"),
    "player": ("g_playerPlayerData", "g_playerPerm", "player_num"),
    "debugmenu": ("debug_fast_bond_flag",),
    "stan": ("stanlinelog_flag",),
}

FUNCTIONS = {
    "player": ("default_player_perspective_and_height", "get_cur_playernum"),
    "lv": ("lvlGetControlsLockedFlag",),
    "debugmenu": ("get_debug_man_pos_flag", "get_debug_fast_bond_flag"),
    "model": ("return_null",),
}


def digest(body: str) -> str:
    return hashlib.sha256(body.encode("utf-8")).hexdigest()


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
    index = brace
    while index < len(source):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
        index += 1
    raise SystemExit(f"unterminated function: {name}")


def extract_variable(source: str, name: str) -> str:
    match = re.search(
        rf"(?m)^[A-Za-z_][^;\n]*\b{re.escape(name)}(?:\s*\[[^\n]*?\])?\s*(?:=|;)",
        source,
    )
    if match is None:
        raise SystemExit(f"variable not found: {name}")
    start = match.start()
    end = source.find(";", match.start())
    if end < 0:
        raise SystemExit(f"unterminated variable: {name}")
    return source[start:end + 1]


def main() -> None:
    parser = argparse.ArgumentParser()
    for name in ("bondview", "bondview2", "player", "lv", "debugmenu", "model", "stan"):
        parser.add_argument(name, type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    sources = {
        name: getattr(args, name).read_text()
        for name in ("bondview", "bondview2", "player", "lv", "debugmenu", "model", "stan")
    }

    sections = [
        "/* Generated from unchanged canonical decomp state and functions. */",
        '#include "ge_original_bond_movement_internal.h"',
        '#include "game/debugmenu_handler.h"',
        '#include "game/lv.h"',
        '#include "game/model.h"',
        '#include "game/player.h"',
        "#include <assets/animationtable_data.h>",
        "",
        "extern s32 g_ControlsLockedFlag;",
        "",
    ]
    for source_name, names in VARIABLES.items():
        for name in names:
            declaration = extract_variable(sources[source_name], name)
            sections.append(f"/* {name} sha256={digest(declaration)} */")
            sections.append(declaration)
            sections.append("")
    for source_name, names in FUNCTIONS.items():
        for name in names:
            body = extract_function(sources[source_name], name)
            sections.append(f"/* {name} sha256={digest(body)} */")
            sections.append(body)
            sections.append("")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(sections))


if __name__ == "__main__":
    main()
