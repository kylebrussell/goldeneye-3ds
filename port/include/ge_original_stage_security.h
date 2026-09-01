#ifndef GE_ORIGINAL_STAGE_SECURITY_H
#define GE_ORIGINAL_STAGE_SECURITY_H

#include "ge_original_stage_prop_materializer.h"

#include <stddef.h>
#include <stdint.h>

enum {
    GE_ORIGINAL_STAGE_SECURITY_ACTIVE_PROP_TICK = UINT32_C(1) << 0,
    GE_ORIGINAL_STAGE_SECURITY_TIMER = UINT32_C(1) << 1,
    GE_ORIGINAL_STAGE_SECURITY_PLAYER_PROP = UINT32_C(1) << 2,
    GE_ORIGINAL_STAGE_SECURITY_STAN_LINE = UINT32_C(1) << 3,
    GE_ORIGINAL_STAGE_SECURITY_MODEL_RELATIONS = UINT32_C(1) << 4,
    GE_ORIGINAL_STAGE_SECURITY_ALARM = UINT32_C(1) << 5,
    GE_ORIGINAL_STAGE_SECURITY_RANDOM = UINT32_C(1) << 6,
    GE_ORIGINAL_STAGE_SECURITY_AIM = UINT32_C(1) << 7,
    GE_ORIGINAL_STAGE_SECURITY_GUNFIRE_SFX = UINT32_C(1) << 8,
    GE_ORIGINAL_STAGE_SECURITY_PLAYER_DAMAGE = UINT32_C(1) << 9,
    GE_ORIGINAL_STAGE_SECURITY_BEAM_RENDER = UINT32_C(1) << 10,
    GE_ORIGINAL_STAGE_SECURITY_CAMERA_MATRICES = UINT32_C(1) << 11,
    GE_ORIGINAL_STAGE_SECURITY_OBJECT_DAMAGE = UINT32_C(1) << 12,
    GE_ORIGINAL_STAGE_SECURITY_DAMAGE_EFFECTS = UINT32_C(1) << 13,
    GE_ORIGINAL_STAGE_SECURITY_SOUND_LIFECYCLE = UINT32_C(1) << 14,
    GE_ORIGINAL_STAGE_SECURITY_LIGHTING_SHADOW = UINT32_C(1) << 15,
};

#define GE_ORIGINAL_STAGE_SECURITY_CCTV_REQUIRED \
    (GE_ORIGINAL_STAGE_SECURITY_ACTIVE_PROP_TICK \
     | GE_ORIGINAL_STAGE_SECURITY_TIMER \
     | GE_ORIGINAL_STAGE_SECURITY_PLAYER_PROP \
     | GE_ORIGINAL_STAGE_SECURITY_STAN_LINE \
     | GE_ORIGINAL_STAGE_SECURITY_MODEL_RELATIONS \
     | GE_ORIGINAL_STAGE_SECURITY_ALARM \
     | GE_ORIGINAL_STAGE_SECURITY_AIM \
     | GE_ORIGINAL_STAGE_SECURITY_CAMERA_MATRICES \
     | GE_ORIGINAL_STAGE_SECURITY_OBJECT_DAMAGE \
     | GE_ORIGINAL_STAGE_SECURITY_DAMAGE_EFFECTS \
     | GE_ORIGINAL_STAGE_SECURITY_LIGHTING_SHADOW)

#define GE_ORIGINAL_STAGE_SECURITY_AUTOGUN_REQUIRED \
    (GE_ORIGINAL_STAGE_SECURITY_ACTIVE_PROP_TICK \
     | GE_ORIGINAL_STAGE_SECURITY_TIMER \
     | GE_ORIGINAL_STAGE_SECURITY_PLAYER_PROP \
     | GE_ORIGINAL_STAGE_SECURITY_STAN_LINE \
     | GE_ORIGINAL_STAGE_SECURITY_MODEL_RELATIONS \
     | GE_ORIGINAL_STAGE_SECURITY_RANDOM \
     | GE_ORIGINAL_STAGE_SECURITY_AIM \
     | GE_ORIGINAL_STAGE_SECURITY_GUNFIRE_SFX \
     | GE_ORIGINAL_STAGE_SECURITY_PLAYER_DAMAGE \
     | GE_ORIGINAL_STAGE_SECURITY_BEAM_RENDER \
     | GE_ORIGINAL_STAGE_SECURITY_CAMERA_MATRICES \
     | GE_ORIGINAL_STAGE_SECURITY_OBJECT_DAMAGE \
     | GE_ORIGINAL_STAGE_SECURITY_DAMAGE_EFFECTS \
     | GE_ORIGINAL_STAGE_SECURITY_SOUND_LIFECYCLE \
     | GE_ORIGINAL_STAGE_SECURITY_LIGHTING_SHADOW)

typedef struct GeOriginalStageSecurityProviders {
    void *context;
    uint32_t runtime_capabilities;
    /* These callbacks are the existing exact default-object construct/place
     * boundary.  They must publish ObjectRecord::model and ::prop before the
     * CCTV/autogun-specific setup continuation runs. */
    int (*construct_standard)(void *context, void *definition,
                              int32_t command_index);
    int (*place_standard)(void *context, void *definition);
    int (*update_room_position)(void *context, void *definition);
    int (*activate_prop)(void *context, void *prop);
    int (*enable_prop)(void *context, void *prop);
    /* Exact setupAutogun allocates 0x30 bytes from MEMPOOL_STAGE for beam
     * state.  The returned allocation remains owned by the stage pool. */
    void *(*allocate_stage)(void *context, size_t size_bytes);
} GeOriginalStageSecurityProviders;

typedef enum GeOriginalStageSecurityStatus {
    GE_ORIGINAL_STAGE_SECURITY_OK = 0,
    GE_ORIGINAL_STAGE_SECURITY_INVALID_ARGUMENT,
    GE_ORIGINAL_STAGE_SECURITY_INVALID_DEFINITION,
    GE_ORIGINAL_STAGE_SECURITY_MISSING_RUNTIME_DEPENDENCY,
    GE_ORIGINAL_STAGE_SECURITY_CONSTRUCTION_FAILED,
    GE_ORIGINAL_STAGE_SECURITY_PLACEMENT_FAILED,
    GE_ORIGINAL_STAGE_SECURITY_ACTIVATION_DEPENDENCY_UNAVAILABLE,
    GE_ORIGINAL_STAGE_SECURITY_ACTIVATION_FAILED,
    GE_ORIGINAL_STAGE_SECURITY_MODEL_RELATION_UNAVAILABLE,
    GE_ORIGINAL_STAGE_SECURITY_LOOK_PAD_UNRESOLVED,
    GE_ORIGINAL_STAGE_SECURITY_BEAM_ALLOCATION_UNAVAILABLE,
    GE_ORIGINAL_STAGE_SECURITY_BEAM_ALLOCATION_FAILED
} GeOriginalStageSecurityStatus;

typedef struct GeOriginalStageSecurityInstance {
    const GeOriginalStagePropConstructionRequest *request;
    void *definition;
    void *prop;
    void *model;
    void *beam;
    uint32_t required_runtime_capabilities;
    uint32_t missing_runtime_capabilities;
    uint8_t type;
    uint8_t constructed;
    uint8_t runtime_ready;
} GeOriginalStageSecurityInstance;

typedef struct GeOriginalStageSecurityDependencyAudit {
    uint32_t tick_capabilities;
    uint32_t render_capabilities;
    uint32_t damage_capabilities;
    uint32_t cleanup_capabilities;
    uint32_t all_capabilities;
} GeOriginalStageSecurityDependencyAudit;

typedef struct GeOriginalStageSecurityModelAudit {
    uint32_t switch_present_mask;
    uint32_t switch_data_mask;
    uint32_t required_render_switch_mask;
    uint32_t optional_fire_switch_mask;
    int32_t num_switches;
    int32_t num_matrices;
    uint8_t render_relations_ready;
} GeOriginalStageSecurityModelAudit;

/* Runs the exact setupCctv or setupAutogun ordering: common default-object
 * construction/placement first, then the authored fixed-point conversion,
 * model relation/pad aim, and runtime-state initialization.  Tracking,
 * alarm, firing, damage, beam and SFX remain in the unchanged objTick body;
 * capabilities prevent activation when any of those services are absent. */
GeOriginalStageSecurityStatus ge_original_stage_security_construct(
    const GeOriginalStagePropConstructionRequest *request,
    void *definition, size_t definition_size,
    const GeOriginalStageSecurityProviders *providers,
    GeOriginalStageSecurityInstance *instance);

uint32_t ge_original_stage_security_required_capabilities(uint8_t type);
int ge_original_stage_security_dependency_audit(
    uint8_t type, GeOriginalStageSecurityDependencyAudit *audit);
int ge_original_stage_security_model_audit(
    const void *definition, GeOriginalStageSecurityModelAudit *audit);
const char *ge_original_stage_security_status_name(
    GeOriginalStageSecurityStatus status);

#endif
