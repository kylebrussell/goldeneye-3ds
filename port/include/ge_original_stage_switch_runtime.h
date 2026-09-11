#ifndef GE_ORIGINAL_STAGE_SWITCH_RUNTIME_H
#define GE_ORIGINAL_STAGE_SWITCH_RUNTIME_H

#include "ge_original_stage_setup.h"

typedef struct GeOriginalStageSwitchRuntime {
    void *links;
    size_t relation_count;
    size_t omitted_count;
} GeOriginalStageSwitchRuntime;

/* Publish the canonical PROPDEF_SWITCH list after both endpoints have been
 * constructed. Missing endpoints are omitted just as in proplvreset2. The
 * zero-initialized runtime owns native-width LinkRecords until close. */
int ge_original_stage_switch_runtime_materialize(
    GeOriginalStageSwitchRuntime *runtime,
    const GeOriginalStageSetupRuntime *setup, void *context,
    void *(*definition_by_command)(void *context, size_t command_index));

/* Must run before the endpoint objects are destroyed. */
void ge_original_stage_switch_runtime_close(
    GeOriginalStageSwitchRuntime *runtime);

#endif
