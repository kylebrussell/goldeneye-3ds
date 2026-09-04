#include "ge_original_bg_visibility.h"
#include "ge_dam_preload_queue.h"

#include <assert.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BG_BASE UINT32_C(0x0f000000)
#define ROOM_OFFSET 20U
#define ROOM_RECORD_COUNT 139U
#define PORTAL_OFFSET (ROOM_OFFSET + ROOM_RECORD_COUNT * 24U)
#define GEOMETRY_OFFSET (PORTAL_OFFSET + 24U)
#define FIXTURE_SIZE (GEOMETRY_OFFSET + 2U * 52U)

/* Link to the shared storage as bytes so this host test models the unchanged
 * ARM getROOMID_isRendered address calculation (room * 0x50) independently
 * of the C struct used by the visibility writer. */
extern uint8_t ge_bg_visibility_room_info[];

static uint8_t canonical_getROOMID_isRendered(uint8_t room)
{
    return ge_bg_visibility_room_info[(size_t)room * 0x50U];
}

static void put_be32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value >> 24);
    data[1] = (uint8_t)(value >> 16);
    data[2] = (uint8_t)(value >> 8);
    data[3] = (uint8_t)value;
}

static void put_be_float(uint8_t *data, float value)
{
    uint32_t bits;

    memcpy(&bits, &value, sizeof(bits));
    put_be32(data, bits);
}

static void put_portal(uint8_t *data, size_t index, uint8_t room1,
                       uint8_t room2, float z, int reverse)
{
    static const float forward[4][2] = {
        {-2.0f, -2.0f}, {-2.0f, 2.0f}, {2.0f, 2.0f}, {2.0f, -2.0f},
    };
    static const float backward[4][2] = {
        {-2.0f, -2.0f}, {2.0f, -2.0f}, {2.0f, 2.0f}, {-2.0f, 2.0f},
    };
    const float (*points)[2] = reverse ? backward : forward;
    const size_t geometry_offset = GEOMETRY_OFFSET + index * 52U;
    uint8_t *record = data + PORTAL_OFFSET + index * 8U;
    size_t point_index;

    put_be32(record, BG_BASE + (uint32_t)geometry_offset);
    record[4] = room1;
    record[5] = room2;
    data[geometry_offset] = 4U;
    for (point_index = 0U; point_index < 4U; ++point_index) {
        uint8_t *point = data + geometry_offset + 4U + point_index * 12U;
        put_be_float(point, points[point_index][0]);
        put_be_float(point + 4U, points[point_index][1]);
        put_be_float(point + 8U, z);
    }
}

static void put_seven_point_portal(uint8_t *data, uint8_t room1,
                                   uint8_t room2, float z)
{
    static const float points[7][2] = {
        {-2.0f, -1.0f}, {-1.0f, 2.0f}, {0.0f, 2.5f},
        {1.0f, 2.0f}, {2.0f, -1.0f}, {1.0f, -2.0f}, {-1.0f, -2.0f},
    };
    uint8_t *record = data + PORTAL_OFFSET;
    size_t point_index;

    memset(data + PORTAL_OFFSET, 0, sizeof(uint32_t) * 4U);
    put_be32(record, BG_BASE + GEOMETRY_OFFSET);
    record[4] = room1;
    record[5] = room2;
    data[GEOMETRY_OFFSET] = 7U;
    for (point_index = 0U; point_index < 7U; ++point_index) {
        uint8_t *point = data + GEOMETRY_OFFSET + 4U
            + point_index * 12U;
        put_be_float(point, points[point_index][0]);
        put_be_float(point + 4U, points[point_index][1]);
        put_be_float(point + 8U, z);
    }
}

static int contains_room(const GeOriginalBgVisibilityResult *result,
                         uint8_t room)
{
    size_t index;

    for (index = 0U; index < result->room_count; ++index) {
        if (result->rooms[index].room == room) return 1;
    }
    return 0;
}

typedef struct FileData {
    uint8_t *bytes;
    size_t size;
} FileData;

typedef struct PreloadLog {
    uint8_t rooms[GE_ORIGINAL_BG_VISIBILITY_MAX_ROOMS];
    size_t count;
} PreloadLog;

static uint8_t log_preload(void *context, uint8_t room)
{
    PreloadLog *log = context;

    assert(log->count < sizeof(log->rooms));
    log->rooms[log->count++] = room;
    return 0U;
}

static FileData read_file(const char *path)
{
    FILE *stream = fopen(path, "rb");
    FileData result = {NULL, 0U};
    long length;

    assert(stream != NULL);
    assert(fseek(stream, 0L, SEEK_END) == 0);
    length = ftell(stream);
    assert(length >= 0L);
    assert(fseek(stream, 0L, SEEK_SET) == 0);
    result.size = (size_t)length;
    result.bytes = malloc(result.size == 0U ? 1U : result.size);
    assert(result.bytes != NULL);
    assert(fread(result.bytes, 1U, result.size, stream) == result.size);
    assert(fclose(stream) == 0);
    return result;
}

static int16_t read_be16(const uint8_t *data)
{
    return (int16_t)(((uint16_t)data[0] << 8) | (uint16_t)data[1]);
}

static uint32_t read_be32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16)
        | ((uint32_t)data[2] << 8) | (uint32_t)data[3];
}

static float read_be_float(const uint8_t *data)
{
    const uint32_t bits = read_be32(data);
    float value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void include_bound(GeOriginalBgRoomBounds *bounds,
                          const float point[3])
{
    size_t axis;

    for (axis = 0U; axis < 3U; ++axis) {
        if (point[axis] < bounds->minimum[axis]) {
            bounds->minimum[axis] = point[axis];
        }
        if (point[axis] > bounds->maximum[axis]) {
            bounds->maximum[axis] = point[axis];
        }
    }
}

static void build_private_dam_bounds(
    const FileData *background, GeOriginalBgRoomBounds bounds[137])
{
    const uint32_t base = UINT32_C(0x0f000000);
    const size_t room_table = read_be32(background->bytes + 4U) - base;
    const size_t portal_table = read_be32(background->bytes + 8U) - base;
    size_t room;
    size_t portal;

    memset(bounds, 0, 137U * sizeof(*bounds));
    for (room = 1U; room < 137U; ++room) {
        const uint8_t *record = background->bytes + room_table + room * 24U;
        float origin[3];
        char path[1024];
        FileData points;
        size_t vertex;
        size_t axis;

        for (axis = 0U; axis < 3U; ++axis) {
            origin[axis] = read_be_float(record + 12U + axis * 4U);
            bounds[room].minimum[axis] = FLT_MAX;
            bounds[room].maximum[axis] = -FLT_MAX;
        }
        assert(snprintf(path, sizeof(path),
            "build/3ds-levels/dam/rooms/room%03u/point_table.bin",
            (unsigned)room) > 0);
        points = read_file(path);
        assert(points.size >= 16U && points.size % 16U == 0U);
        for (vertex = 0U; vertex < points.size; vertex += 16U) {
            float point[3];

            for (axis = 0U; axis < 3U; ++axis) {
                point[axis] = origin[axis]
                    + (float)read_be16(points.bytes + vertex + axis * 2U);
            }
            include_bound(&bounds[room], point);
        }
        free(points.bytes);
    }
    for (portal = 0U; portal < 200U; ++portal) {
        const uint8_t *record = background->bytes + portal_table + portal * 8U;
        const uint32_t address = read_be32(record);
        const uint8_t room1 = record[4];
        const uint8_t room2 = record[5];
        size_t point;
        size_t geometry;

        if (address == 0U) break;
        assert(address >= base && room1 < 137U && room2 < 137U);
        geometry = address - base;
        assert(background->bytes[geometry] >= 3U
            && background->bytes[geometry] <= 6U);
        for (point = 0U; point < background->bytes[geometry]; ++point) {
            float position[3];
            size_t axis;

            for (axis = 0U; axis < 3U; ++axis) {
                position[axis] = read_be_float(background->bytes + geometry
                    + 4U + point * 12U + axis * 4U);
            }
            if (room1 != 0U) include_bound(&bounds[room1], position);
            if (room2 != 0U) include_bound(&bounds[room2], position);
        }
    }
    assert(portal == 194U);
}

static void test_private_dam_spawn(const char *camera_matrix_path)
{
    static const uint8_t expected[] = {
        135U, 133U, 134U, 132U, 124U, 125U,
    };
    static const uint8_t initial_resident[] = {
        135U, 133U, 134U, 132U, 136U, 124U, 125U, 126U, 127U, 128U,
    };
    FileData background = read_file(
        "build/u/assets/obseg/bg/bg_dam_all_p.bin");
    FileData camera = read_file(camera_matrix_path);
    GeOriginalBgRoomBounds bounds[137];
    GeOriginalBgVisibilityInput input = {0};
    GeOriginalBgVisibilityResult result;
    GeOriginalBgVisibilityProviders providers = {0};
    GeDamPreloadQueue preload_queue;
    PreloadLog preload_log = {{0}, 0U};
    uint8_t portal_controls[194];
    uint8_t preload_room;
    size_t index;

    assert(camera.size == sizeof(input.world_to_screen));
    build_private_dam_bounds(&background, bounds);
    input.background = background.bytes;
    input.background_size = background.size;
    input.room_bounds = bounds;
    input.room_count = 137U;
    input.current_room = 135U;
    input.player_position[0] = 4719.0f / 0.23363999f;
    input.player_position[1] = -18.0f / 0.23363999f;
    input.player_position[2] = 3949.0f / 0.23363999f;
    memcpy(input.world_to_screen, camera.bytes,
           sizeof(input.world_to_screen));
    input.level_scale = 0.23363999f;
    input.visibility_scale = 0.2f;
    input.near_distance = 100.0f;
    input.far_distance = 10000.0f;
    input.vertical_fov_degrees = 60.0f;
    input.aspect_ratio = 4.0f / 3.0f;
    input.view_width = 320;
    input.view_height = 240;
    assert(ge_original_bg_visibility_run(&input, &result)
        == GE_ORIGINAL_BG_VISIBILITY_OK);
    {
        static const uint8_t high_rooms[] = {
            124U, 125U, 132U, 133U, 134U, 135U,
        };
        GeOriginalBgRoomVisibilitySnapshot snapshot;
        size_t room_index;
        assert(ge_original_bg_visibility_room_snapshot(135U, &snapshot));
        assert(snapshot.current_room == 135);
        assert(snapshot.maximum_room_count == 137);
        assert(snapshot.rooms_drawn == (int32_t)result.room_count);
        assert(snapshot.rendered != 0U);
        assert(snapshot.loaded_mask == 0U);
        assert(!ge_original_bg_visibility_room_snapshot(
            0U, NULL));
        for (room_index = 0U; room_index < sizeof(high_rooms);
                ++room_index) {
            assert(ge_original_bg_visibility_room_snapshot(
                high_rooms[room_index], &snapshot));
            assert(snapshot.rendered != 0U);
            assert(canonical_getROOMID_isRendered(
                high_rooms[room_index]) == snapshot.rendered);
        }
        assert(canonical_getROOMID_isRendered(136U) == 0U);
    }
    assert(result.portal_count == 194U);
    assert(result.global_visibility_used != 0U);
    assert(result.global_command_count == 389U);
    assert(result.global_stream_fnv1a64
        == UINT64_C(0x39a4179260579f89));
    assert(result.preload_request_count == 0U);
    assert(result.room_count == sizeof(expected));
    for (index = 0U; index < sizeof(expected); ++index) {
        assert(result.rooms[index].room == expected[index]);
    }
    printf("original Dam spawn visibility (%zu room/%u descent):",
           result.room_count, result.portal_descents);
    for (index = 0U; index < result.room_count; ++index) {
        printf(" %u", (unsigned)result.rooms[index].room);
    }
    putchar('\n');

    for (index = 0U; index < sizeof(portal_controls); ++index) {
        const size_t portal_table = read_be32(background.bytes + 8U)
            - UINT32_C(0x0f000000);
        portal_controls[index] = background.bytes[
            portal_table + index * 8U + 6U];
    }
    providers.context = &preload_log;
    providers.preload_room = log_preload;
    providers.portal_controls = portal_controls;
    providers.portal_control_count = sizeof(portal_controls);
    input.providers = &providers;
    input.current_room = 121U;
    assert(ge_original_bg_visibility_run(&input, &result)
        == GE_ORIGINAL_BG_VISIBILITY_OK);
    assert(result.preload_request_count == 33U);
    assert(preload_log.count == 33U);
    assert(preload_log.rooms[0] == 1U);
    assert(preload_log.rooms[5] == 35U);
    assert(preload_log.rooms[6] == 51U);
    assert(preload_log.rooms[11] == 60U);
    assert(preload_log.rooms[32] == 82U);

    assert(ge_dam_preload_queue_init(&preload_queue, 137U, 137U,
        initial_resident, sizeof(initial_resident)) == GE_DAM_PRELOAD_OK);
    providers = ge_dam_preload_queue_providers(
        &preload_queue, portal_controls, sizeof(portal_controls));
    input.providers = &providers;
    assert(ge_original_bg_visibility_run(&input, &result)
        == GE_ORIGINAL_BG_VISIBILITY_OK);
    assert(result.preload_request_count == 1U);
    assert(preload_queue.pending_count == 1U);
    assert(ge_dam_preload_queue_pop(&preload_queue, &preload_room)
        == GE_DAM_PRELOAD_OK);
    assert(preload_room == 1U);
    providers.portal_control_count = 193U;
    assert(ge_original_bg_visibility_run(&input, &result)
        == GE_ORIGINAL_BG_VISIBILITY_INVALID_ARGUMENT);
    providers.portal_control_count = sizeof(portal_controls);
    input.providers = NULL;
    background.bytes[0x0d1dU] = 2U;
    assert(ge_original_bg_visibility_run(&input, &result)
        == GE_ORIGINAL_BG_VISIBILITY_INVALID_BACKGROUND);
    background.bytes[0x0d1dU] = 3U;
    background.bytes[0x0d1eU] = 1U;
    assert(ge_original_bg_visibility_run(&input, &result)
        == GE_ORIGINAL_BG_VISIBILITY_INVALID_BACKGROUND);
    background.bytes[0x0d1eU] = 0U;
    background.bytes[0x0d4bU] ^= 1U;
    assert(ge_original_bg_visibility_run(&input, &result)
        == GE_ORIGINAL_BG_VISIBILITY_INVALID_BACKGROUND);
    free(camera.bytes);
    free(background.bytes);
}

int main(int argc, char **argv)
{
    uint8_t background[FIXTURE_SIZE] = {0};
    GeOriginalBgRoomBounds bounds[4] = {0};
    GeOriginalBgVisibilityInput input = {0};
    GeOriginalBgVisibilityResult forward;
    GeOriginalBgVisibilityResult backward;
    size_t axis;

    put_be32(background + 4U, BG_BASE + ROOM_OFFSET);
    put_be32(background + 8U, BG_BASE + PORTAL_OFFSET);
    /* Synthetic portal-only fixture: a zero global-vis pointer exercises the
     * original live-recursion fallback. */
    put_be32(background + 12U, 0U);
    put_portal(background, 0U, 2U, 1U, -10.0f, 0);
    put_portal(background, 1U, 3U, 1U, 10.0f, 1);
    for (axis = 0U; axis < 3U; ++axis) {
        bounds[1].minimum[axis] = -2.0f;
        bounds[1].maximum[axis] = 2.0f;
        bounds[2].minimum[axis] = -2.0f;
        bounds[2].maximum[axis] = 2.0f;
        bounds[3].minimum[axis] = -2.0f;
        bounds[3].maximum[axis] = 2.0f;
    }
    bounds[2].minimum[2] = -12.0f;
    bounds[2].maximum[2] = -8.0f;
    bounds[3].minimum[2] = 8.0f;
    bounds[3].maximum[2] = 12.0f;
    input.background = background;
    input.background_size = sizeof(background);
    input.room_bounds = bounds;
    input.room_count = 4U;
    input.current_room = 1U;
    input.level_scale = 1.0f;
    input.visibility_scale = 1.0f;
    input.near_distance = 1.0f;
    input.far_distance = 1000.0f;
    input.vertical_fov_degrees = 60.0f;
    input.aspect_ratio = 4.0f / 3.0f;
    input.view_width = 320;
    input.view_height = 240;
    for (axis = 0U; axis < 4U; ++axis) {
        input.world_to_screen[axis][axis] = 1.0f;
    }
    assert(ge_original_bg_visibility_run(&input, &forward)
        == GE_ORIGINAL_BG_VISIBILITY_OK);
    assert(forward.portal_count == 2U);
    assert(forward.room_count == 2U);
    assert(contains_room(&forward, 1U));
    assert(contains_room(&forward, 2U));
    assert(!contains_room(&forward, 3U));

    input.world_to_screen[0][0] = -1.0f;
    input.world_to_screen[2][2] = -1.0f;
    assert(ge_original_bg_visibility_run(&input, &backward)
        == GE_ORIGINAL_BG_VISIBILITY_OK);
    assert(backward.portal_count == 2U);
    assert(backward.room_count == 2U);
    assert(contains_room(&backward, 1U));
    assert(!contains_room(&backward, 2U));
    assert(contains_room(&backward, 3U));
    assert(forward.portal_descents == 2U);
    assert(backward.portal_descents == 2U);
    {
        GeOriginalBgVisibilityStatus status;
        GeOriginalBgVisibilityProgram *program = ge_original_bg_visibility_program_create(
            input.background, input.background_size, input.room_count, &status);
        assert(program != NULL && status == GE_ORIGINAL_BG_VISIBILITY_OK);
        uint8_t alternate_background[FIXTURE_SIZE];
        memcpy(alternate_background, background, sizeof(background));
        put_portal(alternate_background, 0U, 2U, 1U, -20.0f, 0);
        GeOriginalBgVisibilityProgram *alternate = ge_original_bg_visibility_program_create(
            alternate_background, sizeof(alternate_background), input.room_count, &status);
        assert(alternate != NULL && status == GE_ORIGINAL_BG_VISIBILITY_OK);
        uint8_t controls[2] = {0U, 0U};
        GeOriginalBgVisibilityProviders providers = {
            .portal_controls = controls, .portal_control_count = 2U,
        };
        input.providers = &providers;
        for (unsigned frame = 0U; frame < 32U; ++frame) {
            GeOriginalBgVisibilityResult raw, cached;
            /* Interleave a distinct stage's immutable geometry; cached portal
             * pointers must be rebound to the matching execution storage. */
            GeOriginalBgVisibilityInput other = input;
            other.background = alternate_background;
            other.program = alternate;
            assert(ge_original_bg_set_portal_open(0, (frame & 1U) == 0U, controls, 2U));
            assert(ge_original_bg_set_portal_open(1, (frame & 2U) == 0U, controls, 2U));
            input.program = NULL;
            assert(ge_original_bg_visibility_run(&input, &raw) == GE_ORIGINAL_BG_VISIBILITY_OK);
            assert(ge_original_bg_visibility_run(&other, &cached) == GE_ORIGINAL_BG_VISIBILITY_OK);
            input.program = program;
            assert(ge_original_bg_visibility_run(&input, &cached) == GE_ORIGINAL_BG_VISIBILITY_OK);
            assert(memcmp(&raw, &cached, sizeof(raw)) == 0);
        }
        input.background_size--;
        assert(ge_original_bg_visibility_run(&input, &forward)
            == GE_ORIGINAL_BG_VISIBILITY_INVALID_ARGUMENT);
        input.background_size++;
        input.program = NULL;
        input.providers = NULL;
        ge_original_bg_visibility_program_close(program);
        ge_original_bg_visibility_program_close(alternate);
        assert(ge_original_bg_visibility_program_create(NULL, 0U, 0U, &status) == NULL
            && status == GE_ORIGINAL_BG_VISIBILITY_INVALID_ARGUMENT);
    }
    {
        const float through_near[3] = {0.0f, 0.0f, -20.0f};
        const float centre[3] = {0.0f, 0.0f, 0.0f};
        const float through_far[3] = {0.0f, 0.0f, 20.0f};
        assert(ge_original_bg_find_portal_on_line(through_near, centre)
               == 0);
        assert(ge_original_bg_find_portal_on_line(through_far, centre)
               == 1);
    }
    puts("original bg portal visibility: view-dependent room traversal ok");

    /* Cradle's authored background has no portals.  The unchanged original
     * visibility body handles that by forcing room 9 and testing each room's
     * bounds, so an empty portal table is valid rather than a corrupt asset. */
    {
        GeOriginalBgRoomBounds cradle_bounds[12] = {0};
        GeOriginalBgVisibilityResult cradle;

        memset(background + PORTAL_OFFSET, 0,
               sizeof(background) - PORTAL_OFFSET);
        memset(input.world_to_screen, 0, sizeof(input.world_to_screen));
        for (axis = 0U; axis < 4U; ++axis) {
            input.world_to_screen[axis][axis] = 1.0f;
        }
        for (axis = 0U; axis < 3U; ++axis) {
            cradle_bounds[9].minimum[axis] = -2.0f;
            cradle_bounds[9].maximum[axis] = 2.0f;
        }
        input.room_bounds = cradle_bounds;
        input.room_count = 12U;
        input.current_room = 9U;
        input.level_index = 21;
        assert(ge_original_bg_visibility_run(&input, &cradle)
            == GE_ORIGINAL_BG_VISIBILITY_OK);
        assert(cradle.portal_count == 0U);
        assert(contains_room(&cradle, 9U));
        GeOriginalBgVisibilityStatus status;
        GeOriginalBgVisibilityProgram *empty = ge_original_bg_visibility_program_create(
            background, sizeof(background), input.room_count, &status);
        assert(empty != NULL && status == GE_ORIGINAL_BG_VISIBILITY_OK);
        /* Leave live geometry from a larger stage in execution storage. The
         * prepared empty stage must restore its sentinel before both original
         * room traversal and original line/portal lookup can run. */
        uint8_t populated_background[FIXTURE_SIZE];
        memcpy(populated_background, background, sizeof(background));
        put_portal(populated_background, 0U, 2U, 1U, -10.0f, 0);
        GeOriginalBgVisibilityInput populated = input;
        populated.background = populated_background;
        populated.room_bounds = bounds;
        populated.room_count = 4U;
        populated.current_room = 1U;
        populated.level_index = 0;
        assert(ge_original_bg_visibility_run(&populated, &forward)
            == GE_ORIGINAL_BG_VISIBILITY_OK);
        assert(forward.portal_count == 1U);
        input.program = empty;
        assert(ge_original_bg_visibility_run(&input, &backward)
            == GE_ORIGINAL_BG_VISIBILITY_OK);
        assert(memcmp(&cradle, &backward, sizeof(cradle)) == 0);
        const float through[3] = {0.0f, 0.0f, -20.0f};
        const float centre[3] = {0.0f, 0.0f, 0.0f};
        assert(ge_original_bg_find_portal_on_line(through, centre) == -1);
        input.program = NULL;
        ge_original_bg_visibility_program_close(empty);
    }
    puts("original bg portal visibility: authored zero-portal Cradle path ok");

    /* Depot contains an exact seven-point portal. The original visibility
     * scratch path supports up to nine authored points (19 after its optional
     * two-sided expansion), so this must not be rejected as malformed. */
    put_seven_point_portal(background, 2U, 1U, -10.0f);
    input.room_bounds = bounds;
    input.room_count = 4U;
    input.current_room = 1U;
    input.level_index = 0;
    assert(ge_original_bg_visibility_run(&input, &forward)
        == GE_ORIGINAL_BG_VISIBILITY_OK);
    assert(forward.portal_count == 1U);
    puts("original bg portal visibility: authored seven-point Depot portal ok");
    if (argc == 2) test_private_dam_spawn(argv[1]);
    else assert(argc == 1);
    return 0;
}
