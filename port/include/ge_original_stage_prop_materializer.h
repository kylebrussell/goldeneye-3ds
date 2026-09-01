#ifndef GE_ORIGINAL_STAGE_PROP_MATERIALIZER_H
#define GE_ORIGINAL_STAGE_PROP_MATERIALIZER_H

#include "ge_original_stage_setup.h"

#include <stddef.h>
#include <stdint.h>

typedef enum GeOriginalStagePropService {
    GE_ORIGINAL_STAGE_PROP_SERVICE_CONTROL = 0,
    GE_ORIGINAL_STAGE_PROP_SERVICE_DEFAULT_OBJECT,
    GE_ORIGINAL_STAGE_PROP_SERVICE_DOOR,
    GE_ORIGINAL_STAGE_PROP_SERVICE_GUARD,
    GE_ORIGINAL_STAGE_PROP_SERVICE_ITEM,
    GE_ORIGINAL_STAGE_PROP_SERVICE_SPECIAL_OBJECT,
    GE_ORIGINAL_STAGE_PROP_SERVICE_COUNT
} GeOriginalStagePropService;

enum {
    GE_ORIGINAL_STAGE_PROP_CAP_DEFAULT_OBJECT = 1U << 0,
    GE_ORIGINAL_STAGE_PROP_CAP_DOOR = 1U << 1,
    GE_ORIGINAL_STAGE_PROP_CAP_GUARD = 1U << 2,
    GE_ORIGINAL_STAGE_PROP_CAP_ITEM = 1U << 3,
    GE_ORIGINAL_STAGE_PROP_CAP_TINTED_GLASS = 1U << 4,
    GE_ORIGINAL_STAGE_PROP_CAP_MONITOR = 1U << 5,
    GE_ORIGINAL_STAGE_PROP_CAP_SECURITY = 1U << 6,
    GE_ORIGINAL_STAGE_PROP_CAP_SUPPLY = 1U << 7,
    GE_ORIGINAL_STAGE_PROP_CAP_MISC_OBJECT = 1U << 8,
    /* Safe construction/runtime is independently closable from the moving,
     * hazardous and interactive misc-object families. */
    GE_ORIGINAL_STAGE_PROP_CAP_SAFE = 1U << 9,
    /* Alarm switches use the exact misc-object constructor but have an
     * independently closable interaction/runtime boundary. */
    GE_ORIGINAL_STAGE_PROP_CAP_ALARM = 1U << 10,
    /* Gas-releasing objects use the common original object constructor, but
     * their destruction enters the retained toxic-gas damage/fog service.
     * Keep this independent from vehicles, aircraft, tanks and racks so a
     * caller cannot accidentally advertise those moving-object services. */
    GE_ORIGINAL_STAGE_PROP_CAP_GAS_RELEASING = 1U << 11,
    /* CCTV and autoguns share the setup security family, but autoguns also
     * require beam allocation/rendering, gunfire SFX and player damage.  Keep
     * their live-runtime claims independent while accepting the older broad
     * SECURITY bit as a compatibility capability in host audits. */
    GE_ORIGINAL_STAGE_PROP_CAP_CCTV = 1U << 12,
    GE_ORIGINAL_STAGE_PROP_CAP_AUTOGUN = 1U << 13,
};

typedef enum GeOriginalStagePropBlocker {
    GE_ORIGINAL_STAGE_PROP_READY = 0,
    GE_ORIGINAL_STAGE_PROP_CONTROL_ONLY,
    GE_ORIGINAL_STAGE_PROP_MISSING_SERVICE,
    GE_ORIGINAL_STAGE_PROP_MISSING_MODEL,
    GE_ORIGINAL_STAGE_PROP_UNSUPPORTED_BRANCH
} GeOriginalStagePropBlocker;

typedef struct GeOriginalStagePropClassification {
    GeOriginalStagePropService service;
    GeOriginalStagePropBlocker blocker;
} GeOriginalStagePropClassification;

typedef struct GeOriginalStagePropConstructionRequest {
    const GeOriginalStageSetupRuntime *runtime;
    const GeOriginalStagePropRecord *record;
    size_t command_index;
    GeOriginalStagePropService service;
    int32_t model_id;
    int32_t pad_id;
    uint32_t flags;
    uint32_t flags2;
    uint32_t runtime_flags;
    GeOriginalStagePadPlacement placement;
    uint8_t placement_resolved;
} GeOriginalStagePropConstructionRequest;

typedef int (*GeOriginalStagePropConstructCallback)(
    void *context, const GeOriginalStagePropConstructionRequest *request);

typedef struct GeOriginalStagePropMaterializerProviders {
    void *context;
    uint32_t capabilities;
    int (*model_available)(void *context, int32_t model_id);
    /* Service-specific callbacks are the preferred runtime boundary. They
     * receive the exact authored model and relocated pad/STAN placement. The
     * legacy generic callback remains as a compatibility fallback. */
    GeOriginalStagePropConstructCallback construct_default_object;
    GeOriginalStagePropConstructCallback construct_door;
    GeOriginalStagePropConstructCallback construct_guard;
    GeOriginalStagePropConstructCallback construct_item;
    GeOriginalStagePropConstructCallback construct_special_object;
    int (*construct)(void *context, GeOriginalStagePropService service,
                     const GeOriginalStagePropRecord *record,
                     size_t command_index);
} GeOriginalStagePropMaterializerProviders;

typedef struct GeOriginalStagePropMaterializerReport {
    size_t records;
    size_t service_counts[GE_ORIGINAL_STAGE_PROP_SERVICE_COUNT];
    size_t ready;
    size_t constructed;
    size_t control_only;
    size_t missing_service;
    size_t missing_model;
    size_t unsupported_branch;
    size_t failed;
} GeOriginalStagePropMaterializerReport;

int ge_original_stage_prop_construction_request(
    const GeOriginalStageSetupRuntime *runtime, size_t command_index,
    GeOriginalStagePropConstructionRequest *request);

/* Native ABI bridge for the common object record plus the exact PROP/GLASS,
 * DOOR, KEY, COLLECTABLE, and HAT tails. The returned size is zero for
 * branches whose complete canonical native tail is not closed yet.
 * Initialisation preserves authored fields while leaving runtime-owned
 * pointers and matrices zeroed for their canonical constructors. */
size_t ge_original_stage_prop_native_definition_size(
    const GeOriginalStagePropConstructionRequest *request);
int ge_original_stage_prop_native_definition_init(
    const GeOriginalStagePropConstructionRequest *request,
    void *definition, size_t definition_size);
int ge_original_stage_prop_native_definition_header(
    const void *definition, uint16_t *extrascale,
    uint8_t *state, uint8_t *type);
int ge_original_stage_prop_native_definition_set_state(
    void *definition, uint8_t state);
size_t ge_original_stage_prop_native_prop_size(void);
int ge_original_stage_prop_native_bind_prop(
    const GeOriginalStagePropConstructionRequest *request,
    void *definition, void *prop, size_t prop_size);

GeOriginalStagePropClassification ge_original_stage_prop_classify(
    const GeOriginalStagePropRecord *record,
    const GeOriginalStagePropMaterializerProviders *providers);

int ge_original_stage_prop_materialize_ready(
    const GeOriginalStageSetupRuntime *runtime,
    const GeOriginalStagePropMaterializerProviders *providers,
    GeOriginalStagePropMaterializerReport *report);

/* Returns the number of unique authored model IDs required by a service.
 * IDs are emitted in first-record order; callers may pass NULL/zero to query
 * the required capacity without truncating the count. */
size_t ge_original_stage_prop_model_dependencies(
    const GeOriginalStageSetupRuntime *runtime,
    GeOriginalStagePropService service, int32_t *model_ids,
    size_t model_capacity);

const char *ge_original_stage_prop_service_name(
    GeOriginalStagePropService service);
const char *ge_original_stage_prop_blocker_name(
    GeOriginalStagePropBlocker blocker);

#endif
