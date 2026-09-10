#include "ge_original_bond_input_provider.h"

#include <stdint.h>
#include <string.h>

static GeOriginalBondInputProvider g_provider;
static GeOriginalBondInputProvider g_roster[4];
static uint32_t g_roster_count;
static int32_t g_control_style; /* CONTROLLER_CONFIG_HONEY (1.1) */

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

    ge_original_bond_input_unbind_roster();
    memset(&g_provider, 0, sizeof(g_provider));
    g_provider.current_player = current_player;
    g_provider.player_pointers[0] = current_player;
    g_provider.player_permissions = player_permissions;
    g_provider.player_count = 1;
    g_provider.player_actions_enabled = 1;
    g_provider.control_type = g_control_style;
    g_provider.fov_y = 60.0f;
    g_provider.speedgraph_frames = 1;
    ge_original_bond_input_publish_canonical_player(current_player);
}

GeOriginalBondInputProvider *ge_original_bond_input_provider(void)
{
    uint32_t slot;
    for (slot = 0; slot < g_roster_count; ++slot)
        if (g_roster[slot].current_player == g_CurrentPlayer)
            return &g_roster[slot];
    return &g_provider;
}

void ge_original_bond_input_set_control_style(int32_t control_type)
{
    g_control_style = control_type;
    ge_original_bond_input_provider()->control_type = control_type;
}

int32_t ge_original_bond_input_control_style(void)
{
    return g_control_style;
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
    ge_original_bond_input_provider()->fov_y = fov_y;
}

int ge_original_bond_input_bind_roster(struct player *const *players,
    struct player_data *const *permissions, uint32_t count)
{
    uint32_t i, j;
    struct player *next_players[4];
    struct player_data *next_permissions[4];
    if (players == NULL || permissions == NULL || count < 2 || count > 4)
        return 0;
    for (i = 0; i < count; ++i) {
        if (players[i] == NULL || permissions[i] == NULL) return 0;
        for (j = 0; j < i; ++j)
            if (players[i] == players[j] || permissions[i] == permissions[j])
                return 0;
    }
    memcpy(next_players, players, count * sizeof(*players));
    memcpy(next_permissions, permissions, count * sizeof(*permissions));
    players = next_players;
    permissions = next_permissions;
    memset(g_roster, 0, sizeof(g_roster));
    for (i = 0; i < count; ++i) {
        g_roster[i].current_player = players[i];
        g_roster[i].player_permissions = permissions[i];
        g_roster[i].player_number = (int32_t)i;
        g_roster[i].player_count = (int32_t)count;
        g_roster[i].player_actions_enabled = 1;
        g_roster[i].control_type = g_control_style;
        g_roster[i].fov_y = 60.0f;
        g_roster[i].speedgraph_frames = 1;
        memcpy(g_roster[i].player_pointers, players, count * sizeof(*players));
    }
    for (i = 0; i < 4; ++i) g_playerPointers[i] = i < count ? players[i] : NULL;
    g_CurrentPlayer = players[0];
    g_roster_count = count;
    return 1;
}

void ge_original_bond_input_unbind_roster(void)
{
    g_roster_count = 0;
    memset(g_roster, 0, sizeof(g_roster));
}

void ge_original_bond_input_set_timers(int32_t clock_timer,
    int32_t global_timer, float global_timer_delta)
{
    uint32_t i;
    g_provider.clock_timer = clock_timer;
    g_provider.global_timer = global_timer;
    g_provider.global_timer_delta = global_timer_delta;
    for (i = 0; i < g_roster_count; ++i) {
        g_roster[i].clock_timer = clock_timer;
        g_roster[i].global_timer = global_timer;
        g_roster[i].global_timer_delta = global_timer_delta;
    }
}
