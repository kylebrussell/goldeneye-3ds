#include "ge_pica_material.h"

#include <assert.h>
#include <stdio.h>

enum {
    CC_COMBINED = 0,
    CC_TEXEL0 = 1,
    CC_PRIMITIVE = 3,
    CC_SHADE = 4,
    CC_ENVIRONMENT = 5,
    CC_ZERO_AB = 15,
    CC_ZERO_C = 31,
    CC_ZERO_D = 7,
    AC_COMBINED = 0,
    AC_TEXEL0 = 1,
    AC_PRIMITIVE = 3,
    AC_SHADE = 4,
    AC_ENVIRONMENT = 5,
    AC_ONE = 6,
    AC_ZERO = 7
};

typedef struct CombineCycle {
    uint8_t color_a;
    uint8_t color_b;
    uint8_t color_c;
    uint8_t color_d;
    uint8_t alpha_a;
    uint8_t alpha_b;
    uint8_t alpha_c;
    uint8_t alpha_d;
} CombineCycle;

static void set_combine(GeGbiRenderState *state,
                        CombineCycle first,
                        CombineCycle second)
{
    state->combine_mux0 = ((uint32_t)(first.color_a & UINT8_C(0x0f)) << 20)
        | ((uint32_t)(first.color_c & UINT8_C(0x1f)) << 15)
        | ((uint32_t)(first.alpha_a & UINT8_C(0x07)) << 12)
        | ((uint32_t)(first.alpha_c & UINT8_C(0x07)) << 9)
        | ((uint32_t)(second.color_a & UINT8_C(0x0f)) << 5)
        | (uint32_t)(second.color_c & UINT8_C(0x1f));
    state->combine_mux1 = ((uint32_t)(first.color_b & UINT8_C(0x0f)) << 28)
        | ((uint32_t)(second.color_b & UINT8_C(0x0f)) << 24)
        | ((uint32_t)(second.alpha_a & UINT8_C(0x07)) << 21)
        | ((uint32_t)(second.alpha_c & UINT8_C(0x07)) << 18)
        | ((uint32_t)(first.color_d & UINT8_C(0x07)) << 15)
        | ((uint32_t)(first.alpha_b & UINT8_C(0x07)) << 12)
        | ((uint32_t)(first.alpha_d & UINT8_C(0x07)) << 9)
        | ((uint32_t)(second.color_d & UINT8_C(0x07)) << 6)
        | ((uint32_t)(second.alpha_b & UINT8_C(0x07)) << 3)
        | (uint32_t)(second.alpha_d & UINT8_C(0x07));
}

static CombineCycle direct(uint8_t color, uint8_t alpha)
{
    const CombineCycle result = {
        CC_ZERO_AB, CC_ZERO_AB, CC_ZERO_C, color,
        AC_ZERO, AC_ZERO, AC_ZERO, alpha
    };
    return result;
}

static CombineCycle modulate(uint8_t color_a, uint8_t color_c,
                             uint8_t alpha_a, uint8_t alpha_c)
{
    const CombineCycle result = {
        color_a, CC_ZERO_AB, color_c, CC_ZERO_D,
        alpha_a, AC_ZERO, alpha_c, AC_ZERO
    };
    return result;
}

static void test_invalid_arguments(void)
{
    GeGbiRenderState state;
    GePicaMaterial material;

    ge_gbi_state_init(&state);
    assert(ge_pica_material_translate(NULL, &material)
           == GE_PICA_MATERIAL_INVALID_ARGUMENT);
    assert(ge_pica_material_translate(&state, NULL)
           == GE_PICA_MATERIAL_INVALID_ARGUMENT);
}

static void test_default_is_auditable_fallback(void)
{
    GeGbiRenderState state;
    GePicaMaterial material;

    ge_gbi_state_init(&state);
    assert(ge_pica_material_translate(&state, &material)
           == GE_PICA_MATERIAL_OK);
    assert(material.color_combine == GE_PICA_COMBINE_SHADE);
    assert(material.alpha_combine == GE_PICA_ALPHA_SHADE);
    assert(material.fallback_flags == GE_PICA_FALLBACK_COMBINER);
    assert(material.texture_enabled == 0U);
    assert(material.cull_mode == GE_PICA_CULL_NONE);
    assert(material.depth_test_enabled == 0U);
    assert(material.blend_enabled == 0U);
}

static void test_textured_lit_material(void)
{
    GeGbiRenderState state;
    GePicaMaterial material;
    const CombineCycle unused = direct(CC_COMBINED, AC_COMBINED);

    ge_gbi_state_init(&state);
    state.geometry_mode = UINT32_C(0x00000001) | UINT32_C(0x00000004)
        | UINT32_C(0x00000200) | UINT32_C(0x00002000)
        | UINT32_C(0x00010000) | UINT32_C(0x00020000);
    state.texture.enabled = 1U;
    state.texture.scale_s = UINT16_C(0x8000);
    state.texture.scale_t = UINT16_C(0x4000);
    state.rare_texture.texture_id = UINT16_C(182);
    state.rare_texture_valid = 1U;
    state.active_texture_binding = GE_GBI_TEXTURE_BINDING_RARE_ID;
    state.rare_texture.type = UINT8_C(3);
    state.rare_texture.tile = UINT8_C(2);
    state.rare_texture.clamp_mirror_s = UINT8_C(2);
    state.rare_texture.clamp_mirror_t = UINT8_C(1);
    state.rare_texture.shift_s = UINT8_C(4);
    state.rare_texture.shift_t = UINT8_C(5);
    state.other_mode_high = UINT32_C(0x00080000) | UINT32_C(0x00002000);
    state.other_mode_low = UINT32_C(0x10) | UINT32_C(0x20);
    state.fog_multiplier = -32;
    state.fog_offset = 288;
    set_combine(&state,
                modulate(CC_TEXEL0, CC_SHADE, AC_TEXEL0, AC_SHADE),
                unused);

    assert(ge_pica_material_translate(&state, &material)
           == GE_PICA_MATERIAL_OK);
    assert(material.fallback_flags == GE_PICA_FALLBACK_NONE);
    assert(material.color_combine
           == GE_PICA_COMBINE_TEXTURE0_MODULATE_SHADE);
    assert(material.alpha_combine
           == GE_PICA_ALPHA_TEXTURE0_MODULATE_SHADE);
    assert(material.texture_id == 182U);
    assert(material.texture_source == GE_PICA_TEXTURE_SOURCE_RARE_ID);
    assert(material.texture_tile == 2U);
    assert(material.texture_type == 3U);
    assert(material.texture_scale_s == UINT16_C(0x8000));
    assert(material.texture_scale_t == UINT16_C(0x4000));
    assert(material.texture_shift_s == 4U);
    assert(material.texture_shift_t == 5U);
    assert(material.wrap_s == GE_PICA_WRAP_CLAMP);
    assert(material.wrap_t == GE_PICA_WRAP_MIRROR);
    assert(material.min_filter == GE_PICA_FILTER_LINEAR);
    assert(material.mag_filter == GE_PICA_FILTER_LINEAR);
    assert(material.texture_perspective == 1U);
    assert(material.cull_mode == GE_PICA_CULL_BACK);
    assert(material.lighting_enabled == 1U);
    assert(material.smooth_shading == 1U);
    assert(material.fog_enabled == 1U);
    assert(material.fog_multiplier == -32);
    assert(material.fog_offset == 288);
    assert(material.depth_test_enabled == 1U);
    assert(material.depth_write_enabled == 1U);
}

static void test_constants_and_colors(void)
{
    GeGbiRenderState state;
    GePicaMaterial material;
    const CombineCycle unused = direct(CC_COMBINED, AC_COMBINED);

    ge_gbi_state_init(&state);
    state.primitive_color = UINT32_C(0x12345678);
    state.environment_color = UINT32_C(0x90abcdef);
    state.blend_color = UINT32_C(0x01020344);
    state.fog_color = UINT32_C(0xa1b2c3d4);
    set_combine(&state, direct(CC_PRIMITIVE, AC_PRIMITIVE), unused);
    assert(ge_pica_material_translate(&state, &material)
           == GE_PICA_MATERIAL_OK);
    assert(material.fallback_flags == GE_PICA_FALLBACK_NONE);
    assert(material.color_combine == GE_PICA_COMBINE_PRIMITIVE);
    assert(material.alpha_combine == GE_PICA_ALPHA_PRIMITIVE);
    assert(material.primitive_color.red == UINT8_C(0x12));
    assert(material.primitive_color.green == UINT8_C(0x34));
    assert(material.primitive_color.blue == UINT8_C(0x56));
    assert(material.primitive_color.alpha == UINT8_C(0x78));
    assert(material.environment_color.red == UINT8_C(0x90));
    assert(material.environment_color.alpha == UINT8_C(0xef));
    assert(material.fog_color.green == UINT8_C(0xb2));

    set_combine(&state, direct(CC_ENVIRONMENT, AC_ENVIRONMENT), unused);
    assert(ge_pica_material_translate(&state, &material)
           == GE_PICA_MATERIAL_OK);
    assert(material.color_combine == GE_PICA_COMBINE_ENVIRONMENT);
    assert(material.alpha_combine == GE_PICA_ALPHA_ENVIRONMENT);
}

static void test_standard_image_binding(void)
{
    GeGbiRenderState state;
    GePicaMaterial material;
    const CombineCycle unused = direct(CC_COMBINED, AC_COMBINED);

    ge_gbi_state_init(&state);
    state.texture.enabled = 1U;
    state.texture_image_valid = 1U;
    state.active_texture_binding = GE_GBI_TEXTURE_BINDING_IMAGE;
    state.texture_image.address.raw = UINT32_C(0x06001234);
    state.texture_image.width = UINT16_C(64);
    state.texture_image.format = UINT8_C(2);
    state.texture_image.size = UINT8_C(1);
    set_combine(&state,
                modulate(CC_TEXEL0, CC_SHADE, AC_TEXEL0, AC_SHADE),
                unused);

    assert(ge_pica_material_translate(&state, &material)
           == GE_PICA_MATERIAL_OK);
    assert(material.fallback_flags == GE_PICA_FALLBACK_NONE);
    assert(material.texture_source == GE_PICA_TEXTURE_SOURCE_GBI_IMAGE);
    assert(material.texture_image_address == UINT32_C(0x06001234));
    assert(material.texture_image_width == UINT16_C(64));
    assert(material.texture_image_format == UINT8_C(2));
    assert(material.texture_image_size == UINT8_C(1));
}

static void test_two_cycle_approximation(void)
{
    GeGbiRenderState state;
    GePicaMaterial material;

    ge_gbi_state_init(&state);
    state.texture.enabled = 1U;
    state.rare_texture.type = UINT8_C(3);
    state.rare_texture_valid = 1U;
    state.active_texture_binding = GE_GBI_TEXTURE_BINDING_RARE_ID;
    state.other_mode_high = UINT32_C(1) << 20;
    set_combine(&state,
                modulate(CC_TEXEL0, CC_SHADE, AC_TEXEL0, AC_SHADE),
                modulate(CC_COMBINED, CC_SHADE,
                         AC_COMBINED, AC_SHADE));
    assert(ge_pica_material_translate(&state, &material)
           == GE_PICA_MATERIAL_OK);
    assert(material.cycle_type == GE_PICA_CYCLE_TWO);
    assert(material.color_combine
           == GE_PICA_COMBINE_TEXTURE0_MODULATE_SHADE);
    assert(material.alpha_combine
           == GE_PICA_ALPHA_TEXTURE0_MODULATE_SHADE);
    assert(material.fallback_flags == GE_PICA_FALLBACK_TWO_CYCLE);

    set_combine(&state, direct(CC_SHADE, AC_SHADE),
                direct(CC_COMBINED, AC_COMBINED));
    assert(ge_pica_material_translate(&state, &material)
           == GE_PICA_MATERIAL_OK);
    assert(material.color_combine == GE_PICA_COMBINE_SHADE);
    assert(material.alpha_combine == GE_PICA_ALPHA_SHADE);
    assert((material.fallback_flags & GE_PICA_FALLBACK_MISSING_TEXTURE)
           == 0U);
}

static void test_texture_environment_combine(void)
{
    GeGbiRenderState state;
    GePicaMaterial material;
    const CombineCycle texture = direct(CC_TEXEL0, AC_TEXEL0);
    const CombineCycle pass = direct(CC_COMBINED, AC_COMBINED);
    const CombineCycle environment = modulate(CC_TEXEL0, CC_ENVIRONMENT,
        AC_TEXEL0, AC_ENVIRONMENT);
    const CombineCycle combined_environment = modulate(CC_COMBINED,
        CC_ENVIRONMENT, AC_COMBINED, AC_ENVIRONMENT);
    ge_gbi_state_init(&state);
    state.texture.enabled = 1U;
    state.rare_texture_valid = 1U;
    state.active_texture_binding = GE_GBI_TEXTURE_BINDING_RARE_ID;
    set_combine(&state, environment, pass);
    assert(ge_pica_material_translate(&state, &material) == GE_PICA_MATERIAL_OK);
    assert(material.color_combine == GE_PICA_COMBINE_TEXTURE0_MODULATE_ENVIRONMENT);
    assert(material.alpha_combine == GE_PICA_ALPHA_TEXTURE0_MODULATE_ENVIRONMENT);
    assert(!(material.fallback_flags & GE_PICA_FALLBACK_COMBINER));
    state.other_mode_high = UINT32_C(1) << 20;
    assert(ge_pica_material_translate(&state, &material) == GE_PICA_MATERIAL_OK);
    assert(material.color_combine == GE_PICA_COMBINE_TEXTURE0_MODULATE_ENVIRONMENT);
    assert(!(material.fallback_flags & GE_PICA_FALLBACK_COMBINER));
    set_combine(&state, texture, combined_environment);
    assert(ge_pica_material_translate(&state, &material) == GE_PICA_MATERIAL_OK);
    assert(!(material.fallback_flags & GE_PICA_FALLBACK_COMBINER));
    /* A second modulation needs another GPU stage; report the approximation
     * instead of labelling texture * environment * shade as exact. */
    set_combine(&state, environment,
        modulate(CC_COMBINED, CC_SHADE, AC_COMBINED, AC_SHADE));
    assert(ge_pica_material_translate(&state, &material) == GE_PICA_MATERIAL_OK);
    assert(material.fallback_flags & GE_PICA_FALLBACK_COMBINER);
    set_combine(&state, modulate(CC_TEXEL0, CC_SHADE, AC_TEXEL0, AC_SHADE),
        combined_environment);
    assert(ge_pica_material_translate(&state, &material) == GE_PICA_MATERIAL_OK);
    assert(material.fallback_flags & GE_PICA_FALLBACK_COMBINER);
}

static void test_explicit_fallback_signals(void)
{
    GeGbiRenderState state;
    GePicaMaterial material;
    const uint32_t expected = GE_PICA_FALLBACK_COMBINER
        | GE_PICA_FALLBACK_COPY_FILL_CYCLE
        | GE_PICA_FALLBACK_ALPHA_DITHER
        | GE_PICA_FALLBACK_PRIMITIVE_DEPTH
        | GE_PICA_FALLBACK_TEXTURE_DETAIL
        | GE_PICA_FALLBACK_TEXTURE_LOD
        | GE_PICA_FALLBACK_TEXTURE_LUT
        | GE_PICA_FALLBACK_COLOR_DITHER
        | GE_PICA_FALLBACK_CULL_BOTH
        | GE_PICA_FALLBACK_BLENDER
        | GE_PICA_FALLBACK_TEXTURE_AVERAGE
        | GE_PICA_FALLBACK_DETAIL_TEXTURE
        | GE_PICA_FALLBACK_RARE_TEXTURE_TYPE;

    ge_gbi_state_init(&state);
    state.geometry_mode = UINT32_C(0x00003000);
    state.texture.enabled = 1U;
    state.rare_texture.type = UINT8_C(2);
    state.rare_texture_valid = 1U;
    state.active_texture_binding = GE_GBI_TEXTURE_BINDING_RARE_ID;
    state.rare_texture.detail_texture_id = UINT16_C(55);
    state.other_mode_high = (UINT32_C(3) << 20)
        | UINT32_C(0x00060000) | UINT32_C(0x00010000)
        | UINT32_C(0x0000c000) | UINT32_C(0x00003000)
        | UINT32_C(0x000000c0) | UINT32_C(0x00000030);
    state.other_mode_low = UINT32_C(0x00000003) | UINT32_C(0x00000004)
        | UINT32_C(0x00000800) | UINT32_C(0x00004000);
    state.blend_color = UINT32_C(0x0000007f);

    assert(ge_pica_material_translate(&state, &material)
           == GE_PICA_MATERIAL_OK);
    assert(material.fallback_flags == expected);
    assert(material.cycle_type == GE_PICA_CYCLE_FILL);
    assert(material.cull_mode == GE_PICA_CULL_BOTH);
    assert(material.alpha_test == GE_PICA_ALPHA_TEST_THRESHOLD);
    assert(material.alpha_threshold == UINT8_C(0x7f));
    assert(material.depth_mode == GE_PICA_DEPTH_TRANSLUCENT);
    assert(material.blend_enabled == 1U);
}

static GeGbiTextureRectangle test_rectangle(void)
{
    GeGbiTextureRectangle rectangle = {0};

    rectangle.screen.upper_x = 40U;
    rectangle.screen.upper_y = 20U;
    rectangle.screen.lower_x = 80U;
    rectangle.screen.lower_y = 60U;
    rectangle.s = 32;
    rectangle.t = 64;
    rectangle.dsdx = 1024;
    rectangle.dtdy = 512;
    rectangle.tile = 3U;
    return rectangle;
}

static void test_texture_rectangle_translation(void)
{
    GeGbiRenderState state;
    GeGbiTextureRectangle rectangle = test_rectangle();
    GePicaTextureBindingTransform binding = {
        1.0f / 64.0f, -1.0f / 32.0f, 0.0f, 1.0f,
    };
    GePicaTextureRectangleDraw draw;
    GeGbiStateAction action = {0};

    ge_gbi_state_init(&state);
    state.texture.enabled = 1U;
    state.active_texture_binding = GE_GBI_TEXTURE_BINDING_IMAGE;
    state.tiles[3].upper_s = 4U;
    state.tiles[3].upper_t = 8U;
    state.tiles[3].clamp_mirror_s = 2U;
    state.tiles[3].clamp_mirror_t = 1U;
    action.kind = GE_GBI_STATE_ACTION_DRAW_TEXTURE_RECTANGLE;
    action.data.texture_rectangle = rectangle;
    assert(ge_pica_texture_rectangle_translate_action(
               &state, &action, &binding, &draw)
           == GE_PICA_MATERIAL_OK);
    assert(draw.tile == 3U && draw.flipped == 0U);
    assert(draw.material.texture_tile == 3U);
    assert(draw.material.wrap_s == GE_PICA_WRAP_CLAMP);
    assert(draw.material.wrap_t == GE_PICA_WRAP_MIRROR);
    assert(draw.vertices[0].x == 10.0f && draw.vertices[0].y == 5.0f);
    assert(draw.vertices[0].texture_u == 0.0f);
    assert(draw.vertices[0].texture_v == 1.0f);
    assert(draw.vertices[1].x == 20.0f && draw.vertices[1].y == 5.0f);
    assert(draw.vertices[1].texture_u == 10.0f / 64.0f);
    assert(draw.vertices[1].texture_v == 1.0f);
    assert(draw.vertices[2].x == 20.0f && draw.vertices[2].y == 15.0f);
    assert(draw.vertices[2].texture_u == 10.0f / 64.0f);
    assert(draw.vertices[2].texture_v == 1.0f - 5.0f / 32.0f);
    assert(draw.vertices[5].x == 10.0f && draw.vertices[5].y == 15.0f);
    assert(draw.vertices[5].texture_u == 0.0f);
    assert(draw.vertices[5].texture_v == 1.0f - 5.0f / 32.0f);
    assert(draw.vertices[3].x == draw.vertices[0].x);
    assert(draw.vertices[4].texture_u == draw.vertices[2].texture_u);
    assert(draw.vertices[0].red == 1.0f && draw.vertices[0].alpha == 1.0f);
}

static void test_flipped_and_copy_texture_rectangles(void)
{
    GeGbiRenderState state;
    GeGbiTextureRectangle rectangle = test_rectangle();
    const GePicaTextureBindingTransform binding = {
        1.0f / 64.0f, 1.0f / 32.0f, 0.0f, 0.0f,
    };
    GePicaTextureRectangleDraw draw;

    ge_gbi_state_init(&state);
    rectangle.flipped = 1U;
    rectangle.dsdx = 1024;
    rectangle.dtdy = -1024;
    assert(ge_pica_texture_rectangle_translate(
               &state, &rectangle, &binding, &draw)
           == GE_PICA_MATERIAL_OK);
    assert(draw.vertices[1].texture_u == 1.0f / 64.0f);
    assert(draw.vertices[1].texture_v == -8.0f / 32.0f);
    assert(draw.vertices[5].texture_u == 11.0f / 64.0f);
    assert(draw.vertices[5].texture_v == 2.0f / 32.0f);
    assert(draw.vertices[2].texture_u == 11.0f / 64.0f);
    assert(draw.vertices[2].texture_v == -8.0f / 32.0f);

    /* COPY mode consumes four horizontal texels per RDP issue. Its authored
     * 4.0 derivative therefore remains one texel per output pixel. */
    ge_gbi_state_init(&state);
    state.other_mode_high = UINT32_C(2) << 20;
    rectangle = test_rectangle();
    rectangle.dsdx = 4096;
    assert(ge_pica_texture_rectangle_translate(
               &state, &rectangle, &binding, &draw)
           == GE_PICA_MATERIAL_OK);
    assert(draw.material.cycle_type == GE_PICA_CYCLE_COPY);
    assert(draw.vertices[1].texture_u == 11.0f / 64.0f);
}

static void test_texture_rectangle_rejects_unbound_domains(void)
{
    GeGbiRenderState state;
    GeGbiTextureRectangle rectangle = test_rectangle();
    GePicaTextureBindingTransform binding = {
        1.0f, 1.0f, 0.0f, 0.0f,
    };
    GePicaTextureRectangleDraw draw;
    GeGbiStateAction action = {0};

    ge_gbi_state_init(&state);
    assert(ge_pica_texture_rectangle_translate_action(
               &state, &action, &binding, &draw)
           == GE_PICA_MATERIAL_INVALID_ARGUMENT);
    assert(ge_pica_texture_rectangle_translate(
               NULL, &rectangle, &binding, &draw)
           == GE_PICA_MATERIAL_INVALID_ARGUMENT);
    binding.u_scale = 0.0f;
    assert(ge_pica_texture_rectangle_translate(
               &state, &rectangle, &binding, &draw)
           == GE_PICA_MATERIAL_INVALID_ARGUMENT);
    binding.u_scale = 1.0f;
    state.tiles[3].shift_s = 1U;
    assert(ge_pica_texture_rectangle_translate(
               &state, &rectangle, &binding, &draw)
           == GE_PICA_MATERIAL_INVALID_ARGUMENT);
}

static void test_coverage_alpha_endpoints(void)
{
    GeGbiRenderState state;
    GePicaMaterial material;
    ge_gbi_state_init(&state);
    /* SDK AA_ZB_TEX_EDGE: opaque, coverage times alpha, alpha from coverage,
     * compare/write Z, no explicit alpha compare. */
    state.geometry_mode = 1U;
    state.other_mode_low = 0x00003078U;
    assert(ge_pica_material_translate(&state, &material) == GE_PICA_MATERIAL_OK);
    assert(material.alpha_test == GE_PICA_ALPHA_TEST_THRESHOLD);
    assert(material.alpha_threshold == 0U);
    assert(material.depth_test_enabled && material.depth_write_enabled);
    assert(material.fallback_flags & GE_PICA_FALLBACK_COVERAGE);
    /* Preserve every explicit authored threshold, including both endpoints. */
    state.other_mode_low |= 1U;
    for (unsigned threshold = 0U; threshold <= 255U; ++threshold) {
        state.blend_color = threshold;
        assert(ge_pica_material_translate(&state, &material) == GE_PICA_MATERIAL_OK);
        assert(material.alpha_threshold == threshold);
    }
    state.other_mode_low = 0U;
    assert(ge_pica_material_translate(&state, &material) == GE_PICA_MATERIAL_OK);
    assert(material.alpha_test == GE_PICA_ALPHA_TEST_DISABLED);
}

int main(void)
{
    test_coverage_alpha_endpoints();
    test_invalid_arguments();
    test_default_is_auditable_fallback();
    test_textured_lit_material();
    test_constants_and_colors();
    test_standard_image_binding();
    test_two_cycle_approximation();
    test_texture_environment_combine();
    test_explicit_fallback_signals();
    test_texture_rectangle_translation();
    test_flipped_and_copy_texture_rectangles();
    test_texture_rectangle_rejects_unbound_domains();
    puts("PICA material translation tests passed");
    return 0;
}
