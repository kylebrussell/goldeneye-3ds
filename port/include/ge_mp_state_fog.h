#ifndef GE_MP_STATE_FOG_H
#define GE_MP_STATE_FOG_H
#include "ge_mp_state.h"
#include "ge_original_multiplayer_fog_state.h"

#define GE_MP_FOG_KIND 0xebU
#define GE_MP_FOG_NAMESPACE 255U
#define GE_MP_FOG_SLOT 0U
#define GE_MP_FOG_FIELD_COUNT 25U
#define GE_MP_FOG_FIELDS(X) \
 X(256,I32,level_id,1,1,1) \
 X(257,U32,player_count,1,1,1) \
 X(258,U8,cinematic,1,1,1) \
 X(259,F32,visibility_scale,1,1,1) \
 X(260,I32,enabled,1,1,1) \
 X(261,F32,zrange,2,2,2) \
 X(262,I32,current_fog,2,2,2) \
 X(263,U8,current_sky,5,5,5) \
 X(264,I16,current_images,2,2,2) \
 X(265,F32,current_parameters,9,9,9) \
 X(266,F32,details,4,4,4) \
 X(267,F32,intensities,4,4,4) \
 X(268,I32,bindings,4,4,4) \
 X(269,U32,environment_count,1,1,1) \
 X(270,U32,fogless_count,1,1,1) \
 X(271,U32,environment_u32,2,GE_ORIGINAL_FOG_MAX_ROWS*2,values->environment_count*2) \
 X(272,I32,environment_i32,2,GE_ORIGINAL_FOG_MAX_ROWS*2,values->environment_count*2) \
 X(273,U8,environment_u8,5,GE_ORIGINAL_FOG_MAX_ROWS*5,values->environment_count*5) \
 X(274,I16,environment_i16,2,GE_ORIGINAL_FOG_MAX_ROWS*2,values->environment_count*2) \
 X(275,F32,environment_f32,15,GE_ORIGINAL_FOG_MAX_ROWS*15,values->environment_count*15) \
 X(276,U32,fogless_ids,1,GE_ORIGINAL_FOG_MAX_ROWS,values->fogless_count) \
 X(277,U8,fogless_u8,5,GE_ORIGINAL_FOG_MAX_ROWS*5,values->fogless_count*5) \
 X(278,I16,fogless_i16,2,GE_ORIGINAL_FOG_MAX_ROWS*2,values->fogless_count*2) \
 X(279,F32,fogless_f32,9,GE_ORIGINAL_FOG_MAX_ROWS*9,values->fogless_count*9) \
 X(280,F32,transition_near,0,3,values->bindings[3]==-2?3:0)

typedef struct GeMpStateFogOwner {
    GeMpRegistry *registry;
    uint64_t actor_id, lifetime;
} GeMpStateFogOwner;
typedef struct GeMpStateFogSnapshot {
    GeMpStateRecord record;
    GeMpStateField fields[GE_MP_FOG_FIELD_COUNT];
    GeOriginalMultiplayerFogState values;
    GeMpStateCoverage coverage;
    size_t record_count;
} GeMpStateFogSnapshot;

/* Bind after native fog load. In-stage environment loads/switches mutate the
 * same owner. End before fog close; closing/reloading native storage cannot
 * reuse a prior capture owner even when every native address is unchanged. */
GeMpStateStatus ge_mp_state_fog_begin(GeMpStateFogOwner *,GeMpRegistry *);
GeMpStateStatus ge_mp_state_fog_end(GeMpStateFogOwner *);
GeMpStateStatus ge_mp_state_fog_capture(const GeMpStateFogOwner *,GeMpStateFogSnapshot *);
const GeMpRecordSchema *ge_mp_state_fog_schema(void);
/* Snapshot owns every value; keep its address fixed until encoding finishes.
 * This is the fog subdomain only and never sets complete aggregate coverage. */
#endif
