#ifndef GE_ORIGINAL_STAGE_MODEL_PUBLICATION_H
#define GE_ORIGINAL_STAGE_MODEL_PUBLICATION_H

#include "ge_original_model_scene.h"
#include "ge_original_pitem_models.h"
#include "ge_original_door_runtime.h"

#include <stddef.h>
#include <stdint.h>

typedef enum GeOriginalStageModelPublicationStatus {
    GE_ORIGINAL_STAGE_MODEL_PUBLICATION_OK = 0,
    GE_ORIGINAL_STAGE_MODEL_PUBLICATION_INVALID_ARGUMENT,
    GE_ORIGINAL_STAGE_MODEL_PUBLICATION_NOT_VISIBLE,
    GE_ORIGINAL_STAGE_MODEL_PUBLICATION_INVALID_MODEL,
    GE_ORIGINAL_STAGE_MODEL_PUBLICATION_INVALID_PART,
} GeOriginalStageModelPublicationStatus;

/* Restore caller depth state for all world props and additional material
 * state omitted by ROM child lists for authored type-4 glass. Does not mutate the object or calculate gameplay opacity. */
void ge_original_stage_model_publication_glass_material(
    const void *definition, int16_t model_type, GeOriginalModelSceneInput *input);
int ge_original_stage_model_publication_glass_opacity(
    const void *definition, uint8_t *opacity);

/* Immutable setup for retained geometry. The live material publisher must
 * apply the original opacity before drawing; it is not a topology key. */
void ge_original_stage_model_publication_glass_template(
    const void *definition, int16_t model_type, GeOriginalModelSceneInput *input);
int ge_original_stage_model_publication_glass_alpha(
    const void *definition, GePicaMaterial *material);

/* Static type-4 glass: retain its geometry but restore bgLevelRender's
 * lights/LookAt and objTick's object-to-eye normal matrix for vertex shading.
 * Door/articulated matrices require their separate live publication path. */
int ge_original_stage_model_publication_glass_shading(
    const void *definition, const float view[4][4],
    const uint8_t look_at[32], const GePicaMaterial *material,
    GeGbiRenderState *state);

/* Windowed doors use the actual segment-3 index captured at the vertex load,
 * including articulated children; no matrix-zero fallback is permitted. */
int ge_original_stage_model_publication_door_glass_shading(
    const void *definition, const GeOriginalDoorRuntimePublication *publication,
    size_t matrix_index, const float view[4][4], const uint8_t look_at[32],
    const GePicaMaterial *material, GeGbiRenderState *state);

/* Bind only the exact DLCOLLISION node modified by original doorTick.
 * Native vertices remain runtime-owned and are borrowed for one scene build. */
int ge_original_stage_model_publication_door_vertices(
    const void *definition, const GeOriginalPitemModelScenePart *part,
    const GeOriginalDoorRuntimePublication *publication,
    GeOriginalModelSceneInput *input);

/* Binds one currently visible native object's exact Model.render_pos matrix
 * bank to the model-scene input. The bank remains eye-space, exactly as
 * objTick/modelUpdateMatrices published it; live view-to-world is the outer
 * transform that returns flattened vertices to world space. Quantization is
 * deliberately left to ge_original_model_scene_build's N64 s15.16 path. */
GeOriginalStageModelPublicationStatus
ge_original_stage_model_publication_input(
    const GeOriginalPitemModelProvider *models, const void *definition,
    size_t visible_part_index, uint8_t room,
    const float view_to_world[4][4], GeOriginalModelSceneInput *input);

/* Initial resident-scene installation must flatten articulated models before
 * the per-frame ONSCREEN publication pass has run. It uses the same exact
 * Model.render_pos bank and view-to-world transform, but does not treat the
 * transient visibility flag as an absence of model data. */
GeOriginalStageModelPublicationStatus
ge_original_stage_model_publication_resident_input(
    const GeOriginalPitemModelProvider *models, const void *definition,
    size_t visible_part_index, uint8_t room,
    const float view_to_world[4][4], GeOriginalModelSceneInput *input);

const char *ge_original_stage_model_publication_status_name(
    GeOriginalStageModelPublicationStatus status);

#endif
