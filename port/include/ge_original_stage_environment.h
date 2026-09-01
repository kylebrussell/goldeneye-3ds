#ifndef GE_ORIGINAL_STAGE_ENVIRONMENT_H
#define GE_ORIGINAL_STAGE_ENVIRONMENT_H

#include <stdint.h>

#define GE_ORIGINAL_STAGE_ENVIRONMENT_LUT_SIZE 256U

typedef struct GeOriginalStageEnvironment {
    int32_t level_id;
    float blend_multiplier;
    float far_fog;
    float near_fog;
    float max_visibility_range;
    float max_obfuscation_range;
    float min_visibility_range;
    uint32_t intensity;
    int32_t difference_from_far_intensity;
    int32_t far_intensity;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t clouds;
    float cloud_repeat;
    int16_t sky_image_id;
    float cloud_red;
    float cloud_green;
    float cloud_blue;
    uint8_t is_water;
    float water_repeat;
    int16_t water_image_id;
    float water_red;
    float water_green;
    float water_blue;
    float water_concavity;
    uint8_t fog_enabled;
    uint8_t cinematic;
} GeOriginalStageEnvironment;

/* Exact single-player selection used by fogLoadLevelEnvironment. Cinematic
 * first attempts LEVELID+900, then falls through to the normal record. */
int ge_original_stage_environment_select(
    int32_t level_id, int cinematic, GeOriginalStageEnvironment *environment);

uint32_t ge_original_stage_environment_pica_fog_color(
    const GeOriginalStageEnvironment *environment);
int ge_original_stage_environment_build_fog_lut(
    const GeOriginalStageEnvironment *environment,
    float values[GE_ORIGINAL_STAGE_ENVIRONMENT_LUT_SIZE]);

#endif
