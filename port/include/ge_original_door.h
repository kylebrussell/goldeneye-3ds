#ifndef GE_ORIGINAL_DOOR_H
#define GE_ORIGINAL_DOOR_H

#include <stdint.h>

/* Train has the largest exact solo-stage setup at 53 authored doors.  Keep a
 * small fixed margin without introducing runtime allocation into the original
 * door lookup/tick path. */
#define GE_ORIGINAL_DOOR_NATIVE_CAPACITY 64U

typedef enum GeOriginalDoorStatus {
    GE_ORIGINAL_DOOR_OK = 0,
    GE_ORIGINAL_DOOR_INVALID_ARGUMENT,
    GE_ORIGINAL_DOOR_INVALID_SETUP,
    GE_ORIGINAL_DOOR_MISSING_PROVIDER,
    GE_ORIGINAL_DOOR_MODEL_UNAVAILABLE,
    GE_ORIGINAL_DOOR_POSITION_FAILED,
    GE_ORIGINAL_DOOR_WALK_UNAVAILABLE,
    GE_ORIGINAL_DOOR_INIT_FAILED
} GeOriginalDoorStatus;

typedef struct GeOriginalDoorProviders {
    void *context;
    int32_t (*model_load)(void *context, int32_t model_id);
    int (*resolve_model_instance)(void *context, int32_t model_id,
                                  void **header, void **model, float *scale);
    void *(*allocate_collision)(void *context, uint32_t size_bytes);
    int (*walk_tiles)(void *context, void **stan, float start_x,
                      float start_z, float destination_x,
                      float destination_z);
    int (*get_tile_rgb)(void *context, void *stan, float x, float z,
                        uint8_t rgb[3]);
    int (*portal_rooms)(void *context, const void *bound_pad,
                        int32_t *room_a, int32_t *room_b,
                        float point_a[3], float point_b[3]);
    int32_t (*find_portal)(void *context, int32_t room_a, int32_t room_b,
                           const float point_a[3], const float point_b[3]);
    void (*set_portal_open)(void *context, int32_t portal, int open);
    void (*register_room)(void *context, void *prop, int16_t room);
} GeOriginalDoorProviders;

typedef struct GeOriginalDoorPrepared {
    int32_t model_id;
    int16_t pad_id;
    int32_t portal_number;
    int32_t portal_room_a;
    int32_t portal_room_b;
    int32_t linked_door_offset;
    uint16_t door_flags;
    uint16_t door_type;
    float max_frac;
    float perim_frac;
    float accel;
    float decel;
    float max_speed;
    float open_position;
    float travel[3];
    float position[3];
    float centre[3];
    float matrix[4][4];
    float xscale, yscale, zscale;
    void *stan;
    void *model_header;
    void *model_instance;
    void *prop;
    void *collision_data;
    uint32_t model_load_calls;
    int portal_lookup_attempted;
    int portal_control_connected;
    int second_room_registered;
    int constructed;
} GeOriginalDoorPrepared;

void ge_original_door_bind(const GeOriginalDoorProviders *providers,
                           GeOriginalDoorPrepared *prepared);
GeOriginalDoorStatus ge_original_door_construct(void *definition,
                                                 int32_t command_index);
/* Releases the door-runtime sidecar owned for one promoted setup definition.
 * Prop, model-instance and collision ownership remains with the provider. */
int ge_original_door_release(void *definition);
uint32_t ge_original_door_capacity(void);
const char *ge_original_door_status_name(GeOriginalDoorStatus status);

#endif
