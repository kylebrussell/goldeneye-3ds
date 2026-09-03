#ifndef GE_ORIGINAL_FIRST_PERSON_ASSETS_H
#define GE_ORIGINAL_FIRST_PERSON_ASSETS_H

#include <stddef.h>
#include <stdint.h>

#include "ge_asset_pack.h"

#define GE_ORIGINAL_FIRST_PERSON_HAND_BUFFER_SIZE 0x14820U
#define GE_ORIGINAL_FIRST_PERSON_PP7_PATH \
    "converted/models/first-person-pp7/GwppkZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_PP7_SILENCED_PATH \
    "converted/models/first-person-pp7/GwppksilZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_BUG_PATH \
    "converted/models/first-person-pp7/GbugZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_AK47_PATH \
    "converted/models/first-person-pp7/Gak47Z.bin"
#define GE_ORIGINAL_FIRST_PERSON_REMOTE_MINE_PATH \
    "converted/models/first-person-pp7/GremotemineZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_SNIPER_RIFLE_PATH \
    "converted/models/first-person-pp7/GsniperrifleZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_TRIGGER_PATH \
    "converted/models/first-person-pp7/GtriggerZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_FIST_PATH \
    "converted/models/first-person-pp7/GfistZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_MP5K_SILENCED_PATH \
    "converted/models/first-person-pp7/Gmp5ksilZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_PLASTIQUE_PATH \
    "converted/models/first-person-pp7/GplastiqueZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_CAMERA_PATH \
    "converted/models/first-person-pp7/GcameraZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_WATCH_MAGNET_ATTRACT_PATH \
    "converted/models/first-person-pp7/GwatchmagnetattractZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_UZI_PATH \
    "converted/models/first-person-pp7/GuziZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_WATCH_LASER_PATH \
    "converted/models/first-person-pp7/GwatchlaserZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_GRENADE_LAUNCHER_PATH \
    "converted/models/first-person-pp7/GgrenadelaunchZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_GRENADE_PATH \
    "converted/models/first-person-pp7/GgrenadeZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_TIMED_MINE_PATH \
    "converted/models/first-person-pp7/GtimedmineZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_BOMB_CASE_PATH \
    "converted/models/first-person-pp7/GbombcaseZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_MICROCAMERA_PATH \
    "converted/models/first-person-pp7/GmicrocameraZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_GOLDENEYE_KEY_PATH \
    "converted/models/first-person-pp7/GgoldeneyekeyZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_FNP90_PATH \
    "converted/models/first-person-pp7/Gfnp90Z.bin"
#define GE_ORIGINAL_FIRST_PERSON_RUGER_PATH \
    "converted/models/first-person-pp7/GrugerZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_SPECTRE_PATH \
    "converted/models/first-person-pp7/GspectreZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_M16_PATH \
    "converted/models/first-person-pp7/Gm16Z.bin"
#define GE_ORIGINAL_FIRST_PERSON_SHOTGUN_PATH \
    "converted/models/first-person-pp7/GshotgunZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_AUTOSHOT_PATH \
    "converted/models/first-person-pp7/GautoshotZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_MP5K_PATH \
    "converted/models/first-person-pp7/Gmp5kZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_TT33_PATH \
    "converted/models/first-person-pp7/Gtt33Z.bin"
#define GE_ORIGINAL_FIRST_PERSON_SKORPION_PATH \
    "converted/models/first-person-pp7/GskorpionZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_KNIFE_PATH \
    "converted/models/first-person-pp7/GknifeZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_THROW_KNIFE_PATH \
    "converted/models/first-person-pp7/GthrowknifeZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_GOLDEN_GUN_PATH \
    "converted/models/first-person-pp7/GgoldengunZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_SILVER_PP7_PATH \
    "converted/models/first-person-pp7/GsilverwppkZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_GOLD_PP7_PATH \
    "converted/models/first-person-pp7/GgoldwppkZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_LASER_PATH \
    "converted/models/first-person-pp7/GlaserZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_ROCKET_LAUNCHER_PATH \
    "converted/models/first-person-pp7/GrocketlaunchZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_PROXIMITY_MINE_PATH \
    "converted/models/first-person-pp7/GproximitymineZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_TASER_PATH \
    "converted/models/first-person-pp7/GtaserZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_FLARE_PISTOL_PATH \
    "converted/models/first-person-pp7/GflarepistolZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_PITON_GUN_PATH \
    "converted/models/first-person-pp7/GpitongunZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_SUIT_LEFT_HAND_PATH \
    "converted/models/first-person-pp7/Csuit_lf_handZ.bin"
#define GE_ORIGINAL_FIRST_PERSON_JOYPAD_PATH \
    "converted/models/first-person-pp7/GjoypadZ.bin"

typedef enum GeOriginalFirstPersonModel {
    GE_ORIGINAL_FIRST_PERSON_MODEL_PP7 = 0,
    GE_ORIGINAL_FIRST_PERSON_MODEL_PP7_SILENCED = 1,
    GE_ORIGINAL_FIRST_PERSON_MODEL_BUG = 2,
    GE_ORIGINAL_FIRST_PERSON_MODEL_AK47 = 3,
    GE_ORIGINAL_FIRST_PERSON_MODEL_REMOTE_MINE = 4,
    GE_ORIGINAL_FIRST_PERSON_MODEL_SNIPER_RIFLE = 5,
    GE_ORIGINAL_FIRST_PERSON_MODEL_TRIGGER = 6,
    GE_ORIGINAL_FIRST_PERSON_MODEL_FIST = 7,
    GE_ORIGINAL_FIRST_PERSON_MODEL_MP5K_SILENCED = 8,
    GE_ORIGINAL_FIRST_PERSON_MODEL_PLASTIQUE = 9,
    GE_ORIGINAL_FIRST_PERSON_MODEL_CAMERA = 10,
    GE_ORIGINAL_FIRST_PERSON_MODEL_WATCH_MAGNET_ATTRACT = 11,
    GE_ORIGINAL_FIRST_PERSON_MODEL_UZI = 12,
    GE_ORIGINAL_FIRST_PERSON_MODEL_WATCH_LASER = 13,
    GE_ORIGINAL_FIRST_PERSON_MODEL_GRENADE_LAUNCHER = 14,
    GE_ORIGINAL_FIRST_PERSON_MODEL_GRENADE = 15,
    GE_ORIGINAL_FIRST_PERSON_MODEL_TIMED_MINE = 16,
    GE_ORIGINAL_FIRST_PERSON_MODEL_BOMB_CASE = 17,
    GE_ORIGINAL_FIRST_PERSON_MODEL_MICROCAMERA = 18,
    GE_ORIGINAL_FIRST_PERSON_MODEL_GOLDENEYE_KEY = 19,
    GE_ORIGINAL_FIRST_PERSON_MODEL_FNP90 = 20,
    GE_ORIGINAL_FIRST_PERSON_MODEL_RUGER = 21,
    GE_ORIGINAL_FIRST_PERSON_MODEL_SPECTRE = 22,
    GE_ORIGINAL_FIRST_PERSON_MODEL_M16 = 23,
    GE_ORIGINAL_FIRST_PERSON_MODEL_SHOTGUN = 24,
    GE_ORIGINAL_FIRST_PERSON_MODEL_AUTOSHOT = 25,
    GE_ORIGINAL_FIRST_PERSON_MODEL_MP5K = 26,
    GE_ORIGINAL_FIRST_PERSON_MODEL_TT33 = 27,
    GE_ORIGINAL_FIRST_PERSON_MODEL_SKORPION = 28,
    GE_ORIGINAL_FIRST_PERSON_MODEL_KNIFE = 29,
    GE_ORIGINAL_FIRST_PERSON_MODEL_THROW_KNIFE = 30,
    GE_ORIGINAL_FIRST_PERSON_MODEL_GOLDEN_GUN = 31,
    GE_ORIGINAL_FIRST_PERSON_MODEL_SILVER_PP7 = 32,
    GE_ORIGINAL_FIRST_PERSON_MODEL_GOLD_PP7 = 33,
    GE_ORIGINAL_FIRST_PERSON_MODEL_LASER = 34,
    GE_ORIGINAL_FIRST_PERSON_MODEL_ROCKET_LAUNCHER = 35,
    GE_ORIGINAL_FIRST_PERSON_MODEL_PROXIMITY_MINE = 36,
    GE_ORIGINAL_FIRST_PERSON_MODEL_TASER = 37,
    GE_ORIGINAL_FIRST_PERSON_MODEL_FLARE_PISTOL = 38,
    GE_ORIGINAL_FIRST_PERSON_MODEL_PITON_GUN = 39,
    GE_ORIGINAL_FIRST_PERSON_MODEL_SUIT_LEFT_HAND = 40,
    GE_ORIGINAL_FIRST_PERSON_MODEL_JOYPAD = 41,
    GE_ORIGINAL_FIRST_PERSON_MODEL_NONE = 42
} GeOriginalFirstPersonModel;

typedef enum GeOriginalFirstPersonAssetStatus {
    GE_ORIGINAL_FIRST_PERSON_ASSET_OK = 0,
    GE_ORIGINAL_FIRST_PERSON_ASSET_INVALID_ARGUMENT = -1,
    GE_ORIGINAL_FIRST_PERSON_ASSET_WRONG_ROM = -2,
    GE_ORIGINAL_FIRST_PERSON_ASSET_MISSING = -3,
    GE_ORIGINAL_FIRST_PERSON_ASSET_INVALID_SIZE = -4,
    GE_ORIGINAL_FIRST_PERSON_ASSET_INVALID_LAYOUT = -5,
    GE_ORIGINAL_FIRST_PERSON_ASSET_IO_ERROR = -6
} GeOriginalFirstPersonAssetStatus;

typedef struct GeOriginalFirstPersonAssets {
    GeAssetPack *pack;
    uint8_t *hand_buffer[2];
    size_t hand_buffer_size[2];
    size_t loaded_size[2];
    GeOriginalFirstPersonModel loaded_model[2];
    void *native_model[2];
} GeOriginalFirstPersonAssets;

/* Binds the two buffers corresponding to the original GUNRIGHT/GUNLEFT
 * allocation. Storage ownership remains with the stage memory provider. */
GeOriginalFirstPersonAssetStatus ge_original_first_person_assets_init(
    GeOriginalFirstPersonAssets *assets, GeAssetPack *pack,
    void *right_buffer, size_t right_size,
    void *left_buffer, size_t left_size);

/* Copies one exact decompressed model resource into the original hand model
 * region and validates its serialized ModelFileHeader-dependent layout.
 * Native pointer/endian relocation is intentionally a separate gate. */
GeOriginalFirstPersonAssetStatus ge_original_first_person_assets_load_raw(
    GeOriginalFirstPersonAssets *assets, unsigned hand,
    GeOriginalFirstPersonModel model, size_t *bytes_read);

/* Materializes the serialized big-endian model graph into pointer-safe native
 * ModelNode/ModelRoData storage. header_template may be the canonical wppk or
 * wppksil ModelFileHeader from the decompiled gun-data table. A null template
 * retains the serialized model metadata but leaves the skeleton unbound. */
GeOriginalFirstPersonAssetStatus ge_original_first_person_assets_relocate_native(
    GeOriginalFirstPersonAssets *assets, unsigned hand,
    const void *header_template, void **native_header);

/* Resolves the selected item through the canonical gitem_structs table, loads
 * its exact ROM resource, and relocates it using that item's authored header.
 * This is the on-demand hand-model boundary used before pose binding. */
GeOriginalFirstPersonAssetStatus ge_original_first_person_assets_load_item_native(
    GeOriginalFirstPersonAssets *assets, unsigned hand, int32_t item,
    void **native_header);
/* Resolves an item through the canonical gitem relation and reuses either
 * original hand-model region when that exact item is already relocated.
 * preferred_slot is used only on a cache miss. */
GeOriginalFirstPersonAssetStatus ge_original_first_person_assets_acquire_item_native(
    GeOriginalFirstPersonAssets *assets, unsigned preferred_slot, int32_t item,
    void **native_header, unsigned *cache_slot);
/* Consumes the exact starting_weapon[GUNRIGHT/GUNLEFT] result published by
 * bondviewLoadSetupIntroLoadoutSlice without stage-specific item selection. */
GeOriginalFirstPersonAssetStatus ge_original_first_person_assets_acquire_loadout_hand(
    GeOriginalFirstPersonAssets *assets, const int32_t starting_weapon[2],
    unsigned hand, void **native_header, unsigned *cache_slot);
int ge_original_first_person_assets_supports_item(int32_t item);
int ge_original_first_person_assets_native_ready(
    const GeOriginalFirstPersonAssets *assets, unsigned hand);
size_t ge_original_first_person_assets_native_node_count(
    const GeOriginalFirstPersonAssets *assets, unsigned hand);
void *ge_original_first_person_assets_native_header(
    const GeOriginalFirstPersonAssets *assets, unsigned hand);
/* Visit the model's complete ROM-authored texture table, including images
 * used only by inactive fire/reload switches. Renderer preloading only: no
 * switch, hand, model, RNG or gameplay state is advanced. */
int ge_original_first_person_assets_visit_texture_ids(
    const GeOriginalFirstPersonAssets *assets, unsigned asset_slot,
    void *context, int (*visitor)(void *context, uint16_t image_id));
const uint8_t *ge_original_first_person_assets_blob(
    const GeOriginalFirstPersonAssets *assets, unsigned hand,
    size_t *blob_size);
/* Finds the ROM blob which owns a relocated model graph.  A gun hand may use
 * a model loaded through the other staging slot after an original weapon
 * switch, so renderers must follow the graph rather than assume hand==slot. */
const uint8_t *ge_original_first_person_assets_blob_for_root(
    const GeOriginalFirstPersonAssets *assets, const void *root_node,
    size_t *blob_size, unsigned *asset_slot);
int ge_original_first_person_assets_slot_for_header(
    const GeOriginalFirstPersonAssets *assets, const void *native_header,
    unsigned *asset_slot);
/* Resolves the serialized segment-4 vertex base retained for an authored
 * DLCOLLISION node while its canonical native collision ABI uses decoded
 * Vertex storage. */
int ge_original_first_person_assets_collision_vertex_blob_offset(
    const GeOriginalFirstPersonAssets *assets, const void *native_node,
    uint32_t *blob_offset);
void ge_original_first_person_assets_close(GeOriginalFirstPersonAssets *assets);

typedef struct GeOriginalFirstPersonLoaderState {
    uint32_t load_calls;
    uint32_t successful_loads;
    uint32_t texture_pool_handoffs;
    GeOriginalFirstPersonAssetStatus last_status;
} GeOriginalFirstPersonLoaderState;

/* Binds the platform asset-pack loader used by the exact on-demand gun body.
 * Texture decoding remains owned by the native renderer, so a non-null texpool
 * is recorded as a renderer handoff rather than rewriting N64 display lists. */
void ge_original_first_person_assets_bind_loader(
    GeOriginalFirstPersonAssets *assets,
    GeOriginalFirstPersonLoaderState *state);

#endif
