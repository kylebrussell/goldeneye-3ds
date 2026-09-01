#include "ge_original_stage_environment.h"
#include "ge_stage_assets.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint64_t hash_u32(uint64_t hash, uint32_t value)
{
    for (unsigned shift = 0U; shift < 32U; shift += 8U) {
        hash ^= (value >> shift) & 0xffU;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t hash_float(uint64_t hash, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return hash_u32(hash, bits);
}

int main(void)
{
    uint64_t hash = UINT64_C(14695981039346656037);
    size_t fogged = 0U;
    for (int stage = 0; stage < GE_STAGE_COUNT; ++stage) {
        const GeStageAssetDescriptor *descriptor =
            ge_stage_asset_descriptor((GeStageId)stage);
        GeOriginalStageEnvironment environment;
        assert(descriptor != NULL);
        assert(ge_original_stage_environment_select(
            descriptor->level_id, 0, &environment));
        fogged += environment.fog_enabled != 0U;
        hash = hash_u32(hash, (uint32_t)environment.level_id);
        hash = hash_u32(hash, environment.fog_enabled);
        hash = hash_float(hash, environment.blend_multiplier);
        hash = hash_float(hash, environment.far_fog);
        hash = hash_float(hash, environment.near_fog);
        hash = hash_u32(hash, environment.red);
        hash = hash_u32(hash, environment.green);
        hash = hash_u32(hash, environment.blue);
        hash = hash_u32(hash, environment.clouds);
        hash = hash_float(hash, environment.cloud_repeat);
        hash = hash_u32(hash, environment.is_water);
        hash = hash_float(hash, environment.water_repeat);
        hash = hash_u32(hash, (uint16_t)environment.water_image_id);
        hash = hash_float(hash, environment.water_concavity);
    }
    assert(fogged == 17U);
    assert(hash == UINT64_C(0xb623c46258a8a414));
    {
        GeOriginalStageEnvironment dam;
        GeOriginalStageEnvironment dam_cinema;
        GeOriginalStageEnvironment facility;
        GeOriginalStageEnvironment frigate;
        float fog_lut[GE_ORIGINAL_STAGE_ENVIRONMENT_LUT_SIZE];
        assert(ge_original_stage_environment_select(33, 0, &dam));
        assert(dam.fog_enabled && dam.blend_multiplier == 5.0f
               && dam.far_fog == 15000.0f && dam.near_fog == 3333.0f
               && dam.red == 0x10U && dam.green == 0x30U
               && dam.blue == 0x60U && dam.clouds == 1U);
        assert(ge_original_stage_environment_pica_fog_color(&dam)
               == UINT32_C(0x00603010));
        assert(ge_original_stage_environment_build_fog_lut(&dam, fog_lut));
        for (size_t index = 0U;
                index < GE_ORIGINAL_STAGE_ENVIRONMENT_LUT_SIZE; ++index)
            assert(fog_lut[index] >= -1.0f && fog_lut[index] <= 1.0f);
        assert(ge_original_stage_environment_select(33, 1, &dam_cinema));
        assert(dam_cinema.fog_enabled
               && dam_cinema.blend_multiplier == 30.0f);
        assert(ge_original_stage_environment_select(34, 0, &facility));
        assert(facility.fog_enabled && facility.far_fog == 5000.0f
               && facility.clouds == 0U && facility.red == 0x10U
               && facility.green == 0x20U && facility.blue == 0x10U);
        assert(ge_original_stage_environment_select(26, 0, &frigate));
        assert(!frigate.fog_enabled && frigate.clouds == 1U
               && frigate.cloud_repeat == 3000.0f
               && frigate.is_water == 1U && frigate.water_image_id == 2);
        assert(!ge_original_stage_environment_build_fog_lut(
            &frigate, fog_lut));
    }
    printf("21-stage original environment hash: %016llx (%zu fogged)\n",
           (unsigned long long)hash, fogged);
    return 0;
}
