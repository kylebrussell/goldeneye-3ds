#include "ge_original_inventory.h"
#include "ge_original_bond_input_provider.h"
#include "ge_original_language_text.h"

#include <ultra64.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/bondinv.h"
#include "game/gun.h"
#include <string.h>

extern struct player *g_CurrentPlayer;
extern GunModelFileRecord gitem_structs[];
extern s32 g_bondviewForceDisarm;
extern s32 lvlGetControlsLockedFlag(void);
extern s32 bondinvGetTextbyInvIndex(s32 index);

static uint64_t inventory_epoch = 1;

void ge_original_inventory_invalidate(void)
{
    if (++inventory_epoch == 0) inventory_epoch = 1;
}

static int inventory_player_available(void)
{
    const GeOriginalBondInputProvider *provider =
        ge_original_bond_input_provider();
    return g_CurrentPlayer != NULL
        && provider->current_player == g_CurrentPlayer
        && provider->player_count == 1 && provider->player_number == 0
        && provider->player_pointers[0] == g_CurrentPlayer
        && g_CurrentPlayer->p_itemcur != NULL
        && g_CurrentPlayer->equipmaxitems > 0
        && g_CurrentPlayer->equipmaxitems <= 1024;
}

static int inventory_selection_allowed(void)
{
    const GeOriginalBondInputProvider *provider =
        ge_original_bond_input_provider();
    int hand;
    if (!inventory_player_available()
            || provider->controls_locked || lvlGetControlsLockedFlag()
            || !provider->player_actions_enabled || provider->force_disarm > 0
            || g_bondviewForceDisarm > 0 || provider->stop_play_flag
            || provider->game_over_flag || provider->player_is_in_tank
            || g_CurrentPlayer->bonddead || g_CurrentPlayer->cameramode != 0
            || g_CurrentPlayer->watch_animation_state != WATCH_ANIMATION_0x0
            || !g_CurrentPlayer->outside_watch_menu
            || g_CurrentPlayer->open_close_solo_watch_menu)
        return 0;
    for (hand = 0; hand < 2; ++hand) {
        if (g_CurrentPlayer->hands[hand].weaponnum < ITEM_UNARMED
                || g_CurrentPlayer->hands[hand].weaponnum > ITEM_JOYPAD
                || g_CurrentPlayer->hands[hand].weaponnum_watchmenu >= 0
                || g_CurrentPlayer->hands[hand].weapon_action_state != GUN_ANIM_STATE_IDLE
                || g_CurrentPlayer->hands[hand].weapon_current_animation != 0)
            return 0;
    }
    return 1;
}

/* Canonical inventory walkers trust their ring. Verify its bounded storage
 * and closure before invoking them so a partial teardown fails closed. */
static int inventory_ring_valid(void)
{
    const InvItem *first = g_CurrentPlayer->ptr_inventory_first_in_cycle;
    const InvItem *item = first;
    uintptr_t start = (uintptr_t)g_CurrentPlayer->p_itemcur;
    size_t bytes = (size_t)g_CurrentPlayer->equipmaxitems * sizeof(InvItem);
    int visited = 0;
    if (first == NULL) return 1;
    do {
        uintptr_t address = (uintptr_t)item;
        if (address < start || address - start >= bytes
                || (address - start) % sizeof(InvItem) != 0
                || ++visited > g_CurrentPlayer->equipmaxitems)
            return 0;
        if (item->type == INV_ITEM_PROP) {
            if (item->type_inv_item.type_prop.prop == NULL
                    || item->type_inv_item.type_prop.prop->obj == NULL)
                return 0;
        } else if (item->type == INV_ITEM_WEAPON) {
            if (item->type_inv_item.type_weap.weapon < ITEM_UNARMED
                    || item->type_inv_item.type_weap.weapon > ITEM_JOYPAD)
                return 0;
        } else if (item->type != INV_ITEM_DUAL) {
            return 0;
        }
        item = item->next;
    } while (item != first);
    return 1;
}

static void inventory_entry(int32_t index, GeOriginalInventoryEntry *entry)
{
    InvItem *item = bondinvGetItemByIndex(index);
    textoverride *override = NULL;
    const char *name;
    int32_t weapon = bondinvGetTextbyInvIndex(index);
    memset(entry, 0, sizeof(*entry));
    entry->index = index;
    entry->weapon = weapon;
    entry->item_identity = (uintptr_t)item;
    if (item != NULL && item->type == INV_ITEM_PROP) {
        PropRecord *prop = item->type_inv_item.type_prop.prop;
        entry->prop_identity = (uintptr_t)prop;
        override = bondinvGetTextbyObj(prop->obj);
    } else if (item != NULL && item->type == INV_ITEM_WEAPON) {
        override = bondinvGetTextbyWeaponID(weapon);
    }
    /* Same name precedence as bondinvGetNameByIndex: an owned prop's text
     * override, an owned weapon's override, then its LGUN watch name. Virtual
     * all-guns rows deliberately bypass per-weapon overrides, as the watch. */
    if (override != NULL && override->shorttext != 0)
        entry->text_id = (uint16_t)override->shorttext;
    else if (weapon >= ITEM_UNARMED && weapon <= ITEM_JOYPAD)
        entry->text_id = gitem_structs[weapon].watch_equipment_text;
    name = entry->text_id ? ge_original_language_text(entry->text_id) : NULL;
    if (name == NULL || name[0] == '\0') name = "Item";
    {
        size_t length = 0;
        while (length + 1 < sizeof(entry->name) && name[length] != '\0'
                && name[length] != '\n' && name[length] != '\r') {
            entry->name[length] = name[length];
            ++length;
        }
        entry->name[length] = '\0';
    }
    entry->selectable = weapon >= ITEM_UNARMED && weapon <= ITEM_JOYPAD
        && (weapon != ITEM_UNARMED
            || (item != NULL && item->type == INV_ITEM_WEAPON));
    entry->selected = entry->selectable
        && g_CurrentPlayer->hands[GUNRIGHT].weaponnum == weapon
        && g_CurrentPlayer->hands[GUNLEFT].weaponnum == ITEM_UNARMED;
}

GeOriginalInventoryResult ge_original_inventory_snapshot(
    GeOriginalInventorySnapshot *snapshot)
{
    int32_t count;
    size_t i;
    if (snapshot == NULL) return GE_ORIGINAL_INVENTORY_INVALID_ARGUMENT;
    memset(snapshot, 0, sizeof(*snapshot));
    if (!inventory_player_available() || !inventory_ring_valid())
        return GE_ORIGINAL_INVENTORY_UNAVAILABLE;
    count = bondinvCountTotalItemsInInv();
    if (count < 0) return GE_ORIGINAL_INVENTORY_UNAVAILABLE;
    snapshot->total_count = (size_t)count;
    snapshot->count = snapshot->total_count < GE_ORIGINAL_INVENTORY_CAPACITY
        ? snapshot->total_count : GE_ORIGINAL_INVENTORY_CAPACITY;
    snapshot->selection_allowed = inventory_selection_allowed();
    snapshot->player_identity = (uintptr_t)g_CurrentPlayer;
    snapshot->storage_identity = (uintptr_t)g_CurrentPlayer->p_itemcur;
    snapshot->epoch = inventory_epoch;
    for (i = 0; i < snapshot->count; ++i)
        inventory_entry((int32_t)i, &snapshot->entries[i]);
    return GE_ORIGINAL_INVENTORY_OK;
}

GeOriginalInventoryResult ge_original_inventory_select(
    const GeOriginalInventorySnapshot *snapshot, size_t row)
{
    GeOriginalInventoryEntry current;
    const GeOriginalInventoryEntry *captured;
    if (snapshot == NULL || row >= snapshot->count
            || row >= GE_ORIGINAL_INVENTORY_CAPACITY)
        return GE_ORIGINAL_INVENTORY_INVALID_ARGUMENT;
    if (!inventory_player_available() || !inventory_ring_valid())
        return GE_ORIGINAL_INVENTORY_UNAVAILABLE;
    if (snapshot->epoch != inventory_epoch
            || snapshot->player_identity != (uintptr_t)g_CurrentPlayer
            || snapshot->storage_identity != (uintptr_t)g_CurrentPlayer->p_itemcur)
        return GE_ORIGINAL_INVENTORY_STALE;
    if (!snapshot->selection_allowed || !inventory_selection_allowed())
        return GE_ORIGINAL_INVENTORY_BLOCKED;
    captured = &snapshot->entries[row];
    if (captured->index < 0 || captured->index != (int32_t)row
            || captured->index >= bondinvCountTotalItemsInInv())
        return GE_ORIGINAL_INVENTORY_STALE;
    inventory_entry(captured->index, &current);
    if (current.item_identity != captured->item_identity
            || current.prop_identity != captured->prop_identity
            || current.weapon != captured->weapon
            || current.text_id != captured->text_id)
        return GE_ORIGINAL_INVENTORY_STALE;
    if (!current.selectable || !captured->selectable)
        return GE_ORIGINAL_INVENTORY_NOT_SELECTABLE;
    if (current.selected) return GE_ORIGINAL_INVENTORY_NO_CHANGE;
    /* Exact mutation order of options.c:sub_GAME_7F0A8378. The unchanged
     * canonical wrapper returns loaded ammo and retains previous guns for
     * advancing/backstepping out of a gadget, including dual-wield pairs. */
    currentPlayerUnEquipWeaponWrapper(GUNRIGHT, current.weapon);
    currentPlayerUnEquipWeaponWrapper(GUNLEFT, ITEM_UNARMED);
    g_CurrentPlayer->equipcuritem = current.index;
    return GE_ORIGINAL_INVENTORY_OK;
}
