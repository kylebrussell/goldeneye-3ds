#include "ge_original_rareware_logo.h"

#include <string.h>

static const GeOriginalRarewarePassDescriptor ge_rareware_passes[] = {
    {0x43e8U, 0x4fe8U, 25U, 18U,
        GE_ORIGINAL_RAREWARE_PRIMITIVE_PRIMARY, 1U},
    {0x44b0U, 0x0020U, 85U, 8U,
        GE_ORIGINAL_RAREWARE_PRIMITIVE_PRIMARY, 0U},
    {0x4758U, 0x5ff0U, 273U, 242U,
        GE_ORIGINAL_RAREWARE_PRIMITIVE_SECONDARY, 1U},
};

typedef struct GeOriginalRarewareBuild {
    GeOriginalRarewareVertex *vertices;
    size_t capacity;
    size_t count;
    int overflow;
} GeOriginalRarewareBuild;

static uint8_t texture_index(uint32_t address)
{
    static const uint32_t offsets[] = {
        0x0020U, 0x0ae0U, 0x15a0U, 0x2060U, 0x4fe8U, 0x5ff0U,
    };
    size_t index;
    for (index = 0U; index < sizeof(offsets) / sizeof(offsets[0]); ++index) {
        if (address == (UINT32_C(0x02000000) | offsets[index]))
            return (uint8_t)index;
    }
    return UINT8_MAX;
}

static int collect_vertices(const GeGbiPipelineEvent *event, void *context)
{
    GeOriginalRarewareBuild *build = context;
    uint8_t triangle_index;
    if (event->action.kind != GE_GBI_STATE_ACTION_DRAW_TRIANGLES) return 1;
    for (triangle_index = 0U;
            triangle_index < event->action.data.draw.count;
            ++triangle_index) {
        const GeGbiTriangle *triangle =
            &event->action.data.draw.triangles[triangle_index];
        uint8_t corner;
        for (corner = 0U; corner < 3U; ++corner) {
            if (build->count >= build->capacity) {
                build->overflow = 1;
                ++build->count;
                continue;
            }
            build->vertices[build->count].source =
                event->vertex_cache[triangle->vertex[corner]];
            build->vertices[build->count].texture_index =
                texture_index(event->state->texture_image.address.raw);
            ++build->count;
        }
    }
    return 1;
}

const GeOriginalRarewarePassDescriptor *
ge_original_rareware_pass_descriptor(GeOriginalRarewarePass pass)
{
    return (unsigned int)pass < GE_ORIGINAL_RAREWARE_PASS_COUNT
        ? &ge_rareware_passes[(unsigned int)pass] : NULL;
}

GeOriginalRarewareStatus ge_original_rareware_mesh_build(
    const uint8_t *segment, size_t segment_size, GeOriginalRarewarePass pass,
    GeOriginalRarewareVertex *vertices, size_t vertex_capacity,
    GeOriginalRarewareMesh *mesh)
{
    const GeOriginalRarewarePassDescriptor *descriptor =
        ge_original_rareware_pass_descriptor(pass);
    GeGbiMemoryMap memory;
    GeGbiTraversalConfig config = {32U, 20000U};
    GeOriginalRarewareBuild build = {vertices, vertex_capacity, 0U, 0};
    GeGbiAddress root;
    GeGbiPipelineResult pipeline;
    if (segment == NULL || mesh == NULL || descriptor == NULL
            || (vertices == NULL && vertex_capacity != 0U))
        return GE_ORIGINAL_RAREWARE_INVALID_ARGUMENT;
    memset(mesh, 0, sizeof(*mesh));
    mesh->pass = pass;
    mesh->required_vertex_count = descriptor->triangle_count * 3U;
    if (segment_size != GE_ORIGINAL_RAREWARE_SEGMENT_BYTES)
        return GE_ORIGINAL_RAREWARE_INVALID_SEGMENT;
    ge_gbi_memory_map_init(&memory);
    if (ge_gbi_memory_map_set_segment(&memory, 2U, segment, segment_size)
            != GE_GBI_RESOLVE_OK)
        return GE_ORIGINAL_RAREWARE_INVALID_SEGMENT;
    root.raw = UINT32_C(0x02000000) | descriptor->display_list_offset;
    root.offset = descriptor->display_list_offset;
    root.segment = 2U;
    pipeline = ge_gbi_pipeline_execute(
        &memory, root, GE_GBI_BYTE_ORDER_BIG_ENDIAN, &config,
        collect_vertices, &build);
    mesh->commands_visited = pipeline.traversal.commands_visited;
    mesh->triangle_count = pipeline.triangles;
    mesh->vertex_count = build.count <= vertex_capacity
        ? build.count : vertex_capacity;
    if (pipeline.status != GE_GBI_PIPELINE_OK
            || pipeline.unsupported_commands != 0U)
        return GE_ORIGINAL_RAREWARE_PIPELINE_ERROR;
    if (pipeline.traversal.commands_visited != descriptor->command_count
            || pipeline.triangles != descriptor->triangle_count
            || build.count != mesh->required_vertex_count)
        return GE_ORIGINAL_RAREWARE_UNEXPECTED_GEOMETRY;
    if (build.overflow || vertex_capacity < mesh->required_vertex_count)
        return GE_ORIGINAL_RAREWARE_CAPACITY_EXCEEDED;
    return GE_ORIGINAL_RAREWARE_OK;
}
