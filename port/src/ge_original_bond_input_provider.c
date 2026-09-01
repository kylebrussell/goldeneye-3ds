#include "ge_original_bond_input_provider.h"

#include <stdint.h>
#include <string.h>

static GeOriginalBondInputProvider g_provider;

/* Canonical single-player globals used directly by unchanged gameplay and AI
 * bodies.  The camera extraction intentionally owns differently named compact
 * storage; this weak definition remains overrideable by focused host fixtures.
 * g_playerPointers is strongly owned by the retained movement-state slice on
 * ARM.  Keep weak provider-owned storage for focused host links which do not
 * carry that slice; a canonical strong owner or fixture overrides it. */
struct player *g_CurrentPlayer __attribute__((weak));
struct player *g_playerPointers[4] __attribute__((weak));

static void ge_original_bond_input_publish_canonical_player(
    struct player *player)
{
    g_CurrentPlayer = player;
    if ((uintptr_t)(void *)g_playerPointers != 0U) {
        g_playerPointers[0] = player;
        g_playerPointers[1] = NULL;
        g_playerPointers[2] = NULL;
        g_playerPointers[3] = NULL;
    }
}

void ge_original_bond_input_provider_reset_normal_dam(void)
{
    struct player *current_player = g_provider.current_player;
    struct player_data *player_permissions = g_provider.player_permissions;

    memset(&g_provider, 0, sizeof(g_provider));
    g_provider.current_player = current_player;
    g_provider.player_pointers[0] = current_player;
    g_provider.player_permissions = player_permissions;
    g_provider.player_count = 1;
    g_provider.player_actions_enabled = 1;
    g_provider.control_type = 0; /* CONTROLLER_CONFIG_HONEY (1.1) */
    g_provider.fov_y = 60.0f;
    g_provider.speedgraph_frames = 1;
    ge_original_bond_input_publish_canonical_player(current_player);
}

GeOriginalBondInputProvider *ge_original_bond_input_provider(void)
{
    return &g_provider;
}

void ge_original_bond_input_bind_player(
    struct player *player, struct player_data *permissions)
{
    g_provider.current_player = player;
    g_provider.player_permissions = permissions;
    g_provider.player_pointers[0] = player;
    ge_original_bond_input_publish_canonical_player(player);
}

void ge_original_bond_input_set_fov_y(float fov_y)
{
    g_provider.fov_y = fov_y;
}
