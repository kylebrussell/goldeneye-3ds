#include "ge_original_guard_muzzle_flash.h"

#include <math.h>
#include <string.h>

static void transform_point(const float matrix[4][4], const float local[3],
                            float eye[3])
{
    size_t axis;
    for (axis = 0U; axis < 3U; ++axis)
        eye[axis] = local[0] * matrix[0][axis]
            + local[1] * matrix[1][axis]
            + local[2] * matrix[2][axis] + matrix[3][axis];
}

int ge_original_guard_muzzle_flash_build(
    const GeOriginalPitemModelGunfire *gunfire,
    const float matrix[4][4], float model_scale,
    uint32_t scale_random, int16_t uv_cosine, int16_t uv_sine,
    GeOriginalGuardMuzzleFlashPublication *publication)
{
    float direction[3], local[4][3], scaled_size[3], base[3];
    float distance, theta, phi, sin_theta, tmp;
    float cos_phi, sin_phi, cos_theta;
    float x_cos, z_sin, y_sin, x_theta_sin, z_theta_cos;
    int16_t st[4][2];
    int32_t uv_x, uv_y, centre;
    static const uint8_t triangle_vertices[6] = {0U,1U,2U,0U,2U,3U};
    size_t axis, vertex;
    if (gunfire == NULL || matrix == NULL || publication == NULL
            || gunfire->visible == 0U || model_scale <= 0.0f
            || !isfinite(model_scale)) return 0;
    for (axis = 0U; axis < 3U; ++axis) {
        if (!isfinite(gunfire->offset[axis])
                || !isfinite(gunfire->size[axis])) return 0;
        direction[axis] = -(gunfire->offset[0] * matrix[0][axis]
            + gunfire->offset[1] * matrix[1][axis]
            + gunfire->offset[2] * matrix[2][axis] + matrix[3][axis]);
    }
    distance = sqrtf(direction[0] * direction[0]
        + direction[1] * direction[1] + direction[2] * direction[2]);
    if (distance > 0.0f) {
        tmp = 1.0f / (model_scale * distance);
        for (axis = 0U; axis < 3U; ++axis) direction[axis] *= tmp;
    } else {
        direction[0] = direction[1] = 0.0f;
        direction[2] = 1.0f / model_scale;
    }
    tmp = direction[0] * matrix[1][0]
        + direction[1] * matrix[1][1] + direction[2] * matrix[1][2];
    theta = acosf(tmp);
    sin_theta = sinf(theta);
    tmp = -(direction[0] * matrix[2][0]
        + direction[1] * matrix[2][1] + direction[2] * matrix[2][2]);
    tmp /= sin_theta;
    phi = acosf(tmp);
    tmp = -(direction[0] * matrix[0][0]
        + direction[1] * matrix[0][1] + direction[2] * matrix[0][2]);
    if (tmp < 0.0f) phi = 6.28318530717958647692f - phi;
    cos_phi = cosf(phi); sin_phi = sinf(phi);
    cos_theta = cosf(theta);
    scaled_size[0] = gunfire->size[0]
        * (0.75f + (float)(scale_random % 128U) / 256.0f);
    scaled_size[1] = gunfire->size[1]
        * (0.75f + (float)(scale_random % 128U) / 256.0f);
    scaled_size[2] = gunfire->size[2]
        * (0.75f + (float)(scale_random % 128U) / 256.0f);
    x_cos = scaled_size[0] * cos_phi * 0.5f;
    z_sin = scaled_size[2] * sin_phi * 0.5f;
    y_sin = scaled_size[1] * sin_theta * 0.5f;
    x_theta_sin = scaled_size[0] * cos_theta * sin_phi * 0.5f;
    z_theta_cos = scaled_size[2] * cos_theta * cos_phi * 0.5f;
    base[0] = gunfire->offset[0] - scaled_size[0] * 0.5f;
    base[1] = gunfire->offset[1]; base[2] = gunfire->offset[2];
    local[0][0]=base[0]-x_cos-x_theta_sin;local[0][1]=base[1]-y_sin;local[0][2]=base[2]+z_sin-z_theta_cos;
    local[1][0]=base[0]-x_cos+x_theta_sin;local[1][1]=base[1]+y_sin;local[1][2]=base[2]+z_sin+z_theta_cos;
    local[2][0]=base[0]+x_cos+x_theta_sin;local[2][1]=base[1]+y_sin;local[2][2]=base[2]-z_sin+z_theta_cos;
    local[3][0]=base[0]+x_cos-x_theta_sin;local[3][1]=base[1]-y_sin;local[3][2]=base[2]-z_sin-z_theta_cos;
    uv_x = ((int32_t)uv_cosine * gunfire->image_width * 0xb5) >> 18;
    uv_y = ((int32_t)uv_sine * gunfire->image_width * 0xb5) >> 18;
    centre = (int32_t)gunfire->image_width << 4;
    st[0][0]=(int16_t)(centre-uv_x);st[0][1]=(int16_t)(centre-uv_y);
    st[1][0]=(int16_t)(centre+uv_y);st[1][1]=(int16_t)(centre-uv_x);
    st[2][0]=(int16_t)(centre+uv_x);st[2][1]=(int16_t)(centre+uv_y);
    st[3][0]=(int16_t)(centre-uv_y);st[3][1]=(int16_t)(centre+uv_x);
    memset(publication, 0, sizeof(*publication));
    publication->gunfire = *gunfire;
    for (vertex = 0U; vertex < 6U; ++vertex) {
        const uint8_t source = triangle_vertices[vertex];
        transform_point(matrix, local[source],
                        publication->vertices[vertex].position);
        publication->vertices[vertex].texture_s = st[source][0];
        publication->vertices[vertex].texture_t = st[source][1];
    }
    return 1;
}
