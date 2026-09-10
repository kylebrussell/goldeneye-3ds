/* Host test for the stereoscopic clip-shift math and its configuration. */
#include "ge_3ds_stereo.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int near_equal(float a, float b)
{
    return fabsf(a - b) <= 1e-4f * (1.0f + fabsf(b));
}

static void test_defaults_and_zero_paths(void)
{
    Ge3dsStereoConfig config;
    float constant = 1.0f, coefficient = 1.0f;
    Ge3dsStereoUniforms uniforms;
    const float column[2] = {0.0f, -0.005f};
    ge_3ds_stereo_config_defaults(&config);
    assert(config.enabled && config.parallax_px > 0.0f
        && config.convergence > 0.0f);
    /* Mono eye, zero slider and disabled config never shift anything. */
    ge_3ds_stereo_pixel_shift(&config, 1.0f, 0, GE_3DS_STEREO_LAYER_WORLD,
        &constant, &coefficient);
    assert(constant == 0.0f && coefficient == 0.0f);
    ge_3ds_stereo_pixel_shift(&config, 0.0f, -1, GE_3DS_STEREO_LAYER_WORLD,
        &constant, &coefficient);
    assert(constant == 0.0f && coefficient == 0.0f);
    config.enabled = false;
    ge_3ds_stereo_uniforms(&config, 1.0f, 1, GE_3DS_STEREO_LAYER_HUD,
        column, &uniforms);
    assert(uniforms.constant[0] == 0.0f && uniforms.constant[1] == 0.0f
        && uniforms.coefficient[0] == 0.0f && uniforms.coefficient[1] == 0.0f);
    assert(ge_3ds_stereo_max_world_shift_px(&config, 1.0f, 15.0f) == 0.0f);
}

/* The homogeneous shift constant + coefficient*w divided by w must equal
 * P*(D/w - 1) for the left eye: near geometry moves right, far moves left,
 * the convergence plane does not move, and the right eye mirrors it. */
static void test_world_parallax_model(void)
{
    Ge3dsStereoConfig config;
    float constant, coefficient;
    float depth;
    ge_3ds_stereo_config_defaults(&config);
    config.parallax_px = 8.0f;
    config.convergence = 180.0f;
    ge_3ds_stereo_pixel_shift(&config, 1.0f, -1, GE_3DS_STEREO_LAYER_WORLD,
        &constant, &coefficient);
    for (depth = 15.0f; depth < 20000.0f; depth *= 1.7f) {
        const float shift = (constant + coefficient * depth) / depth;
        const float expected = 8.0f * (180.0f / depth - 1.0f);
        assert(near_equal(shift, expected));
    }
    assert(near_equal((constant + coefficient * 180.0f) / 180.0f, 0.0f));
    assert(coefficient < 0.0f); /* far limit is -P for the left eye */
    {
        float right_constant, right_coefficient;
        ge_3ds_stereo_pixel_shift(&config, 1.0f, 1, GE_3DS_STEREO_LAYER_WORLD,
            &right_constant, &right_coefficient);
        assert(near_equal(right_constant, -constant)
            && near_equal(right_coefficient, -coefficient));
    }
    /* Half slider halves every shift. */
    {
        float half_constant, half_coefficient;
        ge_3ds_stereo_pixel_shift(&config, 0.5f, -1, GE_3DS_STEREO_LAYER_WORLD,
            &half_constant, &half_coefficient);
        assert(near_equal(half_constant, constant * 0.5f)
            && near_equal(half_coefficient, coefficient * 0.5f));
    }
}

static void test_flat_layers(void)
{
    Ge3dsStereoConfig config;
    float constant, coefficient;
    ge_3ds_stereo_config_defaults(&config);
    config.parallax_px = 8.0f;
    config.hud_parallax_px = 2.0f;
    ge_3ds_stereo_pixel_shift(&config, 1.0f, -1, GE_3DS_STEREO_LAYER_HUD,
        &constant, &coefficient);
    assert(near_equal(constant, 2.0f) && coefficient == 0.0f);
    ge_3ds_stereo_pixel_shift(&config, 1.0f, -1, GE_3DS_STEREO_LAYER_SKY,
        &constant, &coefficient);
    assert(near_equal(constant, -8.0f) && coefficient == 0.0f);
    ge_3ds_stereo_pixel_shift(&config, 1.0f, -1, GE_3DS_STEREO_LAYER_FLAT,
        &constant, &coefficient);
    assert(constant == 0.0f && coefficient == 0.0f);
}

/* Uniforms follow the screen-x column of the tilted orthographic matrix, so
 * the shader adds them to clip xy without knowing the framebuffer rotation. */
static void test_uniform_mapping(void)
{
    Ge3dsStereoConfig config;
    Ge3dsStereoUniforms uniforms;
    const float column[2] = {0.0f, -0.005f};
    ge_3ds_stereo_config_defaults(&config);
    config.hud_parallax_px = 4.0f;
    ge_3ds_stereo_uniforms(&config, 1.0f, -1, GE_3DS_STEREO_LAYER_HUD,
        column, &uniforms);
    assert(uniforms.constant[0] == 0.0f
        && near_equal(uniforms.constant[1], -0.02f)
        && uniforms.coefficient[0] == 0.0f && uniforms.coefficient[1] == 0.0f);
}

static void test_config_parsing(void)
{
    Ge3dsStereoConfig config;
    FILE *stream = tmpfile();
    assert(stream != NULL);
    fputs("# comment\nparallax_px 12\nconvergence 250\n"
          "first_person_parallax_px 1.5\nhud_parallax_px -3\n"
          "enabled 1\nparallax_px 900\nbogus 1\n", stream);
    rewind(stream);
    /* Out-of-range and unknown keys are rejected but do not undo good ones. */
    assert(!ge_3ds_stereo_config_load(&config, stream));
    fclose(stream);
    assert(config.enabled && near_equal(config.parallax_px, 12.0f)
        && near_equal(config.convergence, 250.0f)
        && near_equal(config.first_person_parallax_px, 1.5f)
        && near_equal(config.hud_parallax_px, -3.0f));
    stream = tmpfile();
    fputs("enabled 0\n", stream);
    rewind(stream);
    assert(ge_3ds_stereo_config_load(&config, stream) && !config.enabled);
    fclose(stream);
}

static void test_culling_margin(void)
{
    Ge3dsStereoConfig config;
    ge_3ds_stereo_config_defaults(&config);
    config.parallax_px = 8.0f;
    config.convergence = 180.0f;
    /* Far-only geometry: the margin is the far parallax itself. */
    assert(near_equal(ge_3ds_stereo_max_world_shift_px(&config, 1.0f, 10000.0f), 8.0f));
    /* The real near endpoint must be retained, including small props. */
    assert(near_equal(ge_3ds_stereo_max_world_shift_px(&config, 1.0f, 15.0f), 88.0f));
    assert(near_equal(ge_3ds_stereo_max_world_shift_px(&config, 1.0f, 60.0f), 16.0f));
    assert(near_equal(ge_3ds_stereo_max_world_shift_px(&config, 0.5f, 60.0f), 8.0f));
    assert(isinf(ge_3ds_stereo_max_world_shift_px(&config, 1.0f, 0.0f)));
    assert(isinf(ge_3ds_stereo_max_world_shift_px(&config, 1.0f, NAN)));
    assert(isinf(ge_3ds_stereo_max_world_shift_px(&config, 1.0f, INFINITY)));
    assert(near_equal(ge_3ds_stereo_max_shift_px(&config, 1.0f,
        GE_3DS_STEREO_LAYER_FIRST_PERSON, 15.0f), 6.0f));
    config.parallax_px = 0.0f;
    assert(ge_3ds_stereo_max_world_shift_px(&config, 1.0f, 0.0f) == 0.0f);
    /* Independently configured weapons can exceed world disparity. */
    config.first_person_parallax_px = 20.0f;
    config.first_person_convergence = 300.0f;
    assert(near_equal(ge_3ds_stereo_max_shift_px(&config, 1.0f,
        GE_3DS_STEREO_LAYER_FIRST_PERSON, 15.0f), 380.0f));
}

static void test_bound_contains_both_eyes(void)
{
    Ge3dsStereoConfig config;
    ge_3ds_stereo_config_defaults(&config);
    for (int layer = GE_3DS_STEREO_LAYER_WORLD;
            layer <= GE_3DS_STEREO_LAYER_FIRST_PERSON; ++layer) {
        for (float slider = 0.0f; slider <= 1.25f; slider += 0.25f) {
            for (float near_depth = 0.5f; near_depth < 20000.0f;
                    near_depth *= 2.0f) {
                const float margin = ge_3ds_stereo_max_shift_px(&config,
                    slider, (Ge3dsStereoLayer)layer, near_depth);
                for (int eye = -1; eye <= 1; eye += 2) {
                    float constant, coefficient;
                    ge_3ds_stereo_pixel_shift(&config, slider, eye,
                        (Ge3dsStereoLayer)layer, &constant, &coefficient);
                    for (float depth = near_depth; depth < 100000.0f;
                            depth *= 1.7f) {
                        const float shift = fabsf(constant / depth + coefficient);
                        assert(shift <= margin + 0.0001f * (1.0f + margin));
                    }
                }
            }
        }
    }
}

int main(void)
{
    test_defaults_and_zero_paths();
    test_world_parallax_model();
    test_flat_layers();
    test_uniform_mapping();
    test_config_parsing();
    test_culling_margin();
    test_bound_contains_both_eyes();
    puts("3DS stereo: off-axis clip shift, layer depths, config and culling margin");
    return 0;
}
