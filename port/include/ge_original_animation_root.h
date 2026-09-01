#ifndef GE_ORIGINAL_ANIMATION_ROOT_H
#define GE_ORIGINAL_ANIMATION_ROOT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum GeOriginalBondAnimationId {
    GE_ORIGINAL_BOND_ANIMATION_SPRINTING = 0,
    GE_ORIGINAL_BOND_ANIMATION_EYE_WALK = 1,
    GE_ORIGINAL_BOND_ANIMATION_IDLE = 2
} GeOriginalBondAnimationId;

typedef struct GeOriginalAnimationRoot GeOriginalAnimationRoot;

/* Materializes the native pointer-bearing ABI from the exact big-endian
 * animation_data segment. The caller retains ownership of segment_bytes. */
GeOriginalAnimationRoot *ge_original_animation_root_create(
    const uint8_t *segment_bytes,
    size_t segment_size,
    GeOriginalBondAnimationId animation_id);
void ge_original_animation_root_destroy(GeOriginalAnimationRoot *root);

uint16_t ge_original_animation_root_frame_count(
    const GeOriginalAnimationRoot *root);
int ge_original_animation_root_decode(const GeOriginalAnimationRoot *root,
                                      uint16_t frame,
                                      int flip,
                                      float position[3],
                                      float *angle_radians);
/* Internal native ABI handle for exact decompiled model animation functions. */
void *ge_original_animation_root_native_abi(
    const GeOriginalAnimationRoot *root);
/* Binds the exact per-frame entry bytes extracted from the original ROM. */
int ge_original_animation_root_bind_frames(GeOriginalAnimationRoot *root,
                                           const uint8_t *frame_bytes,
                                           size_t frame_size);
const uint8_t *ge_original_animation_root_frame_data(
    const GeOriginalAnimationRoot *root,
    uint16_t frame,
    size_t *frame_size);

/* Typed bridge for GeOriginalBondMovementProviders. An exact upstream model
 * step must select each frame; this adapter deliberately does not invent an
 * animation clock or controller. Root translation uses Bond's original 0.1
 * model scale. */
typedef struct GeOriginalAnimationRootSample {
    const GeOriginalAnimationRoot *root;
    uint16_t frame;
    int flip;
    int valid;
} GeOriginalAnimationRootSample;

void ge_original_animation_root_sample_set(
    GeOriginalAnimationRootSample *sample,
    const GeOriginalAnimationRoot *root,
    uint16_t frame,
    int flip);
int ge_original_animation_root_sample_velocity(void *context,
                                               float speed_forwards,
                                               float speed_sideways,
                                               int32_t clock_timer,
                                               float global_timer_delta,
                                               float velocity[3]);

#ifdef __cplusplus
}
#endif
#endif
