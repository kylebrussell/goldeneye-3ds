#include "ge_3ds_material.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

static GPU_TEVSRC ge_3ds_material_source(GePicaApplySource source)
{
    switch (source) {
    case GE_PICA_APPLY_SOURCE_TEXTURE0:
        return GPU_TEXTURE0;
    case GE_PICA_APPLY_SOURCE_CONSTANT:
        return GPU_CONSTANT;
    case GE_PICA_APPLY_SOURCE_PRIMARY:
    default:
        return GPU_PRIMARY_COLOR;
    }
}

static GPU_COMBINEFUNC ge_3ds_material_combine(GePicaApplyCombine combine)
{
    return combine == GE_PICA_APPLY_MODULATE ? GPU_MODULATE : GPU_REPLACE;
}

static GPU_TEXTURE_WRAP_PARAM ge_3ds_material_wrap(GePicaTextureWrap wrap)
{
    switch (wrap) {
    case GE_PICA_WRAP_REPEAT:
        return GPU_REPEAT;
    case GE_PICA_WRAP_MIRROR:
        return GPU_MIRRORED_REPEAT;
    case GE_PICA_WRAP_CLAMP:
    default:
        return GPU_CLAMP_TO_EDGE;
    }
}

static GPU_TEXTURE_FILTER_PARAM ge_3ds_material_filter(
    GePicaTextureFilter filter)
{
    return filter == GE_PICA_FILTER_LINEAR ? GPU_LINEAR : GPU_NEAREST;
}

static GPU_CULLMODE ge_3ds_material_cull(GePicaApplyCull cull)
{
    switch (cull) {
    case GE_PICA_APPLY_CULL_FRONT:
        return GPU_CULL_FRONT_CCW;
    case GE_PICA_APPLY_CULL_BACK:
        return GPU_CULL_BACK_CCW;
    case GE_PICA_APPLY_CULL_NONE:
    default:
        return GPU_CULL_NONE;
    }
}

static uint32_t ge_3ds_material_color(GePicaColor color)
{
    return (uint32_t)color.red | ((uint32_t)color.green << 8)
        | ((uint32_t)color.blue << 16) | ((uint32_t)color.alpha << 24);
}

static void ge_3ds_material_replace_texture_source(
    GePicaApplyChannel *channel,
    GePicaApplySource replacement)
{
    if (channel->source0 == GE_PICA_APPLY_SOURCE_TEXTURE0) {
        channel->source0 = replacement;
    }
    if (channel->source1 == GE_PICA_APPLY_SOURCE_TEXTURE0) {
        channel->source1 = replacement;
    }
}

static void ge_3ds_material_apply_texenv(const GePicaApplyState *state)
{
    C3D_TexEnv *environment = C3D_GetTexEnv(0);

    C3D_TexEnvInit(environment);
    C3D_TexEnvSrc(environment, C3D_RGB,
        ge_3ds_material_source(state->color.source0),
        ge_3ds_material_source(state->color.source1),
        GPU_PRIMARY_COLOR);
    C3D_TexEnvSrc(environment, C3D_Alpha,
        ge_3ds_material_source(state->alpha.source0),
        ge_3ds_material_source(state->alpha.source1),
        GPU_PRIMARY_COLOR);
    C3D_TexEnvFunc(environment, C3D_RGB,
        ge_3ds_material_combine(state->color.combine));
    C3D_TexEnvFunc(environment, C3D_Alpha,
        ge_3ds_material_combine(state->alpha.combine));
    C3D_TexEnvColor(environment,
        ge_3ds_material_color(state->constant_color));
}

Ge3dsMaterialStatus ge_3ds_material_prepare(
    const GePicaMaterial *material,
    const Ge3dsMaterialBinding *binding,
    Ge3dsMaterialResult *result)
{
    C3D_Tex *texture = binding != NULL ? binding->texture0 : NULL;

    if (material == NULL || result == NULL
            || ge_pica_apply_compile(material, &result->state)
                != GE_PICA_APPLY_OK) {
        return GE_3DS_MATERIAL_INVALID_ARGUMENT;
    }
    if (binding != NULL
            && binding->missing_texture_fallback
                != GE_3DS_MATERIAL_TEXTURE_FALLBACK_SHADE
            && binding->missing_texture_fallback
                != GE_3DS_MATERIAL_TEXTURE_FALLBACK_REPLACE) {
        result->state.apply_fallback_flags
            |= GE_PICA_APPLY_FALLBACK_INVALID_ENUM;
    }
    result->texture_bound = UINT8_C(0);
    if (result->state.texture_required != 0U && texture != NULL) {
        result->texture_bound = UINT8_C(1);

        if ((material->fallback_flags & GE_PICA_FALLBACK_MISSING_TEXTURE) != 0U
                && binding->missing_texture_fallback
                    == GE_3DS_MATERIAL_TEXTURE_FALLBACK_REPLACE) {
            result->state.color.source0 = GE_PICA_APPLY_SOURCE_TEXTURE0;
            result->state.color.source1 = GE_PICA_APPLY_SOURCE_PRIMARY;
            result->state.color.combine = GE_PICA_APPLY_REPLACE;
            result->state.alpha.source0 = GE_PICA_APPLY_SOURCE_TEXTURE0;
            result->state.alpha.source1 = GE_PICA_APPLY_SOURCE_PRIMARY;
            result->state.alpha.combine = GE_PICA_APPLY_REPLACE;
            result->state.apply_fallback_flags
                |= GE_PICA_APPLY_FALLBACK_VISIBILITY_OVERRIDE;
        }
    } else if (result->state.texture_required != 0U) {
        ge_3ds_material_replace_texture_source(&result->state.color,
            GE_PICA_APPLY_SOURCE_PRIMARY);
        ge_3ds_material_replace_texture_source(&result->state.alpha,
            GE_PICA_APPLY_SOURCE_PRIMARY);
        result->state.apply_fallback_flags
            |= GE_PICA_APPLY_FALLBACK_TEXTURE_UNBOUND;
    }
    if ((material->fallback_flags & GE_PICA_FALLBACK_MISSING_TEXTURE) != 0U
            && texture == NULL && binding != NULL
            && binding->missing_texture_fallback
                == GE_3DS_MATERIAL_TEXTURE_FALLBACK_SHADE) {
        /* A decoded material can retain a valid constant/zero combine even
         * though its texture reference was not recoverable. For bring-up,
         * force vertex shade so geometry remains visible and record the
         * deliberate departure from the original material. */
        result->state.color.source0 = GE_PICA_APPLY_SOURCE_PRIMARY;
        result->state.color.source1 = GE_PICA_APPLY_SOURCE_PRIMARY;
        result->state.color.combine = GE_PICA_APPLY_REPLACE;
        result->state.alpha.source0 = GE_PICA_APPLY_SOURCE_PRIMARY;
        result->state.alpha.source1 = GE_PICA_APPLY_SOURCE_PRIMARY;
        result->state.alpha.combine = GE_PICA_APPLY_REPLACE;
        result->state.apply_fallback_flags
            |= GE_PICA_APPLY_FALLBACK_VISIBILITY_OVERRIDE;
    }

    return GE_3DS_MATERIAL_OK;
}

Ge3dsMaterialStatus ge_3ds_material_apply_prepared(
    const Ge3dsMaterialResult *prepared,
    const Ge3dsMaterialBinding *binding)
{
    return ge_3ds_material_apply_prepared_delta(
        prepared, binding, NULL, NULL);
}

Ge3dsMaterialStatus ge_3ds_material_apply_prepared_delta(
    const Ge3dsMaterialResult *prepared,
    const Ge3dsMaterialBinding *binding,
    const Ge3dsMaterialResult *previous,
    const Ge3dsMaterialBinding *previous_binding)
{
    C3D_Tex *texture = binding != NULL ? binding->texture0 : NULL;
    C3D_Tex *previous_texture = previous_binding != NULL
        ? previous_binding->texture0 : NULL;
    GPU_WRITEMASK write_mask;
    bool depth_stage_enabled;
    bool texture_parameters_changed;
    bool texenv_changed;
    bool depth_changed;

    if (prepared == NULL
            || (prepared->texture_bound != 0U && texture == NULL))
        return GE_3DS_MATERIAL_INVALID_ARGUMENT;
    texture_parameters_changed = previous == NULL
        || previous->texture_bound == 0U
        || previous_texture != texture
        || previous->state.wrap_s != prepared->state.wrap_s
        || previous->state.wrap_t != prepared->state.wrap_t
        || previous->state.mag_filter != prepared->state.mag_filter
        || previous->state.min_filter != prepared->state.min_filter;
    if (prepared->texture_bound != 0U && texture_parameters_changed) {
        C3D_TexSetWrap(texture, ge_3ds_material_wrap(prepared->state.wrap_s),
            ge_3ds_material_wrap(prepared->state.wrap_t));
        C3D_TexSetFilter(texture,
            ge_3ds_material_filter(prepared->state.mag_filter),
            ge_3ds_material_filter(prepared->state.min_filter));
        if (texture->maxLevel > 0U) {
            /* Tex3DS supplies prefiltered levels for the 3DS GPU. GoldenEye's
             * authored minification state selects the within-level filter;
             * blend adjacent levels to prevent the high-frequency Dam I4/I8
             * surfaces from collapsing into interference bands. */
            C3D_TexSetFilterMipmap(texture, GPU_LINEAR);
        }
    }
    if (prepared->texture_bound != 0U
            && (previous == NULL || previous->texture_bound == 0U
                || previous_texture != texture)) {
        C3D_TexBind(0, texture);
    }

    texenv_changed = previous == NULL
        || memcmp(&previous->state.color, &prepared->state.color,
                  sizeof(prepared->state.color)) != 0
        || memcmp(&previous->state.alpha, &prepared->state.alpha,
                  sizeof(prepared->state.alpha)) != 0
        || memcmp(&previous->state.constant_color,
                  &prepared->state.constant_color,
                  sizeof(prepared->state.constant_color)) != 0;
    if (texenv_changed)
        ge_3ds_material_apply_texenv(&prepared->state);
    if (previous == NULL || previous->state.cull != prepared->state.cull)
        C3D_CullFace(ge_3ds_material_cull(prepared->state.cull));

    depth_changed = previous == NULL
        || previous->state.depth_test_enabled
            != prepared->state.depth_test_enabled
        || previous->state.depth_write_enabled
            != prepared->state.depth_write_enabled;
    if (depth_changed) {
        write_mask = GPU_WRITE_COLOR;
        if (prepared->state.depth_write_enabled != 0U) {
            write_mask = (GPU_WRITEMASK)(write_mask | GPU_WRITE_DEPTH);
        }
        depth_stage_enabled = prepared->state.depth_test_enabled != 0U
            || prepared->state.depth_write_enabled != 0U;
        C3D_DepthTest(depth_stage_enabled,
            prepared->state.depth_test_enabled != 0U
                ? GPU_GREATER : GPU_ALWAYS,
            write_mask);
    }
    if (previous == NULL
            || previous->state.alpha_test_enabled
                != prepared->state.alpha_test_enabled
            || previous->state.alpha_threshold
                != prepared->state.alpha_threshold)
        C3D_AlphaTest(prepared->state.alpha_test_enabled != 0U,
            GPU_GREATER, prepared->state.alpha_threshold);

    if (previous == NULL
            || previous->state.blend_enabled
                != prepared->state.blend_enabled) {
        if (prepared->state.blend_enabled != 0U) {
            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
                GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ZERO);
        } else {
            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
                GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
        }
    }
    return GE_3DS_MATERIAL_OK;
}

Ge3dsMaterialStatus ge_3ds_material_apply(
    const GePicaMaterial *material,
    const Ge3dsMaterialBinding *binding,
    Ge3dsMaterialResult *result)
{
    if (ge_3ds_material_prepare(material, binding, result)
            != GE_3DS_MATERIAL_OK)
        return GE_3DS_MATERIAL_INVALID_ARGUMENT;
    return ge_3ds_material_apply_prepared(result, binding);
}

Ge3dsMaterialStatus ge_3ds_texture_rectangle_submit(
    const GeGbiRenderState *state,
    const GeGbiStateAction *action,
    const GePicaTextureBindingTransform *coordinate_binding,
    const Ge3dsMaterialBinding *material_binding,
    GePicaScreenVertex *vertex_destination,
    size_t vertex_capacity,
    int first_vertex,
    Ge3dsTextureRectangleSubmission *submission)
{
    if (state == NULL || action == NULL || coordinate_binding == NULL
            || material_binding == NULL || vertex_destination == NULL
            || submission == NULL || first_vertex < 0
            || vertex_capacity < GE_3DS_TEXTURE_RECTANGLE_VERTEX_COUNT)
        return GE_3DS_MATERIAL_INVALID_ARGUMENT;
    memset(submission, 0, sizeof(*submission));
    if (ge_pica_texture_rectangle_translate_action(
            state, action, coordinate_binding, &submission->draw)
            != GE_PICA_MATERIAL_OK)
        return GE_3DS_MATERIAL_INVALID_ARGUMENT;
    memcpy(vertex_destination, submission->draw.vertices,
           sizeof(submission->draw.vertices));
    GSPGPU_FlushDataCache(vertex_destination,
                          sizeof(submission->draw.vertices));
    if (ge_3ds_material_apply(&submission->draw.material,
                              material_binding,
                              &submission->material)
            != GE_3DS_MATERIAL_OK)
        return GE_3DS_MATERIAL_INVALID_ARGUMENT;
    if (submission->material.state.draw_enabled != 0U) {
        C3D_DrawArrays(GPU_TRIANGLES, first_vertex,
                       GE_3DS_TEXTURE_RECTANGLE_VERTEX_COUNT);
        submission->draw_submitted = 1U;
    }
    return GE_3DS_MATERIAL_OK;
}
