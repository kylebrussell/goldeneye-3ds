#!/usr/bin/env python3
"""Emit the exact relocation frontier of canonical bondviewProcessInput."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path


RELOCATION = re.compile(
    r"^([0-9a-fA-F]+)\s+(R_MIPS_(?:26|HI16|LO16))\s+(\S+)\s*$"
)

NORMAL_DAM_PROVIDER_CALLS = {
    "cur_player_get_aim_control",
    "cur_player_get_control_type",
    "disablePlayerActionsWhenPausedOrInMpMenu",
    "getPlayerCount",
    "get_cur_player_look_vertical_inverted",
    "get_cur_playernum",
    "lvlGetControlsLockedFlag",
    "viGetFovY",
}
NORMAL_DAM_PROVIDER_DATA = {
    "g_BondCanEnterTank",
    "g_ClockTimer",
    "g_CurrentPlayer",
    "g_GlobalTimerDelta",
    "g_PlayerIsInTank",
    "g_PlayerTankProp",
    "g_bondviewForceDisarm",
    "g_gameOverFlag",
    "g_stopPlayFlag",
}


def run(objdump: str, *args: str) -> str:
    return subprocess.run(
        [objdump, *args],
        check=True,
        text=True,
        stdout=subprocess.PIPE,
    ).stdout


def parse_symbols(text: str, wanted: str) -> tuple[int, int, set[str]]:
    defined: set[str] = set()
    function: tuple[int, int] | None = None

    for line in text.splitlines():
        fields = line.split()
        if len(fields) < 6 or not re.fullmatch(r"[0-9a-fA-F]+", fields[0]):
            continue
        if "*UND*" not in fields:
            defined.add(fields[-1])
        if fields[-1] == wanted and "F" in fields[1:-2]:
            function = (int(fields[0], 16), int(fields[-2], 16))

    if function is None:
        raise SystemExit(f"function symbol not found: {wanted}")
    return function[0], function[1], defined


def add(counts: dict[str, int], symbol: str) -> None:
    counts[symbol] = counts.get(symbol, 0) + 1


def records(counts: dict[str, int]) -> list[dict[str, object]]:
    return [
        {"symbol": symbol, "relocations": counts[symbol]}
        for symbol in sorted(counts)
    ]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--object", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--objdump", default="mips-linux-gnu-objdump")
    parser.add_argument("--symbol", default="bondviewProcessInput")
    args = parser.parse_args()

    symbol_table = run(args.objdump, "-t", str(args.object))
    start, size, defined = parse_symbols(symbol_table, args.symbol)
    end = start + size

    internal_calls: dict[str, int] = {}
    external_calls: dict[str, int] = {}
    internal_data: dict[str, int] = {}
    external_data: dict[str, int] = {}

    for line in run(args.objdump, "-r", str(args.object)).splitlines():
        match = RELOCATION.match(line)
        if not match:
            continue
        offset = int(match.group(1), 16)
        relocation = match.group(2)
        symbol = match.group(3)
        if offset < start or offset >= end or symbol.startswith("."):
            continue

        if relocation == "R_MIPS_26":
            add(internal_calls if symbol in defined else external_calls, symbol)
        else:
            add(internal_data if symbol in defined else external_data, symbol)

    blob = args.object.read_bytes()
    remaining_external_calls = {
        symbol: count
        for symbol, count in external_calls.items()
        if symbol not in NORMAL_DAM_PROVIDER_CALLS
    }
    remaining_external_data = {
        symbol: count
        for symbol, count in external_data.items()
        if symbol not in NORMAL_DAM_PROVIDER_DATA
    }
    result = {
        "format": "goldeneye-bondview-input-relocations-v1",
        "object": str(args.object),
        "object_sha256": hashlib.sha256(blob).hexdigest(),
        "function": {
            "name": args.symbol,
            "text_offset": start,
            "size": size,
            "end": end,
        },
        "frontier": {
            "unique_external_calls": len(external_calls),
            "unique_external_data_symbols": len(external_data),
            "external_calls": records(external_calls),
            "external_data_symbols": records(external_data),
            "unique_internal_calls": len(internal_calls),
            "unique_internal_data_symbols": len(internal_data),
            "internal_calls": records(internal_calls),
            "internal_data_symbols": records(internal_data),
        },
        "normal_dam_provider_tranche": {
            "resolved_calls": sorted(NORMAL_DAM_PROVIDER_CALLS),
            "resolved_data_symbols": sorted(NORMAL_DAM_PROVIDER_DATA),
            "unique_remaining_symbols": (
                len(remaining_external_calls)
                + len(remaining_external_data)
                + len(internal_calls)
                + len(internal_data)
            ),
            "remaining_external_calls": records(remaining_external_calls),
            "remaining_external_data_symbols": records(remaining_external_data),
            "remaining_internal_calls": records(internal_calls),
            "remaining_internal_data_symbols": records(internal_data),
        },
    }

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
