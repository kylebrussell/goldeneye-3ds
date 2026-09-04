#ifndef GE_ORIGINAL_BG_VISIBILITY_H
#define GE_ORIGINAL_BG_VISIBILITY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GE_ORIGINAL_BG_VISIBILITY_MAX_ROOMS 139U
#define GE_ORIGINAL_BG_VISIBILITY_MAX_VISIBLE 204U

typedef struct GeOriginalBgRoomBounds {
    float minimum[3];
    float maximum[3];
} GeOriginalBgRoomBounds;

typedef struct GeOriginalBgVisibilityProgram GeOriginalBgVisibilityProgram;

/* Mirrors the two pieces of mutable state consumed by the original global-vis
 * interpreter. portal_controls is the authoritative current controlbytes1
 * array supplied by the eventual door system. preload_room returns nonzero
 * only when the original room loader actually begins a load. */
typedef uint8_t (*GeOriginalBgPreloadRoom)(void *context, uint8_t room);

typedef struct GeOriginalBgVisibilityProviders {
    void *context;
    GeOriginalBgPreloadRoom preload_room;
    const uint8_t *portal_controls;
    size_t portal_control_count;
} GeOriginalBgVisibilityProviders;

typedef struct GeOriginalBgVisibilityInput {
    const uint8_t *background;
    size_t background_size;
    const GeOriginalBgRoomBounds *room_bounds;
    size_t room_count;
    int32_t level_index;
    uint8_t current_room;
    float player_position[3];
    float world_to_screen[4][4];
    float level_scale;
    float visibility_scale;
    float near_distance;
    float far_distance;
    float vertical_fov_degrees;
    float aspect_ratio;
    int16_t view_left;
    int16_t view_top;
    int16_t view_width;
    int16_t view_height;
    const GeOriginalBgVisibilityProviders *providers;
    /* Optional relocated immutable portal geometry and global-vis stream. Its background must
     * remain unchanged and alive until the program is closed. Door controls,
     * cameras, room flags and original interpreter execution are never cached. */
    const GeOriginalBgVisibilityProgram *program;
} GeOriginalBgVisibilityInput;

typedef struct GeOriginalBgVisibleRoom {
    uint8_t room;
    uint8_t portal_depth;
    uint8_t special;
    uint8_t reserved;
    float screen_box[4];
} GeOriginalBgVisibleRoom;

typedef struct GeOriginalBgVisibilityResult {
    GeOriginalBgVisibleRoom rooms[GE_ORIGINAL_BG_VISIBILITY_MAX_VISIBLE];
    size_t room_count;
    size_t portal_count;
    uint32_t portal_descents;
    size_t global_command_count;
    uint64_t global_stream_fnv1a64;
    size_t preload_request_count;
    uint8_t global_visibility_used;
} GeOriginalBgVisibilityResult;

typedef enum GeOriginalBgVisibilityStatus {
    GE_ORIGINAL_BG_VISIBILITY_OK = 0,
    GE_ORIGINAL_BG_VISIBILITY_INVALID_ARGUMENT,
    GE_ORIGINAL_BG_VISIBILITY_INVALID_BACKGROUND,
    GE_ORIGINAL_BG_VISIBILITY_CAPACITY_EXCEEDED,
    GE_ORIGINAL_BG_VISIBILITY_NO_MEMORY
} GeOriginalBgVisibilityStatus;

/* Read-only view of the exact mutable room publication consumed by
 * getROOMID_isRendered/posIsOnScreen.  This is diagnostic state only: the
 * visibility body above remains its sole writer. */
typedef struct GeOriginalBgRoomVisibilitySnapshot {
    int32_t current_room;
    int32_t maximum_room_count;
    int32_t rooms_drawn;
    uint8_t rendered;
    uint8_t neighbor_to_rendered;
    uint8_t loaded_mask;
} GeOriginalBgRoomVisibilitySnapshot;

/* Runs the original US bgDetermineVisibleRooms portal-polygon recursion and,
 * when present, its bounded/materialized global-visibility command stream. */
GeOriginalBgVisibilityStatus ge_original_bg_visibility_run(
    const GeOriginalBgVisibilityInput *input,
    GeOriginalBgVisibilityResult *result);

GeOriginalBgVisibilityProgram *ge_original_bg_visibility_program_create(
    const uint8_t *background, size_t background_size, size_t room_count,
    GeOriginalBgVisibilityStatus *status);
void ge_original_bg_visibility_program_close(GeOriginalBgVisibilityProgram *program);

int ge_original_bg_visibility_room_snapshot(
    uint8_t room, GeOriginalBgRoomVisibilitySnapshot *snapshot);

/* These retain the exact US bg.c portal polygon test and control-bit update
 * used by door setup/runtime after the visibility world is materialized. */
int32_t ge_original_bg_find_portal_between_rooms(
    int32_t room1, int32_t room2, const float point1[3],
    const float point2[3]);
/* Exact sub_GAME_7F0B9E04 all-portal line query used by tinted glass setup. */
int32_t ge_original_bg_find_portal_on_line(
    const float point1[3], const float point2[3]);
int ge_original_bg_set_portal_open(int32_t portal, int open,
                                   uint8_t *portal_controls,
                                   size_t portal_control_count);

const char *ge_original_bg_visibility_status_name(
    GeOriginalBgVisibilityStatus status);

#ifdef __cplusplus
}
#endif

#endif
