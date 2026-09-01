#!/usr/bin/env python3
"""Extract exact dependency bodies required by canonical secondary gun sinks."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path


FUNCTIONS = (
    ("src/game/bondview2.c", "struct PropRecord *get_ptr_for_players_tank(void)"),
    ("src/game/bondview2.c", "void bondviewSet3dCoord7F07CEB0(coord3d *arg0)"),
    ("src/game/model.c", "void modelUpdateRelationsQuick(Model *model, ModelNode *parent)"),
    ("src/game/propobj.c", "void sub_GAME_7F0402B4(PropRecord *prop, rgba_u8 *color)"),
    ("src/game/propobj.c", "void objChangeShading(ObjectRecord* obj, coord3d* pos, Mtxf* matrix, StandTile* stan)"),
    ("src/game/propobj.c", "void objUpdateThrowKnifeSound(ObjectRecord *obj)"),
    ("src/game/explosion.c", "void setSixExplosionAndSmokeEntries(void)"),
)


def extract_at_signature(source: str, signature: str, start: int = 0) -> str:
    """Brace-parse a definition located by its complete canonical signature."""
    definition = source.index(signature, start)
    brace = source.index("{", definition + len(signature))
    # A declaration or unrelated construct between the signature and brace is a
    # generator error, never something to paper over in emitted game code.
    between = source[definition + len(signature) : brace]
    if ";" in between:
        raise ValueError(f"declaration found instead of definition: {signature}")
    depth = 0
    state = "code"
    escaped = False
    conditional_depths: list[int] = []
    line_start = True
    pos = brace
    while pos < len(source):
        char = source[pos]
        nxt = source[pos + 1] if pos + 1 < len(source) else ""
        if state == "code" and line_start:
            line_end = source.find("\n", pos)
            if line_end < 0:
                line_end = len(source)
            directive = source[pos:line_end].lstrip()
            if directive.startswith(("#if ", "#if\t", "#ifdef ", "#ifndef ")):
                conditional_depths.append(depth)
            elif directive.startswith(("#else", "#elif")):
                if not conditional_depths:
                    raise ValueError(f"unmatched preprocessor branch: {signature}")
                depth = conditional_depths[-1]
            elif directive.startswith("#endif"):
                if not conditional_depths:
                    raise ValueError(f"unmatched preprocessor end: {signature}")
                conditional_depths.pop()
        if state == "line":
            if char == "\n":
                state = "code"
        elif state == "block":
            if char == "*" and nxt == "/":
                state = "code"
                pos += 1
        elif state in ("string", "char"):
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif (state == "string" and char == '"') or (
                state == "char" and char == "'"
            ):
                state = "code"
        elif char == "/" and nxt == "/":
            state = "line"
            pos += 1
        elif char == "/" and nxt == "*":
            state = "block"
            pos += 1
        elif char == '"':
            state = "string"
        elif char == "'":
            state = "char"
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[definition : pos + 1]
        line_start = char == "\n"
        pos += 1
    raise ValueError(f"unterminated definition: {signature}")


def generate(repo: Path) -> str:
    cache: dict[str, str] = {}
    bodies: list[str] = []
    for relative, signature in FUNCTIONS:
        source = cache.setdefault(relative, (repo / relative).read_text())
        bodies.append(extract_at_signature(source, signature))

    gunfire = (repo / "src/game/gunfire.c").read_text()
    us_start = gunfire.index("#if defined(VERSION_US)", gunfire.index("merged from gun2.c"))
    bodies.append(
        extract_at_signature(
            gunfire,
            "void sub_GAME_7F068190(coord3d *zeropos, coord3d *vec)",
            us_start,
        )
    )

    digest = hashlib.sha256("\n\n".join(bodies).encode()).hexdigest()
    preamble = [
        "/* Generated mechanically from canonical sources; do not edit. */",
        f"/* Canonical body SHA-256: {digest} */",
        "#include <math.h>",
        '#include "include/math.h"',
        "#include <ultra64.h>",
        "#include <bondconstants.h>",
        "#include <bondtypes.h>",
        "typedef int PLAYERFLAG;",
        '#include "game/bondview.h"',
        '#include "game/explosion.h"',
        '#include "game/matrixmath.h"',
        '#include "game/model.h"',
        '#include "game/propobj.h"',
        '#include "game/stan.h"',
        '#include "random.h"',
        '#include "snd.h"',
        '#include "ge_original_bond_input_internal.h"',
        "#ifndef RUNTIMEBITFLAG_HASPROJECTILE",
        "#define RUNTIMEBITFLAG_HASPROJECTILE RUNTIMEBITFLAG_00000080",
        "#endif",
        "#ifndef RUNTIMEBITFLAG_THROWING_KNIFE_RELATED",
        "#define RUNTIMEBITFLAG_THROWING_KNIFE_RELATED 0x00000020",
        "#endif",
        "extern ALBank *g_musicSfxBufferPtr;",
        "extern void set_color_shading_from_tile(PropRecord *, u8 [4]);",
        "extern void sub_GAME_7F0402B4(PropRecord *, rgba_u8 *);",
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
    print(f"generated {len(FUNCTIONS) + 1} exact gun dependencies -> {args.output}")


if __name__ == "__main__":
    main()
