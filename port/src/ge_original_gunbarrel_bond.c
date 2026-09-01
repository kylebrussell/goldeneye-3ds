#include "ge_original_gunbarrel_bond.h"

#include "ge_original_character_models.h"
#include "ge_original_guard_animation_table.h"
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

#define GE_GUNBARREL_WALK_ANIMATION_OFFSET UINT32_C(0x4144)
#define GE_GUNBARREL_FIRE_ANIMATION_OFFSET UINT32_C(0x4298)
#define GE_GUNBARREL_ANIMATION_START_TICK UINT32_C(137)
#define GE_GUNBARREL_ANIMATION_SPEEDUP_TICK UINT32_C(212)
#define GE_GUNBARREL_ANIMATION_DATA_PATH \
    "converted/animations/bond/animation_data.bin"
#define GE_GUNBARREL_ANIMATION_ENTRIES_PATH \
    "converted/animations/bond/animation_entries.bin"

typedef struct GeOriginalGunbarrelPart {
    uint8_t character;
    union {
        GeOriginalCharacterModelScenePart character;
        GeOriginalPitemModelScenePart pitem;
    } resource;
} GeOriginalGunbarrelPart;

struct GeOriginalGunbarrelBond {
    GeAssetPack *asset_pack;
    GeOriginalCharacterModelProvider *characters;
    GeOriginalPitemModelProvider *pitems;
    GeOriginalCharacterModelPair character_pair;
    void *gun_header;
    Model *body;
    Model *gun;
    ModelAnimation *walk_animation;
    ModelAnimation *fire_animation;
    uint8_t *animation_data;
    size_t animation_data_size;
    uint8_t *animation_entries;
    size_t animation_entries_size;
    GeOriginalGunbarrelAssets assets;
    GeOriginalGunbarrelPart *parts;
    GeOriginalModelSceneInput *inputs;
    size_t initial_character_part_count;
    size_t initial_gun_part_count;
    size_t initial_part_capacity;
    size_t part_capacity;
    GeDamRoomWorldVertex *vertices;
    GeDamRoomDrawBatch *batches;
    int16_t *batch_model_types;
    size_t vertex_capacity;
    size_t batch_capacity;
    GeOriginalModelSceneCache cache;
    GeOriginalGunbarrelBondScene scene;
    uint32_t animation_timer;
    GeOriginalGunbarrelBondStatus status;
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

static void set_gun_flash(Model *gun, int visible)
{
    if (gun == NULL || gun->obj == NULL) return;
    if (gun->obj->Switches != NULL && gun->obj->Switches[0] != NULL)
        modelGetNodeRwData(gun, gun->obj->Switches[0])->Gunfire.visible =
            visible != 0;
    if (gun->obj->Switches != NULL && gun->obj->Switches[2] != NULL)
        modelGetNodeRwData(gun, gun->obj->Switches[2])->Switch.visible =
            visible != 0;
}

static GeOriginalGunbarrelBondStatus collect_parts(
    GeOriginalGunbarrelBond *bond, size_t *count,
    size_t *character_parts, size_t *gun_parts)
{
    size_t character_count;
    size_t gun_count;
    size_t index;
    if (bond == NULL || count == NULL) return
        GE_ORIGINAL_GUNBARREL_BOND_INVALID_ARGUMENT;
    if (!ge_original_character_model_prepare_instance_relations(
            bond->characters, bond->body))
        return GE_ORIGINAL_GUNBARREL_BOND_MODEL_UNAVAILABLE;
    character_count = ge_original_character_model_instance_scene_part_count(
        bond->characters, bond->body);
    gun_count = ge_original_pitem_model_instance_scene_part_count(
        bond->pitems, bond->gun);
    if (character_count == 0U || gun_count == 0U)
        return GE_ORIGINAL_GUNBARREL_BOND_SCENE_UNAVAILABLE;
    if (character_count > SIZE_MAX - gun_count)
        return GE_ORIGINAL_GUNBARREL_BOND_ALLOCATION_FAILED;
    if (character_count + gun_count > bond->part_capacity) {
        const size_t required = character_count + gun_count;
        GeOriginalGunbarrelPart *parts = realloc(
            bond->parts, required * sizeof(*parts));
        GeOriginalModelSceneInput *inputs;
        if (parts == NULL)
            return GE_ORIGINAL_GUNBARREL_BOND_ALLOCATION_FAILED;
        bond->parts = parts;
        inputs = realloc(bond->inputs, required * sizeof(*inputs));
        if (inputs == NULL)
            return GE_ORIGINAL_GUNBARREL_BOND_ALLOCATION_FAILED;
        bond->inputs = inputs;
        bond->part_capacity = required;
    }
    for (index = 0U; index < character_count; ++index) {
        bond->parts[index].character = 1U;
        if (!ge_original_character_model_instance_scene_part(
                bond->characters, bond->body, index,
                &bond->parts[index].resource.character))
            return GE_ORIGINAL_GUNBARREL_BOND_SCENE_UNAVAILABLE;
    }
    for (index = 0U; index < gun_count; ++index) {
        GeOriginalGunbarrelPart *part =
            &bond->parts[character_count + index];
        part->character = 0U;
        if (!ge_original_pitem_model_instance_scene_part(
                bond->pitems, bond->gun, index, &part->resource.pitem))
            return GE_ORIGINAL_GUNBARREL_BOND_SCENE_UNAVAILABLE;
    }
    *count = character_count + gun_count;
    if (character_parts != NULL) *character_parts = character_count;
    if (gun_parts != NULL) *gun_parts = gun_count;
    return GE_ORIGINAL_GUNBARREL_BOND_OK;
}

static GeOriginalGunbarrelBondStatus build_scene(
    GeOriginalGunbarrelBond *bond, GeOriginalGunbarrelBondScene *scene)
{
    GeOriginalModelScene result;
    GeOriginalModelSceneStatus model_status;
    size_t part_count;
    size_t character_part_count;
    size_t gun_part_count;
    size_t index;
    GeOriginalGunbarrelBondStatus status = collect_parts(
        bond, &part_count, &character_part_count, &gun_part_count);
    if (status != GE_ORIGINAL_GUNBARREL_BOND_OK) return status;
    for (index = 0U; index < part_count; ++index) {
        const GeOriginalGunbarrelPart *part = &bond->parts[index];
        GeOriginalModelSceneInput *input = &bond->inputs[index];
        memset(input, 0, sizeof(*input));
        if (part->character) {
            input->blob = part->resource.character.blob;
            input->blob_size = part->resource.character.blob_size;
            input->primary_offset = part->resource.character.primary_offset;
            input->secondary_offset = part->resource.character.secondary_offset;
            input->segment4_offset = part->resource.character.segment4_offset;
            input->segment3_matrices =
                (const float (*)[4][4])(const void *)bond->body->render_pos;
            input->segment3_matrix_count = bond->character_pair.matrix_count;
        } else {
            input->blob = part->resource.pitem.blob;
            input->blob_size = part->resource.pitem.blob_size;
            input->primary_offset = part->resource.pitem.primary_offset;
            input->secondary_offset = part->resource.pitem.secondary_offset;
            input->segment4_offset = part->resource.pitem.segment4_offset;
            input->segment3_matrices =
                (const float (*)[4][4])(const void *)bond->gun->render_pos;
            input->segment3_matrix_count = (size_t)bond->gun->obj->numMatrices;
        }
        input->world_zbuffer_enabled = 0U;
        identity(input->matrix);
    }
    model_status = ge_original_model_scene_cache_build(
        &bond->cache, bond->inputs, part_count, NULL, &result);
    if (model_status != GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED
            && model_status != GE_ORIGINAL_MODEL_SCENE_OK)
        return GE_ORIGINAL_GUNBARREL_BOND_SCENE_UNAVAILABLE;
    if (result.required_vertex_count > bond->vertex_capacity
            || result.required_batch_count > bond->batch_capacity) {
        GeDamRoomWorldVertex *vertices = realloc(
            bond->vertices, result.required_vertex_count * sizeof(*vertices));
        GeDamRoomDrawBatch *batches;
        if (vertices == NULL && result.required_vertex_count != 0U)
            return GE_ORIGINAL_GUNBARREL_BOND_ALLOCATION_FAILED;
        bond->vertices = vertices;
        batches = realloc(bond->batches,
            result.required_batch_count * sizeof(*batches));
        if (batches == NULL && result.required_batch_count != 0U)
            return GE_ORIGINAL_GUNBARREL_BOND_ALLOCATION_FAILED;
        bond->batches = batches;
        {
            int16_t *model_types = realloc(bond->batch_model_types,
                result.required_batch_count * sizeof(*model_types));
            if (model_types == NULL && result.required_batch_count != 0U)
                return GE_ORIGINAL_GUNBARREL_BOND_ALLOCATION_FAILED;
            bond->batch_model_types = model_types;
        }
        bond->vertex_capacity = result.required_vertex_count;
        bond->batch_capacity = result.required_batch_count;
    }
    {
        const GeDamRoomSceneStorage storage = {
            bond->vertices, bond->vertex_capacity,
            bond->batches, bond->batch_capacity,
        };
        model_status = ge_original_model_scene_cache_build(
            &bond->cache, bond->inputs, part_count, &storage, &result);
    }
    if (model_status != GE_ORIGINAL_MODEL_SCENE_OK)
        return GE_ORIGINAL_GUNBARREL_BOND_SCENE_UNAVAILABLE;
    bond->scene.vertices = bond->vertices;
    bond->scene.batches = bond->batches;
    bond->scene.batch_model_types = bond->batch_model_types;
    bond->scene.vertex_count = result.vertex_count;
    bond->scene.batch_count = result.batch_count;
    bond->scene.triangle_count = result.triangle_count;
    bond->scene.part_count = part_count;
    bond->scene.character_part_count = character_part_count;
    bond->scene.gun_part_count = gun_part_count;
    bond->scene.initial_character_part_count =
        bond->initial_character_part_count;
    bond->scene.initial_gun_part_count = bond->initial_gun_part_count;
    bond->scene.initial_part_capacity = bond->initial_part_capacity;
    bond->scene.allocated_part_capacity = bond->part_capacity;
    bond->scene.animation_timer = bond->animation_timer;
    for (index = 0U; index < part_count; ++index) {
        const size_t begin = bond->cache.input_batch_offsets[index];
        const size_t end = index + 1U < part_count
            ? bond->cache.input_batch_offsets[index + 1U]
            : result.batch_count;
        const int16_t model_type = bond->parts[index].character
            ? bond->parts[index].resource.character.model_type
            : bond->parts[index].resource.pitem.model_type;
        size_t batch_index;
        for (batch_index = begin; batch_index < end; ++batch_index)
            bond->batch_model_types[batch_index] = model_type;
    }
    /* Exact title.c gunbarrelRenderData state immediately before its two
     * drawjointlist passes.  In the VIEWER branch, model types 3/4 use vertex
     * alpha as lighting intensity with black environment and fog colours. */
    bond->scene.render_prop_type = 7U;
    bond->scene.render_zbuffer_enabled = 0U;
    bond->scene.render_cull_mode = 3U; /* CULLMODE_BOTH */
    bond->scene.render_primary_flags = 1U;
    bond->scene.render_secondary_flags = 2U;
    bond->scene.shadow_alpha = 80U;
    bond->scene.viewer_uses_vertex_alpha_lighting = 1U;
    bond->scene.render_environment_rgba = 0U;
    bond->scene.render_fog_rgba = 0U;
    if (scene != NULL) *scene = bond->scene;
    return GE_ORIGINAL_GUNBARREL_BOND_OK;
}

GeOriginalGunbarrelBond *ge_original_gunbarrel_bond_create(
    GeAssetPack *asset_pack, GeOriginalGunbarrelBondStatus *out_status)
{
    GeOriginalGunbarrelBond *bond;
    GeOriginalCharacterModelStatus character_status;
    GeOriginalPitemModelStatus pitem_status;
    GeOriginalGunbarrelBondStatus status =
        GE_ORIGINAL_GUNBARREL_BOND_INVALID_ARGUMENT;
    if (asset_pack == NULL) goto done;
    bond = calloc(1, sizeof(*bond));
    if (bond == NULL) {
        status = GE_ORIGINAL_GUNBARREL_BOND_ALLOCATION_FAILED;
        goto done;
    }
    bond->asset_pack = asset_pack;
    ge_original_gunbarrel_assets(&bond->assets);
    if (!load_asset(asset_pack, GE_GUNBARREL_ANIMATION_DATA_PATH,
                    &bond->animation_data, &bond->animation_data_size)
            || !load_asset(asset_pack, GE_GUNBARREL_ANIMATION_ENTRIES_PATH,
                    &bond->animation_entries, &bond->animation_entries_size)) {
        status = GE_ORIGINAL_GUNBARREL_BOND_ASSET_MISSING;
        goto fail;
    }
    if (!ge_original_guard_animation_table_bind(
                bond->animation_data, bond->animation_data_size)
            || !ge_original_guard_animation_entries_bind(
                bond->animation_entries, bond->animation_entries_size)
            || (bond->walk_animation = ge_port_guard_animation_resolve(
                    GE_GUNBARREL_WALK_ANIMATION_OFFSET)) == NULL
            || (bond->fire_animation = ge_port_guard_animation_resolve(
                    GE_GUNBARREL_FIRE_ANIMATION_OFFSET)) == NULL) {
        status = GE_ORIGINAL_GUNBARREL_BOND_ANIMATION_UNAVAILABLE;
        goto fail;
    }
    bond->characters = ge_original_character_model_provider_create(
        asset_pack, 2U, 1U, &character_status);
    bond->pitems = ge_original_pitem_model_provider_create(
        asset_pack, 1U, 1U, &pitem_status);
    if (bond->characters == NULL || bond->pitems == NULL
            || !ge_original_character_model_resolve_pair(
                bond->characters, bond->assets.body_model,
                bond->assets.head_model, 0, &bond->character_pair)
            || !ge_original_pitem_model_resolve_instance(
                bond->pitems, bond->assets.gun_model,
                &bond->gun_header, (void **)&bond->gun,
                &bond->assets.model_scale)) {
        status = GE_ORIGINAL_GUNBARREL_BOND_MODEL_UNAVAILABLE;
        goto fail;
    }
    /* The Pitem scale output is not initializeGunBarrelIntro's scale. */
    ge_original_gunbarrel_assets(&bond->assets);
    bond->body = bond->character_pair.model_instance;
    bond->initial_character_part_count =
        ge_original_character_model_instance_scene_part_count(
            bond->characters, bond->body);
    bond->initial_gun_part_count = ge_original_pitem_model_scene_part_count(
        bond->pitems, bond->assets.gun_model);
    bond->part_capacity = bond->initial_character_part_count
        + bond->initial_gun_part_count;
    bond->initial_part_capacity = bond->part_capacity;
    bond->parts = calloc(bond->part_capacity, sizeof(*bond->parts));
    bond->inputs = calloc(bond->part_capacity, sizeof(*bond->inputs));
    if (bond->part_capacity == 0U || bond->parts == NULL
            || bond->inputs == NULL) {
        status = GE_ORIGINAL_GUNBARREL_BOND_ALLOCATION_FAILED;
        goto fail;
    }
    status = ge_original_gunbarrel_bond_reset(bond);
    if (status != GE_ORIGINAL_GUNBARREL_BOND_OK) goto fail;
    if (out_status != NULL) *out_status = status;
    return bond;
fail:
    ge_original_gunbarrel_bond_destroy(bond);
done:
    if (out_status != NULL) *out_status = status;
    return NULL;
}

void ge_original_gunbarrel_bond_destroy(GeOriginalGunbarrelBond *bond)
{
    if (bond == NULL) return;
    ge_original_model_scene_cache_close(&bond->cache);
    ge_original_character_model_provider_destroy(bond->characters);
    ge_original_pitem_model_provider_destroy(bond->pitems);
    free(bond->animation_data);
    free(bond->animation_entries);
    free(bond->parts);
    free(bond->inputs);
    free(bond->vertices);
    free(bond->batches);
    free(bond->batch_model_types);
    free(bond);
}

GeOriginalGunbarrelBondStatus ge_original_gunbarrel_bond_reset(
    GeOriginalGunbarrelBond *bond)
{
    coord3d position;
    int32_t start_frame;
    if (bond == NULL || bond->body == NULL || bond->gun == NULL
            || bond->walk_animation == NULL || bond->body->obj == NULL
            || bond->body->obj->Switches == NULL
            || bond->body->obj->Switches[3] == NULL)
        return GE_ORIGINAL_GUNBARREL_BOND_INVALID_ARGUMENT;
    memset(&position, 0, sizeof(position));
    modelSetScale(bond->body, bond->assets.model_scale);
    modelSetAnimTranslationScale(
        bond->body, bond->assets.animation_translation_scale);
    setsuboffset(bond->body, &position);
    setsubroty(bond->body, 0.0f);
    modelSetAnimPlaySpeed(
        bond->body, bond->assets.animation_play_speed, 0.0f);
    start_frame = (int32_t)bond->walk_animation->unk04
        - bond->assets.walk_animation_frame_backstep;
    while (start_frame < 0) start_frame += bond->walk_animation->unk04;
    modelSetAnimation(bond->body, bond->walk_animation, 0,
                      (float)start_frame, 0.91f, 0.0f);
    modelSetScale(bond->gun, bond->assets.model_scale);
    bond->gun->attachedto = bond->body;
    bond->gun->attachedto_objinst = bond->body->obj->Switches[3];
    set_gun_flash(bond->gun, 0);
    bond->animation_timer = 0U;
    memset(&bond->scene, 0, sizeof(bond->scene));
    return bond->status = GE_ORIGINAL_GUNBARREL_BOND_OK;
}

GeOriginalGunbarrelBondStatus ge_original_gunbarrel_bond_tick(
    GeOriginalGunbarrelBond *bond,
    const GeOriginalGunbarrelFrame *frame,
    GeOriginalGunbarrelBondScene *scene)
{
    ModelRenderData render_data;
    Mtxf view;
    Mtxf *attachment;
    uint8_t tick;
    if (bond == NULL || frame == NULL || scene == NULL)
        return GE_ORIGINAL_GUNBARREL_BOND_INVALID_ARGUMENT;
    for (tick = 0U; tick < frame->bond_animation_ticks; ++tick) {
        ++bond->animation_timer;
        if (bond->animation_timer == GE_GUNBARREL_ANIMATION_START_TICK
                && frame->animation_start)
            modelSetAnimation(bond->body, bond->fire_animation, 0,
                              2.0f, 0.910000026f, 16.0f);
        if (bond->animation_timer == GE_GUNBARREL_ANIMATION_SPEEDUP_TICK
                && frame->animation_speedup)
            modelSetAnimSpeed(bond->body, 1.6f, 8.0f);
        modelTickAnim(bond->body, 1, 1);
    }
    set_gun_flash(bond->gun, frame->fire_shot != 0U);
    modelSetDistanceDisabled(1);
    subcalcpos(bond->body);
    matrix_4x4_set_lookat_target(&view,
        bond->assets.camera_position[0], bond->assets.camera_position[1],
        bond->assets.camera_position[2],
        bond->assets.camera_position[0] + bond->assets.camera_direction[0],
        bond->assets.camera_position[1] + bond->assets.camera_direction[1],
        bond->assets.camera_position[2] + bond->assets.camera_direction[2],
        bond->assets.camera_up[0], bond->assets.camera_up[1],
        bond->assets.camera_up[2]);
    memset(&render_data, 0, sizeof(render_data));
    render_data.basemtx = &view;
    render_data.mtxlist = &bond->body->render_pos[0].pos;
    subcalcmatrices(&render_data, bond->body);
    attachment = modelFindNodeMtx(
        bond->body, bond->gun->attachedto_objinst, 0);
    if (attachment == NULL) {
        modelSetDistanceDisabled(0);
        return bond->status =
            GE_ORIGINAL_GUNBARREL_BOND_ATTACHMENT_UNAVAILABLE;
    }
    memset(&render_data, 0, sizeof(render_data));
    render_data.basemtx = attachment;
    render_data.mtxlist = &bond->gun->render_pos[0].pos;
    instcalcmatrices(&render_data, bond->gun);
    modelSetDistanceDisabled(0);
    bond->scene.muzzle_flash_visible = frame->fire_shot != 0U;
    bond->status = build_scene(bond, scene);
    scene->muzzle_flash_visible = frame->fire_shot != 0U;
    scene->animation_timer = bond->animation_timer;
    return bond->status;
}

const char *ge_original_gunbarrel_bond_status_name(
    GeOriginalGunbarrelBondStatus status)
{
    switch (status) {
    case GE_ORIGINAL_GUNBARREL_BOND_OK: return "ok";
    case GE_ORIGINAL_GUNBARREL_BOND_INVALID_ARGUMENT: return "invalid-argument";
    case GE_ORIGINAL_GUNBARREL_BOND_ASSET_MISSING: return "asset-missing";
    case GE_ORIGINAL_GUNBARREL_BOND_MODEL_UNAVAILABLE: return "model-unavailable";
    case GE_ORIGINAL_GUNBARREL_BOND_ANIMATION_UNAVAILABLE: return "animation-unavailable";
    case GE_ORIGINAL_GUNBARREL_BOND_ATTACHMENT_UNAVAILABLE: return "attachment-unavailable";
    case GE_ORIGINAL_GUNBARREL_BOND_MATRIX_UNAVAILABLE: return "matrix-unavailable";
    case GE_ORIGINAL_GUNBARREL_BOND_SCENE_UNAVAILABLE: return "scene-unavailable";
    case GE_ORIGINAL_GUNBARREL_BOND_ALLOCATION_FAILED: return "allocation-failed";
    }
    return "unknown";
}
