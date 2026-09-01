#ifndef GE_ORIGINAL_STAGE_SETUP_H
#define GE_ORIGINAL_STAGE_SETUP_H

#include "ge_asset_pack.h"
#include "ge_stage_assets.h"
#include "ge_stan_native.h"

#include <stddef.h>
#include <stdint.h>

typedef struct stagesetup stagesetup;
typedef struct CreditsEntry_s CreditsEntry;

enum {
    GE_ORIGINAL_STAGE_SETUP_PADS = 1U << 0,
    GE_ORIGINAL_STAGE_SETUP_BOUNDPADS = 1U << 1,
    GE_ORIGINAL_STAGE_SETUP_INTRO = 1U << 2,
    GE_ORIGINAL_STAGE_SETUP_PATHS = 1U << 3,
    GE_ORIGINAL_STAGE_SETUP_AI = 1U << 4,
    GE_ORIGINAL_STAGE_SETUP_PROPDEFS = 1U << 5,
};

typedef struct GeOriginalStagePropRecord GeOriginalStagePropRecord;

/* One entry per authored setup command. `words` is the endian-relocated exact
 * N64 command ABI. Pointer-bearing command relationships live in this native
 * sidecar because host pointers are wider than the serialized 32-bit slots. */
struct GeOriginalStagePropRecord {
    uint32_t *words;
    size_t source_offset;
    size_t word_count;
    GeOriginalStagePropRecord *relations[3];
    GeOriginalStagePropRecord *objective;
    int32_t relation_offsets[3];
    int32_t model_id;
    int32_t pad_id;
    int32_t chr_id;
    int32_t ai_list_id;
    uint16_t tag_id;
    uint8_t relation_count;
    uint8_t type;
};

typedef struct GeOriginalStageSetupRuntime {
    const GeStageAssetDescriptor *descriptor;
    stagesetup *setup;
    uint8_t *source_blob;
    size_t source_size;
    void *pads_storage;
    void *boundpads_storage;
    int *intro_storage;
    CreditsEntry *credits_storage;
    void *waypoints_storage;
    void *waygroups_storage;
    void *patrolpaths_storage;
    void *ailists_storage;
    uint32_t *propdefs_storage;
    GeOriginalStagePropRecord *prop_records;
    void **list_storage;
    size_t list_storage_count;
    size_t list_storage_capacity;
    size_t pad_count;
    size_t boundpad_count;
    size_t intro_word_count;
    size_t credits_count;
    size_t waypoint_count;
    size_t waygroup_count;
    size_t patrolpath_count;
    size_t ailist_count;
    size_t prop_word_count;
    size_t prop_record_count;
    size_t bound_pad_stan_count;
    size_t pad_stan_count;
    uint64_t source_fnv1a64;
    uint32_t relocated;
    uint8_t loaded;
} GeOriginalStageSetupRuntime;

typedef struct GeOriginalStageSpawn {
    int32_t pad_id;
    float position[3];
    float up[3];
    float look[3];
    const char *plink;
    void *stan;
} GeOriginalStageSpawn;

typedef struct GeOriginalStagePadPlacement {
    int32_t pad_id;
    int16_t room;
    uint8_t is_bound_pad;
    uint8_t has_stan;
    float position[3];
    float up[3];
    float look[3];
    float bounds[6];
    const char *plink;
    void *stan;
} GeOriginalStagePadPlacement;

typedef struct GeOriginalStagePathAudit {
    size_t waypoint_count;
    size_t valid_waypoints;
    size_t first_invalid_waypoint;
    int32_t first_invalid_pad_id;
    int32_t first_invalid_group;
} GeOriginalStagePathAudit;

typedef enum GeOriginalStageSetupStatus {
    GE_ORIGINAL_STAGE_SETUP_OK = 0,
    GE_ORIGINAL_STAGE_SETUP_INVALID_ARGUMENT,
    GE_ORIGINAL_STAGE_SETUP_ASSET_NOT_FOUND,
    GE_ORIGINAL_STAGE_SETUP_INVALID_ASSET,
    GE_ORIGINAL_STAGE_SETUP_NO_MEMORY,
    GE_ORIGINAL_STAGE_SETUP_STAN_UNRESOLVED
} GeOriginalStageSetupStatus;

GeOriginalStageSetupStatus ge_original_stage_setup_load(
    GeAssetPack *pack, const GeStageAssetDescriptor *descriptor,
    GeOriginalStageSetupRuntime *runtime);

GeOriginalStageSetupStatus ge_original_stage_setup_bind_stan(
    GeOriginalStageSetupRuntime *runtime, const GeStanNativeMap *stan);

int ge_original_stage_setup_normal_spawn(
    GeOriginalStageSetupRuntime *runtime, GeOriginalStageSpawn *spawn);
/* Returns the endian-relocated INTROTYPE_CREDITS table when the authored
 * setup has one (Cuba). The terminating zero entry is retained in storage but
 * excluded from count, matching bondviewRenderCredits' traversal contract. */
const CreditsEntry *ge_original_stage_setup_credits(
    const GeOriginalStageSetupRuntime *runtime, size_t *count);

/* Resolves an authored ordinary or 10000-based bound pad from the relocated
 * setup. This is the platform-neutral placement boundary used by generic prop
 * constructors; no coordinates or room identity are synthesized. */
int ge_original_stage_setup_pad_placement(
    const GeOriginalStageSetupRuntime *runtime, int32_t pad_id,
    GeOriginalStagePadPlacement *placement);

stagesetup *ge_original_stage_setup_get(
    GeOriginalStageSetupRuntime *runtime);
/* Publishes the fully relocated setup at the same global boundary used by
 * lvlStageLoad before canonical object/AI constructors run. */
int ge_original_stage_setup_publish(
    const GeOriginalStageSetupRuntime *runtime);
/* Reports which relocated setup tables still match g_CurrentSetup. */
uint32_t ge_original_stage_setup_publication_mask(
    const GeOriginalStageSetupRuntime *runtime);
int ge_original_stage_setup_path_audit(
    const GeOriginalStageSetupRuntime *runtime,
    GeOriginalStagePathAudit *audit);
int ge_original_stage_setup_active_path_valid(void);
const GeOriginalStagePropRecord *ge_original_stage_setup_prop_record(
    const GeOriginalStageSetupRuntime *runtime, size_t index);
size_t ge_original_stage_setup_prop_type_count(
    const GeOriginalStageSetupRuntime *runtime, uint8_t type);
void ge_original_stage_setup_close(GeOriginalStageSetupRuntime *runtime);
const char *ge_original_stage_setup_status_name(
    GeOriginalStageSetupStatus status);

#endif
