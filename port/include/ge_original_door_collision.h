#ifndef GE_ORIGINAL_DOOR_COLLISION_H
#define GE_ORIGINAL_DOOR_COLLISION_H

#include <stdint.h>

typedef enum GeOriginalDoorCollisionStatus {
    GE_ORIGINAL_DOOR_COLLISION_OK = 0,
    GE_ORIGINAL_DOOR_COLLISION_INVALID_ARGUMENT,
    GE_ORIGINAL_DOOR_COLLISION_MISSING_CHARACTER_PROVIDER,
    GE_ORIGINAL_DOOR_COLLISION_MISSING_OBJECT_METADATA
} GeOriginalDoorCollisionStatus;

typedef struct GeOriginalDoorCharacterCollisionProviders {
    /* Optional ABI overrides.  NULL callbacks use the mechanically extracted
     * chr.c ChrRecord bounds/width/height/ground bodies directly. */
    void *context;
    int (*polygon_bounds)(void *context, void *prop, void **polygon,
                          int32_t *edges, float *top, float *bottom);
    int (*cylinder_bounds)(void *context, void *prop, float *radius,
                           float *height, float *lower_offset);
    int (*ground)(void *context, void *prop, float *ground);
} GeOriginalDoorCharacterCollisionProviders;

typedef struct GeOriginalDoorCollisionState {
    uint32_t tests;
    uint32_t clear_results;
    uint32_t blocked_results;
    uint32_t missing_character_calls;
    uint32_t missing_object_metadata_calls;
    GeOriginalDoorCollisionStatus status;
} GeOriginalDoorCollisionState;

void ge_original_door_collision_bind(
    const GeOriginalDoorCharacterCollisionProviders *providers,
    GeOriginalDoorCollisionState *state);

/* GeOriginalDoorRuntimeProviders::test_collision compatible callback. */
int ge_original_door_collision_test(void *context, void *prop);

const char *ge_original_door_collision_status_name(
    GeOriginalDoorCollisionStatus status);

#endif
