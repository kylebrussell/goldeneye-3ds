#ifndef GE_ORIGINAL_BOND_CAMERA_H
#define GE_ORIGINAL_BOND_CAMERA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GeOriginalBondCameraConfig {
    float camera_position[3];
    float camera_look_direction[3];
    float camera_up[3];
    float room_origin[3];
    float room_position_scale;
    float camera_local_scale;
    float visibility_scale;
    float projection[4][4];
    float vertical_fov_degrees;
    float perspective_aspect;
    float near_distance;
    int16_t viewport_scale[4];
    int16_t viewport_translation[4];
    uint8_t room;
} GeOriginalBondCameraConfig;

typedef struct GeOriginalBondCameraResult {
    float view[4][4];
    float view_to_world[4][4];
    float projection[4][4];
    float room_origin[3];
    float scaled_room_origin[3];
    float previous_room_origin_camera_rotation[3];
    int16_t viewport_scale[4];
    int16_t viewport_translation[4];
    uint32_t matrix_allocations;
    uint32_t light_allocations;
    uint32_t room_updates;
    uint32_t frustum_updates;
    uint8_t room;
} GeOriginalBondCameraResult;

typedef enum GeOriginalBondCameraStatus {
    GE_ORIGINAL_BOND_CAMERA_OK = 0,
    GE_ORIGINAL_BOND_CAMERA_INVALID_ARGUMENT,
    GE_ORIGINAL_BOND_CAMERA_INVALID_CONFIG,
    GE_ORIGINAL_BOND_CAMERA_ALLOCATION_FAILED,
    GE_ORIGINAL_BOND_CAMERA_INCOMPLETE
} GeOriginalBondCameraStatus;

/* Fills config->projection through libultra's original guPerspectiveF. */
GeOriginalBondCameraStatus ge_original_bond_camera_set_perspective(
    GeOriginalBondCameraConfig *config,
    float vertical_fov_degrees,
    float aspect,
    float near_distance,
    float far_distance);

/* Runs the exact bondviewUpdateCameraMatrices body compiled from
 * src/game/bondview2.c. The port layer supplies only bounded player storage,
 * room/scale state, dyn storage, and observable frustum side effects. */
GeOriginalBondCameraStatus ge_original_bond_camera_run(
    const GeOriginalBondCameraConfig *config,
    GeOriginalBondCameraResult *result);

/* Publishes the exact camera result into the canonical live player object.
 * This is the native platform handoff between the bounded matrix producer
 * and original gun/gameplay consumers. */
int ge_original_bond_camera_publish_live_player(
    const GeOriginalBondCameraConfig *config,
    const GeOriginalBondCameraResult *result);

const char *ge_original_bond_camera_status_name(
    GeOriginalBondCameraStatus status);

#ifdef __cplusplus
}
#endif

#endif
