#include "ge_original_covert_modem_fire.h"

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/bondview.h"
#include "game/gun.h"
#include "game/matrixmath.h"

#include "ge_original_bond_input_internal.h"
#include "ge_original_covert_modem_object.h"
#include "ge_original_covert_modem_projectile.h"
#include "ge_original_player_thrown_object.h"

#include <math.h>
#include <string.h>

static GeOriginalCovertModemFireStats ge_fire_stats;

static ITEM_IDS ge_item_in_hand(GUNHAND hand)
{
    if (g_CurrentPlayer->hands[hand].weaponnum_watchmenu >= 0)
        return g_CurrentPlayer->hands[hand].weaponnum_watchmenu;
    return g_CurrentPlayer->hands[hand].weaponnum;
}

static s32 ge_matrix_is_finite_nonzero(const Mtxf *matrix)
{
    s32 row;
    s32 column;
    f32 magnitude = 0.0f;

    if (matrix == NULL) return FALSE;
    for (row = 0; row < 4; row++)
    {
        for (column = 0; column < 4; column++)
        {
            if (!isfinite(matrix->m[row][column])) return FALSE;
            magnitude += fabsf(matrix->m[row][column]);
        }
    }
    return magnitude > 0.0f;
}

static GeOriginalCovertModemFireStatus
ge_generate_player_thrown_bug(GUNHAND hand)
{
    struct hand *hand_state = &g_CurrentPlayer->hands[hand];
    PropRecord *player_prop = g_CurrentPlayer->prop;
    GeOriginalCovertModemProjectileStats before;
    GeOriginalCovertModemProjectileStats after;

    if (player_prop == NULL || player_prop->stan == NULL)
        return GE_ORIGINAL_COVERT_MODEM_FIRE_NO_PLAYER;
    if (!ge_matrix_is_finite_nonzero(&hand_state->throw_item_pos_related)
            || !ge_matrix_is_finite_nonzero(g_CurrentPlayer->viewtoworldmtxf)
            || !(g_CurrentPlayer->c_perspaspect > 0.0f)
            || !(viGetFovY() > 0.0f))
    {
        ge_fire_stats.pose_rejections++;
        return GE_ORIGINAL_COVERT_MODEM_FIRE_POSE_UNAVAILABLE;
    }

    ge_original_covert_modem_projectile_snapshot(&before);
    ge_original_generate_player_thrown_object_exact((int)hand);
    ge_original_covert_modem_projectile_snapshot(&after);
    if (after.pool_allocations > before.pool_allocations)
        return GE_ORIGINAL_COVERT_MODEM_FIRE_THROWN;
    if (after.pool_exhaustions > before.pool_exhaustions)
        return GE_ORIGINAL_COVERT_MODEM_FIRE_PROJECTILE_FAILED;
    return GE_ORIGINAL_COVERT_MODEM_FIRE_OBJECT_UNAVAILABLE;
}

static GeOriginalCovertModemFireStatus
ge_gun_update_and_fire(GUNHAND hand)
{
    struct hand *hand_state;
    ITEM_IDS item;

    ge_fire_stats.hand_dispatches++;
    hand_state = &g_CurrentPlayer->hands[hand];
    item = ge_item_in_hand(hand);

    if (hand_state->weapon_firing_status != 0 && item == ITEM_BUG)
    {
        GeOriginalCovertModemFireStatus status;
        ge_fire_stats.throw_attempts++;
        status = ge_generate_player_thrown_bug(hand);
        if (status == GE_ORIGINAL_COVERT_MODEM_FIRE_THROWN)
            ge_fire_stats.successful_throws++;
        return status;
    }

    return GE_ORIGINAL_COVERT_MODEM_FIRE_IDLE;
}

void ge_original_covert_modem_fire_reset(void)
{
    memset(&ge_fire_stats, 0, sizeof(ge_fire_stats));
}

GeOriginalCovertModemFireStatus ge_original_covert_modem_fire_tick(void)
{
    GeOriginalCovertModemFireStatus right;
    GeOriginalCovertModemFireStatus left;

    if (g_CurrentPlayer == NULL)
        return GE_ORIGINAL_COVERT_MODEM_FIRE_NO_PLAYER;
    ge_fire_stats.both_hands_ticks++;
    right = ge_gun_update_and_fire(GUNRIGHT);
    left = ge_gun_update_and_fire(GUNLEFT);
    return right != GE_ORIGINAL_COVERT_MODEM_FIRE_IDLE ? right : left;
}

void ge_original_covert_modem_fire_snapshot(
    GeOriginalCovertModemFireStats *stats)
{
    if (stats != NULL) *stats = ge_fire_stats;
}
