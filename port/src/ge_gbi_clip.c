#include "ge_gbi_clip.h"

#include <math.h>
#include <string.h>

#define GE_GBI_CLIP_MINIMUM_W 0.000001f
#define GE_GBI_CLIP_BOUNDARY_EPSILON 0.000001f
#define GE_GBI_CLIP_CANONICAL_MASK ((uint8_t)(GE_GBI_CLIP_LEFT \
    | GE_GBI_CLIP_RIGHT | GE_GBI_CLIP_BOTTOM | GE_GBI_CLIP_TOP \
    | GE_GBI_CLIP_NEAR | GE_GBI_CLIP_FAR))

typedef enum GeGbiClipPlane {
    GE_GBI_CLIP_PLANE_POSITIVE_W,
    GE_GBI_CLIP_PLANE_LEFT,
    GE_GBI_CLIP_PLANE_RIGHT,
    GE_GBI_CLIP_PLANE_BOTTOM,
    GE_GBI_CLIP_PLANE_TOP,
    GE_GBI_CLIP_PLANE_NEAR,
    GE_GBI_CLIP_PLANE_FAR,
    GE_GBI_CLIP_PLANE_COUNT
} GeGbiClipPlane;

static float ge_gbi_clip_plane_distance(const GeGbiProcessedVertex *vertex,
                                        GeGbiClipPlane plane)
{
    const float x = vertex->clip[0];
    const float y = vertex->clip[1];
    const float z = vertex->clip[2];
    const float w = vertex->clip[3];

    switch (plane) {
    case GE_GBI_CLIP_PLANE_POSITIVE_W:
        return w - GE_GBI_CLIP_MINIMUM_W;
    case GE_GBI_CLIP_PLANE_LEFT:
        return w + x;
    case GE_GBI_CLIP_PLANE_RIGHT:
        return w - x;
    case GE_GBI_CLIP_PLANE_BOTTOM:
        return w + y;
    case GE_GBI_CLIP_PLANE_TOP:
        return w - y;
    case GE_GBI_CLIP_PLANE_NEAR:
        return w + z;
    case GE_GBI_CLIP_PLANE_FAR:
        return w - z;
    case GE_GBI_CLIP_PLANE_COUNT:
        break;
    }
    return -1.0f;
}

static uint8_t ge_gbi_clip_byte(float value)
{
    if (value <= 0.0f) {
        return 0U;
    }
    if (value >= 255.0f) {
        return UINT8_MAX;
    }
    return (uint8_t)(value + 0.5f);
}

static void ge_gbi_clip_interpolate_array(float *output, const float *start,
                                          const float *end, size_t count,
                                          float amount)
{
    size_t index;

    for (index = 0U; index < count; ++index) {
        output[index] = start[index] + (end[index] - start[index]) * amount;
    }
}

static uint8_t ge_gbi_clip_flags(const float clip[4])
{
    uint8_t flags = 0U;
    const float w = clip[3];

    if (clip[0] < -w - GE_GBI_CLIP_BOUNDARY_EPSILON) {
        flags |= GE_GBI_CLIP_LEFT;
    }
    if (clip[0] > w + GE_GBI_CLIP_BOUNDARY_EPSILON) {
        flags |= GE_GBI_CLIP_RIGHT;
    }
    if (clip[1] < -w - GE_GBI_CLIP_BOUNDARY_EPSILON) {
        flags |= GE_GBI_CLIP_BOTTOM;
    }
    if (clip[1] > w + GE_GBI_CLIP_BOUNDARY_EPSILON) {
        flags |= GE_GBI_CLIP_TOP;
    }
    if (clip[2] < -w - GE_GBI_CLIP_BOUNDARY_EPSILON) {
        flags |= GE_GBI_CLIP_NEAR;
    }
    if (clip[2] > w + GE_GBI_CLIP_BOUNDARY_EPSILON) {
        flags |= GE_GBI_CLIP_FAR;
    }
    if (w <= 0.0f) {
        flags |= GE_GBI_CLIP_NONPOSITIVE_W;
    }
    return flags;
}

static void ge_gbi_clip_finalize_vertex(GeGbiProcessedVertex *vertex)
{
    const float inverse_w = 1.0f / vertex->clip[3];

    vertex->ndc[0] = vertex->clip[0] * inverse_w;
    vertex->ndc[1] = vertex->clip[1] * inverse_w;
    vertex->ndc[2] = vertex->clip[2] * inverse_w;
    vertex->has_ndc = 1U;
    memset(vertex->screen, 0, sizeof(vertex->screen));
    vertex->has_screen = 0U;
    vertex->clip_flags = ge_gbi_clip_flags(vertex->clip);
}

static void ge_gbi_clip_interpolate(GeGbiProcessedVertex *output,
                                    const GeGbiProcessedVertex *start,
                                    const GeGbiProcessedVertex *end,
                                    float amount)
{
    size_t channel;

    memset(output, 0, sizeof(*output));
    ge_gbi_clip_interpolate_array(output->object, start->object, end->object,
                                  4U, amount);
    ge_gbi_clip_interpolate_array(output->eye, start->eye, end->eye, 4U,
                                  amount);
    ge_gbi_clip_interpolate_array(output->clip, start->clip, end->clip, 4U,
                                  amount);
    ge_gbi_clip_interpolate_array(output->normal, start->normal, end->normal,
                                  3U, amount);
    ge_gbi_clip_interpolate_array(output->texture, start->texture,
                                  end->texture, 2U, amount);
    for (channel = 0U; channel < 4U; ++channel) {
        const float color = (float)start->rgba[channel]
            + ((float)end->rgba[channel] - (float)start->rgba[channel])
                * amount;

        output->rgba[channel] = ge_gbi_clip_byte(color);
    }
    output->texture_generated = (uint8_t)(start->texture_generated != 0U
        && end->texture_generated != 0U);
    ge_gbi_clip_finalize_vertex(output);
}

static int ge_gbi_clip_vertex_is_finite(const GeGbiProcessedVertex *vertex)
{
    size_t index;

    for (index = 0U; index < 4U; ++index) {
        if (!isfinite(vertex->object[index])
                || !isfinite(vertex->eye[index])
                || !isfinite(vertex->clip[index])) {
            return 0;
        }
    }
    for (index = 0U; index < 3U; ++index) {
        if (!isfinite(vertex->normal[index])) {
            return 0;
        }
    }
    for (index = 0U; index < 2U; ++index) {
        if (!isfinite(vertex->texture[index])) {
            return 0;
        }
    }
    return 1;
}

static GeGbiClipStatus ge_gbi_clip_against_plane(
    const GeGbiProcessedVertex *input, size_t input_count,
    GeGbiProcessedVertex *output, size_t *output_count,
    GeGbiClipPlane plane)
{
    const GeGbiProcessedVertex *previous;
    float previous_distance;
    int previous_inside;
    size_t input_index;

    *output_count = 0U;
    if (input_count == 0U) {
        return GE_GBI_CLIP_OK;
    }
    previous = &input[input_count - 1U];
    previous_distance = ge_gbi_clip_plane_distance(previous, plane);
    previous_inside = previous_distance >= 0.0f;

    for (input_index = 0U; input_index < input_count; ++input_index) {
        const GeGbiProcessedVertex *current = &input[input_index];
        const float current_distance
            = ge_gbi_clip_plane_distance(current, plane);
        const int current_inside = current_distance >= 0.0f;

        if (current_inside != previous_inside) {
            const float denominator = previous_distance - current_distance;
            const float amount = previous_distance / denominator;

            if (*output_count >= GE_GBI_CLIP_MAX_POLYGON_VERTICES) {
                return GE_GBI_CLIP_CAPACITY_EXCEEDED;
            }
            ge_gbi_clip_interpolate(&output[*output_count], previous, current,
                                    amount);
            ++*output_count;
        }
        if (current_inside) {
            if (*output_count >= GE_GBI_CLIP_MAX_POLYGON_VERTICES) {
                return GE_GBI_CLIP_CAPACITY_EXCEEDED;
            }
            output[*output_count] = *current;
            ++*output_count;
        }
        previous = current;
        previous_distance = current_distance;
        previous_inside = current_inside;
    }
    return GE_GBI_CLIP_OK;
}

GeGbiClipStatus ge_gbi_clip_triangle(
    const GeGbiProcessedVertex input[3],
    GeGbiClipResult *result)
{
    GeGbiProcessedVertex scratch[2][GE_GBI_CLIP_MAX_POLYGON_VERTICES];
    size_t input_count = 3U;
    size_t plane_index;
    size_t vertex_index;
    uint8_t common_clip_flags = GE_GBI_CLIP_CANONICAL_MASK;
    uint8_t combined_clip_flags = 0U;
    int all_positive_w = 1;

    if (input == NULL || result == NULL) {
        return GE_GBI_CLIP_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    for (vertex_index = 0U; vertex_index < 3U; ++vertex_index) {
        if (!ge_gbi_clip_vertex_is_finite(&input[vertex_index])) {
            return GE_GBI_CLIP_NONFINITE_INPUT;
        }
        const uint8_t actual_flags
            = ge_gbi_clip_flags(input[vertex_index].clip);

        common_clip_flags &= actual_flags;
        combined_clip_flags |= actual_flags;
        if (input[vertex_index].clip[3] < GE_GBI_CLIP_MINIMUM_W) {
            all_positive_w = 0;
        }
    }
    if (common_clip_flags != 0U) {
        return GE_GBI_CLIP_OK;
    }
    if (all_positive_w != 0) {
        if ((combined_clip_flags & GE_GBI_CLIP_CANONICAL_MASK) == 0U) {
            memcpy(result->triangles[0].vertices, input,
                   sizeof(result->triangles[0].vertices));
            for (vertex_index = 0U; vertex_index < 3U; ++vertex_index) {
                ge_gbi_clip_finalize_vertex(
                    &result->triangles[0].vertices[vertex_index]);
            }
            result->triangle_count = 1U;
            return GE_GBI_CLIP_OK;
        }
    }

    memcpy(scratch[0], input, sizeof(GeGbiProcessedVertex) * 3U);
    for (plane_index = 0U; plane_index < GE_GBI_CLIP_PLANE_COUNT;
            ++plane_index) {
        const size_t source_index = plane_index & 1U;
        const size_t destination_index = source_index ^ 1U;
        size_t output_count;
        GeGbiClipStatus status = ge_gbi_clip_against_plane(
            scratch[source_index], input_count, scratch[destination_index],
            &output_count, (GeGbiClipPlane)plane_index);

        if (status != GE_GBI_CLIP_OK) {
            return status;
        }
        input_count = output_count;
        if (input_count < 3U) {
            return GE_GBI_CLIP_OK;
        }
    }
    {
        const GeGbiProcessedVertex *polygon
            = scratch[GE_GBI_CLIP_PLANE_COUNT & 1U];

        if (input_count - 2U > GE_GBI_CLIP_MAX_TRIANGLES) {
            return GE_GBI_CLIP_CAPACITY_EXCEEDED;
        }
        for (vertex_index = 0U; vertex_index < input_count; ++vertex_index) {
            ge_gbi_clip_finalize_vertex(
                &scratch[GE_GBI_CLIP_PLANE_COUNT & 1U][vertex_index]);
        }
        for (vertex_index = 1U; vertex_index + 1U < input_count;
                ++vertex_index) {
            GeGbiClippedTriangle *triangle
                = &result->triangles[result->triangle_count];

            triangle->vertices[0] = polygon[0];
            triangle->vertices[1] = polygon[vertex_index];
            triangle->vertices[2] = polygon[vertex_index + 1U];
            ++result->triangle_count;
        }
    }
    return GE_GBI_CLIP_OK;
}
