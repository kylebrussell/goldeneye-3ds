#include "ge_scene_part_replace.h"
#include <stdlib.h>
#include <string.h>

static int append_span(size_t offset, size_t count, size_t *cursor, size_t limit)
{
    if (offset != *cursor || *cursor > limit || count > limit - *cursor)
        return 0;
    *cursor += count;
    return 1;
}

GeDamDynamicSceneStatus ge_scene_part_replace(
    GeDamDynamicScene *scene, GeScenePartRange **parts, size_t *part_count,
    GeSceneOverlaySpan *tails, size_t tail_count, size_t entry_index,
    const GeScenePartRange *replacement, size_t replacement_count,
    const GeDamRoomWorldVertex *vertices, size_t vertex_count,
    const GeDamRoomDrawBatch *batches, size_t batch_count,
    GeSceneOverlaySpan *changed_suffix)
{
    GeScenePartRange *candidate;
    GeDamDynamicSceneStatus status;
    size_t first = 0U, end, new_count, i, destination;
    size_t vc = 0U, bc = 0U, old_v, old_b, removed_v, removed_b;
    size_t new_total_v, new_total_b;
    if (scene == NULL || !scene->initialized || parts == NULL
            || part_count == NULL || changed_suffix == NULL
            || *part_count > SIZE_MAX / sizeof(**parts)
            || (*part_count != 0U && *parts == NULL)
            || (replacement_count != 0U && replacement == NULL)
            || (tail_count != 0U && tails == NULL))
        return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
    /* Validate the complete layout before deriving insertion points. Empty
     * entries are absent, but empty trailing segments retain their position. */
    for (i = 0U; i < *part_count; ++i) {
        const GeScenePartRange *p = &(*parts)[i];
        const size_t next_part = i != 0U
                && (*parts)[i - 1U].entry_index == p->entry_index
            ? (*parts)[i - 1U].part_index + 1U : 0U;
        if ((i != 0U && p->entry_index < (*parts)[i - 1U].entry_index)
                || p->part_index != next_part
                || !append_span(p->vertex_offset, p->vertex_count,
                    &vc, scene->overlay_vertex_count)
                || !append_span(p->batch_offset, p->batch_count,
                    &bc, scene->overlay_batch_count))
            return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
        if (p->entry_index < entry_index) first = i + 1U;
    }
    end = first;
    while (end < *part_count && (*parts)[end].entry_index == entry_index) ++end;
    old_v = first < *part_count ? (*parts)[first].vertex_offset : vc;
    old_b = first < *part_count ? (*parts)[first].batch_offset : bc;
    removed_v = (end < *part_count ? (*parts)[end].vertex_offset : vc) - old_v;
    removed_b = (end < *part_count ? (*parts)[end].batch_offset : bc) - old_b;
    for (i = 0U; i < tail_count; ++i)
        if (!append_span(tails[i].vertex_offset, tails[i].vertex_count,
                &vc, scene->overlay_vertex_count)
                || !append_span(tails[i].batch_offset, tails[i].batch_count,
                    &bc, scene->overlay_batch_count))
            return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
    if (vc != scene->overlay_vertex_count || bc != scene->overlay_batch_count
            || replacement_count > SIZE_MAX - (*part_count - (end - first))
            || vertex_count > SIZE_MAX - (vc - removed_v)
            || batch_count > SIZE_MAX - (bc - removed_b))
        return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
    new_total_v = vc - removed_v + vertex_count;
    new_total_b = bc - removed_b + batch_count;
    vc = bc = 0U;
    for (i = 0U; i < replacement_count; ++i) {
        const GeScenePartRange *p = &replacement[i];
        if (p->entry_index != entry_index || p->part_index != i
                || !append_span(p->vertex_offset, p->vertex_count, &vc, vertex_count)
                || !append_span(p->batch_offset, p->batch_count, &bc, batch_count))
            return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
    }
    if (vc != vertex_count || bc != batch_count)
        return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
    new_count = *part_count - (end - first) + replacement_count;
    if (new_count > SIZE_MAX / sizeof(*candidate))
        return GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
    candidate = new_count != 0U ? malloc(new_count * sizeof(*candidate)) : NULL;
    if (new_count != 0U && candidate == NULL) return GE_DAM_DYNAMIC_SCENE_NO_MEMORY;
    destination = 0U;
    vc = bc = 0U;
    for (i = 0U; i < new_count; ++i) {
        const GeScenePartRange *source = i < first ? &(*parts)[i]
            : i < first + replacement_count ? &replacement[i - first]
            : &(*parts)[end + i - first - replacement_count];
        candidate[destination] = *source;
        candidate[destination].vertex_offset = vc;
        candidate[destination].batch_offset = bc;
        vc += source->vertex_count;
        bc += source->batch_count;
        ++destination;
    }
    status = ge_dam_dynamic_scene_replace_overlay_segment(scene,
        old_v, removed_v, vertices, vertex_count,
        old_b, removed_b, batches, batch_count);
    if (status != GE_DAM_DYNAMIC_SCENE_OK) {
        free(candidate);
        return status;
    }
    /* All remaining arithmetic was bounded before the geometry transaction. */
    for (i = 0U; i < tail_count; ++i) {
        tails[i].vertex_offset = vc;
        tails[i].batch_offset = bc;
        vc += tails[i].vertex_count;
        bc += tails[i].batch_count;
    }
    free(*parts);
    *parts = candidate;
    *part_count = new_count;
    *changed_suffix = (GeSceneOverlaySpan){
        old_v, new_total_v - old_v, old_b, new_total_b - old_b
    };
    return GE_DAM_DYNAMIC_SCENE_OK;
}
