#ifndef GE_ORIGINAL_DOOR_SCENE_H
#define GE_ORIGINAL_DOOR_SCENE_H

#include <stddef.h>
#include <stdint.h>

#include "ge_original_door_runtime.h"
#include "ge_original_model178_runtime.h"
#include "ge_original_model_scene.h"

typedef enum GeOriginalDoorSceneStatus {
    GE_ORIGINAL_DOOR_SCENE_OK = 0,
    GE_ORIGINAL_DOOR_SCENE_INVALID_ARGUMENT,
    GE_ORIGINAL_DOOR_SCENE_SNAPSHOT_UNAVAILABLE,
    GE_ORIGINAL_DOOR_SCENE_INVALID_MODEL_LAYOUT,
    GE_ORIGINAL_DOOR_SCENE_INVALID_VERTEX_PUBLICATION
} GeOriginalDoorSceneStatus;

/* Stable renderer handoff for one authored PdamgatedoorZ instance.  blob is a
 * private big-endian copy whose segment-4 vertex array has been replaced by
 * the exact clipped Vertex publication when the door runtime produced one. */
typedef struct GeOriginalDoorScenePublication {
    GeOriginalDoorSceneStatus status;
    GeOriginalDoorRuntimePublication runtime;
    GeOriginalModelSceneInput input;
    uint8_t blob[GE_ORIGINAL_MODEL178_BLOB_SIZE];
    uint8_t uses_clipped_vertices;
} GeOriginalDoorScenePublication;

GeOriginalDoorSceneStatus ge_original_door_scene_prepare(
    const void *door_definition, const void *model_blob,
    size_t model_blob_size, GeOriginalDoorScenePublication *publication);

const char *ge_original_door_scene_status_name(
    GeOriginalDoorSceneStatus status);

#endif
