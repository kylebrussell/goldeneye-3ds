#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>
#include <bondtypes.h>

#include "ge_original_stage_model_publication.h"

#include <string.h>

static GeOriginalStageModelPublicationStatus
ge_original_stage_model_publication_input_exact(
    const GeOriginalPitemModelProvider *models, const void *definition,
    size_t visible_part_index, uint8_t room,
    const float view_to_world[4][4], GeOriginalModelSceneInput *input,
    int require_visible)
{
    const ObjectRecord *object = definition;
    const Model *model;
    GeOriginalPitemModelScenePart part;

    if (models == NULL || object == NULL || view_to_world == NULL
            || input == NULL)
        return GE_ORIGINAL_STAGE_MODEL_PUBLICATION_INVALID_ARGUMENT;
    if (object->prop == NULL)
        return require_visible
            ? GE_ORIGINAL_STAGE_MODEL_PUBLICATION_NOT_VISIBLE
            : GE_ORIGINAL_STAGE_MODEL_PUBLICATION_INVALID_MODEL;
    if (require_visible
            && (object->prop->flags & PROPFLAG_ONSCREEN) == 0U)
        return GE_ORIGINAL_STAGE_MODEL_PUBLICATION_NOT_VISIBLE;
    model = object->model;
    if (model == NULL || model->obj == NULL || model->render_pos == NULL
            || model->obj->numMatrices <= 0 || object->obj < 0)
        return GE_ORIGINAL_STAGE_MODEL_PUBLICATION_INVALID_MODEL;
    if (!ge_original_pitem_model_instance_scene_part(
            models, model, visible_part_index, &part)
            || part.matrix_index >= (uint16_t)model->obj->numMatrices)
        return GE_ORIGINAL_STAGE_MODEL_PUBLICATION_INVALID_PART;

    memset(input, 0, sizeof(*input));
    input->blob = part.blob;
    input->blob_size = part.blob_size;
    input->primary_offset = part.primary_offset;
    input->secondary_offset = part.secondary_offset;
    input->segment4_offset = part.segment4_offset;
    input->room_id = room;
    input->world_zbuffer_enabled = 1U;
    input->segment3_matrices =
        (const float (*)[4][4])(const void *)&model->render_pos[0].pos;
    input->segment3_matrix_count = (size_t)model->obj->numMatrices;
    memcpy(input->matrix, view_to_world, sizeof(input->matrix));
    return GE_ORIGINAL_STAGE_MODEL_PUBLICATION_OK;
}

GeOriginalStageModelPublicationStatus
ge_original_stage_model_publication_input(
    const GeOriginalPitemModelProvider *models, const void *definition,
    size_t visible_part_index, uint8_t room,
    const float view_to_world[4][4], GeOriginalModelSceneInput *input)
{
    return ge_original_stage_model_publication_input_exact(
        models, definition, visible_part_index, room, view_to_world, input,
        1);
}

GeOriginalStageModelPublicationStatus
ge_original_stage_model_publication_resident_input(
    const GeOriginalPitemModelProvider *models, const void *definition,
    size_t visible_part_index, uint8_t room,
    const float view_to_world[4][4], GeOriginalModelSceneInput *input)
{
    return ge_original_stage_model_publication_input_exact(
        models, definition, visible_part_index, room, view_to_world, input,
        0);
}

const char *ge_original_stage_model_publication_status_name(
    GeOriginalStageModelPublicationStatus status)
{
    switch (status) {
    case GE_ORIGINAL_STAGE_MODEL_PUBLICATION_OK: return "ok";
    case GE_ORIGINAL_STAGE_MODEL_PUBLICATION_INVALID_ARGUMENT:
        return "invalid argument";
    case GE_ORIGINAL_STAGE_MODEL_PUBLICATION_NOT_VISIBLE:
        return "not visible";
    case GE_ORIGINAL_STAGE_MODEL_PUBLICATION_INVALID_MODEL:
        return "invalid model";
    case GE_ORIGINAL_STAGE_MODEL_PUBLICATION_INVALID_PART:
        return "invalid part";
    default: return "unknown";
    }
}
