#ifndef GE_ORIGINAL_BOND_MOVEMENT_INTERNAL_H
#define GE_ORIGINAL_BOND_MOVEMENT_INTERNAL_H

#include <math.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

/* AIPARSE suppresses the generated typedef retained by bondview.h. */
typedef int PLAYERFLAG;
#include "game/bondview.h"
#include "game/stan.h"
#include "game/stanintersection.h"
#include "ge_original_player_spawn_internal.h"

#define FULL_CROUCH_OFFSET (-100.0f)
#define DegToRad1Fact(DEG) \
    ((float)((DEG) * (float)(6.28318530717958647692 / 360.0)))
#define g_CurrentPlayer (ge_original_spawn_player_get())

extern s32 g_ClockTimer;
extern f32 g_GlobalTimerDelta;
extern StandTile *stanSavedColl_tile;

int ge_port_bond_movement_cdtypes(void);
void ge_port_bond_movement_collision_dimensions(float *radius,
                                                float *height,
                                                float *height_end);
void ge_port_bond_movement_set_prop_collision(void *prop, int enabled);
void ge_port_bond_movement_publish(struct player *player);
void ge_port_bond_movement_consume_head_root(void);
void ge_port_bond_movement_record_canonical_collision(
    float prior_x, float prior_z);

s32 bondviewTryMoveToStan(struct coord3d *position, StandTile **stan);
s32 bondviewTrySimpleMovePlayerCollision(struct coord3d *next_position,
                                         struct coord3d *collision_point_0,
                                         struct coord3d *collision_point_1);
s32 bondviewTryFractionMovePlayerCollision(
    struct coord3d *next_position,
    struct coord3d *collision_1_point_0,
    struct coord3d *collision_1_point_1,
    struct coord3d *collision_2_point_0,
    struct coord3d *collision_2_point_1);
s32 bondviewTryEdgeMovePlayerCollision(struct coord3d *next_position,
                                       struct coord3d *collision_point_0,
                                       struct coord3d *collision_point_1);
s32 bondviewTryEndHopPlayerCollision(struct coord3d *next_position,
                                     struct coord3d *collision_point_0,
                                     struct coord3d *collision_point_1);
void bondviewCalcUpdatePlayerCollision(struct coord3d *offset,
                                       s32 allow_scoot);
void bondviewApplyVertaTheta(void);
void bheadUpdatePos(struct coord3d *velocity);

#endif
