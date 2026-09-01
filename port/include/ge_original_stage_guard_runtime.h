#ifndef GE_ORIGINAL_STAGE_GUARD_RUNTIME_H
#define GE_ORIGINAL_STAGE_GUARD_RUNTIME_H

#include "ge_original_character_models.h"
#include "ge_original_model_scene.h"
#include "ge_original_stage_prop_materializer.h"
#include "ge_original_pitem_models.h"
#include "ge_original_guard_muzzle_flash.h"

#include <stddef.h>
#include <stdint.h>

struct player;

typedef struct GeOriginalStageGuardRuntime GeOriginalStageGuardRuntime;

typedef enum GeOriginalStageGuardRuntimeStatus {
    GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK = 0,
    GE_ORIGINAL_STAGE_GUARD_RUNTIME_INVALID_ARGUMENT,
    GE_ORIGINAL_STAGE_GUARD_RUNTIME_CAPACITY_EXHAUSTED,
    GE_ORIGINAL_STAGE_GUARD_RUNTIME_PLACEMENT_UNAVAILABLE,
    GE_ORIGINAL_STAGE_GUARD_RUNTIME_HEAD_SELECTION_UNAVAILABLE,
    GE_ORIGINAL_STAGE_GUARD_RUNTIME_SUNGLASSES_SELECTION_UNAVAILABLE,
    GE_ORIGINAL_STAGE_GUARD_RUNTIME_MODEL_UNAVAILABLE,
    GE_ORIGINAL_STAGE_GUARD_RUNTIME_AI_LIST_UNAVAILABLE,
    GE_ORIGINAL_STAGE_GUARD_RUNTIME_ACTOR_UNAVAILABLE,
    GE_ORIGINAL_STAGE_GUARD_RUNTIME_WEAPON_UNAVAILABLE,
    GE_ORIGINAL_STAGE_GUARD_RUNTIME_WEAPON_ABI_UNAVAILABLE,
    GE_ORIGINAL_STAGE_GUARD_RUNTIME_HAT_UNAVAILABLE,
    GE_ORIGINAL_STAGE_GUARD_RUNTIME_HAT_ABI_UNAVAILABLE,
    GE_ORIGINAL_STAGE_GUARD_RUNTIME_LIGHTING_UNAVAILABLE,
    GE_ORIGINAL_STAGE_GUARD_RUNTIME_MATRIX_UNAVAILABLE,
    GE_ORIGINAL_STAGE_GUARD_RUNTIME_SCENE_ERROR,
    GE_ORIGINAL_STAGE_GUARD_RUNTIME_SCENE_CAPACITY_EXCEEDED
} GeOriginalStageGuardRuntimeStatus;

typedef struct GeOriginalStageGuardRuntimeServices {
    void *context;
    /* Production supplies chrpropAllocate-backed storage so unchanged room
     * registration can derive canonical g_Props indices. Tooling may omit
     * this callback and use the runtime's isolated fallback storage. */
    void *(*allocate_prop)(void *context);
    /* These callbacks are deliberately required only for the two canonical
     * random branches. Production binds bodyChooseHead/randomGetNext state;
     * test and tooling callers must make their choice explicit. */
    int (*choose_head)(void *context, int32_t body_id, int32_t *head_id);
    int (*choose_sunglasses)(void *context, uint16_t appearance_flags,
                             int *sunglasses);
    int (*room_resident)(void *context, uint8_t room_id);
    int (*tile_rgb)(void *context, void *stan, float x, float z,
                    uint8_t rgb[3]);
} GeOriginalStageGuardRuntimeServices;

typedef struct GeOriginalStageGuardSnapshot {
    size_t command_index;
    int32_t chr_id;
    int32_t body_id;
    int32_t authored_head_id;
    int32_t resolved_head_id;
    int32_t ai_list_id;
    int32_t pad_id;
    uint16_t preset;
    uint16_t chrpreset;
    uint16_t health;
    uint16_t reaction;
    uint16_t appearance_flags;
    uint8_t room_id;
    uint8_t sunglasses;
    uint8_t visible;
    uint8_t matrices_ready;
    uint8_t ai_list_resolved;
    uint8_t active_linked;
    uint8_t animation_active;
    uint8_t action_type;
    uint16_t ai_offset;
    uint8_t ai_opcode;
    int8_t sleep;
    uint32_t chr_flags;
    uint32_t prop_flags;
    uint16_t hidden;
    uint8_t alertness;
    uint8_t morale;
    uint8_t stand_prestand;
    uint8_t stand_reaim;
    int32_t firecount[2];
    int32_t last_seen_target_60;
    int32_t last_heard_target_60;
    float vision_range;
    float damage;
    float max_damage;
    float shotbondsum;
    float animation_frame;
    float prop_zdepth;
    float model_size;
    float position[3];
    float angle;
    float model_angle;
    void *model_instance;
    void *prop_record;
    void *chr_record;
} GeOriginalStageGuardSnapshot;

/* Reports the canonical ACT_DIE/ACT_DEAD or unlinked lifecycle boundary
 * without exposing original action constants to platform frontends. */
int ge_original_stage_guard_snapshot_death_complete(
    const GeOriginalStageGuardSnapshot *snapshot);

typedef struct GeOriginalStageGuardWeaponSnapshot {
    size_t command_index;
    int32_t owner_chr_id;
    int32_t model_id;
    int32_t weapon_id;
    uint8_t hand;
    uint8_t matrices_ready;
    void *model_instance;
    void *prop_record;
    void *weapon_record;
} GeOriginalStageGuardWeaponSnapshot;

typedef struct GeOriginalStageGuardWeaponBindReport {
    size_t authored_assigned_collectables;
    size_t owner_not_present;
    size_t attached;
    size_t failed_command_index;
    int32_t failed_model_id;
    int32_t failed_owner_chr_id;
    uint8_t failed_branch;
} GeOriginalStageGuardWeaponBindReport;

typedef struct GeOriginalStageGuardHatSnapshot {
    size_t command_index;
    int32_t owner_chr_id;
    int32_t model_id;
    uint8_t matrices_ready;
    void *model_instance;
    void *prop_record;
    void *hat_record;
} GeOriginalStageGuardHatSnapshot;

typedef struct GeOriginalStageGuardHatBindReport {
    size_t authored_assigned_hats;
    size_t owner_not_present;
    size_t attached;
    size_t failed_command_index;
    int32_t failed_model_id;
    int32_t failed_owner_chr_id;
    uint8_t failed_branch;
} GeOriginalStageGuardHatBindReport;

typedef struct GeOriginalStageGuardLightingSnapshot {
    uint8_t current_rgba[4];
    uint8_t target_rgba[4];
} GeOriginalStageGuardLightingSnapshot;

typedef struct GeOriginalStageGuardShadowPublication {
    float vertices[4][3];
    int32_t matrix_index;
    uint32_t image_id;
    uint8_t image_width;
    uint8_t image_height;
    uint8_t opacity;
} GeOriginalStageGuardShadowPublication;

typedef struct GeOriginalStageGuardScene {
    GeOriginalStageGuardRuntimeStatus status;
    size_t guard_count;
    size_t resident_guard_count;
    size_t published_guard_count;
    size_t culled_guard_count;
    size_t input_count;
    size_t vertex_count;
    size_t batch_count;
    size_t triangle_count;
    size_t commands_visited;
    size_t required_vertex_count;
    size_t required_batch_count;
} GeOriginalStageGuardScene;

/* Conservative renderer-only form of the original character visibility
 * sphere test. Invalid camera/bounds inputs are deliberately accepted so a
 * missing publication parameter can never hide an authored actor. */
int ge_original_stage_guard_draw_sphere_visible(
    const float world_to_view[4][4], float vertical_fov_degrees,
    float aspect, float near_distance, const float center[3], float radius);

typedef struct GeOriginalStageGuardSceneScratchStats {
    size_t input_capacity;
    size_t character_part_capacity;
    uint64_t collect_calls;
    uint64_t allocation_events;
    uint64_t allocation_free_collect_calls;
} GeOriginalStageGuardSceneScratchStats;

GeOriginalStageGuardRuntime *ge_original_stage_guard_runtime_create(
    GeOriginalCharacterModelProvider *models, size_t guard_capacity,
    const GeOriginalStageGuardRuntimeServices *services,
    GeOriginalStageGuardRuntimeStatus *status);
void ge_original_stage_guard_runtime_destroy(
    GeOriginalStageGuardRuntime *runtime);
int ge_original_stage_guard_runtime_load_player_body(
    void *context, struct player *player, int32_t body_id,
    int32_t head_id, float yaw);
int ge_original_stage_guard_runtime_attach_player_held_item(
    void *context, struct player *player, int32_t prop_id,
    int32_t item_id, uint32_t flags);

/* Installs this runtime as the generic materializer's exact GUARD service. */
int ge_original_stage_guard_runtime_materializer(
    GeOriginalStageGuardRuntime *runtime,
    GeOriginalStagePropMaterializerProviders *providers);
int ge_original_stage_guard_runtime_construct(
    void *context, const GeOriginalStagePropConstructionRequest *request);

size_t ge_original_stage_guard_runtime_count(
    const GeOriginalStageGuardRuntime *runtime);
int ge_original_stage_guard_runtime_snapshot(
    const GeOriginalStageGuardRuntime *runtime, size_t guard_index,
    GeOriginalStageGuardSnapshot *snapshot);
/* Diagnostic-only view publication for controller probes. The point retains
 * the exact 0.25 interpolation used by chrGetOnscreenRenderBounds, anchored
 * to the current canonical character prop so an older view-matrix publication
 * cannot pull aim away from a moving actor. Offscreen actors use the same
 * canonical character height/fraction until renderer matrices are available.
 * It never mutates actor or gameplay state. */
int ge_original_stage_guard_runtime_autoaim_world_position(
    const GeOriginalStageGuardRuntime *runtime, size_t guard_index,
    const float view_to_world[4][4], float world_position[3]);
int ge_original_stage_guard_runtime_actor(
    GeOriginalStageGuardRuntime *runtime, size_t guard_index,
    void **prop_record, void **chr_record);
/* O(1) combat counters for platform-side frame auditing. Unlike the full
 * diagnostic snapshot this does not scan g_ActiveProps for linkage state. */
int ge_original_stage_guard_runtime_firecount(
    const GeOriginalStageGuardRuntime *runtime, size_t guard_index,
    int32_t firecount[2]);
void *ge_original_stage_guard_runtime_stan(
    const GeOriginalStageGuardRuntime *runtime, size_t guard_index);
int ge_original_stage_guard_runtime_set_visibility(
    GeOriginalStageGuardRuntime *runtime, size_t guard_index,
    int visible, uint8_t room_id);

/* Runs setup's unchanged weaponAssignToHome construction/equip semantics for
 * authored ASSIGNEDTOCHR collectables whose owner is resident in this guard
 * runtime. Missing owners are canonical no-ops (difficulty/setup variants).
 * The provider and exact projectile dependency callback must outlive runtime. */
GeOriginalStageGuardRuntimeStatus
ge_original_stage_guard_runtime_bind_authored_weapons(
    GeOriginalStageGuardRuntime *runtime,
    const GeOriginalStageSetupRuntime *setup,
    GeOriginalPitemModelProvider *models,
    int (*load_projectile_models)(void *context, int32_t weapon_id),
    void *projectile_context,
    GeOriginalStageGuardWeaponBindReport *report);
size_t ge_original_stage_guard_runtime_weapon_count(
    const GeOriginalStageGuardRuntime *runtime);
int ge_original_stage_guard_runtime_weapon_snapshot(
    const GeOriginalStageGuardRuntime *runtime, size_t weapon_index,
    GeOriginalStageGuardWeaponSnapshot *snapshot);
size_t ge_original_stage_guard_runtime_muzzle_flash_count(
    const GeOriginalStageGuardRuntime *runtime);
/* Publishes the unchanged dogfnegx quad for one currently visible authored
 * guard weapon. Returns zero while its canonical Gunfire RW relation is off. */
int ge_original_stage_guard_runtime_muzzle_flash(
    const GeOriginalStageGuardRuntime *runtime, size_t weapon_index,
    GeOriginalGuardMuzzleFlashPublication *publication);
GeOriginalStageGuardRuntimeStatus
ge_original_stage_guard_runtime_bind_authored_hats(
    GeOriginalStageGuardRuntime *runtime,
    const GeOriginalStageSetupRuntime *setup,
    GeOriginalPitemModelProvider *models,
    GeOriginalStageGuardHatBindReport *report);
size_t ge_original_stage_guard_runtime_hat_count(
    const GeOriginalStageGuardRuntime *runtime);
int ge_original_stage_guard_runtime_hat_snapshot(
    const GeOriginalStageGuardRuntime *runtime, size_t hat_index,
    GeOriginalStageGuardHatSnapshot *snapshot);

/* Enumerates every guard-owned authored prop (including child attachments)
 * for command-index lookup.  Parented weapons/hats must not be inserted in
 * the top-level active list because chrpropReparent reuses prev/next for the
 * child-sibling chain. */
size_t ge_original_stage_guard_runtime_active_prop_count(
    const GeOriginalStageGuardRuntime *runtime);
int ge_original_stage_guard_runtime_active_prop(
    GeOriginalStageGuardRuntime *runtime, size_t active_index,
    size_t *command_index, void **prop_record);

/* Enumerates only root guard props in authored order for g_ActiveProps. */
size_t ge_original_stage_guard_runtime_root_prop_count(
    const GeOriginalStageGuardRuntime *runtime);
int ge_original_stage_guard_runtime_root_prop(
    GeOriginalStageGuardRuntime *runtime, size_t root_index,
    size_t *command_index, void **prop_record);

/* Publishes the exact tile-derived shade interpolation and authored SHADOW
 * node quad required by the platform renderer. Shadow vertices remain local
 * to matrix_index, matching doshadow's segment-3 matrix semantics. */
GeOriginalStageGuardRuntimeStatus
ge_original_stage_guard_runtime_update_lighting(
    GeOriginalStageGuardRuntime *runtime);
int ge_original_stage_guard_runtime_lighting_snapshot(
    const GeOriginalStageGuardRuntime *runtime, size_t guard_index,
    GeOriginalStageGuardLightingSnapshot *snapshot);
size_t ge_original_stage_guard_runtime_shadow_count(
    const GeOriginalStageGuardRuntime *runtime, size_t guard_index);
int ge_original_stage_guard_runtime_shadow(
    const GeOriginalStageGuardRuntime *runtime, size_t guard_index,
    size_t shadow_index, GeOriginalStageGuardShadowPublication *shadow);

/* Retains the unchanged chrTick/chrRenderHeldWeapon transient matrices in the
 * native instances' durable renderer storage. The initialization/offscreen
 * fallback runs unchanged subcalcmatrices from authored position, facing,
 * scale, attached-head RW data and the caller's camera. This is model
 * publication, not a replacement AI or animation tick. */
GeOriginalStageGuardRuntimeStatus
ge_original_stage_guard_runtime_update_matrices(
    GeOriginalStageGuardRuntime *runtime,
    const float world_to_view[4][4]);

/* Flattens only currently visible, resident guards into runtime-coordinate
 * batches. Passing null storage is an exact capacity query. */
GeOriginalStageGuardRuntimeStatus ge_original_stage_guard_runtime_build_scene(
    GeOriginalStageGuardRuntime *runtime,
    const float view_to_world[4][4],
    const GeDamRoomSceneStorage *storage,
    GeOriginalStageGuardScene *scene);
GeOriginalStageGuardRuntimeStatus
ge_original_stage_guard_runtime_build_scene_cached(
    GeOriginalStageGuardRuntime *runtime, GeOriginalModelSceneCache *cache,
    const float view_to_world[4][4],
    const GeDamRoomSceneStorage *storage,
    GeOriginalStageGuardScene *scene);
void ge_original_stage_guard_runtime_scene_scratch_stats(
    const GeOriginalStageGuardRuntime *runtime,
    GeOriginalStageGuardSceneScratchStats *stats);

/* Transactionally appends one built guard scene after an existing overlay
 * prefix and rebases guard-local batches. Cursor values are unchanged on any
 * failure, allowing the caller to abandon the combined set_overlay swap. */
GeOriginalStageGuardRuntimeStatus ge_original_stage_guard_runtime_append_scene(
    GeOriginalStageGuardRuntime *runtime,
    const float view_to_world[4][4],
    const GeDamRoomSceneStorage *storage,
    size_t *vertex_cursor, size_t *batch_cursor,
    GeOriginalStageGuardScene *scene);

GeOriginalStageGuardRuntimeStatus ge_original_stage_guard_runtime_last_status(
    const GeOriginalStageGuardRuntime *runtime);
const char *ge_original_stage_guard_runtime_status_name(
    GeOriginalStageGuardRuntimeStatus status);

#endif
