#ifndef GE_ORIGINAL_EFFECT_RUNTIME_H
#define GE_ORIGINAL_EFFECT_RUNTIME_H
#include <stdint.h>
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
