#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/bondview.h"

#include "ge_original_guard_animation_table.h"
#include "ge_original_dam_guard_model.h"
#include "ge_original_character_models.h"
#include "ge_asset_pack.h"
#include "ge_original_player_gait_internal.h"
#include "assets/animationtable_data.h"
#include "assets/animationtable_entries.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The exact model slice also retains getPlayer_c_lodscalez.  This focused
 * animation harness never enters that player-owned branch, but provide its
 * canonical service boundary so dead-strip behavior is not linker-specific. */
static struct player harness_player;
struct player *ge_original_spawn_player_get(void) { return &harness_player; }

static unsigned char *read_file(const char *path, size_t *size)
{
    FILE *file = fopen(path, "rb");
    unsigned char *bytes;
    long length;
    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    length = ftell(file);
    assert(length > 0);
    assert(fseek(file, 0, SEEK_SET) == 0);
    bytes = malloc((size_t)length);
    assert(bytes != NULL);
    assert(fread(bytes, 1, (size_t)length, file) == (size_t)length);
    fclose(file);
    *size = (size_t)length;
    return bytes;
}

static void exercise_campaign_pair(
    GeOriginalCharacterModelProvider *provider,int32_t body_id,
    int32_t head_id,ModelAnimation *animation)
{
    GeOriginalCharacterModelPair pair;Model *model;ModelRenderData renderdata;
    Mtxf base;size_t tick,matrix,row,column;
    assert(ge_original_character_model_resolve_pair(
        provider,body_id,head_id,0,&pair));
    model=pair.model_instance;
    assert(model!=NULL&&model->obj!=NULL&&model->render_pos!=NULL
           &&ge_original_character_model_prepare_instance_relations(
               provider,model));
    memset(&base,0,sizeof(base));
    for(row=0U;row<4U;++row)base.m[row][row]=1.0f;
    modelSetAnimation(model,animation,0,0.0f,0.5f,0.0f);
    for(tick=0U;tick<120U;++tick){
        memset(&renderdata,0,sizeof(renderdata));
        renderdata.basemtx=&base;
        renderdata.mtxlist=&model->render_pos[0].pos;
        modelTickAnim(model,1,1);
        subcalcmatrices(&renderdata,model);
        for(matrix=0U;matrix<pair.matrix_count;++matrix)
            for(row=0U;row<4U;++row)for(column=0U;column<4U;++column)
                assert(isfinite(model->render_pos[matrix].pos.m[row][column]));
    }
    assert(model->anim==animation&&model->animframe1>0.0f);
}

static float exercise_guard_pose_bounds(Model *model,
                                        ModelAnimation *animation)
{
    RenderPosView matrices[0x14];
    ModelRenderData renderdata;
    Mtxf base;
    float maximum_joint_distance = 0.0f;
    size_t tick;
    size_t matrix;
    size_t row;
    size_t column;

    assert(model != NULL && model->obj != NULL
           && model->obj->numMatrices <= 0x14);
    memset(&base, 0, sizeof(base));
    for (row = 0U; row < 4U; row++) base.m[row][row] = 1.0f;
    modelSetAnimation(model, animation, 0, 0.0f, 0.5f, 0.0f);
    for (tick = 0U; tick < 120U; tick++) {
        Mtxf *root;
        modelTickAnim(model, 1, 1);
        memset(&renderdata, 0, sizeof(renderdata));
        renderdata.basemtx = &base;
        renderdata.mtxlist = &matrices[0].pos;
        subcalcmatrices(&renderdata, model);
        root = modelFindNodeMtx(model, model->obj->RootNode, 0);
        assert(root != NULL);
        for (matrix = 0U; matrix < (size_t)model->obj->numMatrices;
                matrix++) {
            float dx;
            float dy;
            float dz;
            float distance;
            for (row = 0U; row < 4U; row++)
                for (column = 0U; column < 4U; column++)
                    assert(isfinite(matrices[matrix].pos.m[row][column]));
            dx = matrices[matrix].pos.m[3][0] - root->m[3][0];
            dy = matrices[matrix].pos.m[3][1] - root->m[3][1];
            dz = matrices[matrix].pos.m[3][2] - root->m[3][2];
            distance = sqrtf(dx * dx + dy * dy + dz * dz);
            if (distance > maximum_joint_distance)
                maximum_joint_distance = distance;
        }
    }
    return maximum_joint_distance;
}

int main(int argc, char **argv)
{
    unsigned char *records;
    unsigned char *entries;
    size_t records_size;
    size_t entries_size;
    Model guard_model = {0};
    ModelFileHeader guard_header = {0};
    ModelNode guard_root = {0};
    union ModelRoData guard_rodata = {0};
    ModelAnimation *death_animation;
    ModelAnimation *idle_animation;
    ModelAnimation *fire_animation;
    ModelAnimation *adjacent_animation;
    ModelAnimation *adjacent_next_animation;
    u16 adjacent_frame_count;
    ModelAnimBitField *adjacent_descriptors;
    u8 *adjacent_stream;
    ModelJoint root_joint = {0};
    ModelSkeleton root_skeleton = {0};
    Model authored_guard = {0};
    ModelFileHeader *authored_header;
    u32 authored_rwdata[512] = {0};
    ModelRwData_HeaderRecord *authored_root_rwdata;
    coord3d rotation_zero;
    coord3d rotation_two;
    const uint8_t *frame_zero;
    const uint8_t *frame_two;
    int32_t frame_zero_handle;
    int32_t frame_two_handle;
    size_t frame_size;
    float idle_joint_extent;
    float fire_joint_extent;

    GeAssetPack pack;GeOriginalCharacterModelProvider *provider;
    GeOriginalCharacterModelStatus provider_status;

    assert(argc == 4);
    harness_player.c_lodscalez = 1.0f;
    records = read_file(argv[1], &records_size);
    entries = read_file(argv[2], &entries_size);
    assert(ge_original_guard_animation_table_bind(records, records_size));
    assert(ge_original_guard_animation_entries_bind(entries, entries_size));
    death_animation = ge_port_guard_animation_resolve(PTR_ANIM_death_head);
    idle_animation = ge_port_guard_animation_resolve(PTR_ANIM_idle);
    fire_animation = ge_port_guard_animation_resolve(
        PTR_ANIM_fire_standing_fast);
    assert(death_animation != NULL);
    assert(idle_animation != NULL && fire_animation != NULL);
    assert(ge_port_guard_animation_resolve(PTR_ANIM_death_head)
           == death_animation);
    assert(ge_port_guard_animation_owns(death_animation));

    /* These authored records are only 0x30 bytes apart. The native table
     * publishes exactly the 20-byte N64 animation header at each offset, so
     * resolving the latter must not disturb the former. */
    assert(PTR_ANIM_bond_watch > PTR_ANIM_bond_eye_fire
           && PTR_ANIM_bond_watch - PTR_ANIM_bond_eye_fire >= 20U);
    adjacent_animation = ge_port_guard_animation_resolve(
        PTR_ANIM_bond_eye_fire);
    assert(adjacent_animation != NULL);
    adjacent_frame_count = adjacent_animation->unk04;
    adjacent_descriptors = adjacent_animation->bitDescriptors;
    adjacent_stream = adjacent_animation->bitStream;
    adjacent_next_animation = ge_port_guard_animation_resolve(
        PTR_ANIM_bond_watch);
    assert(adjacent_next_animation != NULL
           && adjacent_next_animation != adjacent_animation);
    assert(adjacent_animation->unk04 == adjacent_frame_count
           && adjacent_animation->bitDescriptors == adjacent_descriptors
           && adjacent_animation->bitStream == adjacent_stream);
    assert(ge_port_guard_animation_owns(adjacent_animation)
           && ge_port_guard_animation_owns(adjacent_next_animation));
    assert(death_animation->address == PTR_ANIM_ENTRY_death_head);
    assert(death_animation->unk04 > 2U);
    frame_size = (size_t)(death_animation->unk0E >> 3);
    assert(frame_size > 0U);

    guard_root.Opcode = MODELNODE_OPCODE_GROUPSIMPLE;
    guard_root.Data = &guard_rodata;
    guard_header.RootNode = &guard_root;
    guard_model.obj = &guard_header;
    guard_model.playspeed = 1.0f;
    modelSetAnimation(&guard_model, death_animation, 0, 0.0f, 0.5f, 0.0f);
    assert(guard_model.framea == 0);
    frame_zero_handle = ge_port_player_gait_load_animation_frame(
        death_animation, guard_model.framea, NULL);
    assert(frame_zero_handle < 0);
    frame_zero = ge_port_guard_animation_frame_data(frame_zero_handle);
    assert(frame_zero != NULL);
    assert(memcmp(frame_zero, entries + PTR_ANIM_ENTRY_death_head,
                  frame_size) == 0);

    modelTickAnim(&guard_model, 4, 0);
    assert(fabsf(guard_model.animframe1 - 2.0f) < 0.000001f);
    assert(guard_model.framea == 2);
    frame_two_handle = ge_port_player_gait_load_animation_frame(
        death_animation, guard_model.framea, NULL);
    assert(frame_two_handle < 0 && frame_two_handle != frame_zero_handle);
    frame_two = ge_port_guard_animation_frame_data(frame_two_handle);
    assert(frame_two != NULL);
    assert(memcmp(frame_two,
                  entries + PTR_ANIM_ENTRY_death_head + 2U * frame_size,
                  frame_size) == 0);
    assert(memcmp(frame_zero, frame_two, frame_size) != 0);
    root_joint.NodeType = 0x401U;
    root_skeleton.numjoints = 1;
    root_skeleton.Joints = &root_joint;
    root_skeleton.SkeletonSize = 3;
    ge_port_player_gait_decode_joint_handle(
        0, 0, &root_skeleton, death_animation,
        frame_zero_handle, &rotation_zero);
    ge_port_player_gait_decode_joint_handle(
        0, 0, &root_skeleton, death_animation,
        frame_two_handle, &rotation_two);
    assert(isfinite(rotation_zero.x) && isfinite(rotation_zero.y)
           && isfinite(rotation_zero.z));
    assert(isfinite(rotation_two.x) && isfinite(rotation_two.y)
           && isfinite(rotation_two.z));
    assert(rotation_zero.x != rotation_two.x
           || rotation_zero.y != rotation_two.y
           || rotation_zero.z != rotation_two.z);

    /* The earlier GROUPSIMPLE fixture intentionally bypasses the character
     * root-motion branch. Exercise the same authored HEADER root, skeleton,
     * rwdata indexing and animation clock used by live Dam guards. */
    authored_header = ge_original_dam_guard_model_header();
    assert(authored_header != NULL);
    modelCalculateRwDataLen(authored_header);
    assert(authored_header->numRecords > 0
           && authored_header->numRecords <= 512);
    animInit(&authored_guard, authored_header, authored_rwdata);
    assert(authored_guard.obj == authored_header);
    assert(authored_guard.datas == (union ModelRwData **)authored_rwdata);
    authored_root_rwdata = &modelGetNodeRwData(
        &authored_guard, authored_header->RootNode)->Header;
    authored_root_rwdata->pos.x = 10.0f;
    authored_root_rwdata->pos.y = 20.0f;
    authored_root_rwdata->pos.z = 30.0f;
    modelSetAnimation(&authored_guard, death_animation, 0,
                      0.0f, 0.5f, 0.0f);
    assert(authored_guard.anim == death_animation);
    assert(isfinite(authored_root_rwdata->unk24.x)
           && isfinite(authored_root_rwdata->unk24.y)
           && isfinite(authored_root_rwdata->unk24.z));
    modelTickAnim(&authored_guard, 4, 1);
    assert(fabsf(authored_guard.animframe1 - 2.0f) < 0.000001f);
    assert(authored_guard.framea == 2);
    assert(isfinite(authored_root_rwdata->unk34.x)
           && isfinite(authored_root_rwdata->unk34.y)
           && isfinite(authored_root_rwdata->unk34.z));
    authored_guard.scale = 0.1f;
    idle_joint_extent = exercise_guard_pose_bounds(
        &authored_guard, idle_animation);
    fire_joint_extent = exercise_guard_pose_bounds(
        &authored_guard, fire_animation);
    assert(idle_joint_extent > 1.0f && idle_joint_extent < 500.0f);
    assert(fire_joint_extent > 1.0f && fire_joint_extent < 500.0f);
    printf("guard matrix bounds: idle=%.3f fire=%.3f\n",
           idle_joint_extent, fire_joint_extent);

    assert(ge_asset_pack_open(&pack,argv[3])==GE_ASSET_PACK_OK);
    provider=ge_original_character_model_provider_create(
        &pack,4U,2U,&provider_status);
    assert(provider!=NULL&&provider_status==GE_ORIGINAL_CHARACTER_MODEL_OK);
    exercise_campaign_pair(provider,2,42,death_animation);
    exercise_campaign_pair(provider,17,66,death_animation);
    ge_original_character_model_provider_destroy(provider);
    ge_asset_pack_close(&pack);

    ge_original_guard_animation_table_reset();
    free(entries);
    free(records);
    puts("canonical guard animation: Dam + sustained Facility/Depot matrices passed");
    return 0;
}
