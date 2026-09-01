#ifndef GE_ORIGINAL_DEFAULT_OBJECT_H
#define GE_ORIGINAL_DEFAULT_OBJECT_H

#include <stdint.h>

typedef enum GeOriginalDefaultObjectStatus {
    GE_ORIGINAL_DEFAULT_OBJECT_OK = 0,
    GE_ORIGINAL_DEFAULT_OBJECT_INVALID_ARGUMENT,
    GE_ORIGINAL_DEFAULT_OBJECT_MISSING_PROVIDER,
    GE_ORIGINAL_DEFAULT_OBJECT_UNSUPPORTED_BRANCH,
    GE_ORIGINAL_DEFAULT_OBJECT_INVALID_PAD,
    GE_ORIGINAL_DEFAULT_OBJECT_MODEL_UNAVAILABLE,
    GE_ORIGINAL_DEFAULT_OBJECT_COLLISION_ALLOCATION_FAILED,
    GE_ORIGINAL_DEFAULT_OBJECT_POSITION_FAILED,
    GE_ORIGINAL_DEFAULT_OBJECT_INIT_FAILED,
    GE_ORIGINAL_DEFAULT_OBJECT_NOT_CONSTRUCTED,
    GE_ORIGINAL_DEFAULT_OBJECT_PLACEMENT_DEPENDENCY_UNAVAILABLE,
    GE_ORIGINAL_DEFAULT_OBJECT_PLACEMENT_FAILED
} GeOriginalDefaultObjectStatus;

typedef enum GeOriginalDefaultObjectPlacementStage {
    GE_ORIGINAL_DEFAULT_OBJECT_PLACEMENT_NOT_STARTED = 0,
    GE_ORIGINAL_DEFAULT_OBJECT_PLACEMENT_NEEDS_FLOOR = 1,
    GE_ORIGINAL_DEFAULT_OBJECT_PLACEMENT_NEEDS_ROOM_BOUNDS = 2,
    GE_ORIGINAL_DEFAULT_OBJECT_PLACEMENT_NEEDS_WALK = 3,
    GE_ORIGINAL_DEFAULT_OBJECT_PLACEMENT_NEEDS_COLLISION = 4,
    GE_ORIGINAL_DEFAULT_OBJECT_PLACEMENT_NEEDS_TILE_RGB = 5,
    GE_ORIGINAL_DEFAULT_OBJECT_PLACEMENT_COMPLETE = 6
} GeOriginalDefaultObjectPlacementStage;

typedef struct GeOriginalDefaultObjectProviders {
    void *context;
    int32_t (*model_load)(void *context, int32_t model_id);
    int32_t (*get_player_count)(void *context);
    int32_t (*get_scenario)(void *context);
    int (*resolve_model_instance)(void *context, int32_t model_id,
                                  void **model_header, void **model_instance,
                                  float *pitem_scale);
    void *(*allocate_collision)(void *context, uint32_t size_bytes);
    int (*get_floor_y)(void *context, void *stan, float x, float z,
                       float *floor_y);
    /* Returns 1 for a containing room object, 0 for none, negative when the
     * original room-object query is unavailable. */
    int (*get_room_object_bounds)(void *context, const float position[3],
                                  int16_t room, float *top, float *bottom);
    /* Returns original walkTiles result, or a negative unavailable status. */
    int (*walk_tiles)(void *context, void **stan, float start_x,
                      float start_z, float destination_x,
                      float destination_z);
    int (*get_tile_rgb)(void *context, void *stan, float x, float z,
                        uint8_t rgb[3]);
} GeOriginalDefaultObjectProviders;

typedef struct GeOriginalDefaultObjectPrepared {
    int32_t model_id;
    int16_t pad_id;
    uint8_t state;
    uint32_t flags;
    uint32_t flags2;
    float extra_scale;
    float damage;
    float position[3];
    float shade_position[3];
    float matrix[4][4];
    void *stan;
    void *model_header;
    void *model_instance;
    void *prop;
    void *collision_data;
    float pitem_scale;
    uint32_t model_load_calls;
    float placement_position[3];
    void *placement_stan;
    uint32_t placement_stage;
    int prepared;
    int position_validated;
    int object_initialized;
    int placement_completed;
    int bound_pad;
} GeOriginalDefaultObjectPrepared;

void ge_original_default_object_bind(
    const GeOriginalDefaultObjectProviders *providers,
    GeOriginalDefaultObjectPrepared *prepared);

/*
 * Runs the exact ordinary-pad prefix of domakedefaultobj (including the
 * positive-pad setupSingleMonitor branch) through the point
 * where position, basis, scale, damage and starting STAN are prepared.
 * Completion requires native PitemZ/model instance and objInit closure.
 */
GeOriginalDefaultObjectStatus ge_original_default_object_prepare_standard(
    void *object_definition, int32_t command_index);

/* Continues the prefix through radius-zero getposstan and the exact
 * preallocated objInit success path. It stops before moveToPad/shading. */
GeOriginalDefaultObjectStatus ge_original_default_object_construct_standard(
    void *object_definition, int32_t command_index);

/* Continues through the exact moveToPad body. The function returns before
 * mutating object placement if a required provider seam is unavailable. */
GeOriginalDefaultObjectStatus ge_original_default_object_place_standard(
    void *object_definition);

const char *ge_original_default_object_status_name(
    GeOriginalDefaultObjectStatus status);

#endif
