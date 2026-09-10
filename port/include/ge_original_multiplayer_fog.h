#ifndef GE_ORIGINAL_MULTIPLAYER_FOG_H
#define GE_ORIGINAL_MULTIPLAYER_FOG_H

#include <stdint.h>

typedef struct GeOriginalMultiplayerFogSnapshot {
    int32_t level_id, selected_id, main_id, alternate_id;
    uint32_t player_count;
    float visibility_scale, zrange[2];
    float near_fog[3]; /* NearFog, MaxVisRange, MaxObfuscationRange */
    float distances[4]; /* scaled near/far clip, reciprocal/constant colour */
    float intensities[4]; /* far, difference, scaled far, scaled difference */
    int32_t difference_from_far_intensity, far_intensity;
    uint8_t red, green, blue, fog_enabled, has_near_fog, cinematic;
} GeOriginalMultiplayerFogSnapshot;

/* After native players exist and before world constructors/culling: count
 * must match getPlayerCount(). Explicit authored stage visibility scale is
 * required because the camera adapter has not produced its first frame yet.
 * Calls the original loader and native VI perspective/scale services. A load
 * on an existing stage preserves the original cinematic/alternate semantics.
 * Pure preview startup uses select_for_player_count() in stage_environment.h. */
int ge_original_multiplayer_fog_load(int32_t level_id, int cinematic,
    uint32_t player_count, float visibility_scale);
/* At stage teardown, before player/visibility services disappear. No VI or
 * camera callbacks, allocation, registry mutation or eye-time writes. */
void ge_original_multiplayer_fog_close(void);
int ge_original_multiplayer_fog_snapshot(GeOriginalMultiplayerFogSnapshot *);

/* Original fogGetPropDistColor(PropRecord*, rgba_f32*) and
 * chrobjFogVisRangeRelated(PropRecord*, f32) remain native ABI functions;
 * include game/bgfog.h and game/propobj.h at native call sites. */

/* Extracted-source/adapter boundary; not renderer entrypoints. */
float ge_original_multiplayer_fog_visibility_scale(void);
void ge_original_multiplayer_fog_reset_exact(void);
void ge_original_multiplayer_fog_snapshot_exact(GeOriginalMultiplayerFogSnapshot *);
#endif
