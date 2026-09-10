#ifndef GE_ORIGINAL_EFFECT_RUNTIME_H
#define GE_ORIGINAL_EFFECT_RUNTIME_H
#include <stdint.h>
#include <stddef.h>
struct Explosion;struct Smoke;struct Scorch;struct BulletImpact;struct FlyingParticles;
struct s_shattered_window_piece;struct s_bullet_spark;struct s_moving_bullet_spark;struct CasingRecord;
typedef struct GeOriginalEffectRuntimeStorage {
    const struct Explosion *explosions;
    const struct Smoke *smoke;
    const struct Scorch *scorches;
    const struct BulletImpact *impacts;
    const struct FlyingParticles *particles;
    const struct s_shattered_window_piece *shards;
    const struct s_bullet_spark *sparks;
    const struct s_moving_bullet_spark *moving_sparks;
    const struct CasingRecord *casings;
    uint32_t particle_count,shard_count,scorch_count,player_count;
    int32_t stage;
    uint64_t lifetime;
} GeOriginalEffectRuntimeStorage;
/* Read-only owner view. A reset at the same addresses advances lifetime. */
int ge_original_effect_runtime_storage(GeOriginalEffectRuntimeStorage *storage);
/* After all consumers/exporters close, before another stage reset. */
void ge_original_effect_runtime_end(void);
/* US native spark/casing reset, then effect and shard stage allocation.
 * Invalid roster leaves state untouched. Call before effect construction. */
int ge_original_effect_runtime_reset(uint32_t count,int32_t stage);
/* Once per committed world tick, after the original clocks/difficulty/sky
 * and before chrpropTick: sparks, casings, shards, flying particles. */
int ge_original_effect_runtime_pre_world(void);
/* Once per canonical viewport, after propsTickPlayer and before gun producer
 * or local renderer capture. Uses current native visibility alpha ordering. */
int ge_original_effect_runtime_retire_view(void);
/* Native opaque/post-BG/alpha prop traversal. Callback handles non-effect
 * native render semantics, returning zero to stop on an ownership failure. */
int ge_original_effect_runtime_render_view(void *context,
    int (*render_prop)(void *context,void *prop,int alpha));
/* Original per-object impact eligibility and bit retirement; no GPU work. */
void ge_original_effect_impact_semantic_exact(void *prop,int alpha);
#endif
