#include "ge_texture_uv.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int close_float(float left, float right)
{
    return fabsf(left - right) < 0.00001f;
}

int main(void)
{
    GePicaMaterial material;
    GeTextureUv uv;
    GeTextureUvContext context;

    memset(&material, 0, sizeof(material));
    material.texture_scale_s = UINT16_MAX;
    material.texture_scale_t = UINT16_MAX;
    assert(ge_texture_uv_normalize(INT16_C(2048), INT16_C(1024),
                                   &material, 64U, 32U, &uv)
           == GE_TEXTURE_UV_OK);
    assert(close_float(uv.u, 65535.0f / 65536.0f));
    assert(close_float(uv.v, 65535.0f / 65536.0f));

    /* Dam texture 949 is TEXTURETYPE_LOD with shift 15.  The decompiled
     * texLoadFromGdl applies that shift to detail tile 0, while the primary
     * image bound by this adapter is the unshifted tile-1 mip chain. */
    material.texture_type = 0U;
    material.texture_shift_s = 15U;
    material.texture_shift_t = 15U;
    assert(ge_texture_uv_normalize(INT16_C(1024), INT16_C(-512),
                                   &material, 64U, 64U, &uv)
           == GE_TEXTURE_UV_OK);
    assert(close_float(uv.u, 0.5f * 65535.0f / 65536.0f));
    assert(close_float(uv.v, -0.25f * 65535.0f / 65536.0f));

    /* The MIPMAP and TILE constructors ignore the encoded shift fields too. */
    material.texture_type = 2U;
    material.texture_shift_s = 2U;
    material.texture_shift_t = 10U;
    assert(ge_texture_uv_normalize(INT16_C(2048), INT16_C(2048),
                                   &material, 64U, 64U, &uv)
           == GE_TEXTURE_UV_OK);
    assert(close_float(uv.u, 65535.0f / 65536.0f));
    assert(close_float(uv.v, 65535.0f / 65536.0f));

    assert(ge_texture_uv_normalize(0, 0, &material, 0U, 32U, &uv)
           == GE_TEXTURE_UV_INVALID_ARGUMENT);
    assert(ge_texture_uv_normalize(0, 0, NULL, 32U, 32U, &uv)
           == GE_TEXTURE_UV_INVALID_ARGUMENT);
    material.texture_scale_s = 0x0d80U; /* ROM PwindowZ: 54 << 6. */
    material.texture_scale_t = 32U << 6;
    assert(ge_texture_uv_prepare(&material, 64U, 32U, &context) == GE_TEXTURE_UV_OK);
    assert(ge_texture_uv_generated_prepared(1.0f, 0.5f, &context, &uv) == GE_TEXTURE_UV_OK);
    assert(uv.u == 54.0f / 64.0f && uv.v == 0.5f);
    assert(ge_texture_uv_generated_prepared(0.0f, 0.0f, &context, &uv) == GE_TEXTURE_UV_OK);
    assert(uv.u == 0.0f && uv.v == 0.0f);
    assert(ge_texture_uv_generated_prepared(NAN, 0.0f, &context, &uv)
        == GE_TEXTURE_UV_INVALID_ARGUMENT);
    puts("N64 authored texture-coordinate conversion tests passed");
    return 0;
}
