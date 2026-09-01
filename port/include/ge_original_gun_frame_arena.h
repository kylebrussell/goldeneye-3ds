#ifndef GE_ORIGINAL_GUN_FRAME_ARENA_H
#define GE_ORIGINAL_GUN_FRAME_ARENA_H

#include <stddef.h>
#include <stdint.h>

typedef struct GeOriginalDynFrameAudit {
    uint64_t generation;
    size_t capacity;
    size_t used;
    int active;
    int within_bounds;
} GeOriginalDynFrameAudit;

/* Publishes the current frame's platform-owned scratch buffer to the exact
 * dynAllocate bump allocator used by gunUpdateAndFire. */
int ge_original_gun_frame_arena_begin(void *storage, size_t storage_size);
size_t ge_original_gun_frame_arena_used(void);
int ge_original_gun_frame_arena_active(void);
int ge_original_gun_frame_arena_audit(GeOriginalDynFrameAudit *audit);
int ge_original_gun_frame_arena_finalize(GeOriginalDynFrameAudit *audit);

/* Typed live-player publication consumed by the exact gun body.  This avoids
 * aliasing the bounded camera producer's compact player ABI. */
void *ge_original_gun_current_player_view_to_world(void);
void *ge_original_gun_host_model(unsigned hand);
int32_t *ge_original_gun_host_model_rwdata(unsigned hand);
void ge_original_gun_host_model_set_render_pos(unsigned hand, void *matrices);
/* Read-only counterpart used after the exact gun body has finished.  Unlike
 * ge_original_gun_host_model this does not clear the canonical model state. */
void *ge_original_gun_host_model_peek(unsigned hand);

#endif
