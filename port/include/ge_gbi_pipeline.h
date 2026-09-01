#ifndef GE_GBI_PIPELINE_H
#define GE_GBI_PIPELINE_H

#include "ge_gbi_state.h"
#include "ge_gbi_traverse.h"
#include "ge_gbi_vertex.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GeGbiPipelineEvent {
    GeGbiStateAction action;
    const GeGbiRenderState *state;
    const GeGbiVertex *vertex_cache;
    GeGbiAddress command_address;
    size_t call_depth;
    const GeGbiProcessedVertex *processed_vertex_cache;
    /* Zero-based root index for ge_gbi_pipeline_execute_sequence(). */
    size_t sequence_index;
} GeGbiPipelineEvent;

/* Return zero to request a clean stop after the current action. */
typedef int (*GeGbiPipelineCallback)(const GeGbiPipelineEvent *event,
                                    void *user_data);

typedef enum GeGbiPipelineStatus {
    GE_GBI_PIPELINE_OK = 0,
    GE_GBI_PIPELINE_STOPPED,
    GE_GBI_PIPELINE_INVALID_ARGUMENT,
    GE_GBI_PIPELINE_TRAVERSAL_ERROR,
    GE_GBI_PIPELINE_STATE_ERROR
} GeGbiPipelineStatus;

typedef struct GeGbiPipelineResult {
    GeGbiPipelineStatus status;
    GeGbiTraversalResult traversal;
    GeGbiStateStatus state_status;
    GeGbiRenderState final_state;
    size_t actions_emitted;
    size_t draw_calls;
    size_t triangles;
    size_t unsupported_commands;
} GeGbiPipelineResult;

/*
 * Traverses a segmented GoldenEye display list, maintains the portable RSP/RDP
 * shadow state, fills the 16-slot vertex cache, and emits renderer actions.
 */
GeGbiPipelineResult ge_gbi_pipeline_execute(
    const GeGbiMemoryMap *memory,
    GeGbiAddress root_address,
    GeGbiByteOrder byte_order,
    const GeGbiTraversalConfig *config,
    GeGbiPipelineCallback callback,
    void *user_data);

/*
 * Executes consecutive roots as one canonical RSP/RDP task. Render state,
 * matrix stacks, and the 16-slot RSP vertex cache survive each root's
 * G_ENDDL and are available to the next root. This is required by GoldenEye
 * model nodes whose Secondary list continues work begun by Primary.
 */
GeGbiPipelineResult ge_gbi_pipeline_execute_sequence(
    const GeGbiMemoryMap *memory,
    const GeGbiAddress *root_addresses,
    size_t root_count,
    GeGbiByteOrder byte_order,
    const GeGbiTraversalConfig *config,
    GeGbiPipelineCallback callback,
    void *user_data);

#ifdef __cplusplus
}
#endif

#endif
