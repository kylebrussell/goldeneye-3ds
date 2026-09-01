#include "ge_3ds_original_autogun_beam.h"

#include <bondconstants.h>

#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

enum {
    GE_ORIGINAL_AUTOGUN_BEAM_RADIUS = 30
};

static int ge_finite3(const float value[3])
{
    return isfinite(value[0]) && isfinite(value[1]) && isfinite(value[2]);
}

static void ge_beam_vertex(Ge3dsOriginalAutogunBeamVertex *vertex,
                           const float position[3], const float uv[2])
{
    vertex->x = position[0];
    vertex->y = position[1];
    vertex->z = position[2];
    vertex->u = uv[0];
    vertex->v = uv[1];
    vertex->red = 1.0f;
    vertex->green = 1.0f;
    vertex->blue = 1.0f;
    vertex->alpha = 1.0f;
}

static int ge_n64_vertex_component(float value, float *component)
{
    if (!isfinite(value) || value < (float)INT16_MIN
            || value > (float)INT16_MAX || component == NULL) return 0;
    *component = (float)(int16_t)value;
    return 1;
}

static int ge_emit_fnp90_beam(
    const GeOriginalStageAutogunBeamSnapshot *beam,
    const float viewer[3],
    const Ge3dsOriginalAutogunBeamTextureUv *texture,
    Ge3dsOriginalAutogunBeamVertex *vertices)
{
    float position[3];
    float endpoint[3];
    float right[3];
    float local[4];
    float points[4][3];
    float start_offset = beam->distance;
    float distance = beam->minimum_distance;
    float length;
    size_t axis;

    if (!ge_finite3(beam->origin) || !ge_finite3(beam->direction)
            || !ge_finite3(viewer) || !isfinite(beam->maximum_distance)
            || !isfinite(beam->minimum_distance)
            || !isfinite(beam->distance)
            || beam->weapon_id != ITEM_FNP90) return 0;

    memcpy(position, beam->origin, sizeof(position));
    if (start_offset > 0.0f) {
        for (axis = 0U; axis < 3U; ++axis)
            position[axis] += start_offset * beam->direction[axis];
    } else {
        distance += start_offset;
        start_offset = 0.0f;
    }
    if (beam->maximum_distance < start_offset + distance)
        distance = beam->maximum_distance - start_offset;
    for (axis = 0U; axis < 3U; ++axis)
        endpoint[axis] = position[axis] + distance * beam->direction[axis];

    /* Exact sub_GAME_7F061E18 billboard: direction cross the vector from
     * beam endpoint to Bond, with the original vertical fallback. */
    right[0] = beam->direction[1] * (viewer[2] - endpoint[2])
        - beam->direction[2] * (viewer[1] - endpoint[1]);
    right[1] = beam->direction[2] * (viewer[0] - endpoint[0])
        - beam->direction[0] * (viewer[2] - endpoint[2]);
    right[2] = beam->direction[0] * (viewer[1] - endpoint[1])
        - beam->direction[1] * (viewer[0] - endpoint[0]);
    length = sqrtf(right[0] * right[0] + right[1] * right[1]
        + right[2] * right[2]);
    if (length != 0.0f) {
        const float scale = (float)GE_ORIGINAL_AUTOGUN_BEAM_RADIUS / length;
        right[0] *= scale;
        right[1] *= scale;
        right[2] *= scale;
    } else {
        right[0] = 0.0f;
        right[1] = (float)GE_ORIGINAL_AUTOGUN_BEAM_RADIUS;
        right[2] = 0.0f;
    }

    /* Preserve the original Vtx s16 assignment before its 0.1 model scale.
     * The far offset is direction*distance*10 in local coordinates and its
     * 0.9 taper is quantized together with that offset. */
    for (axis = 0U; axis < 3U; ++axis) {
        if (!ge_n64_vertex_component(right[axis], &local[0])
                || !ge_n64_vertex_component(-right[axis], &local[1])
                || !ge_n64_vertex_component(
                    beam->direction[axis] * distance * 10.0f
                        + right[axis] * 0.9f, &local[2])
                || !ge_n64_vertex_component(
                    beam->direction[axis] * distance * 10.0f
                        - right[axis] * 0.9f, &local[3])) return 0;
        points[0][axis] = position[axis] + local[0] * 0.1f;
        points[1][axis] = position[axis] + local[1] * 0.1f;
        points[2][axis] = position[axis] + local[2] * 0.1f;
        points[3][axis] = position[axis] + local[3] * 0.1f;
    }
    ge_beam_vertex(&vertices[0], points[0], texture->top_right);
    ge_beam_vertex(&vertices[1], points[2], texture->bottom_right);
    ge_beam_vertex(&vertices[2], points[3], texture->bottom_left);
    ge_beam_vertex(&vertices[3], points[0], texture->top_right);
    ge_beam_vertex(&vertices[4], points[3], texture->bottom_left);
    ge_beam_vertex(&vertices[5], points[1], texture->top_left);
    return 1;
}

int ge_3ds_original_autogun_beams_build_draw_list(
    const GeOriginalStageAutogunBeamSnapshot *snapshots,
    size_t snapshot_count, const float viewer_position[3],
    const Ge3dsOriginalAutogunBeamTextureUv *texture_uv,
    Ge3dsOriginalAutogunBeamDrawList *draw_list)
{
    size_t index;
    if (draw_list == NULL) return 0;
    memset(draw_list, 0, sizeof(*draw_list));
    if ((snapshot_count != 0U && snapshots == NULL)
            || viewer_position == NULL || texture_uv == NULL
            || snapshot_count > GE_3DS_ORIGINAL_AUTOGUN_BEAM_CAPACITY)
        return 0;
    draw_list->source_count = snapshot_count;
    for (index = 0U; index < snapshot_count; ++index) {
        const GeOriginalStageAutogunBeamSnapshot *beam = &snapshots[index];
        Ge3dsOriginalAutogunBeamVertex *destination;
        if (!beam->active || beam->age < 0) continue;
        ++draw_list->active_count;
        destination = &draw_list->vertices[draw_list->vertex_count];
        if (!ge_emit_fnp90_beam(
                beam, viewer_position, texture_uv, destination)) return 0;
        draw_list->vertex_count +=
            GE_3DS_ORIGINAL_AUTOGUN_BEAM_VERTICES;
    }
    return 1;
}
