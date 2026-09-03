#ifndef GE_TEXTURE_UV_H
#define GE_TEXTURE_UV_H

#include "ge_pica_material.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum GeTextureUvStatus {
    GE_TEXTURE_UV_OK = 0,
    GE_TEXTURE_UV_INVALID_ARGUMENT
} GeTextureUvStatus;

typedef struct GeTextureUv {
    float u;
    float v;
} GeTextureUv;

/* Validated snapshot for one batch. No borrowed material/texture pointers;
 * rebuild after a material or image change. Do not modify prepared fields. */
typedef struct GeTextureUvContext {
    float scale_s, scale_t;
    float width, height;
    uint8_t valid;
} GeTextureUvContext;

GeTextureUvStatus ge_texture_uv_prepare(const GePicaMaterial *material,
    uint32_t texture_width, uint32_t texture_height, GeTextureUvContext *context);
GeTextureUvStatus ge_texture_uv_normalize_prepared(int16_t texture_s,
    int16_t texture_t, const GeTextureUvContext *context, GeTextureUv *result);

/* Converts an N64 s10.5 vertex coordinate into normalized source-image UVs.
 * Mirrors gsSPTexture's unsigned 0.16 scale for the unshifted primary/base
 * image bound by this adapter (not tile 0's detail shift). */
GeTextureUvStatus ge_texture_uv_normalize(int16_t texture_s,
                                          int16_t texture_t,
                                          const GePicaMaterial *material,
                                          uint32_t texture_width,
                                          uint32_t texture_height,
                                          GeTextureUv *result);

#ifdef __cplusplus
}
#endif

#endif
