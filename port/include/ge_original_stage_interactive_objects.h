#ifndef GE_ORIGINAL_STAGE_INTERACTIVE_OBJECTS_H
#define GE_ORIGINAL_STAGE_INTERACTIVE_OBJECTS_H

#include "ge_original_stage_prop_materializer.h"

#include <stddef.h>
#include <stdint.h>

typedef enum GeOriginalStageInteractiveBlocker {
    GE_ORIGINAL_STAGE_INTERACTIVE_READY = 0,
    GE_ORIGINAL_STAGE_INTERACTIVE_DIFFICULTY_FILTERED,
    GE_ORIGINAL_STAGE_INTERACTIVE_CANONICAL_NO_OBJECT,
    GE_ORIGINAL_STAGE_INTERACTIVE_INVALID_DEFINITION,
    GE_ORIGINAL_STAGE_INTERACTIVE_PLACEMENT_UNRESOLVED,
    GE_ORIGINAL_STAGE_INTERACTIVE_CANONICAL_NO_STAN,
    GE_ORIGINAL_STAGE_INTERACTIVE_ASSIGNED_ITEM_SERVICE,
    GE_ORIGINAL_STAGE_INTERACTIVE_EMBEDDED_ITEM_SERVICE,
    GE_ORIGINAL_STAGE_INTERACTIVE_MODEL_UNAVAILABLE,
    GE_ORIGINAL_STAGE_INTERACTIVE_PROJECTILE_MODEL_SERVICE,
    GE_ORIGINAL_STAGE_INTERACTIVE_DEFAULT_OBJECT_SERVICE,
    GE_ORIGINAL_STAGE_INTERACTIVE_DOOR_CONSTRUCTION_SERVICE,
    GE_ORIGINAL_STAGE_INTERACTIVE_CONSTRUCTION_FAILED,
    GE_ORIGINAL_STAGE_INTERACTIVE_DOOR_LINK_SERVICE,
    GE_ORIGINAL_STAGE_INTERACTIVE_INVALID_DOOR_RELATION,
    /* A negative pad without INSIDEANOTHEROBJ would enter domakedefaultobj's
     * ordinary-pad array branch.  It is not an embedded owner relation and is
     * kept distinct instead of inventing placement. */
    GE_ORIGINAL_STAGE_INTERACTIVE_INVALID_NEGATIVE_PAD,
    GE_ORIGINAL_STAGE_INTERACTIVE_BLOCKER_COUNT
} GeOriginalStageInteractiveBlocker;

typedef struct GeOriginalStageInteractiveEntry {
    const GeOriginalStagePropRecord *record;
    size_t command_index;
    size_t definition_size;
    void *definition;
    void *prop;
    void *model_instance;
    int32_t linked_entry;
    int32_t model_id;
    int32_t pad_id;
    int32_t owner_command_index;
    int16_t room;
    int8_t weapon_id;
    uint8_t type;
    uint8_t constructed;
    uint8_t externally_owned;
    GeOriginalStageInteractiveBlocker blocker;
} GeOriginalStageInteractiveEntry;

typedef struct GeOriginalStageInteractiveProviders {
    void *context;
    uint8_t difficulty;
    uint8_t player_count;
    int (*model_available)(void *context, int32_t model_id);
    int (*load_projectile_models)(void *context, int8_t weapon_id);
    int (*construct_default_object)(
        void *context, const GeOriginalStagePropConstructionRequest *request,
        void *definition, size_t definition_size,
        void **prop, void **model_instance);
    int (*construct_door)(
        void *context, const GeOriginalStagePropConstructionRequest *request,
        void *definition, size_t definition_size,
        void **prop, void **model_instance);
    /* Exact weaponAssignToHome/setupHat ownership is already constructed by
     * the resident guard runtime.  This callback adopts that original prop
     * graph by authored command index without constructing a second object. */
    int (*resolve_assigned_item)(
        void *context, const GeOriginalStagePropConstructionRequest *request,
        void **prop, void **model_instance);
    /* Exact INSIDEANOTHEROBJ branch plus the later setup-command owner
     * reparent pass. owner_command_index is command_index + authored pad. */
    int (*construct_embedded_item)(
        void *context, const GeOriginalStagePropConstructionRequest *request,
        int32_t owner_command_index, void *definition, size_t definition_size,
        void **prop, void **model_instance);
    int (*link_doors)(void *context, void *first_definition,
                      void *second_definition);
    void (*release_object)(void *context, void *definition,
                           void *prop, void *model_instance);
} GeOriginalStageInteractiveProviders;

typedef struct GeOriginalStageInteractiveReport {
    size_t records;
    size_t door_records;
    /* Doors which unchanged proplvreset2/setupDoor will actually construct
     * for this difficulty/player selection. setupDoor returns without an
     * object when the authored bound pad has no STAN. */
    size_t expected_door_constructions;
    size_t canonical_skipped_door_no_stan;
    size_t constructed_doors;
    size_t key_records;
    size_t collectable_records;
    size_t hat_records;
    size_t expected_item_constructions;
    size_t constructed_items;
    size_t definitions;
    size_t definition_bytes;
    size_t constructed;
    size_t linked_door_pairs;
    size_t blocker_counts[GE_ORIGINAL_STAGE_INTERACTIVE_BLOCKER_COUNT];
} GeOriginalStageInteractiveReport;

typedef struct GeOriginalStageInteractiveRuntime {
    const GeOriginalStageSetupRuntime *setup;
    GeOriginalStageInteractiveProviders providers;
    GeOriginalStageInteractiveEntry *entries;
    size_t entry_count;
    GeOriginalStageInteractiveReport report;
    uint8_t loaded;
} GeOriginalStageInteractiveRuntime;

/* Materializes the exact native setup definitions and, when canonical service
 * callbacks are present, constructs their live object/model instances. Missing
 * canonical branches remain explicit per-entry blockers. */
int ge_original_stage_interactive_materialize(
    const GeOriginalStageSetupRuntime *setup,
    const GeOriginalStageInteractiveProviders *providers,
    GeOriginalStageInteractiveRuntime *runtime);

const GeOriginalStageInteractiveEntry *ge_original_stage_interactive_entry(
    const GeOriginalStageInteractiveRuntime *runtime, size_t index);
size_t ge_original_stage_interactive_expected_door_count(
    const GeOriginalStageInteractiveRuntime *runtime);
size_t ge_original_stage_interactive_expected_item_count(
    const GeOriginalStageInteractiveRuntime *runtime);
size_t ge_original_stage_interactive_live_item_count(
    const GeOriginalStageInteractiveRuntime *runtime);
/* Enumerates live KEY/COLLECTABLE/HAT props in exact authored command order.
 * Assigned guard items are included but remain externally owned. */
int ge_original_stage_interactive_active_item(
    const GeOriginalStageInteractiveRuntime *runtime, size_t active_index,
    size_t *command_index, void **prop);
/* Enumerates only independently active item roots. Embedded owner children and
 * guard-owned assigned items remain available through active_item above, but
 * must not be linked into g_ActiveProps because those links share prev/next. */
size_t ge_original_stage_interactive_root_item_count(
    const GeOriginalStageInteractiveRuntime *runtime);
int ge_original_stage_interactive_root_item(
    const GeOriginalStageInteractiveRuntime *runtime, size_t root_index,
    size_t *command_index, void **prop);
void ge_original_stage_interactive_close(
    GeOriginalStageInteractiveRuntime *runtime);
const char *ge_original_stage_interactive_blocker_name(
    GeOriginalStageInteractiveBlocker blocker);

#endif
