#!/usr/bin/env python3
"""Extract the exact decompiled model bodies needed by player_gait_object.

The generated translation unit keeps the port adapter synchronized with the
canonical model.c implementation. The primary/secondary joint path retains
the original quaternion blend; only its pointer-through-s32 calls are routed
through typed, native-safe providers.
"""

from __future__ import annotations

import argparse
from pathlib import Path


FUNCTIONS = (
    "modelFindNodeMtxIndex",
    "modelFindNodeMtx",
    "getsubmatrix",
    "getinstsize",
    "getjointsize",
    "getpartoffset",
    "setpartoffset",
    "getsuboffset",
    "setsuboffset",
    "modelSetScale",
    "modelSetDistanceDisabled",
    "interpolate3dVectors",
    "sub_GAME_7F06D0CC",
    "sub_GAME_7F06D160",
    "sub_GAME_7F06D490",
    "subcalcpos",
    "process_01_group_heading",
    "modelBuildGroupMatrices",
    "sub_GAME_7F06DB5C",
    "modelAnimReadBitsAsU16Angle",
    "sub_GAME_7F06DEC0",
    "process_02_position",
    "process_15_subposition",
    "modelUpdateDistanceRelations",
    "modelApplyDistanceRelations",
    "modelApplyToggleRelations",
    "modelApplyHeadRelations",
    "modelUpdateReorderRelations",
    "subcalcmatrices",
    "modelCalculateRwDataIndexes",
    "modelCalculateRwDataLen",
    "modelApplyReorderRelationsByArg",
    "modelApplyReorderRelations",
    "modelInitRwData",
    "modelInit",
    "animInit",
    "modelTestRayIntersectsTransformedBBox",
    "modelTestRayIntersectsNodeBBox",
    "sub_GAME_7F074CAC",
)


def find_function(source: str, name: str) -> str:
    needle = f"{name}("
    search_from = 0
    while True:
        pos = source.find(needle, search_from)
        if pos < 0:
            raise ValueError(f"function not found: {name}")
        line_start = source.rfind("\n", 0, pos) + 1
        prefix = source[line_start:pos]
        paren = pos + len(name)
        depth = 0
        end_paren = paren
        while end_paren < len(source):
            if source[end_paren] == "(":
                depth += 1
            elif source[end_paren] == ")":
                depth -= 1
                if depth == 0:
                    break
            end_paren += 1
        after = end_paren + 1
        while after < len(source) and source[after].isspace():
            after += 1
        if source.startswith("//", after):
            after = source.find("\n", after)
            if after < 0:
                after = len(source)
            while after < len(source) and source[after].isspace():
                after += 1
        if prefix.strip() and ";" not in prefix and "=" not in prefix and \
                after < len(source) and source[after] == "{":
            brace = after
            break
        search_from = pos + len(needle)

    depth = 0
    index = brace
    state = "code"
    while index < len(source):
        char = source[index]
        nxt = source[index + 1] if index + 1 < len(source) else ""
        if state == "code":
            if char == "/" and nxt == "*":
                state = "block"
                index += 2
                continue
            if char == "/" and nxt == "/":
                state = "line"
                index += 2
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
                    return source[line_start : index + 1]
        elif state == "block":
            if char == "*" and nxt == "/":
                state = "code"
                index += 2
                continue
        elif state == "line":
            if char == "\n":
                state = "code"
        elif state in ("string", "char"):
            if char == "\\":
                index += 2
                continue
            if (state == "string" and char == '"') or (
                state == "char" and char == "'"
            ):
                state = "code"
        index += 1
    raise ValueError(f"unterminated function: {name}")


def find_declaration(source: str, name: str) -> str:
    import re
    match = re.search(rf"(?m)^[^/\n][^;\n]*\b{name}\b[^;\n]*;$", source)
    if match is None:
        raise ValueError(f"declaration not found: {name}")
    return match.group(0)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("model_source", type=Path)
    parser.add_argument("setup_source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    source = args.model_source.read_text()
    setup_source = args.setup_source.read_text()
    object_source = (args.model_source.parent / "objecthandler.c").read_text()
    bondview_source = (args.model_source.parent / "bondview.c").read_text()
    pieces = [
        "/* Generated from src/game/model.c; do not hand-edit. */",
        '#include "ge_original_player_gait_internal.h"',
        '#include <math.h>',
        '#include <stdint.h>',
        "typedef int PLAYERFLAG;",
        '#include "game/bondview.h"',
        '#include "game/player.h"',
        '#include "ge_original_player_spawn_internal.h"',
        "#define g_CurrentPlayer (ge_original_spawn_player_get())",
        "#if defined(GE_PORT_MODEL_HOST_RWDATA_ABI)",
        "#define GE_PORT_RWDATA_ALIGNMENT(record_type) _Alignof(union ModelRwData)",
        "#else",
        "#define GE_PORT_RWDATA_ALIGNMENT(record_type) _Alignof(record_type)",
        "#endif",
        "#define GE_PORT_ALIGN_RWDATA_INDEX(value, record_type) do { \\",
        "    const size_t ge_alignment_words = GE_PORT_RWDATA_ALIGNMENT(record_type) / sizeof(u32); \\",
        "    (value) = (u16)(((value) + ge_alignment_words - 1u) \\",
        "        & ~(ge_alignment_words - 1u)); \\",
        "} while (0)",
        "#define loadAnimationFrame ge_port_player_gait_load_animation_frame",
        "#define modelResetAnimationsScratchBuffer ge_port_player_gait_reset_animation_frames",
        "#define instcalcmatrices ge_port_player_gait_instcalcmatrices",
        "#define process_15_subposition ge_port_player_gait_process_15_subposition",
        "",
        find_declaration(object_source, "g_ModelDistanceDisabled"),
        find_declaration(object_source, "g_ModelDistanceScale"),
        find_declaration(object_source, "D_80036408"),
        find_function(bondview_source, "getPlayer_c_lodscalez"),
        "",
    ]
    for name in FUNCTIONS:
        body = find_function(source, name)
        if name == "sub_GAME_7F06DB5C":
            # Retain every original quaternion/auxiliary-matrix branch. The
            # decomp spells a parent/matrix pointer as s32 and calls the
            # two-argument joint callback through a three-s32 cast. Only
            # repair that ABI; this path also serves guards, not just gait.
            replacements = (
                ("sub_GAME_7F06DB5C(",
                 "ge_port_player_gait_build_group_quaternion("),
                ("s32 *new_var;", "intptr_t *new_var;"),
                ("s32 sp1C;", "intptr_t sp1C;"),
                ("(s32)arg2->Parent", "(intptr_t)arg2->Parent"),
                ("(s32)&sp48[sp54]", "(intptr_t)&sp48[sp54]"),
                ("((void (*)(s32, s32, s32)) g_ModelJointPositionedFunc)"
                 "(sp54, sp1C, sp1C);",
                 "g_ModelJointPositionedFunc(sp54, (Mtxf *)sp1C);"),
            )
            for needle, replacement in replacements:
                if body.count(needle) != 1:
                    raise ValueError("quaternion matrix native-ABI adaptation drift: "
                                     + needle)
                body = body.replace(needle, replacement)
        if name == "modelCalculateRwDataIndexes":
            runtime_records = (
                ("ModelRoData_HeaderRecord",
                 "Header", "struct ModelRwData_HeaderRecord"),
                ("ModelRoData_Op07Record",
                 "Op07", "struct ModelRwData_Op07Record"),
                ("ModelRoData_LODRecord",
                 "LOD", "struct ModelRwData_LODRecord"),
                ("ModelRoData_SwitchRecord",
                 "Switch", "struct ModelRwData_SwitchRecord"),
                ("ModelRoData_HeadPlaceholderRecord",
                 "HeadPlaceholder",
                 "struct ModelRwData_HeadPlaceholderRecord"),
                ("ModelRoData_BSPRecord",
                 "BSP", "struct ModelRwData_BSPRecord"),
                ("ModelRoData_Op11Record",
                 "Op11", "struct ModelRwData_Op11Record"),
                ("ModelRoData_GunfireRecord",
                 "Gunfire", "struct ModelRwData_GunfireRecord"),
                ("ModelRoData_DisplayList_CollisionRecord",
                 "DisplayListCollisions",
                 "struct ModelRwData_DisplayList_CollisionRecord"),
            )
            for ro_type, field, rw_type in runtime_records:
                needle = (
                    f"{ro_type} *rodata = "
                    f"&node->Data->{field};\n"
                    "                    rodata->RwDataIndex = len;"
                )
                replacement = needle.replace(
                    "                    rodata->RwDataIndex = len;",
                    f"                    GE_PORT_ALIGN_RWDATA_INDEX(len, {rw_type});\n"
                    "                    rodata->RwDataIndex = len;",
                )
                if body.count(needle) != 1:
                    raise ValueError(f"alignment insertion drift: {ro_type}")
                body = body.replace(needle, replacement)
        if name == "modelApplyReorderRelations":
            needle = "union ModelRwData *rwdata = modelGetNodeRwData(model, node);"
            replacement = (
                "struct ModelRwData_BSPRecord *rwdata = "
                "(struct ModelRwData_BSPRecord *)modelGetNodeRwData(model, node);"
            )
            if body.count(needle) != 1:
                raise ValueError("BSP rwdata native-type adaptation drift")
            body = body.replace(needle, replacement).replace(
                "rwdata->BSP.visible", "rwdata->visible"
            )
        if name == "modelUpdateReorderRelations":
            needle = "union ModelRwData *rwdata = modelGetNodeRwData(model, node);"
            replacement = (
                "struct ModelRwData_BSPRecord *rwdata = "
                "(struct ModelRwData_BSPRecord *)modelGetNodeRwData(model, node);"
            )
            if body.count(needle) != 1:
                raise ValueError("BSP update native-type adaptation drift")
            body = body.replace(needle, replacement).replace(
                "rwdata->BSP.visible", "rwdata->visible"
            )
        if name == "modelUpdateDistanceRelations":
            needle = "union ModelRwData *rwdata = modelGetNodeRwData(model, node);"
            replacement = (
                "struct ModelRwData_LODRecord *rwdata = "
                "(struct ModelRwData_LODRecord *)modelGetNodeRwData(model, node);"
            )
            if body.count(needle) != 1:
                raise ValueError("LOD rwdata native-type adaptation drift")
            body = body.replace(needle, replacement).replace(
                "rwdata->LOD.visible", "rwdata->visible"
            )
        if name == "process_02_position":
            body = body.replace(
                "sub_GAME_7F06DEC0(",
                "ge_port_player_gait_decode_joint_handle(",
            ).replace(
                "sub_GAME_7F06DB5C(",
                "ge_port_player_gait_build_group_quaternion(",
            )
        pieces.append(body + "\n")
    pieces.extend([
        "#undef process_15_subposition",
        "#define skeleton_guard ge_port_player_gait_root_skeleton",
        find_function(setup_source, "sub_GAME_7F0062C0") + "\n",
        "#undef skeleton_guard",
    ])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(pieces))


if __name__ == "__main__":
    main()
