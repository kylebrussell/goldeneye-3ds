#ifndef GE_ORIGINAL_GUNBARREL_BOND_H
#define GE_ORIGINAL_GUNBARREL_BOND_H

#include "ge_asset_pack.h"
#include "ge_dam_room.h"
#include "ge_original_gunbarrel.h"
#include "ge_original_model_scene.h"

#include <stddef.h>
#include <stdint.h>

typedef struct GeOriginalGunbarrelBond GeOriginalGunbarrelBond;

typedef enum GeOriginalGunbarrelBondStatus {
    GE_ORIGINAL_GUNBARREL_BOND_OK = 0,
    GE_ORIGINAL_GUNBARREL_BOND_INVALID_ARGUMENT,
    GE_ORIGINAL_GUNBARREL_BOND_ASSET_MISSING,
    GE_ORIGINAL_GUNBARREL_BOND_MODEL_UNAVAILABLE,
    GE_ORIGINAL_GUNBARREL_BOND_ANIMATION_UNAVAILABLE,
    GE_ORIGINAL_GUNBARREL_BOND_ATTACHMENT_UNAVAILABLE,
    GE_ORIGINAL_GUNBARREL_BOND_MATRIX_UNAVAILABLE,
    GE_ORIGINAL_GUNBARREL_BOND_SCENE_UNAVAILABLE,
    GE_ORIGINAL_GUNBARREL_BOND_ALLOCATION_FAILED
} GeOriginalGunbarrelBondStatus;

typedef struct GeOriginalGunbarrelBondScene {
    const GeDamRoomWorldVertex *vertices;
    const GeDamRoomDrawBatch *batches;
    /* Exact DisplayListCollisions.ModelType for each corresponding batch.
     * The title renderer's modelRenderNodeDl wrapper uses this value to apply
     * its inherited material before invoking the child display list. */
    const int16_t *batch_model_types;
    size_t vertex_count;
    size_t batch_count;
    size_t triangle_count;
    size_t part_count;
    size_t character_part_count;
    size_t gun_part_count;
    size_t initial_character_part_count;
    size_t initial_gun_part_count;
    size_t initial_part_capacity;
    size_t allocated_part_capacity;
    uint32_t animation_timer;
    uint8_t muzzle_flash_visible;
    uint8_t render_prop_type;
    uint8_t render_zbuffer_enabled;
    uint8_t render_cull_mode;
    uint8_t render_primary_flags;
    uint8_t render_secondary_flags;
    uint8_t shadow_alpha;
    uint8_t viewer_uses_vertex_alpha_lighting;
    uint32_t render_environment_rgba;
    uint32_t render_fog_rgba;
} GeOriginalGunbarrelBondScene;

/* Owns exact ROM-backed CdjbondZ/CheadbrosnanZ/PROP_CHRWPPK instances.
 * The provider and renderer boundaries are portable; no fallback geometry is
 * produced when any canonical resource is unavailable. */
GeOriginalGunbarrelBond *ge_original_gunbarrel_bond_create(
    GeAssetPack *asset_pack, GeOriginalGunbarrelBondStatus *status);
void ge_original_gunbarrel_bond_destroy(GeOriginalGunbarrelBond *bond);

/* Replays initializeGunBarrelIntro's exact model/animation initialization. */
GeOriginalGunbarrelBondStatus ge_original_gunbarrel_bond_reset(
    GeOriginalGunbarrelBond *bond);

/* Applies the events already emitted by ge_original_gunbarrel_tick, then
 * executes the unchanged per-tick modelTickAnim body followed by subcalcpos,
 * body subcalcmatrices and attached-gun instcalcmatrices in source order. */
GeOriginalGunbarrelBondStatus ge_original_gunbarrel_bond_tick(
    GeOriginalGunbarrelBond *bond,
    const GeOriginalGunbarrelFrame *frame,
    GeOriginalGunbarrelBondScene *scene);

const char *ge_original_gunbarrel_bond_status_name(
    GeOriginalGunbarrelBondStatus status);

#endif
