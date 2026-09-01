#include "ge_original_stage_environment.h"
#include "ge_original_stage_environment_table_types.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

extern const GeOriginalEnvironmentRecord ge_original_fog_tables[];
extern const GeOriginalEnvironmentFoglessRecord ge_original_fogless_tables[];

static const GeOriginalEnvironmentRecord *ge_environment_fogged(int32_t id)
{
    const GeOriginalEnvironmentRecord *record;
    for (record = ge_original_fog_tables; record->Id != 0U; ++record) {
        if ((int32_t)record->Id == id) return record;
    }
    return NULL;
}

static const GeOriginalEnvironmentFoglessRecord *ge_environment_fogless(
    int32_t id)
{
    const GeOriginalEnvironmentFoglessRecord *record;
    for (record = ge_original_fogless_tables; record->Id != 0U; ++record) {
        if ((int32_t)record->Id == id) return record;
    }
    return &ge_original_fogless_tables[0];
}

int ge_original_stage_environment_select(
    int32_t level_id, int cinematic, GeOriginalStageEnvironment *environment)
{
    const GeOriginalEnvironmentRecord *fogged = NULL;
    if (environment == NULL) return 0;
    if (cinematic) fogged = ge_environment_fogged(level_id + 900);
    if (fogged == NULL) fogged = ge_environment_fogged(level_id);
    memset(environment, 0, sizeof(*environment));
    environment->level_id = level_id;
    environment->cinematic = cinematic != 0;
    if (fogged != NULL) {
        environment->blend_multiplier = fogged->Visibility.BlendMultiplier;
        environment->far_fog = fogged->Visibility.FarFog;
        environment->near_fog = fogged->Visibility.Nfd.NearFog;
        environment->max_visibility_range =
            fogged->Visibility.Nfd.MaxVisRange;
        environment->max_obfuscation_range =
            fogged->Visibility.Nfd.MaxObfuscationRange;
        environment->min_visibility_range = fogged->Visibility.MinVisrange;
        environment->intensity = fogged->Visibility.Intensity;
        environment->difference_from_far_intensity =
            fogged->Fog.DifferenceFromFarIntensity;
        environment->far_intensity = fogged->Fog.FarIntensity;
        environment->red = fogged->Sky.Red;
        environment->green = fogged->Sky.Green;
        environment->blue = fogged->Sky.Blue;
        environment->clouds = fogged->Sky.Clouds;
        environment->cloud_repeat = fogged->Sky.CloudRepeat;
        environment->sky_image_id = fogged->Sky.SkyImageId;
        environment->cloud_red = fogged->Sky.CloudRed;
        environment->cloud_green = fogged->Sky.CloudGreen;
        environment->cloud_blue = fogged->Sky.CloudBlue;
        environment->is_water = fogged->Sky.IsWater;
        environment->water_repeat = fogged->Sky.WaterRepeat;
        environment->water_image_id = fogged->Sky.WaterImageId;
        environment->water_red = fogged->Sky.WaterRed;
        environment->water_green = fogged->Sky.WaterGreen;
        environment->water_blue = fogged->Sky.WaterBlue;
        environment->water_concavity = fogged->Sky.WaterConcavity;
        environment->fog_enabled = 1U;
    } else {
        const GeOriginalEnvironmentFoglessRecord *record =
            ge_environment_fogless(level_id);
        environment->red = record->Red;
        environment->green = record->Green;
        environment->blue = record->Blue;
        environment->clouds = record->Clouds;
        environment->cloud_repeat = record->CloudRepeat;
        environment->sky_image_id = record->SkyImageId;
        environment->cloud_red = record->CloudRed;
        environment->cloud_green = record->CloudGreen;
        environment->cloud_blue = record->CloudBlue;
        environment->is_water = record->IsWater;
        environment->water_repeat = record->WaterRepeat;
        environment->water_image_id = record->WaterImageId;
        environment->water_red = record->WaterRed;
        environment->water_green = record->WaterGreen;
        environment->water_blue = record->WaterBlue;
        environment->water_concavity = record->WaterConcavity;
    }
    return 1;
}

uint32_t ge_original_stage_environment_pica_fog_color(
    const GeOriginalStageEnvironment *environment)
{
    if (environment == NULL) return 0U;
    return (uint32_t)environment->red
        | ((uint32_t)environment->green << 8U)
        | ((uint32_t)environment->blue << 16U);
}

int ge_original_stage_environment_build_fog_lut(
    const GeOriginalStageEnvironment *environment,
    float values[GE_ORIGINAL_STAGE_ENVIRONMENT_LUT_SIZE])
{
    float previous_visibility = 0.0f;
    size_t index;
    if (environment == NULL || values == NULL
            || environment->fog_enabled == 0U
            || !isfinite(environment->blend_multiplier)
            || !isfinite(environment->far_fog)
            || environment->blend_multiplier <= 0.0f
            || environment->far_fog <= environment->blend_multiplier
            || environment->far_intensity
                == environment->difference_from_far_intensity)
        return 0;
    for (index = 0U; index <= 128U; ++index) {
        const float depth = (float)index / 128.0f;
        const float near_clip = environment->blend_multiplier;
        const float far_clip = environment->far_fog;
        const float eye_distance = far_clip * near_clip
            / (depth * (far_clip - near_clip) + near_clip);
        const float difference =
            (float)environment->difference_from_far_intensity / 1000.0f;
        const float far_intensity =
            (float)environment->far_intensity / 1000.0f;
        const float multiplier = 128.0f / (far_intensity - difference);
        const float offset = 256.0f * (0.5f - difference)
            / (far_intensity - difference);
        const float reciprocal =
            ((far_clip * -multiplier * (near_clip + 1.0f))
                / (far_clip - near_clip)) / 255.0f;
        const float constant =
            ((multiplier * (far_clip + 1.0f) / (far_clip - near_clip))
                + offset) / 255.0f;
        float fog = reciprocal / eye_distance + constant;
        float visibility;
        if (fog < 0.0f) fog = 0.0f;
        if (fog > 1.0f) fog = 1.0f;
        visibility = 1.0f - fog;
        if (index < 128U) values[index] = visibility;
        if (index > 0U) values[index + 127U] =
            visibility - previous_visibility;
        previous_visibility = visibility;
    }
    return 1;
}
