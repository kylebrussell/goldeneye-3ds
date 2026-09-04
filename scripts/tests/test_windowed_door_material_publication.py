#!/usr/bin/env python3
"""Exercise the actual live door material publisher with rebased host buffers."""
from pathlib import Path
import re
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
main = (repo / "platform/3ds/source/main.c").read_text()
adapter = (repo / "port/src/ge_original_stage_model_publication.c").read_text()


def function(source, name):
    match = re.search(rf"(?:static )?(?:bool|int) {name}\([^;]*?\)\s*\{{", source, re.S)
    assert match, name
    begin = source.index("{", match.start())
    depth = 0
    for end in range(begin, len(source)):
        depth += (source[end] == "{") - (source[end] == "}")
        if depth == 0:
            return source[match.start():end + 1]
    raise AssertionError(name)


publisher = function(main, "refresh_stage_windowed_door_materials")
assert "segment->batch_offset + local" in publisher
assert "malloc(" not in publisher and "ge_original_model_scene" not in publisher
assert main.count("ge_original_stage_model_publication_glass_template(") == 2
assert "range->batch_offset = batch_count - door_batch_offset;" in main
assert "entry->definition, &storage.batches[batch].material" in main
assert main.count("free(candidate_door_scene_parts);") == 2
assert main.count("free(objects->door_scene_parts);") == 3

test = r'''
#include "ge_scene_part_replace.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
/* Entry lookup and native opacity are independently tested by the original
 * type/GBI suite. Keep this test focused on actual publication ownership. */
typedef struct { unsigned opacity; bool windowed; } Definition;
typedef struct { void *definition; bool constructed; } GeOriginalStageInteractiveEntry;
typedef struct { GeOriginalStageInteractiveEntry *entries; size_t count; } Interactive;
typedef struct { GeDamDynamicScene dynamic_scene; } RuntimeDamPreview;
typedef struct { GeDamRoomDrawBatch *batches; size_t batch_offset, batch_count; } RuntimeDamOverlaySegment;
typedef GeScenePartRange RuntimeStageScenePartRange;
typedef struct {
    RuntimeDamPreview *preview;
    RuntimeDamOverlaySegment door_overlay;
    RuntimeStageScenePartRange *door_scene_parts;
    size_t door_scene_part_count;
    Interactive interactive;
} RuntimeStageOrdinaryObjects;
static const GeOriginalStageInteractiveEntry *ge_original_stage_interactive_entry(
    const Interactive *interactive, size_t index)
{ return index < interactive->count ? &interactive->entries[index] : NULL; }
static int ge_original_stage_model_publication_glass_opacity(const void *ptr, uint8_t *opacity)
{
    const Definition *definition=ptr;
    if (!definition->windowed) return 0;
    *opacity=(uint8_t)definition->opacity;
    return 1;
}
''' + function(adapter, "ge_original_stage_model_publication_glass_alpha") + "\n" + publisher + r'''
int main(void)
{
    GeDamRoomDrawBatch local[4]={0}, overlay[12]={0}, combined[16]={0};
    Definition definitions[2]={{0,true},{0,false}};
    GeOriginalStageInteractiveEntry entries[2]={{&definitions[0],true},{&definitions[1],true}};
    RuntimeStageScenePartRange ranges[2]={{.entry_index=0,.batch_count=3},
        {.entry_index=1,.batch_offset=3,.batch_count=1}};
    RuntimeDamPreview preview={0};
    preview.dynamic_scene.overlay_batches=overlay;
    preview.dynamic_scene.batches=combined;
    preview.dynamic_scene.overlay_batch_count=12;
    preview.dynamic_scene.scene.batch_count=16;
    RuntimeStageOrdinaryObjects objects={.preview=&preview,
        .door_overlay={local,2,4},.door_scene_parts=ranges,.door_scene_part_count=2,
        .interactive={entries,2}};
    local[0].material.alpha_combine=GE_PICA_ALPHA_TEXTURE0; /* opaque frame */
    for(size_t i=1;i<4;++i) local[i].material.alpha_combine=
        GE_PICA_ALPHA_TEXTURE0_MODULATE_SHADE_ADD_PRIMITIVE;
    for(unsigned frame=0;frame<512;++frame) {
        /* Ordinary props grow/shrink before the door segment; stored ranges
         * remain segment-local. All unrelated bytes must stay untouched. */
        objects.door_overlay.batch_offset=frame%6;
        definitions[0].opacity=frame%256;
        for(size_t i=0;i<12;++i) overlay[i].material.primitive_color.alpha=71;
        for(size_t i=0;i<16;++i) combined[i].material.primitive_color.alpha=71;
        for(size_t i=0;i<4;++i) local[i].material.primitive_color.alpha=71;
        assert(refresh_stage_windowed_door_materials(&objects));
        for(size_t i=0;i<12;++i) {
            bool changed=i==objects.door_overlay.batch_offset+1
                || i==objects.door_overlay.batch_offset+2;
            assert(overlay[i].material.primitive_color.alpha==(changed?frame%256:71));
            assert(combined[4+i].material.primitive_color.alpha==(changed?frame%256:71));
        }
        for(size_t i=0;i<4;++i) assert(combined[i].material.primitive_color.alpha==71);
        assert(local[0].material.primitive_color.alpha==71 && local[3].material.primitive_color.alpha==71);
    }
    ranges[0].batch_offset=5;
    assert(!refresh_stage_windowed_door_materials(&objects));
    ranges[0].batch_offset=0; objects.door_overlay.batch_offset=12;
    assert(!refresh_stage_windowed_door_materials(&objects));
    objects.door_overlay.batch_offset=0; ranges[0].entry_index=2;
    assert(!refresh_stage_windowed_door_materials(&objects));
    puts("windowed doors: 512 opacity/rebase publications, opaque-frame isolation and invalid-range checks passed");
}
'''
with tempfile.TemporaryDirectory(prefix="ge-window-door-material-") as directory:
    path = Path(directory)
    (path / "test.c").write_text(test)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic",
                    "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                    "-I", str(repo / "port/include"), str(path / "test.c"),
                    "-o", str(path / "test")], check=True)
    subprocess.run([str(path / "test")], check=True)
