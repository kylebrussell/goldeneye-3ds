#ifndef GE_GBI_TRAVERSE_H
#define GE_GBI_TRAVERSE_H

#include "ge_gbi_decoder.h"
#include "ge_gbi_matrix.h"
#include "ge_gbi_rsp.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GE_GBI_SEGMENT_COUNT 16U
#define GE_GBI_PHYSICAL_REGION_COUNT 16U
#define GE_GBI_VERTEX_CACHE_SIZE 16U
#define GE_GBI_TRAVERSE_HARD_MAX_DEPTH 64U
#define GE_GBI_TRAVERSE_HARD_MAX_CONTROL_PATH 512U

typedef struct GeGbiMemorySegment {
    const uint8_t *bytes;
    size_t byte_count;
} GeGbiMemorySegment;

typedef struct GeGbiPhysicalRegion {
    uint32_t base_address;
    const uint8_t *bytes;
    size_t byte_count;
} GeGbiPhysicalRegion;

typedef struct GeGbiMemoryMap {
    GeGbiMemorySegment segments[GE_GBI_SEGMENT_COUNT];
    GeGbiPhysicalRegion physical_regions[GE_GBI_PHYSICAL_REGION_COUNT];
} GeGbiMemoryMap;

typedef enum GeGbiResolveStatus {
    GE_GBI_RESOLVE_OK,
    GE_GBI_RESOLVE_INVALID_ARGUMENT,
    GE_GBI_RESOLVE_UNMAPPED_SEGMENT,
    GE_GBI_RESOLVE_OUT_OF_BOUNDS
} GeGbiResolveStatus;

typedef struct GeGbiVertex {
    int16_t x;
    int16_t y;
    int16_t z;
    uint16_t flag;
    int16_t texture_s;
    int16_t texture_t;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
    uint8_t cache_slot;
} GeGbiVertex;

typedef struct GeGbiTraversalEvent {
    GeGbiCommand command;
    GeGbiAddress command_address;
    size_t call_depth;
    uint8_t has_vertex_batch;
    uint8_t vertex_count;
    GeGbiVertex vertices[GE_GBI_VERTEX_CACHE_SIZE];
    uint8_t has_matrix;
    GeGbiMatrix matrix;
    uint8_t has_viewport;
    GeGbiViewport viewport;
    uint8_t has_light;
    GeGbiLight light;
} GeGbiTraversalEvent;

/* Return zero to stop traversal successfully at the current command. */
typedef int (*GeGbiTraversalCallback)(const GeGbiTraversalEvent *event,
                                     void *user_data);

typedef struct GeGbiTraversalConfig {
    size_t max_call_depth;
    size_t max_commands;
} GeGbiTraversalConfig;

/* RSP segment table state that can span consecutive CPU-submitted roots. */
typedef struct GeGbiTraversalRuntimeState {
    uint32_t segment_bases[GE_GBI_SEGMENT_COUNT];
    uint16_t valid_segment_mask;
} GeGbiTraversalRuntimeState;

typedef enum GeGbiTraversalStatus {
    GE_GBI_TRAVERSAL_OK,
    GE_GBI_TRAVERSAL_STOPPED,
    GE_GBI_TRAVERSAL_INVALID_ARGUMENT,
    GE_GBI_TRAVERSAL_UNMAPPED_SEGMENT,
    GE_GBI_TRAVERSAL_OUT_OF_BOUNDS,
    GE_GBI_TRAVERSAL_DECODE_ERROR,
    GE_GBI_TRAVERSAL_CALL_DEPTH_LIMIT,
    GE_GBI_TRAVERSAL_COMMAND_LIMIT,
    GE_GBI_TRAVERSAL_CONTROL_PATH_LIMIT,
    GE_GBI_TRAVERSAL_CYCLE
} GeGbiTraversalStatus;

typedef struct GeGbiTraversalResult {
    GeGbiTraversalStatus status;
    GeGbiStatus decode_status;
    GeGbiAddress stop_address;
    size_t commands_visited;
    size_t vertex_batches;
    size_t vertices_fetched;
    size_t matrices_fetched;
    size_t rsp_payloads_fetched;
    size_t maximum_call_depth;
} GeGbiTraversalResult;

void ge_gbi_memory_map_init(GeGbiMemoryMap *memory);

GeGbiResolveStatus ge_gbi_memory_map_set_segment(GeGbiMemoryMap *memory,
                                                  uint8_t segment,
                                                  const uint8_t *bytes,
                                                  size_t byte_count);

GeGbiResolveStatus ge_gbi_memory_resolve(const GeGbiMemoryMap *memory,
                                         GeGbiAddress address,
                                         size_t required_bytes,
                                         const uint8_t **resolved_bytes);

GeGbiResolveStatus ge_gbi_memory_map_set_physical_region(
    GeGbiMemoryMap *memory,
    size_t region_index,
    uint32_t base_address,
    const uint8_t *bytes,
    size_t byte_count);

GeGbiResolveStatus ge_gbi_memory_resolve_physical(
    const GeGbiMemoryMap *memory,
    uint32_t physical_address,
    size_t required_bytes,
    const uint8_t **resolved_bytes);

GeGbiTraversalResult ge_gbi_traverse_display_list(
    const GeGbiMemoryMap *memory,
    GeGbiAddress root_address,
    GeGbiByteOrder byte_order,
    const GeGbiTraversalConfig *config,
    GeGbiTraversalCallback callback,
    void *user_data);

GeGbiTraversalResult ge_gbi_traverse_display_list_continue(
    const GeGbiMemoryMap *memory,
    GeGbiAddress root_address,
    GeGbiByteOrder byte_order,
    const GeGbiTraversalConfig *config,
    GeGbiTraversalCallback callback,
    void *user_data,
    GeGbiTraversalRuntimeState *runtime_state);

const char *ge_gbi_traversal_status_name(GeGbiTraversalStatus status);

#ifdef __cplusplus
}
#endif

#endif
