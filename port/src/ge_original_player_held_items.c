#include "ge_original_player_held_items.h"

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#ifndef PLAYERFLAG
typedef int PLAYERFLAG;
#endif
#include "ge_original_bg_geometry_internal.h"
#include "ge_original_bond_input_internal.h"

static void *ge_held_light_context;
static GeOriginalPlayerLightBreak ge_held_light_break;
static GeOriginalPlayerHeldItemsStatus ge_held_status;

extern void chraiCheckUseHeldItems(void);

void ge_original_player_held_items_bind_lights(
    void *context, GeOriginalPlayerLightBreak light_break)
{
    ge_held_light_context = context;
    ge_held_light_break = light_break;
}

void ge_original_player_held_items_light_break_exact(
    Gfx *triangle, u32 type, s32 room)
{
    if (ge_held_light_break == NULL
            || !ge_held_light_break(ge_held_light_context, triangle, type, room))
        ge_held_status = GE_ORIGINAL_PLAYER_HELD_ITEMS_LIGHT_FRONTIER;
}

void ge_original_player_held_items_stan_texture_frontier(void)
{
    ge_held_status = GE_ORIGINAL_PLAYER_HELD_ITEMS_STAN_TEXTURE_FRONTIER;
}

GeOriginalPlayerHeldItemsStatus ge_original_player_held_items_tick(void)
{
    if (g_CurrentPlayer == NULL || g_playerPerm == NULL)
        return GE_ORIGINAL_PLAYER_HELD_ITEMS_NO_PLAYER;
    if (ge_original_bg_geometry_native_rooms() == NULL
            || ge_original_bg_geometry_room_count() == 0U)
        return GE_ORIGINAL_PLAYER_HELD_ITEMS_BG_FRONTIER;
    ge_held_status = GE_ORIGINAL_PLAYER_HELD_ITEMS_OK;
    chraiCheckUseHeldItems();
    return ge_held_status;
}
