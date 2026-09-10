#ifndef GE_ORIGINAL_PLAYER_HELD_ITEMS_H
#define GE_ORIGINAL_PLAYER_HELD_ITEMS_H

#include <stdint.h>

/* Canonical lvlRender order for EACH selected simulation player:
 * propsTick -> onscreen count -> autoaim -> held items -> interaction/reload
 * -> propsTickPlayer -> gunUpdateAndFire. Never call again from local draw.
 * Camera matrices, ordered onscreen props and model hit lists must be ready. */
typedef enum GeOriginalPlayerHeldItemsStatus {
    GE_ORIGINAL_PLAYER_HELD_ITEMS_OK = 0,
    GE_ORIGINAL_PLAYER_HELD_ITEMS_NO_PLAYER,
    GE_ORIGINAL_PLAYER_HELD_ITEMS_LIGHT_FRONTIER,
    GE_ORIGINAL_PLAYER_HELD_ITEMS_STAN_TEXTURE_FRONTIER
} GeOriginalPlayerHeldItemsStatus;

/* The native light owner must resolve the hit triangle's Gfx/vertex ABI and
 * perform the original lightFixtureBreak semantics. No empty light tables or
 * replacement light mechanics are supplied here. NULL records a frontier if
 * this branch is actually reached. A frontier requires aborting the match;
 * earlier canonical shot mutations have already occurred and are not undone. */
typedef int (*GeOriginalPlayerLightBreak)(void *context, void *triangle,
                                        uint32_t triangle_type, int32_t room);
void ge_original_player_held_items_bind_lights(
    void *context, GeOriginalPlayerLightBreak light_break);
GeOriginalPlayerHeldItemsStatus ge_original_player_held_items_tick(void);

/* Exact bondviewMovePlayerUpdateViewport gunSetSightVisible call, including
 * multiplayer g_playerPerm->sight instead of the solo options table. */
void ge_original_player_apply_sight_option_exact(void);

#endif
