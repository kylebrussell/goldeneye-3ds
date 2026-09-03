#ifndef GE_SCENE_PART_REPLACE_H
#define GE_SCENE_PART_REPLACE_H

#include "ge_dam_dynamic_scene.h"

typedef struct GeScenePartRange {
    size_t entry_index;
    size_t part_index;
    const void *node;
    size_t vertex_offset;
    size_t vertex_count;
    size_t batch_offset;
    size_t batch_count;
} GeScenePartRange;

typedef struct GeSceneOverlaySpan {
    size_t vertex_offset, vertex_count;
    size_t batch_offset, batch_count;
} GeSceneOverlaySpan;

/* Replace one entry in an ordered ordinary-prop prefix followed by retained
 * segments (doors/guards). Replacement parts/geometry are entry-local. The
 * malloc-owned range array, trailing segment offsets and scene commit together;
 * failure preserves all three. No original object/model state is touched.
 * changed_suffix describes the overlay-local range needing GPU republication. */
GeDamDynamicSceneStatus ge_scene_part_replace(
    GeDamDynamicScene *scene, GeScenePartRange **parts, size_t *part_count,
    GeSceneOverlaySpan *tails, size_t tail_count, size_t entry_index,
    const GeScenePartRange *replacement, size_t replacement_count,
    const GeDamRoomWorldVertex *vertices, size_t vertex_count,
    const GeDamRoomDrawBatch *batches, size_t batch_count,
    GeSceneOverlaySpan *changed_suffix);

#endif
