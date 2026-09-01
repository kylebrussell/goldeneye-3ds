#include "ge_original_bond_move_state.h"

#include <ultra64.h>
#include <bondconstants.h>
typedef int PLAYERFLAG;
#include "game/bondview.h"
#include "game/player.h"

#include <assert.h>

extern struct player_data g_playerPlayerData[4];
extern struct player_data *g_playerPerm;
extern s32 g_ControlsLockedFlag;
extern s32 g_bondviewForceDisarm;
extern s32 g_PlayerIsInTank;
extern struct PropRecord *g_PlayerTankProp;
extern enum CAMERAMODE g_CameraMode;
extern struct coord3d g_DefaultMoveBondOffset;
extern vec3d g_ForceBondMoveOffset;
extern s32 stanlinelog_flag;

s32 get_cur_playernum(void);
s32 lvlGetControlsLockedFlag(void);
s32 get_debug_fast_bond_flag(void);
s32 get_debug_man_pos_flag(void);

s32 g_ControlsLockedFlag;

int main(void)
{
    assert(g_playerPerm == 0);
    assert(ge_original_bond_move_state_initialize_single_player());
    assert(g_playerPerm == &g_playerPlayerData[PLAYER_1]);
    assert(get_cur_playernum() == PLAYER_1);
    assert(g_playerPerm->player_perspective_height == 1.0f);
    assert(g_playerPerm->handicap == 1.0f);

    assert(g_bondviewForceDisarm == 0);
    assert(g_PlayerIsInTank == 0);
    assert(g_PlayerTankProp == 0);
    assert(g_CameraMode == CAMERAMODE_NONE);
    assert(g_DefaultMoveBondOffset.f[0] == 0.0f);
    assert(g_DefaultMoveBondOffset.f[1] == 0.0f);
    assert(g_DefaultMoveBondOffset.f[2] == 0.0f);
    assert(g_ForceBondMoveOffset.f[0] == 0.0f);
    assert(stanlinelog_flag == 0);
    assert(get_debug_fast_bond_flag() == 0);
    assert(get_debug_man_pos_flag() == 0);

    g_ControlsLockedFlag = 1;
    assert(lvlGetControlsLockedFlag() == 1);
    g_ControlsLockedFlag = 0;
    assert(lvlGetControlsLockedFlag() == 0);
    return 0;
}
