#ifndef GE_ORIGINAL_BOND_CAMERA_INTERNAL_H
#define GE_ORIGINAL_BOND_CAMERA_INTERNAL_H

#include <ultra64.h>
#include <PR/gu.h>

typedef union Mtxf {
    f32 m[4][4];
    s32 unused;
} Mtxf;

typedef struct coord3d {
    union {
        struct {
            f32 x;
            f32 y;
            f32 z;
        };
        f32 f[3];
    };
} coord3d;

typedef struct StandTile StandTile;

/* Exact semantic layout consumed by change_player_pos_to_target.  Pointer
 * width follows the target, so the ARM build retains the original 0x54-byte
 * collision record while host tests remain pointer-safe. */
struct collision434 {
    StandTile *current_tile_ptr;
    coord3d collision_position;
    coord3d theta_transform;
    coord3d pos3;
    f32 collision_radius;
    coord3d pos;
    coord3d applied_view;
    coord3d applied_view2;
    StandTile *current_tile_ptr_for_portals;
};

/* Host-safe semantic subset of struct player used by the isolated original
 * camera function. Pointer fields remain pointers on 64-bit test hosts. */
typedef struct GePortBondCameraPlayer {
    coord3d current_model_pos;
    coord3d previous_model_pos;
    coord3d current_room_pos;
    Mtx *field_5C;
    Mtx *field_60;
    Mtx *field_64;
    Mtx *field_68;
    Mtx *field_10C4;
    Mtx *field_10C8;
    Mtxf *field_10CC;
    Mtxf *field_10E8;
    Mtxf *projmatrixf;
    Mtxf *viewtoworldmtxf;
    Mtxf *field_10EC;
    s32 field_10E0;
    s32 field_10E4;
} GePortBondCameraPlayer;

/* Isolated camera-slice storage.  This must not export the canonical
 * `struct player *g_CurrentPlayer` symbol consumed by gameplay/AI code: the
 * compact camera harness has a deliberately different ABI. */
extern GePortBondCameraPlayer *ge_original_bond_camera_player;
#if defined(GE_PORT_BOND_CAMERA_SLICE)
#define g_CurrentPlayer ge_original_bond_camera_player
#endif
extern f32 D_800364CC;

u8 bondviewGetCurrentPlayersRoom(void);
void getRoomPositionScaledByIndex(s32 room, coord3d *position);
f32 get_room_data_float1(void);
void setPlayerRoomField(s32 room);
Mtx *dynAllocateMatrix(void);
LookAt *dynAllocateLights(s32 count);
Mtxf *currentPlayerGetProjectionMatrixF(void);
void set_BONDdata_field_10E0(s32 value);
f32 bgGetLevelVisibilityScale(void);
void currentPlayerSetMatrix10C8(Mtx *matrix);
void currentPlayerSetMatrix10C4(Mtx *matrix);
void *currentPlayerSetMatrix10CC(Mtxf *matrix);
void currentPlayerSetViewToWorldMtxf(Mtxf *matrix);
void sub_GAME_7F078464(s32 value);
void bondviewUpdateFrustumPlanes(void);
Mtxf *camGetWorldToScreenMtxf(void);

void mtx4RotateVecInPlace(Mtxf *matrix, coord3d *vector);
void matrix_4x4_set_lookat(Mtxf *matrix,
    f32 eye_x, f32 eye_y, f32 eye_z,
    f32 forward_x, f32 forward_y, f32 forward_z,
    f32 up_x, f32 up_y, f32 up_z);
void matrix_4x4_set_basis_and_position(Mtxf *matrix,
    f32 position_x, f32 position_y, f32 position_z,
    f32 basis_x, f32 basis_y, f32 basis_z,
    f32 up_x, f32 up_y, f32 up_z);
void matrix_4x4_multiply(Mtxf *left, Mtxf *right, Mtxf *result);
void matrix_scalar_multiply(f32 scalar, f32 *matrix);
/* The original call site passes its fixed matrices through s32 pointers. */
void sub_GAME_7F059334(s32 *source, s32 *destination);

void bondviewUpdateCameraMatrices(coord3d *camera_position,
    coord3d *camera_look_direction, coord3d *camera_up);

void change_player_pos_to_target(struct collision434 *collision,
    coord3d *position, StandTile *stan);

#endif
