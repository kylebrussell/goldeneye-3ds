#ifndef GE_ORIGINAL_DOOR_RUNTIME_H
#define GE_ORIGINAL_DOOR_RUNTIME_H

#include <stdint.h>

typedef enum GeOriginalDoorSoundEvent {
    GE_ORIGINAL_DOOR_SOUND_START_OPEN = 0,
    GE_ORIGINAL_DOOR_SOUND_START_CLOSE,
    GE_ORIGINAL_DOOR_SOUND_FINISH_OPEN,
    GE_ORIGINAL_DOOR_SOUND_FINISH_CLOSE
} GeOriginalDoorSoundEvent;

typedef enum GeOriginalDoorRuntimeStatus {
    GE_ORIGINAL_DOOR_RUNTIME_OK = 0,
    GE_ORIGINAL_DOOR_RUNTIME_INVALID_ARGUMENT,
    GE_ORIGINAL_DOOR_RUNTIME_MISSING_COLLISION_PROVIDER,
    GE_ORIGINAL_DOOR_RUNTIME_MISSING_VERTEX_STORAGE
} GeOriginalDoorRuntimeStatus;

typedef struct GeOriginalDoorRuntimeProviders {
    void *context;
    int32_t (*global_timer)(void *context);
    int32_t (*clock_timer)(void *context);
    int (*test_collision)(void *context, void *prop);
    void (*update_shade)(void *context, void *prop, uint8_t rgba[4]);
    void *(*acquire_vertices)(void *context, void *door,
                              uint32_t vertex_count,
                              uint32_t bytes_per_vertex);
    void (*publish_vertices)(void *context, void *door,
                             const void *vertices, uint32_t vertex_count);
    void (*sound_event)(void *context, void *door,
                        GeOriginalDoorSoundEvent event);
} GeOriginalDoorRuntimeProviders;

typedef struct GeOriginalDoorRuntimeState {
    uint32_t ticks;
    uint32_t collision_tests;
    uint32_t collision_rollbacks;
    uint32_t bbox_rebuilds;
    uint32_t clipped_vertex_rebuilds;
    uint32_t portal_open_events;
    uint32_t portal_close_events;
    uint32_t sound_events;
    uint32_t completed_opens;
    uint32_t completed_closes;
    int32_t last_global_timer;
    int32_t last_clock_timer;
    GeOriginalDoorRuntimeStatus status;
} GeOriginalDoorRuntimeState;

#define GE_ORIGINAL_DOOR_COLLISION_EDGE_CAPACITY 8U
#define GE_ORIGINAL_DOOR_MATRIX_CAPACITY 16U

/* Immutable publication of the exact state produced by the selected
 * propobj.c door runtime.  matrix includes the canonical displacement and
 * DOORFLAG_FLIP transform.  clipped_vertices is model-local Vertex data and
 * remains owned by its storage provider (or the door runtime fallback) until
 * the next bind or publication. */
typedef struct GeOriginalDoorRuntimePublication {
    float matrix[4][4];
    /* Canonical object-render matrices in model matrix-index order. Entry 0
     * is the base transform. Caverns eyelid uses entries 1..2 and iris uses
     * entries 1..12 exactly as the decompiled render branch does. */
    float matrices[GE_ORIGINAL_DOOR_MATRIX_CAPACITY][4][4];
    float position[3];
    float bbox[6];
    float collision_polygon[GE_ORIGINAL_DOOR_COLLISION_EDGE_CAPACITY][2];
    float collision_top;
    float collision_bottom;
    float open_position;
    float max_frac;
    float speed;
    const void *clipped_vertices;
    uint32_t clipped_vertex_count;
    uint32_t clipped_vertex_stride;
    uint32_t generation;
    uint16_t matrix_count;
    uint16_t articulated;
    int32_t collision_edges;
    int32_t open_state;
    int32_t portal_number;
    int16_t room;
} GeOriginalDoorRuntimePublication;

void ge_original_door_runtime_bind(
    const GeOriginalDoorRuntimeProviders *providers,
    GeOriginalDoorRuntimeState *state);
GeOriginalDoorRuntimeStatus ge_original_door_runtime_activate(
    void *door, int32_t state);
GeOriginalDoorRuntimeStatus ge_original_door_runtime_tick(void *door);
int ge_original_door_runtime_link_pair(void *first_definition,
                                       void *second_definition);
/* Same validity/generation semantics as snapshot without matrix construction. */
int ge_original_door_runtime_generation(const void *door, uint32_t *generation);
int ge_original_door_runtime_snapshot(
    const void *door, GeOriginalDoorRuntimePublication *publication);
const char *ge_original_door_runtime_status_name(
    GeOriginalDoorRuntimeStatus status);

#endif
