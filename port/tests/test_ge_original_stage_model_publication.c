#include "ge_asset_pack.h"
#include "ge_original_model_scene.h"
#include "ge_original_pitem_models.h"
#include "ge_original_stage_model_publication.h"

#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>
#include <bondtypes.h>

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void set_identity(float matrix[4][4])
{
    memset(matrix, 0, sizeof(float) * 16U);
    matrix[0][0] = matrix[1][1] = matrix[2][2] = matrix[3][3] = 1.0f;
}

static int exercise_model(GeOriginalPitemModelProvider *models,
    int32_t model_id, size_t minimum_matrix_count, int articulated)
{
    void *header_pointer = NULL;
    void *model_pointer = NULL;
    ModelFileHeader *header;
    Model *model;
    ObjectRecord object;
    PropRecord prop;
    GeOriginalModelSceneInput input;
    GeOriginalModelScene query;
    GeOriginalModelScene built;
    GeDamRoomSceneStorage storage;
    GeDamRoomWorldVertex *vertices;
    GeDamRoomDrawBatch *batches;
    float view_to_world[4][4];
    float (*before)[4][4];
    float scale = 0.0f;
    size_t matrix_index;
    size_t value;
    size_t visible_part_count;
    int nonzero_matrix_part = 0;

    if (!ge_original_pitem_model_resolve_instance(models, model_id,
            &header_pointer, &model_pointer, &scale)) return 0;
    header = header_pointer;
    model = model_pointer;
    if (header == NULL || model == NULL
            || header->numMatrices <= 0
            || (size_t)header->numMatrices < minimum_matrix_count
            || ge_original_pitem_model_instance_scene_part_count(
                models, model) == 0U) {
        assert(ge_original_pitem_model_release_instance(models, model));
        return 0;
    }
    for (matrix_index = 0U; matrix_index < (size_t)header->numMatrices;
            ++matrix_index) {
        set_identity(model->render_pos[matrix_index].pos.m);
        if (articulated) {
            model->render_pos[matrix_index].pos.m[0][0] =
                1.0f + (float)matrix_index * 0.125f;
            model->render_pos[matrix_index].pos.m[3][0] =
                13.25f + (float)matrix_index * 7.5f;
            model->render_pos[matrix_index].pos.m[3][1] =
                -2.75f - (float)matrix_index * 3.25f;
            model->render_pos[matrix_index].pos.m[3][2] =
                40.5f + (float)matrix_index * 11.0f;
        }
    }
    before = malloc((size_t)header->numMatrices * sizeof(*before));
    assert(before != NULL);
    memcpy(before, &model->render_pos[0].pos,
        (size_t)header->numMatrices * sizeof(*before));
    set_identity(view_to_world);
    if (articulated) {
        view_to_world[3][0] = 100.0f;
        view_to_world[3][1] = -50.0f;
        view_to_world[3][2] = 25.0f;
    }
    memset(&object, 0, sizeof(object));
    memset(&prop, 0, sizeof(prop));
    object.obj = model_id;
    object.model = model;
    object.prop = &prop;
    prop.obj = &object;
    prop.flags = PROPFLAG_ONSCREEN;
    visible_part_count = ge_original_pitem_model_instance_scene_part_count(
        models, model);
    assert(visible_part_count != 0U);
    assert(ge_original_stage_model_publication_input(
        models, &object, 0U, 17U, view_to_world, &input)
        == GE_ORIGINAL_STAGE_MODEL_PUBLICATION_OK);
    assert(input.segment3_matrices
            == (const float (*)[4][4])(const void *)&model->render_pos[0].pos
        && input.segment3_matrix_count == (size_t)header->numMatrices
        && input.room_id == 17U && input.world_zbuffer_enabled == 1U
        && memcmp(input.matrix, view_to_world, sizeof(view_to_world)) == 0
        && input.position[0] == 0.0f && input.position[1] == 0.0f
        && input.position[2] == 0.0f);
    assert(ge_original_stage_model_publication_input(
        models, &object, visible_part_count, 17U,
        view_to_world, &input)
        == GE_ORIGINAL_STAGE_MODEL_PUBLICATION_INVALID_PART);
    assert(ge_original_model_scene_build(&input, NULL, &query)
        == GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED);
    assert(query.required_vertex_count != 0U
        && query.required_batch_count != 0U);
    vertices = calloc(query.required_vertex_count, sizeof(*vertices));
    batches = calloc(query.required_batch_count, sizeof(*batches));
    assert(vertices != NULL && batches != NULL);
    storage = (GeDamRoomSceneStorage){
        vertices, query.required_vertex_count,
        batches, query.required_batch_count,
    };
    assert(ge_original_model_scene_build(&input, &storage, &built)
        == GE_ORIGINAL_MODEL_SCENE_OK);
    for (value = 0U; value < built.vertex_count; ++value)
        assert(isfinite(vertices[value].world[0])
            && isfinite(vertices[value].world[1])
            && isfinite(vertices[value].world[2]));
    /* The renderer performs s15.16 quantization from this borrowed bank; it
     * must never rewrite canonical animation matrices in place. */
    assert(memcmp(before, &model->render_pos[0].pos,
        (size_t)header->numMatrices * sizeof(*before)) == 0);
    prop.flags = 0U;
    assert(ge_original_stage_model_publication_input(
        models, &object, 0U, 17U, view_to_world, &input)
        == GE_ORIGINAL_STAGE_MODEL_PUBLICATION_NOT_VISIBLE);
    for (value = 0U; value < visible_part_count; ++value) {
        GeOriginalPitemModelScenePart resident_part;
        assert(ge_original_pitem_model_instance_scene_part(
            models, model, value, &resident_part));
        assert(resident_part.matrix_index
            < (uint16_t)header->numMatrices);
        if (resident_part.matrix_index != 0U) nonzero_matrix_part = 1;
        assert(ge_original_stage_model_publication_resident_input(
            models, &object, value, 17U, view_to_world, &input)
            == GE_ORIGINAL_STAGE_MODEL_PUBLICATION_OK);
        assert(input.segment3_matrices
            == (const float (*)[4][4])
                (const void *)&model->render_pos[0].pos);
        if (resident_part.matrix_index != 0U) {
            GeOriginalModelSceneInput missing_bank = input;
            GeOriginalModelScene missing_query;
            GeOriginalModelSceneStatus missing_status;
            missing_bank.segment3_matrices = NULL;
            missing_bank.segment3_matrix_count = 0U;
            missing_status = ge_original_model_scene_build(
                &missing_bank, NULL, &missing_query);
            assert(missing_status == GE_ORIGINAL_MODEL_SCENE_PIPELINE_ERROR);
        }
    }
    if (minimum_matrix_count > 1U) assert(nonzero_matrix_part);
    free(batches);
    free(vertices);
    free(before);
    assert(ge_original_pitem_model_release_instance(models, model));
    return 1;
}

int main(int argc, char **argv)
{
    GeAssetPack pack;
    GeOriginalPitemModelProvider *models;
    GeOriginalPitemModelStatus status;
    int32_t model_id;
    int identity_found = 0;
    int articulated_found = 0;

    assert(argc == 2);
    assert(ge_asset_pack_open(&pack, argv[1]) == GE_ASSET_PACK_OK);
    models = ge_original_pitem_model_provider_create(
        &pack, 341U, 1U, &status);
    assert(models != NULL && status == GE_ORIGINAL_PITEM_MODEL_OK);
    for (model_id = 0; model_id < 341 && !identity_found; ++model_id)
        identity_found = exercise_model(models, model_id, 1U, 0);
    for (model_id = 0; model_id < 341 && !articulated_found; ++model_id)
        articulated_found = exercise_model(models, model_id, 2U, 1);
    assert(identity_found && articulated_found);
    /* PcctvZ is the authored Bunker 2 room-49 target: its multi-matrix live
     * display-list parts must all publish before ONSCREEN is set. */
    assert(exercise_model(models, 24, 2U, 1));
    ge_original_pitem_model_provider_destroy(models);
    ge_asset_pack_close(&pack);
    puts("live ordinary identity/articulated model publication passed");
    return 0;
}
