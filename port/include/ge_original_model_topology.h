#ifndef GE_ORIGINAL_MODEL_TOPOLOGY_H
#define GE_ORIGINAL_MODEL_TOPOLOGY_H
#include <stddef.h>
#include <stdint.h>

/* Borrowed native storage. No loading, traversal, relation update or RNG.
 * A resource view names shared nodes; an instance view names cloned nodes.
 * Source offsets describe authored node positions, never host addresses. */
typedef struct GeOriginalModelTopologyView {
    const void *header;
    size_t header_bytes;
    const void *nodes;
    size_t node_count,node_stride;
    const uint32_t *source_node_offsets;
    const void *rodatas;
    size_t rodata_stride;
    int32_t model_id;
} GeOriginalModelTopologyView;

/* Exact static embedded player-gait physical objects in authored order.
 * The four ModelNodes are distinct objects, not a fabricated C array. */
const void *ge_original_player_gait_topology_header(size_t *bytes);
size_t ge_original_player_gait_topology_node_count(void);
int ge_original_player_gait_topology_node(size_t index,const void **node,size_t *bytes);
#endif
