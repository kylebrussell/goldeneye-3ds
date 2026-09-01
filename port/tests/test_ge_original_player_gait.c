#include "ge_original_player_gait.h"
#include "ge_original_player_gait_internal.h"
#include "ge_original_guard_animation_table.h"
#include "ge_original_player_spawn_internal.h"

/* The bounded spawn object retains the exact intro tail which counts this
 * zero-terminated table.  The production link supplies the canonical table;
 * this focused gait binary only needs a representative terminator. */
s32 g_bondviewBondDeathAnimations[] = { 1, 0 };
s32 g_bondviewBondDeathAnimationsCount;
#include "assets/animationtable_data.h"
#include "assets/animationtable_entries.h"
typedef int PLAYERFLAG;
#include "game/bondview.h"
#include "game/bondhead.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

s32 g_ClockTimer;
void bheadUpdatePos(coord3d *velocity);

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

static int matrix_is_finite(float matrix[4][4])
{
    int row;
    int column;
    for (row = 0; row < 4; ++row) {
        for (column = 0; column < 4; ++column) {
            if (!isfinite(matrix[row][column])) {
                return 0;
            }
        }
    }
    return 1;
}

int main(int argc, char **argv)
{
    unsigned char *data;
    unsigned char *walk_frames;
    unsigned char *sprint_frames;
    unsigned char *idle_frames;
    unsigned char *guard_frames;
    size_t size;
    size_t walk_frames_size;
    size_t sprint_frames_size;
    size_t idle_frames_size;
    size_t guard_frames_size;
    GeOriginalAnimationRoot *walk;
    GeOriginalAnimationRoot *sprint;
    GeOriginalAnimationRoot *idle;
    GeOriginalPlayerGait *gait;
    GeOriginalPlayerGaitStatus status;
    float matrices[4][4][4] = {{{0}}};
    float delta[3];
    float first_z;
    Model bound_model;
    Model *native_model;
    uint32_t bound_rwdata[64];
    RenderPosView bound_matrices[4];
    int tick;
    Model rw_model = {0};
    ModelNode rw_node = {0};
    union ModelRoData rw_rodata = {0};
    u32 rw_words[16] = {0};
    const u16 rw_opcodes[] = {
        MODELNODE_OPCODE_HEADER,
        MODELNODE_OPCODE_LOD,
        MODELNODE_OPCODE_SWITCH,
        MODELNODE_OPCODE_BSP,
        MODELNODE_OPCODE_DLCOLLISION,
    };
    size_t opcode_index;
    ModelNode bsp_node = {0};
    ModelNode bsp_left = {0};
    ModelNode bsp_right = {0};
    union ModelRoData bsp_rodata = {0};

    assert(argc == 6);
    rw_model.datas = (union ModelRwData **)(void *)rw_words;
    rw_node.Data = &rw_rodata;
    rw_rodata.Header.RwDataIndex = 5;
    for (opcode_index = 0;
         opcode_index < sizeof(rw_opcodes) / sizeof(rw_opcodes[0]);
         opcode_index++) {
        memset(&rw_rodata, 0, sizeof(rw_rodata));
        rw_node.Opcode = rw_opcodes[opcode_index];
        switch (rw_opcodes[opcode_index]) {
        case MODELNODE_OPCODE_HEADER:
            rw_rodata.Header.RwDataIndex = 5;
            break;
        case MODELNODE_OPCODE_LOD:
            rw_rodata.LOD.RwDataIndex = 5;
            break;
        case MODELNODE_OPCODE_SWITCH:
            rw_rodata.Switch.RwDataIndex = 5;
            break;
        case MODELNODE_OPCODE_BSP:
            rw_rodata.BSP.RwDataIndex = 5;
            break;
        case MODELNODE_OPCODE_DLCOLLISION:
            rw_rodata.DisplayListCollisions.RwDataIndex = 5;
            break;
        }
        assert(modelGetNodeRwData(&rw_model, &rw_node) ==
               (union ModelRwData *)(void *)&rw_words[5]);
    }
    bsp_node.Opcode = MODELNODE_OPCODE_BSP;
    bsp_node.Data = &bsp_rodata;
    bsp_rodata.BSP.RwDataIndex = 5;
    bsp_rodata.BSP.leftChild = &bsp_left;
    bsp_rodata.BSP.rightChild = &bsp_right;
    bsp_left.Next = &bsp_right;
    bsp_right.Next = &bsp_left;
    ((struct ModelRwData_BSPRecord *)(void *)&rw_words[5])->visible = TRUE;
    modelApplyReorderRelations(&rw_model, &bsp_node);
    assert(bsp_node.Child == &bsp_left);
    assert(bsp_left.Prev == NULL && bsp_left.Next == &bsp_right);
    assert(bsp_right.Prev == &bsp_left && bsp_right.Next == NULL);
    bsp_left.Next = &bsp_right;
    bsp_right.Next = &bsp_left;
    modelApplyReorderRelationsByArg(&bsp_node, FALSE);
    assert(bsp_node.Child == &bsp_right);
    assert(bsp_right.Prev == NULL && bsp_right.Next == &bsp_left);
    assert(bsp_left.Prev == &bsp_right && bsp_left.Next == NULL);
    {
        Model switch_model = {0};
        ModelFileHeader switch_header = {0};
        ModelNode switch_node = {0};
        ModelNode controlled_bbox = {0};
        union ModelRoData switch_rodata = {0};
        u32 switch_rw_words[4] = {0};
        RenderPosView switch_matrices[1] = {{{{0}}}};
        Mtxf switch_base = {{{0}}};
        ModelRenderData render_data = {0};

        switch_node.Opcode = MODELNODE_OPCODE_SWITCH;
        switch_node.Data = &switch_rodata;
        switch_rodata.Switch.RwDataIndex = 1;
        switch_rodata.Switch.Controls = &controlled_bbox;
        controlled_bbox.Opcode = MODELNODE_OPCODE_BBOX;
        controlled_bbox.Parent = &switch_node;
        switch_header.RootNode = &switch_node;
        switch_model.obj = &switch_header;
        switch_model.datas =
            (union ModelRwData **)(void *)switch_rw_words;
        ((struct ModelRwData_SwitchRecord *)(void *)&switch_rw_words[1])
            ->visible = TRUE;
        render_data.basemtx = &switch_base;
        render_data.mtxlist = &switch_matrices[0].pos;
        ge_port_player_gait_instcalcmatrices(&render_data, &switch_model);
        assert(switch_node.Child == &controlled_bbox);
    }
    data = read_file(argv[1], &size);
    walk_frames = read_file(argv[2], &walk_frames_size);
    sprint_frames = read_file(argv[3], &sprint_frames_size);
    idle_frames = read_file(argv[4], &idle_frames_size);
    guard_frames = read_file(argv[5], &guard_frames_size);
    assert(ge_original_guard_animation_table_bind(data, size));
    assert(ge_original_guard_animation_entries_bind(
        guard_frames, guard_frames_size));
    walk = ge_original_animation_root_create(
        data, size, GE_ORIGINAL_BOND_ANIMATION_EYE_WALK);
    sprint = ge_original_animation_root_create(
        data, size, GE_ORIGINAL_BOND_ANIMATION_SPRINTING);
    idle = ge_original_animation_root_create(
        data, size, GE_ORIGINAL_BOND_ANIMATION_IDLE);
    assert(walk != NULL && sprint != NULL && idle != NULL);
    assert(ge_original_animation_root_bind_frames(
        walk, walk_frames, walk_frames_size));
    assert(ge_original_animation_root_bind_frames(
        sprint, sprint_frames, sprint_frames_size));
    assert(ge_original_animation_root_bind_frames(
        idle, idle_frames, idle_frames_size));
    ge_original_player_gait_bind_bond_animations(walk, sprint);
    assert(fabsf(g_BondMoveAnimationSetup[0].speedMultiplier -
                 5.314286f) < 0.00001f);
    assert(fabsf(g_BondMoveAnimationSetup[1].speedMultiplier -
                 18.842106f) < 0.00001f);

    gait = ge_original_player_gait_create(walk, &status);
    assert(gait != NULL && status == GE_ORIGINAL_PLAYER_GAIT_OK);
    assert(ge_original_player_gait_rw_words(gait) > 0);
    assert(ge_original_player_gait_native_model(gait) != NULL);
    ge_original_player_gait_set_loop(gait, 9.5f, 27.0f, 0.0f);

    assert(ge_original_player_gait_tick_root(gait, 1, matrices, delta));
    assert(matrix_is_finite(matrices[0]));
    assert(fabsf(matrices[0][3][0] - 0.060632f) < 0.0001f);
    assert(fabsf(matrices[0][3][1] - 155.816681f) < 0.001f);
    assert(fabsf(matrices[0][3][2] - 4.080441f) < 0.001f);
    first_z = matrices[0][3][2];
    for (tick = 0; tick < 24; ++tick) {
        assert(ge_original_player_gait_tick_root(gait, 1, matrices, delta));
        assert(matrix_is_finite(matrices[1]));
        assert(isfinite(delta[0]) && isfinite(delta[1]) && isfinite(delta[2]));
    }
    assert(fabsf(matrices[0][3][2] - first_z) > 0.01f);

    assert(ge_original_player_gait_set_animation(
        gait, sprint, 0, 7.5f, 0.5f, 0.0f));
    ge_original_player_gait_set_loop(gait, 7.5f, 17.0f, 0.0f);
    assert(ge_original_player_gait_tick_root(gait, 1, matrices, delta));
    assert(matrix_is_finite(matrices[0]));
    assert(fabsf(matrices[0][3][0] - -6.321112f) < 0.001f);
    assert(fabsf(matrices[0][3][1] - 142.119904f) < 0.001f);
    assert(fabsf(matrices[0][3][2] - 101.212090f) < 0.001f);

    ge_original_player_gait_destroy(gait);

    {
        Model guard_model = {0};
        ModelFileHeader guard_header = {0};
        ModelNode guard_root = {0};
        union ModelRoData guard_rodata = {0};
        ModelAnimation *death_animation = ge_port_guard_animation_resolve(
            PTR_ANIM_death_head);
        const uint8_t *frame_zero;
        const uint8_t *frame_two;
        int32_t frame_zero_handle;
        int32_t frame_two_handle;
        size_t frame_size;

        assert(death_animation != NULL);
        assert(death_animation->address == PTR_ANIM_ENTRY_death_head);
        assert(death_animation->unk04 > 2U);
        frame_size = (size_t)(death_animation->unk0E >> 3);
        assert(frame_size > 0U);
        guard_root.Opcode = MODELNODE_OPCODE_GROUPSIMPLE;
        guard_root.Data = &guard_rodata;
        guard_header.RootNode = &guard_root;
        guard_model.obj = &guard_header;
        guard_model.playspeed = 1.0f;
        modelSetAnimation(&guard_model, death_animation, 0, 0.0f,
                          0.5f, 0.0f);
        assert(guard_model.framea == 0);
        frame_zero_handle = ge_port_player_gait_load_animation_frame(
            death_animation, guard_model.framea, NULL);
        assert(frame_zero_handle < 0);
        frame_zero = ge_port_guard_animation_frame_data(frame_zero_handle);
        assert(frame_zero != NULL);
        assert(memcmp(frame_zero,
                      guard_frames + PTR_ANIM_ENTRY_death_head,
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
                      guard_frames + PTR_ANIM_ENTRY_death_head
                          + 2U * frame_size,
                      frame_size) == 0);
        assert(memcmp(frame_zero, frame_two, frame_size) != 0);
    }

    gait = ge_original_player_gait_create(walk, &status);
    assert(gait != NULL && status == GE_ORIGINAL_PLAYER_GAIT_OK);
    ge_original_player_gait_set_loop(gait, 9.5f, 27.0f, 0.0f);
    assert(ge_original_player_gait_tick_root(gait, 1, matrices, delta));
    assert(ge_original_player_gait_set_animation(
        gait, sprint, 0, 7.5f, 0.5f, 12.0f));
    ge_original_player_gait_set_loop(gait, 7.5f, 17.0f, 0.0f);
    native_model = ge_original_player_gait_native_model(gait);
    assert(native_model->anim2 == ge_original_animation_root_native_abi(walk));
    assert(fabsf(native_model->unk84 - 1.0f) < 0.000001f);
    assert(ge_original_player_gait_tick_root(gait, 0, matrices, delta));
    assert(matrix_is_finite(matrices[0]));
    assert(fabsf(matrices[0][3][0] - 0.060632f) < 0.0001f);
    assert(fabsf(matrices[0][3][1] - 155.816696f) < 0.001f);
    assert(fabsf(matrices[0][3][2] - 4.080441f) < 0.001f);
    assert(ge_original_player_gait_tick_root(gait, 1, matrices, delta));
    assert(matrix_is_finite(matrices[0]));
    assert(fabsf(native_model->unk84 - 11.0f / 12.0f) < 0.000001f);
    assert(fabsf(matrices[0][3][0] - -0.247195f) < 0.001f);
    assert(fabsf(matrices[0][3][1] - 155.825623f) < 0.001f);
    assert(fabsf(matrices[0][3][2] - 15.375690f) < 0.001f);
    assert(ge_original_player_gait_tick_root(gait, 5, matrices, delta));
    assert(matrix_is_finite(matrices[0]));
    assert(native_model->anim2 == ge_original_animation_root_native_abi(walk));
    assert(fabsf(native_model->unk84 - 0.5f) < 0.000001f);
    assert(fabsf(matrices[0][3][0] - 0.032919f) < 0.001f);
    assert(fabsf(matrices[0][3][1] - 155.107025f) < 0.001f);
    assert(fabsf(matrices[0][3][2] - 55.877979f) < 0.001f);
    assert(ge_original_player_gait_tick_root(gait, 6, matrices, delta));
    assert(matrix_is_finite(matrices[0]));
    assert(fabsf(native_model->unk84) < 0.000001f);
    assert(fabsf(matrices[0][3][0] - 4.812627f) < 0.001f);
    assert(fabsf(matrices[0][3][1] - 149.076279f) < 0.001f);
    assert(fabsf(matrices[0][3][2] - 118.877991f) < 0.001f);
    assert(native_model->anim2 == NULL);
    ge_original_player_gait_destroy(gait);

    gait = ge_original_player_gait_create_bound(
        walk, &bound_model, bound_rwdata,
        sizeof(bound_rwdata) / sizeof(bound_rwdata[0]), bound_matrices, 4,
        &status);
    assert(gait != NULL && status == GE_ORIGINAL_PLAYER_GAIT_OK);
    assert(ge_original_player_gait_native_model(gait) == &bound_model);
    ge_original_player_gait_set_loop(gait, 9.5f, 27.0f, 0.0f);
    assert(ge_original_player_gait_tick_root(gait, 1, matrices, delta));
    assert(matrix_is_finite(bound_matrices[0].pos.m));
    ge_original_player_gait_destroy(gait);

    ge_original_spawn_player_reset(159.0f);
    assert(ge_original_spawn_player_get()->startnewbonddie == TRUE);
    assert(ge_original_spawn_player_get()->redbloodfinished == FALSE);
    assert(ge_original_spawn_player_get()->deathanimfinished == FALSE);
    gait = ge_original_player_gait_create_current_player(walk, &status);
    assert(gait != NULL && status == GE_ORIGINAL_PLAYER_GAIT_OK);
    assert(ge_original_player_gait_native_model(gait) ==
           (void *)&ge_original_spawn_player_get()->model);
    assert(ge_original_spawn_player_get()->standheight == 0.0f);
    assert(ge_original_player_gait_calibrate_current_player_standing(
        gait, idle, walk));
    assert(fabsf(ge_original_spawn_player_get()->standheight -
                 160.314392f) < 0.001f);
    assert(fabsf(ge_original_spawn_player_get()->standbodyoffset.y -
                 -52.014389f) < 0.001f);
    assert(fabsf(ge_original_spawn_player_get()->standbodyoffset.z -
                 3.668215f) < 0.001f);
    assert(((Model *)(void *)&ge_original_spawn_player_get()->model)->anim ==
           ge_original_animation_root_native_abi(walk));
    assert(((Model *)(void *)&ge_original_spawn_player_get()->model)
               ->animlooping == 1);
    assert(fabsf(((Model *)(void *)&ge_original_spawn_player_get()->model)
                     ->animloopframe - 9.5f) < 0.000001f);
    assert(fabsf(((Model *)(void *)&ge_original_spawn_player_get()->model)
                     ->endframe - 27.0f) < 0.000001f);
    {
        struct player *player = ge_original_spawn_player_get();
        coord3d idle_head = {0};
        const float authored_standheight = player->standheight;
        extern s32 g_ClockTimer;

        idle_head.y = authored_standheight;
        player->resetheadpos = TRUE;
        g_ClockTimer = 1;
        bheadUpdatePos(&idle_head);
        assert(fabsf(player->headpos.y - authored_standheight) < 0.0001f);
        g_ClockTimer = 6;
        bheadUpdatePos(&idle_head);
        assert(fabsf(player->headpos.y - authored_standheight) < 0.0001f);
        player->field_5C0 =
            ((Model *)(void *)&player->model)->animframe1;
        bheadAdjustAnimation(0.0f);
        assert(modelGetAbsAnimSpeed((Model *)(void *)&player->model) == 0.0f);
        assert(player->standheight == authored_standheight);
        modelSetAnimSpeed((Model *)(void *)&player->model, 0.5f, 0.0f);
    }
    assert(ge_original_player_gait_tick_root(gait, 1, matrices, delta));
    assert(fabsf(ge_original_spawn_player_get()
                     ->bondheadmatrices[0].m[3][2] - 4.080441f) < 0.001f);
    native_model = (Model *)(void *)&ge_original_spawn_player_get()->model;
    ge_original_spawn_player_get()->headanim = 0;
    ge_original_spawn_player_get()->field_5C0 = native_model->animframe1;
    bheadAdjustAnimation(1.0f);
    assert(native_model->anim ==
           ge_original_animation_root_native_abi(sprint));
    assert(native_model->anim2 ==
           ge_original_animation_root_native_abi(walk));
    assert(fabsf(native_model->unk84 - 1.0f) < 0.000001f);
    assert(ge_original_player_gait_tick_root(gait, 1, matrices, delta));
    assert(matrix_is_finite(
        ge_original_spawn_player_get()->bondheadmatrices[0].m));
    assert(fabsf(matrices[0][3][0] - 0.351760f) < 0.001f);
    assert(fabsf(matrices[0][3][1] - 155.874237f) < 0.001f);
    assert(fabsf(matrices[0][3][2] - 12.822325f) < 0.001f);
    assert(fabsf(native_model->unk84 - 11.0f / 12.0f) < 0.000001f);
    assert(ge_original_player_gait_tick_root(gait, 11, matrices, delta));
    assert(native_model->anim2 == NULL);
    assert(fabsf(native_model->unk84) < 0.000001f);
    assert(matrix_is_finite(
        ge_original_spawn_player_get()->bondheadmatrices[0].m));
    ge_original_player_gait_destroy(gait);

    ge_original_animation_root_destroy(walk);
    ge_original_animation_root_destroy(sprint);
    ge_original_animation_root_destroy(idle);
    free(walk_frames);
    free(sprint_frames);
    free(idle_frames);
    ge_original_guard_animation_table_reset();
    free(guard_frames);
    free(data);
    return 0;
}
