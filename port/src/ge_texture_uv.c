#include "ge_texture_uv.h"

#include <stddef.h>

GeTextureUvStatus ge_texture_uv_normalize(int16_t texture_s,
                                          int16_t texture_t,
                                          const GePicaMaterial *material,
                                          uint32_t texture_width,
                                          uint32_t texture_height,
                                          GeTextureUv *result)
{
    const float n64_subtexel = 1.0f / 32.0f;
    const float rsp_scale = 1.0f / 65536.0f;

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
    result->u = (float)texture_s * n64_subtexel
        * (float)material->texture_scale_s * rsp_scale
        / (float)texture_width;
    result->v = (float)texture_t * n64_subtexel
        * (float)material->texture_scale_t * rsp_scale
        / (float)texture_height;
    return GE_TEXTURE_UV_OK;
}
