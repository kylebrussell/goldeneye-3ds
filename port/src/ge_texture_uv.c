#include "ge_texture_uv.h"

#include <stddef.h>
#include <math.h>

GeTextureUvStatus ge_texture_uv_generated_prepared(float generated_s,
    float generated_t, const GeTextureUvContext *context, GeTextureUv *result)
{
    if (context == NULL || !context->valid || result == NULL
            || !isfinite(generated_s) || !isfinite(generated_t))
        return GE_TEXTURE_UV_INVALID_ARGUMENT;
    result->u = generated_s * context->scale_s / 64.0f / context->width;
    result->v = generated_t * context->scale_t / 64.0f / context->height;
    return GE_TEXTURE_UV_OK;
}

GeTextureUvStatus ge_texture_uv_prepare(const GePicaMaterial *material,
    uint32_t texture_width, uint32_t texture_height, GeTextureUvContext *context)
{
    if (context == NULL) return GE_TEXTURE_UV_INVALID_ARGUMENT;
    context->valid = 0U;
    if (material == NULL || texture_width == 0U || texture_height == 0U
            || material->texture_shift_s > 15U || material->texture_shift_t > 15U)
        return GE_TEXTURE_UV_INVALID_ARGUMENT;
    context->scale_s = (float)material->texture_scale_s;
    context->scale_t = (float)material->texture_scale_t;
    context->width = (float)texture_width;
    context->height = (float)texture_height;
    context->valid = 1U;
    return GE_TEXTURE_UV_OK;
}

static void ge_texture_uv_normalize_values(int16_t texture_s, int16_t texture_t,
    float scale_s, float scale_t, float width, float height, GeTextureUv *result)
{
    /* Preserve scalar grouping and division. Combining scales or replacing
     * arbitrary image dimensions with reciprocals can change UV bits. */
    result->u = (float)texture_s * (1.0f / 32.0f)
        * scale_s * (1.0f / 65536.0f) / width;
    result->v = (float)texture_t * (1.0f / 32.0f)
        * scale_t * (1.0f / 65536.0f) / height;
}

GeTextureUvStatus ge_texture_uv_normalize_prepared(int16_t texture_s,
    int16_t texture_t, const GeTextureUvContext *context, GeTextureUv *result)
{
    if (context == NULL || !context->valid || result == NULL)
        return GE_TEXTURE_UV_INVALID_ARGUMENT;
    ge_texture_uv_normalize_values(texture_s, texture_t,
        context->scale_s, context->scale_t, context->width, context->height, result);
    return GE_TEXTURE_UV_OK;
}

GeTextureUvStatus ge_texture_uv_normalize(int16_t texture_s,
                                          int16_t texture_t,
                                          const GePicaMaterial *material,
                                          uint32_t texture_width,
                                          uint32_t texture_height,
                                          GeTextureUv *result)
{
    if (material == NULL || result == NULL || texture_width == 0U
            || texture_height == 0U || material->texture_shift_s > 15U
            || material->texture_shift_t > 15U) {
        return GE_TEXTURE_UV_INVALID_ARGUMENT;
    }
    /* texLoadFromGdl applies the command's shifts only to tile 0 created by
     * texWriteTileFromDefinition for TEXTURETYPE_LOD/DETAIL.  The primary
     * image is also installed unshifted in tile 1 as the base of its mip
     * chain; MIPMAP/TILE paths ignore these fields entirely.  This adapter
     * binds that primary/base image. Applying tile 0's detail shift here made
     * Dam's shift-15 canyon rock sample at twice its authored base frequency
     * over the entire view instead of only in the N64 detail blend. */
    ge_texture_uv_normalize_values(texture_s, texture_t,
        (float)material->texture_scale_s, (float)material->texture_scale_t,
        (float)texture_width, (float)texture_height, result);
    return GE_TEXTURE_UV_OK;
}
