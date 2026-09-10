#include "ge_original_multiplayer_players.h"
#include "ge_original_bond_input_provider.h"
#include "ge_original_player_spawn_internal.h"
#include "ge_original_input.h"
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/bondinv.h"
#include "game/chrObjRandom.h"
#include <random.h>

extern struct player_data g_playerPlayerData[4];
extern struct player_data *g_playerPerm;
extern s32 player_num;
extern s32 random_byte;
void default_player_perspective_and_height(void);
void init_player_BONDdata(void);

typedef struct GeMultiplayerStorage {
    struct player players[4];
    InvItem inventory[4][30];
    struct player *previous_players[4];
    struct player_data previous_data[4];
    PLAYER_ID previous_shuffle[4];
    struct player *previous_current;
    struct player_data *previous_perm;
    s32 previous_number;
    s32 previous_random_byte;
    u64 previous_rng;
    u64 previous_chr_rng;
    int32_t previous_control_style;
    int32_t control_styles[4];
    uint8_t controls_explicit;
    uint32_t count;
    uint32_t local;
    uint64_t lifetime;
} GeMultiplayerStorage;
static GeMultiplayerStorage *g_players;
static uint64_t g_players_lifetime;

struct player *ge_original_multiplayer_player_storage(s32 slot)
{
    return g_players != NULL && slot >= 0 && (uint32_t)slot < g_players->count
        ? &g_players->players[slot] : NULL;
}

int ge_original_multiplayer_players_create(uint32_t count, uint32_t local_slot,
                                          uint64_t seed, uint64_t chr_seed)
{
    struct player *players[4];
    struct player_data *permissions[4];
    uint32_t i;
    GeMultiplayerStorage *storage;
    if (g_players != NULL || count < 2 || count > 4 || local_slot >= count
            || g_players_lifetime == UINT64_MAX)
        return 0;
    storage = calloc(1, sizeof(*storage));
    if (storage == NULL) return 0;
    storage->count = count;
    storage->local = local_slot;
    storage->lifetime = ++g_players_lifetime;
    memcpy(storage->previous_players, g_playerPointers, sizeof(g_playerPointers));
    memcpy(storage->previous_data, g_playerPlayerData, sizeof(storage->previous_data));
    memcpy(storage->previous_shuffle, array_PLAYER_IDs, sizeof(storage->previous_shuffle));
    storage->previous_current = g_CurrentPlayer;
    storage->previous_perm = g_playerPerm;
    storage->previous_number = player_num;
    storage->previous_random_byte = random_byte;
    storage->previous_rng = g_randomSeed;
    storage->previous_chr_rng = g_chrObjRandomSeed;
    storage->previous_control_style = ge_original_bond_input_control_style();
    g_players = storage;
    /* boss.c resets these native stage fields before constructing viewports.
     * In particular, the prior stage shuffle must not survive construction. */
    reset_play_data_ptrs();
    memset(g_playerPlayerData, 0, sizeof(storage->previous_data));
    default_player_perspective_and_height();
    /* Inputs are complete stream states, not the original 32-bit setter
     * arguments (which add one and would truncate the agreed high bits). */
    g_randomSeed = seed;
    g_chrObjRandomSeed = chr_seed;
    random_byte = (s32)(randomGetNext() & 0xff);
    for (i = 0; i < count; ++i) {
        initBONDdataforPlayer((s32)i);
        players[i] = &storage->players[i];
        permissions[i] = &g_playerPlayerData[i];
    }
    if (!ge_original_bond_input_bind_roster(players, permissions, count)
            || !ge_original_input_configure_players(count)) {
        ge_original_multiplayer_players_destroy();
        return 0;
    }
    ge_original_spawn_player_bind_current(&g_CurrentPlayer);
    for (i = 0; i < count; ++i) {
        ge_original_multiplayer_select_player(i);
        init_player_BONDdata();
    }
    return ge_original_multiplayer_select_local_player();
}

void ge_original_multiplayer_players_destroy(void)
{
    GeMultiplayerStorage *storage = g_players;
    if (storage == NULL) return;
    ge_original_spawn_player_bind_current(NULL);
    ge_original_bond_input_unbind_roster();
    memcpy(g_playerPointers, storage->previous_players, sizeof(storage->previous_players));
    memcpy(g_playerPlayerData, storage->previous_data, sizeof(storage->previous_data));
    memcpy(array_PLAYER_IDs, storage->previous_shuffle, sizeof(storage->previous_shuffle));
    g_CurrentPlayer = storage->previous_current;
    g_playerPerm = storage->previous_perm;
    player_num = storage->previous_number;
    random_byte = storage->previous_random_byte;
    g_randomSeed = storage->previous_rng;
    g_chrObjRandomSeed = storage->previous_chr_rng;
    ge_original_bond_input_set_control_style(storage->previous_control_style);
    g_players = NULL;
    free(storage);
    ge_original_input_init();
}

int ge_original_multiplayer_select_player(uint32_t slot)
{
    if (g_players == NULL || slot >= g_players->count) return 0;
    set_cur_player((s32)slot);
    return ge_original_input_select_player(slot);
}
int ge_original_multiplayer_select_local_player(void)
{
    return g_players != NULL && ge_original_multiplayer_select_player(g_players->local);
}
uint32_t ge_original_multiplayer_player_count(void)
{
    return g_players != NULL ? g_players->count : 0;
}
struct player *ge_original_multiplayer_player(uint32_t slot)
{
    return g_players != NULL && slot < g_players->count ? &g_players->players[slot] : NULL;
}
int32_t ge_original_multiplayer_control_style(int32_t slot)
{
    if (slot < 0 || g_players == NULL || (uint32_t)slot >= g_players->count) return 0;
    if(g_players->controls_explicit)return g_players->control_styles[slot];
    /* Every local machine supplies both original controller roles. */
    return ge_original_input_dual_analog() ? CONTROLLER_CONFIG_GALORE : CONTROLLER_CONFIG_HONEY;
}
int ge_original_multiplayer_players_apply_control_styles(const int32_t *styles,uint32_t count)
{
    if(!g_players || count!=g_players->count || !styles)return 0;
    for(uint32_t i=0;i<count;++i)
        if(g_playerPointers[i]!=&g_players->players[i])return 0;
    if(!ge_original_bond_input_set_roster_control_styles(styles,count))return 0;
    for(uint32_t i=0;i<count;++i) {
        g_players->control_styles[i]=styles[i];
        g_players->players[i].controldef=styles[i];
    }
    g_players->controls_explicit=1;
    return 1;
}
int ge_original_multiplayer_players_control_storage(GeOriginalMultiplayerControlStorage *out)
{
    if(!g_players || !out)return 0;
    for(unsigned i=0;i<4;++i)out->styles[i]=g_players->control_styles[i];
    out->count=g_players->count;out->explicit_selection=g_players->controls_explicit;
    return 1;
}
int ge_original_multiplayer_player_bind_inventory(uint32_t slot)
{
    if (g_players == NULL || slot >= g_players->count) return 0;
    memset(g_players->inventory[slot], 0, sizeof(g_players->inventory[slot]));
    g_players->players[slot].p_itemcur = g_players->inventory[slot];
    g_players->players[slot].equipmaxitems = 30;
    return 1;
}

int ge_original_multiplayer_inventory_storage(uint32_t slot,
    GeOriginalMultiplayerInventoryStorage *storage)
{
    if (g_players == NULL || slot >= 4 || storage == NULL) return 0;
    storage->items = g_players->inventory[slot];
    storage->player = &g_players->players[slot];
    storage->lifetime = g_players->lifetime;
    storage->capacity = 30;
    storage->active_player = slot < g_players->count;
    return 1;
}

int ge_original_multiplayer_player_model_storage(uint32_t slot,
    const void **model, size_t *bytes)
{
    /* The decompiled player spells this native embedded Model as one pointer
     * plus overlaid scalar fields. Gait uses this same address on both ABIs. */
    _Static_assert(offsetof(struct player, field_6CC) - offsetof(struct player, model)
        >= sizeof(Model), "player storage must contain the native full Model");
    if (!g_players || slot >= 4 || !model || !bytes) return 0;
    *model = &g_players->players[slot].model;
    *bytes = sizeof(Model);
    return 1;
}
