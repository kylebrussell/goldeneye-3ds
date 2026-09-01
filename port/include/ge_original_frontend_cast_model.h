#ifndef GE_ORIGINAL_FRONTEND_CAST_MODEL_H
#define GE_ORIGINAL_FRONTEND_CAST_MODEL_H

#include "ge_asset_pack.h"
#include "ge_dam_room.h"
#include "ge_original_frontend_cast.h"

#include <stddef.h>
#include <stdint.h>

typedef struct GeOriginalFrontendCastModel GeOriginalFrontendCastModel;

typedef enum GeOriginalFrontendCastModelStatus {
    GE_ORIGINAL_FRONTEND_CAST_MODEL_OK = 0,
    GE_ORIGINAL_FRONTEND_CAST_MODEL_INVALID_ARGUMENT,
    GE_ORIGINAL_FRONTEND_CAST_MODEL_ASSET_MISSING,
    GE_ORIGINAL_FRONTEND_CAST_MODEL_MODEL_UNAVAILABLE,
    GE_ORIGINAL_FRONTEND_CAST_MODEL_ANIMATION_UNAVAILABLE,
    GE_ORIGINAL_FRONTEND_CAST_MODEL_ATTACHMENT_UNAVAILABLE,
    GE_ORIGINAL_FRONTEND_CAST_MODEL_SCENE_UNAVAILABLE,
    GE_ORIGINAL_FRONTEND_CAST_MODEL_ALLOCATION_FAILED
} GeOriginalFrontendCastModelStatus;

typedef struct GeOriginalFrontendCastModelScene {
    const GeDamRoomWorldVertex *vertices;
    const GeDamRoomDrawBatch *batches;
    const int16_t *batch_model_types;
    size_t vertex_count;
    size_t batch_count;
    size_t triangle_count;
    size_t part_count;
    size_t character_part_count;
    size_t weapon_part_count;
    size_t allocated_part_capacity;
    GeOriginalFrontendCastSelection selection;
    float camera_eye[3];
    float camera_target[3];
    float camera_up[3];
    float root_position[3];
    float transformed_target[3];
    float fade;
    uint32_t animation_ticks;
    uint8_t weapon_attached;
    uint8_t weapon_attachment_switch;
    uint8_t weapon_left_hand_rotation;
    /* constructor_menu18_displaycast's exact ModelRenderData state. */
    uint8_t render_prop_type;
    uint8_t render_zbuffer_enabled;
    uint8_t render_cull_mode;
    uint8_t render_flags;
    uint8_t render_lighting_enabled;
    uint8_t render_texture_gen_enabled;
    float reflection_camera_eye_z;
} GeOriginalFrontendCastModelScene;

/* Owns the exact ROM-backed body/head/optional weapon instances selected by
 * ge_original_frontend_cast.  begin_selection intentionally replaces the
 * preceding actor arena, matching init/update_menu18_displaycast. */
GeOriginalFrontendCastModel *ge_original_frontend_cast_model_create(
    GeAssetPack *asset_pack, GeOriginalFrontendCastModelStatus *status);
void ge_original_frontend_cast_model_destroy(
    GeOriginalFrontendCastModel *owner);

GeOriginalFrontendCastModelStatus
ge_original_frontend_cast_model_begin_selection(
    GeOriginalFrontendCastModel *owner,
    const GeOriginalFrontendCastSelection *selection);

/* Executes constructor_menu18_displaycast's unchanged model ordering:
 * modelTickAnim, subcalcpos, identity matrix pass, camera damping, look-at
 * matrix pass, optional switch-3/5 weapon attachment, and scene collection. */
GeOriginalFrontendCastModelStatus ge_original_frontend_cast_model_tick(
    GeOriginalFrontendCastModel *owner, GeOriginalFrontendCast *cast,
    uint32_t clock_ticks, float timer_delta,
    GeOriginalFrontendCastModelScene *scene);

const char *ge_original_frontend_cast_model_status_name(
    GeOriginalFrontendCastModelStatus status);

#endif
