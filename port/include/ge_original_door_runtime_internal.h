#ifndef GE_ORIGINAL_DOOR_RUNTIME_INTERNAL_H
#define GE_ORIGINAL_DOOR_RUNTIME_INTERNAL_H

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "ge_original_door_internal.h"
#include "ge_original_door_runtime.h"

s32 ge_port_door_runtime_global_timer(void);
s32 ge_port_door_runtime_clock_timer(void);
ObjectRecord *ge_port_door_runtime_object(DoorRecord *door);
DoorRecord *ge_port_door_runtime_native_definition(void *definition);
s32 ge_port_door_runtime_test_collision(PropRecord *prop);
void ge_port_door_runtime_update_shade(PropRecord *prop, rgba_u8 *color);
Vertex *ge_port_door_runtime_acquire_vertices(DoorRecord *door, s32 count);
void ge_port_door_runtime_publish_vertices(
    DoorRecord *door, const Vertex *vertices, s32 count);
void ge_port_door_runtime_sound(
    DoorRecord *door, GeOriginalDoorSoundEvent event);
void ge_port_door_runtime_note_bbox(void);
void ge_port_door_runtime_note_clipped(void);
void ge_port_door_runtime_note_portal(s32 open);
void ge_port_door_runtime_note_completed(s32 open);

void ge_original_door_activate_slice(DoorRecord *door, s32 state);
void ge_original_door_set_open_state_slice(DoorRecord *door, s32 state);
void ge_original_door_matrix_slice(DoorRecord *door, Mtxf *matrix);
void ge_original_door_runtime_tick_slice(DoorRecord *door);

#endif
