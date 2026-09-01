#ifndef GE_ORIGINAL_BG_CONNECTIVITY_INTERNAL_H
#define GE_ORIGINAL_BG_CONNECTIVITY_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

typedef int32_t s32;
typedef int32_t bool;
typedef uint8_t u8;
typedef float f32;

typedef struct coord3d {
    union {
        struct {
            f32 x;
            f32 y;
            f32 z;
        };
        f32 f[3];
    };
} coord3d;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

typedef struct bg_portal_entry {
    u8 numPoints;
    u8 padding[3];
    coord3d point;
} bg_portal_entry;

typedef struct bg_portal_data_entry {
    bg_portal_entry *offset_portal;
    u8 connectedRoom1;
    u8 connectedRoom2;
    u8 controlbytes1;
    u8 controlbytes2;
} bg_portal_data_entry;

extern bg_portal_data_entry *g_BgPortals;
extern f32 room_data_float1;

s32 bgGetConnectedRooms(s32 roomIndex, s32 *list, s32 max);
bool bgRoomsSharePortal(s32 room1, s32 room2);
bool bgIsBboxOverlapping(coord3d *portalbbmin, coord3d *portalbbmax,
                         coord3d *propbbmin, coord3d *propbbmax);
void sub_GAME_7F0BA2D4(coord3d *bbmin, coord3d *bbmax,
                       s32 *room_list, s32 *count, s32 max_count);

#endif
