#ifndef GE_ORIGINAL_CHARACTER_MODELS_H
#define GE_ORIGINAL_CHARACTER_MODELS_H

#include "ge_asset_pack.h"

#include <stddef.h>
#include <stdint.h>

typedef struct GeOriginalCharacterModelProvider GeOriginalCharacterModelProvider;

typedef enum GeOriginalCharacterModelStatus {
    GE_ORIGINAL_CHARACTER_MODEL_OK = 0,
    GE_ORIGINAL_CHARACTER_MODEL_INVALID_ARGUMENT,
    GE_ORIGINAL_CHARACTER_MODEL_INVALID_ID,
    GE_ORIGINAL_CHARACTER_MODEL_NOT_FOUND,
    GE_ORIGINAL_CHARACTER_MODEL_CAPACITY_EXHAUSTED,
    GE_ORIGINAL_CHARACTER_MODEL_ALLOCATION_FAILED,
    GE_ORIGINAL_CHARACTER_MODEL_INVALID_LAYOUT,
    GE_ORIGINAL_CHARACTER_MODEL_UNSUPPORTED_OPCODE
} GeOriginalCharacterModelStatus;

typedef struct GeOriginalCharacterModelMetadata {
    int32_t model_id;
    const char *name;
    int is_body_dependency;
    int is_head_dependency;
    int is_male;
    int has_integrated_head;
    float scale;
    float pov;
} GeOriginalCharacterModelMetadata;

typedef struct GeOriginalCharacterModelStats {
    size_t model_capacity;
    size_t loaded_models;
    size_t instance_capacity;
    size_t instantiated_models;
    size_t source_blob_bytes;
    size_t native_resource_bytes;
    size_t native_instance_bytes;
    uint16_t last_unsupported_opcode;
} GeOriginalCharacterModelStats;

typedef struct GeOriginalCharacterModelScenePart {
    const void *node;
    const uint8_t *blob;
    size_t blob_size;
    uint32_t primary_offset;
    uint32_t secondary_offset;
    uint32_t segment4_offset;
    int16_t model_type;
} GeOriginalCharacterModelScenePart;

/* Authored MODELNODE_OPCODE_SHADOW state after native relocation. Geometry is
 * kept in the model node's local coordinates and names the exact segment-3
 * matrix that the platform renderer must apply. */
typedef struct GeOriginalCharacterModelShadow {
    float position[2];
    float size[2];
    float scale;
    float height_above_ground;
    int32_t matrix_index;
    uint32_t image_id;
    uint8_t image_width;
    uint8_t image_height;
} GeOriginalCharacterModelShadow;

typedef struct GeOriginalCharacterModelPair {
    int32_t body_id;
    int32_t head_id;
    void *model_header;
    void *model_instance;
    float scale;
    float pov;
    size_t matrix_count;
} GeOriginalCharacterModelPair;

GeOriginalCharacterModelProvider *ge_original_character_model_provider_create(
    GeAssetPack *pack, size_t model_capacity, size_t instance_capacity,
    GeOriginalCharacterModelStatus *status);
void ge_original_character_model_provider_destroy(
    GeOriginalCharacterModelProvider *provider);

size_t ge_original_character_model_dependency_count(void);
int ge_original_character_model_dependency_metadata(
    size_t dependency_index, GeOriginalCharacterModelMetadata *metadata);
int ge_original_character_model_load(void *context, int32_t model_id);
int ge_original_character_model_available(void *context, int32_t model_id);
/* Visits the relocated texture tables of resources already loaded for this
 * stage, including currently hidden LOD/switch parts. Does not load models,
 * traverse or mutate instance relations, animate, or consume original RNG. */
int ge_original_character_models_visit_texture_ids(
    const GeOriginalCharacterModelProvider *provider, void *context,
    int (*visitor)(void *context, uint16_t image_id));
int ge_original_character_model_resolve_instance(
    void *context, int32_t model_id, void **model_header,
    void **model_instance, float *scale, float *pov);

/* Constructs the unchanged makeonebody body/head instance boundary after the
 * caller has resolved canonical random-head/sunglasses choices. Integrated-
 * head bodies require head_id == -1. External-head bodies require one of the
 * exact packaged head dependencies. */
int ge_original_character_model_resolve_pair(
    void *context, int32_t body_id, int32_t head_id, int sunglasses,
    GeOriginalCharacterModelPair *pair);
int ge_original_character_model_instance_set_root(
    void *model_instance, const float position[3], float angle);

/* Reapplies the original modelApplyRelations-style LOD/BSP/SWITCH/HEAD
 * topology for this instance immediately before an unchanged canonical model
 * traversal. Character resources are shared exactly as in the original
 * loader, so attached-head parents must be restored for the active model. */
int ge_original_character_model_prepare_instance_relations(
    GeOriginalCharacterModelProvider *provider, void *model_instance);

/* Enumerates the currently related model tree, including the exact attached
 * head selected in the instance RW data. Call after the canonical model
 * relation/matrix pass so LOD/BSP/switch visibility is authoritative. */
size_t ge_original_character_model_instance_scene_part_count(
    const GeOriginalCharacterModelProvider *provider,
    const void *model_instance);
/* Enumerates the currently related body/head display-list parts in one tree
 * traversal.  This is the bulk equivalent of calling instance_scene_part for
 * every index; it preserves authored traversal order and exact part values.
 * Null storage with zero capacity is a count query. */
int ge_original_character_model_instance_scene_parts(
    const GeOriginalCharacterModelProvider *provider,
    const void *model_instance,
    GeOriginalCharacterModelScenePart *parts, size_t part_capacity,
    size_t *part_count);
int ge_original_character_model_instance_scene_part(
    const GeOriginalCharacterModelProvider *provider,
    const void *model_instance, size_t part_index,
    GeOriginalCharacterModelScenePart *part);
size_t ge_original_character_model_instance_shadow_count(
    const GeOriginalCharacterModelProvider *provider,
    const void *model_instance);
int ge_original_character_model_instance_shadow(
    const GeOriginalCharacterModelProvider *provider,
    const void *model_instance, size_t shadow_index,
    GeOriginalCharacterModelShadow *shadow);

size_t ge_original_character_model_scene_part_count(
    const GeOriginalCharacterModelProvider *provider, int32_t model_id);
int ge_original_character_model_scene_part(
    const GeOriginalCharacterModelProvider *provider, int32_t model_id,
    size_t part_index, GeOriginalCharacterModelScenePart *part);
GeOriginalCharacterModelStatus ge_original_character_model_last_status(
    const GeOriginalCharacterModelProvider *provider);
void ge_original_character_model_get_stats(
    const GeOriginalCharacterModelProvider *provider,
    GeOriginalCharacterModelStats *stats);
const char *ge_original_character_model_status_name(
    GeOriginalCharacterModelStatus status);

#endif
