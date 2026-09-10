#ifndef GE_ORIGINAL_BG_GEOMETRY_INTERNAL_H
#define GE_ORIGINAL_BG_GEOMETRY_INTERNAL_H

#include "ge_original_bg_geometry.h"
#include <ultra64.h>
#include <bondtypes.h>
#include "game/bg.h"
#include "game/lightfixture.h"

#define GE_BG_GEOMETRY_MAX_ROOMS GE_ORIGINAL_BG_VISIBILITY_MAX_ROOMS
#define GE_BG_GEOMETRY_MAX_BYTES (32U * 1024U * 1024U)
#define GE_BG_LIGHT_FIXTURES 100U
#define GE_BG_DARKENED_VERTICES 512U

typedef struct GeOriginalBgCommandInfo {
    uint32_t point_indices[12];
    int16_t texture;
    int8_t tile_format, tile_size;
    uint8_t triangle_count;
} GeOriginalBgCommandInfo;

typedef struct GeOriginalBgRoomGeometry {
    GeDamRoomBlobDescriptor blobs;
    /* Vertex contains native pointers and widens on 64-bit hosts; Vtx keeps
     * the 16-byte GPU/light layout. Never alias arrays of the two types. */
    Vertex *collision_vertices;
    GeOriginalBgCommandInfo *commands[2];
    size_t command_counts[2];
    size_t vertex_count;
} GeOriginalBgRoomGeometry;

struct GeOriginalBgGeometry {
    s_room_info rooms[GE_BG_GEOMETRY_MAX_ROOMS];
    coord3d origins[GE_BG_GEOMETRY_MAX_ROOMS];
    GeOriginalBgRoomGeometry room_geometry[GE_BG_GEOMETRY_MAX_ROOMS];
    s_lightfixture fixtures[GE_BG_LIGHT_FIXTURES];
    struct s_darkened_light darkened[GE_BG_DARKENED_VERTICES];
    int16_t fixture_slot, fixture_room;
    int32_t darkened_cursor;
    size_t room_count, allocated_bytes;
    float level_scale, inverse_scale;
    GeOriginalBgGeometryStatus status;
};

extern GeOriginalBgGeometry *ge_bg_geometry_current;

static inline uint32_t ge_bg_gfx_word(const Gfx *gfx, size_t word)
{
    const uint8_t *p = (const uint8_t *)gfx + word * 4U;
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
        | ((uint32_t)p[2] << 8) | p[3];
}
static inline uint16_t ge_bg_gfx_half(const Gfx *gfx, size_t half)
{
    const uint8_t *p = (const uint8_t *)gfx + half * 2U;
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}
static inline Gfx *ge_bg_gfx_at(const void *base, ptrdiff_t command)
{ return (Gfx *)((uint8_t *)base + command * 8); }
static inline int32_t ge_bg_distance_squared(int32_t x, int32_t y, int32_t z)
{
    return (int32_t)((uint32_t)x * (uint32_t)x
        + (uint32_t)y * (uint32_t)y + (uint32_t)z * (uint32_t)z);
}

const GeOriginalBgCommandInfo *ge_bg_command_info(int32_t room, const Gfx *gfx);
void *ge_bg_bounds_allocate(size_t size);
void ge_bg_build_bounds_exact(int32_t room);
int ge_bg_bullet_hit_exact(coord3d *from, coord3d *to, int32_t room, HitThing *hit);
void ge_bg_light_init_exact(void);
void ge_bg_light_room_exact(int32_t room);
void ge_bg_light_begin_exact(Gfx *command);
void ge_bg_light_end_exact(Gfx *command);
int ge_bg_is_light_exact(int32_t texture);
void ge_bg_light_break_exact(Gfx *command, uint32_t type, int32_t room);
void ge_bg_light_indices_exact(Gfx *command, uint32_t type,
    int32_t *a, int32_t *b, int32_t *c);

/* Native pointer table for the original held-item room-bbox closure. It is
 * deliberately separate from the fixed 0x50 visibility publication. */
s_room_info *ge_original_bg_geometry_native_rooms(void);

#endif
