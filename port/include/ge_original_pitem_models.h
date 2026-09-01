#ifndef GE_ORIGINAL_PITEM_MODELS_H
#define GE_ORIGINAL_PITEM_MODELS_H

#include "ge_asset_pack.h"

#include <stddef.h>
#include <stdint.h>

typedef struct GeOriginalPitemModelProvider GeOriginalPitemModelProvider;

typedef enum GeOriginalPitemModelStatus {
    GE_ORIGINAL_PITEM_MODEL_OK = 0,
    GE_ORIGINAL_PITEM_MODEL_INVALID_ARGUMENT,
    GE_ORIGINAL_PITEM_MODEL_INVALID_ID,
    GE_ORIGINAL_PITEM_MODEL_NOT_FOUND,
    GE_ORIGINAL_PITEM_MODEL_CAPACITY_EXHAUSTED,
    GE_ORIGINAL_PITEM_MODEL_ALLOCATION_FAILED,
    GE_ORIGINAL_PITEM_MODEL_INVALID_LAYOUT,
    GE_ORIGINAL_PITEM_MODEL_UNSUPPORTED_OPCODE
} GeOriginalPitemModelStatus;

typedef struct GeOriginalPitemModelStats {
    size_t model_capacity;
    size_t loaded_models;
    size_t instance_capacity;
    size_t instantiated_models;
    size_t fixed_capacity_bytes;
    size_t source_blob_bytes;
    size_t native_resource_bytes;
    size_t native_instance_bytes;
    uint16_t last_unsupported_opcode;
} GeOriginalPitemModelStats;

typedef struct GeOriginalPitemModelScenePart {
    /* Exact relocated DL/DLCOLLISION ModelNode identity. This lets dynamic
     * canonical publications (monitor Switches, articulated parts) select the
     * flattened scene part without deriving identity from display-list data. */
    const void *node;
    const uint8_t *blob;
    size_t blob_size;
    uint32_t primary_offset;
    uint32_t secondary_offset;
    uint32_t segment4_offset;
    uint16_t vertex_count;
    /* Model matrix selected by the nearest canonical GROUP ancestor. */
    uint16_t matrix_index;
    int16_t model_type;
} GeOriginalPitemModelScenePart;

/* Exact relocated MODELNODE_OPCODE_GUNFIRE relation.  The node is not a
 * display list: dogfnegx consumes this metadata and the instance RW visible
 * bit to generate a camera-facing quad at render time. */
typedef struct GeOriginalPitemModelGunfire {
    float offset[3];
    float size[3];
    float scale;
    uint16_t image_id;
    uint16_t matrix_index;
    uint8_t image_width;
    uint8_t image_height;
    uint8_t image_format;
    uint8_t image_depth;
    uint8_t flags_s;
    uint8_t flags_t;
    uint8_t visible;
} GeOriginalPitemModelGunfire;

/* ROM-local image named by a model's relocated ModelFileTextures table.  A
 * segmented 0x05 address denotes pixels embedded in the PitemZ blob; ordinary
 * Rare image IDs are intentionally rejected by this query and continue
 * through the shared texture catalog.  The returned pixels remain owned by
 * the provider. */
typedef struct GeOriginalPitemEmbeddedTexture {
    const uint8_t *pixels;
    size_t available_bytes;
    uint32_t segmented_address;
    uint16_t width;
    uint16_t height;
    uint8_t render_depth;
    uint8_t flags_s;
    uint8_t flags_t;
} GeOriginalPitemEmbeddedTexture;

/* Creates a provider over the exact canonical PitemZ_entries table linked from
 * the decomp. Models remain lazy: their byte-identical PitemZ payload is read
 * and relocated only when model_load/model_available first requests its ID. */
GeOriginalPitemModelProvider *ge_original_pitem_model_provider_create(
    GeAssetPack *pack, size_t model_capacity, size_t instance_capacity,
    GeOriginalPitemModelStatus *status);

void ge_original_pitem_model_provider_destroy(
    GeOriginalPitemModelProvider *provider);

/* Direct callback seams for GeOriginalDefaultObjectProviders and the generic
 * stage prop materializer. resolve_instance returns a fresh native Model for
 * every successful call, matching modelmgrInstantiateModel ownership. */
int32_t ge_original_pitem_model_load(void *context, int32_t model_id);
int ge_original_pitem_model_available(void *context, int32_t model_id);
int ge_original_pitem_model_resolve_instance(
    void *context, int32_t model_id, void **model_header,
    void **model_instance, float *pitem_scale);
/* Releases one instance returned by resolve_instance. Relocated model
 * resources remain cached; the exact instance slot is reusable on the next
 * construction request. */
int ge_original_pitem_model_release_instance(
    GeOriginalPitemModelProvider *provider, void *model_instance);

/* Enumerates exact DLCOLLISION display-list roots in canonical node order for
 * the portable model-scene renderer. A zero secondary_offset is reported as
 * UINT32_MAX, matching GeOriginalModelSceneInput's no-list sentinel. */
size_t ge_original_pitem_model_scene_part_count(
    const GeOriginalPitemModelProvider *provider, int32_t model_id);
int ge_original_pitem_model_scene_part(
    const GeOriginalPitemModelProvider *provider, int32_t model_id,
    size_t part_index, GeOriginalPitemModelScenePart *part);
/* Resolves an exact relocated ModelNode pointer back to its canonical scene
 * part. Non-DL nodes and nodes owned by a different resource are rejected. */
int ge_original_pitem_model_scene_part_for_node(
    const GeOriginalPitemModelProvider *provider, int32_t model_id,
    const void *node, size_t *part_index,
    GeOriginalPitemModelScenePart *part);

/* Applies the unchanged modelApplyToggleRelations/LOD/BSP relation semantics
 * to one live native Model and enumerates only display-list nodes reachable
 * from its resulting tree. This is required by PwalletbondZ and every other
 * Pitem whose authored Switches select instance-specific visible geometry. */
size_t ge_original_pitem_model_instance_scene_part_count(
    const GeOriginalPitemModelProvider *provider,
    const void *model_instance);
int ge_original_pitem_model_instance_scene_part(
    const GeOriginalPitemModelProvider *provider,
    const void *model_instance, size_t part_index,
    GeOriginalPitemModelScenePart *part);

size_t ge_original_pitem_model_instance_gunfire_count(
    const GeOriginalPitemModelProvider *provider,
    const void *model_instance);
int ge_original_pitem_model_instance_gunfire(
    const GeOriginalPitemModelProvider *provider,
    const void *model_instance, size_t gunfire_index,
    GeOriginalPitemModelGunfire *gunfire);

/* Resolves the exact embedded image backing a SETTIMG command. */
int ge_original_pitem_model_embedded_texture(
    const GeOriginalPitemModelProvider *provider, int32_t model_id,
    uint32_t segmented_address,
    GeOriginalPitemEmbeddedTexture *texture);

/* Exact disable_all_switches/set_item_visibility_in_objinstance adapter from
 * front.c. The switch index addresses ModelFileHeader::Switches, not a node
 * traversal index. */
int ge_original_pitem_model_instance_disable_switches(
    GeOriginalPitemModelProvider *provider, void *model_instance);
int ge_original_pitem_model_instance_set_switch(
    GeOriginalPitemModelProvider *provider, void *model_instance,
    size_t switch_index, int visible);
const void *ge_original_pitem_model_instance_switch_node(
    const GeOriginalPitemModelProvider *provider,
    const void *model_instance, size_t switch_index);

/* Exact object-hit traversal is only safe for a live Model whose ROM display
 * lists and native vertex arrays were both validated by this provider. */
int ge_original_pitem_model_hit_ready(
    const GeOriginalPitemModelProvider *provider,
    const void *model_instance);

/* Native ABI service used by the unchanged bgTestHitOnObj body after its N64
 * segmented-pointer operations have been translated.  A zero result means
 * the base is not owned by a live, validated Pitem resource. */
size_t ge_original_native_model_hit_blob_size(const void *base_address);

GeOriginalPitemModelStatus ge_original_pitem_model_last_status(
    const GeOriginalPitemModelProvider *provider);
void ge_original_pitem_model_get_stats(
    const GeOriginalPitemModelProvider *provider,
    GeOriginalPitemModelStats *stats);
const char *ge_original_pitem_model_status_name(
    GeOriginalPitemModelStatus status);

#endif
