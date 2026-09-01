#ifndef GE_ORIGINAL_DOOR_COLLISION_INTERNAL_H
#define GE_ORIGINAL_DOOR_COLLISION_INTERNAL_H

#include <ultra64.h>
#include <bondtypes.h>

s32 ge_original_door_collision_exact_slice(PropRecord *prop);
void ge_port_door_collision_chr_update_bounds(
    PropRecord *prop, struct rect4f **polygon, s32 *edges,
    f32 *top, f32 *bottom);
void ge_port_door_collision_chr_width_height(
    PropRecord *prop, f32 *radius, f32 *height, f32 *lower_offset);
f32 ge_port_door_collision_chr_ground(PropRecord *prop);
void ge_original_door_chrUpdateCollisionBounds_exact(
    PropRecord *prop, rect4f **polygon, s32 *edges, f32 *top, f32 *bottom);
void ge_original_door_chrGetChrWidthHeight_exact(
    PropRecord *prop, f32 *radius, f32 *height, f32 *lower_offset);
f32 ge_original_door_chrGetChrGround_exact(PropRecord *prop);
u32 ge_port_door_collision_character_flags(PropRecord *prop);
u8 ge_port_door_collision_object_state(ObjectRecord *object);
u8 ge_port_door_collision_object_type(ObjectRecord *object);

#endif
