#include "ge_gbi_traverse.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct EventLog {
    GeGbiCommandKind kinds[32];
    size_t depths[32];
    size_t count;
    GeGbiVertex vertices[GE_GBI_VERTEX_CACHE_SIZE];
    uint8_t vertex_count;
    size_t stop_after;
} EventLog;

static int log_event(const GeGbiTraversalEvent *event, void *user_data)
{
    EventLog *log = (EventLog *)user_data;

    assert(log->count < sizeof(log->kinds) / sizeof(log->kinds[0]));
    log->kinds[log->count] = event->command.kind;
    log->depths[log->count] = event->call_depth;
    ++log->count;
    if (event->has_vertex_batch != 0U) {
        log->vertex_count = event->vertex_count;
        memcpy(log->vertices, event->vertices,
               (size_t)event->vertex_count * sizeof(event->vertices[0]));
    }
    return log->stop_after == 0U || log->count < log->stop_after;
}

static GeGbiAddress address(uint8_t segment, uint32_t offset)
{
    GeGbiAddress result;

    result.segment = segment;
    result.offset = offset;
    result.raw = ((uint32_t)segment << 24) | offset;
    return result;
}

static void test_nested_push_branch_and_vertices(void)
{
    static const uint8_t commands[0x70] = {
        /* 0x00: gsSPVertex(0x02000000, 3, 0) */
        0x04, 0x20, 0x00, 0x30, 0x02, 0x00, 0x00, 0x00,
        /* 0x08: gsSPDisplayList(0x01000040) */
        0x06, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x40,
        /* 0x10: gsSP1Triangle(0,1,2,0) */
        0xbf, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x14,
        /* 0x18: end */
        0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        /* 0x20..0x3f padding */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        /* 0x40: set primitive color */
        0xfa, 0x00, 0x00, 0x00, 0x11, 0x22, 0x33, 0x44,
        /* 0x48: gsSPBranchList(0x01000060) */
        0x06, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x60,
        /* 0x50..0x5f padding */
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        /* 0x60: Rare TRI4 with one triangle */
        0xb1, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x21,
        /* 0x68: end child and return to root */
        0xb8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    static const uint8_t vertices[] = {
        0xff, 0xff, 0x00, 0x02, 0x80, 0x00, 0x12, 0x34,
        0xff, 0xc0, 0x00, 0x20, 0x10, 0x20, 0x30, 0x40,
        0x00, 0x64, 0xff, 0x38, 0x01, 0x2c, 0x00, 0x00,
        0x00, 0x40, 0xff, 0x80, 0x50, 0x60, 0x70, 0x80,
        0x7f, 0xff, 0x80, 0x00, 0x00, 0x00, 0xab, 0xcd,
        0x12, 0x34, 0xed, 0xcc, 0x90, 0xa0, 0xb0, 0xc0
    };
    GeGbiMemoryMap memory;
    GeGbiTraversalConfig config = {4U, 32U};
    GeGbiTraversalResult result;
    EventLog log;

    memset(&log, 0, sizeof(log));
    ge_gbi_memory_map_init(&memory);
    assert(ge_gbi_memory_map_set_segment(&memory, 1U, commands,
                                         sizeof(commands)) == GE_GBI_RESOLVE_OK);
    assert(ge_gbi_memory_map_set_segment(&memory, 2U, vertices,
                                         sizeof(vertices)) == GE_GBI_RESOLVE_OK);

    result = ge_gbi_traverse_display_list(&memory, address(1U, 0U),
                                          GE_GBI_BYTE_ORDER_BIG_ENDIAN,
                                          &config, log_event, &log);
    assert(result.status == GE_GBI_TRAVERSAL_OK);
    assert(result.commands_visited == 8U);
    assert(result.vertex_batches == 1U);
    assert(result.vertices_fetched == 3U);
    assert(result.maximum_call_depth == 1U);
    assert(log.count == 8U);
    assert(log.kinds[0] == GE_GBI_COMMAND_VERTEX);
    assert(log.kinds[1] == GE_GBI_COMMAND_DISPLAY_LIST);
    assert(log.kinds[2] == GE_GBI_COMMAND_SET_PRIM_COLOR);
    assert(log.kinds[3] == GE_GBI_COMMAND_DISPLAY_LIST);
    assert(log.kinds[4] == GE_GBI_COMMAND_TRIANGLE4);
    assert(log.kinds[5] == GE_GBI_COMMAND_END_DISPLAY_LIST);
    assert(log.kinds[6] == GE_GBI_COMMAND_TRIANGLE);
    assert(log.kinds[7] == GE_GBI_COMMAND_END_DISPLAY_LIST);
    assert(log.depths[0] == 0U && log.depths[2] == 1U
           && log.depths[4] == 1U && log.depths[6] == 0U);

    assert(log.vertex_count == 3U);
    assert(log.vertices[0].x == -1);
    assert(log.vertices[0].y == 2);
    assert(log.vertices[0].z == (int16_t)-32768);
    assert(log.vertices[0].flag == UINT16_C(0x1234));
    assert(log.vertices[0].texture_s == -64);
    assert(log.vertices[0].texture_t == 32);
    assert(log.vertices[0].red == UINT8_C(0x10));
    assert(log.vertices[0].alpha == UINT8_C(0x40));
    assert(log.vertices[0].cache_slot == 0U);
    assert(log.vertices[1].x == 100);
    assert(log.vertices[1].y == -200);
    assert(log.vertices[1].z == 300);
    assert(log.vertices[2].cache_slot == 2U);
}

static void test_little_endian_vertex(void)
{
    static const uint8_t commands[] = {
        0x10, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x02,
        0x00, 0x00, 0x00, 0xb8, 0x00, 0x00, 0x00, 0x00
    };
    static const uint8_t vertex[] = {
        0x34, 0x12, 0xfe, 0xff, 0x00, 0x80, 0xcd, 0xab,
        0x40, 0x00, 0x80, 0xff, 1, 2, 3, 4
    };
    GeGbiMemoryMap memory;
    GeGbiTraversalConfig config = {2U, 4U};
    GeGbiTraversalResult result;
    EventLog log;

    memset(&log, 0, sizeof(log));
    ge_gbi_memory_map_init(&memory);
    assert(ge_gbi_memory_map_set_segment(&memory, 1U, commands,
                                         sizeof(commands)) == GE_GBI_RESOLVE_OK);
    assert(ge_gbi_memory_map_set_segment(&memory, 2U, vertex,
                                         sizeof(vertex)) == GE_GBI_RESOLVE_OK);
    result = ge_gbi_traverse_display_list(&memory, address(1U, 0U),
                                          GE_GBI_BYTE_ORDER_LITTLE_ENDIAN,
                                          &config, log_event, &log);
    assert(result.status == GE_GBI_TRAVERSAL_OK);
    assert(log.vertices[0].x == (int16_t)0x1234);
    assert(log.vertices[0].y == -2);
    assert(log.vertices[0].z == (int16_t)-32768);
    assert(log.vertices[0].flag == UINT16_C(0xabcd));
    assert(log.vertices[0].texture_s == 64);
    assert(log.vertices[0].texture_t == -128);
    assert(log.vertices[0].red == 1U && log.vertices[0].alpha == 4U);
}

static void test_resolution_and_limits(void)
{
    static const uint8_t branch_cycle[] = {
        0x06, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x08,
        0x06, 0x01, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00
    };
    static const uint8_t nested[] = {
        0x06, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x08,
        0x06, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x10,
        0xb8, 0, 0, 0, 0, 0, 0, 0
    };
    static const uint8_t bad_vertex[] = {
        0x04, 0x20, 0x00, 0x30, 0x02, 0x00, 0x00, 0x00,
        0xb8, 0, 0, 0, 0, 0, 0, 0
    };
    static const uint8_t short_vertices[16] = {0};
    static const uint8_t physical_bytes[16] = {0};
    GeGbiMemoryMap memory;
    GeGbiTraversalConfig config = {4U, 16U};
    GeGbiTraversalResult result;
    EventLog log;
    const uint8_t *resolved = NULL;

    memset(&log, 0, sizeof(log));
    ge_gbi_memory_map_init(&memory);
    assert(ge_gbi_memory_map_set_physical_region(
               &memory, 0U, UINT32_C(0xfffffff8), physical_bytes,
               sizeof(physical_bytes)) == GE_GBI_RESOLVE_INVALID_ARGUMENT);
    assert(ge_gbi_memory_map_set_physical_region(
               &memory, 0U, UINT32_C(0x1000), physical_bytes,
               sizeof(physical_bytes)) == GE_GBI_RESOLVE_OK);
    assert(ge_gbi_memory_resolve_physical(
               &memory, UINT32_C(0x1008), 8U, &resolved)
           == GE_GBI_RESOLVE_OK);
    assert(resolved == physical_bytes + 8U);
    assert(ge_gbi_memory_resolve_physical(
               &memory, UINT32_C(0x1008), 9U, &resolved)
           == GE_GBI_RESOLVE_OUT_OF_BOUNDS);
    assert(ge_gbi_memory_map_set_segment(&memory, 16U, nested,
                                         sizeof(nested))
           == GE_GBI_RESOLVE_INVALID_ARGUMENT);
    assert(ge_gbi_memory_resolve(&memory, address(1U, 0U), 1U,
                                 &resolved) == GE_GBI_RESOLVE_UNMAPPED_SEGMENT);
    result = ge_gbi_traverse_display_list(&memory, address(1U, 0U),
                                          GE_GBI_BYTE_ORDER_BIG_ENDIAN,
                                          &config, log_event, &log);
    assert(result.status == GE_GBI_TRAVERSAL_UNMAPPED_SEGMENT);
    assert(result.commands_visited == 0U);

    assert(ge_gbi_memory_map_set_segment(&memory, 1U, branch_cycle,
                                         sizeof(branch_cycle)) == GE_GBI_RESOLVE_OK);
    result = ge_gbi_traverse_display_list(&memory, address(1U, 0U),
                                          GE_GBI_BYTE_ORDER_BIG_ENDIAN,
                                          &config, log_event, &log);
    assert(result.status == GE_GBI_TRAVERSAL_CYCLE);
    assert(result.commands_visited == 2U);
    assert(result.stop_address.offset == 0U);

    memset(&log, 0, sizeof(log));
    assert(ge_gbi_memory_map_set_segment(&memory, 1U, nested,
                                         sizeof(nested)) == GE_GBI_RESOLVE_OK);
    config.max_call_depth = 1U;
    result = ge_gbi_traverse_display_list(&memory, address(1U, 0U),
                                          GE_GBI_BYTE_ORDER_BIG_ENDIAN,
                                          &config, log_event, &log);
    assert(result.status == GE_GBI_TRAVERSAL_CALL_DEPTH_LIMIT);
    assert(result.commands_visited == 2U);

    memset(&log, 0, sizeof(log));
    config.max_call_depth = 0U;
    result = ge_gbi_traverse_display_list(&memory, address(1U, 0U),
                                          GE_GBI_BYTE_ORDER_BIG_ENDIAN,
                                          &config, log_event, &log);
    assert(result.status == GE_GBI_TRAVERSAL_CALL_DEPTH_LIMIT);
    assert(result.commands_visited == 1U);

    memset(&log, 0, sizeof(log));
    config.max_call_depth = 4U;
    config.max_commands = 1U;
    result = ge_gbi_traverse_display_list(&memory, address(1U, 0U),
                                          GE_GBI_BYTE_ORDER_BIG_ENDIAN,
                                          &config, log_event, &log);
    assert(result.status == GE_GBI_TRAVERSAL_COMMAND_LIMIT);
    assert(result.commands_visited == 1U);

    memset(&log, 0, sizeof(log));
    config.max_commands = 16U;
    assert(ge_gbi_memory_map_set_segment(&memory, 1U, bad_vertex,
                                         sizeof(bad_vertex)) == GE_GBI_RESOLVE_OK);
    assert(ge_gbi_memory_map_set_segment(&memory, 2U, short_vertices,
                                         sizeof(short_vertices)) == GE_GBI_RESOLVE_OK);
    result = ge_gbi_traverse_display_list(&memory, address(1U, 0U),
                                          GE_GBI_BYTE_ORDER_BIG_ENDIAN,
                                          &config, log_event, &log);
    assert(result.status == GE_GBI_TRAVERSAL_OUT_OF_BOUNDS);
    assert(result.commands_visited == 0U);

    memset(&log, 0, sizeof(log));
    log.stop_after = 1U;
    assert(ge_gbi_memory_map_set_segment(&memory, 1U, nested,
                                         sizeof(nested)) == GE_GBI_RESOLVE_OK);
    result = ge_gbi_traverse_display_list(&memory, address(1U, 0U),
                                          GE_GBI_BYTE_ORDER_BIG_ENDIAN,
                                          &config, log_event, &log);
    assert(result.status == GE_GBI_TRAVERSAL_STOPPED);
    assert(result.commands_visited == 1U);
    assert(strcmp(ge_gbi_traversal_status_name(result.status), "stopped") == 0);
}

int main(void)
{
    test_nested_push_branch_and_vertices();
    test_little_endian_vertex();
    test_resolution_and_limits();
    puts("GoldenEye bounded display-list traversal tests passed");
    return 0;
}
