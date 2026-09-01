#!/usr/bin/env python3
"""Extract the canonical US cartridge casing creation/ejection chain."""

from __future__ import annotations

import argparse
import hashlib
import re
from pathlib import Path

from extract_gun_update_and_fire_slice import extract_function


CONSTANTS = (
    "g_CasingSwitchScale",
    "g_PistolCasingHorizontalSpeed",
    "g_PistolCasingRotationScaleX",
    "g_PistolCasingRotationOffsetX",
    "g_PistolCasingRotationScaleY",
    "g_PistolCasingRotationOffsetY",
    "g_PistolCasingRotationScaleZ",
    "g_PistolCasingRotationOffsetZ",
    "g_PistolCasingRandomDivisor",
    "g_PistolCasingGravity",
    "g_RifleCasingHorizontalSpeed",
    "g_RifleCasingVerticalSpeed",
    "g_RifleCasingRotationScaleX",
    "g_RifleCasingRotationOffsetX",
    "g_RifleCasingRotationScaleY",
    "g_RifleCasingRotationOffsetY",
    "g_RifleCasingRotationScaleZ",
    "g_RifleCasingRotationOffsetZ",
    "g_RifleCasingRandomDivisor",
    "g_RifleCasingGravity",
)


def scalar_declaration(source: str, name: str) -> str:
    matches = re.findall(
        rf"(?m)^const f32 {re.escape(name)}\s*=\s*[^;]+;", source
    )
    if not matches:
        raise ValueError(f"missing canonical casing constant: {name}")
    return matches[-1]


def generate(repo: Path) -> str:
    source = (repo / "src/game/gunfire.c").read_text()
    us_start = source.index("#if defined(VERSION_US)", source.index("merged from gun2.c"))
    us_source = source[us_start:]
    bodies = [
        extract_function(us_source, "casingCreate"),
        extract_function(us_source, "sub_GAME_7F068508"),
    ]
    constants = [scalar_declaration(source, name) for name in CONSTANTS]
    digest = hashlib.sha256(
        "\n\n".join((*constants, *bodies)).encode()
    ).hexdigest()
    preamble = [
        "/* Generated mechanically from canonical US gunfire.c; do not edit. */",
        f"/* Canonical body/data SHA-256: {digest} */",
        "#include <ultra64.h>",
        "#include <bondconstants.h>",
        "#include <bondtypes.h>",
        "typedef int PLAYERFLAG;",
        '#include "game/bondview.h"',
        '#include "game/gun.h"',
        '#include "game/matrixmath.h"',
        '#include "random.h"',
        '#include "ge_original_bond_input_internal.h"',
        "CasingRecord g_Casings[20];",
        "/* Original D_80035EA4/EA8/EAC are three adjacent zero words and",
        " * the canonical body intentionally reads them as one coord3d. */",
        "u32 D_80035EA4[3] = {0, 0, 0};",
        "#define THROWMTX_OFFSET 0xAD8",
        "#define THROWPOS_OFFSET 0xB08",
        "#define THROWPOS_PREV_OFFSET 0xB48",
        "#define THROWMTX ((Mtxf *)((u8 *)g_CurrentPlayer + handoffset + THROWMTX_OFFSET))",
        "#define THROWPOS(k) (((f32 *)((u8 *)g_CurrentPlayer + handoffset + THROWPOS_OFFSET))[k])",
        "#define THROWPREV(k) (((f32 *)((u8 *)g_CurrentPlayer + handoffset + THROWPOS_PREV_OFFSET))[k])",
        "CasingRecord *casingCreate(ModelFileHeader *, Mtxf *);",
        *constants,
    ]
    return "\n".join(preamble) + "\n\n" + "\n\n".join(bodies) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    output = generate(args.repo)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(output)
    print(f"generated 2 exact US casing bodies -> {args.output}")


if __name__ == "__main__":
    main()
