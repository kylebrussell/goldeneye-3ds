#!/usr/bin/env python3
"""Execute the live renderer's door-index/publication loop under sanitizers.

Original matrices/LookAt/GBI shading are covered by test_stage_model_publication.
This isolates buffer ownership, rebase, visibility and GPU publication with
explicit snapshot/shading/upload seams, not replacement gameplay.
"""
from pathlib import Path
import re
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
main = (repo / "platform/3ds/source/main.c").read_text()

match = re.search(r"static bool refresh_stage_windowed_door_shading\([^;]*?\)\s*\{", main, re.S)
assert match
depth = 0
for end in range(main.index("{", match.start()), len(main)):
    depth += (main[end] == "{") - (main[end] == "}")
    if depth == 0:
        publisher = main[match.start():end + 1]
        break
assert "malloc(" not in publisher and "ge_original_model_scene" not in publisher
assert "candidate_door_matrix_indices + vertex_count - door_vertex_offset" in main
assert "view.matrix_indices, range->vertex_count" in main
assert main.count("free(objects->door_scene_matrix_indices);") == 3
assert main.count("free(candidate_door_matrix_indices);") == 2
assert "refresh_stage_windowed_door_shading(objects, gpu_destination)" in main

test = r'''
#include "ge_scene_part_replace.h"
#include "ge_3ds_shade_cache.h"
#include "ge_original_door_runtime.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
typedef struct { bool windowed; } Definition;
typedef struct { void *definition; bool constructed; int room; } GeOriginalStageInteractiveEntry;
typedef struct { GeOriginalStageInteractiveEntry *entries; size_t count; } Interactive;
typedef struct {
    GeDamDynamicScene dynamic_scene;
    float original_camera_view[4][4]; uint8_t original_camera_look_at[32]; bool visible;
} RuntimeDamPreview;
typedef struct {
    GeDamRoomWorldVertex *vertices; GeDamRoomDrawBatch *batches;
    size_t vertex_offset, vertex_count, batch_offset, batch_count;
} RuntimeDamOverlaySegment;
typedef GeScenePartRange RuntimeStageScenePartRange;
typedef struct {
    RuntimeDamPreview *preview; RuntimeDamOverlaySegment door_overlay;
    RuntimeStageScenePartRange *door_scene_parts; size_t door_scene_part_count;
    uint16_t *door_scene_matrix_indices; size_t door_scene_matrix_index_count;
    Interactive interactive;
} RuntimeStageOrdinaryObjects;
typedef int Vertex;
static struct { uint64_t glass_shading_work[3]; } fine_profile;
static unsigned snapshots, uploads;
static size_t upload_vertex, upload_batch;
static const GeOriginalStageInteractiveEntry *ge_original_stage_interactive_entry(
    const Interactive *interactive, size_t index)
{ return index < interactive->count ? &interactive->entries[index] : NULL; }
static int ge_original_stage_model_publication_glass_opacity(const void *ptr, uint8_t *opacity)
{ *opacity=0; return ((const Definition *)ptr)->windowed; }
static bool dam_visibility_contains_room(const RuntimeDamPreview *preview, int room)
{ return preview->visible && room==7; }
int ge_original_door_runtime_snapshot(const void *definition, GeOriginalDoorRuntimePublication *publication)
{ (void)definition; ++snapshots; memset(publication,0,sizeof(*publication)); publication->matrix_count=3; return 1; }
static int ge_original_stage_model_publication_door_glass_shading(
    const void *definition, const GeOriginalDoorRuntimePublication *publication,
    size_t matrix, const float view[4][4], const uint8_t look_at[32],
    const GePicaMaterial *material, GeGbiRenderState *state)
{
    (void)definition; (void)look_at; (void)material;
    if(matrix>=publication->matrix_count) return 0;
    memset(state,0,sizeof(*state));
    state->modelview_stack.entries[0].elements[0][0]=(float)matrix+view[0][0];
    return 1;
}
/* Visible tags expose wrong indices. Original shade math is tested separately. */
GeGbiVertexProcessStatus ge_gbi_vertex_shade(const GeGbiRenderState *state,
    const GeGbiVertex *source, GeGbiProcessedVertex *processed)
{
    (void)source; processed->texture_generated=1;
    processed->texture[0]=state->modelview_stack.entries[0].elements[0][0];
    return GE_GBI_VERTEX_PROCESS_OK;
}
static bool upload_dam_gpu_world_scene_range(RuntimeDamPreview *preview, Vertex *gpu,
    size_t first, size_t count, size_t batch, size_t batches, unsigned mode)
{
    (void)preview; (void)gpu; assert(count==6 && batches==1 && mode==3);
    ++uploads; upload_vertex=first; upload_batch=batch; return true;
}
''' + publisher + r'''
int main(void)
{
    GeDamRoomWorldVertex local[9]={0}, overlay[24]={0}, combined[28]={0};
    GeDamRoomDrawBatch batches[2]={0};
    uint16_t indices[9]={2,1,0,0,1,2,0,1,2};
    Definition definition={true};
    GeOriginalStageInteractiveEntry entry={&definition,true,7};
    RuntimeStageScenePartRange range={.entry_index=0,.vertex_offset=3,.vertex_count=6,
        .batch_offset=1,.batch_count=1};
    RuntimeDamPreview preview={.visible=true};
    preview.dynamic_scene.overlay_vertices=overlay; preview.dynamic_scene.vertices=combined;
    preview.dynamic_scene.overlay_vertex_count=24; preview.dynamic_scene.scene.vertex_count=28;
    preview.dynamic_scene.overlay_batch_count=12; preview.dynamic_scene.scene.batch_count=16;
    RuntimeStageOrdinaryObjects objects={.preview=&preview,
        .door_overlay={local,batches,2,9,1,2},.door_scene_parts=&range,.door_scene_part_count=1,
        .door_scene_matrix_indices=indices,.door_scene_matrix_index_count=9,.interactive={&entry,1}};
    batches[1].first_vertex=3; batches[1].vertex_count=6;
    batches[1].material.lighting_enabled=1; batches[1].material.texture_gen_enabled=1;
    for(unsigned frame=0;frame<256;++frame) {
        objects.door_overlay.vertex_offset=frame%8; objects.door_overlay.batch_offset=frame%6;
        preview.original_camera_view[0][0]=(float)frame+1;
        memset(local,0,sizeof(local)); memset(overlay,0,sizeof(overlay)); memset(combined,0,sizeof(combined));
        for(size_t i=0;i<24;++i) overlay[i].world[0]=42;
        unsigned before=uploads;
        assert(refresh_stage_windowed_door_shading(&objects,NULL));
        assert(uploads==before+1);
        assert(upload_vertex==4+objects.door_overlay.vertex_offset+3);
        assert(upload_batch==4+objects.door_overlay.batch_offset+1);
        for(size_t i=0;i<24;++i) {
            size_t first=objects.door_overlay.vertex_offset+3;
            bool changed=i>=first && i<first+6;
            float expected=changed?(float)indices[i-objects.door_overlay.vertex_offset]+frame+1:0;
            assert(overlay[i].processed.texture[0]==expected && combined[4+i].processed.texture[0]==expected);
            assert(overlay[i].world[0]==42);
        }
        for(size_t i=0;i<9;++i) assert(local[i].processed.texture[0]==(i<3?0:(float)indices[i]+frame+1));
        assert(refresh_stage_windowed_door_shading(&objects,NULL));
        assert(uploads==before+1);
    }
    unsigned before=snapshots; preview.visible=false;
    assert(refresh_stage_windowed_door_shading(&objects,NULL) && snapshots==before);
    preview.visible=true; definition.windowed=false;
    assert(refresh_stage_windowed_door_shading(&objects,NULL) && snapshots==before);
    definition.windowed=true; indices[3]=3;
    assert(!refresh_stage_windowed_door_shading(&objects,NULL));
    indices[3]=0; range.vertex_count=5;
    assert(!refresh_stage_windowed_door_shading(&objects,NULL));
    range.vertex_count=6; objects.door_scene_matrix_index_count=8;
    assert(!refresh_stage_windowed_door_shading(&objects,NULL));
    objects.door_scene_matrix_index_count=9; objects.door_overlay.vertex_offset=24;
    assert(!refresh_stage_windowed_door_shading(&objects,NULL));
    puts("door shading: 256 rebases, per-vertex matrices, unchanged uploads, visibility and bounds passed");
}
'''
with tempfile.TemporaryDirectory(prefix="ge-door-shading-") as directory:
    path = Path(directory)
    (path / "test.c").write_text(test)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                    "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                    "-I", str(repo / "port/include"), "-I", str(repo / "platform/3ds/include"), str(path / "test.c"),
                    "-o", str(path / "test")], check=True)
    subprocess.run([str(path / "test")], check=True)
