#include "ge_dam_environment.h"

#include <stddef.h>

/* LEVELID_DAM from the original fog_tables in src/game/bgfog.c. */
const GeDamEnvironment ge_dam_environment = {
    5.0f,
    15000.0f,
    995.0f / 1000.0f,
    1000.0f / 1000.0f,
    0x10U,
    0x30U,
    0x60U,
    1U,
    5000.0f,
    255U,
    255U,
    255U,
};

uint32_t ge_dam_environment_pica_fog_color(void)
{
    return (uint32_t)ge_dam_environment.red
        | ((uint32_t)ge_dam_environment.green << 8U)
        | ((uint32_t)ge_dam_environment.blue << 16U);
}

static float ge_dam_depth_to_eye_distance(float depth)
{
    const float near_clip = ge_dam_environment.near_clip;
    const float far_clip = ge_dam_environment.far_clip;

    return far_clip * near_clip
        / (depth * (far_clip - near_clip) + near_clip);
}

float ge_dam_environment_fog_amount(float eye_distance)
{
    const float near_clip = ge_dam_environment.near_clip;
    const float far_clip = ge_dam_environment.far_clip;
    const float difference =
        ge_dam_environment.fog_difference_intensity;
    const float far_intensity =
        ge_dam_environment.fog_far_intensity;
    const float intensity_range = far_intensity - difference;
    const float fog_multiplier =
        (256.0f * (0.5f - 0.0f)) / intensity_range;
    const float fog_offset =
        (256.0f * (0.5f - difference)) / intensity_range;
    const float reciprocal_coefficient =
        ((far_clip * -fog_multiplier * (near_clip + 1.0f))
            / (far_clip - near_clip))
        / 255.0f;
    const float constant_coefficient =
        ((fog_multiplier * (far_clip + 1.0f) / (far_clip - near_clip))
            + fog_offset)
        / 255.0f;
    float fog;

    if (eye_distance <= 0.0f) {
        return 0.0f;
    }

    fog = reciprocal_coefficient / eye_distance + constant_coefficient;
    if (fog < 0.0f) {
        return 0.0f;
    }
    if (fog > 1.0f) {
        return 1.0f;
    }
    return fog;
}

void ge_dam_environment_build_fog_lut(
    float values[GE_DAM_ENVIRONMENT_LUT_SIZE])
{
    float previous_visibility = 0.0f;
    size_t index;

    if (values == NULL) {
        return;
    }

    /* FogLut_FromArray follows the PICA register layout: values 0..127 are
     * visibility samples and 128..255 are their forward differences. This is
     * the same arrangement used by Citro3D's FogLut_Exp. */
    for (index = 0U; index <= 128U; ++index) {
        const float depth = (float)index
            / 128.0f;
        const float eye_distance = ge_dam_depth_to_eye_distance(depth);
        const float visibility =
            1.0f - ge_dam_environment_fog_amount(eye_distance);

        if (index < 128U) {
            values[index] = visibility;
        }
        if (index > 0U) {
            values[index + 127U] = visibility - previous_visibility;
        }
        previous_visibility = visibility;
    }
}
