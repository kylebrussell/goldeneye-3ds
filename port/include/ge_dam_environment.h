#ifndef GE_DAM_ENVIRONMENT_H
#define GE_DAM_ENVIRONMENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GE_DAM_ENVIRONMENT_LUT_SIZE 256U

typedef struct GeDamEnvironment {
    float near_clip;
    float far_clip;
    float fog_difference_intensity;
    float fog_far_intensity;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t clouds;
    float cloud_repeat;
    uint8_t cloud_red;
    uint8_t cloud_green;
    uint8_t cloud_blue;
} GeDamEnvironment;

extern const GeDamEnvironment ge_dam_environment;

float ge_dam_environment_fog_amount(float eye_distance);

/* Packs the authored RGB bytes in the little-endian RGB8 register order used
 * by C3D_FogColor/PICA, rather than the source record's visual RRGGBB order. */
uint32_t ge_dam_environment_pica_fog_color(void);

/* Builds the input array expected by Citro3D's FogLut_FromArray. The first
 * 128 values are scene visibility and the second 128 are interpolation
 * deltas. This is the PICA equivalent of GoldenEye's fogLoadCurrentEnvironment
 * and gSPFogPosition path. */
void ge_dam_environment_build_fog_lut(
    float values[GE_DAM_ENVIRONMENT_LUT_SIZE]);

#ifdef __cplusplus
}
#endif

#endif
