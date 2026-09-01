#!/usr/bin/env python3
"""Prove the isolated objective photograph geometry bodies remain canonical."""

from __future__ import annotations

import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from extract_dam_guard_chr_scheduler_slice import function_text


def tokens(text: str) -> list[str]:
    text = re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)
    return re.findall(
        r"0[xX][0-9A-Fa-f]+|\d+\.\d+(?:[eE][+-]?\d+)?[fF]?|"
        r"[A-Za-z_]\w*|==|!=|<=|>=|&&|\|\||->|<<|>>|\S",
        text,
    )


def main() -> int:
    repo = Path(__file__).resolve().parents[2]
    emitted = (repo / "port/src/ge_original_stage_objective_photo_exact.c").read_text()
    sources = {
        "modelGetAxisExtents": (repo / "src/game/chrprop.c").read_text(),
        "modelGetXYExtents": (repo / "src/game/chrprop.c").read_text(),
        "projectRectCornersTo2D": (repo / "src/game/chrprop.c").read_text(),
        "objGetOnscreenRenderBounds": (repo / "src/game/propobj.c").read_text(),
        "transform3Dto2DCoords": (repo / "src/game/bondview.c").read_text(),
    }
    for name, source in sources.items():
        assert tokens(function_text(emitted, name)) == tokens(
            function_text(source, name)
        ), name
    print("objective photograph exactness: 5 canonical bodies token-identical")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
