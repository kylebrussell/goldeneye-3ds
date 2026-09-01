#include "ge_gbi_vertex.h"

#include <math.h>
#include <string.h>

#define GE_GBI_VERTEX_EPSILON 0.0000001f
#define GE_GBI_PI 3.14159265358979323846f

static void ge_gbi_vertex_transform(float result[4], const float vector[4],
                                    const GeGbiMatrix *matrix)
{
    size_t column;

    for (column = 0U; column < 4U; ++column) {
        size_t row;

        result[column] = 0.0f;
        for (row = 0U; row < 4U; ++row) {
            result[column] += vector[row] * matrix->elements[row][column];
        }
    }
}

static int ge_gbi_vertex_normalize(float vector[3])
{
    const float length_squared = vector[0] * vector[0]
        + vector[1] * vector[1] + vector[2] * vector[2];

    if (length_squared <= GE_GBI_VERTEX_EPSILON) {
        vector[0] = 0.0f;
        vector[1] = 0.0f;
        vector[2] = 0.0f;
        return 0;
    }
    {
        const float inverse_length = 1.0f / sqrtf(length_squared);

        vector[0] *= inverse_length;
        vector[1] *= inverse_length;
        vector[2] *= inverse_length;
    }
    return 1;
}

static float ge_gbi_vertex_dot(const float left[3], const float right[3])
{
    return left[0] * right[0] + left[1] * right[1] + left[2] * right[2];
}

static float ge_gbi_vertex_clamp_unit(float value)
{
    if (value < -1.0f) {
        return -1.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

static uint8_t ge_gbi_vertex_color_byte(float value)
{
    if (value <= 0.0f) {
        return 0U;
    }
    if (value >= 255.0f) {
        return UINT8_MAX;
    }
    return (uint8_t)(value + 0.5f);
}

static void ge_gbi_vertex_transform_normal(const GeGbiMatrix *modelview,
                                           const GeGbiVertex *vertex,
                                           float normal[3])
{
    const float source[3] = {
        (float)(int8_t)vertex->red / 127.0f,
        (float)(int8_t)vertex->green / 127.0f,
        (float)(int8_t)vertex->blue / 127.0f
    };
    size_t column;

    for (column = 0U; column < 3U; ++column) {
        size_t row;

        normal[column] = 0.0f;
        for (row = 0U; row < 3U; ++row) {
            normal[column] += source[row] * modelview->elements[row][column];
        }
    }
    (void)ge_gbi_vertex_normalize(normal);
}

static void ge_gbi_vertex_apply_lighting(const GeGbiRenderState *state,
                                         const GeGbiVertex *vertex,
                                         GeGbiProcessedVertex *processed)
{
    float color[3] = {0.0f, 0.0f, 0.0f};
    uint8_t light_index;
    const uint8_t ambient_slot = state->directional_light_count;

    if (ambient_slot < GE_GBI_LIGHT_COUNT
            && (state->valid_lights & (uint8_t)(UINT8_C(1) << ambient_slot))
                != 0U) {
        size_t channel;

        for (channel = 0U; channel < 3U; ++channel) {
            color[channel] = state->lights[ambient_slot].color[channel];
        }
    }

    for (light_index = 0U;
            light_index < state->directional_light_count
                && light_index < GE_GBI_LIGHT_COUNT;
            ++light_index) {
        float direction[3];
        float intensity;
        size_t channel;

        if ((state->valid_lights & (uint8_t)(UINT8_C(1) << light_index))
                == 0U) {
            continue;
        }
        for (channel = 0U; channel < 3U; ++channel) {
            direction[channel]
                = (float)state->lights[light_index].direction[channel]
                / 127.0f;
        }
        if (!ge_gbi_vertex_normalize(direction)) {
            continue;
        }
        intensity = ge_gbi_vertex_dot(processed->normal, direction);
        if (intensity <= 0.0f) {
            continue;
        }
        for (channel = 0U; channel < 3U; ++channel) {
            color[channel] += intensity
                * state->lights[light_index].color[channel];
        }
    }

    processed->rgba[0] = ge_gbi_vertex_color_byte(color[0]);
    processed->rgba[1] = ge_gbi_vertex_color_byte(color[1]);
    processed->rgba[2] = ge_gbi_vertex_color_byte(color[2]);
    processed->rgba[3] = vertex->alpha;
}

static void ge_gbi_vertex_generate_texture(const GeGbiRenderState *state,
                                           GeGbiProcessedVertex *processed)
{
    size_t axis;

    if ((state->geometry_mode & GE_GBI_GEOMETRY_TEXTURE_GEN) == 0U
            || state->valid_look_at != UINT8_C(3)) {
        return;
    }
    for (axis = 0U; axis < 2U; ++axis) {
        float direction[3];
        float dot;
        size_t component;

        for (component = 0U; component < 3U; ++component) {
            direction[component]
                = (float)state->look_at[axis].direction[component] / 127.0f;
        }
        if (!ge_gbi_vertex_normalize(direction)) {
            return;
        }
        dot = ge_gbi_vertex_clamp_unit(
            ge_gbi_vertex_dot(processed->normal, direction));
        if ((state->geometry_mode & GE_GBI_GEOMETRY_TEXTURE_GEN_LINEAR) != 0U) {
            processed->texture[axis] = acosf(dot) / GE_GBI_PI;
        } else {
            processed->texture[axis] = (dot + 1.0f) * 0.5f;
        }
    }
    processed->texture_generated = 1U;
}

static uint8_t ge_gbi_vertex_clip_flags(const float clip[4])
{
    uint8_t flags = 0U;
    const float w = clip[3];

    if (clip[0] < -w) {
        flags |= GE_GBI_CLIP_LEFT;
    }
    if (clip[0] > w) {
        flags |= GE_GBI_CLIP_RIGHT;
    }
    if (clip[1] < -w) {
        flags |= GE_GBI_CLIP_BOTTOM;
    }
    if (clip[1] > w) {
        flags |= GE_GBI_CLIP_TOP;
    }
    if (clip[2] < -w) {
        flags |= GE_GBI_CLIP_NEAR;
    }
    if (clip[2] > w) {
        flags |= GE_GBI_CLIP_FAR;
    }
    if (w <= 0.0f) {
        flags |= GE_GBI_CLIP_NONPOSITIVE_W;
    }
    return flags;
}

GeGbiVertexProcessStatus ge_gbi_vertex_process(
    const GeGbiRenderState *state,
    const GeGbiVertex *vertex,
    GeGbiProcessedVertex *processed)
{
    const GeGbiMatrix *modelview;
    const GeGbiMatrix *projection;
    float inverse_w;

    if (state == NULL || vertex == NULL || processed == NULL) {
        return GE_GBI_VERTEX_PROCESS_INVALID_ARGUMENT;
    }
    modelview = ge_gbi_matrix_stack_top(&state->modelview_stack);
    projection = ge_gbi_matrix_stack_top(&state->projection_stack);
    if (modelview == NULL || projection == NULL) {
        return GE_GBI_VERTEX_PROCESS_INVALID_STATE;
    }

    memset(processed, 0, sizeof(*processed));
    processed->object[0] = vertex->x;
    processed->object[1] = vertex->y;
    processed->object[2] = vertex->z;
    processed->object[3] = 1.0f;
    processed->texture[0] = vertex->texture_s;
    processed->texture[1] = vertex->texture_t;
    ge_gbi_vertex_transform(processed->eye, processed->object, modelview);
    ge_gbi_vertex_transform(processed->clip, processed->eye, projection);
    processed->clip_flags = ge_gbi_vertex_clip_flags(processed->clip);

    if (fabsf(processed->clip[3]) > GE_GBI_VERTEX_EPSILON) {
        inverse_w = 1.0f / processed->clip[3];
        processed->ndc[0] = processed->clip[0] * inverse_w;
        processed->ndc[1] = processed->clip[1] * inverse_w;
        processed->ndc[2] = processed->clip[2] * inverse_w;
        processed->has_ndc = 1U;
        if (state->viewport_valid != 0U) {
            processed->screen[0] = processed->ndc[0]
                * state->viewport.scale[0] * 0.25f
                + state->viewport.translation[0] * 0.25f;
            processed->screen[1] = processed->ndc[1]
                * state->viewport.scale[1] * 0.25f
                + state->viewport.translation[1] * 0.25f;
            processed->screen[2] = processed->ndc[2]
                * state->viewport.scale[2] + state->viewport.translation[2];
            processed->has_screen = 1U;
        }
    }

    if ((state->geometry_mode & GE_GBI_GEOMETRY_LIGHTING) != 0U) {
        ge_gbi_vertex_transform_normal(modelview, vertex, processed->normal);
        ge_gbi_vertex_apply_lighting(state, vertex, processed);
        ge_gbi_vertex_generate_texture(state, processed);
    } else {
        processed->rgba[0] = vertex->red;
        processed->rgba[1] = vertex->green;
        processed->rgba[2] = vertex->blue;
        processed->rgba[3] = vertex->alpha;
    }

    return GE_GBI_VERTEX_PROCESS_OK;
}
