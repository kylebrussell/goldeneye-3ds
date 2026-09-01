#!/usr/bin/env python3
"""Extract the unchanged US fog/sky tables from the decompiled bgfog.c."""

from __future__ import annotations

import argparse
from pathlib import Path
import re


def declaration(source: str, marker: str) -> str:
    start = source.index(marker)
    end = source.index("\n};", start) + 3
    return source[start:end]


def enum_values(source: str, enum_name: str, prefix: str) -> dict[str, int]:
    """Resolve simple canonical integer entries from a named enum."""
    match = re.search(
        rf"typedef\s+enum\s+{enum_name}\s*\{{(.*?)\}}\s*{enum_name}\s*;",
        source,
        flags=re.DOTALL,
    )
    if match is None:
        raise ValueError(f"canonical {enum_name} enum was not found")
    body = re.sub(r"/\*.*?\*/", "", match.group(1), flags=re.DOTALL)
    body = re.sub(r"//.*", "", body)
    values: dict[str, int] = {}
    value = -1
    for item in body.split(","):
        item = item.strip()
        if not item:
            continue
        if "=" in item:
            name, expression = (part.strip() for part in item.split("=", 1))
            if re.fullmatch(r"-?(?:0[xX][0-9a-fA-F]+|[0-9]+)", expression) is None:
                continue
            value = int(expression, 0)
        else:
            name = item
            value += 1
        if name.startswith(prefix):
            values[name] = value
    return values


def resolve_constants(table: str, values: dict[str, int]) -> str:
    def replace(match: re.Match[str]) -> str:
        name = match.group(0)
        if name not in values:
            raise ValueError(f"unresolved canonical level id {name}")
        return str(values[name])

    return re.sub(
        r"\b(?:LEVELID|ENVIRONMENTDATA)_[A-Z0-9_]+\b", replace, table)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    source = (args.repo / "src/game/bgfog.c").read_text()
    constants = (args.repo / "src/bondconstants.h").read_text()
    non_eu = source.index("#else\n/**\n * Address 0x80044E10.")
    fog = declaration(source[non_eu:], "EnvironmentRecord fog_tables[] = {")
    fogless = declaration(source, "EnvironmentFoglessRecord fog_tables2[] = {")
    fog = fog.replace(
        "EnvironmentRecord fog_tables[]",
        "const GeOriginalEnvironmentRecord ge_original_fog_tables[]",
        1,
    )
    fogless = fogless.replace(
        "EnvironmentFoglessRecord fog_tables2[]",
        "const GeOriginalEnvironmentFoglessRecord "
        "ge_original_fogless_tables[]",
        1,
    )
    ids = enum_values(constants, "LEVELID", "LEVELID_")
    ids.update(enum_values(
        constants, "ENVIRONMENTDATA_IDS", "ENVIRONMENTDATA_"))
    environment_end = re.search(
        r"^#define\s+ENVIRONMENTDATA_END\s+(-?[0-9]+)\s*$",
        constants,
        flags=re.MULTILINE,
    )
    if environment_end is None:
        raise ValueError("canonical ENVIRONMENTDATA_END was not found")
    ids["ENVIRONMENTDATA_END"] = int(environment_end.group(1), 10)
    fog = resolve_constants(fog, ids)
    fogless = resolve_constants(fogless, ids)
    output = """/* Generated verbatim from src/game/bgfog.c; do not edit. */
#include \"ge_original_stage_environment_table_types.h\"

""" + fog + "\n\n" + fogless + "\n"
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(output)


if __name__ == "__main__":
    main()
