#include "ge_original_bond_move_state.h"

#include <ultra64.h>
#include <bondconstants.h>
typedef int PLAYERFLAG;
#include "game/player.h"

extern struct player_data g_playerPlayerData[4];
extern struct player_data *g_playerPerm;
extern s32 player_num;
void default_player_perspective_and_height(void);

int ge_original_bond_move_state_initialize_single_player(void)
{
    player_num = PLAYER_1;
    g_playerPerm = &g_playerPlayerData[PLAYER_1];
    default_player_perspective_and_height();
    return g_playerPerm != 0;
}
