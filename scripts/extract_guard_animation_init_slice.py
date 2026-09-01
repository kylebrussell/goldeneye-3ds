#!/usr/bin/env python3
"""Extract GoldenEye's canonical guard weapon/hit animation startup unit."""

from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path


def load_module(path: Path, name: str):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def source_range(text: str, start: str, end: str) -> str:
    first = text.index(start)
    last = text.index(end, first)
    return text[first:last].rstrip()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    repo = args.repo.resolve()
    helper = load_module(repo / "scripts/extract_guard_bullet_hit_slice.py",
                         "guard_hit_extract")
    data_helper = load_module(
        repo / "scripts/extract_bond_move_explosion_slice.py",
        "explosion_extract")
    chr_source = (repo / "src/game/chr.c").read_text()
    init_source = (repo / "src/game/initactorpropstuff.c").read_text()
    table_source = (repo / "src/game/initanitable.c").read_text()

    pieces = [
        "/* Generated from canonical guard animation startup; do not hand-edit. */",
        "#include <stdint.h>",
        "#include <ultra64.h>",
        "#include <bondgame.h>",
        "#include <bondconstants.h>",
        "#include <bondtypes.h>",
        '#include "assets/animationtable_data.h"',
        '#include "game/chr.h"',
        '#include "game/initactorpropstuff.h"',
        '#include "game/initanitable.h"',
        '#include "game/math_floor.h"',
        '#include "game/model.h"',
        "extern struct animation_table_data *ptr_animation_table;",
        "extern ModelSkeleton skeleton_guard;",
        "extern u32 _animation_entriesSegmentRomStart[];",
        "extern void *ge_port_guard_animation_resolve(u32);",
        "",
    ]

    # Canonical firing group records are one contiguous authored-data block.
    firing_data = source_range(
        chr_source,
        "struct weapon_firing_animation_table rifle_firing_animation_group1[]",
        "struct weapon_firing_animation_table D_80030078[]")
    # The N64 linker deliberately places crouched group A directly after the
    # unterminated group1 record. Native linkers/ASan may insert a redzone, so
    # reproduce that authored contiguous logical table explicitly.
    group_a = data_helper.extract_data(
        chr_source, "crouched_rifle_firing_animation_groupA")
    group_a_body = group_a[group_a.index("{") + 1:group_a.rindex("}")].strip()
    marker = (
        "struct weapon_firing_animation_table "
        "crouched_rifle_firing_animation_group1[] = {\n"
        "    { PTR_ANIM_fire_kneel_right_leg, 27.0, 0, 0, 0, -1.0, "
        "35.0, 75.0, -1.0, -1.0, 31.0, 75.0, 0.87266463, "
        "-0.69813174, 0.90757126, -0.69813174, 1.5, 1.5 },\n};")
    replacement = marker[:-3] + "\n" + group_a_body + "\n};"
    if marker not in firing_data:
        raise ValueError("missing authored crouched rifle adjacency")
    pieces.append(firing_data.replace(marker, replacement))
    # D_80030078 is already owned by the canonical damage slice.
    pieces.append(data_helper.extract_data(chr_source, "D_80030660"))
    for name in ("D_80030984", "D_80030988", "D_8003098C",
                 "D_80030990", "D_80030994", "D_80030998",
                 "D_8003099C", "D_800309A0", "D_800309A4"):
        pieces.append(data_helper.extract_data(chr_source, name))
    pieces.append(data_helper.extract_data(table_source,
                                           "animation_table_ptrs2"))

    pieces.append(helper.find_function(init_source, "sub_GAME_7F0001F0"))
    pieces.append(helper.find_function(init_source, "sub_GAME_7F000290"))
    pieces.append(source_range(table_source, "struct anim_entry\n{",
                               "void expand_ani_table_entries"))
    expand_body = helper.find_function(table_source,
                                       "expand_ani_table_entries")
    pieces.extend([
        "#ifdef GE_PORT_ANIMATION_INIT_NATIVE_ABI",
        "void expand_ani_table_entries(s32 **arg0)",
        "{",
        "    s32 **entry = arg0;",
        "    while (*entry != 0) {",
        "        if (*entry != (s32 *)1)",
        "            *entry = ge_port_guard_animation_resolve((u32)(uintptr_t)*entry);",
        "        entry++;",
        "    }",
        "}",
        "#else",
        expand_body,
        "#endif",
    ])

    group_table = helper.find_function(init_source, "initResolveAnimGroupTable")
    resolve_table = helper.find_function(init_source, "initResolveAnimTable")
    pieces.extend([
        "#if defined(GE_PORT_ANIMATION_INIT_HOST_POINTER_ABI) || "
        "defined(GE_PORT_ANIMATION_INIT_NATIVE_ABI)",
        group_table.replace(
            "((0, animoffset)) + ((s32)ptr_animation_table)",
            "(uintptr_t)ge_port_guard_animation_resolve((u32)animoffset)"),
        "#else",
        group_table,
        "#endif",
        helper.find_function(init_source, "initResolveAnimGroups"),
        "#if defined(GE_PORT_ANIMATION_INIT_HOST_POINTER_ABI) || "
        "defined(GE_PORT_ANIMATION_INIT_NATIVE_ABI)",
        resolve_table.replace("s32 address;", "uintptr_t address;").replace(
            "(*((s32 *)entries)) + (0, address)",
            "(uintptr_t)ge_port_guard_animation_resolve((u32)address)"),
        "#else",
        resolve_table,
        "#endif",
    ])

    # Mach-O host tests and the 3DS final linker cannot materialize the N64
    # linker's absolute ANIM_DATA_* aliases in ordinary C expressions.  These
    # macros recreate the same byte-offset operands at that platform ABI edge.
    anims = ("walking", "running", "sprinting", "walking_unarmed",
             "running_one_handed_weapon", "sprinting_one_handed_weapon",
             "walking_female", "running_female")
    pieces.append("#if defined(GE_PORT_ANIMATION_INIT_OFFSETS)")
    for anim in anims:
        pieces.append(
            f"#define ANIM_DATA_{anim} "
            f"(*(s32 *)(uintptr_t)PTR_ANIM_{anim})")
    pieces.append("#endif")

    init_block = source_range(init_source, "#define ANIM_PTR(anim)",
                              "/**\n * Address: 7F000980")
    init_block = init_block.replace(
        "((ModelAnimation *)((s32)&anim + ((s32)ptr_animation_table)))",
        "((ModelAnimation *)ge_port_guard_animation_resolve((u32)(uintptr_t)&anim))")
    pieces.append(init_block)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n\n".join(pieces) + "\n")


if __name__ == "__main__":
    main()
