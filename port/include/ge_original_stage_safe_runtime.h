#ifndef GE_ORIGINAL_STAGE_SAFE_RUNTIME_H
#define GE_ORIGINAL_STAGE_SAFE_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

typedef enum GeOriginalStageSafeRuntimeStatus {
    GE_ORIGINAL_STAGE_SAFE_RUNTIME_OK = 0,
    GE_ORIGINAL_STAGE_SAFE_RUNTIME_INVALID_ARGUMENT,
    GE_ORIGINAL_STAGE_SAFE_RUNTIME_NOT_BOUND,
    GE_ORIGINAL_STAGE_SAFE_RUNTIME_INVALID_RELATION,
    GE_ORIGINAL_STAGE_SAFE_RUNTIME_DUPLICATE_RELATION
} GeOriginalStageSafeRuntimeStatus;

typedef struct GeOriginalStageSafeRuntime {
    void *head;
    size_t relation_count;
    uint64_t generation;
    uint64_t pickup_tests;
    uint64_t blocked_pickups;
    GeOriginalStageSafeRuntimeStatus status;
    uint8_t bound;
} GeOriginalStageSafeRuntime;

/* Owns the canonical g_LevelLoadPropSafeItem list for one active stage. */
void ge_original_stage_safe_runtime_bind(GeOriginalStageSafeRuntime *runtime);
void ge_original_stage_safe_runtime_close(GeOriginalStageSafeRuntime *runtime);

/* Direct GeOriginalStageSafeItemProviders::register_relation callback. The
 * already-resolved relation is linked at the canonical list head through the
 * unchanged initSetLevelLoadPropSafeItem body. */
int ge_original_stage_safe_runtime_register_relation(
    void *context, void *relation);

/* Calls the unchanged objCanPickupFromSafe body. A linked item remains blocked
 * until its exact authored safe door is more than half open. */
int ge_original_stage_safe_runtime_can_pickup(
    GeOriginalStageSafeRuntime *runtime, void *object_definition);

const char *ge_original_stage_safe_runtime_status_name(
    GeOriginalStageSafeRuntimeStatus status);

#endif
