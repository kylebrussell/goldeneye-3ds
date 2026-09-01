#include "ge_dam_world.h"

#include <string.h>

#define GE_DAM_BG_BASE UINT32_C(0x0f000000)
#define GE_DAM_BG_HEADER_SIZE 20U
#define GE_DAM_BG_ROOM_RECORD_SIZE 24U
#define GE_DAM_BG_PORTAL_RECORD_SIZE 8U

#ifdef GE_PORT_BG_VISIBILITY_AVAILABLE
extern GeDamWorldPortal *g_BgPortals;
#else
GeDamWorldPortal *g_BgPortals;
#endif

int32_t bgGetConnectedRooms(int32_t roomIndex, int32_t *list, int32_t max);
int32_t bgRoomsSharePortal(int32_t room1, int32_t room2);

static uint32_t ge_dam_world_be32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16)
        | ((uint32_t)data[2] << 8) | (uint32_t)data[3];
}

static float ge_dam_world_be_float(const uint8_t *data)
{
    const uint32_t bits = ge_dam_world_be32(data);
    float value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

static int ge_dam_world_offset(uint32_t address, size_t size,
                               size_t *offset)
{
    const uint32_t relative = address - GE_DAM_BG_BASE;

    if (address < GE_DAM_BG_BASE || (uint64_t)relative >= size) return 0;
    *offset = relative;
    return 1;
}

GeDamWorldStatus ge_dam_world_parse(const uint8_t *background,
                                    size_t background_size,
                                    GeDamWorld *world)
{
    size_t room_offset;
    size_t portal_offset;
    size_t visibility_offset;
    size_t room_records;
    size_t room_index;
    size_t portal_index;

    if (background == NULL || world == NULL) {
        return GE_DAM_WORLD_INVALID_ARGUMENT;
    }
    memset(world, 0, sizeof(*world));
    if (background_size < GE_DAM_BG_HEADER_SIZE
            || !ge_dam_world_offset(ge_dam_world_be32(background + 4U),
                                    background_size, &room_offset)
            || !ge_dam_world_offset(ge_dam_world_be32(background + 8U),
                                    background_size, &portal_offset)
            || !ge_dam_world_offset(ge_dam_world_be32(background + 12U),
                                    background_size, &visibility_offset)
            || room_offset >= visibility_offset
            || visibility_offset > portal_offset
            || (visibility_offset - room_offset)
                % GE_DAM_BG_ROOM_RECORD_SIZE != 0U) {
        return GE_DAM_WORLD_INVALID_BACKGROUND;
    }
    room_records = (visibility_offset - room_offset)
        / GE_DAM_BG_ROOM_RECORD_SIZE;
    /* dummy room 0, playable rooms 1..N, stream-bounds sentinel, terminator */
    if (room_records < 3U || room_records > GE_DAM_WORLD_MAX_ROOMS + 2U) {
        return GE_DAM_WORLD_INVALID_BACKGROUND;
    }
    world->room_count = room_records - 2U;
    for (room_index = 0U; room_index < world->room_count;
            ++room_index) {
        const uint8_t *record = background + room_offset
            + room_index * GE_DAM_BG_ROOM_RECORD_SIZE;
        size_t axis;

        for (axis = 0U; axis < 3U; ++axis) {
            world->rooms[room_index].origin[axis] =
                ge_dam_world_be_float(record + 12U + axis * 4U);
        }
    }
    for (portal_index = 0U; portal_index < GE_DAM_WORLD_MAX_PORTALS;
            ++portal_index) {
        const size_t offset = portal_offset
            + portal_index * GE_DAM_BG_PORTAL_RECORD_SIZE;
        GeDamWorldPortal *portal;
        uint32_t geometry_address;
        size_t geometry_offset;

        if (offset > background_size
                || background_size - offset < GE_DAM_BG_PORTAL_RECORD_SIZE) {
            return GE_DAM_WORLD_INVALID_BACKGROUND;
        }
        geometry_address = ge_dam_world_be32(background + offset);
        if (geometry_address == 0U && background[offset + 4U] == 0U
                && background[offset + 5U] == 0U) {
            break;
        }
        if (!ge_dam_world_offset(geometry_address, background_size,
                                 &geometry_offset)
                || geometry_offset + 4U > background_size
                || background[geometry_offset] < 3U) {
            return GE_DAM_WORLD_INVALID_BACKGROUND;
        }
        portal = &world->portals[portal_index];
        /* Connectivity never dereferences geometry. Preserve a stable,
         * non-null token so the original sentinel loop is unchanged. */
        portal->geometry_token = (void *)(uintptr_t)(geometry_offset + 1U);
        portal->connected_room1 = background[offset + 4U];
        portal->connected_room2 = background[offset + 5U];
        portal->control_bytes1 = background[offset + 6U];
        portal->control_bytes2 = background[offset + 7U];
        if (portal->connected_room1 >= world->room_count
                || portal->connected_room2 >= world->room_count) {
            return GE_DAM_WORLD_INVALID_BACKGROUND;
        }
    }
    /* Cradle's authored background has no portals.  A terminator at the
     * first record is valid; the stage streams only its selected room. */
    if (portal_index == GE_DAM_WORLD_MAX_PORTALS) {
        return GE_DAM_WORLD_INVALID_BACKGROUND;
    }
    world->portal_count = portal_index;
    world->portals[portal_index].geometry_token = NULL;
    return GE_DAM_WORLD_OK;
}

GeDamWorldStatus ge_dam_world_collect_connected(
    GeDamWorld *world,
    uint8_t start_room,
    uint8_t *rooms,
    size_t max_rooms,
    size_t *room_count)
{
    uint8_t seen[GE_DAM_WORLD_MAX_ROOMS] = {0};
    size_t cursor = 0U;
    size_t count = 0U;

    if (room_count == NULL) return GE_DAM_WORLD_INVALID_ARGUMENT;
    *room_count = 0U;
    if (world == NULL || rooms == NULL || max_rooms == 0U
            || start_room >= world->room_count) {
        return GE_DAM_WORLD_INVALID_ARGUMENT;
    }
    g_BgPortals = world->portals;
    rooms[count++] = start_room;
    seen[start_room] = 1U;
    if (world->portal_count == 0U) {
        *room_count = count;
        return GE_DAM_WORLD_OK;
    }
    while (cursor < count && count < max_rooms) {
        int32_t connected[GE_DAM_WORLD_MAX_ROOMS];
        const int32_t connected_count = bgGetConnectedRooms(
            rooms[cursor++], connected, (int32_t)GE_DAM_WORLD_MAX_ROOMS);
        int32_t index;

        for (index = 0; index < connected_count && count < max_rooms;
                ++index) {
            const int32_t room = connected[index];

            if (room >= 0 && (size_t)room < world->room_count
                    && seen[room] == 0U) {
                seen[room] = 1U;
                rooms[count++] = (uint8_t)room;
            }
        }
    }
    *room_count = count;
    return GE_DAM_WORLD_OK;
}

const GeDamWorldRoom *ge_dam_world_room(const GeDamWorld *world,
                                       uint8_t room)
{
    return world != NULL && room < world->room_count
        ? &world->rooms[room] : NULL;
}

int ge_dam_world_rooms_share_portal(GeDamWorld *world,
                                    uint8_t room1,
                                    uint8_t room2)
{
    if (world == NULL || room1 >= world->room_count
            || room2 >= world->room_count) return 0;
    g_BgPortals = world->portals;
    return bgRoomsSharePortal(room1, room2) != 0;
}

const char *ge_dam_world_status_name(GeDamWorldStatus status)
{
    switch (status) {
    case GE_DAM_WORLD_OK: return "ok";
    case GE_DAM_WORLD_INVALID_ARGUMENT: return "invalid argument";
    case GE_DAM_WORLD_INVALID_BACKGROUND: return "invalid background";
    case GE_DAM_WORLD_CAPACITY_EXCEEDED: return "capacity exceeded";
    default: return "unknown";
    }
}
