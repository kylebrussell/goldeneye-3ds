#ifndef GE_ORIGINAL_STAGE_PADLOCK_RUNTIME_H
#define GE_ORIGINAL_STAGE_PADLOCK_RUNTIME_H

#include "ge_original_stage_setup.h"

typedef struct GeOriginalStagePadlockRuntime {
    void *links;
    size_t relation_count;
    size_t omitted_count;
} GeOriginalStagePadlockRuntime;

/* Publish the canonical PROPDEF_LOCK_DOOR list after both endpoints have been
 * constructed. Missing endpoints are omitted just as in proplvreset2. The
 * zero-initialized runtime owns native-width LockDoorRecords until close. */
int ge_original_stage_padlock_runtime_materialize(
    GeOriginalStagePadlockRuntime *runtime,
    const GeOriginalStageSetupRuntime *setup, void *context,
    void *(*definition_by_command)(void *context, size_t command_index));

/* Must run before the endpoint objects are destroyed. */
void ge_original_stage_padlock_runtime_close(
    GeOriginalStagePadlockRuntime *runtime);

/* Direct door-interaction provider; context is this bound runtime. Calls the
 * unchanged doorIsPadlockFree health check against every authored lock. */
int ge_original_stage_padlock_runtime_is_free(void *context, void *definition);

#endif
