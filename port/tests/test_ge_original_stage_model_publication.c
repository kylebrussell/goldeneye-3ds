#include "ge_asset_pack.h"
#include "ge_original_model_scene.h"
#include "ge_original_pitem_models.h"
#include "ge_original_stage_model_publication.h"
#include "ge_pica_apply.h"

#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>
#include <bondtypes.h>
#include "matrixmath.h"
#include <PR/gu.h>

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

static void exercise_live_matrix_capture(const GeOriginalModelSceneInput *input,
    int require_nonzero)
{
    GeOriginalModelScene query, built;
    GeOriginalModelSceneCache cache = {0};
    assert(ge_original_model_scene_build(input, NULL, &query)
        == GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED);
    size_t count = query.required_vertex_count;
    GeDamRoomWorldVertex *reference = calloc(count, sizeof(*reference));
    GeDamRoomWorldVertex *indexed = calloc(count, sizeof(*indexed));
    GeDamRoomDrawBatch *reference_batches = calloc(query.required_batch_count, sizeof(*reference_batches));
    GeDamRoomDrawBatch *indexed_batches = calloc(query.required_batch_count, sizeof(*indexed_batches));
    uint16_t *indices = calloc(count, sizeof(*indices));
    assert(reference && indexed && reference_batches && indexed_batches && indices);
    GeDamRoomSceneStorage regular = {reference, count, reference_batches, query.required_batch_count};
    GeDamRoomSceneStorage captured = {indexed, count, indexed_batches, query.required_batch_count};
    assert(ge_original_model_scene_build_preflighted(input, &query, &regular, &built)
        == GE_ORIGINAL_MODEL_SCENE_OK);
    assert(ge_original_model_scene_build_matrix_template_preflighted(input, &query,
        &captured, indices, count, &built) == GE_ORIGINAL_MODEL_SCENE_OK);
    /* Real bank and outer transform are preserved, not an identity template. */
    assert(memcmp(reference, indexed, count * sizeof(*reference)) == 0);
    assert(memcmp(reference_batches, indexed_batches,
        query.required_batch_count * sizeof(*reference_batches)) == 0);
    int nonzero = 0;
    for (size_t vertex = 0U; vertex < count; ++vertex) {
        assert(indices[vertex] < input->segment3_matrix_count);
        nonzero |= indices[vertex] != 0U;
    }
    if (require_nonzero) assert(nonzero);
    assert(ge_original_model_scene_cache_build(&cache, input, 1U, &captured, &built)
        == GE_ORIGINAL_MODEL_SCENE_OK);
    GeOriginalModelSceneTemplateView template_view;
    assert(ge_original_model_scene_cache_template_view(&cache, 0U, &template_view));
    assert(memcmp(indices, template_view.matrix_indices, count * sizeof(*indices)) == 0);
    ge_original_model_scene_cache_close(&cache);
    free(indices); free(indexed_batches); free(reference_batches); free(indexed); free(reference);
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
        exercise_live_matrix_capture(&input, resident_part.matrix_index != 0U);
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

static void exercise_glass(GeOriginalPitemModelProvider *models)
{
    GeOriginalPitemModelScenePart part;
    struct TintedGlassRecord glass = {0};
    GeOriginalModelSceneInput input = {0};
    GeOriginalModelScene scene;
    GeOriginalModelSceneCache cache = {0};
    GeDamRoomWorldVertex vertices[6];
    GeDamRoomDrawBatch batches[1];
    GeDamRoomSceneStorage storage = {vertices, 6U, batches, 1U};
    GePicaApplyState applied;
    unsigned opacity;
    void *header = NULL, *model = NULL;
    float scale;
    assert(ge_original_pitem_model_resolve_instance(models, 104,
        &header, &model, &scale));
    assert(ge_original_pitem_model_scene_part(models, 104, 0U, &part));
    assert(part.model_type == 4);
    input.blob = part.blob;
    input.blob_size = part.blob_size;
    input.primary_offset = part.primary_offset;
    input.secondary_offset = part.secondary_offset;
    input.segment4_offset = part.segment4_offset;
    set_identity(input.matrix);
    glass.type = PROPDEF_TINTED_GLASS;
    for (opacity = 0U; opacity <= 255U; opacity += 51U) {
        glass.calculatedopacity = opacity;
        ge_original_stage_model_publication_glass_material(&glass, 4, &input);
        assert(input.parent_setup_enabled == 1U);
        assert(ge_original_model_scene_cache_build(&cache, &input, 1U,
            &storage, &scene) == GE_ORIGINAL_MODEL_SCENE_OK);
        assert(scene.vertex_count == 6U && scene.batch_count == 1U);
        assert(batches[0].material.blend_enabled);
        assert(batches[0].material.depth_test_enabled);
        assert(!batches[0].material.depth_write_enabled);
        assert(batches[0].material.primitive_color.alpha == opacity);
        assert(batches[0].material.alpha_combine
            == GE_PICA_ALPHA_TEXTURE0_MODULATE_SHADE_ADD_PRIMITIVE);
        assert(vertices[0].processed.rgba[0] >= 150U);
        assert(ge_pica_apply_compile(&batches[0].material, &applied)
            == GE_PICA_APPLY_OK);
        assert(applied.alpha.combine == GE_PICA_APPLY_MULTIPLY_ADD
            && applied.constant_color.alpha == opacity);
        assert(ge_original_model_scene_cache_build(&cache, &input, 1U,
            &storage, &scene) == GE_ORIGINAL_MODEL_SCENE_OK);
    }
    /* Real authored window normals and ST scale, original LookAt producer,
     * and the same matrix composition as objTick. Camera refresh must only
     * change shade/ST, not geometry or retained topology. */
    assert(batches[0].material.texture_gen_enabled);
    assert(batches[0].material.texture_scale_s == 0x0d80U);
    set_identity(glass.mtx.m);
    glass.mtx.m[0][0] = 0.0f;
    glass.mtx.m[0][2] = 0.5f;
    glass.mtx.m[2][0] = -0.5f;
    glass.mtx.m[2][2] = 0.0f;
    glass.mtx.m[1][1] = 0.5f;
    float first_u = 0.0f;
    int different_uv = 0;
    const uint64_t topology_rebuilds = cache.topology_rebuilds;
    for (unsigned angle = 0U; angle < 24U; ++angle) {
        float view[4][4];
        LookAt look_at = {0};
        const float radians = (float)angle * 0.25f;
        GeGbiRenderState shading;
        Mtxf expected, camera;
        guLookAtReflectF(view, &look_at, 0.0f, 0.0f, 0.0f,
            sinf(radians), 0.0f, cosf(radians), 0.0f, 1.0f, 0.0f);
        assert(ge_original_stage_model_publication_glass_shading(&glass,
            view, (const uint8_t *)&look_at, &batches[0].material, &shading));
        memcpy(camera.m, view, sizeof(camera.m));
        matrix_4x4_multiply_homogeneous(&camera, &glass.mtx, &expected);
        for (size_t row = 0U; row < 3U; ++row)
            for (size_t column = 0U; column < 3U; ++column)
                assert(shading.modelview_stack.entries[0].elements[row][column]
                    == (float)(int32_t)(expected.m[row][column] * 65536.0f) / 65536.0f);
        for (size_t vertex = 0U; vertex < 6U; ++vertex) {
            GeGbiProcessedVertex full, retained = vertices[vertex].processed;
            assert(ge_gbi_vertex_process(&shading, &vertices[vertex].source, &full)
                == GE_GBI_VERTEX_PROCESS_OK);
            assert(ge_gbi_vertex_shade(&shading, &vertices[vertex].source, &retained)
                == GE_GBI_VERTEX_PROCESS_OK);
            assert(retained.texture_generated == 1U);
            assert(memcmp(full.texture, retained.texture, sizeof(full.texture)) == 0);
            assert(memcmp(full.rgba, retained.rgba, sizeof(full.rgba)) == 0);
            assert(memcmp(retained.object, vertices[vertex].processed.object,
                sizeof(retained.object)) == 0);
            assert(memcmp(retained.eye, vertices[vertex].processed.eye,
                sizeof(retained.eye)) == 0);
            assert(memcmp(retained.clip, vertices[vertex].processed.clip,
                sizeof(retained.clip)) == 0);
            assert(retained.texture[0] >= 0.0f && retained.texture[0] <= 1.0f);
            assert(retained.texture[1] >= 0.0f && retained.texture[1] <= 1.0f);
            if (vertex == 0U) {
                if (angle == 0U) first_u = retained.texture[0];
                else if (fabsf(first_u - retained.texture[0]) > 0.1f) different_uv = 1;
            }
        }
    }
    assert(different_uv && cache.topology_rebuilds == topology_rebuilds);
    ge_original_model_scene_cache_close(&cache);
    /* Windowed-door parent state is the same original type-4 branch, but its
     * changing calculated opacity is not immutable model topology. */
    struct DoorRecord door = {0};
    door.type = PROPDEF_DOOR;
    door.doorFlags = DOORFLAG_WINDOWED;
    ge_original_stage_model_publication_glass_template(&door, 4, &input);
    for (opacity = 0U; opacity <= 255U; ++opacity) {
        door.calculatedopacity = opacity;
        ge_original_stage_model_publication_glass_template(&door, 4, &input);
        assert(ge_original_model_scene_cache_build(&cache, &input, 1U,
            &storage, &scene) == GE_ORIGINAL_MODEL_SCENE_OK);
        assert(cache.topology_rebuilds == 1U);
        assert(ge_original_stage_model_publication_glass_alpha(&door, &batches[0].material));
        assert(batches[0].material.primitive_color.alpha == opacity);
        assert(batches[0].material.depth_test_enabled && !batches[0].material.depth_write_enabled);
        assert(ge_pica_apply_compile(&batches[0].material, &applied) == GE_PICA_APPLY_OK);
        assert(applied.alpha.combine == GE_PICA_APPLY_MULTIPLY_ADD
            && applied.constant_color.alpha == opacity);
    }
    assert(cache.unchanged_builds == 255U);
    /* The publication boundary accepts every canonical door matrix slot,
     * not just the base. Use rotating/scaled fixture matrices and original
     * LookAt/GBI shading; authored vertex normals still come from PwindowZ. */
    GeOriginalDoorRuntimePublication publication = {0};
    publication.matrix_count = GE_ORIGINAL_DOOR_MATRIX_CAPACITY;
    for (unsigned frame = 0U; frame < 32U; ++frame) {
        float view[4][4];
        LookAt look_at = {0};
        guLookAtReflectF(view, &look_at, 10.0f, 20.0f, 30.0f,
            10.0f + sinf(frame * 0.2f), 20.0f, 30.0f + cosf(frame * 0.2f),
            0.0f, 1.0f, 0.0f);
        for (size_t matrix = 0U; matrix < publication.matrix_count; ++matrix) {
            float (*world)[4] = publication.matrices[matrix];
            float rotation = frame * 0.13f + matrix * 0.3f;
            set_identity(world);
            world[0][0] = world[2][2] = cosf(rotation) * 0.5f;
            world[0][2] = sinf(rotation) * 0.5f;
            world[2][0] = -world[0][2];
            world[1][1] = 0.5f;
            world[3][0] = (float)matrix * 17.0f;
            GeGbiRenderState shading;
            Mtxf camera, local, expected;
            assert(ge_original_stage_model_publication_door_glass_shading(
                &door, &publication, matrix, view, (const uint8_t *)&look_at,
                &batches[0].material, &shading));
            memcpy(camera.m, view, sizeof(camera.m));
            memcpy(local.m, world, sizeof(local.m));
            matrix_4x4_multiply_homogeneous(&camera, &local, &expected);
            for (size_t row = 0U; row < 3U; ++row)
                for (size_t column = 0U; column < 3U; ++column)
                    assert(shading.modelview_stack.entries[0].elements[row][column]
                        == (float)(int32_t)(expected.m[row][column] * 65536.0f) / 65536.0f);
            for (size_t vertex = 0U; vertex < 6U; ++vertex) {
                GeGbiProcessedVertex full, retained = vertices[vertex].processed;
                assert(ge_gbi_vertex_process(&shading, &vertices[vertex].source, &full)
                    == GE_GBI_VERTEX_PROCESS_OK);
                assert(ge_gbi_vertex_shade(&shading, &vertices[vertex].source, &retained)
                    == GE_GBI_VERTEX_PROCESS_OK);
                assert(memcmp(full.texture, retained.texture, sizeof(full.texture)) == 0);
                assert(memcmp(full.rgba, retained.rgba, sizeof(full.rgba)) == 0);
                assert(memcmp(retained.eye, vertices[vertex].processed.eye, sizeof(retained.eye)) == 0);
                assert(memcmp(retained.clip, vertices[vertex].processed.clip, sizeof(retained.clip)) == 0);
            }
        }
        GeGbiRenderState rejected;
        assert(!ge_original_stage_model_publication_door_glass_shading(
            &door, &publication, publication.matrix_count, view, (const uint8_t *)&look_at,
            &batches[0].material, &rejected));
        door.doorFlags = 0U;
        assert(!ge_original_stage_model_publication_door_glass_shading(
            &door, &publication, 0U, view, (const uint8_t *)&look_at,
            &batches[0].material, &rejected));
        door.doorFlags = DOORFLAG_WINDOWED;
    }
    GePicaMaterial opaque = batches[0].material;
    opaque.alpha_combine = GE_PICA_ALPHA_TEXTURE0;
    opaque.primitive_color.alpha = 71U;
    assert(!ge_original_stage_model_publication_glass_alpha(&door, &opaque));
    assert(opaque.primitive_color.alpha == 71U);
    door.doorFlags = 0U;
    assert(!ge_original_stage_model_publication_glass_alpha(&door, &batches[0].material));
    door.doorFlags = DOORFLAG_WINDOWED;
    door.flags2 = 0x10000U;
    ge_original_stage_model_publication_glass_template(&door, 4, &input);
    assert(input.world_zbuffer_enabled == 0U);
    ge_original_model_scene_cache_close(&cache);
    assert(ge_original_pitem_model_release_instance(models, model));
    puts("authored glass lighting/blend/opacity and cache invalidation passed");
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
    exercise_glass(models);
    ge_original_pitem_model_provider_destroy(models);
    ge_asset_pack_close(&pack);
    puts("live ordinary identity/articulated model publication passed");
    return 0;
}
