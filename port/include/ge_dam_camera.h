#ifndef GE_DAM_CAMERA_H
#define GE_DAM_CAMERA_H

#include "ge_dam_room.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GE_DAM_CAMERA_VERTICAL_FOV_RADIANS 1.0471975511965976f
#define GE_DAM_CAMERA_ASPECT_5_3 (5.0f / 3.0f)
#define GE_DAM_CAMERA_MAX_CLIPPED_POLYGON_VERTICES 12U

typedef struct GeDamCameraConfig {
    float eye[3];
    float forward[3];
    float up[3];
    float vertical_fov_radians;
    float aspect;
    float near_distance;
    float far_distance;
    float viewport_x;
    float viewport_y;
    float viewport_width;
    float viewport_height;
} GeDamCameraConfig;

typedef struct GeDamCamera {
    float view[4][4];
    float projection[4][4];
    float viewport_x;
    float viewport_y;
    float viewport_width;
    float viewport_height;
    /* Affine NDC-to-screen transform. The explicit values preserve the
     * original RSP Vp convention, whose Y scale is positive, without making
     * gameplay camera policy part of this backend. */
    float viewport_scale[2];
    float viewport_translation[2];
} GeDamCamera;

typedef struct GeDamCameraVertex {
    float world[3];
    float camera[3];
    float clip[4];
    float screen[3];
    float texture[2];
    float texture_over_depth[2];
    /* Reciprocal homogeneous clip W. Named inverse_depth as N64 perspective
     * matrices produce W equal to positive camera depth. */
    float inverse_depth;
    uint8_t rgba[4];
} GeDamCameraVertex;

typedef struct GeDamCameraStorage {
    GeDamCameraVertex *vertices;
    size_t vertex_capacity;
} GeDamCameraStorage;

typedef struct GeDamCameraResult {
    size_t input_triangle_count;
    size_t visible_input_triangle_count;
    size_t output_triangle_count;
    size_t required_vertex_count;
    size_t vertex_count;
    /* Number of source-triangle frustum-clip evaluations performed. This is
     * work telemetry only and does not affect projection output. */
    size_t clip_triangle_evaluations;
} GeDamCameraResult;

typedef struct GeDamCameraBatch {
    size_t source_batch;
    size_t first_vertex;
    size_t vertex_count;
} GeDamCameraBatch;

typedef struct GeDamCameraSceneStorage {
    GeDamCameraVertex *vertices;
    size_t vertex_capacity;
    GeDamCameraBatch *batches;
    size_t batch_capacity;
} GeDamCameraSceneStorage;

typedef struct GeDamCameraSceneResult {
    size_t input_triangle_count;
    size_t visible_input_triangle_count;
    size_t output_triangle_count;
    size_t required_vertex_count;
    size_t required_batch_count;
    size_t vertex_count;
    size_t batch_count;
    /* Exact clipping work for capacity/performance regression coverage. */
    size_t clip_triangle_evaluations;
} GeDamCameraSceneResult;

typedef enum GeDamCameraStatus {
    GE_DAM_CAMERA_OK = 0,
    GE_DAM_CAMERA_INVALID_ARGUMENT,
    GE_DAM_CAMERA_INVALID_CONFIG,
    GE_DAM_CAMERA_NONFINITE_INPUT,
    GE_DAM_CAMERA_CAPACITY_EXCEEDED
} GeDamCameraStatus;

/* Bring-up/test convenience only. Returns a 60 degree vertical field of view,
 * 5:3 aspect, and 400 by 240 viewport. Production gameplay should use the
 * matrices produced by the original engine through prepare_matrices. */
GeDamCameraConfig ge_dam_camera_default_config(void);

/* Bring-up/test convenience that builds libultra-compatible view/projection
 * matrices. forward and up need not be normalized, but they must be nonzero
 * and nonparallel. This function does not define gameplay camera policy. */
GeDamCameraStatus ge_dam_camera_prepare(const GeDamCameraConfig *config,
                                        GeDamCamera *camera);

/* Production entry point for original engine state. Matrices use libultra's
 * Mtxf convention: row vectors are transformed as v * view * projection.
 * Pass camGetWorldToScreenMtxf() and currentPlayerGetProjectionMatrixF()
 * after bondviewUpdateCameraMatrices/viSetupCurrentPlayerView. The matrices
 * are copied, so their dynalloc lifetime does not escape this call. */
GeDamCameraStatus ge_dam_camera_prepare_matrices(
    const float view[4][4],
    const float projection[4][4],
    float viewport_x,
    float viewport_y,
    float viewport_width,
    float viewport_height,
    GeDamCamera *camera);

/* Original-engine entry point. viewport_scale and viewport_translation are
 * copied directly from Vp.vp.vscale/vtrans after viSetupCurrentPlayerView.
 * Their two fractional bits are decoded exactly as the RSP does. This keeps
 * the positive-Y N64 viewport convention; callers must not pre-flip it. */
GeDamCameraStatus ge_dam_camera_prepare_rsp_viewport(
    const float view[4][4],
    const float projection[4][4],
    const int16_t viewport_scale[4],
    const int16_t viewport_translation[4],
    GeDamCamera *camera);

/* Adapts authored background coordinates to a camera whose matrices consume
 * original runtime coordinates. For row-vector libultra matrices this is
 * exactly authored * scale * view, so only the first three rows of view are
 * scaled and the source geometry remains untouched. */
GeDamCameraStatus ge_dam_camera_scale_world(
    const GeDamCamera *camera,
    float authored_to_runtime_scale,
    GeDamCamera *scaled_camera);

/* Projects independent triangles from ge_dam_rooms_build. input_vertex_count
 * must be divisible by three. All six frustum planes are clipped and clipped
 * polygons are triangulated while preserving input winding.
 *
 * A null/zero storage is a supported capacity query. If storage is too small,
 * required_vertex_count is reported and vertex_count remains zero; no partial
 * output is published.
 *
 * texture[] remains in the original GBI coordinate units. For a renderer that
 * CPU-projects to an orthographic screen, perspective-correct UV at a pixel is
 * interpolate(texture_over_depth) / interpolate(inverse_depth). A renderer
 * using true perspective clip coordinates may use texture[] directly. */
GeDamCameraStatus ge_dam_camera_project(
    const GeDamCamera *camera,
    const GeDamRoomWorldVertex *input_vertices,
    size_t input_vertex_count,
    const GeDamCameraStorage *storage,
    GeDamCameraResult *result);

/* Projects each decoded display-list batch independently so frustum clipping
 * can split or discard triangles without allowing vertices to cross an
 * original material boundary. A null storage is a supported capacity query. */
GeDamCameraStatus ge_dam_camera_project_batches(
    const GeDamCamera *camera,
    const GeDamRoomWorldVertex *input_vertices,
    size_t input_vertex_count,
    const GeDamRoomDrawBatch *input_batches,
    size_t input_batch_count,
    const GeDamCameraSceneStorage *storage,
    GeDamCameraSceneResult *result);

/* Fixed-capacity native-renderer path. Unlike the atomic/query API above,
 * this emits each batch during its first and only clipping traversal. On a
 * capacity error scratch storage may contain partial output and must not be
 * published. Successful output is identical to ge_dam_camera_project_batches
 * while clip_triangle_evaluations equals the source triangle count. */
GeDamCameraStatus ge_dam_camera_project_batches_bounded(
    const GeDamCamera *camera,
    const GeDamRoomWorldVertex *input_vertices,
    size_t input_vertex_count,
    const GeDamRoomDrawBatch *input_batches,
    size_t input_batch_count,
    const GeDamCameraSceneStorage *storage,
    GeDamCameraSceneResult *result);

const char *ge_dam_camera_status_name(GeDamCameraStatus status);

#ifdef __cplusplus
}
#endif

#endif
