#ifndef GE_ORIGINAL_DOOR_INTERNAL_H
#define GE_ORIGINAL_DOOR_INTERNAL_H

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/matrixmath.h"
#include "ge_original_dam_world.h"
#include "ge_original_default_object_internal.h"
#include "ge_original_door.h"

s32 ge_original_setup_door_slice(ObjectRecord *object, s32 command_index,
    ModelFileHeader *header, Model *model, f32 pitem_scale,
    void *collision_data, const GeOriginalDamDoorSetup *setup);
s32 ge_port_door_walk(StandTile **stan, f32 sx, f32 sz, f32 dx, f32 dz);
s32 ge_port_door_tile_rgb(StandTile *stan, f32 x, f32 z, u8 rgb[3]);
s32 ge_port_door_portal_rooms(const BoundPadRecord *pad, s32 *room_a,
    s32 *room_b, coord3d *point_a, coord3d *point_b);
s32 ge_port_door_find_portal(s32 room_a, s32 room_b,
    const coord3d *point_a, const coord3d *point_b);
void ge_port_door_set_portal_open(s32 portal, s32 open);
void ge_port_door_register_room(PropRecord *prop, s16 room);
void ge_port_door_publish(const GeOriginalDoorPrepared *prepared);

#endif
