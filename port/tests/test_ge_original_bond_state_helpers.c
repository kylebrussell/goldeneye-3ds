#include "ge_original_bond_input_provider.h"

#include <assert.h>
#include <string.h>

typedef int PLAYERFLAG;
#include "game/bondview.h"
#include "game/player.h"

void currentPlayerSetXAutoAimEnabled(bool enabled);
bool currentPlayerGetXAutoAimEnabledRedirect(void);
void currentPlayerSetYAutoAimEnabled(bool enabled);
bool currentPlayerGetYAutoAimEnabledRedirect(void);

int main(void)
{
    struct player player;
    struct player_data permissions;
    GeOriginalBondInputProvider *provider;

    memset(&player, 0, sizeof(player));
    memset(&permissions, 0, sizeof(permissions));
    provider = ge_original_bond_input_provider();
    provider->current_player = &player;
    provider->player_permissions = &permissions;
    ge_original_bond_input_provider_reset_normal_dam();

    assert(currentPlayerGetXAutoAimEnabledRedirect() == FALSE);
    assert(currentPlayerGetYAutoAimEnabledRedirect() == FALSE);
    currentPlayerSetXAutoAimEnabled(TRUE);
    currentPlayerSetYAutoAimEnabled(TRUE);
    assert(currentPlayerGetXAutoAimEnabledRedirect() == TRUE);
    assert(currentPlayerGetYAutoAimEnabledRedirect() == TRUE);

    provider->player_count = 2;
    permissions.autoaim = FALSE;
    assert(currentPlayerGetXAutoAimEnabledRedirect() == FALSE);
    assert(currentPlayerGetYAutoAimEnabledRedirect() == FALSE);
    permissions.autoaim = TRUE;
    assert(currentPlayerGetXAutoAimEnabledRedirect() == TRUE);
    assert(currentPlayerGetYAutoAimEnabledRedirect() == TRUE);
    return 0;
}
