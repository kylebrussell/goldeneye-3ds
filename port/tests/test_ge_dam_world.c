#include "ge_dam_world.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BG_BASE 0x0f000000U
#define ROOM_OFFSET 20U
#define ROOM_RECORDS 139U
#define VISIBILITY_OFFSET (ROOM_OFFSET + ROOM_RECORDS * 24U)
#define PORTAL_OFFSET VISIBILITY_OFFSET
#define GEOMETRY_OFFSET (PORTAL_OFFSET + 32U)
#define FIXTURE_SIZE (GEOMETRY_OFFSET + 32U)

static void put_be32(unsigned char *data, unsigned value)
{
    data[0] = (unsigned char)(value >> 24);
    data[1] = (unsigned char)(value >> 16);
    data[2] = (unsigned char)(value >> 8);
    data[3] = (unsigned char)value;
}

static void put_be_float(unsigned char *data, float value)
{
    unsigned bits;

    memcpy(&bits, &value, sizeof(bits));
    put_be32(data, bits);
}

static void put_portal(unsigned char *data, size_t index,
                       unsigned room1, unsigned room2)
{
    unsigned char *record = data + PORTAL_OFFSET + index * 8U;
    const size_t geometry = GEOMETRY_OFFSET + index * 4U;

    put_be32(record, BG_BASE + (unsigned)geometry);
    record[4] = (unsigned char)room1;
    record[5] = (unsigned char)room2;
    data[geometry] = 4U;
}

static void test_synthetic_original_connectivity(void)
{
    unsigned char data[FIXTURE_SIZE] = {0};
    GeDamWorld world;
    uint8_t rooms[8];
    size_t room_count;

    put_be32(data + 4U, BG_BASE + ROOM_OFFSET);
    put_be32(data + 8U, BG_BASE + PORTAL_OFFSET);
    put_be32(data + 12U, BG_BASE + VISIBILITY_OFFSET);
    put_be_float(data + ROOM_OFFSET + 135U * 24U + 12U, 4416.0f);
    put_be_float(data + ROOM_OFFSET + 135U * 24U + 16U, 177.0f);
    put_be_float(data + ROOM_OFFSET + 135U * 24U + 20U, 3860.0f);
    put_portal(data, 0U, 134U, 133U);
    put_portal(data, 1U, 135U, 133U);
    put_portal(data, 2U, 132U, 133U);

    assert(ge_dam_world_parse(data, sizeof(data), &world)
        == GE_DAM_WORLD_OK);
    assert(world.room_count == 137U);
    assert(world.portal_count == 3U);
    assert(ge_dam_world_room(&world, 135U)->origin[0] == 4416.0f);
    assert(ge_dam_world_collect_connected(&world, 135U, rooms,
        sizeof(rooms), &room_count) == GE_DAM_WORLD_OK);
    assert(room_count == 4U);
    assert(rooms[0] == 135U && rooms[1] == 133U);
    assert(rooms[2] == 134U && rooms[3] == 132U);
    assert(ge_dam_world_rooms_share_portal(&world, 135U, 133U));
    assert(!ge_dam_world_rooms_share_portal(&world, 135U, 132U));
}

static void test_authored_zero_portal_world(void)
{
    unsigned char data[ROOM_OFFSET + 4U * 24U + 8U] = {0};
    GeDamWorld world;
    uint8_t rooms[2] = {0};
    size_t room_count = 0U;
    const size_t portal_offset = ROOM_OFFSET + 4U * 24U;

    /* dummy room, one playable room, bounds sentinel, terminator */
    put_be32(data + 4U, BG_BASE + ROOM_OFFSET);
    put_be32(data + 8U, BG_BASE + (unsigned)portal_offset);
    put_be32(data + 12U, BG_BASE + (unsigned)portal_offset);
    assert(ge_dam_world_parse(data, sizeof(data), &world)
           == GE_DAM_WORLD_OK);
    assert(world.room_count == 2U && world.portal_count == 0U);
    assert(ge_dam_world_collect_connected(&world, 1U, rooms,
        sizeof(rooms), &room_count) == GE_DAM_WORLD_OK);
    assert(room_count == 1U && rooms[0] == 1U);
}

static void test_private_dam_background(void)
{
    const char *path = "build/u/assets/obseg/bg/bg_dam_all_p.bin";
    FILE *stream = fopen(path, "rb");
    long size;
    unsigned char *data;
    GeDamWorld world;
    uint8_t rooms[10];
    static const uint8_t expected[10] = {
        135U, 133U, 134U, 132U, 136U,
        124U, 125U, 126U, 127U, 128U,
    };
    size_t room_count;

    if (stream == NULL) return;
    assert(fseek(stream, 0L, SEEK_END) == 0);
    size = ftell(stream);
    assert(size > 0L && fseek(stream, 0L, SEEK_SET) == 0);
    data = malloc((size_t)size);
    assert(data != NULL);
    assert(fread(data, 1U, (size_t)size, stream) == (size_t)size);
    fclose(stream);
    assert(ge_dam_world_parse(data, (size_t)size, &world)
        == GE_DAM_WORLD_OK);
    assert(world.portal_count == 194U);
    assert(ge_dam_world_collect_connected(&world, 135U, rooms, 10U,
        &room_count) == GE_DAM_WORLD_OK);
    assert(room_count == 10U);
    assert(memcmp(rooms, expected, sizeof(expected)) == 0);
    free(data);
}

static void test_private_facility_background(void)
{
    static const uint8_t expected_initial[10] = {
        13U, 15U, 14U, 12U, 9U, 8U, 5U, 6U, 7U, 10U,
    };
    const char *path = "build/u/assets/obseg/bg/bg_ark_all_p.bin";
    FILE *stream = fopen(path, "rb");
    long size;
    unsigned char *data;
    GeDamWorld world;
    uint8_t connected[GE_DAM_WORLD_MAX_ROOMS];
    size_t connected_count;
    size_t index;

    if (stream == NULL) return;
    assert(fseek(stream, 0L, SEEK_END) == 0);
    size = ftell(stream);
    assert(size == 200576L && fseek(stream, 0L, SEEK_SET) == 0);
    data = malloc((size_t)size);
    assert(data != NULL);
    assert(fread(data, 1U, (size_t)size, stream) == (size_t)size);
    fclose(stream);
    assert(ge_dam_world_parse(data, (size_t)size, &world)
        == GE_DAM_WORLD_OK);
    assert(world.room_count == 78U);
    assert(world.portal_count == 109U);
    assert(ge_dam_world_room(&world, 13U) != NULL);
    assert(ge_dam_world_room(&world, 78U) == NULL);
    assert(ge_dam_world_collect_connected(
        &world, 13U, connected, sizeof(connected), &connected_count)
        == GE_DAM_WORLD_OK);
    assert(connected_count == 75U);
    assert(memcmp(connected, expected_initial, sizeof(expected_initial)) == 0);
    printf("Facility spawn-connected rooms (%zu):", connected_count);
    for (index = 0U; index < connected_count; ++index)
        printf(" %u", (unsigned)connected[index]);
    putchar('\n');
    free(data);
}

int main(void)
{
    test_synthetic_original_connectivity();
    test_authored_zero_portal_world();
    test_private_dam_background();
    test_private_facility_background();
    puts("Original background portal connectivity tests passed");
    return 0;
}
