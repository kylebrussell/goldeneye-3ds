#ifndef GE_MP_STATE_EFFECTS_H
#define GE_MP_STATE_EFFECTS_H
#include "ge_mp_state.h"
#include "ge_original_effect_runtime.h"
#define GE_MP_EFFECTS_KIND 0xE8U
#define GE_MP_EFFECTS_COMPONENTS 11U
#define GE_MP_EFFECTS_RECORDS 657U
#define GE_MP_EFFECTS_FIELDS 11U
enum GeMpEffectComponent { GE_MP_EFFECT_GLOBAL,GE_MP_EFFECT_EXPLOSION,
 GE_MP_EFFECT_SMOKE,GE_MP_EFFECT_PARTICLE,GE_MP_EFFECT_IMPACT,GE_MP_EFFECT_SCORCH,
 GE_MP_EFFECT_SHARD,GE_MP_EFFECT_SPARK,GE_MP_EFFECT_MOVING_SPARK,
 GE_MP_EFFECT_PROJECTILE,GE_MP_EFFECT_CASING };
typedef enum GeMpEffectReferenceKind { GE_MP_EFFECT_REF_PROP,GE_MP_EFFECT_REF_OBJECT,
 GE_MP_EFFECT_REF_AUDIO,GE_MP_EFFECT_REF_IMAGE_TABLE,GE_MP_EFFECT_REF_MODEL_HEADER
} GeMpEffectReferenceKind;
typedef struct GeMpStateEffectsProviders {
 void *context;
 GeMpStateStatus (*reference)(void *,GeMpEffectReferenceKind,const void *,uint64_t *);
} GeMpStateEffectsProviders;
struct Projectile;
typedef struct GeMpStateEffectsOwner {
 GeMpRegistry *registry;
 GeOriginalEffectRuntimeStorage storage;
 const struct Projectile *projectiles;
 uint64_t projectile_lifetime,ids[GE_MP_EFFECTS_RECORDS];
 uint16_t counts[GE_MP_EFFECTS_COMPONENTS],offsets[GE_MP_EFFECTS_COMPONENTS];
 uint16_t count;
 uint8_t live;
} GeMpStateEffectsOwner;
/* Typed sequences are packed into separate typed arenas, never native struct
 * bytes. Component-specific sequence order is documented in NativeEffectsState.md. */
typedef struct GeMpStateEffectsSnapshot {
 GeMpStateRecord records[GE_MP_EFFECTS_RECORDS];
 GeMpStateField fields[GE_MP_EFFECTS_RECORDS][GE_MP_EFFECTS_FIELDS];
 uint8_t component[GE_MP_EFFECTS_RECORDS],active[GE_MP_EFFECTS_RECORDS];
 uint16_t slot[GE_MP_EFFECTS_RECORDS];
 int32_t i32[4096];int16_t i16[16384];int8_t i8[2048];
 uint32_t u32[4096];uint16_t u16[4096];uint8_t u8[16384];
 float f32[16384];uint64_t refs[2048];
 size_t used[8],record_count;
 GeMpStateCoverage coverage;
 uint32_t unresolved_kinds;
} GeMpStateEffectsSnapshot;
/* IDs identify physical buffer locations for one native stage lifetime,
 * including inactive slots. They are not effect-spawn generation claims. */
GeMpStateStatus ge_mp_state_effects_begin(GeMpStateEffectsOwner *,GeMpRegistry *);
GeMpStateStatus ge_mp_state_effects_end(GeMpStateEffectsOwner *);
GeMpStateStatus ge_mp_state_effects_export(const GeMpStateEffectsOwner *,
 const GeMpStateEffectsProviders *,GeMpStateEffectsSnapshot *);
GeMpStateStatus ge_mp_state_effects_actor(const GeMpStateEffectsOwner *,
 unsigned component,const void *native_pointer,uint64_t *actor);
const GeMpRecordSchema *ge_mp_state_effects_schema(void);
#endif
