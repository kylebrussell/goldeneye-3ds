#include "ge_dam_environment.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void assert_near(float actual, float expected, float tolerance)
{
    assert(fabsf(actual - expected) <= tolerance);
}

int main(void)
{
    float fog[GE_DAM_ENVIRONMENT_LUT_SIZE];
    size_t index;

    assert_near(ge_dam_environment.near_clip, 5.0f, 0.0001f);
    assert_near(ge_dam_environment.far_clip, 15000.0f, 0.0001f);
    assert(ge_dam_environment.red == 0x10U);
    assert(ge_dam_environment.green == 0x30U);
    assert(ge_dam_environment.blue == 0x60U);
    assert(ge_dam_environment_pica_fog_color() == UINT32_C(0x00603010));
    assert(ge_dam_environment.clouds == 1U);
    assert_near(ge_dam_environment.cloud_repeat, 5000.0f, 0.0001f);

    ge_dam_environment_build_fog_lut(fog);

    /* Reverse depth zero is the far plane. PICA's fog LUT stores scene
     * visibility, so it rises toward one as depth approaches the camera. */
    assert_near(fog[0], 0.0f, 0.0001f);
    assert_near(fog[127], 1.0f, 0.0001f);
    for (index = 1U; index < 128U; ++index) {
        assert(fog[index] >= fog[index - 1U]);
        assert(fog[index] >= 0.0f);
        assert(fog[index] <= 1.0f);
    }
    for (index = 128U; index < GE_DAM_ENVIRONMENT_LUT_SIZE; ++index) {
        assert(fog[index] >= 0.0f);
        assert(fog[index] <= 1.0f);
    }

    /* The exact decompiled reciprocal curve is already 86% fogged at the
     * authored NearFog visibility distance of 3333 world units. */
    assert_near(ge_dam_environment_fog_amount(3333.0f),
                0.8633f, 0.001f);

    puts("Canonical Dam fog environment LUT passed");
    return 0;
}
