#!/usr/bin/env python3
"""Compare prepared world runs to the original scalar traversal, including publication changes."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
source = (repo / 'platform/3ds/source/main.c').read_text()

def block(start):
    begin = source.index('{', start)
    depth = 1
    end = begin + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

compat = block(source.index('static bool dam_batch_materials_compatible('))
loop = block(source.index('            while (next < draw_batch_count)', source.index('static void renderer_draw(')))
reference = r'''
            while (next < draw_batch_count) {
                const size_t next_source = camera_render
                    ? dam_preview->render_batches[next].source_batch : next;
                const size_t next_first = camera_render
                    ? dam_preview->render_batches[next].first_vertex
                    : dam_preview->batches[next_source].first_vertex;
                const GeDamRoomDrawBatch *next_batch =
                    &dam_preview->batches[next_source];
                const bool next_room_visible = !gpu_world_render
                    || renderer_room_visible(&room_visibility, next_batch->room_id);
                if (!next_room_visible || next_first != scanned_vertex_end
                        || batch->coordinate_space
                            != next_batch->coordinate_space) {
                    break;
                }
                /* The anchor batch already proved this draw visible. With
                 * identical material/texture/projection, the GPU can clip
                 * subsequent triangles in the same draw. CPU culling is
                 * only needed to cross an incompatible material's range. */
                if (!dam_batch_materials_compatible(batch, next_batch)) {
                    if (gpu_world_render && !renderer_world_batch_profiled(
                            dam_preview, next_source,
                            world_batch_visibility_cache,
                            visibility_cache_count, clip_contexts, room_frustum)) {
                        scanned_vertex_end += next_batch->vertex_count;
                        next++;
                        continue;
                    }
                    break;
                }
                scanned_vertex_end += camera_render
                    ? dam_preview->render_batches[next].vertex_count
                    : next_batch->vertex_count;
                vertex_count = scanned_vertex_end - first_vertex;
                merged_authored_batches++;
                next++;
            }

'''
planner = block(source.index('static void renderer_update_world_batch_runs('))
planner += '\n' + block(source.index('static size_t renderer_count_world_material_groups('))
compat += '\n' + block(source.index('static bool dam_batches_compatible('))
prefix = r'''
#include "ge_draw_batch_visibility.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef struct {size_t source_batch, first_vertex, vertex_count;} RenderBatch;
typedef struct {
 GeDamRoomDrawBatch *batches; RenderBatch *render_batches;
 size_t batch_count;
 struct {struct {size_t batch_count;} scene; size_t overlay_batch_count; bool initialized; void *room_ranges;
  GeDamRoomWorldVertex *vertices; GeDamRoomDrawBatch *batches;} dynamic_scene;
 GeDamRoomWorldVertex *source_vertices;
 uint32_t *gpu_batch_run_ends;
 size_t gpu_batch_run_count; uintptr_t gpu_batch_run_room_identity;
} RuntimeDamPreview;
static struct {uint64_t world_merge_detail[3], world_run_prepare[4], rendered_frames;} fine_profile;
static uint64_t svcGetSystemTick(void) {return 0;}
static bool allocation_failure;
static void *test_realloc(void *p,size_t n) {return allocation_failure ? NULL : realloc(p,n);}
#define realloc test_realloc
typedef struct {unsigned ignored;} Membership;
static bool visible[128], rooms[8];
static size_t checks;
static bool renderer_room_visible(const Membership *m, uint32_t room)
{ (void)m; return room < 8 && rooms[room]; }
static bool renderer_world_batch_profiled(const RuntimeDamPreview *p, size_t i,
    uint8_t *cache, size_t count, const GeDrawBatchClipContext *c, const uint8_t *r)
{ (void)p; (void)cache; (void)count; (void)c; (void)r; ++checks; return visible[i]; }
typedef struct {size_t next, vertices, merged, checks;} Run;
'''
setup = r'''
    const bool camera_render = false, gpu_world_render = true, sample_submission = false;
    const size_t source_index = start;
    const Membership room_visibility = {0};
    uint8_t world_batch_visibility_cache[128] = {0};
    const size_t visibility_cache_count = 128;
    const GeDrawBatchClipContext clip_contexts[3] = {0};
    const uint8_t *room_frustum = NULL;
    const GeDamRoomDrawBatch *batch = &dam_preview->batches[start];
    const size_t first_vertex = batch->first_vertex;
    size_t vertex_count = batch->vertex_count, next = start + 1;
    size_t scanned_vertex_end = first_vertex + vertex_count, merged_authored_batches = 1;
    checks = 0;
    (void)source_index; (void)sample_submission; (void)camera_render; (void)gpu_world_render; (void)world_batch_visibility_cache;
    (void)visibility_cache_count; (void)clip_contexts; (void)room_frustum;
'''
test = prefix + compat + planner
for name, body in [('candidate', loop), ('reference', reference)]:
    test += '\nstatic Run ' + name + '(RuntimeDamPreview *dam_preview, size_t start, size_t draw_batch_count) {\n' + setup + body + '\nreturn (Run){next,vertex_count,merged_authored_batches,checks};\n}\n'
test += r'''
int main(void) {
    uint32_t random = 1771;
    size_t tested = 0, old_checks = 0, new_checks = 0;
    for (size_t frame = 0; frame < 4096; ++frame) {
        GeDamRoomDrawBatch batches[128] = {0};
        RuntimeDamPreview preview = {0};
        preview.batches = batches;
        const size_t count = 1 + frame % 128;
        size_t vertex = 0;
        for (size_t room = 0; room < 8; ++room) rooms[room] = (room + frame) % 5 != 0;
        for (size_t i = 0; i < count; ++i) {
            random = random * 1664525U + 1013904223U;
            if (i) batches[i] = batches[i - 1];
            batches[i].first_vertex = vertex;
            batches[i].vertex_count = (1 + random % 5) * 3;
            if (random % 3 == 0) batches[i].material.primitive_color.red ^= 1;
            if (random % 11 == 0) batches[i].texture.texture_id ^= 1;
            if (random % 19 == 0) batches[i].coordinate_space ^= 1;
            if (random % 17 == 0) batches[i].room_id = (uint8_t)(random % 8);
            visible[i] = random % 7 < 3;
            vertex += batches[i].vertex_count + (random % 23 == 0 ? 3 : 0);
        }
        preview.batch_count = count;
        preview.dynamic_scene.overlay_batch_count = frame % 7 < count ? frame % 7 : 0;
        allocation_failure = frame % 17 == 0;
        renderer_update_world_batch_runs(&preview, 0);
        /* Partial immutable publication must invalidate a prior compatible run. */
        if (count > 3 && frame % 5 == 0) {
            batches[1].material.primitive_color.red ^= 1;
            renderer_update_world_batch_runs(&preview, 1);
        }
        size_t material_groups = count != 0 ? 1 : 0;
        for (size_t i=1; i<count; ++i)
            if (!dam_batches_compatible(&batches[i-1], &batches[i])) ++material_groups;
        assert(renderer_count_world_material_groups(&preview)==material_groups);
        for (size_t i = 0; i < count; ++i) if (visible[i] && rooms[batches[i].room_id]) {
            Run a = candidate(&preview, i, count), b = reference(&preview, i, count);
            assert(a.next == b.next && a.vertices == b.vertices && a.merged == b.merged && a.checks == b.checks);
            const size_t old_end = batches[i].first_vertex + b.vertices;
            const size_t new_end = batches[i].first_vertex + a.vertices;
            for (size_t j = i; j < a.next; ++j) {
                if (batches[j].first_vertex >= old_end && batches[j].first_vertex < new_end)
                    assert(!visible[j]); /* Only fully clipped trailing geometry is added. */
                if (visible[j] && batches[j].first_vertex < new_end)
                    assert(dam_batch_materials_compatible(&batches[i], &batches[j]));
            }
            old_checks += b.checks; new_checks += a.checks; ++tested;
        }
        free(preview.gpu_batch_run_ends);
    }
    /* Room publications have a lifetime independent of overlay buffers. */
    GeDamRoomDrawBatch room_batches[6] = {0};
    for (size_t i=0; i<6; ++i) {
        room_batches[i].first_vertex=i*3; room_batches[i].vertex_count=3;
    }
    int room_ranges[2] = {0};
    RuntimeDamPreview p = {0};
    p.batches=room_batches; p.batch_count=6; p.dynamic_scene.overlay_batch_count=2;
    p.dynamic_scene.initialized=true; p.dynamic_scene.room_ranges=&room_ranges[0];
    p.dynamic_scene.batches=room_batches; p.dynamic_scene.scene.batch_count=6;
    allocation_failure=false;
    renderer_update_world_batch_runs(&p,0);
    assert(p.gpu_batch_run_ends[0]==4);
    uint32_t *plan=p.gpu_batch_run_ends;
    room_batches[4].material.primitive_color.red=7; /* animated overlay */
    renderer_update_world_batch_runs(&p,0); /* redundant full texture upload */
    assert(p.gpu_batch_run_ends==plan && p.gpu_batch_run_ends[0]==4);
    /* A transient old preview count cannot turn an animated overlay into
     * immutable room metadata or trigger a needless plan rebuild. */
    p.batch_count=5;
    renderer_update_world_batch_runs(&p,0);
    assert(p.gpu_batch_run_count==4 && p.gpu_batch_run_ends[0]==4);
    p.batch_count=6;
    /* Same-sized replacement rooms must invalidate the previous run. */
    room_batches[1].material.primitive_color.red=9;
    p.dynamic_scene.room_ranges=&room_ranges[1];
    renderer_update_world_batch_runs(&p,0);
    assert(p.gpu_batch_run_ends[0]==1 && p.gpu_batch_run_ends[1]==2
        && p.gpu_batch_run_ends[2]==4);
    p.dynamic_scene.overlay_batch_count=1;
    allocation_failure=true;
    renderer_update_world_batch_runs(&p,0);
    assert(p.gpu_batch_run_ends==NULL && p.gpu_batch_run_count==0);
    assert(new_checks == old_checks);
    printf("Prepared world runs: %zu starts preserve visible materials/order; checks %zu -> %zu\n",
        tested, old_checks, new_checks);
}
'''
with tempfile.TemporaryDirectory(prefix='ge-world-runs-') as temporary:
    directory = Path(temporary)
    (directory / 'test.c').write_text(test)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-O2',
                    '-fsanitize=address,undefined', '-I', str(repo / 'port/include'),
                    str(directory / 'test.c'), '-o', str(directory / 'test')], check=True)
    subprocess.run([str(directory / 'test')], check=True)
