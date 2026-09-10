#ifndef GE_MP_STATE_MODELS_H
#define GE_MP_STATE_MODELS_H
#include "ge_mp_state_props.h"
#include "ge_original_model_callbacks.h"
#include "ge_original_player_gait.h"
#define GE_MP_MODELS_KIND 0xECU
#define GE_MP_MODELS_CAPACITY 1024U
#define GE_MP_MODELS_FIELDS 10U
enum GeMpModelExtent { GE_MP_MODEL_PREFIX,GE_MP_MODEL_FULL };
typedef struct GeMpModelDefinition {
    const void *model;
    size_t bytes;
    uint8_t creation_namespace,extent;
    uint16_t slot;
} GeMpModelDefinition;
typedef enum GeMpModelReferenceKind {
    GE_MP_MODEL_REF_CHARACTER,GE_MP_MODEL_REF_HEADER,GE_MP_MODEL_REF_MATRICES,
    GE_MP_MODEL_REF_RW,GE_MP_MODEL_REF_ATTACHED_MODEL,GE_MP_MODEL_REF_ATTACHED_NODE,
    GE_MP_MODEL_REF_ANIMATION,GE_MP_MODEL_REF_ANIMATION2,
    GE_MP_MODEL_REF_FRAME_A,GE_MP_MODEL_REF_FRAME_B,
    GE_MP_MODEL_REF_FRAME2_A,GE_MP_MODEL_REF_FRAME2_B,
    GE_MP_MODEL_REF_ROOT_CALLBACK,GE_MP_MODEL_REF_FLIP_CALLBACK
} GeMpModelReferenceKind;
typedef struct GeMpStateModelsProviders {
    void *context;
    GeMpStateStatus (*reference)(void *,GeMpModelReferenceKind,const void *,uint64_t *);
    /* Native s32 frame storage is an address or provider-local handle, never
     * an integer state value. Resolve with its owning model/frame context. */
    GeMpStateStatus (*frame_reference)(void *,GeMpModelReferenceKind,const void *model,int32_t word,uint64_t *);
    /* Map the actual function to an agreed semantic role, never registration
     * order or function-address bits. Unknown nonnull callbacks must fail. */
    GeMpStateStatus (*root_role)(void *,GeOriginalModelRootCallback,uint32_t *);
    GeMpStateStatus (*flip_role)(void *,GeOriginalModelFlipCallback,uint32_t *);
} GeMpStateModelsProviders;
typedef struct GeMpStateModelsOwner {
    GeMpStatePropsOwner *props;
    GeMpRegistry *registry;
    uint64_t pool_id,pool_lifetime,ids[GE_MP_MODELS_CAPACITY];
    GeMpModelDefinition definitions[GE_MP_MODELS_CAPACITY];
    size_t count;
    uint8_t live;
} GeMpStateModelsOwner;
typedef struct GeMpModelValues {
    uint8_t extent,creation_namespace;
    uint16_t slot;
    int16_t prefix[2],frames[4];
    int8_t flags[4];
    int32_t unk9c;
    float floats[28];
    uint64_t refs[12];
    uint32_t callbacks[2]; /* root, animation flip */
} GeMpModelValues;
typedef struct GeMpStateModelsSnapshot {
    GeMpStateRecord records[GE_MP_MODELS_CAPACITY];
    GeMpStateField fields[GE_MP_MODELS_CAPACITY][GE_MP_MODELS_FIELDS];
    GeMpModelValues values[GE_MP_MODELS_CAPACITY];
    size_t record_count;
    GeMpStateCoverage coverage;
    uint32_t unresolved_kinds;
} GeMpStateModelsSnapshot;
/* Native prefix is offsetof(Model,anim); full is sizeof(Model). Never pass a
 * 0x20 N64 allocation as a full Model. Extents adapt to native pointer width. */
size_t ge_mp_state_models_native_size(uint8_t extent);
/* All physical descriptors must be key sorted, unique and readable until
 * end, including inactive slots. IDs represent stage-lived physical storage;
 * neither begin nor end fabricates native model allocation/free transitions.
 * End before providers/players/pools are destroyed or registry is replaced. */
GeMpStateStatus ge_mp_state_models_begin(GeMpStateModelsOwner *,GeMpStatePropsOwner *,const GeMpModelDefinition *,size_t);
GeMpStateStatus ge_mp_state_models_end(GeMpStateModelsOwner *);
GeMpStateStatus ge_mp_state_models_actor(void *,const void *,uint64_t *);
GeMpStateStatus ge_mp_state_models_export(const GeMpStateModelsOwner *,const GeMpStateModelsProviders *,GeMpStateModelsSnapshot *);
const GeMpRecordSchema *ge_mp_state_models_schema(void);
#endif
