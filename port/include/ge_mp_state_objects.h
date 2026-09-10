#ifndef GE_MP_STATE_OBJECTS_H
#define GE_MP_STATE_OBJECTS_H
#include "ge_mp_state_props.h"
#define GE_MP_OBJECTS_KIND 0xEAU
#define GE_MP_OBJECTS_CAPACITY 1024U
#define GE_MP_OBJECTS_FIELDS 21U
/* These IDs identify physical definition containers within one stage, not
 * individual PropRecord allocations. Descriptor keys must be strictly sorted. */
typedef struct GeMpObjectDefinition {
    const void *definition;
    size_t bytes;
    uint8_t creation_namespace,type;
    uint16_t slot;
} GeMpObjectDefinition;
typedef enum GeMpObjectReferenceKind {
    GE_MP_OBJECT_REF_PROP,GE_MP_OBJECT_REF_MODEL,GE_MP_OBJECT_REF_COLLISION,
    GE_MP_OBJECT_REF_PROJECTILE,GE_MP_OBJECT_REF_EMBEDMENT,
    GE_MP_OBJECT_REF_LINKED_OBJECT
} GeMpObjectReferenceKind;
typedef struct GeMpStateObjectsProviders {
    void *context;
    GeMpStateStatus (*reference)(void *,GeMpObjectReferenceKind,const void *,uint64_t *);
} GeMpStateObjectsProviders;
typedef struct GeMpStateObjectsOwner {
    GeMpStatePropsOwner *props;
    GeMpRegistry *registry;
    uint64_t pool_id,pool_lifetime,ids[GE_MP_OBJECTS_CAPACITY];
    GeMpObjectDefinition definitions[GE_MP_OBJECTS_CAPACITY];
    size_t count;
    uint8_t live;
} GeMpStateObjectsOwner;
typedef struct GeMpObjectValues {
    uint8_t type,state,variant,creation_namespace,colors[8];
    uint16_t extrascale,slot_models[13],quantities[13];
    int16_t model_pad[2],timer;
    int8_t weapons[2];
    int32_t ammo;
    uint32_t flags[3];
    float pose[21],armour[2];
    uint64_t prop,model,collision,projectile,embedment,dualweapon;
} GeMpObjectValues;
typedef struct GeMpStateObjectsSnapshot {
    GeMpStateRecord records[GE_MP_OBJECTS_CAPACITY];
    GeMpStateField fields[GE_MP_OBJECTS_CAPACITY][GE_MP_OBJECTS_FIELDS];
    GeMpObjectValues values[GE_MP_OBJECTS_CAPACITY];
    size_t record_count;
    GeMpStateCoverage coverage;
    uint32_t unresolved_kinds;
} GeMpStateObjectsSnapshot;
/* Native sizeof for exactly the four supported source record types; zero
 * means unsupported. No guessed offsets or serialized byte layout. */
size_t ge_mp_state_objects_native_size(uint8_t type);
/* Actual fixed native g_AmmoCrates physical slots, including free records. */
size_t ge_mp_state_objects_native_ammo_definitions(uint8_t creation_namespace,
    GeMpObjectDefinition *definitions,size_t capacity);
/* Begin after E3 and native definition allocation. All definitions, including
 * free dynamic containers, must remain readable at stable addresses until end.
 * End this owner BEFORE destroying their containing stage/runtime owners or
 * resetting/replacing its registry. */
GeMpStateStatus ge_mp_state_objects_begin(GeMpStateObjectsOwner *,
    GeMpStatePropsOwner *,const GeMpObjectDefinition *,size_t);
GeMpStateStatus ge_mp_state_objects_end(GeMpStateObjectsOwner *);
GeMpStateStatus ge_mp_state_objects_export(const GeMpStateObjectsOwner *,
    const GeMpStateObjectsProviders *,GeMpStateObjectsSnapshot *);
GeMpStateStatus ge_mp_state_objects_actor(void *owner,const void *definition,uint64_t *);
const GeMpRecordSchema *ge_mp_state_objects_schema(void);
#endif
