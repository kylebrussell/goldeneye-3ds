#!/usr/bin/env python3
"""Extract GoldenEye's exact ChrRecord collision-bounds leaves."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


FUNCTIONS = (
    "chrUpdateCollisionBounds",
    "chrGetChrWidthHeight",
    "chrGetChrGround",
)
RENAMES = {
    name: f"ge_original_door_{name}_exact" for name in FUNCTIONS
}


def extract_function(source: str, name: str) -> str:
    match = re.search(
        rf"(?m)^[A-Za-z_][^\n;{{}}]*\b{name}\s*\([^;\n]*\)[^;{{}}]*\{{",
        source,
    )
    if match is None:
        raise ValueError(f"missing {name}")
    brace = source.index("{", match.start())
    depth = 0
    for pos in range(brace, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[match.start():pos + 1]
    raise ValueError(f"unterminated {name}")


def generate(repo: Path) -> str:
    source = (repo / "src/game/chr.c").read_text()
    sections = [
        "/* Generated mechanically from canonical GoldenEye ChrRecord bodies. */",
        "#include <ultra64.h>",
        "#include <bondconstants.h>",
        "#include <bondtypes.h>",
        "#include \"ge_original_door_collision_internal.h\"",
        "",
    ]
    for name in FUNCTIONS:
        body = re.sub(rf"\b{name}\b", RENAMES[name],
                      extract_function(source, name))
        sections.extend((body, ""))
    return "\n".join(sections)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(generate(args.repo))
    print(f"generated {len(FUNCTIONS)} exact character collision bodies -> {args.output}")


if __name__ == "__main__":
    main()
