#ifndef GE_ORIGINAL_MULTIPLAYER_FOG_STATE_H
#define GE_ORIGINAL_MULTIPLAYER_FOG_STATE_H
#include <stdint.h>

#define GE_ORIGINAL_FOG_MAX_ROWS 64U

/* Typed scalar sequences in native table/field order. No ABI padding, address
 * or graphics output enters this owner view. See NativeFogState.md for paths. */
typedef struct GeOriginalMultiplayerFogState {
    int32_t level_id, enabled, current_fog[2], bindings[4];
    uint32_t player_count, environment_count, fogless_count;
    uint8_t cinematic, current_sky[5];
    int16_t current_images[2];
    float visibility_scale, zrange[2], current_parameters[9];
    float details[4], intensities[4], transition_near[3];
    uint32_t environment_u32[GE_ORIGINAL_FOG_MAX_ROWS*2];
    int32_t environment_i32[GE_ORIGINAL_FOG_MAX_ROWS*2];
    uint8_t environment_u8[GE_ORIGINAL_FOG_MAX_ROWS*5];
    int16_t environment_i16[GE_ORIGINAL_FOG_MAX_ROWS*2];
    float environment_f32[GE_ORIGINAL_FOG_MAX_ROWS*15];
    uint32_t fogless_ids[GE_ORIGINAL_FOG_MAX_ROWS];
    uint8_t fogless_u8[GE_ORIGINAL_FOG_MAX_ROWS*5];
    int16_t fogless_i16[GE_ORIGINAL_FOG_MAX_ROWS*2];
    float fogless_f32[GE_ORIGINAL_FOG_MAX_ROWS*9];
} GeOriginalMultiplayerFogState;

/* Zero means closed. Lifetime is ownership evidence, never canonical data. */
uint64_t ge_original_multiplayer_fog_lifetime(void);
/* Read-only; invalid/unowned references are rejected before dereference.
 * Bindings are found/main/alternate/near: -1 null, table index otherwise;
 * near alone may be -2 for the native transition record's Nfd member. */
int ge_original_multiplayer_fog_state_read(GeOriginalMultiplayerFogState *);
int ge_original_multiplayer_fog_state_read_exact(GeOriginalMultiplayerFogState *);
#endif
