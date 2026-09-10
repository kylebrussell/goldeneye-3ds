#ifndef GE_ORIGINAL_BG_VISIBILITY_STATE_H
#define GE_ORIGINAL_BG_VISIBILITY_STATE_H
#include "ge_original_bg_visibility.h"

/* Read-only adapter boundary. Pointer identities are checked internally and
 * never emitted as values. These arrays copy the successful canonical pass;
 * the renderer interpreter neither publishes nor reads this bridge. */
typedef struct GeOriginalBgVisibilityStateSource {
    uint32_t room_count, portal_count, command_count;
    uint64_t stream_hash;
    float player_position[3], world_to_screen[16], projection[6], screen_box[4];
    float clip_range[2], projection_offsets[2];
    int16_t viewport[4];
    uint8_t command_types[512], command_lengths[512];
    int32_t command_arguments[512];
} GeOriginalBgVisibilityStateSource;

#ifndef GE_RENDERER_BG_VISIBILITY_NAMESPACE_H
void ge_original_bg_visibility_state_begin(void);
void ge_original_bg_visibility_state_publish(const GeOriginalBgVisibilityInput *);
void ge_original_bg_visibility_state_retire(const GeOriginalBgVisibilityProgram *);
int ge_original_bg_visibility_state_read(const GeOriginalBgVisibilityProgram *,
    const uint8_t *background, size_t background_size, GeOriginalBgVisibilityStateSource *);
#endif
#endif
