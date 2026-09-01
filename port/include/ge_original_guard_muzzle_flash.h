#ifndef GE_ORIGINAL_GUARD_MUZZLE_FLASH_H
#define GE_ORIGINAL_GUARD_MUZZLE_FLASH_H

#include "ge_original_pitem_models.h"

#include <stdint.h>

typedef struct GeOriginalGuardMuzzleFlashVertex {
    float position[3];
    int16_t texture_s;
    int16_t texture_t;
} GeOriginalGuardMuzzleFlashVertex;

typedef struct GeOriginalGuardMuzzleFlashPublication {
    GeOriginalPitemModelGunfire gunfire;
    GeOriginalGuardMuzzleFlashVertex vertices[6];
} GeOriginalGuardMuzzleFlashPublication;

/* Portable dogfnegx geometry boundary.  render_pos is the already-computed
 * eye-space canonical model matrix.  The caller supplies the two unchanged
 * randomGetNext results and coss/sins values so this helper neither invents
 * renderer RNG nor approximates the N64 angle table. */
int ge_original_guard_muzzle_flash_build(
    const GeOriginalPitemModelGunfire *gunfire,
    const float render_pos[4][4], float model_scale,
    uint32_t scale_random, int16_t uv_cosine, int16_t uv_sine,
    GeOriginalGuardMuzzleFlashPublication *publication);

#endif
