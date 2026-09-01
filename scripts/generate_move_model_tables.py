#!/usr/bin/env python3
"""Generate canonical prop/character model tables needed by MoveBond damage."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path


GUARD_HEADER_INCLUDE = (
    "#include <assets/obseg/chr/greatguard2/modelFileHeader.inc.c>"
)
BUG_HEADER_INCLUDE = (
    "#include <assets/obseg/prop/chrbug/ModelFileHeader.inc.c>"
)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def generate(repo: Path) -> str:
    prop_headers_path = repo / "assets/obseg/prop/propModelFileHeaders.inc.c"
    prop_headers = prop_headers_path.read_text()
    if prop_headers.count(BUG_HEADER_INCLUDE) != 1:
        raise ValueError("chrbug model-header include drift")
    prop_headers_without_live_bug = prop_headers.replace(
        BUG_HEADER_INCLUDE,
        "/* chrbug_header is supplied by the exact relocated bug model. */",
    )
    chr_headers_path = repo / "assets/obseg/chr/chrModelFileHeaders.inc.c"
    chr_headers = chr_headers_path.read_text()
    if chr_headers.count(GUARD_HEADER_INCLUDE) != 1:
        raise ValueError("greatguard2 model-header include drift")
    chr_headers_without_live_guard = chr_headers.replace(
        GUARD_HEADER_INCLUDE,
        "/* greatguard2_header is supplied by the exact relocated Dam guard model. */",
    )
    tracked = (
        "assets/obseg/prop/propModelFileHeaders.inc.c",
        "assets/embedded/skeletons/props.inc.c",
        "assets/embedded/skeletons/suit_lf_hand.inc.c",
        "assets/obseg/prop/propItemModelFileRecord.inc.c",
        "assets/obseg/chr/chrModelFileHeaders.inc.c",
        "assets/obseg/chr/chrModelFileRecords.inc.c",
        "assets/obseg/chr/chrHeadHats.inc.c",
    )
    sections = [
        "/* Generated from the canonical pobjdata/cobjdata include manifests. */",
        "#include <ultra64.h>",
        "#include <bondgame.h>",
        "#include <bondconstants.h>",
        "#include <bondtypes.h>",
        '#include "game/chrobjdata.h"',
        "#undef MODELSKELETON",
        "#define MODELSKELETON(NAME, NUMJOINTS, SKELSIZE) \\",
        "    ModelSkeleton SKELETON(NAME) = {NUMJOINTS, 0, JOINTLIST(NAME), SKELSIZE, 0};",
        "#undef PROPFILERECORD",
        "/* All 340 canonical prop records spell their scale explicitly.  Avoid the",
        " * original empty-argument detector, whose token pasting is not accepted by",
        " * the native/3DS clang front ends, while preserving each authored value. */",
        "#define PROPFILERECORD(NAME, SCALE) {&NAME##_header, STR(P##NAME##Z), SCALE},",
        "",
    ]
    for relative in tracked:
        sections.append(f"/* {relative} sha256={digest(repo / relative)} */")
        if relative == "assets/obseg/prop/propModelFileHeaders.inc.c":
            sections.append(prop_headers_without_live_bug)
        elif relative == "assets/obseg/chr/chrModelFileHeaders.inc.c":
            sections.append(chr_headers_without_live_guard)
        else:
            sections.append(f'#include "{relative}"')
        sections.append("")
    sections.extend([
        "/* Exact cardinality of the canonical PitemZ_entries initializer. */",
        "const u32 ge_original_pitem_model_table_count =",
        "    (u32)(sizeof(PitemZ_entries) / sizeof(PitemZ_entries[0]));",
        "",
    ])
    return "\n".join(sections)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    output = generate(args.repo)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(output)
    print(f"generated canonical MoveBond model tables -> {args.output}")


if __name__ == "__main__":
    main()
