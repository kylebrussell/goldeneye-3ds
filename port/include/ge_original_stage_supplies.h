#ifndef GE_ORIGINAL_STAGE_SUPPLIES_H
#define GE_ORIGINAL_STAGE_SUPPLIES_H

#include "ge_original_default_object.h"
#include "ge_original_stage_prop_materializer.h"

#include <stddef.h>
#include <stdint.h>

typedef struct GeOriginalStageSupplyProviders {
    const GeOriginalDefaultObjectProviders *default_object;
    GeOriginalDefaultObjectPrepared *prepared;
    /* proplvreset2 reads the selected multiplayer weapon-set record only when
     * getPlayerCount() >= 2. The callback returns its exact ammo type/amount. */
    int (*get_multiplayer_ammo)(void *context, int32_t *ammo_type,
                                int32_t *ammo_amount);
    /* Exact tail of domakedefaultobj after moveToPad: room publication,
     * chrpropActivate, then chrpropEnable, in this order. */
    int (*update_room_position)(void *context, void *definition);
    int (*activate_prop)(void *context, void *prop);
    int (*enable_prop)(void *context, void *prop);
} GeOriginalStageSupplyProviders;

typedef enum GeOriginalStageSupplyStatus {
    GE_ORIGINAL_STAGE_SUPPLY_OK = 0,
    GE_ORIGINAL_STAGE_SUPPLY_INVALID_ARGUMENT,
    GE_ORIGINAL_STAGE_SUPPLY_INVALID_DEFINITION,
    GE_ORIGINAL_STAGE_SUPPLY_UNSUPPORTED_OWNERSHIP,
    GE_ORIGINAL_STAGE_SUPPLY_MULTIPLAYER_DEPENDENCY_UNAVAILABLE,
    GE_ORIGINAL_STAGE_SUPPLY_MULTIPLAYER_AMMO_INVALID,
    GE_ORIGINAL_STAGE_SUPPLY_SLOT_MODEL_UNAVAILABLE,
    GE_ORIGINAL_STAGE_SUPPLY_CANONICAL_NO_OBJECT,
    GE_ORIGINAL_STAGE_SUPPLY_CONSTRUCTION_FAILED,
    GE_ORIGINAL_STAGE_SUPPLY_PLACEMENT_FAILED,
    GE_ORIGINAL_STAGE_SUPPLY_ACTIVATION_DEPENDENCY_UNAVAILABLE,
    GE_ORIGINAL_STAGE_SUPPLY_ACTIVATION_FAILED
} GeOriginalStageSupplyStatus;

typedef struct GeOriginalStageSupplyInstance {
    void *definition;
    void *prop;
    void *model;
    size_t command_index;
    size_t slot_model_loads;
    uint8_t type;
    uint8_t constructed;
    uint8_t activated;
} GeOriginalStageSupplyInstance;

/* Exact selected-record setup branch for PROPDEF_MAGAZINE, PROPDEF_AMMO and
 * PROPDEF_ARMOUR. Difficulty/player flags are evaluated by proplvreset2's
 * caller before this function. Negative-pad, embedded and assigned ownership
 * stay explicit until their canonical owner pass is supplied. */
GeOriginalStageSupplyStatus ge_original_stage_supply_construct_exact(
    const GeOriginalStagePropConstructionRequest *request,
    void *definition, size_t definition_size,
    void *prop, size_t prop_size,
    const GeOriginalStageSupplyProviders *providers,
    GeOriginalStageSupplyInstance *instance);

const char *ge_original_stage_supply_status_name(
    GeOriginalStageSupplyStatus status);

/* Owned native selection view; setup/pads/STAN/source_blob remain borrowed.
 * Close this BEFORE the source runtime. Never call stage_setup_close(view).
 * Publish source->setup; pass &prepared.view to dependency scans/materializers. */
typedef struct GeOriginalStageMultiplayerSetup {
    GeOriginalStageSetupRuntime view;
    GeOriginalStagePropRecord *records;
    uint32_t *words;
    size_t weapon_slots;
    size_t ammo_crates;
    size_t omitted;
    int32_t player_count;
    int32_t scenario;
    int32_t weapon_set;
} GeOriginalStageMultiplayerSetup;

GeOriginalStageSupplyStatus ge_original_stage_multiplayer_setup_prepare(
    const GeOriginalStageSetupRuntime *source, int32_t player_count,
    int32_t scenario, int32_t weapon_set,
    GeOriginalStageMultiplayerSetup *prepared);
void ge_original_stage_multiplayer_setup_close(
    GeOriginalStageMultiplayerSetup *prepared);

#endif
