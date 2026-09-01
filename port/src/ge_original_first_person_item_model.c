#include "ge_original_first_person_assets.h"

#include <string.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/gun.h"

extern GunModelFileRecord gitem_structs[];

typedef struct GeOriginalFirstPersonItemRelation {
    ITEM_IDS item;
    GeOriginalFirstPersonModel model;
    const char *resource;
} GeOriginalFirstPersonItemRelation;

static const GeOriginalFirstPersonItemRelation ge_item_relations[] = {
    {ITEM_WPPK, GE_ORIGINAL_FIRST_PERSON_MODEL_PP7, "GwppkZ"},
    {ITEM_WPPKSIL, GE_ORIGINAL_FIRST_PERSON_MODEL_PP7_SILENCED, "GwppksilZ"},
    {ITEM_BUG, GE_ORIGINAL_FIRST_PERSON_MODEL_BUG, "GbugZ"},
    {ITEM_AK47, GE_ORIGINAL_FIRST_PERSON_MODEL_AK47, "Gak47Z"},
    {ITEM_REMOTEMINE, GE_ORIGINAL_FIRST_PERSON_MODEL_REMOTE_MINE,
     "GremotemineZ"},
    {ITEM_SNIPERRIFLE, GE_ORIGINAL_FIRST_PERSON_MODEL_SNIPER_RIFLE,
     "GsniperrifleZ"},
    {ITEM_TRIGGER, GE_ORIGINAL_FIRST_PERSON_MODEL_TRIGGER, "GtriggerZ"},
    {ITEM_FIST, GE_ORIGINAL_FIRST_PERSON_MODEL_FIST, "GfistZ"},
    {ITEM_MP5KSIL, GE_ORIGINAL_FIRST_PERSON_MODEL_MP5K_SILENCED,
     "Gmp5ksilZ"},
    {ITEM_PLASTIQUE, GE_ORIGINAL_FIRST_PERSON_MODEL_PLASTIQUE, "GplastiqueZ"},
    {ITEM_CAMERA, GE_ORIGINAL_FIRST_PERSON_MODEL_CAMERA, "GcameraZ"},
    {ITEM_WATCHMAGNETATTRACT,
     GE_ORIGINAL_FIRST_PERSON_MODEL_WATCH_MAGNET_ATTRACT,
     "GwatchmagnetattractZ"},
    {ITEM_UZI, GE_ORIGINAL_FIRST_PERSON_MODEL_UZI, "GuziZ"},
    {ITEM_WATCHLASER, GE_ORIGINAL_FIRST_PERSON_MODEL_WATCH_LASER,
     "GwatchlaserZ"},
    {ITEM_GRENADELAUNCH,
     GE_ORIGINAL_FIRST_PERSON_MODEL_GRENADE_LAUNCHER,
     "GgrenadelaunchZ"},
    {ITEM_GRENADE, GE_ORIGINAL_FIRST_PERSON_MODEL_GRENADE, "GgrenadeZ"},
    {ITEM_TIMEDMINE, GE_ORIGINAL_FIRST_PERSON_MODEL_TIMED_MINE,
     "GtimedmineZ"},
    {ITEM_BOMBCASE, GE_ORIGINAL_FIRST_PERSON_MODEL_BOMB_CASE, "GbombcaseZ"},
    {ITEM_MICROCAMERA, GE_ORIGINAL_FIRST_PERSON_MODEL_MICROCAMERA,
     "GmicrocameraZ"},
    {ITEM_GOLDENEYEKEY, GE_ORIGINAL_FIRST_PERSON_MODEL_GOLDENEYE_KEY,
     "GgoldeneyekeyZ"},
    {ITEM_FNP90, GE_ORIGINAL_FIRST_PERSON_MODEL_FNP90, "Gfnp90Z"},
    {ITEM_RUGER, GE_ORIGINAL_FIRST_PERSON_MODEL_RUGER, "GrugerZ"},
    {ITEM_SPECTRE, GE_ORIGINAL_FIRST_PERSON_MODEL_SPECTRE, "GspectreZ"},
    {ITEM_M16, GE_ORIGINAL_FIRST_PERSON_MODEL_M16, "Gm16Z"},
    {ITEM_SHOTGUN, GE_ORIGINAL_FIRST_PERSON_MODEL_SHOTGUN, "GshotgunZ"},
    {ITEM_AUTOSHOT, GE_ORIGINAL_FIRST_PERSON_MODEL_AUTOSHOT, "GautoshotZ"},
    {ITEM_MP5K, GE_ORIGINAL_FIRST_PERSON_MODEL_MP5K, "Gmp5kZ"},
    {ITEM_TT33, GE_ORIGINAL_FIRST_PERSON_MODEL_TT33, "Gtt33Z"},
    {ITEM_SKORPION, GE_ORIGINAL_FIRST_PERSON_MODEL_SKORPION, "GskorpionZ"},
    {ITEM_KNIFE, GE_ORIGINAL_FIRST_PERSON_MODEL_KNIFE, "GknifeZ"},
    {ITEM_THROWKNIFE, GE_ORIGINAL_FIRST_PERSON_MODEL_THROW_KNIFE,
     "GthrowknifeZ"},
    {ITEM_GOLDENGUN, GE_ORIGINAL_FIRST_PERSON_MODEL_GOLDEN_GUN,
     "GgoldengunZ"},
    {ITEM_SILVERWPPK, GE_ORIGINAL_FIRST_PERSON_MODEL_SILVER_PP7,
     "GsilverwppkZ"},
    {ITEM_GOLDWPPK, GE_ORIGINAL_FIRST_PERSON_MODEL_GOLD_PP7, "GgoldwppkZ"},
    {ITEM_LASER, GE_ORIGINAL_FIRST_PERSON_MODEL_LASER, "GlaserZ"},
    {ITEM_ROCKETLAUNCH, GE_ORIGINAL_FIRST_PERSON_MODEL_ROCKET_LAUNCHER,
     "GrocketlaunchZ"},
    {ITEM_PROXIMITYMINE, GE_ORIGINAL_FIRST_PERSON_MODEL_PROXIMITY_MINE,
     "GproximitymineZ"},
    {ITEM_TASER, GE_ORIGINAL_FIRST_PERSON_MODEL_TASER, "GtaserZ"},
    {ITEM_FLAREPISTOL, GE_ORIGINAL_FIRST_PERSON_MODEL_FLARE_PISTOL,
     "GflarepistolZ"},
    {ITEM_PITONGUN, GE_ORIGINAL_FIRST_PERSON_MODEL_PITON_GUN, "GpitongunZ"},
    {ITEM_SUIT_LF_HAND, GE_ORIGINAL_FIRST_PERSON_MODEL_SUIT_LEFT_HAND,
     "Csuit_lf_handZ"},
    {ITEM_JOYPAD, GE_ORIGINAL_FIRST_PERSON_MODEL_JOYPAD, "GjoypadZ"},
};

static const GeOriginalFirstPersonItemRelation *ge_item_relation(int32_t item)
{
    size_t index;
    for (index = 0U;
         index < sizeof(ge_item_relations) / sizeof(ge_item_relations[0]);
         ++index) {
        if ((int32_t)ge_item_relations[index].item == item)
            return &ge_item_relations[index];
    }
    return NULL;
}

int ge_original_first_person_assets_supports_item(int32_t item)
{
    return ge_item_relation(item) != NULL;
}

GeOriginalFirstPersonAssetStatus ge_original_first_person_assets_load_item_native(
    GeOriginalFirstPersonAssets *assets, unsigned hand, int32_t item,
    void **native_header)
{
    const GeOriginalFirstPersonItemRelation *relation;
    GunModelFileRecord *record;
    GeOriginalFirstPersonAssetStatus status;

    if (assets == NULL || native_header == NULL || hand >= 2U)
        return GE_ORIGINAL_FIRST_PERSON_ASSET_INVALID_ARGUMENT;
    *native_header = NULL;
    relation = ge_item_relation(item);
    if (relation == NULL)
        return GE_ORIGINAL_FIRST_PERSON_ASSET_INVALID_ARGUMENT;

    record = &gitem_structs[item];
    if (record->item_header == NULL || record->item_file_name == NULL
            || strcmp(record->item_file_name, relation->resource) != 0)
        return GE_ORIGINAL_FIRST_PERSON_ASSET_INVALID_LAYOUT;
    status = ge_original_first_person_assets_load_raw(
        assets, hand, relation->model, NULL);
    if (status != GE_ORIGINAL_FIRST_PERSON_ASSET_OK) return status;
    return ge_original_first_person_assets_relocate_native(
        assets, hand, record->item_header, native_header);
}

GeOriginalFirstPersonAssetStatus ge_original_first_person_assets_acquire_item_native(
    GeOriginalFirstPersonAssets *assets, unsigned preferred_slot, int32_t item,
    void **native_header, unsigned *cache_slot)
{
    const GeOriginalFirstPersonItemRelation *relation;
    unsigned slot;
    GeOriginalFirstPersonAssetStatus status;
    if (assets == NULL || native_header == NULL || preferred_slot >= 2U)
        return GE_ORIGINAL_FIRST_PERSON_ASSET_INVALID_ARGUMENT;
    *native_header = NULL;
    if (cache_slot != NULL) *cache_slot = 2U;
    relation = ge_item_relation(item);
    if (relation == NULL)
        return GE_ORIGINAL_FIRST_PERSON_ASSET_INVALID_ARGUMENT;
    for (slot = 0U; slot < 2U; ++slot) {
        if (assets->loaded_model[slot] != relation->model
                || !ge_original_first_person_assets_native_ready(assets, slot))
            continue;
        *native_header = ge_original_first_person_assets_native_header(
            assets, slot);
        if (cache_slot != NULL) *cache_slot = slot;
        return GE_ORIGINAL_FIRST_PERSON_ASSET_OK;
    }
    status = ge_original_first_person_assets_load_item_native(
        assets, preferred_slot, item, native_header);
    if (status == GE_ORIGINAL_FIRST_PERSON_ASSET_OK && cache_slot != NULL)
        *cache_slot = preferred_slot;
    return status;
}

GeOriginalFirstPersonAssetStatus ge_original_first_person_assets_acquire_loadout_hand(
    GeOriginalFirstPersonAssets *assets, const int32_t starting_weapon[2],
    unsigned hand, void **native_header, unsigned *cache_slot)
{
    if (starting_weapon == NULL || hand >= 2U)
        return GE_ORIGINAL_FIRST_PERSON_ASSET_INVALID_ARGUMENT;
    return ge_original_first_person_assets_acquire_item_native(
        assets, hand, starting_weapon[hand], native_header, cache_slot);
}
