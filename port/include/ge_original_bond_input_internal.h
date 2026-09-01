#ifndef GE_ORIGINAL_BOND_INPUT_INTERNAL_H
#define GE_ORIGINAL_BOND_INPUT_INTERNAL_H

#include "ge_original_bond_input_provider.h"

#ifndef DegToRad1Fact
#define DegToRad1Fact(DEG) \
    ((float)((DEG) * (float)(6.28318530717958647692 / 360.0)))
#endif

/*
 * Typed substitutions for external reads only. The canonical function body
 * remains in src/game/bondview2.c with its original branch and call order.
 */
#define g_CurrentPlayer \
    (ge_original_bond_input_provider()->current_player)
#define g_playerPerm \
    (ge_original_bond_input_provider()->player_permissions)
#define g_playerPointers \
    (ge_original_bond_input_provider()->player_pointers)
#define g_stopPlayFlag \
    (ge_original_bond_input_provider()->stop_play_flag)
#define g_gameOverFlag \
    (ge_original_bond_input_provider()->game_over_flag)
#define g_bondviewForceDisarm \
    (ge_original_bond_input_provider()->force_disarm)
#define g_PlayerIsInTank \
    (ge_original_bond_input_provider()->player_is_in_tank)
#define g_PlayerTankProp \
    (ge_original_bond_input_provider()->player_tank_prop)
#define g_PlayerTankYOffset \
    (ge_original_bond_input_provider()->player_tank_y_offset)
#define g_BondCanEnterTank \
    (ge_original_bond_input_provider()->bond_can_enter_tank)
#define g_ClockTimer \
    (ge_original_bond_input_provider()->clock_timer)
#define g_GlobalTimer \
    (ge_original_bond_input_provider()->global_timer)
#define g_GlobalTimerDelta \
    (ge_original_bond_input_provider()->global_timer_delta)
#define viGetFovY() \
    (ge_original_bond_input_provider()->fov_y)
#define viSetFovY(FOV) \
    ge_original_bond_input_set_fov_y((FOV))
#define set_cur_player_fovy(FOV) \
    (g_CurrentPlayer->fovy = (FOV))
#define speedgraphframes \
    (ge_original_bond_input_provider()->speedgraph_frames)
#define getPlayerCount() \
    (ge_original_bond_input_provider()->player_count)
#define get_cur_playernum() \
    (ge_original_bond_input_provider()->player_number)
#define lvlGetControlsLockedFlag() \
    (ge_original_bond_input_provider()->controls_locked)
#define disablePlayerActionsWhenPausedOrInMpMenu() \
    (ge_original_bond_input_provider()->player_actions_enabled)
#define cur_player_get_control_type() \
    (ge_original_bond_input_provider()->control_type)
#define cur_player_get_aim_control() \
    (ge_original_bond_input_provider()->aim_control)
#define get_cur_player_look_vertical_inverted() \
    (ge_original_bond_input_provider()->look_vertical_inverted)

/* File-local tank state remains authentic mutable game state. */
extern s32 g_EnterTankAudioState;
extern f32 g_TankEnteringSitHeight;
extern f32 g_TankEnteringSitHeightRemain;
extern f32 g_TankEnterBondHorizAngleDeg;
extern f32 g_TankEnterBondVertAngleDeg;
extern coord3d g_EnterTankCoord;

#endif
