#include <assert.h>
#include <string.h>

#include "game/gun.h"

extern GunModelFileRecord gitem_structs[];
extern WeaponStats default_weaponstats;
extern AmmoStats ammo_related[30];
extern struct gun_trigger_state g_ZeroTriggerState;

int main(void)
{
    const WeaponStats *pp7 = gitem_structs[ITEM_WPPK].item_weapon_stats;
    const WeaponStats *silenced =
        gitem_structs[ITEM_WPPKSIL].item_weapon_stats;
    const WeaponStats *ak47 = gitem_structs[ITEM_AK47].item_weapon_stats;
    const WeaponStats *bug = gitem_structs[ITEM_BUG].item_weapon_stats;

    assert(gitem_structs[ITEM_UNARMED].has_no_model == TRUE);
    assert(gitem_structs[ITEM_UNARMED].item_weapon_stats == NULL);
    assert(gitem_structs[ITEM_WPPK].item_header != NULL);
    assert(gitem_structs[ITEM_WPPKSIL].item_header != NULL);
    assert(strcmp(gitem_structs[ITEM_WPPK].item_file_name, "GwppkZ") == 0);
    assert(strcmp(gitem_structs[ITEM_WPPKSIL].item_file_name, "GwppksilZ") == 0);
    assert(gitem_structs[ITEM_BUG].item_header != NULL);
    assert(strcmp(gitem_structs[ITEM_BUG].item_file_name, "GbugZ") == 0);
    assert(gitem_structs[ITEM_BUG].item_header->numSwitches == 0x1c);
    assert(gitem_structs[ITEM_BUG].item_header->numMatrices == 3);
    assert(gitem_structs[ITEM_BUG].item_header->numtextures == 6);
    assert(gitem_structs[ITEM_WPPK].item_header->numSwitches == 0x24);
    assert(gitem_structs[ITEM_WPPK].item_header->numMatrices == 6);
    assert(gitem_structs[ITEM_WPPK].item_header->numtextures == 0xC);
    assert(default_weaponstats.BitFlags ==
        (WEAPONSTATBITFLAG_CLICKY | WEAPONSTATBITFLAG_ONLY_1_HANDED));
    assert(pp7 != NULL);
    assert(silenced != NULL);
    assert(ak47 != NULL);
    assert(bug != NULL);
    assert(pp7->AmmoType == AMMO_9MM);
    assert(silenced->AmmoType == AMMO_9MM);
    assert(ak47->AmmoType == AMMO_RIFLE);
    assert((ak47->BitFlags & WEAPONSTATBITFLAG_ONLY_1_HANDED) == 0U);
    assert(bug->AmmoType == AMMO_BUG);
    assert((bug->BitFlags & WEAPONSTATBITFLAG_SINGLE_USE_RELOAD) != 0);
    assert(pp7->Zoom == 0.0f);
    assert(silenced->Zoom == 0.0f);
    assert((silenced->BitFlags & WEAPONSTATBITFLAG_HAS_AUTO_AIM) != 0);
    assert((silenced->BitFlags & WEAPONSTATBITFLAG_HAS_AMMO) != 0);
    assert((silenced->BitFlags & WEAPONSTATBITFLAG_DISABLE_CROUCH) == 0);
    assert(silenced->ptr_cartridge_struct != NULL);
    assert(silenced->ptr_cartridge_struct->RootNode != NULL);
    assert(silenced->ptr_cartridge_struct->Skeleton != NULL);
    assert(silenced->ptr_cartridge_struct->Textures != NULL);
    assert(silenced->ptr_cartridge_struct->numMatrices == 1);
    assert(silenced->ptr_cartridge_struct->numtextures == 2);
    assert(ammo_related[AMMO_9MM].MaxAmmo == 0x320U);
    assert(ammo_related[AMMO_9MM].IconImage == 0x02000C84U);
    assert(ammo_related[AMMO_9MM].IconYOffset == 0.0f);
    assert(g_ZeroTriggerState.triggerOn[GUNRIGHT] == 0);
    assert(g_ZeroTriggerState.triggerOn[GUNLEFT] == 0);
    return 0;
}
