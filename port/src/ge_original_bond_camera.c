#ifdef GE_PORT_BOND_CAMERA_SLICE

#include "ge_original_bond_camera.h"
#include "ge_original_bond_camera_internal.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define GE_BOND_CAMERA_MATRIX_CAPACITY 5U
#define GE_BOND_CAMERA_LIGHT_CAPACITY 2U

typedef union GeBondCameraMatrixSlot {
    Mtx fixed;
    Mtxf floating;
} GeBondCameraMatrixSlot;

typedef struct GeBondCameraHarness {
    GePortBondCameraPlayer player;
    Mtxf projection;
    GeBondCameraMatrixSlot matrices[GE_BOND_CAMERA_MATRIX_CAPACITY];
    LookAt lookat;
    coord3d room_origin;
    f32 room_position_scale;
    f32 visibility_scale;
    u32 matrix_cursor;
    u32 light_count;
    u32 room_updates;
    u32 frustum_updates;
    u8 room;
    s32 allocation_failed;
} GeBondCameraHarness;

static GeBondCameraHarness ge_bond_camera_harness;
GePortBondCameraPlayer *ge_original_bond_camera_player =
    &ge_bond_camera_harness.player;
f32 D_800364CC = 1.0f;

static int ge_bond_camera_float3_finite(const float value[3])
{
    return isfinite(value[0]) && isfinite(value[1]) && isfinite(value[2]);
}

static int ge_bond_camera_matrix_finite(const float value[4][4])
{
    size_t row;
    size_t column;

    for (row = 0U; row < 4U; ++row) {
        for (column = 0U; column < 4U; ++column) {
            if (!isfinite(value[row][column])) {
                return 0;
            }
        }
    }
    return 1;
}

static int ge_bond_camera_basis_valid(const float look[3], const float up[3])
{
    const float look_length_squared = look[0] * look[0]
        + look[1] * look[1] + look[2] * look[2];
    const float up_length_squared = up[0] * up[0]
        + up[1] * up[1] + up[2] * up[2];
    const float cross_x = up[1] * look[2] - up[2] * look[1];
    const float cross_y = up[2] * look[0] - up[0] * look[2];
    const float cross_z = up[0] * look[1] - up[1] * look[0];
    const float cross_length_squared = cross_x * cross_x
        + cross_y * cross_y + cross_z * cross_z;

    return isfinite(look_length_squared)
        && isfinite(up_length_squared)
        && isfinite(cross_length_squared)
        && look_length_squared > 1.0e-12f
        && up_length_squared > 1.0e-12f
        && cross_length_squared > 1.0e-12f;
}

u8 bondviewGetCurrentPlayersRoom(void)
{
    return ge_bond_camera_harness.room;
}

void getRoomPositionScaledByIndex(s32 room, coord3d *position)
{
    (void)room;
    *position = ge_bond_camera_harness.room_origin;
}

f32 get_room_data_float1(void)
{
    return ge_bond_camera_harness.room_position_scale;
}

void setPlayerRoomField(s32 room)
{
    ge_bond_camera_harness.room = (u8)room;
    ge_bond_camera_harness.room_updates++;
}

Mtx *dynAllocateMatrix(void)
{
    Mtx *matrix;

    if (ge_bond_camera_harness.matrix_cursor
            >= GE_BOND_CAMERA_MATRIX_CAPACITY) {
        ge_bond_camera_harness.allocation_failed = TRUE;
        return NULL;
    }
    matrix = &ge_bond_camera_harness.matrices[
        ge_bond_camera_harness.matrix_cursor].fixed;
    ge_bond_camera_harness.matrix_cursor++;
    memset(matrix, 0, sizeof(*matrix));
    return matrix;
}

LookAt *dynAllocateLights(s32 count)
{
    if (count < 0 || (u32)count > GE_BOND_CAMERA_LIGHT_CAPACITY) {
        ge_bond_camera_harness.allocation_failed = TRUE;
        return NULL;
    }
    ge_bond_camera_harness.light_count = (u32)count;
    memset(&ge_bond_camera_harness.lookat, 0,
           sizeof(ge_bond_camera_harness.lookat));
    return &ge_bond_camera_harness.lookat;
}

Mtxf *currentPlayerGetProjectionMatrixF(void)
{
    return g_CurrentPlayer->projmatrixf;
}

void set_BONDdata_field_10E0(s32 value)
{
    g_CurrentPlayer->field_10E0 = value;
}

f32 bgGetLevelVisibilityScale(void)
{
    return ge_bond_camera_harness.visibility_scale;
}

void currentPlayerSetMatrix10C8(Mtx *matrix)
{
    g_CurrentPlayer->field_10C8 = matrix;
}

void currentPlayerSetMatrix10C4(Mtx *matrix)
{
    g_CurrentPlayer->field_10C4 = matrix;
}

void *currentPlayerSetMatrix10CC(Mtxf *matrix)
{
    g_CurrentPlayer->field_10E8 = g_CurrentPlayer->field_10CC;
    g_CurrentPlayer->field_10CC = matrix;
    return matrix;
}

void currentPlayerSetViewToWorldMtxf(Mtxf *matrix)
{
    g_CurrentPlayer->field_10EC = g_CurrentPlayer->viewtoworldmtxf;
    g_CurrentPlayer->viewtoworldmtxf = matrix;
}

void sub_GAME_7F078464(s32 value)
{
    g_CurrentPlayer->field_10E4 = value;
}

void bondviewUpdateFrustumPlanes(void)
{
    ge_bond_camera_harness.frustum_updates++;
}

Mtxf *camGetWorldToScreenMtxf(void)
{
    return g_CurrentPlayer->field_10CC;
}

GeOriginalBondCameraStatus ge_original_bond_camera_run(
    const GeOriginalBondCameraConfig *config,
    GeOriginalBondCameraResult *result)
{
    coord3d camera_position;
    coord3d camera_look_direction;
    coord3d camera_up;

    if (config == NULL || result == NULL) {
        return GE_ORIGINAL_BOND_CAMERA_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    if (!ge_bond_camera_float3_finite(config->camera_position)
            || !ge_bond_camera_float3_finite(
                config->camera_look_direction)
            || !ge_bond_camera_float3_finite(config->camera_up)
            || !ge_bond_camera_float3_finite(config->room_origin)
            || !ge_bond_camera_basis_valid(config->camera_look_direction,
                                            config->camera_up)
            || !isfinite(config->room_position_scale)
            || !isfinite(config->camera_local_scale)
            || !isfinite(config->visibility_scale)
            || !ge_bond_camera_matrix_finite(config->projection)
            || config->room_position_scale <= 0.0f
            || config->camera_local_scale <= 0.0f
            || config->visibility_scale <= 0.0f
            || config->viewport_scale[0] == 0
            || config->viewport_scale[1] == 0) {
        return GE_ORIGINAL_BOND_CAMERA_INVALID_CONFIG;
    }

    memset(&ge_bond_camera_harness, 0, sizeof(ge_bond_camera_harness));
    ge_bond_camera_harness.room_origin.x = config->room_origin[0];
    ge_bond_camera_harness.room_origin.y = config->room_origin[1];
    ge_bond_camera_harness.room_origin.z = config->room_origin[2];
    ge_bond_camera_harness.room_position_scale = config->room_position_scale;
    ge_bond_camera_harness.visibility_scale = config->visibility_scale;
    ge_bond_camera_harness.room = config->room;
    /* Projection storage is separate from the five dyn matrix allocations
     * made by the original function. */
    memcpy(ge_bond_camera_harness.projection.m, config->projection,
           sizeof(ge_bond_camera_harness.projection.m));
    ge_bond_camera_harness.player.projmatrixf =
        &ge_bond_camera_harness.projection;
    D_800364CC = config->camera_local_scale;

    camera_position.x = config->camera_position[0];
    camera_position.y = config->camera_position[1];
    camera_position.z = config->camera_position[2];
    camera_look_direction.x = config->camera_look_direction[0];
    camera_look_direction.y = config->camera_look_direction[1];
    camera_look_direction.z = config->camera_look_direction[2];
    camera_up.x = config->camera_up[0];
    camera_up.y = config->camera_up[1];
    camera_up.z = config->camera_up[2];

    bondviewUpdateCameraMatrices(
        &camera_position, &camera_look_direction, &camera_up);
    if (ge_bond_camera_harness.allocation_failed != FALSE) {
        return GE_ORIGINAL_BOND_CAMERA_ALLOCATION_FAILED;
    }
    if (g_CurrentPlayer->field_10CC == NULL
            || g_CurrentPlayer->viewtoworldmtxf == NULL
            || ge_bond_camera_harness.matrix_cursor
                != GE_BOND_CAMERA_MATRIX_CAPACITY
            || ge_bond_camera_harness.light_count
                != GE_BOND_CAMERA_LIGHT_CAPACITY
            || ge_bond_camera_harness.room_updates != 1U
            || ge_bond_camera_harness.frustum_updates != 1U) {
        return GE_ORIGINAL_BOND_CAMERA_INCOMPLETE;
    }

    memcpy(result->view, g_CurrentPlayer->field_10CC->m,
           sizeof(result->view));
    memcpy(result->view_to_world, g_CurrentPlayer->viewtoworldmtxf->m,
           sizeof(result->view_to_world));
    memcpy(result->projection, config->projection, sizeof(result->projection));
    memcpy(result->room_origin, g_CurrentPlayer->current_model_pos.f,
           sizeof(result->room_origin));
    memcpy(result->scaled_room_origin, g_CurrentPlayer->current_room_pos.f,
           sizeof(result->scaled_room_origin));
    memcpy(result->previous_room_origin_camera_rotation,
           g_CurrentPlayer->previous_model_pos.f,
           sizeof(result->previous_room_origin_camera_rotation));
    memcpy(result->viewport_scale, config->viewport_scale,
           sizeof(result->viewport_scale));
    memcpy(result->viewport_translation, config->viewport_translation,
           sizeof(result->viewport_translation));
    result->matrix_allocations = ge_bond_camera_harness.matrix_cursor;
    result->light_allocations = ge_bond_camera_harness.light_count;
    result->room_updates = ge_bond_camera_harness.room_updates;
    result->frustum_updates = ge_bond_camera_harness.frustum_updates;
    result->room = ge_bond_camera_harness.room;
    return GE_ORIGINAL_BOND_CAMERA_OK;
}

GeOriginalBondCameraStatus ge_original_bond_camera_set_perspective(
    GeOriginalBondCameraConfig *config,
    float vertical_fov_degrees,
    float aspect,
    float near_distance,
    float far_distance)
{
    u16 perspective_normalize;

    if (config == NULL) {
        return GE_ORIGINAL_BOND_CAMERA_INVALID_ARGUMENT;
    }
    if (!isfinite(vertical_fov_degrees) || !isfinite(aspect)
            || !isfinite(near_distance) || !isfinite(far_distance)
            || vertical_fov_degrees <= 0.0f
            || vertical_fov_degrees >= 180.0f
            || aspect <= 0.0f || near_distance <= 0.0f
            || far_distance <= near_distance) {
        return GE_ORIGINAL_BOND_CAMERA_INVALID_CONFIG;
    }
    guPerspectiveF(config->projection, &perspective_normalize,
        vertical_fov_degrees, aspect, near_distance, far_distance, 1.0f);
    config->vertical_fov_degrees = vertical_fov_degrees;
    config->perspective_aspect = aspect;
    config->near_distance = near_distance;
    return perspective_normalize != 0U
        ? GE_ORIGINAL_BOND_CAMERA_OK
        : GE_ORIGINAL_BOND_CAMERA_INCOMPLETE;
}

const char *ge_original_bond_camera_status_name(
    GeOriginalBondCameraStatus status)
{
    switch (status) {
    case GE_ORIGINAL_BOND_CAMERA_OK:
        return "ok";
    case GE_ORIGINAL_BOND_CAMERA_INVALID_ARGUMENT:
        return "invalid argument";
    case GE_ORIGINAL_BOND_CAMERA_INVALID_CONFIG:
        return "invalid config";
    case GE_ORIGINAL_BOND_CAMERA_ALLOCATION_FAILED:
        return "allocation failed";
    case GE_ORIGINAL_BOND_CAMERA_INCOMPLETE:
        return "incomplete";
    default:
        return "unknown";
    }
}

#endif
