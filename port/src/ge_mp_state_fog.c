#include "ge_mp_state_fog.h"
#include <math.h>
#include <string.h>

#define SCHEMA(id,type,member,min,max,count) {id,GE_MP_STATE_##type,0,min,max},
static const GeMpFieldSchema fields[]={GE_MP_FOG_FIELDS(SCHEMA)};
#undef SCHEMA
_Static_assert(sizeof(fields)/sizeof(fields[0])==GE_MP_FOG_FIELD_COUNT,
    "Review fog field count");
static const GeMpRecordSchema schema={GE_MP_FOG_KIND,GE_MP_SYSTEM_OBJECTS,
    fields,GE_MP_FOG_FIELD_COUNT};
const GeMpRecordSchema *ge_mp_state_fog_schema(void) { return &schema; }

static GeMpStateStatus valid(const GeMpStateFogOwner *owner)
{
    if (!owner || !owner->registry || !owner->actor_id || !owner->lifetime
            || owner->lifetime!=ge_original_multiplayer_fog_lifetime())
        return GE_MP_STATE_STALE;
    if ((uint32_t)(owner->actor_id>>32)!=ge_mp_registry_key(GE_MP_FOG_KIND,
            GE_MP_FOG_NAMESPACE,GE_MP_FOG_SLOT)) return GE_MP_STATE_INVALID;
    return ge_mp_registry_resolve(owner->registry,owner->actor_id,NULL);
}

GeMpStateStatus ge_mp_state_fog_begin(GeMpStateFogOwner *owner,GeMpRegistry *registry)
{
    if (!owner || !registry || !registry->entries || registry->count>registry->capacity)
        return GE_MP_STATE_INVALID;
    if (owner->actor_id || owner->lifetime || owner->registry) return GE_MP_STATE_STALE;
    const uint64_t lifetime=ge_original_multiplayer_fog_lifetime();
    if (!lifetime) return GE_MP_STATE_INCOMPLETE;
    const uint32_t key=ge_mp_registry_key(GE_MP_FOG_KIND,GE_MP_FOG_NAMESPACE,GE_MP_FOG_SLOT);
    size_t index;
    for (index=0;index<registry->count;++index)
        if (registry->entries[index].key==key) break;
    GeMpStateStatus status;
    if (index==registry->count) {
        status=ge_mp_registry_define(registry,key);
        if (status!=GE_MP_STATE_OK) return status;
    }
    uint64_t actor=0;
    status=ge_mp_registry_acquire(registry,key,&actor);
    if (status!=GE_MP_STATE_OK) return status;
    *owner=(GeMpStateFogOwner){registry,actor,lifetime};
    return GE_MP_STATE_OK;
}

GeMpStateStatus ge_mp_state_fog_end(GeMpStateFogOwner *owner)
{
    if (!owner || !owner->registry || !owner->actor_id) return GE_MP_STATE_STALE;
    GeMpStateStatus status=valid(owner);
    GeMpStateStatus released=ge_mp_registry_release(owner->registry,owner->actor_id);
    if (released!=GE_MP_STATE_OK) status=released;
    memset(owner,0,sizeof(*owner));
    return status;
}

GeMpStateStatus ge_mp_state_fog_capture(const GeMpStateFogOwner *owner,
    GeMpStateFogSnapshot *snapshot)
{
    if (!snapshot) return GE_MP_STATE_INVALID;
    memset(snapshot,0,sizeof(*snapshot));
    GeMpStateStatus status=valid(owner);
    if (status!=GE_MP_STATE_OK) return status;
    GeOriginalMultiplayerFogState *values=&snapshot->values;
    if (!ge_original_multiplayer_fog_state_read(values)) {
        snapshot->coverage.unresolved_references=1;
        return GE_MP_STATE_INCOMPLETE;
    }
    size_t index=0;
#define FIELD(id,type,member,min,max,count) \
    snapshot->fields[index++]=(GeMpStateField){id,GE_MP_STATE_##type,count,&values->member};
    GE_MP_FOG_FIELDS(FIELD)
#undef FIELD
    for (size_t field=0;field<index;++field) {
        const GeMpStateField *f=&snapshot->fields[field];
        if (f->type==GE_MP_STATE_F32)
            for (size_t i=0;i<f->count;++i)
                if (!isfinite(((const float *)f->values)[i])) {
                    snapshot->coverage.unclassified_fields=1;
                    return GE_MP_STATE_INCOMPLETE;
                }
    }
    snapshot->record=(GeMpStateRecord){owner->actor_id,GE_MP_FOG_KIND,
        snapshot->fields,GE_MP_FOG_FIELD_COUNT};
    snapshot->record_count=1;
    snapshot->coverage.enumerated_systems=GE_MP_SYSTEM_OBJECTS;
    return GE_MP_STATE_OK;
}
