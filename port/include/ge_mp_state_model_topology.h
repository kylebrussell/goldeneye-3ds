#ifndef GE_MP_STATE_MODEL_TOPOLOGY_H
#define GE_MP_STATE_MODEL_TOPOLOGY_H
#include "ge_mp_state_props.h"
#define GE_MP_MODEL_TOPOLOGY_KIND 0xEDU
#define GE_MP_MODEL_TOPOLOGY_CAPACITY 2048U
#define GE_MP_MODEL_TOPOLOGY_FIELDS 8U
enum GeMpModelTopologyComponent {GE_MP_TOPOLOGY_NODE=1,GE_MP_TOPOLOGY_HEADER=2};
typedef struct GeMpModelTopologyDefinition {
    const void *native;
    size_t bytes;
    uint8_t creation_namespace,component;
    uint16_t slot;
} GeMpModelTopologyDefinition;
typedef enum GeMpModelTopologyReferenceKind {
    GE_MP_TOPOLOGY_REF_NODE,GE_MP_TOPOLOGY_REF_RODATA,GE_MP_TOPOLOGY_REF_SKELETON,
    GE_MP_TOPOLOGY_REF_SWITCH_TABLE,GE_MP_TOPOLOGY_REF_TEXTURE_TABLE,GE_MP_TOPOLOGY_REF_SWITCH_NAMES
} GeMpModelTopologyReferenceKind;
typedef struct GeMpStateModelTopologyProviders {
    void *context;
    GeMpStateStatus (*reference)(void *,GeMpModelTopologyReferenceKind,const void *,uint64_t *);
} GeMpStateModelTopologyProviders;
typedef struct GeMpStateModelTopologyOwner {
    GeMpStatePropsOwner *props;
    GeMpRegistry *registry;
    uint64_t pool_id,pool_lifetime,ids[GE_MP_MODEL_TOPOLOGY_CAPACITY];
    GeMpModelTopologyDefinition definitions[GE_MP_MODEL_TOPOLOGY_CAPACITY];
    size_t count;
    uint8_t live;
} GeMpStateModelTopologyOwner;
typedef struct GeMpModelTopologyValues {
    uint8_t component,creation_namespace;
    uint16_t opcode,slot;
    uint64_t refs[5];
    int16_t counts[4];
    float radius;
    int32_t is_loaded;
} GeMpModelTopologyValues;
typedef struct GeMpStateModelTopologySnapshot {
    GeMpStateRecord records[GE_MP_MODEL_TOPOLOGY_CAPACITY];
    GeMpStateField fields[GE_MP_MODEL_TOPOLOGY_CAPACITY][GE_MP_MODEL_TOPOLOGY_FIELDS];
    GeMpModelTopologyValues values[GE_MP_MODEL_TOPOLOGY_CAPACITY];
    size_t record_count;
    GeMpStateCoverage coverage;
    uint32_t unresolved_kinds;
} GeMpStateModelTopologySnapshot;
size_t ge_mp_state_model_topology_native_size(uint8_t component);
/* Physical stage-lived native headers/nodes, exactly once per address. Begin
 * after E3; end before containing providers/storage and E3 are destroyed.
 * Key order is caller manifest policy, not node traversal/registration order. */
GeMpStateStatus ge_mp_state_model_topology_begin(GeMpStateModelTopologyOwner *,GeMpStatePropsOwner *,const GeMpModelTopologyDefinition *,size_t);
/* Add newly loaded stage-lived objects without changing existing IDs. Input
 * is key-sorted, has only new keys/addresses, and cannot alias owner storage.
 * Failure is transactional. No native allocation is fabricated by this API. */
GeMpStateStatus ge_mp_state_model_topology_add(GeMpStateModelTopologyOwner *,const GeMpModelTopologyDefinition *,size_t);
GeMpStateStatus ge_mp_state_model_topology_end(GeMpStateModelTopologyOwner *);
GeMpStateStatus ge_mp_state_model_topology_actor(void *,const void *,uint64_t *);
/* Bounds/provenance check for a later typed reader before it dereferences a
 * known native header/node. Still requires the containing-storage lease. */
GeMpStateStatus ge_mp_state_model_topology_check(const GeMpStateModelTopologyOwner *,const void *,uint8_t component);
GeMpStateStatus ge_mp_state_model_topology_export(const GeMpStateModelTopologyOwner *,const GeMpStateModelTopologyProviders *,GeMpStateModelTopologySnapshot *);
const GeMpRecordSchema *ge_mp_state_model_topology_schema(void);
#endif
