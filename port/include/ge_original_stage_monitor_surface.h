#ifndef GE_ORIGINAL_STAGE_MONITOR_SURFACE_H
#define GE_ORIGINAL_STAGE_MONITOR_SURFACE_H

#include "ge_dam_room.h"
#include "ge_original_dam_monitor_render.h"

#include <stddef.h>
#include <stdint.h>

enum {
    GE_ORIGINAL_STAGE_MONITOR_SURFACE_VERTEX_COUNT = 6,
    GE_ORIGINAL_STAGE_MONITOR_SURFACE_TRIANGLE_COUNT = 2
};

typedef enum GeOriginalStageMonitorSurfaceStatus {
    GE_ORIGINAL_STAGE_MONITOR_SURFACE_OK = 0,
    GE_ORIGINAL_STAGE_MONITOR_SURFACE_INVALID_ARGUMENT,
    GE_ORIGINAL_STAGE_MONITOR_SURFACE_INVALID_SNAPSHOT,
    GE_ORIGINAL_STAGE_MONITOR_SURFACE_TARGET_MISMATCH
} GeOriginalStageMonitorSurfaceStatus;

typedef struct GeOriginalStageMonitorSurfaceResult {
    GeOriginalStageMonitorSurfaceStatus status;
    uint32_t room_id;
    uint32_t texture_id;
    size_t batch_index;
    size_t first_vertex;
    size_t vertex_count;
    size_t triangle_count;
} GeOriginalStageMonitorSurfaceResult;

/* True when two canonical monitor ticks produce identical renderer output.
 * pause60 is intentionally excluded: it schedules the next original image
 * change but does not alter the currently published texture, vertices, or
 * material. This lets the 3DS backend retain exact monitor timing without
 * recommitting and flushing an unchanged six-vertex surface every frame. */
int ge_original_stage_monitor_surface_output_equal(
    const GeOriginalDamMonitorRenderSnapshot *left,
    const GeOriginalDamMonitorRenderSnapshot *right);

/* Applies the exact dynamic quad emitted by process_monitor_animation_microcode
 * to the matching flattened DLCOLLISION batch. Spatial output is retained from
 * the canonical model scene; authored local coordinates must match before the
 * dynamic s/t, colour, texture and render state are committed atomically. */
GeOriginalStageMonitorSurfaceStatus ge_original_stage_monitor_surface_apply(
    const GeOriginalDamMonitorRenderSnapshot *snapshot,
    GeDamRoomSceneStorage *scene, size_t batch_index,
    GeOriginalStageMonitorSurfaceResult *result);

/* Locates the unique exact authored quad inside one flattened model-part batch
 * range, then applies it. This is the direct scene-part publication seam. */
GeOriginalStageMonitorSurfaceStatus
ge_original_stage_monitor_surface_apply_part(
    const GeOriginalDamMonitorRenderSnapshot *snapshot,
    GeDamRoomSceneStorage *scene, size_t first_batch, size_t batch_count,
    GeOriginalStageMonitorSurfaceResult *result);

const char *ge_original_stage_monitor_surface_status_name(
    GeOriginalStageMonitorSurfaceStatus status);

#endif
