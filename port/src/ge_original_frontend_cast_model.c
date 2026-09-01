#include "ge_original_frontend_cast_model.h"

#include "ge_original_character_models.h"
#include "ge_original_guard_animation_table.h"
#include "ge_original_model_scene.h"
#include "ge_original_pitem_models.h"

#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/matrixmath.h"
#include "game/model.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define GE_CAST_ANIMATION_DATA_PATH \
    "converted/animations/bond/animation_data.bin"
#define GE_CAST_ANIMATION_ENTRIES_PATH \
    "converted/animations/bond/animation_entries.bin"
#define GE_CAST_MODEL_SCALE 0.1f
#define GE_CAST_ANIMATION_TRANSLATION_SCALE 0.1f
#define GE_CAST_ANIMATION_TICK_SPEED 0.5f

typedef struct GeOriginalFrontendCastPart {
    uint8_t character;
    union {
        GeOriginalCharacterModelScenePart character;
        GeOriginalPitemModelScenePart pitem;
    } resource;
} GeOriginalFrontendCastPart;

struct GeOriginalFrontendCastModel {
    GeAssetPack *asset_pack;
    GeOriginalCharacterModelProvider *characters;
    GeOriginalPitemModelProvider *pitems;
    GeOriginalCharacterModelPair character_pair;
    void *weapon_header;
    Model *body;
    Model *weapon;
    ModelAnimation *animation;
    uint8_t *animation_data;
    size_t animation_data_size;
    uint8_t *animation_entries;
    size_t animation_entries_size;
    GeOriginalFrontendCastSelection selection;
    GeOriginalFrontendCastPart *parts;
    GeOriginalModelSceneInput *inputs;
    size_t part_capacity;
    GeDamRoomWorldVertex *vertices;
    GeDamRoomDrawBatch *batches;
    int16_t *batch_model_types;
    size_t vertex_capacity;
    size_t batch_capacity;
    GeOriginalModelSceneCache cache;
    GeOriginalFrontendCastModelScene scene;
    uint32_t animation_ticks;
    uint8_t bound;
};

extern void modelSetAnimPlaySpeed(Model *model, float animation_rate,
                                  float startframe);
extern void instcalcmatrices(ModelRenderData *render_data, Model *model);

static int load_asset(GeAssetPack *pack, const char *path,
                      uint8_t **bytes, size_t *size)
{
    const GeAssetPackEntry *entry;
    uint8_t *storage;
    if (pack == NULL || path == NULL || bytes == NULL || size == NULL
            || (entry = ge_asset_pack_find(pack, path)) == NULL
            || entry->data_size == 0U || entry->data_size > SIZE_MAX)
        return 0;
    storage = malloc((size_t)entry->data_size);
    if (storage == NULL) return 0;
    if (ge_asset_pack_read(pack, path, storage, (size_t)entry->data_size,
                           NULL) != GE_ASSET_PACK_OK) {
        free(storage);
        return 0;
    }
    *bytes = storage;
    *size = (size_t)entry->data_size;
    return 1;
}

static void identity(float matrix[4][4])
{
    size_t axis;
    memset(matrix, 0, sizeof(float) * 16U);
    for (axis = 0U; axis < 4U; ++axis) matrix[axis][axis] = 1.0f;
}

static int same_selection(const GeOriginalFrontendCastSelection *left,
                          const GeOriginalFrontendCastSelection *right)
{
    return left != NULL && right != NULL
        && left->character_index == right->character_index
        && left->body == right->body && left->head == right->head
        && left->weapon_prop == right->weapon_prop
        && left->animation_id == right->animation_id
        && left->animation_record_offset == right->animation_record_offset
        && left->animation_start_frame == right->animation_start_frame
        && left->animation_playback_speed == right->animation_playback_speed
        && left->animation_camera_preset == right->animation_camera_preset
        && left->animation_flip == right->animation_flip;
}

static void set_weapon_flash(Model *weapon, int visible)
{
    if (weapon == NULL || weapon->obj == NULL) return;
    if (weapon->obj->Switches != NULL && weapon->obj->Switches[0] != NULL)
        modelGetNodeRwData(weapon,
            weapon->obj->Switches[0])->Gunfire.visible = visible != 0;
    if (weapon->obj->Switches != NULL && weapon->obj->Switches[2] != NULL)
        modelGetNodeRwData(weapon,
            weapon->obj->Switches[2])->Switch.visible = visible != 0;
}

static void release_selection(GeOriginalFrontendCastModel *owner)
{
    if (owner == NULL) return;
    ge_original_model_scene_cache_close(&owner->cache);
    ge_original_character_model_provider_destroy(owner->characters);
    ge_original_pitem_model_provider_destroy(owner->pitems);
    owner->characters = NULL;
    owner->pitems = NULL;
    owner->body = NULL;
    owner->weapon = NULL;
    owner->weapon_header = NULL;
    owner->animation = NULL;
    owner->bound = 0U;
    owner->animation_ticks = 0U;
    memset(&owner->character_pair, 0, sizeof(owner->character_pair));
    memset(&owner->selection, 0, sizeof(owner->selection));
    memset(&owner->scene, 0, sizeof(owner->scene));
}

static GeOriginalFrontendCastModelStatus reserve_parts(
    GeOriginalFrontendCastModel *owner, size_t required)
{
    GeOriginalFrontendCastPart *parts;
    GeOriginalModelSceneInput *inputs;
    if (required <= owner->part_capacity)
        return GE_ORIGINAL_FRONTEND_CAST_MODEL_OK;
    parts = realloc(owner->parts, required * sizeof(*parts));
    if (parts == NULL)
        return GE_ORIGINAL_FRONTEND_CAST_MODEL_ALLOCATION_FAILED;
    owner->parts = parts;
    inputs = realloc(owner->inputs, required * sizeof(*inputs));
    if (inputs == NULL)
        return GE_ORIGINAL_FRONTEND_CAST_MODEL_ALLOCATION_FAILED;
    owner->inputs = inputs;
    owner->part_capacity = required;
    return GE_ORIGINAL_FRONTEND_CAST_MODEL_OK;
}

static GeOriginalFrontendCastModelStatus collect_parts(
    GeOriginalFrontendCastModel *owner, size_t *part_count,
    size_t *character_part_count, size_t *weapon_part_count)
{
    size_t characters;
    size_t weapons = 0U;
    size_t index;
    GeOriginalFrontendCastModelStatus status;
    if (owner == NULL || part_count == NULL || character_part_count == NULL
            || weapon_part_count == NULL || owner->body == NULL)
        return GE_ORIGINAL_FRONTEND_CAST_MODEL_INVALID_ARGUMENT;
    if (!ge_original_character_model_prepare_instance_relations(
            owner->characters, owner->body))
        return GE_ORIGINAL_FRONTEND_CAST_MODEL_MODEL_UNAVAILABLE;
    characters = ge_original_character_model_instance_scene_part_count(
        owner->characters, owner->body);
    if (owner->weapon != NULL)
        weapons = ge_original_pitem_model_instance_scene_part_count(
            owner->pitems, owner->weapon);
    if (characters == 0U || (owner->weapon != NULL && weapons == 0U)
            || characters > SIZE_MAX - weapons)
        return GE_ORIGINAL_FRONTEND_CAST_MODEL_SCENE_UNAVAILABLE;
    status = reserve_parts(owner, characters + weapons);
    if (status != GE_ORIGINAL_FRONTEND_CAST_MODEL_OK) return status;
    for (index = 0U; index < characters; ++index) {
        owner->parts[index].character = 1U;
        if (!ge_original_character_model_instance_scene_part(
                owner->characters, owner->body, index,
                &owner->parts[index].resource.character))
            return GE_ORIGINAL_FRONTEND_CAST_MODEL_SCENE_UNAVAILABLE;
    }
    for (index = 0U; index < weapons; ++index) {
        GeOriginalFrontendCastPart *part = &owner->parts[characters + index];
        part->character = 0U;
        if (!ge_original_pitem_model_instance_scene_part(
                owner->pitems, owner->weapon, index, &part->resource.pitem))
            return GE_ORIGINAL_FRONTEND_CAST_MODEL_SCENE_UNAVAILABLE;
    }
    *part_count = characters + weapons;
    *character_part_count = characters;
    *weapon_part_count = weapons;
    return GE_ORIGINAL_FRONTEND_CAST_MODEL_OK;
}

static GeOriginalFrontendCastModelStatus build_scene(
    GeOriginalFrontendCastModel *owner,
    const GeOriginalFrontendCastFrame *frame,
    GeOriginalFrontendCastModelScene *scene)
{
    GeOriginalModelScene result;
    GeOriginalModelSceneStatus model_status;
    size_t part_count;
    size_t character_parts;
    size_t weapon_parts;
    size_t index;
    GeOriginalFrontendCastModelStatus status = collect_parts(
        owner, &part_count, &character_parts, &weapon_parts);
    if (status != GE_ORIGINAL_FRONTEND_CAST_MODEL_OK) return status;
    for (index = 0U; index < part_count; ++index) {
        const GeOriginalFrontendCastPart *part = &owner->parts[index];
        GeOriginalModelSceneInput *input = &owner->inputs[index];
        memset(input, 0, sizeof(*input));
        if (part->character) {
            input->blob = part->resource.character.blob;
            input->blob_size = part->resource.character.blob_size;
            input->primary_offset = part->resource.character.primary_offset;
            input->secondary_offset = part->resource.character.secondary_offset;
            input->segment4_offset = part->resource.character.segment4_offset;
            input->segment3_matrices =
                (const float (*)[4][4])(const void *)owner->body->render_pos;
            input->segment3_matrix_count = owner->character_pair.matrix_count;
        } else {
            input->blob = part->resource.pitem.blob;
            input->blob_size = part->resource.pitem.blob_size;
            input->primary_offset = part->resource.pitem.primary_offset;
            input->secondary_offset = part->resource.pitem.secondary_offset;
            input->segment4_offset = part->resource.pitem.segment4_offset;
            input->segment3_matrices =
                (const float (*)[4][4])(const void *)owner->weapon->render_pos;
            input->segment3_matrix_count = (size_t)owner->weapon->obj->numMatrices;
        }
        input->world_zbuffer_enabled = 1U;
        identity(input->matrix);
    }
    model_status = ge_original_model_scene_cache_build(
        &owner->cache, owner->inputs, part_count, NULL, &result);
    if (model_status != GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED
            && model_status != GE_ORIGINAL_MODEL_SCENE_OK)
        return GE_ORIGINAL_FRONTEND_CAST_MODEL_SCENE_UNAVAILABLE;
    if (result.required_vertex_count > owner->vertex_capacity
            || result.required_batch_count > owner->batch_capacity) {
        GeDamRoomWorldVertex *vertices = realloc(owner->vertices,
            result.required_vertex_count * sizeof(*vertices));
        GeDamRoomDrawBatch *batches;
        int16_t *model_types;
        if (vertices == NULL && result.required_vertex_count != 0U)
            return GE_ORIGINAL_FRONTEND_CAST_MODEL_ALLOCATION_FAILED;
        owner->vertices = vertices;
        batches = realloc(owner->batches,
            result.required_batch_count * sizeof(*batches));
        if (batches == NULL && result.required_batch_count != 0U)
            return GE_ORIGINAL_FRONTEND_CAST_MODEL_ALLOCATION_FAILED;
        owner->batches = batches;
        model_types = realloc(owner->batch_model_types,
            result.required_batch_count * sizeof(*model_types));
        if (model_types == NULL && result.required_batch_count != 0U)
            return GE_ORIGINAL_FRONTEND_CAST_MODEL_ALLOCATION_FAILED;
        owner->batch_model_types = model_types;
        owner->vertex_capacity = result.required_vertex_count;
        owner->batch_capacity = result.required_batch_count;
    }
    {
        const GeDamRoomSceneStorage storage = {
            owner->vertices, owner->vertex_capacity,
            owner->batches, owner->batch_capacity,
        };
        model_status = ge_original_model_scene_cache_build(
            &owner->cache, owner->inputs, part_count, &storage, &result);
    }
    if (model_status != GE_ORIGINAL_MODEL_SCENE_OK)
        return GE_ORIGINAL_FRONTEND_CAST_MODEL_SCENE_UNAVAILABLE;
    memset(&owner->scene, 0, sizeof(owner->scene));
    owner->scene.vertices = owner->vertices;
    owner->scene.batches = owner->batches;
    owner->scene.batch_model_types = owner->batch_model_types;
    owner->scene.vertex_count = result.vertex_count;
    owner->scene.batch_count = result.batch_count;
    owner->scene.triangle_count = result.triangle_count;
    owner->scene.part_count = part_count;
    owner->scene.character_part_count = character_parts;
    owner->scene.weapon_part_count = weapon_parts;
    owner->scene.allocated_part_capacity = owner->part_capacity;
    owner->scene.selection = owner->selection;
    memcpy(owner->scene.camera_eye, frame->camera_eye,
           sizeof(owner->scene.camera_eye));
    memcpy(owner->scene.camera_target, frame->camera_target,
           sizeof(owner->scene.camera_target));
    memcpy(owner->scene.camera_up, frame->camera_up,
           sizeof(owner->scene.camera_up));
    owner->scene.fade = frame->fade;
    owner->scene.animation_ticks = owner->animation_ticks;
    owner->scene.weapon_attached = owner->weapon != NULL;
    owner->scene.weapon_attachment_switch =
        owner->weapon == NULL ? 0U
            : (owner->selection.animation_flip ? 5U : 3U);
    owner->scene.weapon_left_hand_rotation = owner->weapon != NULL
        && owner->selection.animation_flip != 0U;
    owner->scene.render_prop_type = (uint8_t)PROP_TYPE_EXPLOSION;
    owner->scene.render_zbuffer_enabled = 1U;
    owner->scene.render_cull_mode = 3U; /* CULLMODE_BOTH */
    owner->scene.render_flags = 3U;
    owner->scene.render_lighting_enabled = 1U;
    owner->scene.render_texture_gen_enabled = 1U;
    owner->scene.reflection_camera_eye_z = 4000.0f;
    for (index = 0U; index < part_count; ++index) {
        const size_t begin = owner->cache.input_batch_offsets[index];
        const size_t end = index + 1U < part_count
            ? owner->cache.input_batch_offsets[index + 1U]
            : result.batch_count;
        const int16_t model_type = owner->parts[index].character
            ? owner->parts[index].resource.character.model_type
            : owner->parts[index].resource.pitem.model_type;
        size_t batch_index;
        for (batch_index = begin; batch_index < end; ++batch_index)
            owner->batch_model_types[batch_index] = model_type;
    }
    if (scene != NULL) *scene = owner->scene;
    return GE_ORIGINAL_FRONTEND_CAST_MODEL_OK;
}

GeOriginalFrontendCastModel *ge_original_frontend_cast_model_create(
    GeAssetPack *asset_pack, GeOriginalFrontendCastModelStatus *out_status)
{
    GeOriginalFrontendCastModel *owner = NULL;
    GeOriginalFrontendCastModelStatus status =
        GE_ORIGINAL_FRONTEND_CAST_MODEL_INVALID_ARGUMENT;
    if (asset_pack == NULL) goto done;
    owner = calloc(1, sizeof(*owner));
    if (owner == NULL) {
        status = GE_ORIGINAL_FRONTEND_CAST_MODEL_ALLOCATION_FAILED;
        goto done;
    }
    owner->asset_pack = asset_pack;
    if (!load_asset(asset_pack, GE_CAST_ANIMATION_DATA_PATH,
                    &owner->animation_data, &owner->animation_data_size)
            || !load_asset(asset_pack, GE_CAST_ANIMATION_ENTRIES_PATH,
                    &owner->animation_entries,
                    &owner->animation_entries_size)) {
        status = GE_ORIGINAL_FRONTEND_CAST_MODEL_ASSET_MISSING;
        ge_original_frontend_cast_model_destroy(owner);
        owner = NULL;
        goto done;
    }
    status = GE_ORIGINAL_FRONTEND_CAST_MODEL_OK;
done:
    if (out_status != NULL) *out_status = status;
    return owner;
}

void ge_original_frontend_cast_model_destroy(
    GeOriginalFrontendCastModel *owner)
{
    if (owner == NULL) return;
    release_selection(owner);
    free(owner->animation_data);
    free(owner->animation_entries);
    free(owner->parts);
    free(owner->inputs);
    free(owner->vertices);
    free(owner->batches);
    free(owner->batch_model_types);
    free(owner);
}

GeOriginalFrontendCastModelStatus
ge_original_frontend_cast_model_begin_selection(
    GeOriginalFrontendCastModel *owner,
    const GeOriginalFrontendCastSelection *selection)
{
    GeOriginalCharacterModelStatus character_status;
    GeOriginalPitemModelStatus pitem_status;
    coord3d position;
    size_t attachment_switch;
    if (owner == NULL || selection == NULL || selection->body < 0
            || selection->head < -1 || selection->weapon_prop < -1
            || selection->animation_record_offset == 0U
            || !isfinite(selection->animation_start_frame)
            || !isfinite(selection->animation_playback_speed))
        return GE_ORIGINAL_FRONTEND_CAST_MODEL_INVALID_ARGUMENT;
    release_selection(owner);
    if (!ge_original_guard_animation_table_bind(
            owner->animation_data, owner->animation_data_size)
            || !ge_original_guard_animation_entries_bind(
                owner->animation_entries, owner->animation_entries_size)
            || (owner->animation = ge_port_guard_animation_resolve(
                selection->animation_record_offset)) == NULL)
        return GE_ORIGINAL_FRONTEND_CAST_MODEL_ANIMATION_UNAVAILABLE;
    owner->characters = ge_original_character_model_provider_create(
        owner->asset_pack, 2U, 1U, &character_status);
    if (owner->characters == NULL
            || !ge_original_character_model_resolve_pair(
                owner->characters, selection->body, selection->head, 0,
                &owner->character_pair)) {
        release_selection(owner);
        return GE_ORIGINAL_FRONTEND_CAST_MODEL_MODEL_UNAVAILABLE;
    }
    owner->body = owner->character_pair.model_instance;
    if (selection->weapon_prop >= 0) {
        float pitem_scale = 0.0f;
        owner->pitems = ge_original_pitem_model_provider_create(
            owner->asset_pack, 1U, 1U, &pitem_status);
        if (owner->pitems == NULL
                || !ge_original_pitem_model_resolve_instance(
                    owner->pitems, selection->weapon_prop,
                    &owner->weapon_header, (void **)&owner->weapon,
                    &pitem_scale)) {
            release_selection(owner);
            return GE_ORIGINAL_FRONTEND_CAST_MODEL_MODEL_UNAVAILABLE;
        }
    }
    owner->selection = *selection;
    memset(&position, 0, sizeof(position));
    modelSetScale(owner->body, GE_CAST_MODEL_SCALE);
    modelSetAnimTranslationScale(
        owner->body, GE_CAST_ANIMATION_TRANSLATION_SCALE);
    setsuboffset(owner->body, &position);
    setsubroty(owner->body, 0.0f);
    modelSetAnimPlaySpeed(owner->body, GE_CAST_ANIMATION_TICK_SPEED, 0.0f);
    modelSetAnimation(owner->body, owner->animation,
        selection->animation_flip != 0U,
        selection->animation_start_frame,
        selection->animation_playback_speed, 0.0f);
    if (owner->weapon != NULL) {
        attachment_switch = selection->animation_flip ? 5U : 3U;
        if (owner->body->obj == NULL || owner->body->obj->Switches == NULL
                || owner->body->obj->Switches[attachment_switch] == NULL) {
            release_selection(owner);
            return GE_ORIGINAL_FRONTEND_CAST_MODEL_ATTACHMENT_UNAVAILABLE;
        }
        modelSetScale(owner->weapon, GE_CAST_MODEL_SCALE);
        owner->weapon->attachedto = owner->body;
        owner->weapon->attachedto_objinst =
            owner->body->obj->Switches[attachment_switch];
        set_weapon_flash(owner->weapon, 0);
    }
    owner->animation_ticks = 0U;
    owner->bound = 1U;
    return GE_ORIGINAL_FRONTEND_CAST_MODEL_OK;
}

GeOriginalFrontendCastModelStatus ge_original_frontend_cast_model_tick(
    GeOriginalFrontendCastModel *owner, GeOriginalFrontendCast *cast,
    uint32_t clock_ticks, float timer_delta,
    GeOriginalFrontendCastModelScene *scene)
{
    ModelRenderData render_data;
    GeOriginalFrontendCastFrame frame;
    Mtxf view;
    Mtxf left_hand;
    Mtxf *attachment;
    coord3d root;
    coord3d transformed;
    GeOriginalFrontendCastModelStatus status;
    if (owner == NULL || cast == NULL || scene == NULL || !owner->bound
            || owner->body == NULL || clock_ticks == 0U
            || !isfinite(timer_delta) || timer_delta <= 0.0f
            || !same_selection(&owner->selection, &cast->selection))
        return GE_ORIGINAL_FRONTEND_CAST_MODEL_INVALID_ARGUMENT;
    modelTickAnim(owner->body, (int32_t)clock_ticks, 1);
    owner->animation_ticks += clock_ticks;
    modelSetDistanceDisabled(1);
    subcalcpos(owner->body);
    set_weapon_flash(owner->weapon, 0);

    identity(view.m);
    memset(&render_data, 0, sizeof(render_data));
    render_data.basemtx = &view;
    render_data.mtxlist = &owner->body->render_pos[0].pos;
    subcalcmatrices(&render_data, owner->body);
    getsuboffset(owner->body, &root);
    transformed.x = (root.x - cast->root_position_smoothed[0]) / timer_delta;
    transformed.y = (root.y - cast->root_position_smoothed[1]) / timer_delta;
    transformed.z = (root.z - cast->root_position_smoothed[2]) / timer_delta;
    mtx4TransformVecInPlace(
        (Mtxf *)(void *)owner->body->render_pos, &transformed);
    {
        const float root_position[3] = { root.x, root.y, root.z };
        const float transformed_target[3] = {
            transformed.x, transformed.y, transformed.z,
        };
        if (ge_original_frontend_cast_apply_pose(
                cast, root_position, transformed_target,
                clock_ticks, timer_delta) != GE_ORIGINAL_FRONTEND_CAST_OK
                || ge_original_frontend_cast_snapshot(
                    cast, &frame) != GE_ORIGINAL_FRONTEND_CAST_OK) {
            modelSetDistanceDisabled(0);
            return GE_ORIGINAL_FRONTEND_CAST_MODEL_INVALID_ARGUMENT;
        }
        memcpy(owner->scene.root_position, root_position,
               sizeof(owner->scene.root_position));
        memcpy(owner->scene.transformed_target, transformed_target,
               sizeof(owner->scene.transformed_target));
    }

    matrix_4x4_set_lookat_target(&view,
        frame.camera_eye[0], frame.camera_eye[1], frame.camera_eye[2],
        frame.camera_target[0], frame.camera_target[1],
        frame.camera_target[2], frame.camera_up[0], frame.camera_up[1],
        frame.camera_up[2]);
    memset(&render_data, 0, sizeof(render_data));
    render_data.basemtx = &view;
    render_data.mtxlist = &owner->body->render_pos[0].pos;
    subcalcmatrices(&render_data, owner->body);
    if (owner->weapon != NULL) {
        attachment = modelFindNodeMtx(
            owner->body, owner->weapon->attachedto_objinst, 0);
        if (attachment == NULL) {
            modelSetDistanceDisabled(0);
            return GE_ORIGINAL_FRONTEND_CAST_MODEL_ATTACHMENT_UNAVAILABLE;
        }
        if (owner->weapon->attachedto_objinst
                == owner->body->obj->Switches[5]) {
            matrix_4x4_set_rotation_around_z(M_PI_F, &left_hand);
            matrix_4x4_multiply_in_place(attachment, &left_hand);
            attachment = &left_hand;
        }
        memset(&render_data, 0, sizeof(render_data));
        render_data.basemtx = attachment;
        render_data.mtxlist = &owner->weapon->render_pos[0].pos;
        instcalcmatrices(&render_data, owner->weapon);
    }
    modelSetDistanceDisabled(0);
    status = build_scene(owner, &frame, scene);
    if (status == GE_ORIGINAL_FRONTEND_CAST_MODEL_OK) {
        memcpy(owner->scene.root_position,
            (const float[3]){ root.x, root.y, root.z },
            sizeof(owner->scene.root_position));
        memcpy(owner->scene.transformed_target,
            (const float[3]){ transformed.x, transformed.y, transformed.z },
            sizeof(owner->scene.transformed_target));
        *scene = owner->scene;
    }
    return status;
}

const char *ge_original_frontend_cast_model_status_name(
    GeOriginalFrontendCastModelStatus status)
{
    switch (status) {
    case GE_ORIGINAL_FRONTEND_CAST_MODEL_OK: return "ok";
    case GE_ORIGINAL_FRONTEND_CAST_MODEL_INVALID_ARGUMENT:
        return "invalid-argument";
    case GE_ORIGINAL_FRONTEND_CAST_MODEL_ASSET_MISSING: return "asset-missing";
    case GE_ORIGINAL_FRONTEND_CAST_MODEL_MODEL_UNAVAILABLE:
        return "model-unavailable";
    case GE_ORIGINAL_FRONTEND_CAST_MODEL_ANIMATION_UNAVAILABLE:
        return "animation-unavailable";
    case GE_ORIGINAL_FRONTEND_CAST_MODEL_ATTACHMENT_UNAVAILABLE:
        return "attachment-unavailable";
    case GE_ORIGINAL_FRONTEND_CAST_MODEL_SCENE_UNAVAILABLE:
        return "scene-unavailable";
    case GE_ORIGINAL_FRONTEND_CAST_MODEL_ALLOCATION_FAILED:
        return "allocation-failed";
    }
    return "unknown";
}
