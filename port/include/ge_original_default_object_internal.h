#ifndef GE_ORIGINAL_DEFAULT_OBJECT_INTERNAL_H
#define GE_ORIGINAL_DEFAULT_OBJECT_INTERNAL_H

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

#include "game/matrixmath.h"
#include "ge_original_default_object.h"
#include "ge_original_dam_world.h"

extern stagesetup g_CurrentSetup;

#define modelLoad ge_port_default_object_model_load
#define getPlayerCount ge_port_default_object_player_count
#define get_scenario ge_port_default_object_scenario

s32 ge_port_default_object_model_load(s32 model_id);
s32 ge_port_default_object_player_count(void);
s32 ge_port_default_object_scenario(void);
void ge_port_default_object_publish(ObjectRecord *object, s32 model_id,
                                    f32 extra_scale,
                                    const coord3d *position,
                                    const coord3d *shade_position,
                                    const Mtxf *matrix, StandTile *stan);
u16 ge_port_default_object_extrascale(ObjectRecord *object);
u8 ge_port_default_object_state(ObjectRecord *object);
u8 ge_port_default_object_type(ObjectRecord *object);
void ge_port_default_object_set_state(ObjectRecord *object, u8 state);

s32 ge_original_domakedefaultobj_standard_prefix_slice(
    s32 stage_id, ObjectRecord *object, s32 command_index);
s32 ge_original_getposstan_zero_radius_slice(
    struct coord3d *position, StandTile *stan,
    struct coord3d *position_return, StandTile **stan_return);
PropRecord *ge_original_objInitPreallocatedSlice(
    ObjectRecord *object, ModelFileHeader *model_header, PropRecord *prop,
    Model *model, f32 pitem_scale, void *collision_data);
s32 ge_original_move_to_pad_slice(
    ObjectRecord *object, struct coord3d *position, Mtxf *matrix,
    StandTile *stan, struct coord3d *pad_position);
s32 ge_original_bound_pad_scale_slice(ObjectRecord *object, Mtxf *matrix);
s32 ge_original_move_onscreen_to_pad_slice(
    ObjectRecord *object, coord3d *position, Mtxf *matrix,
    StandTile *stan, coord3d *pad_position);
ModelRoData_BoundingBoxRecord *chrobjGetBboxFromObjFile(
    ModelFileHeader *object);
ModelRoData_BoundingBoxRecord *chrobjGetBboxFromObjectRecord(
    ObjectRecord *object);
void chrobjCollisionRelated(ObjectRecord *object);
f32 chrpropSumMatrixPosY(ModelRoData_BoundingBoxRecord *bbox, Mtxf *matrix);
f32 chrpropSumMatrixNegY(ModelRoData_BoundingBoxRecord *bbox, Mtxf *matrix);
f32 chrpropSumMatrixPosX(ModelRoData_BoundingBoxRecord *bbox, Mtxf *matrix);
f32 chrpropSumMatrixNegX(ModelRoData_BoundingBoxRecord *bbox, Mtxf *matrix);
f32 chrpropSumMatrixPosZ(ModelRoData_BoundingBoxRecord *bbox, Mtxf *matrix);
f32 chrpropSumMatrixNegZ(ModelRoData_BoundingBoxRecord *bbox, Mtxf *matrix);
void chrpropDeregisterRooms(PropRecord *prop);
void chrpropRegisterRooms(PropRecord *prop);
void chrpropUpdateRoomList(PropRecord *prop, coord3d *bbmin,
                           coord3d *bbmax, f32 radius);
void sub_GAME_7F03F540(ModelRoData_BoundingBoxRecord *bbox, Mtxf *matrix,
                       rect4f *polygon, struct collision_data *collision);
void ge_original_object_collision_bounds_slice(
    PropRecord *prop, coord2d **polygon, s32 *edges,
    f32 *top, f32 *bottom);

s32 ge_port_default_object_floor_y(StandTile *stan, f32 x, f32 z,
                                   f32 *floor_y);
s32 ge_port_default_object_room_bounds(const coord3d *position, s32 room,
                                       f32 *top, f32 *bottom);
s32 ge_port_default_object_walk(StandTile **stan, f32 start_x, f32 start_z,
                                f32 destination_x, f32 destination_z);
s32 ge_port_default_object_tile_rgb(StandTile *stan, f32 x, f32 z,
                                    u8 rgb[3]);

/* Exact objChangeShading state updates, with the authored tile-colour lookup
 * kept as the existing typed world-provider seam. */
s32 ge_original_obj_change_shading_slice(
    ObjectRecord *obj, coord3d *pos, Mtxf *matrix, StandTile *stan);
void ge_original_setup_update_object_room_position_slice(ObjectRecord *obj);
s32 ge_port_default_object_collision(ObjectRecord *object);
void ge_port_default_object_publish_placement(const coord3d *position,
                                              StandTile *stan, u32 stage);

#endif
