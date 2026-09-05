#!/usr/bin/env python3
"""Extract the canonical character bullet-hit geometry/registration tranche.

The generated translation unit keeps the native adapter synchronized with the
decompiled objecthandler, propobj, chr, chrprop, bg and triangle-intersection
bodies. Actor reaction/damage is intentionally outside this slice until its
animation, AI, statistics and sound dependencies are linked authentically.
"""

from __future__ import annotations

import argparse
import re
from pathlib import Path


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
                raise ValueError(f"unterminated signature comment: {name}")
            while after < len(source) and source[after].isspace():
                after += 1
        # A few decomp files retain obsolete implementations inside block
        # comments. Reject signatures whose line starts inside such a comment.
        before = source[:line_start]
        in_block_comment = before.rfind("/*") > before.rfind("*/")
        if not in_block_comment and prefix.strip() \
                and ";" not in prefix and "=" not in prefix \
                and after < len(source) and source[after] == "{":
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
                    return source[line_start:index + 1]
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
            if (state == "string" and char == '"') or \
                    (state == "char" and char == "'"):
                state = "code"
        index += 1
    raise ValueError(f"unterminated function: {name}")


def apply_native_model_hit_abi(body: str, name: str) -> str:
    """Translate only N64 address/byte-order operations at the port ABI.

    Geometry traversal, ordering and intersection behavior remain the exact
    decompiled bodies.  Relocated host models hold direct pointers while their
    authored Gfx streams remain big-endian, unlike N64 segmented pointers and
    native-endian command words assumed by the original source.
    """
    if name == "bgTestHitOnObj":
        body = body.replace("    Vertex *vtxbase;", """#if defined(GE_PORT_MODEL_HIT_NATIVE_ABI)
    GeNativeModelHitVertices vertex_range = ge_port_model_hit_vertices;
#else
    Vertex *vtxbase;
#endif""")
        body = body.replace("((u32 *) gdl)[1]", "GE_MODEL_HIT_U32(gdl, 1)")
        body = body.replace("((u32 *) gdl)[0]", "GE_MODEL_HIT_U32(gdl, 0)")
        body = body.replace("((u16 *) gdl)[3]", "GE_MODEL_HIT_U16(gdl, 3)")
        body = body.replace("((u16 *) gdl)[2]", "GE_MODEL_HIT_U16(gdl, 2)")
        body = body.replace("((u16 *) gdl)[1]", "GE_MODEL_HIT_U16(gdl, 1)")
        vertex_base = "(Vertex *) ((((s32) vertices) + padC) - (op << 4))"
        if body.count(vertex_base) != 1:
            raise ValueError("unexpected bgTestHitOnObj vertex-base shape")
        body = body.replace("vtxbase = " + vertex_base + ";", """#if defined(GE_PORT_MODEL_HIT_NATIVE_ABI)
            if (!ge_native_model_hit_vertices_load(&vertex_range,
                    GE_MODEL_HIT_U32(gdl, 0), GE_MODEL_HIT_U32(gdl, 1)))
                return FALSE;
#else
            vtxbase = GE_MODEL_HIT_VERTEX_BASE(vertices, padC, op << 4);
#endif""")
        for indices in ("idx", "idx2"):
            # Rare pads TRI4 with repeated-index triangles. Rejecting an
            # unloaded padding slot must not discard an earlier valid hit.
            # Such triangles have zero area regardless of their coordinates.
            cursor = "gdl = GE_MODEL_HIT_GDL_NEXT(gdl);" if indices == "idx" else "++s2;"
            marker = re.compile(r"(?P<indent> +)for \(i = 0; i < 3; i\+\+\)\s*\{\s*v = vtxbase;\s*v \+= " + indices + r"\[i\];")
            if len(marker.findall(body)) != 1:
                raise ValueError("unexpected collision vertex-loop shape")
            body = marker.sub(lambda m: "#if defined(GE_PORT_MODEL_HIT_NATIVE_ABI)\n"
                + f"if ({indices}[0] == {indices}[1] || {indices}[0] == {indices}[2] || {indices}[1] == {indices}[2]) {{ {cursor} continue; }}\n"
                + "#endif\n" + m.group(0), body)
            old = re.compile(r"v = vtxbase;\s*v \+= " + indices + r"\[i\];")
            if len(old.findall(body)) != 1:
                raise ValueError("unexpected collision vertex-read shape")
            body = old.sub("""#if defined(GE_PORT_MODEL_HIT_NATIVE_ABI)
                    v = ge_port_model_hit_vertex(&vertex_range, vertices, """
                    + indices + """[i]);
                    if (v == NULL) return FALSE;
#else
                    v = vtxbase; v += """ + indices + """[i];
#endif""", body)
            old_points = (f"pt0 = vtxbase; pt0 += {indices}[0]; "
                f"pt1 = vtxbase; pt1 += {indices}[1]; "
                f"pt2 = vtxbase; pt2 += {indices}[2];")
            if body.count(old_points) != 1:
                raise ValueError("unexpected collision triangle-read shape")
            points = "\n".join(f"pt{i} = ge_port_model_hit_vertex("
                f"&vertex_range, vertices, {indices}[{i}]);" for i in range(3))
            body = body.replace(old_points,
                "#if defined(GE_PORT_MODEL_HIT_NATIVE_ABI)\n" + points
                + "\n#else\n" + old_points + "\n#endif")
            for i in range(3):
                old_point = f"&vtxbase[{indices}[{i}]]"
                body = body.replace(old_point, f"pt{i}")
        if body.count("gdl++;") != 2 or body.count("tcmd--;") != 2:
            raise ValueError("unexpected bgTestHitOnObj Gfx cursor shape")
        body = body.replace("gdl++;", "gdl = GE_MODEL_HIT_GDL_NEXT(gdl);")
        body = body.replace("tcmd--;", "tcmd = GE_MODEL_HIT_GDL_PREV(tcmd);")
        texture_lookup = re.compile(
            r"padC = \(\(u32 \*\) tcmd\)\[1\] - 8;\n"
            r"(?P<indent>\s*)texnum = \*\(\(u16 \*\) "
            r"\(padC \| 0x80000000\)\);")
        if len(texture_lookup.findall(body)) != 2:
            raise ValueError("unexpected bgTestHitOnObj texture lookup shape")
        body = texture_lookup.sub(
            lambda match: "#if defined(GE_PORT_MODEL_HIT_NATIVE_ABI)\n"
                + match.group("indent")
                + "texnum = ge_port_model_hit_texture_number(tcmd);\n"
                + "#else\n" + match.group(0) + "\n#endif",
            body)
    elif name == "propobjFindHit":
        old_primary = """s3 = (Gfx *)((uintptr_t)rodata->BaseAddr + ((u32)rodata->Primary & 0xffffff));"""
        new_primary = """#if defined(GE_PORT_MODEL_HIT_NATIVE_ABI)
                            s3 = rodata->Primary;
#else
                            s3 = (Gfx *)((uintptr_t)rodata->BaseAddr + ((u32)rodata->Primary & 0xffffff));
#endif"""
        if body.count(old_primary) != 2:
            raise ValueError("unexpected propobjFindHit primary-pointer shape")
        body = body.replace(old_primary, new_primary)
        old_secondary = """s5 = (void *)((uintptr_t)rodata->BaseAddr + ((u32)rodata->Secondary & 0xffffff));"""
        new_secondary = """#if defined(GE_PORT_MODEL_HIT_NATIVE_ABI)
                            s5 = rodata->Secondary;
#else
                            s5 = (void *)((uintptr_t)rodata->BaseAddr + ((u32)rodata->Secondary & 0xffffff));
#endif"""
        if body.count(old_secondary) != 1:
            raise ValueError("unexpected collision secondary-pointer shape")
        body = body.replace(old_secondary, new_secondary)
        old_secondary_dl = """s5 = (Gfx *)((uintptr_t)rodata->BaseAddr + ((u32)rodata->Secondary & 0xffffff));"""
        new_secondary_dl = """#if defined(GE_PORT_MODEL_HIT_NATIVE_ABI)
                            s5 = rodata->Secondary;
#else
                            s5 = (Gfx *)((uintptr_t)rodata->BaseAddr + ((u32)rodata->Secondary & 0xffffff));
#endif"""
        if body.count(old_secondary_dl) != 1:
            raise ValueError("unexpected DL secondary-pointer shape")
        body = body.replace(old_secondary_dl, new_secondary_dl)
        old_vertices = "vertices = (void *)(uintptr_t)rodata->BaseAddr;"
        new_vertices = """#if defined(GE_PORT_MODEL_HIT_NATIVE_ABI)
                        vertices = rodata->Vertices;
#else
                        vertices = (void *)(uintptr_t)rodata->BaseAddr;
#endif"""
        if body.count(old_vertices) != 1:
            raise ValueError("unexpected DL vertex-base shape")
        body = body.replace(old_vertices, new_vertices)
        marker = "if (s3 != NULL)\n        {"
        native_base = """if (s3 != NULL)
        {
#if defined(GE_PORT_MODEL_HIT_NATIVE_ABI)
            ge_port_model_hit_base_addr = type == MODELNODE_OPCODE_DLCOLLISION
                ? node->Data->DisplayListCollisions.BaseAddr
                : node->Data->DisplayList.BaseAddr;
            ge_port_model_hit_base_size =
                ge_original_native_model_hit_blob_size(
                    ge_port_model_hit_base_addr);
            const void *source_vertices = type == MODELNODE_OPCODE_DLCOLLISION
                ? (const void *)node->Data->DisplayListCollisions.Vertices
                : (const void *)node->Data->DisplayList.Vertices;
            ge_port_model_hit_vertices.count = type == MODELNODE_OPCODE_DLCOLLISION
                ? (size_t)node->Data->DisplayListCollisions.numVertices
                : (size_t)node->Data->DisplayList.numVertices;
            ge_port_model_hit_vertices.blob_offset = 0U;
            ge_port_model_hit_vertices.blob_offset_known =
                ge_original_native_model_hit_vertex_offset(
                    ge_port_model_hit_base_addr, source_vertices,
                    &ge_port_model_hit_vertices.blob_offset);
            ge_port_model_hit_vertices.base_index = 0;
            ge_port_model_hit_vertices.loaded = 0;
#endif"""
        if body.count(marker) != 1:
            raise ValueError("unexpected propobjFindHit dispatch shape")
        body = body.replace(marker, native_base)
    return body


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    game = args.repo / "src/game"
    sources = {
        "object": (game / "objecthandler.c").read_text(),
        "propobj": (game / "propobj.c").read_text(),
        "chr": (game / "chr.c").read_text(),
        "chrprop": (game / "chrprop.c").read_text(),
        "bg": (game / "bg.c").read_text(),
        "triangle": (game / "line_tri_intersect.c").read_text(),
    }
    pieces = [
        "/* Generated from canonical decompiled sources; do not hand-edit. */",
        "#include <stdint.h>",
        "#include <ultra64.h>",
        "#include <PR/gbi.h>",
        '#include "include/math.h"',
        '#include "gbi_extension.h"',
        "#include <bondconstants.h>",
        "#include <bondtypes.h>",
        '#include "bondaicommands.h"',
        "typedef int PLAYERFLAG;",
        '#include "game/bg.h"',
        '#include "game/bondview.h"',
        '#include "game/chr.h"',
        '#include "game/gun.h"',
        '#include "game/matrixmath.h"',
        '#include "game/model.h"',
        '#include "game/objecthandler.h"',
        '#include "game/propobj.h"',
        "extern void matrix_4x4_invert_affine(Mtxf *matrix, Mtxf *result);",
        "extern bool modelTestRayIntersectsNodeBBox(Model *model, ModelNode *node, coord3d *pos, coord3d *dir);",
        "extern s32 sub_GAME_7F074CAC(Model *model, ModelNode *node, coord3d *raypos, coord3d *raydir);",
        "#if defined(GE_PORT_MODEL_HIT_NATIVE_ABI)",
        '#include "ge_native_model_hit_vertices.h"',
        "extern size_t ge_original_native_model_hit_blob_size(const void *base_address);",
        "extern int ge_original_native_model_hit_vertex_offset(const void *, const void *, uint32_t *);",
        "static GeNativeModelHitVertices ge_port_model_hit_vertices;",
        "static Vertex *ge_port_model_hit_vertex(const GeNativeModelHitVertices *range, Vertex *vertices, unsigned slot) {",
        "    size_t index;",
        "    return vertices != NULL && ge_native_model_hit_vertex_index(range, slot, &index)",
        "        ? vertices + index : NULL;",
        "}",
        "static const uint8_t *ge_port_model_hit_base_addr;",
        "static size_t ge_port_model_hit_base_size;",
        "static uint16_t ge_port_model_hit_read_be16(const void *pointer) {",
        "    const uint8_t *p = pointer;",
        "    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);",
        "}",
        "static uint32_t ge_port_model_hit_read_be32(const void *pointer) {",
        "    const uint8_t *p = pointer;",
        "    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)",
        "        | ((uint32_t)p[2] << 8) | p[3];",
        "}",
        "static s32 ge_port_model_hit_texture_number(const Gfx *command) {",
        "    const uint32_t address = ge_port_model_hit_read_be32(",
        "        (const uint8_t *)command + 4U);",
        "    const size_t offset = address & UINT32_C(0x00ffffff);",
        "    if (ge_port_model_hit_base_addr == NULL || offset < 8U",
        "            || offset > ge_port_model_hit_base_size",
        "            || ge_port_model_hit_base_size - offset < 1U) return -1;",
        "    return ge_port_model_hit_read_be16(",
        "        ge_port_model_hit_base_addr + offset - 8U);",
        "}",
        "#define GE_MODEL_HIT_U32(pointer, index) ge_port_model_hit_read_be32((const uint8_t *)(pointer) + (index) * 4U)",
        "#define GE_MODEL_HIT_U16(pointer, index) ge_port_model_hit_read_be16((const uint8_t *)(pointer) + (index) * 2U)",
        "#define GE_MODEL_HIT_GDL_NEXT(pointer) ((Gfx *)((uint8_t *)(pointer) + 8U))",
        "#define GE_MODEL_HIT_GDL_PREV(pointer) ((Gfx *)((uint8_t *)(pointer) - 8U))",
        "#define GE_MODEL_HIT_VERTEX_BASE(pointer, offset, back) ((Vertex *)((uintptr_t)(pointer) + (size_t)(offset) - (size_t)(back)))",
        "#else",
        "#define GE_MODEL_HIT_U32(pointer, index) (((u32 *)(pointer))[index])",
        "#define GE_MODEL_HIT_U16(pointer, index) (((u16 *)(pointer))[index])",
        "#define GE_MODEL_HIT_GDL_NEXT(pointer) ((pointer) + 1)",
        "#define GE_MODEL_HIT_GDL_PREV(pointer) ((pointer) - 1)",
        "#define GE_MODEL_HIT_VERTEX_BASE(pointer, offset, back) ((Vertex *)((((s32)(pointer)) + (offset)) - (back)))",
        "#endif",
        "",
        "BoundVec D_8003204C = {0x7FFF, 0x7FFF, 0x7FFF};",
        "BoundVec D_80032058 = {-0x8000, -0x8000, -0x8000};",
        "coord3d D_80032064 = {0, 0, 0};",
        "BoundVec D_80032070 = {0x7FFF, 0x7FFF, 0x7FFF};",
        "BoundVec D_8003207C = {-0x8000, -0x8000, -0x8000};",
        "coord3d D_80032088 = {0, 0, 0};",
        "",
    ]
    sequence = (
        ("bg", "bgTestRayIntersectsBbox"),
        ("triangle", "intersectRayTriangle"),
        ("propobj", "bgTestHitOnObj"),
        ("propobj", "projectileTestPropBoundingSphere"),
        ("propobj", "propobjFindHit"),
        ("object", "sub_GAME_7F06B120"),
        ("object", "sub_GAME_7F06B248"),
        ("object", "probably_damage_detail_blood_effect_related"),
        ("object", "sub_GAME_7F06C010"),
        ("chrprop", "chrpropAddBulletHit"),
        ("chr", "chrTestHit"),
    )
    pieces.extend(apply_native_model_hit_abi(
                      find_function(sources[source], name), name)
                  for source, name in sequence)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n\n".join(pieces) + "\n")


if __name__ == "__main__":
    main()
