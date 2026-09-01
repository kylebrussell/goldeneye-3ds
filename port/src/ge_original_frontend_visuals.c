#include "ge_original_frontend_visuals.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define GE_ORIGINAL_FRONTEND_NORMAL_EPSILON 0.0000001f

static int normalize3(float vector[3])
{
    const float length_squared = vector[0] * vector[0]
        + vector[1] * vector[1] + vector[2] * vector[2];
    float inverse_length;
    if (length_squared <= GE_ORIGINAL_FRONTEND_NORMAL_EPSILON) {
        memset(vector, 0, 3U * sizeof(*vector));
        return 0;
    }
    inverse_length = 1.0f / sqrtf(length_squared);
    vector[0] *= inverse_length;
    vector[1] *= inverse_length;
    vector[2] *= inverse_length;
    return 1;
}

static uint8_t color_byte(float value)
{
    if (value <= 0.0f) return 0U;
    if (value >= 255.0f) return UINT8_MAX;
    return (uint8_t)(value + 0.5f);
}

int ge_original_frontend_generate_lit_vertex(
    const uint8_t packed_normal[3], uint8_t alpha,
    float rotation_y_radians, const uint8_t ambient_rgb[3],
    const uint8_t diffuse_rgb[3], const int8_t light_direction[3],
    GeOriginalFrontendGeneratedVertex *output)
{
    float source[3];
    float direction[3];
    float diffuse;
    const float cosine = cosf(rotation_y_radians);
    const float sine = sinf(rotation_y_radians);
    size_t channel;
    if (packed_normal == NULL || ambient_rgb == NULL || diffuse_rgb == NULL
            || light_direction == NULL || output == NULL)
        return 0;
    source[0] = (float)(int8_t)packed_normal[0] / 127.0f;
    source[1] = (float)(int8_t)packed_normal[1] / 127.0f;
    source[2] = (float)(int8_t)packed_normal[2] / 127.0f;
    output->normal[0] = source[0] * cosine + source[2] * sine;
    output->normal[1] = source[1];
    output->normal[2] = -source[0] * sine + source[2] * cosine;
    (void)normalize3(output->normal);
    for (channel = 0U; channel < 3U; ++channel)
        direction[channel] = (float)light_direction[channel] / 127.0f;
    (void)normalize3(direction);
    diffuse = output->normal[0] * direction[0]
        + output->normal[1] * direction[1]
        + output->normal[2] * direction[2];
    if (diffuse < 0.0f) diffuse = 0.0f;
    for (channel = 0U; channel < 3U; ++channel)
        output->lit_rgba[channel] = color_byte(
            (float)ambient_rgb[channel]
                + diffuse * (float)diffuse_rgb[channel]);
    output->lit_rgba[3] = alpha;
    /* guLookAtReflect(eye +Z, target origin, up +Y) publishes +X and +Y.
     * Fast3D's non-linear G_TEXTURE_GEN maps each signed dot to [0,1]. */
    output->generated_uv[0] = (output->normal[0] + 1.0f) * 0.5f;
    output->generated_uv[1] = (output->normal[1] + 1.0f) * 0.5f;
    return 1;
}

void ge_original_frontend_rareware_body_uv(
    const GeOriginalFrontendGeneratedVertex *vertex, float uv[2])
{
    /* Fast3D's generated signed-dot domain and s10.5 texture coordinates
     * make 0x0800 span one 32-texel image. */
    const float generated_scale = 1.0f / 2048.0f;
    const float tile_fraction = 1.0f / 4.0f;
    if (vertex == NULL || uv == NULL) return;
    /* Generated coordinates span the 32 texels selected by mask=5.  Convert
     * the RSP scale and 10.2 tile origin into native normalized coordinates;
     * repeat wrapping preserves the negative-origin sampling intentionally
     * used by the authored highlight map. */
    uv[0] = vertex->generated_uv[0]
            * (float)GE_ORIGINAL_RAREWARE_BODY_TEXTURE_SCALE_S
                * generated_scale
        - ((float)GE_ORIGINAL_RAREWARE_BODY_TILE_ULS * tile_fraction)
            / (float)GE_ORIGINAL_RAREWARE_REFLECTION_TEXTURE_WIDTH;
    uv[1] = vertex->generated_uv[1]
            * (float)GE_ORIGINAL_RAREWARE_BODY_TEXTURE_SCALE_T
                * generated_scale
        - ((float)GE_ORIGINAL_RAREWARE_BODY_TILE_ULT * tile_fraction)
            / (float)GE_ORIGINAL_RAREWARE_REFLECTION_TEXTURE_HEIGHT;
}

void ge_original_frontend_rareware_project(
    const float authored[3], float rotation_y_degrees, float camera_eye_z,
    float projected[3])
{
    const float radians = rotation_y_degrees
        * (3.14159265358979323846f / 180.0f);
    const float cosine = cosf(radians);
    const float sine = sinf(radians);
    const float focal = 120.0f
        / tanf(60.0f * 0.5f * (3.14159265358979323846f / 180.0f));
    float rotated_x;
    float rotated_z;
    float depth;
    float scale;
    if (authored == NULL || projected == NULL) return;
    rotated_x = authored[0] * cosine + authored[2] * sine;
    rotated_z = -authored[0] * sine + authored[2] * cosine;
    depth = camera_eye_z - rotated_z;
    scale = focal / fmaxf(1.0f, depth);
    /* The original 320x240 perspective is uniformly mapped to the centred
     * 320x240 region of the 400-pixel top screen. */
    projected[0] = 200.0f + rotated_x * scale;
    projected[1] = 120.0f - authored[1] * scale;
    projected[2] = fminf(0.999f, fmaxf(0.001f, depth / 5000.0f));
}
