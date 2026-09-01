#ifndef GE_ORIGINAL_STAGE_SPECIAL_OBJECTS_H
#define GE_ORIGINAL_STAGE_SPECIAL_OBJECTS_H

#include "ge_original_stage_setup.h"

#include <stddef.h>
#include <stdint.h>

typedef struct GeOriginalStagePropConstructionRequest
    GeOriginalStagePropConstructionRequest;

enum { GE_ORIGINAL_STAGE_SPECIAL_TYPE_COUNT = 17 };

typedef struct GeOriginalStageSpecialTypeAudit {
    uint8_t type;
    size_t total;
    size_t stage_count;
    size_t by_stage[GE_STAGE_COUNT];
} GeOriginalStageSpecialTypeAudit;

typedef struct GeOriginalStageSpecialAudit {
    GeOriginalStageSpecialTypeAudit types[GE_ORIGINAL_STAGE_SPECIAL_TYPE_COUNT];
    size_t total;
} GeOriginalStageSpecialAudit;

/* Adds one exact relocated stage setup to the campaign-wide service-gap
 * inventory. Ordinary objects, doors, guards, items and control records are
 * deliberately excluded. */
int ge_original_stage_special_audit_add(
    GeOriginalStageSpecialAudit *audit, GeStageId stage,
    const GeOriginalStageSetupRuntime *setup);

int ge_original_stage_special_type_index(uint8_t type);
const char *ge_original_stage_special_type_name(uint8_t type);

typedef struct GeOriginalStageTintedGlassProviders {
    void *context;
    int32_t (*find_portal)(void *context, const float point_a[3],
                           const float point_b[3]);
    int (*construct_standard)(void *context, void *definition,
                              int32_t command_index);
    int (*place_standard)(void *context, void *definition);
} GeOriginalStageTintedGlassProviders;

typedef enum GeOriginalStageTintedGlassStatus {
    GE_ORIGINAL_STAGE_TINTED_GLASS_OK = 0,
    GE_ORIGINAL_STAGE_TINTED_GLASS_INVALID_ARGUMENT,
    GE_ORIGINAL_STAGE_TINTED_GLASS_INVALID_DEFINITION,
    GE_ORIGINAL_STAGE_TINTED_GLASS_PLACEMENT_UNRESOLVED,
    GE_ORIGINAL_STAGE_TINTED_GLASS_MISSING_PORTAL_SERVICE,
    GE_ORIGINAL_STAGE_TINTED_GLASS_CONSTRUCTION_FAILED,
    GE_ORIGINAL_STAGE_TINTED_GLASS_PLACEMENT_FAILED,
} GeOriginalStageTintedGlassStatus;

/* Runs the exact PROPDEF_TINTED_GLASS setup branch before continuing through
 * the unchanged standard-object construct/place callbacks. The portal ray is
 * derived solely from the authored bound pad, matching proplvreset2. */
GeOriginalStageTintedGlassStatus ge_original_stage_tinted_glass_construct(
    const GeOriginalStagePropConstructionRequest *request,
    void *definition, size_t definition_size,
    const GeOriginalStageTintedGlassProviders *providers);
const char *ge_original_stage_tinted_glass_status_name(
    GeOriginalStageTintedGlassStatus status);

enum {
    GE_ORIGINAL_STAGE_MISC_DEP_DEFAULT_OBJECT = 1U << 0,
    GE_ORIGINAL_STAGE_MISC_DEP_PROP_PUBLICATION = 1U << 1,
    GE_ORIGINAL_STAGE_MISC_DEP_RACK_MATRICES = 1U << 2,
    GE_ORIGINAL_STAGE_MISC_DEP_ALARM_INTERACTION = 1U << 3,
    GE_ORIGINAL_STAGE_MISC_DEP_GAS_DAMAGE_EFFECT = 1U << 4,
    GE_ORIGINAL_STAGE_MISC_DEP_AI_LIST = 1U << 5,
    GE_ORIGINAL_STAGE_MISC_DEP_OBJECT_ACTION_TICK = 1U << 6,
    GE_ORIGINAL_STAGE_MISC_DEP_MOVING_MODEL_MATRICES = 1U << 7,
    GE_ORIGINAL_STAGE_MISC_DEP_OBJECT_AUDIO = 1U << 8,
    GE_ORIGINAL_STAGE_MISC_DEP_TANK_PROJECTILE = 1U << 9,
    GE_ORIGINAL_STAGE_MISC_DEP_TANK_FLOOR_COLLISION = 1U << 10,
    GE_ORIGINAL_STAGE_MISC_DEP_SAFE_RELATION = 1U << 11,
};

typedef struct GeOriginalStageMiscProviders {
    void *context;
    int (*construct_standard)(void *context, void *definition,
                              int32_t command_index);
    int (*place_standard)(void *context, void *definition);
    int (*update_room_position)(void *context, void *definition);
    int (*activate_prop)(void *context, void *prop);
    int (*enable_prop)(void *context, void *prop);
    int (*resolve_ai_list)(void *context, int32_t list_id,
                           void **resolved_list);
    int (*set_model_switch)(void *context, void *model,
                            uint32_t switch_index, int enabled);
    int (*load_tank_projectiles)(void *context);
    int (*get_floor_y)(void *context, void *stan, float x, float z,
                       float *floor_y);
} GeOriginalStageMiscProviders;

typedef enum GeOriginalStageMiscStatus {
    GE_ORIGINAL_STAGE_MISC_OK = 0,
    GE_ORIGINAL_STAGE_MISC_INVALID_ARGUMENT,
    GE_ORIGINAL_STAGE_MISC_INVALID_DEFINITION,
    GE_ORIGINAL_STAGE_MISC_UNSUPPORTED_TYPE,
    GE_ORIGINAL_STAGE_MISC_UNSUPPORTED_OWNERSHIP,
    GE_ORIGINAL_STAGE_MISC_PLACEMENT_UNRESOLVED,
    GE_ORIGINAL_STAGE_MISC_MISSING_AI_LIST,
    GE_ORIGINAL_STAGE_MISC_MISSING_SWITCH_SERVICE,
    GE_ORIGINAL_STAGE_MISC_MISSING_PROJECTILE_SERVICE,
    GE_ORIGINAL_STAGE_MISC_MISSING_FLOOR_SERVICE,
    GE_ORIGINAL_STAGE_MISC_CONSTRUCTION_FAILED,
    GE_ORIGINAL_STAGE_MISC_PLACEMENT_FAILED,
    GE_ORIGINAL_STAGE_MISC_ACTIVATION_FAILED,
} GeOriginalStageMiscStatus;

typedef struct GeOriginalStageMiscInstance {
    void *definition;
    void *prop;
    void *model;
    size_t command_index;
    uint32_t runtime_dependencies;
    uint8_t type;
    uint8_t constructed;
    uint8_t activated;
} GeOriginalStageMiscInstance;

/* Exact selected-record setup continuation for alarm/rack/gas/safe and the
 * vehicle, aircraft and tank records. This closes construction only; the
 * returned dependency mask names the unchanged runtime services still needed
 * before a caller may tick or interact with the object. */
GeOriginalStageMiscStatus ge_original_stage_misc_construct_exact(
    const GeOriginalStagePropConstructionRequest *request,
    void *definition, size_t definition_size,
    const GeOriginalStageMiscProviders *providers,
    GeOriginalStageMiscInstance *instance);
uint32_t ge_original_stage_misc_runtime_dependencies(uint8_t type);
const char *ge_original_stage_misc_status_name(
    GeOriginalStageMiscStatus status);

typedef struct GeOriginalStageSafeItemProviders {
    void *context;
    void *(*find_definition)(void *context, size_t command_index);
    int (*register_relation)(void *context, void *relation);
} GeOriginalStageSafeItemProviders;

/* Exact second-pass PROPDEF_SAFE_ITEM relation. The command-relative authored
 * indices are resolved only to already-live item/safe/door definitions. */
GeOriginalStageMiscStatus ge_original_stage_safe_item_link_exact(
    const GeOriginalStagePropConstructionRequest *request,
    void *definition, size_t definition_size,
    const GeOriginalStageSafeItemProviders *providers);

#endif
