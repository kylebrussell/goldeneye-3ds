#ifndef GE_ORIGINAL_MODEL_SCENE_H
#define GE_ORIGINAL_MODEL_SCENE_H

#include "ge_dam_room.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GE_ORIGINAL_MODEL_SCENE_NO_LIST UINT32_MAX

typedef enum GeOriginalModelSceneStatus {
    GE_ORIGINAL_MODEL_SCENE_OK = 0,
    GE_ORIGINAL_MODEL_SCENE_INVALID_ARGUMENT,
    GE_ORIGINAL_MODEL_SCENE_INVALID_LAYOUT,
    GE_ORIGINAL_MODEL_SCENE_PIPELINE_ERROR,
    GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED
} GeOriginalModelSceneStatus;

typedef struct GeOriginalModelSceneInput {
    const uint8_t *blob;
    size_t blob_size;
    uint32_t primary_offset;
    uint32_t secondary_offset;
    /* Some DLCOLLISION models bind their native vertex array as segment 4;
     * plain DL models keep segment-5 vertex addresses and use NO_LIST here. */
    uint32_t segment4_offset;
    uint32_t room_id;
    /* modelRender supplies this state immediately before invoking a world
     * model's primary/secondary display lists.  The display lists themselves
     * deliberately omit the inherited Z-buffer bit and, for some model
     * types, the matching compare/write policy. */
    uint8_t world_zbuffer_enabled;
    /* Native model render matrices are the canonical segment-3 matrix bank.
     * When supplied, each matrix is encoded as the N64 s15.16 ABI before GBI
     * traversal and processed model-view positions are transformed by matrix
     * (normally view-to-world) plus position.  Null retains the older single
     * identity segment-3 behavior and transforms raw model vertices. */
    const float (*segment3_matrices)[4][4];
    size_t segment3_matrix_count;
    float matrix[4][4];
    float position[3];
} GeOriginalModelSceneInput;

typedef struct GeOriginalModelScene {
    GeOriginalModelSceneStatus status;
    size_t list_count;
    size_t vertex_count;
    size_t batch_count;
    size_t triangle_count;
    size_t commands_visited;
    size_t required_vertex_count;
    size_t required_batch_count;
} GeOriginalModelScene;

typedef uint64_t (*GeOriginalModelSceneProfileClock)(void *context);
typedef int (*GeOriginalModelSceneTextureVisitor)(
    void *context, uint16_t texture_id);

/* Changed, contiguous output ranges from the last successful cache build.
 * Offsets are relative to its storage. A topology/buffer change also marks
 * immutable attributes dirty, so GPU consumers must remap UVs/materials. */
typedef struct GeOriginalModelScenePublicationRange {
    size_t vertex_offset;
    size_t vertex_count;
    size_t batch_offset;
    size_t batch_count;
    uint8_t static_data_changed;
} GeOriginalModelScenePublicationRange;

/* Persistent renderer cache for immutable model display-list topology.
 * Current segment-3 matrices, outer transforms, and room publications remain
 * caller-owned and are reapplied on every build.  A changed list/blob layout
 * invalidates the retained templates and falls back to one canonical decode. */
typedef struct GeOriginalModelSceneCache {
    GeOriginalModelScene *queries;
    GeDamRoomWorldVertex *template_vertices;
    GeDamRoomDrawBatch *template_batches;
    uint16_t *template_matrix_indices;
    /* For flattened triangle streams, points at the first earlier template
     * vertex with the same authored position and segment-3 matrix.  This is
     * renderer-only topology metadata: canonical matrices and output vertex
     * ordering are unchanged. */
    uint32_t *template_transform_sources;
    float (*quantized_matrices)[4][4];
    size_t *input_quantized_matrix_offsets;
    uint64_t *input_quantized_matrix_hashes;
    uint64_t *input_publication_signatures;
    uint64_t *published_input_publication_signatures;
    size_t *input_vertex_offsets;
    size_t *input_batch_offsets;
    size_t input_count;
    size_t required_vertex_count;
    size_t required_batch_count;
    size_t triangle_count;
    size_t commands_visited;
    size_t quantized_matrix_capacity;
    uint64_t topology_signature;
    uint64_t build_attempts;
    uint64_t cached_builds;
    uint64_t unchanged_builds;
    uint64_t identity_outer_vertices_published;
    /* Topology-stable publications into the same renderer-owned buffers can
     * retain immutable decoded source/material data. Only canonical matrix,
     * world-position, and room fields are rewritten on those frames. */
    uint64_t static_vertex_copies_avoided;
    uint64_t static_batch_copies_avoided;
    /* Matrix banks shared by related model parts are quantized once per
     * publication. This is adapter-only reuse; the canonical matrix values
     * and authored input ordering remain unchanged. */
    uint64_t matrix_elements_quantized;
    uint64_t shared_matrix_banks_reused;
    uint64_t duplicate_vertex_transforms_avoided;
    /* When one animated model changes, retain byte-identical publications
     * for the other model inputs already resident in the same output buffer.
     * This is renderer-owned reuse only; canonical matrices and input order
     * are still sampled on every build. */
    uint64_t unchanged_input_publications;
    uint64_t unchanged_input_vertices_avoided;
    uint64_t unchanged_input_batches_avoided;
    uint64_t topology_rebuilds;
    uint64_t topology_variant_hits;
    uint64_t topology_variant_evictions;
    uint64_t topology_component_hits;
    uint64_t topology_component_misses;
    /* Duplicate-transform maps are immutable per decoded model component.
     * Aggregate visibility changes reuse them rather than rebuilding the
     * same vertex hash during a combat-frame topology transition. */
    uint64_t topology_transform_maps_built;
    uint64_t topology_transform_map_vertices_reused;
    size_t topology_component_bytes;
    /* Private retained immutable topology variants.  The public fields stay
     * opaque so platform callers cannot depend on cache implementation. */
    void *topology_variants;
    size_t topology_variant_count;
    size_t topology_variant_next;
    void *topology_components;
    size_t topology_component_count;
    size_t topology_component_capacity;
    GeOriginalModelSceneProfileClock profile_clock;
    void *profile_clock_context;
    uint64_t profile_build_ticks;
    uint64_t profile_topology_ticks;
    uint64_t profile_publication_signature_ticks;
    uint64_t profile_matrix_quantization_ticks;
    uint64_t profile_vertex_transform_ticks;
    uint64_t profile_batch_publication_ticks;
    uint64_t profile_build_calls;
    uint64_t publication_signature;
    GeDamRoomWorldVertex *published_vertices;
    GeDamRoomDrawBatch *published_batches;
    GeOriginalModelScenePublicationRange *publication_ranges;
    size_t publication_range_count;
    size_t publication_range_capacity;
    uint8_t topology_ready;
    uint8_t publication_ready;
} GeOriginalModelSceneCache;

/*
 * Flattens the original segment-5 model display lists into the same authored
 * world-vertex/material-batch ABI used by Dam background rooms.  The supplied
 * matrix and position are the values already produced by the original object
 * constructor; this adapter does not invent placement or model state.
 *
 * Passing null/zero-capacity storage performs a capacity query.  Counts are
 * published only after every requested list has traversed successfully.
 */
GeOriginalModelSceneStatus ge_original_model_scene_build(
    const GeOriginalModelSceneInput *input,
    const GeDamRoomSceneStorage *storage,
    GeOriginalModelScene *scene);

/* Initial renderer-template decode. Each flattened output vertex receives the
 * canonical segment-3 matrix index that was active when its N64 vertex load
 * occurred. This lets platform renderers retain decoded local geometry and
 * reapply changing original model matrices without retraversing GBI. */
GeOriginalModelSceneStatus ge_original_model_scene_build_matrix_template(
    const GeOriginalModelSceneInput *input,
    const GeDamRoomSceneStorage *storage,
    uint16_t *matrix_indices, size_t matrix_index_capacity,
    GeOriginalModelScene *scene);

GeOriginalModelSceneStatus ge_original_model_scene_cache_build(
    GeOriginalModelSceneCache *cache,
    const GeOriginalModelSceneInput *inputs, size_t input_count,
    const GeDamRoomSceneStorage *storage, GeOriginalModelScene *scene);
void ge_original_model_scene_cache_close(GeOriginalModelSceneCache *cache);
void ge_original_model_scene_cache_bind_profile_clock(
    GeOriginalModelSceneCache *cache,
    GeOriginalModelSceneProfileClock clock, void *context);

/* Visits each distinct authored texture used by a valid published batch.
 * Dynamic model topology can introduce character heads, hats and weapons
 * after the initial room texture load; the platform renderer uses this exact
 * batch-derived list to make those images resident without changing actor
 * visibility or animation state. */
int ge_original_model_scene_visit_textures(
    const GeDamRoomDrawBatch *batches, size_t batch_count,
    void *context, GeOriginalModelSceneTextureVisitor visitor);

const char *ge_original_model_scene_status_name(
    GeOriginalModelSceneStatus status);

#ifdef __cplusplus
}
#endif

#endif
