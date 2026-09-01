#ifndef GE_ORIGINAL_STAGE_MONITOR_H
#define GE_ORIGINAL_STAGE_MONITOR_H

#include "ge_original_dam_monitor_render.h"
#include "ge_original_pitem_models.h"

#include <stddef.h>
#include <stdint.h>

typedef struct GeOriginalStagePropConstructionRequest
    GeOriginalStagePropConstructionRequest;

enum { GE_ORIGINAL_STAGE_MONITOR_IMAGE_COUNT = 52 };

typedef struct GeOriginalStageMonitorProviders {
    void *context;
    int (*construct_standard)(void *context, void *definition,
                              int32_t command_index);
    int (*place_standard)(void *context, void *definition);
    int (*construct_owned)(void *context,
                           const GeOriginalStagePropConstructionRequest *request,
                           void *definition, size_t definition_size,
                           int32_t owner_command_index, int embedded);
} GeOriginalStageMonitorProviders;

typedef enum GeOriginalStageMonitorStatus {
    GE_ORIGINAL_STAGE_MONITOR_OK = 0,
    GE_ORIGINAL_STAGE_MONITOR_INVALID_ARGUMENT,
    GE_ORIGINAL_STAGE_MONITOR_INVALID_DEFINITION,
    GE_ORIGINAL_STAGE_MONITOR_INVALID_IMAGE,
    GE_ORIGINAL_STAGE_MONITOR_PLACEMENT_UNRESOLVED,
    GE_ORIGINAL_STAGE_MONITOR_EMBEDDED_OWNER_REQUIRED,
    GE_ORIGINAL_STAGE_MONITOR_INSIDE_OWNER_REQUIRED,
    GE_ORIGINAL_STAGE_MONITOR_CONSTRUCTION_FAILED,
    GE_ORIGINAL_STAGE_MONITOR_PLACEMENT_FAILED,
    GE_ORIGINAL_STAGE_MONITOR_OWNER_UNAVAILABLE,
} GeOriginalStageMonitorStatus;

typedef struct GeOriginalStageMonitorAttachmentPublication {
    const float (*segment3_matrices)[4][4];
    size_t segment3_matrix_count;
    uint8_t room;
} GeOriginalStageMonitorAttachmentPublication;

int ge_original_stage_monitor_controller_initialize(
    void *monitor_record, int32_t image_num);
int ge_original_stage_monitor_owner_command_exact(
    const GeOriginalStagePropConstructionRequest *request,
    const void *definition, int32_t *owner_command_index, int *embedded);
void ge_original_stage_monitor_set_image_exact(
    void *monitor_record, int32_t image_num);
const void *ge_original_stage_monitor_command_list(int32_t image_num);
int ge_original_stage_monitor_render_screen_tick(
    void *model_instance, void *monitor_record,
    uint32_t object_flags, uint32_t object_flags2,
    size_t screen_slot, GeOriginalDamMonitorRenderSnapshot *snapshot);

GeOriginalStageMonitorStatus ge_original_stage_monitor_construct(
    const GeOriginalStagePropConstructionRequest *request,
    void *definition, size_t definition_size,
    const GeOriginalStageMonitorProviders *providers);

int ge_original_stage_monitor_tick(
    void *definition, size_t definition_size, size_t screen_slot,
    GeOriginalDamMonitorRenderSnapshot *snapshot);

/* Exact setupSingleMonitor negative-pad branch and domakedefaultobj plus the
 * proplvreset2 INSIDEANOTHEROBJ owner pass. */
int ge_original_stage_monitor_construct_owned_exact(
    const GeOriginalStagePropConstructionRequest *request,
    void *definition, size_t definition_size, void *prop, size_t prop_size,
    GeOriginalPitemModelProvider *models, void *owner_definition,
    void *owner_prop, void *collision_data, int32_t player_count,
    int embedded, void **model_instance_out);
int ge_original_stage_monitor_bind_owned_prop_exact(
    const GeOriginalStagePropConstructionRequest *request,
    void *definition, void *prop, size_t prop_size);
void ge_original_stage_monitor_release_owned_exact(
    void *definition, GeOriginalPitemModelProvider *models);
int ge_original_stage_monitor_bind_inside_owner_exact(
    void *definition, void *owner_definition, void *owner_prop);
int ge_original_stage_monitor_compose_attachment_exact(
    const float owner_switch_matrix[4][4],
    const float embedment_matrix[4][4], float output[4][4]);
int ge_original_stage_monitor_publish_attachment_exact(
    void *definition, const float owner_matrix[4][4],
    const float owner_position[3], uint8_t owner_room,
    GeOriginalStageMonitorAttachmentPublication *publication);

const char *ge_original_stage_monitor_status_name(
    GeOriginalStageMonitorStatus status);

#endif
