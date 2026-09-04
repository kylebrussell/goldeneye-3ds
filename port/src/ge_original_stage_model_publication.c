#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>
#include <bondtypes.h>

#include "ge_original_stage_model_publication.h"

#include <string.h>

/* The SDK macro shifts a signed int into bit 31. Preserve its exact command
 * bits while making the host/ARM encoding well-defined under UBSan. */
#undef GBL_c1
#define GBL_c1(a, b, c, d) \
    ((uint32_t)(a) << 30 | (uint32_t)(b) << 26 \
     | (uint32_t)(c) << 22 | (uint32_t)(d) << 18)
#include "ge_original_model_render_setup.inc"

int ge_original_stage_model_publication_glass_opacity(
    const void *definition, uint8_t *opacity)
{
    const ObjectRecord *object = definition;
    if (object == NULL || opacity == NULL) return 0;
    if (object->type == PROPDEF_GLASS) *opacity = 0U;
    else if (object->type == PROPDEF_TINTED_GLASS)
        *opacity = (uint8_t)((const struct TintedGlassRecord *)object)->calculatedopacity;
    else if (object->type == PROPDEF_DOOR
            && (((const struct DoorRecord *)object)->doorFlags & DOORFLAG_WINDOWED))
        *opacity = (uint8_t)((const struct DoorRecord *)object)->calculatedopacity;
    else return 0;
    return 1;
}

void ge_original_stage_model_publication_glass_material(
    const void *definition, int16_t model_type, GeOriginalModelSceneInput *input)
{
    const ObjectRecord *object = definition;
    ModelRenderData render = {0};
    size_t pass;
    uint8_t opacity;
    if (object == NULL || input == NULL || model_type != 4
            || (object->type != PROPDEF_GLASS
                && object->type != PROPDEF_TINTED_GLASS
                && !(object->type == PROPDEF_DOOR
                    && (((const struct DoorRecord *)object)->doorFlags
                        & DOORFLAG_WINDOWED)))) return;
    /* chrobjRenderProp's unfaded object branch. Opacity is owned by the
     * original object tick, never recomputed by the platform renderer. */
    render.PropType = PROP_TYPE_MAX;
    render.zbufferenabled = (object->flags2 & 0x10000) == 0;
    if (!ge_original_stage_model_publication_glass_opacity(object, &opacity)) return;
    render.envcolour.word = (uint32_t)opacity << 8;
    input->parent_setup_enabled = 1U;
    input->world_zbuffer_enabled = (uint8_t)render.zbufferenabled;
    memset(input->parent_setup, 0, sizeof(input->parent_setup));
    for (pass = 0U; pass < 2U; ++pass) {
        Gfx commands[12];
        Gfx *gdl = commands;
        size_t word;
        const uintptr_t base = 0x02000000U + pass * 128U;
        /* bgLevelRender publishes these lights before all model lists. */
        gSPNumLights(gdl++, NUMLIGHTS_1);
        gSPLight(gdl++, (const Light *)(base + 96U), 1);
        gSPLight(gdl++, (const Light *)(base + 112U), 2);
        render.gdl = gdl;
        ge_model_render_type4(&render, pass == 0U);
        gdl = render.gdl;
        gSPEndDisplayList(gdl++);
        for (word = 0U; word < (size_t)(gdl - commands) * 2U; ++word) {
            const uint32_t value = (uint32_t)(word % 2U == 0U
                ? commands[word / 2U].words.w0 : commands[word / 2U].words.w1);
            uint8_t *bytes = input->parent_setup[pass] + word * 4U;
            bytes[0] = (uint8_t)(value >> 24);
            bytes[1] = (uint8_t)(value >> 16);
            bytes[2] = (uint8_t)(value >> 8);
            bytes[3] = (uint8_t)value;
        }
        memcpy(input->parent_setup[pass] + 96U, &ge_model_level_light.l[0], 16U);
        memcpy(input->parent_setup[pass] + 112U, &ge_model_level_light.a, 8U);
    }
}

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
    ge_original_stage_model_publication_glass_material(
        object, part.model_type, input);
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
