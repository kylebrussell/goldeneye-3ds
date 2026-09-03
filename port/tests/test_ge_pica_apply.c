#include "ge_pica_apply.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static GePicaMaterial ge_test_material(void)
{
    GePicaMaterial material;

    memset(&material, 0, sizeof(material));
    material.color_combine = GE_PICA_COMBINE_SHADE;
    material.alpha_combine = GE_PICA_ALPHA_SHADE;
    material.wrap_s = GE_PICA_WRAP_REPEAT;
    material.wrap_t = GE_PICA_WRAP_CLAMP;
    material.min_filter = GE_PICA_FILTER_NEAREST;
    material.mag_filter = GE_PICA_FILTER_LINEAR;
    return material;
}

static void test_invalid_arguments(void)
{
    GePicaMaterial material = ge_test_material();
    GePicaApplyState state;

    assert(ge_pica_apply_compile(NULL, &state)
        == GE_PICA_APPLY_INVALID_ARGUMENT);
    assert(ge_pica_apply_compile(&material, NULL)
        == GE_PICA_APPLY_INVALID_ARGUMENT);
}

static void test_textured_modulate_and_draw_state(void)
{
    GePicaMaterial material = ge_test_material();
    GePicaApplyState state;

    material.fallback_flags = GE_PICA_FALLBACK_ALPHA_DITHER;
    material.color_combine = GE_PICA_COMBINE_TEXTURE0_MODULATE_PRIMITIVE;
    material.alpha_combine = GE_PICA_ALPHA_TEXTURE0_MODULATE_SHADE;
    material.primitive_color.red = UINT8_C(0x12);
    material.primitive_color.green = UINT8_C(0x34);
    material.primitive_color.blue = UINT8_C(0x56);
    material.cull_mode = GE_PICA_CULL_BACK;
    material.depth_test_enabled = UINT8_C(1);
    material.depth_write_enabled = UINT8_C(1);
    material.alpha_test = GE_PICA_ALPHA_TEST_THRESHOLD;
    material.alpha_threshold = UINT8_C(0x7f);
    material.blend_enabled = UINT8_C(1);

    assert(ge_pica_apply_compile(&material, &state) == GE_PICA_APPLY_OK);
    assert(state.material_fallback_flags == GE_PICA_FALLBACK_ALPHA_DITHER);
    assert(state.apply_fallback_flags == GE_PICA_APPLY_FALLBACK_NONE);
    assert(state.color.source0 == GE_PICA_APPLY_SOURCE_TEXTURE0);
    assert(state.color.source1 == GE_PICA_APPLY_SOURCE_CONSTANT);
    assert(state.color.combine == GE_PICA_APPLY_MODULATE);
    assert(state.alpha.source0 == GE_PICA_APPLY_SOURCE_TEXTURE0);
    assert(state.alpha.source1 == GE_PICA_APPLY_SOURCE_PRIMARY);
    assert(state.alpha.combine == GE_PICA_APPLY_MODULATE);
    assert(state.constant_color.red == UINT8_C(0x12));
    assert(state.constant_color.green == UINT8_C(0x34));
    assert(state.constant_color.blue == UINT8_C(0x56));
    assert(state.constant_color.alpha == UINT8_MAX);
    assert(state.texture_required == UINT8_C(1));
    assert(state.draw_enabled == UINT8_C(1));
    assert(state.cull == GE_PICA_APPLY_CULL_BACK);
    assert(state.wrap_s == GE_PICA_WRAP_REPEAT);
    assert(state.wrap_t == GE_PICA_WRAP_CLAMP);
    assert(state.min_filter == GE_PICA_FILTER_NEAREST);
    assert(state.mag_filter == GE_PICA_FILTER_LINEAR);
    assert(state.depth_test_enabled == UINT8_C(1));
    assert(state.depth_write_enabled == UINT8_C(1));
    assert(state.alpha_test_enabled == UINT8_C(1));
    assert(state.alpha_threshold == UINT8_C(0x7f));
    assert(state.blend_enabled == UINT8_C(1));
}

static void test_independent_constant_components(void)
{
    GePicaMaterial material = ge_test_material();
    GePicaApplyState state;

    material.color_combine = GE_PICA_COMBINE_ENVIRONMENT;
    material.alpha_combine = GE_PICA_ALPHA_PRIMITIVE;
    material.environment_color.red = UINT8_C(0x81);
    material.environment_color.green = UINT8_C(0x82);
    material.environment_color.blue = UINT8_C(0x83);
    material.environment_color.alpha = UINT8_C(0x84);
    material.primitive_color.alpha = UINT8_C(0x35);

    assert(ge_pica_apply_compile(&material, &state) == GE_PICA_APPLY_OK);
    assert(state.color.source0 == GE_PICA_APPLY_SOURCE_CONSTANT);
    assert(state.alpha.source0 == GE_PICA_APPLY_SOURCE_CONSTANT);
    assert(state.constant_color.red == UINT8_C(0x81));
    assert(state.constant_color.green == UINT8_C(0x82));
    assert(state.constant_color.blue == UINT8_C(0x83));
    assert(state.constant_color.alpha == UINT8_C(0x35));
}

static void test_cull_both_skips_draw(void)
{
    GePicaMaterial material = ge_test_material();
    GePicaApplyState state;

    material.cull_mode = GE_PICA_CULL_BOTH;
    assert(ge_pica_apply_compile(&material, &state) == GE_PICA_APPLY_OK);
    assert(state.draw_enabled == UINT8_C(0));
    assert(state.cull == GE_PICA_APPLY_CULL_NONE);
    assert(state.apply_fallback_flags
        == GE_PICA_APPLY_FALLBACK_CULL_BOTH);
}

static void test_independent_texture_environment_components(void)
{
    GePicaMaterial material = ge_test_material();
    GePicaApplyState state;

    material.environment_color = (GePicaColor){19U, 67U, 139U, 110U};
    material.primitive_color = (GePicaColor){241U, 193U, 167U, 33U};
    material.color_combine = GE_PICA_COMBINE_TEXTURE0_MODULATE_ENVIRONMENT;
    material.alpha_combine = GE_PICA_ALPHA_TEXTURE0_MODULATE_PRIMITIVE;
    assert(ge_pica_apply_compile(&material, &state) == GE_PICA_APPLY_OK);
    assert(state.constant_color.red == 19U && state.constant_color.green == 67U
        && state.constant_color.blue == 139U && state.constant_color.alpha == 33U);
    material.color_combine = GE_PICA_COMBINE_TEXTURE0_MODULATE_PRIMITIVE;
    material.alpha_combine = GE_PICA_ALPHA_TEXTURE0_MODULATE_ENVIRONMENT;
    assert(ge_pica_apply_compile(&material, &state) == GE_PICA_APPLY_OK);
    assert(state.constant_color.red == 241U && state.constant_color.green == 193U
        && state.constant_color.blue == 167U && state.constant_color.alpha == 110U);
    assert(state.apply_fallback_flags == 0U && state.texture_required);
    assert(state.color.combine == GE_PICA_APPLY_MODULATE
        && state.alpha.combine == GE_PICA_APPLY_MODULATE);
}

static void test_unapplied_depth_mode_and_fog_are_reported(void)
{
    GePicaMaterial material = ge_test_material();
    GePicaApplyState state;

    material.depth_mode = GE_PICA_DEPTH_DECAL;
    material.fog_enabled = UINT8_C(1);
    assert(ge_pica_apply_compile(&material, &state) == GE_PICA_APPLY_OK);
    assert(state.apply_fallback_flags
        == (GE_PICA_APPLY_FALLBACK_DEPTH_MODE
            | GE_PICA_APPLY_FALLBACK_FOG));
}

static void test_invalid_enums_are_bounded(void)
{
    GePicaMaterial material = ge_test_material();
    GePicaApplyState state;

    material.color_combine = (GePicaCombineMode)99;
    material.alpha_combine = (GePicaAlphaMode)99;
    material.cull_mode = (GePicaCullMode)99;
    material.wrap_s = (GePicaTextureWrap)99;

    assert(ge_pica_apply_compile(&material, &state) == GE_PICA_APPLY_OK);
    assert((state.apply_fallback_flags
        & GE_PICA_APPLY_FALLBACK_INVALID_ENUM) != 0U);
    assert(state.color.source0 == GE_PICA_APPLY_SOURCE_PRIMARY);
    assert(state.alpha.source0 == GE_PICA_APPLY_SOURCE_PRIMARY);
    assert(state.cull == GE_PICA_APPLY_CULL_NONE);
    assert(state.wrap_s == GE_PICA_WRAP_CLAMP);
    assert(state.wrap_t == GE_PICA_WRAP_CLAMP);
    assert(state.min_filter == GE_PICA_FILTER_NEAREST);
    assert(state.mag_filter == GE_PICA_FILTER_NEAREST);
}

int main(void)
{
    test_invalid_arguments();
    test_textured_modulate_and_draw_state();
    test_independent_constant_components();
    test_independent_texture_environment_components();
    test_cull_both_skips_draw();
    test_unapplied_depth_mode_and_fog_are_reported();
    test_invalid_enums_are_bounded();
    puts("PICA application mapping tests passed");
    return 0;
}
