#!/usr/bin/env python3
"""Exercise actual renderer room-proof/fallback/caching wiring under ASan."""
from pathlib import Path
import re
import subprocess
import sys
import tempfile

repo = Path(__file__).resolve().parents[2]
source = (repo / "platform/3ds/source/main.c").read_text()


def function(name):
    match = re.search(rf"static\s+[\w\s*]+\b{name}\s*\([^;]*?\)\s*\{{", source)
    assert match, name
    depth = 0
    for index in range(source.index("{", match.start()), len(source)):
        depth += (source[index] == "{") - (source[index] == "}")
        if depth == 0:
            return source[match.start():index + 1]
    raise AssertionError(name)


room_type = re.search(r"typedef struct RuntimeRendererRoomVisibility\s*\{.*?\} RuntimeRendererRoomVisibility;", source, re.S).group(0)
draw = function("renderer_draw")
assert draw.index("renderer_prepare_room_visibility(") < draw.index("renderer_prepare_room_frustum(") < draw.index("for (i = 0U; i < draw_batch_count;)")
assert draw.count("clip_contexts, room_frustum)") == 2
assert "if (renderer_prepare_room_frustum(" in draw
assert "draw_profile_room_frustum=%llu,%llu,%llu" in source

test = r'''
#include "ge_dam_dynamic_scene.h"
#include "ge_original_bg_visibility.h"
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
typedef struct RuntimeDamPreview {
    GeDamRoomWorldVertex *source_vertices;
    GeDamRoomDrawBatch *batches;
    size_t source_vertex_count, batch_count;
    GeDamDynamicScene dynamic_scene;
    uint8_t original_camera_room;
    GeDrawBatchWorldBounds *gpu_batch_bounds;
    size_t gpu_batch_bounds_capacity;
} RuntimeDamPreview;
static struct {
    uint64_t world_room_frustum_tests, world_room_frustum_visible_batches,
        world_room_frustum_culled_batches, world_frustum_tests,
        world_frustum_first_vertex_visible, world_frustum_bounds_inside,
        world_frustum_bounds_outside, world_frustum_culled_batches,
        world_frustum_culled_vertices;
} fine_profile;
''' + room_type + "\n" + "\n".join(function(name) for name in (
    "renderer_prepare_room_visibility", "renderer_room_visible",
    "renderer_prepare_room_frustum", "renderer_world_batch_may_draw")) + r'''
static void check_batches(RuntimeDamPreview *preview,
    const GeDrawBatchClipContext contexts[3], const uint8_t classes[256])
{
    uint8_t cache[8] = {0};
    for(size_t i=0;i<8;++i) {
        const GeDamRoomDrawBatch *b=&preview->batches[i];
        GeDrawBatchVisibility ref=ge_draw_batch_world_visibility_prepared(
            preview->source_vertices,24,b,NULL,&contexts[b->coordinate_space]);
        bool actual=renderer_world_batch_may_draw(preview,i,cache,8,contexts,classes);
        assert(actual==(ref!=GE_DRAW_BATCH_BOUNDS_CULLED && ref!=GE_DRAW_BATCH_VERTICES_CULLED));
        uint64_t calls=fine_profile.world_frustum_tests;
        assert(renderer_world_batch_may_draw(preview,i,cache,8,contexts,classes)==actual);
        assert(fine_profile.world_frustum_tests==calls);
    }
    assert(renderer_world_batch_may_draw(preview,8,cache,8,contexts,classes));
}
int main(void)
{
    GeDamRoomWorldVertex vertices[24]={0};
    GeDamRoomDrawBatch batches[8]={0};
    GeDamDynamicRoomRange ranges[3]={0};
    RuntimeDamPreview preview={.source_vertices=vertices,.batches=batches,
        .source_vertex_count=24,.batch_count=8,.original_camera_room=1};
    GeDamDynamicScene *scene=&preview.dynamic_scene;
    scene->initialized=1; scene->vertices=vertices; scene->batches=batches;
    scene->scene.vertex_count=24; scene->scene.batch_count=8;
    scene->overlay_vertex_count=9; scene->overlay_batch_count=3;
    scene->room_ranges=ranges; scene->room_count=3;
    for(size_t i=0;i<8;++i) {
        batches[i].first_vertex=i*3; batches[i].vertex_count=3;
        batches[i].room_id=i==0?1:i<3?2:i<5?3:2;
        if(i>=5) batches[i].coordinate_space=(uint8_t)(i-5);
        for(size_t v=i*3;v<i*3+3;++v) vertices[v].world[0]=i<3?2.0f:0.0f;
    }
    for(size_t r=0;r<3;++r) {
        scene->room_ids[r]=(uint8_t)(r+1);
        ranges[r].first_vertex=r==0?0:r==1?3:9;
        ranges[r].first_batch=r==0?0:r==1?1:3;
        ranges[r].scene.vertex_count=r==0?3:6;
        ranges[r].scene.batch_count=r==0?1:2;
        GeDamRoomDrawBatch span={.first_vertex=ranges[r].first_vertex,
            .vertex_count=ranges[r].scene.vertex_count};
        assert(ge_draw_batch_world_bounds_build(vertices,24,&span,&ranges[r].world_bounds));
    }
    float matrix[4][4]={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}};
    GeDrawBatchClipContext contexts[3];
    for(size_t c=0;c<3;++c)ge_draw_batch_clip_context_init(&contexts[c],matrix);
    GeOriginalBgVisibilityResult visible={0};
    visible.room_count=3;
    for(size_t r=0;r<3;++r)visible.rooms[r].room=(uint8_t)(r+1);
    RuntimeRendererRoomVisibility membership;
    renderer_prepare_room_visibility(&membership,&visible,true);
    uint8_t classes[256];
    assert(renderer_prepare_room_frustum(&preview,&membership,&contexts[0],classes));
    assert(classes[1]==GE_DRAW_BATCH_BOUNDS_UNCERTAIN);
    assert(classes[2]==GE_DRAW_BATCH_BOUNDS_OUTSIDE);
    assert(classes[3]==GE_DRAW_BATCH_BOUNDS_INSIDE);
    check_batches(&preview,contexts,classes);
    assert(fine_profile.world_room_frustum_tests==2);
    assert(fine_profile.world_room_frustum_visible_batches==2);
    assert(fine_profile.world_room_frustum_culled_batches==2);
    /* No proof means the draw loop passes NULL and retains the old batch
     * path without its per-batch room lookup (common in single-room views). */
    membership.rooms[2]=membership.rooms[3]=0;
    assert(!renderer_prepare_room_frustum(&preview,&membership,&contexts[0],classes));
    for(size_t i=0;i<256;++i)assert(classes[i]==GE_DRAW_BATCH_BOUNDS_UNCERTAIN);
    check_batches(&preview,contexts,NULL);
    membership.rooms[2]=membership.rooms[3]=1;
    assert(renderer_prepare_room_frustum(&preview,&membership,&contexts[0],classes));
    /* Dynamic overlays with the outside room's ID still use all three
     * original coordinate projections, including an authored-space overlay. */
    for(size_t i=5;i<8;++i)assert(renderer_world_batch_may_draw(&preview,i,NULL,0,contexts,classes));
    membership.rooms[2]=0;
    renderer_prepare_room_frustum(&preview,&membership,&contexts[0],classes);
    assert(classes[2]==GE_DRAW_BATCH_BOUNDS_UNCERTAIN);
    check_batches(&preview,contexts,classes);
    membership.rooms[2]=1;
    ranges[2].world_bounds.valid=0;
    renderer_prepare_room_frustum(&preview,&membership,&contexts[0],classes);
    assert(classes[3]==GE_DRAW_BATCH_BOUNDS_UNCERTAIN);
    check_batches(&preview,contexts,classes);
    ranges[2].world_bounds.valid=1;
    ranges[1].first_vertex=SIZE_MAX;
    renderer_prepare_room_frustum(&preview,&membership,&contexts[0],classes);
    assert(classes[2]==GE_DRAW_BATCH_BOUNDS_UNCERTAIN);
    ranges[1].first_vertex=3;
    preview.source_vertex_count=23;
    renderer_prepare_room_frustum(&preview,&membership,&contexts[0],classes);
    for(size_t i=0;i<256;++i)assert(classes[i]==GE_DRAW_BATCH_BOUNDS_UNCERTAIN);
    preview.source_vertex_count=24;
    /* Camera changes must replace, not retain, last frame's room proof. */
    matrix[3][0]=3.0f;
    for(size_t c=0;c<3;++c)ge_draw_batch_clip_context_init(&contexts[c],matrix);
    renderer_prepare_room_frustum(&preview,&membership,&contexts[0],classes);
    assert(classes[3]==GE_DRAW_BATCH_BOUNDS_OUTSIDE);
    check_batches(&preview,contexts,classes);
    matrix[0][0]=NAN;
    for(size_t c=0;c<3;++c)ge_draw_batch_clip_context_init(&contexts[c],matrix);
    assert(!renderer_prepare_room_frustum(&preview,&membership,&contexts[0],classes));
    for(size_t i=0;i<256;++i)assert(classes[i]==GE_DRAW_BATCH_BOUNDS_UNCERTAIN);
    check_batches(&preview,contexts,classes);
    puts("Actual renderer room proofs: portal membership, current room, dynamic overlays/all coordinate spaces, cache, invalid bounds/ranges, stale publication and changed/nonfinite camera passed");
}
'''
with tempfile.TemporaryDirectory(prefix="ge-room-frustum-") as temporary:
    directory = Path(temporary)
    (directory / "test.c").write_text(test)
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                    "-I", str(repo / "port/include"), str(directory / "test.c"),
                    str(repo / "port/src/ge_draw_batch_visibility.c"), "-lm",
                    "-o", str(directory / "test")], check=True)
    subprocess.run([str(directory / "test")], check=True)
if len(sys.argv) == 2:
    # Reuse the exact extracted adapter in private actual-pack comparisons.
    Path(sys.argv[1]).write_text(test.split("static void check_batches(", 1)[0])
