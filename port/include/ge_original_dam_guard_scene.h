#ifndef GE_ORIGINAL_DAM_GUARD_SCENE_H
#define GE_ORIGINAL_DAM_GUARD_SCENE_H

#include "ge_original_model_scene.h"

#include <stddef.h>
#include <stdint.h>

#define GE_ORIGINAL_DAM_GUARD_MODEL_BLOB_SIZE 23840U
#define GE_ORIGINAL_DAM_GUARD_MAX_SCENE_INPUTS 104U

typedef enum GeOriginalDamGuardSceneStatus {
    GE_ORIGINAL_DAM_GUARD_SCENE_OK = 0,
    GE_ORIGINAL_DAM_GUARD_SCENE_INVALID_ARGUMENT,
    GE_ORIGINAL_DAM_GUARD_SCENE_GUARDS_NOT_READY,
    GE_ORIGINAL_DAM_GUARD_SCENE_MODEL_LAYOUT_ERROR,
    GE_ORIGINAL_DAM_GUARD_SCENE_PIPELINE_ERROR,
    GE_ORIGINAL_DAM_GUARD_SCENE_CAPACITY_EXCEEDED
} GeOriginalDamGuardSceneStatus;

typedef struct GeOriginalDamGuardScene {
    GeOriginalDamGuardSceneStatus status;
    size_t guard_count;
    size_t input_count;
    size_t vertex_count;
    size_t batch_count;
    size_t triangle_count;
    size_t commands_visited;
    size_t required_vertex_count;
    size_t required_batch_count;
} GeOriginalDamGuardScene;

/* Renderer-owned persistent scratch for the canonical guard model inputs.
 * Input matrices and rooms are republished from the original Model/Prop state
 * every call. Decoded local vertices, segment-3 matrix associations, and
 * material batches are retained until the authored list topology changes;
 * steady-state live frames perform zero GBI traversal and only reapply the
 * current canonical matrices to retained geometry. */
typedef struct GeOriginalDamGuardSceneCache {
    GeOriginalModelSceneInput *inputs;
    GeOriginalModelScene *queries;
    GeDamRoomWorldVertex *template_vertices;
    GeDamRoomDrawBatch *template_batches;
    uint16_t *template_matrix_indices;
    float (*quantized_matrices)[4][4];
    size_t *input_vertex_offsets;
    size_t *input_batch_offsets;
    size_t capacity;
    size_t input_count;
    size_t required_vertex_count;
    size_t required_batch_count;
    size_t triangle_count;
    size_t commands_visited;
    size_t quantized_matrix_capacity;
    uint64_t topology_signature;
    uint64_t build_attempts;
    uint64_t single_pass_builds;
    uint64_t topology_rebuilds;
    int initialized;
    int topology_ready;
} GeOriginalDamGuardSceneCache;

/* Publishes the currently selected canonical DLCOLLISION nodes for all four
 * authored guards. Their model matrices are view-space, as in the original
 * render path; view_to_world removes that camera transform and leaves the
 * exact model/root/joint transform consumed by the world overlay. */
GeOriginalDamGuardSceneStatus ge_original_dam_guard_scene_inputs(
    const uint8_t *model_blob, size_t model_blob_size,
    const float view_to_world[4][4],
    GeOriginalModelSceneInput *inputs, size_t input_capacity,
    size_t *input_count);

/* Exact chrRenderHeldWeapon publication extends each live authored guard
 * with its canonically attached PchrkalashZ display list. */
GeOriginalDamGuardSceneStatus ge_original_dam_guard_scene_inputs_with_weapons(
    const uint8_t *model_blob, size_t model_blob_size,
    const uint8_t *weapon_blob, size_t weapon_blob_size,
    const float view_to_world[4][4],
    GeOriginalModelSceneInput *inputs, size_t input_capacity,
    size_t *input_count);

/* Aggregates all published guard inputs into the shared dynamic-overlay ABI.
 * Null storage is a capacity query and returns CAPACITY_EXCEEDED with exact
 * required counts, matching ge_original_model_scene_build. */
GeOriginalDamGuardSceneStatus ge_original_dam_guard_scene_build(
    const uint8_t *model_blob, size_t model_blob_size,
    const float view_to_world[4][4],
    const GeDamRoomSceneStorage *storage,
    GeOriginalDamGuardScene *scene);

GeOriginalDamGuardSceneStatus ge_original_dam_guard_scene_build_with_weapons(
    const uint8_t *model_blob, size_t model_blob_size,
    const uint8_t *weapon_blob, size_t weapon_blob_size,
    const float view_to_world[4][4],
    const GeDamRoomSceneStorage *storage,
    GeOriginalDamGuardScene *scene);

int ge_original_dam_guard_scene_cache_init(
    GeOriginalDamGuardSceneCache *cache);

void ge_original_dam_guard_scene_cache_close(
    GeOriginalDamGuardSceneCache *cache);

GeOriginalDamGuardSceneStatus ge_original_dam_guard_scene_build_cached(
    GeOriginalDamGuardSceneCache *cache,
    const uint8_t *model_blob, size_t model_blob_size,
    const float view_to_world[4][4],
    const GeDamRoomSceneStorage *storage,
    GeOriginalDamGuardScene *scene);

GeOriginalDamGuardSceneStatus
ge_original_dam_guard_scene_build_cached_with_weapons(
    GeOriginalDamGuardSceneCache *cache,
    const uint8_t *model_blob, size_t model_blob_size,
    const uint8_t *weapon_blob, size_t weapon_blob_size,
    const float view_to_world[4][4],
    const GeDamRoomSceneStorage *storage,
    GeOriginalDamGuardScene *scene);

const char *ge_original_dam_guard_scene_status_name(
    GeOriginalDamGuardSceneStatus status);

#endif
