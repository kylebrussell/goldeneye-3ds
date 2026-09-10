#ifndef GE_MP_STATE_PROPS_H
#define GE_MP_STATE_PROPS_H
#include "ge_mp_state.h"
#include "ge_original_prop_state.h"
#define GE_MP_PROPS_KIND 0xE3U
#define GE_MP_PROP_POOL_KIND 0xE5U
#define GE_MP_PROP_CAPACITY 600U
#define GE_MP_PROP_ROOM_CAPACITY 256U
#define GE_MP_PROP_FIELDS 20U
#define GE_MP_PROP_POOL_FIELDS 17U

typedef enum GeMpPropReferenceKind {
    GE_MP_PROP_REF_OBJECT, GE_MP_PROP_REF_DOOR, GE_MP_PROP_REF_WEAPON,
    GE_MP_PROP_REF_CHARACTER, GE_MP_PROP_REF_EXPLOSION, GE_MP_PROP_REF_SMOKE,
    GE_MP_PROP_REF_STAN, GE_MP_PROP_REF_PLAYER
} GeMpPropReferenceKind;
typedef struct GeMpStatePropsProviders {
    void *context;
    GeMpStateStatus (*reference)(void *,GeMpPropReferenceKind,const void *,uint64_t *);
} GeMpStatePropsProviders;
typedef struct GeMpStatePropsOwner {
    GeMpRegistry *registry;
    GeOriginalPropPoolStorage storage;
    uint64_t ids[600],pool_id,allocations_observed;
    GeMpStateStatus fault;
    uint8_t live,stage_ended;
} GeMpStatePropsOwner;
typedef struct GeMpPropValues {
    uint8_t type,flags,rooms[4];
    int16_t timer;
    int32_t unk30;
    float position[3],zdepth;
    /* object,door,weapon,character,explosion,smoke,stan,parent,child,prev,next,player */
    uint64_t refs[12];
} GeMpPropValues;
typedef struct GeMpPropPoolValues {
    uint64_t active[600],room_members[3840],head,tail;
    uint32_t free_order[600],free_head,room_capacity,block_members[3840];
    int16_t room_heads[256],block_next[256];
    uint16_t room_counts[256];
    uint8_t block_free[256],free_type[600],free_rooms[2400];
    float free_position[1800],free_zdepth[600];
    int32_t free_unk30[600];
} GeMpPropPoolValues;
typedef struct GeMpStatePropsSnapshot {
    GeMpStateRecord records[601];
    GeMpStateField prop_fields[600][GE_MP_PROP_FIELDS],pool_fields[GE_MP_PROP_POOL_FIELDS];
    GeMpPropValues props[600];
    GeMpPropPoolValues pool;
    uint64_t child_order[600];
    size_t record_count;
    GeMpStateCoverage coverage;
} GeMpStatePropsSnapshot;

/* Zero-init owner. After prop_state_reset, before ANY allocation (even one
 * subsequently freed), define all 600 E3/ns255 keys and acquire E5/ns255/0.
 * Optional native callbacks acquire/release E3 on every allocation/free.
 * Registry failures invalidate this domain without changing native behavior. */
GeMpStateStatus ge_mp_state_props_begin(GeMpStatePropsOwner *,GeMpRegistry *);
/* After native cleanup, before next reset. Individual removal can free every
 * prop, or ge_original_prop_state_end can retire the whole stage pool after
 * its consumers close. That distinct event validates the physical free list
 * and observed allocation history before retiring residual IDs. End unbinds
 * even on error; missed lifecycle events still return INCOMPLETE. */
GeMpStateStatus ge_mp_state_props_end(GeMpStatePropsOwner *);
GeMpStateStatus ge_mp_state_props_export(const GeMpStatePropsOwner *,
    const GeMpStatePropsProviders *,GeMpStatePropsSnapshot *);
/* Adapter usable directly by the inventory domain's prop_actor callback. */
GeMpStateStatus ge_mp_state_props_actor(void *owner,const void *native_prop,uint64_t *actor_id);
const GeMpRecordSchema *ge_mp_state_props_schemas(size_t *count);
#endif
