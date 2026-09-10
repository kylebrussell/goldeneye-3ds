#!/usr/bin/env python3
"""Retain original background/light algorithms through a native Gfx ABI seam."""
from pathlib import Path
import argparse
import hashlib
import re
from extract_guard_bullet_hit_slice import find_function
from extract_bg_texture_emission import generate as texture_emission


NAMES = {
    'bgBuildRoomVtxBounds': 'ge_bg_build_bounds_exact',
    'bgTestRayIntersectionInRoom': 'ge_bg_ray_hit_exact',
    'bgTestBulletHitBackground': 'ge_bg_bullet_hit_exact',
    'bgTestRayIntersectsBbox': 'ge_bg_ray_box_exact',
    'intersectRayTriangle': 'ge_bg_ray_triangle_exact',
    'lightFixtureInitTables': 'ge_bg_light_init_exact',
    'lightFixtureFindFreeSlot': 'ge_bg_light_free_exact',
    'lightFixtureEntryBegin': 'ge_bg_light_begin_exact',
    'lightFixtureEntryEnd': 'ge_bg_light_end_exact',
    'check_if_imageID_is_light': 'ge_bg_is_light_exact',
    'lightFindVertexBaseForTri': 'ge_bg_light_vertices_exact',
    'extract_vertex_indices_from_triangle': 'ge_bg_light_indices_exact',
    'extract_vertex_coords_from_triangle': 'ge_bg_light_coords_exact',
    'redarken_lights_in_room': 'ge_bg_light_redarken_exact',
    'darken_vertex_in_room': 'ge_bg_light_darken_vertex_exact',
    'darkened_light_table_contains_vertex': 'ge_bg_light_contains_vertex_exact',
    'darken_triangle_in_room': 'ge_bg_light_darken_triangle_exact',
    'darkened_light_table_contains_triangle': 'ge_bg_light_contains_triangle_exact',
    'lightIsCoordNearDarkenedVertex': 'ge_bg_light_near_exact',
    'lightFixtureBreak': 'ge_bg_light_break_exact',
    'clear_light_fixturetable_in_room': 'ge_bg_light_room_exact',
}


def words(body):
    body = re.sub(r'\(\(u32\s*\*\)\s*(gdl|gfx)\)\[(\d)\]', r'ge_bg_gfx_word(\1, \2)', body)
    body = re.sub(r'\(\(s32\s*\*\)\s*(gdl|gfx)\)\[(\d)\]', r'((s32)ge_bg_gfx_word(\1, \2))', body)
    body = re.sub(r'\(\(u16\s*\*\)\s*(gdl|gfx)\)\[(\d)\]', r'ge_bg_gfx_half(\1, \2)', body)
    return body


def native(body, name):
    if name == 'bgBuildRoomVtxBounds':
        body = body.replace('gdl[cmdindex].dma.cmd', '((s8 *)ge_bg_gfx_at(gdl, cmdindex))[0]')
        body = body.replace('gdl[cmdindex].dma.par', '((u8 *)ge_bg_gfx_at(gdl, cmdindex))[1]')
        body = body.replace('gdl[cmdindex].dma.addr', 'ge_bg_gfx_word(ge_bg_gfx_at(gdl, cmdindex), 1)')
        body = body.replace('memaAlloc(', 'ge_bg_bounds_allocate(')
        old = '(Vtx *)(SEGMENT_OFFSET(ge_bg_gfx_word(ge_bg_gfx_at(gdl, cmdindex), 1)) + (u32)vertices)'
        assert old in body
        body = body.replace(old, '(Vtx *)((u8 *)vertices + SEGMENT_OFFSET(ge_bg_gfx_word(ge_bg_gfx_at(gdl, cmdindex), 1)))')
    elif name == 'bgTestRayIntersectionInRoom':
        # The raw room Gdl retains Rare image IDs; texLoadFromGdl's expanded
        # G_SETTIMG pointer-minus-eight lookup is represented by the validated
        # command provenance produced from the same authored texture command.
        for unused in range(2):
            marker = body.index('G_SETTIMG')
            start = body.rfind('\n', 0, marker) + 1
            end = body.index('texnum = *((u16 *) (temp.word | 0x80000000));', marker)
            end = body.index('}', end) + 1
            body = body[:start] + '                        texnum = ge_bg_command_info(roomnum, gdl)->texture;' + body[end:]
        body = body.replace('gdl = &gdl[point->gdlindex];', 'gdl = ge_bg_gfx_at(gdl, point->gdlindex);')
        body = body.replace('    s32 dist;', '    s32 dist = 0; /* IDO-only initial score temporary. */')
        body = body.replace('(Vertex *)((s32)temp.vertices + (((u32 *)gdl)[1] & 0x00ffffff))',
                            '&ge_bg_geometry_current->room_geometry[roomnum].collision_vertices[(ge_bg_gfx_word(gdl, 1) & 0x00ffffff) / 16U]')
        for indices in ('idx', 'idx2'):
            for i in range(3):
                old = f'(Vertex *)((s32)vtxbase - (0 - ({indices}[{i}] << 4)))'
                assert old in body
                body = body.replace(old, f'&vtxbase[{indices}[{i}]]')
            # Zero-area TRI4 padding may name an unloaded cache slot. It can
            # never hit, and must not form an invalid vertex pointer first.
            marker = ('                i = 0;' if indices == 'idx' else '                        i = 0;')
            pos = body.index(marker, body.index(f'{indices}[0] ='))
            cursor = 'gdl = ge_bg_gfx_at(gdl, 1); op = *((s8 *)gdl);' if indices == 'idx' else '++s2;'
            body = body[:pos] + f'''                if ({indices}[0] == {indices}[1] || {indices}[1] == {indices}[2] || {indices}[0] == {indices}[2])
                {{ {cursor} continue; }}
''' + body[pos:]
        origin = '(coord3d *) (((roomnum * 24) + ((s32) ptr_bgdata_room_fileposition_list)) + 12)'
        assert body.count(origin) == 2
        body = body.replace(origin, '&ge_bg_geometry_current->origins[roomnum]')
        body = body.replace('((dx * dx) + (dy * dy)) + (dz * dz)', 'ge_bg_distance_squared(dx, dy, dz)')
        # Original found flag precedes the Archives light-shaft filter; only
        # filtered hits leave output uninitialized. Preserve the filter and
        # avoid publishing that indeterminate output on the native boundary.
        body = body.replace('return found;', 'return found && bestScore != 0x7fffffff;')
    elif name == 'bgTestBulletHitBackground':
        body = body.replace('struct HitThingSub tmp;', 'HitThing tmp;')
        body = body.replace('dx * dx + dy * dy + dz * dz', 'ge_bg_distance_squared(dx, dy, dz)')
        start = body.index('    if (found)', body.index('for (i = 0;'))
        body = body[:start] + '''    if (found && bestdist != 0x7fffffff)
    {
        const GeOriginalBgCommandInfo *provenance = ge_bg_command_info(roomnum, hit->tricmd);
        hit->tileformat = provenance->tile_format;
        hit->tilesize = provenance->tile_size;
    }
    return found && bestdist != 0x7fffffff;
}'''
    elif name == 'lightFindVertexBaseForTri':
        body = body.replace('gfx->dma.cmd', '((u8 *)gfx)[0]')
        body = body.replace('gfx--;', 'gfx = ge_bg_gfx_at(gfx, -1);')
        start = body.index('    ret = gfx->dma.addr;')
        body = body[:start] + '''    ret = (Vtx *)((u8 *)g_BgRoomInfo[room_index].vertices
        + (ge_bg_gfx_word(gfx, 1) & 0x00ffffffU));
    return ret;
}'''
    elif name == 'extract_vertex_indices_from_triangle':
        for i in range(3):
            body = body.replace(f'gfx->tri.tri.v[{i}]', f'((u8 *)gfx)[{5+i}]')
    elif name in ('darken_vertex_in_room', 'darkened_light_table_contains_vertex'):
        body = body.replace('((u32)vertex - (u32)g_BgRoomInfo[room_index].vertices)',
                            '((uintptr_t)vertex - (uintptr_t)g_BgRoomInfo[room_index].vertices)')
    elif name == 'lightFixtureBreak':
        body = body.replace('fixture_gfx->dma.cmd', '((s8 *)fixture_gfx)[0]')
        body = body.replace('fixture_gfx++', 'fixture_gfx = ge_bg_gfx_at(fixture_gfx, 1)')
        for expr in ('hit_gfx < light_fixture_table[i].ptr_start_pertinent_DL',
                     'hit_gfx >= light_fixture_table[i].ptr_end_pertinent_DL',
                     'fixture_gfx < light_fixture_table[i].ptr_end_pertinent_DL'):
            left, op, right = expr.split(' ')
            body = body.replace(expr, f'(uintptr_t){left} {op} (uintptr_t){right}')
        for suffix in ('12', '13', '23'):
            old = f'(diff_x_{suffix} * diff_x_{suffix}) + (diff_y_{suffix} * diff_y_{suffix}) + (diff_z_{suffix} * diff_z_{suffix})'
            assert old in body
            body = body.replace(old, f'ge_bg_distance_squared(diff_x_{suffix}, diff_y_{suffix}, diff_z_{suffix})')
    body = words(body)
    body = body.replace('score = dist - 4;', 'score = (s32)((u32)dist - 4U);')
    body = body.replace('gdl++;', 'gdl = ge_bg_gfx_at(gdl, 1);')
    body = re.sub(r'ptr_bgdata_room_fileposition_list\[([^\]]+)\]\.pos',
                  r'ge_bg_geometry_current->origins[\1]', body)
    return body


def generate(repo):
    bg=(repo/'src/game/bg.c').read_text()
    lights=(repo/'src/game/lightfixture.c').read_text()
    triangle=(repo/'src/game/line_tri_intersect.c').read_text()
    out=['''/* Generated original BG collision/light algorithms, native addresses only. */
#include "ge_original_bg_geometry_internal.h"
#include <math.h>
#include "game/glass.h"
#include <assets/image_externs.h>
#include "game/image_bank.h"
#include <gbi_extension.h>
#ifdef AIPARSE
enum {
#define IMAGE(NAME, ...) IMAGE_##NAME,
#include <assets/images.def>
#undef IMAGE
    GE_BG_IMAGE_COUNT
};
#endif
#define g_BgRoomInfo (ge_bg_geometry_current->rooms)
#define room_data_float1 (ge_bg_geometry_current->level_scale)
#define get_room_data_float1() (ge_bg_geometry_current->level_scale)
#define get_room_data_float2() (ge_bg_geometry_current->inverse_scale)
#define LIGHTFIXTURE_TABLE_MAX ((s32)GE_BG_LIGHT_FIXTURES)
#define DARKENED_LIGHT_TABLE_MAX ((s32)GE_BG_DARKENED_VERTICES)
#define light_fixture_table (ge_bg_geometry_current->fixtures)
#define current_light_fixture_slot (ge_bg_geometry_current->fixture_slot)
#define current_light_fixture_room (ge_bg_geometry_current->fixture_room)
#define darkened_light_table (ge_bg_geometry_current->darkened)
#define cur_entry_darkened_light_table (ge_bg_geometry_current->darkened_cursor)
#define ALIGN16(val) (((val) + 0xF) & ~0xF)
#undef SEGMENT_OFFSET
#define SEGMENT_OFFSET(a) ((uint32_t)(a) & 0x00ffffffU)
static void getRoomPositionScaledByIndex(s32 room, coord3d *out)
{
    for (size_t i=0; i<3; ++i) out->f[i] = ge_bg_geometry_current->origins[room].f[i]
        * ge_bg_geometry_current->inverse_scale;
}
''']
    for name in ('D_80044868','D_80044874','D_80044880','D_8004488C'):
        out.append('static '+re.search(r'BoundVec '+name+r'\s*=\s*\{[^}]+\};',bg)[0])
    functions=[]
    for name in NAMES:
        source=bg if name.startswith('bg') else triangle if name=='intersectRayTriangle' else lights
        original=find_function(source,name)
        body=native(original,name)
        for old,new in NAMES.items(): body=re.sub(r'\b'+old+r'\b',new,body)
        functions.append(body)
        out.append(body[:body.index('{')].strip()+';')
    for body in functions:
        out += [body]
    out.append(texture_emission(repo))
    out.insert(1,'/* Source closure sha256='+hashlib.sha256((''.join(find_function(bg if n.startswith('bg') else triangle if n=='intersectRayTriangle' else lights,n) for n in NAMES)).encode()).hexdigest()+' */')
    return '\n\n'.join(out)+'\n'


if __name__=='__main__':
    parser=argparse.ArgumentParser(); parser.add_argument('repo',type=Path); parser.add_argument('output',type=Path)
    args=parser.parse_args(); args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(generate(args.repo))
