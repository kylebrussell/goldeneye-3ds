#include "ge_gbi_pipeline.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

enum {
    RAREWARE_ROM_START = 0x29e560,
    RAREWARE_ROM_END = 0x2a4d50,
    RAREWARE_SEGMENT_SIZE = RAREWARE_ROM_END - RAREWARE_ROM_START,
    RAREWARE_ROOT_OFFSET = 0x44b0
};

typedef struct RarewareAudit {
    size_t actions;
    size_t draws;
    size_t triangles;
    size_t command_kinds[GE_GBI_COMMAND_FULL_SYNC + 1U];
    int minimum[3];
    int maximum[3];
    uint64_t minimum_twice_area;
    uint64_t maximum_twice_area;
} RarewareAudit;

static GeGbiAddress rareware_address(uint32_t offset)
{
    GeGbiAddress address;

    address.raw = UINT32_C(0x02000000) | offset;
    address.segment = 2U;
    address.offset = offset;
    return address;
}

static int audit_action(const GeGbiPipelineEvent *event, void *user_data)
{
    RarewareAudit *audit = (RarewareAudit *)user_data;

    ++audit->actions;
    if (event->action.kind == GE_GBI_STATE_ACTION_DRAW_TRIANGLES) {
        uint8_t triangle_index;

        ++audit->draws;
        audit->triangles += event->action.data.draw.count;
        for (triangle_index = 0U;
                triangle_index < event->action.data.draw.count;
                triangle_index++) {
            const GeGbiTriangle *triangle =
                &event->action.data.draw.triangles[triangle_index];
            uint8_t vertex_index;
            int coordinates_by_vertex[3][2];

            for (vertex_index = 0U; vertex_index < 3U; vertex_index++) {
                const GeGbiVertex *vertex =
                    &event->vertex_cache[triangle->vertex[vertex_index]];
                const int coordinates[3] = {vertex->x, vertex->y, vertex->z};
                size_t axis;

                coordinates_by_vertex[vertex_index][0] = vertex->x;
                coordinates_by_vertex[vertex_index][1] = vertex->y;

                for (axis = 0U; axis < 3U; axis++) {
                    if (coordinates[axis] < audit->minimum[axis]) {
                        audit->minimum[axis] = coordinates[axis];
                    }
                    if (coordinates[axis] > audit->maximum[axis]) {
                        audit->maximum[axis] = coordinates[axis];
                    }
                }
            }
            {
                const int64_t twice_area =
                    (int64_t)(coordinates_by_vertex[1][0]
                              - coordinates_by_vertex[0][0])
                        * (coordinates_by_vertex[2][1]
                           - coordinates_by_vertex[0][1])
                    - (int64_t)(coordinates_by_vertex[1][1]
                                - coordinates_by_vertex[0][1])
                        * (coordinates_by_vertex[2][0]
                           - coordinates_by_vertex[0][0]);
                const uint64_t absolute_area = twice_area < 0
                    ? (uint64_t)(-twice_area) : (uint64_t)twice_area;

                if (absolute_area < audit->minimum_twice_area) {
                    audit->minimum_twice_area = absolute_area;
                }
                if (absolute_area > audit->maximum_twice_area) {
                    audit->maximum_twice_area = absolute_area;
                }
            }
        }
    }
    return 1;
}

static int audit_command(const GeGbiTraversalEvent *event, void *user_data)
{
    RarewareAudit *audit = (RarewareAudit *)user_data;

    assert((unsigned int)event->command.kind
           < sizeof(audit->command_kinds) / sizeof(audit->command_kinds[0]));
    ++audit->command_kinds[event->command.kind];
    return 1;
}

static uint64_t fnv1a64(const uint8_t *bytes, size_t byte_count)
{
    uint64_t hash = UINT64_C(0xcbf29ce484222325);
    size_t index;

    for (index = 0U; index < byte_count; ++index) {
        hash ^= bytes[index];
        hash *= UINT64_C(0x100000001b3);
    }
    return hash;
}

static int read_rareware_segment(uint8_t *segment, size_t segment_size)
{
    static const char *const paths[] = {
        "build/u/ge007.u.z64",
        "baserom.u.z64"
    };
    size_t index;

    for (index = 0U; index < sizeof(paths) / sizeof(paths[0]); ++index) {
        FILE *rom = fopen(paths[index], "rb");

        if (rom != NULL) {
            const int seek_status = fseek(rom, RAREWARE_ROM_START, SEEK_SET);
            const size_t bytes_read = seek_status == 0
                ? fread(segment, 1U, segment_size, rom) : 0U;
            const int close_status = fclose(rom);

            if (bytes_read == segment_size && close_status == 0) {
                return 1;
            }
        }
    }
    return 0;
}

int main(void)
{
    uint8_t *segment = (uint8_t *)malloc(RAREWARE_SEGMENT_SIZE);
    GeGbiMemoryMap memory;
    GeGbiTraversalConfig config = {32U, 20000U};
    GeGbiTraversalResult traversal;
    GeGbiPipelineResult result;
    GeGbiPipelineResult front_model;
    GeGbiPipelineResult body_model;
    RarewareAudit audit = {0};
    RarewareAudit body_audit = {0};

    audit.minimum[0] = INT_MAX;
    audit.minimum[1] = INT_MAX;
    audit.minimum[2] = INT_MAX;
    audit.maximum[0] = INT_MIN;
    audit.maximum[1] = INT_MIN;
    audit.maximum[2] = INT_MIN;
    audit.minimum_twice_area = UINT64_MAX;
    body_audit.minimum[0] = INT_MAX;
    body_audit.minimum[1] = INT_MAX;
    body_audit.minimum[2] = INT_MAX;
    body_audit.maximum[0] = INT_MIN;
    body_audit.maximum[1] = INT_MIN;
    body_audit.maximum[2] = INT_MIN;
    body_audit.minimum_twice_area = UINT64_MAX;

    assert(segment != NULL);
    assert(read_rareware_segment(segment, RAREWARE_SEGMENT_SIZE) != 0);
    assert(fnv1a64(segment, RAREWARE_SEGMENT_SIZE)
           == UINT64_C(0x711748dede1398e8));
    ge_gbi_memory_map_init(&memory);
    assert(ge_gbi_memory_map_set_segment(&memory, 2U, segment,
                                         RAREWARE_SEGMENT_SIZE)
           == GE_GBI_RESOLVE_OK);

    traversal = ge_gbi_traverse_display_list(
        &memory, rareware_address(RAREWARE_ROOT_OFFSET),
        GE_GBI_BYTE_ORDER_BIG_ENDIAN, &config, audit_command, &audit);
    assert(traversal.status == GE_GBI_TRAVERSAL_OK);
    assert(traversal.commands_visited == 85U);
    assert(traversal.vertex_batches == 2U);
    assert(traversal.vertices_fetched == 30U);
    assert(audit.command_kinds[GE_GBI_COMMAND_CLEAR_GEOMETRY_MODE] == 1U);
    assert(audit.command_kinds[GE_GBI_COMMAND_TEXTURE] == 1U);
    assert(audit.command_kinds[GE_GBI_COMMAND_PIPE_SYNC] == 4U);
    assert(audit.command_kinds[GE_GBI_COMMAND_SET_OTHER_MODE_HIGH] == 2U);
    assert(audit.command_kinds[GE_GBI_COMMAND_SET_OTHER_MODE_LOW] == 1U);
    assert(audit.command_kinds[GE_GBI_COMMAND_SET_COMBINE] == 1U);
    assert(audit.command_kinds[GE_GBI_COMMAND_SET_TEXTURE_IMAGE] == 4U);
    assert(audit.command_kinds[GE_GBI_COMMAND_SET_TILE] == 28U);
    assert(audit.command_kinds[GE_GBI_COMMAND_LOAD_SYNC] == 4U);
    assert(audit.command_kinds[GE_GBI_COMMAND_LOAD_BLOCK] == 4U);
    assert(audit.command_kinds[GE_GBI_COMMAND_SET_TILE_SIZE] == 24U);
    assert(audit.command_kinds[GE_GBI_COMMAND_VERTEX] == 2U);
    assert(audit.command_kinds[GE_GBI_COMMAND_TRIANGLE] == 8U);
    assert(audit.command_kinds[GE_GBI_COMMAND_END_DISPLAY_LIST] == 1U);

    result = ge_gbi_pipeline_execute(&memory,
                                     rareware_address(RAREWARE_ROOT_OFFSET),
                                     GE_GBI_BYTE_ORDER_BIG_ENDIAN, &config,
                                     audit_action, &audit);
    if (result.status != GE_GBI_PIPELINE_OK) {
        fprintf(stderr,
                "Rareware pipeline failure: pipeline=%d traversal=%s "
                "decode=%d state=%d at 02%06x after %zu commands\n",
                (int)result.status,
                ge_gbi_traversal_status_name(result.traversal.status),
                (int)result.traversal.decode_status,
                (int)result.state_status,
                (unsigned int)result.traversal.stop_address.offset,
                result.traversal.commands_visited);
    }

    assert(result.status == GE_GBI_PIPELINE_OK);
    assert(result.traversal.status == GE_GBI_TRAVERSAL_OK);
    assert(result.traversal.commands_visited == 85U);
    assert(result.actions_emitted == 11U);
    assert(result.draw_calls == 8U);
    assert(result.triangles == 8U);
    assert(result.unsupported_commands == 0U);
    assert(result.draw_calls == audit.draws);
    assert(result.triangles == audit.triangles);
    assert(audit.minimum[0] == -103 && audit.maximum[0] == 124);
    assert(audit.minimum[1] == -157 && audit.maximum[1] == -121);
    assert(audit.minimum[2] == 11 && audit.maximum[2] == 11);
    assert(audit.minimum_twice_area == UINT64_C(2016));
    assert(audit.maximum_twice_area == UINT64_C(2052));
    printf("Rareware bounds: x=%d..%d y=%d..%d z=%d..%d\n",
           audit.minimum[0], audit.maximum[0],
           audit.minimum[1], audit.maximum[1],
           audit.minimum[2], audit.maximum[2]);
    printf("Rareware triangle twice-area: %llu..%llu\n",
           (unsigned long long)audit.minimum_twice_area,
           (unsigned long long)audit.maximum_twice_area);
    printf("Rareware logo: %zu commands, %zu draws, %zu triangles, "
           "%zu unsupported\n",
           result.traversal.commands_visited, result.draw_calls,
           result.triangles, result.unsupported_commands);

    front_model = ge_gbi_pipeline_execute(
        &memory, rareware_address(0x43e8U),
        GE_GBI_BYTE_ORDER_BIG_ENDIAN, &config, NULL, NULL);
    body_model = ge_gbi_pipeline_execute(
        &memory, rareware_address(0x4758U),
        GE_GBI_BYTE_ORDER_BIG_ENDIAN, &config, audit_action, &body_audit);
    printf("Rareware model lists: front=%d/%zu/%zu/%zu body=%d/%zu/%zu/%zu\n",
           (int)front_model.status, front_model.traversal.commands_visited,
           front_model.draw_calls, front_model.triangles,
           (int)body_model.status, body_model.traversal.commands_visited,
           body_model.draw_calls, body_model.triangles);
    assert(front_model.status == GE_GBI_PIPELINE_OK);
    assert(front_model.traversal.commands_visited == 25U);
    assert(front_model.draw_calls == 18U);
    assert(front_model.triangles == 18U);
    assert(front_model.unsupported_commands == 0U);
    assert(body_model.status == GE_GBI_PIPELINE_OK);
    assert(body_model.traversal.commands_visited == 273U);
    assert(body_model.draw_calls == 242U);
    assert(body_model.triangles == 242U);
    assert(body_model.unsupported_commands == 0U);
    assert(body_audit.minimum[0] == -143 && body_audit.maximum[0] == 165);
    assert(body_audit.minimum[1] == -207 && body_audit.maximum[1] == 208);
    assert(body_audit.minimum[2] == -14 && body_audit.maximum[2] == 15);
    assert(body_audit.minimum_twice_area == UINT64_C(0));
    assert(body_audit.maximum_twice_area == UINT64_C(81504));
    printf("Rareware body bounds: x=%d..%d y=%d..%d z=%d..%d\n",
           body_audit.minimum[0], body_audit.maximum[0],
           body_audit.minimum[1], body_audit.maximum[1],
           body_audit.minimum[2], body_audit.maximum[2]);
    printf("Rareware body projected twice-area: %llu..%llu\n",
           (unsigned long long)body_audit.minimum_twice_area,
           (unsigned long long)body_audit.maximum_twice_area);

    free(segment);
    return 0;
}
