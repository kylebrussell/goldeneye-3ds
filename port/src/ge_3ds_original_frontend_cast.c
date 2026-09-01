#include "ge_3ds_original_frontend_cast.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define GE_CAST_FOV_Y_DEGREES 46.0f
#define GE_CAST_HALF_WIDTH 200.0f
#define GE_CAST_HALF_HEIGHT 120.0f
#define GE_CAST_SCREEN_HEIGHT 240.0f
#define GE_CAST_FAR 2000.0f
#define GE_CAST_NEAR 10.0f
#define GE_PI 3.14159265358979323846f

int ge_3ds_original_frontend_cast_project(
    const float camera_space[3], float projected[3])
{
    float depth;
    float focal;
    float canonical_vi_y;
    if (camera_space == NULL || projected == NULL
            || !isfinite(camera_space[0])
            || !isfinite(camera_space[1])
            || !isfinite(camera_space[2]))
        return 0;
    depth = -camera_space[2];
    if (depth < GE_CAST_NEAR) return 0;
    focal = GE_CAST_HALF_HEIGHT
        / tanf(GE_CAST_FOV_Y_DEGREES * 0.5f
            * (GE_PI / 180.0f));
    projected[0] = GE_CAST_HALF_WIDTH
        + camera_space[0] * focal / depth;
    /* N64 VI: positive camera-up moves toward smaller top-down Y. */
    canonical_vi_y = GE_CAST_HALF_HEIGHT
        - camera_space[1] * focal / depth;
    /* Mtx_OrthoTilt: convert that top-down VI result to bottom-up PICA Y. */
    projected[1] = GE_CAST_SCREEN_HEIGHT - canonical_vi_y;
    projected[2] = fminf(0.999f,
        fmaxf(0.001f, 1.0f - depth / GE_CAST_FAR));
    return isfinite(projected[0]) && isfinite(projected[1])
        && isfinite(projected[2]);
}

static Ge3dsOriginalFrontendCastClipVertex interpolate_near(
    const Ge3dsOriginalFrontendCastClipVertex *from,
    const Ge3dsOriginalFrontendCastClipVertex *to)
{
    Ge3dsOriginalFrontendCastClipVertex result;
    const float denominator = to->camera_space[2] - from->camera_space[2];
    const float t = (-GE_CAST_NEAR - from->camera_space[2]) / denominator;
    size_t index;
    for (index = 0U; index < 3U; ++index)
        result.camera_space[index] = from->camera_space[index]
            + (to->camera_space[index] - from->camera_space[index]) * t;
    result.camera_space[2] = -GE_CAST_NEAR;
    for (index = 0U; index < 2U; ++index)
        result.texture[index] = from->texture[index]
            + (to->texture[index] - from->texture[index]) * t;
    for (index = 0U; index < 4U; ++index)
        result.rgba[index] = from->rgba[index]
            + (to->rgba[index] - from->rgba[index]) * t;
    return result;
}

size_t ge_3ds_original_frontend_cast_clip_project_triangle(
    const Ge3dsOriginalFrontendCastClipVertex input[3],
    Ge3dsOriginalFrontendCastProjectedVertex output[6])
{
    Ge3dsOriginalFrontendCastClipVertex polygon[4];
    size_t polygon_count = 0U;
    size_t edge;
    size_t output_count = 0U;
    if (input == NULL || output == NULL) return 0U;
    for (edge = 0U; edge < 3U; ++edge) {
        const Ge3dsOriginalFrontendCastClipVertex *from =
            &input[(edge + 2U) % 3U];
        const Ge3dsOriginalFrontendCastClipVertex *to = &input[edge];
        const int from_inside = from->camera_space[2] <= -GE_CAST_NEAR;
        const int to_inside = to->camera_space[2] <= -GE_CAST_NEAR;
        if (from_inside != to_inside)
            polygon[polygon_count++] = interpolate_near(from, to);
        if (to_inside) polygon[polygon_count++] = *to;
    }
    if (polygon_count < 3U || polygon_count > 4U) return 0U;
    for (edge = 1U; edge + 1U < polygon_count; ++edge) {
        const size_t indices[3] = {0U, edge, edge + 1U};
        size_t corner;
        for (corner = 0U; corner < 3U; ++corner) {
            const Ge3dsOriginalFrontendCastClipVertex *source =
                &polygon[indices[corner]];
            Ge3dsOriginalFrontendCastProjectedVertex *destination =
                &output[output_count++];
            if (!ge_3ds_original_frontend_cast_project(
                    source->camera_space, destination->projected))
                return 0U;
            memcpy(destination->texture, source->texture,
                sizeof(destination->texture));
            memcpy(destination->rgba, source->rgba,
                sizeof(destination->rgba));
        }
    }
    return output_count;
}
