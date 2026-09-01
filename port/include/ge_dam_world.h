#ifndef GE_DAM_WORLD_H
#define GE_DAM_WORLD_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GE_DAM_WORLD_MAX_ROOMS 137U
#define GE_DAM_WORLD_MAX_PORTALS 256U

typedef struct GeDamWorldRoom {
    float origin[3];
} GeDamWorldRoom;

typedef struct GeDamWorldPortal {
    void *geometry_token;
    uint8_t connected_room1;
    uint8_t connected_room2;
    uint8_t control_bytes1;
    uint8_t control_bytes2;
} GeDamWorldPortal;

typedef struct GeDamWorld {
    GeDamWorldRoom rooms[GE_DAM_WORLD_MAX_ROOMS];
    GeDamWorldPortal portals[GE_DAM_WORLD_MAX_PORTALS + 1U];
    size_t room_count;
    size_t portal_count;
} GeDamWorld;

typedef enum GeDamWorldStatus {
    GE_DAM_WORLD_OK = 0,
    GE_DAM_WORLD_INVALID_ARGUMENT,
    GE_DAM_WORLD_INVALID_BACKGROUND,
    GE_DAM_WORLD_CAPACITY_EXCEEDED
} GeDamWorldStatus;

/* Parses an original big-endian stage bg header, room origins and complete
 * portal ownership table up to the solo-stage storage capacity. Stream
 * payloads remain in the asset pack. */
GeDamWorldStatus ge_dam_world_parse(const uint8_t *background,
                                    size_t background_size,
                                    GeDamWorld *world);

/* Breadth-first component order using the exact bgGetConnectedRooms body
 * compiled from src/game/bg.c. max_rooms is a streaming/load budget, not a
 * replacement visibility policy. */
GeDamWorldStatus ge_dam_world_collect_connected(
    GeDamWorld *world,
    uint8_t start_room,
    uint8_t *rooms,
    size_t max_rooms,
    size_t *room_count);

const GeDamWorldRoom *ge_dam_world_room(const GeDamWorld *world,
                                       uint8_t room);
int ge_dam_world_rooms_share_portal(GeDamWorld *world,
                                    uint8_t room1,
                                    uint8_t room2);
const char *ge_dam_world_status_name(GeDamWorldStatus status);

#ifdef __cplusplus
}
#endif

#endif
