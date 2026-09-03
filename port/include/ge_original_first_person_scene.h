#ifndef GE_ORIGINAL_FIRST_PERSON_SCENE_H
#define GE_ORIGINAL_FIRST_PERSON_SCENE_H

#include "ge_original_first_person_assets.h"
#include "ge_original_model_scene.h"
#include "ge_original_gun_live.h"

#include <stddef.h>
#include <stdint.h>

/* Exact maxima measured by the canonical model renderer over every authored
 * first-person resource currently supported by the generic asset cache. */
#define GE_ORIGINAL_FIRST_PERSON_SUPPORTED_DISPLAY_LIST_CAPACITY 28U
#define GE_ORIGINAL_FIRST_PERSON_SUPPORTED_VERTEX_CAPACITY 4119U
#define GE_ORIGINAL_FIRST_PERSON_SUPPORTED_BATCH_CAPACITY 419U
#define GE_ORIGINAL_FIRST_PERSON_SUPPORTED_TEXTURE_CAPACITY 22U
#define GE_ORIGINAL_FIRST_PERSON_MAX_DISPLAY_LISTS \
    GE_ORIGINAL_FIRST_PERSON_SUPPORTED_DISPLAY_LIST_CAPACITY

typedef enum GeOriginalFirstPersonSceneStatus {
    GE_ORIGINAL_FIRST_PERSON_SCENE_OK = 0,
    GE_ORIGINAL_FIRST_PERSON_SCENE_INVALID_ARGUMENT,
    GE_ORIGINAL_FIRST_PERSON_SCENE_HAND_NOT_PUBLISHED,
    GE_ORIGINAL_FIRST_PERSON_SCENE_MODEL_LAYOUT_ERROR,
    GE_ORIGINAL_FIRST_PERSON_SCENE_PIPELINE_ERROR,
    GE_ORIGINAL_FIRST_PERSON_SCENE_CAPACITY_EXCEEDED
} GeOriginalFirstPersonSceneStatus;

typedef struct GeOriginalFirstPersonScene {
    GeOriginalFirstPersonSceneStatus status;
    uint64_t generation;
    size_t display_list_count;
    size_t vertex_count;
    size_t batch_count;
    size_t triangle_count;
    size_t commands_visited;
    size_t required_vertex_count;
    size_t required_batch_count;
} GeOriginalFirstPersonScene;

typedef struct GeOriginalFirstPersonSceneRequirements {
    size_t display_list_capacity;
    size_t vertex_capacity;
    size_t batch_capacity;
    size_t texture_capacity;
} GeOriginalFirstPersonSceneRequirements;

typedef uint64_t (*GeOriginalFirstPersonSceneProfileClock)(void *context);

typedef struct GeOriginalFirstPersonTopology GeOriginalFirstPersonTopology;

typedef struct GeOriginalFirstPersonSceneCache {
    GeOriginalModelSceneInput *inputs;
    GeOriginalModelScene *queries;
    size_t *input_vertex_offsets;
    size_t *input_batch_offsets;
    GeDamRoomWorldVertex *template_vertices;
    GeDamRoomDrawBatch *template_batches;
    uint16_t *template_matrix_indices;
    uint32_t *template_transform_sources;
    float (*quantized_matrices)[4][4];
    uint8_t *quantized_matrix_changed;
    size_t *input_quantized_matrix_offsets;
    uint64_t *input_quantized_matrix_hashes;
    size_t capacity;
    size_t input_count;
    size_t required_vertex_count;
    size_t required_batch_count;
    size_t triangle_count;
    size_t commands_visited;
    size_t quantized_matrix_capacity;
    uint64_t topology_signature;
    uint64_t build_attempts;
    uint64_t topology_rebuilds;
    uint64_t topology_reuses;
    uint64_t topology_publications;
    /* One inactive decoded layout, not another gameplay/model instance. */
    GeOriginalFirstPersonTopology *previous_topology;
    uint64_t single_pass_builds;
    uint64_t unchanged_builds;
    uint64_t matrix_bank_quantizations;
    uint64_t matrix_elements_quantized;
    uint64_t matrix_elements_requantized_avoided;
    uint64_t shared_matrix_banks_reused;
    uint64_t static_vertex_copies_avoided;
    uint64_t static_batch_copies_avoided;
    uint64_t eye_space_vertices_published;
    uint64_t duplicate_vertex_transforms_avoided;
    uint64_t cross_input_duplicate_transforms_avoided;
    uint64_t unchanged_matrix_vertices_reused;
    GeOriginalFirstPersonSceneProfileClock profile_clock;
    void *profile_clock_context;
    uint64_t profile_build_ticks;
    uint64_t profile_input_topology_ticks;
    uint64_t profile_matrix_signature_ticks;
    uint64_t profile_vertex_transform_ticks;
    uint64_t profile_batch_publication_ticks;
    uint64_t profile_build_calls;
    uint64_t publication_signature;
    GeDamRoomWorldVertex *published_vertices;
    GeDamRoomDrawBatch *published_batches;
    uint8_t initialized;
    uint8_t topology_ready;
    uint8_t publication_ready;
    uint8_t publication_eye_space;
} GeOriginalFirstPersonSceneCache;

/* Flattens the active display-list nodes selected by the unchanged gun body.
 * The segment-3 bank is exactly hand->mtxlist; view_to_world merely converts
 * its camera-space output for the shared original-camera projection stage. */
GeOriginalFirstPersonSceneStatus ge_original_first_person_scene_build(
    const GeOriginalFirstPersonAssets *assets, unsigned hand,
    const float view_to_world[4][4],
    const GeDamRoomSceneStorage *storage,
    GeOriginalFirstPersonScene *scene);

static inline GeOriginalFirstPersonSceneRequirements
ge_original_first_person_scene_supported_requirements(void)
{
    GeOriginalFirstPersonSceneRequirements requirements;
    requirements.display_list_capacity =
        GE_ORIGINAL_FIRST_PERSON_SUPPORTED_DISPLAY_LIST_CAPACITY;
    requirements.vertex_capacity =
        GE_ORIGINAL_FIRST_PERSON_SUPPORTED_VERTEX_CAPACITY;
    requirements.batch_capacity =
        GE_ORIGINAL_FIRST_PERSON_SUPPORTED_BATCH_CAPACITY;
    requirements.texture_capacity =
        GE_ORIGINAL_FIRST_PERSON_SUPPORTED_TEXTURE_CAPACITY;
    return requirements;
}

/* Retains decoded display-list topology and reapplies only the unchanged
 * gun body's current hand matrix bank. This is a renderer cache: model
 * selection, visibility, matrices, and generation remain decomp-owned.
 * Passing an identity view_to_world explicitly publishes the canonical hand
 * matrix bank's eye-space coordinates in `world`; projection-only platform
 * backends can thereby avoid a view_to_world/world_to_view round trip. */
int ge_original_first_person_scene_cache_init(
    GeOriginalFirstPersonSceneCache *cache);
void ge_original_first_person_scene_cache_close(
    GeOriginalFirstPersonSceneCache *cache);
void ge_original_first_person_scene_cache_bind_profile_clock(
    GeOriginalFirstPersonSceneCache *cache,
    GeOriginalFirstPersonSceneProfileClock clock, void *context);
GeOriginalFirstPersonSceneStatus ge_original_first_person_scene_build_cached(
    GeOriginalFirstPersonSceneCache *cache,
    const GeOriginalFirstPersonAssets *assets, unsigned hand,
    const float view_to_world[4][4],
    const GeDamRoomSceneStorage *storage,
    GeOriginalFirstPersonScene *scene);

const char *ge_original_first_person_scene_status_name(
    GeOriginalFirstPersonSceneStatus status);

#endif
