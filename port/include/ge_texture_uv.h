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

/* Converts an N64 s10.5 vertex coordinate into normalized source-image UVs.
 * This mirrors gsSPTexture's unsigned 0.16 scale and the RDP tile-shift rule:
 * shifts 0..10 divide by 2^n, while 11..15 multiply by 2^(16-n). */
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
