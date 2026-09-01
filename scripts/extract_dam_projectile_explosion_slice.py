#!/usr/bin/env python3
"""Extract the exact projectile collision and prop explosion scheduler closure."""

from __future__ import annotations

import argparse
import hashlib
import re
from pathlib import Path


FUNCTIONS = (
    ("src/game/propobj.c", "projectileLineTestModel"),
    ("src/game/propobj.c", "sub_GAME_7F041400"),
    ("src/game/propobj.c", "projectileTestObjectCollision"),
    ("src/game/propobj.c", "projectileTestObjectCollisionRecursive"),
    ("src/game/propobj.c", "sub_GAME_7F041BB8"),
    ("src/game/propobj.c", "projectileFindCollidingProp"),
    ("src/game/propobj.c", "propExplode"),
    ("src/game/model.c", "modelFindNextProjectileHitCandidate"),
)


def extract_function(source: str, name: str) -> str:
    search_source = source
    search_offset = 0
    if name == "propExplode":
        signature = re.search(
            r"#if defined\(VERSION_JP\) \|\| defined\(VERSION_EU\)\n"
            r"s32 propExplode\([^\n]+\)\n#else\n"
            r"void propExplode\([^\n]+\)\n#endif",
            source,
        )
        if signature is None:
            raise ValueError("missing conditional propExplode signature")
        us_signature = re.search(
            r"void propExplode\([^\n]+\)", signature.group(0)
        )
        if us_signature is None:
            raise ValueError("missing US propExplode signature")
        search_source = (
            source[: signature.start()]
            + us_signature.group(0)
            + source[signature.end() :]
        )

    match = re.search(
        rf"(?m)^[A-Za-z_][^\n;{{}}]*\b{re.escape(name)}\s*"
        r"\([^;{}]*\)[^;{}]*\{",
        search_source,
    )
    if match is None:
        raise ValueError(f"missing function {name}")
    brace = search_source.index("{", match.start())
    depth = 0
    state = "code"
    pos = brace
    while pos < len(search_source):
        char = search_source[pos]
        nxt = search_source[pos + 1] if pos + 1 < len(search_source) else ""
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
                    return search_source[match.start() : pos + 1]
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
    sources = {
        path: (repo / path).read_text() for path, _ in FUNCTIONS
    }
    bodies = [extract_function(sources[path], name) for path, name in FUNCTIONS]
    digest = hashlib.sha256("\n\n".join(bodies).encode()).hexdigest()
    pieces = [
        "/* Generated unchanged projectile/explosion scheduler closure. */",
        f"/* Canonical bodies SHA-256: {digest} */",
        "#include <ultra64.h>",
        "#include <PR/gbi.h>",
        "#include <bondgame.h>",
        "#include <bondconstants.h>",
        "#include <bondtypes.h>",
        '#include "bondaicommands.h"',
        "#include <math.h>",
        '#include "bg.h"',
        '#include "bondview.h"',
        '#include "chraction.h"',
        '#include "chr.h"',
        '#include "explosion.h"',
        '#include "model.h"',
        '#include "objecthandler.h"',
        '#include "player.h"',
        '#include "propobj.h"',
        '#include "stan.h"',
        '#include "stanintersection.h"',
        "",
        "#ifndef RUNTIMEBITFLAG_ISRETICK",
        "#define RUNTIMEBITFLAG_ISRETICK (1u << 3)",
        "#endif",
        "extern void guNormalize(f32 *x, f32 *y, f32 *z);",
        "extern Mtxf *currentPlayerGetViewToWorldMtxf(void);",
        "extern bool propobjFindHit(Model *model, ModelNode *start_node,",
        "        coord3d *raypos, coord3d *raydir, HitThing *hitthing,",
        "        s32 *mtxindex, ModelNode **dstnode);",
        "extern s32 propDoorGetCdTypes(PropRecord *prop);",
        "extern bool modelTestRayIntersectsNodeBBox(Model *model, ModelNode *node,",
        "        coord3d *raypos, coord3d *raydir);",
        "extern s32 sub_GAME_7F074CAC(Model *model, ModelNode *node,",
        "        coord3d *raypos, coord3d *raydir);",
        "extern u32 modelFindNextProjectileHitCandidate(Model *model,",
        "        coord3d *raypos, coord3d *raydir, ModelNode **nodeptr);",
        "",
    ]
    pieces.extend(bodies)
    pieces.append("")
    return "\n\n".join(pieces)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(render(args.repo))
    print(f"generated {len(FUNCTIONS)} exact projectile/explosion services")


if __name__ == "__main__":
    main()
