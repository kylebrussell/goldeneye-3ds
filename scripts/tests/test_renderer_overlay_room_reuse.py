#!/usr/bin/env python3
"""Run the actual GPU upload adapter with host buffers; no GPU/FPS claim."""
from pathlib import Path
import re
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
source = (repo / "platform/3ds/source/main.c").read_text()


def function(name):
    match = re.search(rf"static [\w *]+\b{name}\s*\([^;]*?\)\s*\{{", source, re.S)
    assert match, name
    begin = source.index("{", match.start())
    depth = 0
    for index in range(begin, len(source)):
        depth += (source[index] == "{") - (source[index] == "}")
        if depth == 0:
            return source[match.start():index + 1]
    raise AssertionError(name)


publication = function("publish_stage_overlay_with_textures")
assert "scene->scene.batch_count - scene->overlay_batch_count" in publication
assert "{batches, batch_count}" in publication
order = [publication.index(name) for name in (
    "ge_3ds_scene_textures_reconcile_prepare_ranges(",
    "prepare_stage_guard_texture_residency(",
    "ge_3ds_scene_textures_reconcile_commit_after(",
    "memcpy(dam_scene_texture_slots", "candidate->slots = NULL;",
    "objects->preview->source_vertices = scene->vertices;")]
assert order == sorted(order)
install = function("install_stage_ordinary_object_scenes")
assert "ge_dam_dynamic_scene_set_overlay(" not in install
assert install.count("publish_stage_overlay_with_textures(") == 2
assert "objects->guard_scene_cache.publication_ready = 0U;" in install
assert install.index("objects->guard_scene = candidate_guard_scene;") > install.rindex(
    "publish_stage_overlay_with_textures(")
for name in ("refresh_stage_ordinary_object_scenes", "refresh_stage_live_overlays"):
    body = function(name)
    assert body.index("dam_gpu_room_prefix_is_current(") < body.index(
        "install_stage_ordinary_object_scenes(")
    assert "upload_dam_gpu_scene_after_overlay(" in body

upload = function("upload_dam_gpu_world_scene_range")
prepare_uv = upload.index("ge_3ds_scene_texture_uv_prepare(")
assert upload.index("if (!map_texture_uv) continue;") < prepare_uv
assert prepare_uv < upload.index("for (vertex_index = batch->first_vertex;", prepare_uv)
assert upload.index("ge_3ds_scene_texture_map_uv_prepared(") > prepare_uv

vertex = re.search(r"typedef struct Vertex\s*\{.*?\} Vertex;", source, re.S).group(0)
colors = re.search(r"static const float renderer_normalized_color\[.*?\};", source, re.S).group(0)
test = r'''
#include "ge_3ds_scene_texture.h"
#include "ge_dam_dynamic_scene.h"
#include "ge_draw_batch_visibility.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define DAM_ROOM_VERTEX_COUNT 600U
#define GE_3DS_EXPERIMENT_COLOR_LOOKUP 0
''' + vertex + "\n" + colors + r'''
typedef struct RuntimeDamPreview {
    bool gpu_world_ready;
    GeDamRoomWorldVertex *source_vertices;
    GeDamRoomDrawBatch *batches;
    size_t source_vertex_count, batch_count, vertex_count;
    GeDamDynamicScene dynamic_scene;
    Ge3dsSceneTextures *scene_textures;
    GeDrawBatchWorldBounds *gpu_batch_bounds;
    size_t gpu_batch_bounds_capacity;
    size_t gpu_dirty_vertex_offset, gpu_dirty_vertex_count;
    size_t gpu_uploaded_vertex_count;
    uint64_t gpu_uploaded_scene_generation;
    uint64_t gpu_overlay_only_uploads, gpu_room_vertices_reused;
} RuntimeDamPreview;
static size_t writes;
static struct { uint64_t texture_uv_work[4]; } fine_profile;
const Ge3dsSceneTextureSlot *ge_3ds_scene_textures_find(
    const Ge3dsSceneTextures *scene, uint16_t id)
{
    for (size_t i=0;i<scene->texture_count;++i)
        if (scene->slots[i].image_id==id && scene->slots[i].loaded)
            return &scene->slots[i];
    return NULL;
}
/* A deterministic map seam; the same map is used by full and partial paths. */
GeTextureUvStatus ge_3ds_scene_texture_uv_prepare(const Ge3dsSceneTextureSlot *slot,
    const GePicaMaterial *material, Ge3dsSceneTextureUvContext *context)
{
    (void)material;
    if (slot==NULL) return GE_TEXTURE_UV_INVALID_ARGUMENT;
    context->normalization.width=(float)slot->width;
    context->normalization.height=(float)slot->height;
    return GE_TEXTURE_UV_OK;
}
GeTextureUvStatus ge_3ds_scene_texture_map_uv_prepared(
    const Ge3dsSceneTextureUvContext *context, int16_t s, int16_t t, GeTextureUv *uv)
{
    uv->u=(float)s / context->normalization.width;
    uv->v=(float)t / context->normalization.height;
    return GE_TEXTURE_UV_OK;
}
''' + function("renderer_upload_world_vertices").replace(
    "renderer_upload_world_vertices(", "upload_vertices_impl(", 1) + r'''
static void renderer_upload_world_vertices(const GeDamRoomWorldVertex *source,
    Vertex *destination, size_t count, bool map)
{ writes+=count; upload_vertices_impl(source,destination,count,map); }
''' + "\n".join(function(name) for name in (
    "upload_dam_gpu_world_scene_range", "upload_dam_gpu_world_scene",
    "dam_gpu_room_prefix_is_current", "upload_dam_gpu_scene_after_overlay")) + r'''
int main(void)
{
    GeDamRoomWorldVertex vertices[600]={0};
    GeDamRoomDrawBatch batches[50]={0};
    Vertex full[600]={0}, actual[600]={0};
    Ge3dsSceneTextureSlot slots[2]={{.image_id=1,.loaded=1,.width=32,.height=16},
        {.image_id=2,.loaded=1,.width=64,.height=32}};
    Ge3dsSceneTextures textures={.slots=slots,.capacity=2,.texture_count=2};
    RuntimeDamPreview a={.gpu_world_ready=true,.source_vertices=vertices,.batches=batches,
        .source_vertex_count=516,.batch_count=19,.scene_textures=&textures};
    a.dynamic_scene.generation=1;
    a.dynamic_scene.overlay_vertex_count=6;
    a.dynamic_scene.overlay_batch_count=2;
    RuntimeDamPreview b=a;
    for(size_t i=0;i<600;++i) {
        vertices[i].world[0]=(float)i;
        vertices[i].world[1]=-(float)i;
        vertices[i].world[2]=(float)(i%31);
        vertices[i].source.texture_s=(int16_t)i;
        vertices[i].source.texture_t=(int16_t)(-2*(int)i);
        for(size_t c=0;c<4;++c) vertices[i].processed.rgba[c]=(uint8_t)(i+c*33);
    }
    for(size_t i=0;i<50;++i) {
        batches[i].first_vertex=i<17?i*30:510+(i-17)*3;
        batches[i].vertex_count=i<17?30:3;
        batches[i].texture.texture_id=(uint16_t)(1+i%2);
    }
    assert(upload_dam_gpu_world_scene(&a,full));
    assert(upload_dam_gpu_world_scene(&b,actual));
    size_t full_writes=0,partial_writes=0;
    for(size_t step=0;step<240;++step) {
        /* Growth/shrink/empty, new materials, moving/recolored vertices. */
        textures.missing_count=step%13==0;
        if(textures.missing_count) slots[1].loaded=0;
        bool retain=dam_gpu_room_prefix_is_current(&b);
        if(step%17==0) { b.gpu_uploaded_scene_generation=0; retain=dam_gpu_room_prefix_is_current(&b); }
        /* A formerly missing room image becomes available during the
         * transaction with a different UV scale: the fallback must remap it. */
        if(textures.missing_count) {
            slots[1].loaded=1; slots[1].width=32U+(uint32_t)(step%4)*32U;
            textures.missing_count=0;
        }
        size_t tail=(step%11)*3;
        a.dynamic_scene.overlay_vertex_count=b.dynamic_scene.overlay_vertex_count=tail;
        a.dynamic_scene.overlay_batch_count=b.dynamic_scene.overlay_batch_count=tail/3;
        a.source_vertex_count=b.source_vertex_count=510+tail;
        a.batch_count=b.batch_count=17+tail/3;
        a.dynamic_scene.generation=b.dynamic_scene.generation=step+2;
        a.gpu_dirty_vertex_count=b.gpu_dirty_vertex_count=0;
        for(size_t i=510;i<510+tail;++i) {
            vertices[i].world[1]=(float)(step+i);
            vertices[i].source.texture_t=(int16_t)step;
            vertices[i].processed.rgba[0]=(uint8_t)step;
        }
        for(size_t i=17;i<a.batch_count;++i)
            batches[i].texture.texture_id=(uint16_t)(1+(step+i)%2);
        writes=0; assert(upload_dam_gpu_world_scene(&a,full)); full_writes+=writes;
        writes=0; assert(upload_dam_gpu_scene_after_overlay(&b,actual,retain)); partial_writes+=writes;
        assert(writes==(retain?tail:510+tail));
        assert(memcmp(full,actual,sizeof(full))==0);
        assert(a.vertex_count==b.vertex_count && a.gpu_uploaded_scene_generation==b.gpu_uploaded_scene_generation);
        for(size_t i=0;i<a.batch_count;++i) {
            assert(a.gpu_batch_bounds[i].valid==b.gpu_batch_bounds[i].valid);
            if(a.gpu_batch_bounds[i].valid) assert(memcmp(&a.gpu_batch_bounds[i],&b.gpu_batch_bounds[i],sizeof(*a.gpu_batch_bounds))==0);
        }
        assert(b.gpu_dirty_vertex_count==(retain?tail:510+tail));
        if(tail && retain) assert(b.gpu_dirty_vertex_offset==510);
    }
    assert(partial_writes<full_writes && b.gpu_room_vertices_reused>0);
    assert(!dam_gpu_room_prefix_is_current(NULL));
    b.gpu_world_ready=false;
    assert(!dam_gpu_room_prefix_is_current(&b));
    b.gpu_world_ready=true;
    size_t count=b.source_vertex_count;
    b.source_vertex_count=601;
    assert(!upload_dam_gpu_scene_after_overlay(&b,actual,true));
    b.source_vertex_count=count;
    assert(!upload_dam_gpu_scene_after_overlay(&b,NULL,true));
    /* Mode 2 is only entered for a model-cache pose-only publication. UVs
     * have already been mapped; shade bytes remain the immutable template. */
    for(size_t step=0;step<256;++step) {
        const size_t start=510, count=3;
        b.gpu_dirty_vertex_count=0;
        for(size_t i=start;i<start+count;++i) {
            vertices[i].world[0]=(float)step-(float)i;
            vertices[i].world[1]=-(float)step;
            vertices[i].world[2]=(float)step/7.0f;
        }
        assert(upload_dam_gpu_world_scene_range(&a,full,start,count,17,1,0U));
        assert(upload_dam_gpu_world_scene_range(&b,actual,start,count,17,1,2U));
        assert(memcmp(full,actual,sizeof(full))==0);
        assert(b.gpu_dirty_vertex_offset==start && b.gpu_dirty_vertex_count==count);
    }
    assert(!upload_dam_gpu_world_scene_range(&b,actual,0,0,0,0,3U));
    free(a.gpu_batch_bounds);free(b.gpu_batch_bounds);
    printf("240 actual adapter overlay transitions: %zu full vs %zu partial vertex writes; exact full buffers, room bounds, UVs, colors and empty/shrink/growth/fallback counts\n",full_writes,partial_writes);
}
'''
with tempfile.TemporaryDirectory(prefix="ge-overlay-room-upload-") as temporary:
    directory = Path(temporary)
    (directory / "test.c").write_text(test)
    subprocess.run(["cc", "-std=c11", "-O2", "-Wall", "-Wextra", "-Werror",
                    "-Wno-unused-const-variable", "-fsanitize=address,undefined",
                    "-fno-omit-frame-pointer", "-I", str(repo / "port/include"),
                    "-I", str(repo / "platform/3ds/include"), "-I",
                    str(repo / "platform/3ds/tests/include"), str(directory / "test.c"),
                    str(repo / "port/src/ge_draw_batch_visibility.c"), "-lm",
                    "-o", str(directory / "test")], check=True)
    subprocess.run([str(directory / "test")], check=True)
