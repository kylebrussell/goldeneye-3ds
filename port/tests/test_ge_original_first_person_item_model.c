#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/gun.h"
#include "ge_original_first_person_assets.h"

extern GunModelFileRecord gitem_structs[];

static uint8_t right_buffer[GE_ORIGINAL_FIRST_PERSON_HAND_BUFFER_SIZE];
static uint8_t left_buffer[GE_ORIGINAL_FIRST_PERSON_HAND_BUFFER_SIZE];

static ModelNode *find_node_opcode(ModelNode *node, uint16_t opcode)
{
    while (node != NULL) {
        if ((node->Opcode & UINT16_C(0xff)) == opcode) return node;
        if (node->Child != NULL) {
            node = node->Child;
        } else {
            while (node != NULL && node->Next == NULL) node = node->Parent;
            if (node != NULL) node = node->Next;
        }
    }
    return NULL;
}

int main(int argc, char **argv)
{
    GeAssetPack pack;
    GeOriginalFirstPersonAssets assets;
    ModelFileHeader *header = NULL;
    ModelFileHeader *canonical;
    ModelFileHeader *cached_ak47;
    ModelNode *interlink;
    unsigned cache_slot = 2U;
    int32_t loadout[2] = {ITEM_AK47, ITEM_WPPKSIL};

    assert(argc == 2);
    assert(ge_asset_pack_open(&pack, argv[1]) == GE_ASSET_PACK_OK);
    assert(ge_original_first_person_assets_init(
        &assets, &pack, right_buffer, sizeof(right_buffer),
        left_buffer, sizeof(left_buffer)) == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);

    assert(ge_original_first_person_assets_load_item_native(
        &assets, 0U, ITEM_WPPKSIL, (void **)&header)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(header != NULL && header->RootNode != NULL);
    assert(header->Skeleton == gitem_structs[ITEM_WPPKSIL].item_header->Skeleton);

    canonical = gitem_structs[ITEM_BUG].item_header;
    assert(canonical != NULL && canonical->Skeleton != NULL);
    assert(ge_original_first_person_assets_load_item_native(
        &assets, 0U, ITEM_BUG, (void **)&header)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(header != NULL && header != canonical);
    assert(header->RootNode != NULL);
    assert(header->Skeleton == canonical->Skeleton);
    assert(header->numSwitches == canonical->numSwitches);
    assert(header->numMatrices == canonical->numMatrices);
    assert(header->numtextures == canonical->numtextures);
    assert(fabsf(header->BoundingVolumeRadius
                 - canonical->BoundingVolumeRadius) < 1.0e-5f);
    assert(assets.loaded_model[0] == GE_ORIGINAL_FIRST_PERSON_MODEL_BUG);
    assert(ge_original_first_person_assets_native_node_count(&assets, 0U) == 2U);
    canonical = gitem_structs[ITEM_SNIPERRIFLE].item_header;
    assert(ge_original_first_person_assets_acquire_item_native(
        &assets, 1U, ITEM_SNIPERRIFLE, (void **)&header, &cache_slot)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(cache_slot == 1U && header->Skeleton == canonical->Skeleton);
    assert(header->numSwitches == 36 && header->numMatrices == 4
           && header->numtextures == 7);
    assert(ge_original_first_person_assets_native_node_count(&assets, 1U) == 26U);
    canonical = gitem_structs[ITEM_TRIGGER].item_header;
    assert(ge_original_first_person_assets_acquire_item_native(
        &assets, 0U, ITEM_TRIGGER, (void **)&header, &cache_slot)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(cache_slot == 0U && header->Skeleton == canonical->Skeleton);
    assert(header->numSwitches == 36 && header->numMatrices == 4
           && header->numtextures == 22);
    assert(ge_original_first_person_assets_native_node_count(&assets, 0U) == 19U);
    interlink = find_node_opcode(header->RootNode, MODELNODE_OPCODE_INTERLINK);
    assert(interlink != NULL && interlink->Data != NULL);
    assert(fabsf(interlink->Data->Interlinkage.pos.f[0] - 20.208658f)
           < 1.0e-5f);
    assert(fabsf(interlink->Data->Interlinkage.pos.f[1] - 32.66917f)
           < 1.0e-5f);
    assert(fabsf(interlink->Data->Interlinkage.pos.f[2] + 18.414536f)
           < 1.0e-5f);
    assert(interlink->Data->Interlinkage.Scale == 0.0f);
    canonical = gitem_structs[ITEM_FIST].item_header;
    assert(ge_original_first_person_assets_acquire_item_native(
        &assets, 1U, ITEM_FIST, (void **)&header, &cache_slot)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(cache_slot == 1U && header->Skeleton == canonical->Skeleton);
    assert(header->numSwitches == 36 && header->numMatrices == 3
           && header->numtextures == 14);
    assert(ge_original_first_person_assets_native_node_count(&assets, 1U) == 11U);
    canonical = gitem_structs[ITEM_MP5KSIL].item_header;
    assert(ge_original_first_person_assets_acquire_item_native(
        &assets, 0U, ITEM_MP5KSIL, (void **)&header, &cache_slot)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(cache_slot == 0U && header->Skeleton == canonical->Skeleton);
    assert(header->numSwitches == 36 && header->numMatrices == 4
           && header->numtextures == 9);
    assert(ge_original_first_person_assets_native_node_count(&assets, 0U) == 27U);
    canonical = gitem_structs[ITEM_PLASTIQUE].item_header;
    assert(ge_original_first_person_assets_acquire_item_native(
        &assets, 1U, ITEM_PLASTIQUE, (void **)&header, &cache_slot)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(cache_slot == 1U && header->Skeleton == canonical->Skeleton);
    assert(header->numSwitches == 28 && header->numMatrices == 3
           && header->numtextures == 3);
    assert(ge_original_first_person_assets_native_node_count(&assets, 1U) == 6U);
    canonical = gitem_structs[ITEM_CAMERA].item_header;
    assert(ge_original_first_person_assets_acquire_item_native(
        &assets, 0U, ITEM_CAMERA, (void **)&header, &cache_slot)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(cache_slot == 0U && header->Skeleton == canonical->Skeleton);
    assert(header->numSwitches == 28 && header->numMatrices == 3
           && header->numtextures == 11);
    assert(ge_original_first_person_assets_native_node_count(&assets, 0U) == 2U);
    canonical = gitem_structs[ITEM_WATCHMAGNETATTRACT].item_header;
    assert(ge_original_first_person_assets_acquire_item_native(
        &assets, 1U, ITEM_WATCHMAGNETATTRACT, (void **)&header, &cache_slot)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(cache_slot == 1U && header->Skeleton == canonical->Skeleton);
    assert(header->numSwitches == 29 && header->numMatrices == 3
           && header->numtextures == 9);
    assert(ge_original_first_person_assets_native_node_count(&assets, 1U) == 2U);
    {
        static const struct {
            ITEM_IDS item;
            GeOriginalFirstPersonModel model;
            uint16_t switches;
            uint16_t matrices;
            uint16_t textures;
            size_t nodes;
        } intro_cases[] = {
            {ITEM_UZI, GE_ORIGINAL_FIRST_PERSON_MODEL_UZI,
             36U, 5U, 12U, 22U},
            {ITEM_WATCHLASER, GE_ORIGINAL_FIRST_PERSON_MODEL_WATCH_LASER,
             36U, 4U, 22U, 19U},
            {ITEM_GRENADELAUNCH,
             GE_ORIGINAL_FIRST_PERSON_MODEL_GRENADE_LAUNCHER,
             36U, 5U, 15U, 36U},
            {ITEM_GRENADE, GE_ORIGINAL_FIRST_PERSON_MODEL_GRENADE,
             36U, 3U, 5U, 2U},
            {ITEM_TIMEDMINE, GE_ORIGINAL_FIRST_PERSON_MODEL_TIMED_MINE,
             28U, 3U, 4U, 2U},
            {ITEM_BOMBCASE, GE_ORIGINAL_FIRST_PERSON_MODEL_BOMB_CASE,
             28U, 3U, 6U, 2U},
            {ITEM_MICROCAMERA, GE_ORIGINAL_FIRST_PERSON_MODEL_MICROCAMERA,
             35U, 3U, 7U, 2U},
            {ITEM_GOLDENEYEKEY,
             GE_ORIGINAL_FIRST_PERSON_MODEL_GOLDENEYE_KEY,
             28U, 3U, 5U, 2U},
            {ITEM_FNP90, GE_ORIGINAL_FIRST_PERSON_MODEL_FNP90,
             36U, 4U, 10U, 30U},
            {ITEM_RUGER, GE_ORIGINAL_FIRST_PERSON_MODEL_RUGER,
             36U, 6U, 14U, 40U},
            {ITEM_SPECTRE, GE_ORIGINAL_FIRST_PERSON_MODEL_SPECTRE,
             36U, 4U, 11U, 28U},
            {ITEM_M16, GE_ORIGINAL_FIRST_PERSON_MODEL_M16,
             36U, 4U, 8U, 30U},
            {ITEM_SHOTGUN, GE_ORIGINAL_FIRST_PERSON_MODEL_SHOTGUN,
             28U, 4U, 13U, 39U},
            {ITEM_AUTOSHOT, GE_ORIGINAL_FIRST_PERSON_MODEL_AUTOSHOT,
             36U, 4U, 16U, 63U},
            {ITEM_MP5K, GE_ORIGINAL_FIRST_PERSON_MODEL_MP5K,
             36U, 4U, 9U, 25U},
            {ITEM_TT33, GE_ORIGINAL_FIRST_PERSON_MODEL_TT33,
             36U, 6U, 15U, 47U},
            {ITEM_SKORPION, GE_ORIGINAL_FIRST_PERSON_MODEL_SKORPION,
             36U, 4U, 12U, 42U},
            {ITEM_KNIFE, GE_ORIGINAL_FIRST_PERSON_MODEL_KNIFE,
             36U, 3U, 9U, 30U},
            {ITEM_THROWKNIFE, GE_ORIGINAL_FIRST_PERSON_MODEL_THROW_KNIFE,
             36U, 3U, 9U, 32U},
            {ITEM_GOLDENGUN, GE_ORIGINAL_FIRST_PERSON_MODEL_GOLDEN_GUN,
             36U, 5U, 11U, 32U},
            {ITEM_SILVERWPPK, GE_ORIGINAL_FIRST_PERSON_MODEL_SILVER_PP7,
             36U, 6U, 11U, 45U},
            {ITEM_GOLDWPPK, GE_ORIGINAL_FIRST_PERSON_MODEL_GOLD_PP7,
             36U, 6U, 11U, 45U},
            {ITEM_LASER, GE_ORIGINAL_FIRST_PERSON_MODEL_LASER,
             36U, 3U, 13U, 25U},
            {ITEM_ROCKETLAUNCH,
             GE_ORIGINAL_FIRST_PERSON_MODEL_ROCKET_LAUNCHER,
             36U, 3U, 10U, 35U},
            {ITEM_PROXIMITYMINE,
             GE_ORIGINAL_FIRST_PERSON_MODEL_PROXIMITY_MINE,
             28U, 3U, 3U, 2U},
            {ITEM_TASER, GE_ORIGINAL_FIRST_PERSON_MODEL_TASER,
             35U, 3U, 17U, 34U},
            {ITEM_FLAREPISTOL, GE_ORIGINAL_FIRST_PERSON_MODEL_FLARE_PISTOL,
             28U, 3U, 2U, 2U},
            {ITEM_PITONGUN, GE_ORIGINAL_FIRST_PERSON_MODEL_PITON_GUN,
             28U, 3U, 2U, 2U},
            {ITEM_SUIT_LF_HAND,
             GE_ORIGINAL_FIRST_PERSON_MODEL_SUIT_LEFT_HAND,
             10U, 9U, 22U, 28U},
            {ITEM_JOYPAD, GE_ORIGINAL_FIRST_PERSON_MODEL_JOYPAD,
             14U, 13U, 4U, 27U},
        };
        size_t index;
        for (index = 0U;
             index < sizeof(intro_cases) / sizeof(intro_cases[0]);
             ++index) {
            const unsigned slot = (unsigned)(index & 1U);
            GeOriginalFirstPersonAssetStatus status;
            canonical = gitem_structs[intro_cases[index].item].item_header;
            assert(canonical != NULL && canonical->Skeleton != NULL);
            assert(ge_original_first_person_assets_supports_item(
                intro_cases[index].item));
            status = ge_original_first_person_assets_acquire_item_native(
                &assets, slot, intro_cases[index].item,
                (void **)&header, &cache_slot);
            if (status != GE_ORIGINAL_FIRST_PERSON_ASSET_OK)
                fprintf(stderr, "item %d failed with asset status %d\n",
                        (int)intro_cases[index].item, (int)status);
            assert(status == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
            assert(cache_slot == slot);
            assert(assets.loaded_model[slot] == intro_cases[index].model);
            assert(header->Skeleton == canonical->Skeleton);
            assert((uint16_t)header->numSwitches
                   == intro_cases[index].switches);
            assert((uint16_t)header->numMatrices
                   == intro_cases[index].matrices);
            assert((uint16_t)header->numtextures
                   == intro_cases[index].textures);
            assert(ge_original_first_person_assets_native_node_count(
                &assets, slot) == intro_cases[index].nodes);
            if (intro_cases[index].item == ITEM_SUIT_LF_HAND) {
                assert((header->RootNode->Opcode & UINT16_C(0xff))
                       == MODELNODE_OPCODE_HEADER);
                assert(header->RootNode->Data != NULL);
                assert(header->RootNode->Data->Header.FirstGroupNode != NULL);
            }
            if (intro_cases[index].item == ITEM_FNP90) {
                ModelNode *node = header->RootNode;
                ModelNode *muzzle = NULL;
                while (node != NULL) {
                    if ((node->Opcode & UINT16_C(0xff))
                            == MODELNODE_OPCODE_DLPRIMARY) {
                        muzzle = node;
                        break;
                    }
                    if (node->Child != NULL) node = node->Child;
                    else {
                        while (node != NULL && node->Next == NULL)
                            node = node->Parent;
                        if (node != NULL) node = node->Next;
                    }
                }
                assert(muzzle != NULL);
                assert(muzzle->Data->DisplayListPrimary.numVertices == 3);
                assert(muzzle->Data->DisplayListPrimary.Vertices != NULL);
                assert(muzzle->Data->DisplayListPrimary.Primary != NULL);
            } else if (intro_cases[index].item == ITEM_TASER) {
                ModelNode *node = header->RootNode;
                ModelNode *collision = NULL;
                uint32_t vertex_blob_offset = 0U;
                while (node != NULL) {
                    if ((node->Opcode & UINT16_C(0xff))
                            == MODELNODE_OPCODE_DLCOLLISION) {
                        collision = node;
                        break;
                    }
                    if (node->Child != NULL) node = node->Child;
                    else {
                        while (node != NULL && node->Next == NULL)
                            node = node->Parent;
                        if (node != NULL) node = node->Next;
                    }
                }
                assert(collision != NULL);
                assert(collision->Data->DisplayListCollisions.Primary != NULL);
                assert(collision->Data->DisplayListCollisions.Secondary == NULL);
                assert(collision->Data->DisplayListCollisions.numVertices == 4);
                assert(collision->Data->DisplayListCollisions
                       .numCollisionVertices == 4);
                assert(collision->Data->DisplayListCollisions.Vertices != NULL);
                assert(collision->Data->DisplayListCollisions
                       .CollisionVertices != NULL);
                assert(collision->Data->DisplayListCollisions.PointUsage[0]
                       == -1);
                assert(ge_original_first_person_assets_collision_vertex_blob_offset(
                    &assets, collision, &vertex_blob_offset));
                assert(vertex_blob_offset == UINT32_C(0x3218));
            }
        }
    }
    canonical = gitem_structs[ITEM_AK47].item_header;
    assert(canonical != NULL && canonical->Skeleton != NULL);
    assert(ge_original_first_person_assets_load_item_native(
        &assets, 1U, ITEM_AK47, (void **)&header)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(header != NULL && header != canonical && header->RootNode != NULL);
    assert(header->Skeleton == canonical->Skeleton);
    assert(header->numSwitches == 36);
    assert(header->numMatrices == 4);
    assert(header->numtextures == 18);
    assert(fabsf(header->BoundingVolumeRadius - 941.9339f) < 1.0e-4f);
    assert(assets.loaded_model[1] == GE_ORIGINAL_FIRST_PERSON_MODEL_AK47);
    assert(ge_original_first_person_assets_native_node_count(&assets, 1U) == 28U);
    cached_ak47 = header;
    assert(ge_original_first_person_assets_supports_item(ITEM_AK47));
    assert(ge_original_first_person_assets_supports_item(ITEM_SNIPERRIFLE));
    assert(ge_original_first_person_assets_supports_item(ITEM_MP5K));
    assert(ge_original_first_person_assets_supports_item(ITEM_KNIFE));
    assert(!ge_original_first_person_assets_supports_item(ITEM_DOORDECODER));
    assert(ge_original_first_person_assets_acquire_item_native(
        &assets, 0U, ITEM_AK47, (void **)&header, &cache_slot)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(header == cached_ak47 && cache_slot == 1U);
    cache_slot = 2U;
    assert(ge_original_first_person_assets_slot_for_header(
        &assets, header, &cache_slot));
    assert(cache_slot == 1U);
    assert(ge_original_first_person_assets_acquire_loadout_hand(
        &assets, loadout, 0U, (void **)&header, &cache_slot)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(header == cached_ak47 && cache_slot == 1U);
    canonical = gitem_structs[ITEM_REMOTEMINE].item_header;
    assert(canonical != NULL && canonical->Skeleton != NULL);
    assert(ge_original_first_person_assets_load_item_native(
        &assets, 0U, ITEM_REMOTEMINE, (void **)&header)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(header != NULL && header != canonical && header->RootNode != NULL);
    assert(header->Skeleton == canonical->Skeleton);
    assert(header->numSwitches == 35);
    assert(header->numMatrices == 3);
    assert(header->numtextures == 3);
    assert(fabsf(header->BoundingVolumeRadius - 50.999378f) < 1.0e-5f);
    assert(assets.loaded_model[0]
           == GE_ORIGINAL_FIRST_PERSON_MODEL_REMOTE_MINE);
    assert(ge_original_first_person_assets_native_node_count(&assets, 0U) == 2U);
    assert(ge_original_first_person_assets_load_item_native(
        &assets, 0U, ITEM_UNARMED, (void **)&header)
        == GE_ORIGINAL_FIRST_PERSON_ASSET_INVALID_ARGUMENT);
    assert(header == NULL);

    ge_original_first_person_assets_close(&assets);
    ge_asset_pack_close(&pack);
    puts("canonical authored intro first-person model switches passed");
    return 0;
}
