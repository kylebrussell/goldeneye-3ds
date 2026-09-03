#include "ge_original_player_gait.h"
#include "ge_original_player_gait_internal.h"
#include "ge_original_bond_movement.h"
#include "ge_original_guard_animation_table.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "game/chrobjdata.h"
#include "game/bondhead.h"
#include "ge_original_player_spawn_internal.h"
typedef int PLAYERFLAG;
#include "game/bondview.h"

struct GeOriginalPlayerGait {
    Model owned_model;
    Model *model;
    u32 *rwdata;
    uint16_t rw_words;
    RenderPosView owned_matrices[4];
    RenderPosView *matrices;
    coord3d previous_root;
    int have_previous_root;
    int owns_rwdata;
    const GeOriginalAnimationRoot *animation;
    const u8 *frame_slots[4];
    size_t frame_slot_count;
    int matrix_build_ok;
};

coord3d D_80036244 = {0};
coord3d D_80036254 = {0};
coord3d D_80036094 = {0};
coord3d D_800360A0 = {0};
coord3d D_800360AC = {0};
coord3d D_800360B8 = {0};
u32 g_ModelAnimMergingEnabled = 1;
void (*g_ModelJointPositionedFunc)(s32 mtxindex, Mtxf *mtx) = NULL;
static GeOriginalPlayerGait *g_active_gait;
static GeOriginalPlayerGait *g_current_player_gait;
static int g_auto_selected_gait;
static const GeOriginalAnimationRoot *g_bond_walking;
static const GeOriginalAnimationRoot *g_bond_sprinting;
static const GeOriginalAnimationRoot *g_bond_idle;
static Model *g_bond_anim_flip_model;
static void (*g_bond_anim_flip_function)(void);
static ModelJoint g_player_gait_root_joint;
ModelSkeleton ge_port_player_gait_root_skeleton;
#if UINTPTR_MAX > UINT32_MAX
void ge_port_model_set_anim_flip_function(Model *model, void *callback)
{
    g_bond_anim_flip_model = model;
    g_bond_anim_flip_function = (void (*)(void))callback;
}

void ge_port_model_invoke_anim_flip_function(Model *model)
{
    if (model == g_bond_anim_flip_model && g_bond_anim_flip_function != NULL) {
        g_bond_anim_flip_function();
    }
}
#endif

static const GeOriginalAnimationRoot *root_for_native_animation(
    ModelAnimation *animation)
{
    if (animation == (ModelAnimation *)
            ge_original_animation_root_native_abi(g_bond_idle)) {
        return g_bond_idle;
    }
    if (animation == (ModelAnimation *)
            ge_original_animation_root_native_abi(g_bond_walking)) {
        return g_bond_walking;
    }
    if (animation == (ModelAnimation *)
            ge_original_animation_root_native_abi(g_bond_sprinting)) {
        return g_bond_sprinting;
    }
    return NULL;
}

void ge_original_player_gait_bind_bond_animations(
    const GeOriginalAnimationRoot *walking,
    const GeOriginalAnimationRoot *sprinting)
{
    static const s32 loop_frames[2] = {9, 7};
    static const s32 end_frames[2] = {27, 17};
    const GeOriginalAnimationRoot *roots[2];
    s32 totals[3];
    s32 i;
    g_bond_walking = walking;
    g_bond_sprinting = sprinting;
    roots[0] = walking;
    roots[1] = sprinting;
    memset(&g_player_gait_root_joint, 0, sizeof(g_player_gait_root_joint));
    g_player_gait_root_joint.NodeType = 0x401;
    g_player_gait_root_joint.mtxA = 0;
    g_player_gait_root_joint.mtxB = 0;
    memset(&ge_port_player_gait_root_skeleton, 0,
           sizeof(ge_port_player_gait_root_skeleton));
    ge_port_player_gait_root_skeleton.numjoints = 1;
    ge_port_player_gait_root_skeleton.Joints = &g_player_gait_root_joint;
    ge_port_player_gait_root_skeleton.SkeletonSize = 3;
    for (i = 0; i < 2; i++) {
        ModelAnimation *animation = (ModelAnimation *)
            ge_original_animation_root_native_abi(roots[i]);
        g_BondMoveAnimationSetup[i].speedMultiplier = 0.0f;
        if (animation != NULL) {
            sub_GAME_7F0062C0(animation, loop_frames[i], end_frames[i],
                              totals);
            g_BondMoveAnimationSetup[i].speedMultiplier =
                ((f32)totals[2] * 0.1f) /
                (g_BondMoveAnimationSetup[i].endframe -
                 g_BondMoveAnimationSetup[i].loopframe);
        }
    }
}

int ge_original_player_gait_calibrate_current_player_standing(
    GeOriginalPlayerGait *gait,
    const GeOriginalAnimationRoot *idle,
    const GeOriginalAnimationRoot *walking)
{
    struct player *player = ge_original_spawn_player_get();
    ModelAnimation *idle_animation;
    ModelAnimation *walking_animation;
    float matrices[4][4][4];
    float root_delta[3];
    int calibrated = 0;

    if (gait == NULL || player == NULL ||
        gait->model != (Model *)(void *)&player->model ||
        idle == NULL || walking == NULL ||
        ge_original_animation_root_frame_data(idle, 0, NULL) == NULL ||
        ge_original_animation_root_frame_data(walking, 0, NULL) == NULL) {
        return 0;
    }
    idle_animation = (ModelAnimation *)
        ge_original_animation_root_native_abi(idle);
    walking_animation = (ModelAnimation *)
        ge_original_animation_root_native_abi(walking);
    if (idle_animation == NULL || walking_animation == NULL) {
        return 0;
    }

    /* Unchanged ordering from initBondDATAdefaults.c:172-198. */
    g_bond_idle = idle;
    g_bond_walking = walking;
    gait->animation = idle;
    /* create_with_storage installs walking so standalone gait users can tick
     * immediately. The live current-player path calls this calibration next;
     * restore the exact post-model-allocation state which canonical
     * initBondDATAdefaults has immediately before installing idle. */
    animInit(gait->model, &player_gait_object_header, gait->rwdata);
    modelSetScale(gait->model, 0.1f);
    gait->model->render_pos = gait->matrices;
    modelSetAnimation(gait->model, idle_animation, 0, 0.0f, 0.5f, 0.0f);
    if (ge_original_player_gait_tick_root(
            gait, 0, matrices, root_delta) &&
        isfinite(matrices[0][3][1]) &&
        isfinite(matrices[0][3][2]) &&
        isfinite(matrices[1][3][1]) &&
        isfinite(matrices[1][3][2])) {
        player->standheight = matrices[0][3][1];
        player->standbodyoffset.x = 0.0f;
        player->standbodyoffset.y =
            matrices[1][3][1] - matrices[0][3][1];
        player->standbodyoffset.z =
            matrices[1][3][2] - matrices[0][3][2];
        calibrated = 1;
    }

    gait->animation = walking;
    gait->have_previous_root = 0;
    modelSetAnimation(gait->model, walking_animation, 0, 9.5f, 0.5f, 0.0f);
    modelSetAnimLooping(gait->model, 9.5f, 0.0f);
    modelSetAnimEndFrame(gait->model, 27.0f);
    modelSetAnimFlipFunction(gait->model, bheadFlipAnimation);
    return calibrated;
}

ModelAnimation *ge_port_bond_animation_lookup(s32 band)
{
    const GeOriginalAnimationRoot *root = band == 0 ? g_bond_walking
        : (band == 1 ? g_bond_sprinting : NULL);
    return (ModelAnimation *)ge_original_animation_root_native_abi(root);
}

static const u8 *resolve_frame_handle(s32 handle)
{
    size_t index;
    if (g_active_gait == NULL || handle <= 0) {
        return NULL;
    }
    index = (size_t)(handle - 1);
    return index < g_active_gait->frame_slot_count
        ? g_active_gait->frame_slots[index] : NULL;
}

s32 ge_port_player_gait_load_animation_frame(ModelAnimation *animation,
                                              s32 frame,
                                              ModelSkeleton *skeleton)
{
    const u8 *bytes;
    const GeOriginalAnimationRoot *root;
    (void)skeleton;
    root = root_for_native_animation(animation);
    if (g_active_gait == NULL && g_current_player_gait != NULL &&
            root != NULL) {
        g_active_gait = g_current_player_gait;
        g_active_gait->animation = root;
        g_active_gait->frame_slot_count = 0;
        g_active_gait->matrix_build_ok = 0;
        g_auto_selected_gait = 1;
    }
    if (root == NULL && g_active_gait != NULL &&
            animation == (ModelAnimation *)
                ge_original_animation_root_native_abi(
                    g_active_gait->animation)) {
        root = g_active_gait->animation;
    }
    if (g_active_gait == NULL || root == NULL || frame < 0 ||
            g_active_gait->frame_slot_count >= 4u) {
        return ge_port_guard_animation_load_frame(animation, frame);
    }
    bytes = ge_original_animation_root_frame_data(
        root, (uint16_t)frame, NULL);
    if (bytes == NULL) {
        return 0;
    }
    g_active_gait->frame_slots[g_active_gait->frame_slot_count] = bytes;
    g_active_gait->frame_slot_count++;
    return (s32)g_active_gait->frame_slot_count;
}

void ge_port_player_gait_reset_animation_frames(void)
{
    /* The original resets the scratch allocator after copying frames; the
     * copied bytes remain valid through instcalcmatrices. Our handles do too. */
}

void ge_port_player_gait_decode_joint_handle(
    s32 jointnum, s32 flip, ModelSkeleton *skeleton,
    ModelAnimation *animation, s32 frame_handle, coord3d *rotation)
{
    const u8 *frame = resolve_frame_handle(frame_handle);
    if (frame == NULL)
        frame = ge_port_guard_animation_frame_data(frame_handle);
    if (frame == NULL || rotation == NULL) {
        if (g_active_gait != NULL) {
            g_active_gait->matrix_build_ok = 0;
        }
        return;
    }
    sub_GAME_7F06DEC0(jointnum, flip, skeleton, animation,
                      (u8 *)(uintptr_t)frame, rotation);
}

void ge_port_player_gait_instcalcmatrices(ModelRenderData *render_data,
                                          Model *model)
{
    ModelNode *node;
    const u8 *frame_a;
    const u8 *frame_b;
    if (model == NULL || model->obj == NULL || render_data == NULL ||
        render_data->mtxlist == NULL) {
        if (g_auto_selected_gait) {
            g_active_gait = NULL;
            g_auto_selected_gait = 0;
        }
        return;
    }
    if (g_active_gait == NULL || model != g_active_gait->model) {
        model->render_pos = (RenderPosView *)render_data->mtxlist;
        render_data->mtxlist += model->obj->numMatrices;
        node = model->obj->RootNode;
        while (node != NULL) {
            switch (node->Opcode & 0xffu) {
            case MODELNODE_OPCODE_HEADER:
                process_01_group_heading(render_data, model, node);
                break;
            case MODELNODE_OPCODE_GROUP:
                process_02_position(render_data, model, node);
                break;
            case MODELNODE_OPCODE_GROUPSIMPLE:
                ge_port_player_gait_process_15_subposition(
                    render_data, model, node);
                break;
            case MODELNODE_OPCODE_LOD:
                modelUpdateDistanceRelations(model, node);
                break;
            case MODELNODE_OPCODE_BSP:
                modelUpdateReorderRelations(model, node);
                break;
            case MODELNODE_OPCODE_SWITCH:
                modelApplyToggleRelations(model, node);
                break;
            case MODELNODE_OPCODE_HEAD:
                modelApplyHeadRelations(model, node);
                break;
            case MODELNODE_OPCODE_BBOX:
            case MODELNODE_OPCODE_SHADOW:
            case MODELNODE_OPCODE_DL:
            case MODELNODE_OPCODE_GUNFIRE:
            case MODELNODE_OPCODE_DLCOLLISION:
                break;
            default:
                abort();
            }
            if (node->Child != NULL) {
                node = node->Child;
            } else {
                while (node != NULL) {
                    if (node->Next != NULL) {
                        node = node->Next;
                        break;
                    }
                    node = node->Parent;
                }
            }
        }
        if (g_auto_selected_gait) {
            g_active_gait = NULL;
            g_auto_selected_gait = 0;
        }
        return;
    }
    frame_a = resolve_frame_handle(model->unk34);
    frame_b = model->unk2c != 0.0f
        ? resolve_frame_handle(model->unk38) : frame_a;
    if (frame_a == NULL || frame_b == NULL ||
        (model->unk84 != 0.0f && model->anim2 == NULL) ||
        (model->anim2 != NULL &&
         (resolve_frame_handle(model->unk64) == NULL ||
          (model->unk5c != 0.0f &&
           resolve_frame_handle(model->unk68) == NULL)))) {
        if (g_auto_selected_gait) {
            g_active_gait = NULL;
            g_auto_selected_gait = 0;
        }
        return;
    }

    model->render_pos = (RenderPosView *)render_data->mtxlist;
    render_data->mtxlist += model->obj->numMatrices;
    g_active_gait->matrix_build_ok = 1;
    process_01_group_heading(render_data, model, model->obj->RootNode);
    for (node = model->obj->RootNode->Child;
         node != NULL; node = node->Child) {
        if ((node->Opcode & 0xffu) != MODELNODE_OPCODE_GROUP) {
            g_active_gait->matrix_build_ok = 0;
            if (g_auto_selected_gait) {
                g_active_gait = NULL;
                g_auto_selected_gait = 0;
            }
            return;
        }
        process_02_position(render_data, model, node);
        if (!g_active_gait->matrix_build_ok) {
            if (g_auto_selected_gait) {
                g_active_gait = NULL;
                g_auto_selected_gait = 0;
            }
            return;
        }
    }
    if (g_auto_selected_gait) {
        g_active_gait = NULL;
        g_auto_selected_gait = 0;
    }
}

static int validate_embedded_graph(void)
{
    ModelNode *nodes[4];
    ModelNode *node;
    size_t count = 0;

    player_gait_object_header.RootNode = &player_gait_hdr;
    node = player_gait_object_header.RootNode;
    while (node != NULL && count < 4) {
        nodes[count++] = node;
        node = node->Child;
    }
    return count == 4 && node == NULL &&
        (nodes[0]->Opcode & 0xffu) == MODELNODE_OPCODE_HEADER &&
        (nodes[1]->Opcode & 0xffu) == MODELNODE_OPCODE_GROUP &&
        (nodes[2]->Opcode & 0xffu) == MODELNODE_OPCODE_GROUP &&
        (nodes[3]->Opcode & 0xffu) == MODELNODE_OPCODE_GROUP &&
        player_gait_object_header.Skeleton == &skeleton_player_gait_object &&
        player_gait_object_header.Skeleton->numjoints == 16 &&
        player_gait_object_header.Skeleton->SkeletonSize == 45 &&
        player_gait_object_header.numMatrices == 4;
}

union ModelRwData *modelGetNodeRwData(Model *model, ModelNode *node)
{
    s32 index = 0;
#if defined(GE_PORT_MODEL_HOST_RWDATA_ABI)
    union ModelRwData **words;
#else
    u32 *words;
#endif
    if (model == NULL || node == NULL || node->Data == NULL ||
        model->datas == NULL) {
        return NULL;
    }
#if defined(GE_PORT_MODEL_HOST_RWDATA_ABI)
    words = model->datas;
#else
    words = (u32 *)(void *)model->datas;
#endif
    switch (node->Opcode & 0xffu) {
    case MODELNODE_OPCODE_HEADER:
        index = node->Data->Header.RwDataIndex;
        break;
    case MODELNODE_OPCODE_DLCOLLISION:
        index = node->Data->DisplayListCollisions.RwDataIndex;
        break;
    case MODELNODE_OPCODE_OP07:
        index = node->Data->Op07.RwDataIndex;
        break;
    case MODELNODE_OPCODE_LOD:
        index = node->Data->LOD.RwDataIndex;
        break;
    case MODELNODE_OPCODE_SWITCH:
        index = node->Data->Switch.RwDataIndex;
        break;
    case MODELNODE_OPCODE_BSP:
        index = node->Data->BSP.RwDataIndex;
        break;
    case MODELNODE_OPCODE_OP11:
        index = node->Data->Op11.RwDataIndex;
        break;
    case MODELNODE_OPCODE_GUNFIRE:
        index = node->Data->Gunfire.RwDataIndex;
        break;
    case MODELNODE_OPCODE_HEAD:
        index = node->Data->HeadPlaceholder.RwDataIndex;
        break;
    }
    while (node->Parent != NULL) {
        node = node->Parent;
        if ((node->Opcode & 0xffu) == MODELNODE_OPCODE_HEAD) {
            ModelRwData_HeadPlaceholderRecord *head =
                modelGetNodeRwData(model, node);
#if defined(GE_PORT_MODEL_HOST_RWDATA_ABI)
            words = (union ModelRwData **)(void *)head->RwDatas;
#else
            words = (u32 *)(void *)head->RwDatas;
#endif
            break;
        }
    }
    return (union ModelRwData *)(void *)&words[index];
}

static GeOriginalPlayerGait *create_with_storage(
    const GeOriginalAnimationRoot *animation,
    Model *native_model,
    u32 *native_rwdata,
    size_t native_rw_word_capacity,
    RenderPosView *native_matrices,
    size_t native_matrix_capacity,
    int owns_storage,
    GeOriginalPlayerGaitStatus *status)
{
    GeOriginalPlayerGait *gait;
    ModelAnimation *native_animation;
    if (status != NULL) {
        *status = GE_ORIGINAL_PLAYER_GAIT_INVALID_ARGUMENT;
    }
    native_animation = (ModelAnimation *)
        ge_original_animation_root_native_abi(animation);
    if (native_animation == NULL ||
        ge_original_animation_root_frame_data(animation, 0, NULL) == NULL) {
        return NULL;
    }
    if (!validate_embedded_graph()) {
        if (status != NULL) {
            *status = GE_ORIGINAL_PLAYER_GAIT_INVALID_EMBEDDED_MODEL;
        }
        return NULL;
    }

    modelCalculateRwDataLen(&player_gait_object_header);
    if (player_gait_object_header.numRecords <= 0) {
        if (status != NULL) {
            *status = GE_ORIGINAL_PLAYER_GAIT_INVALID_EMBEDDED_MODEL;
        }
        return NULL;
    }
    if (!owns_storage && (native_model == NULL || native_rwdata == NULL ||
                          native_matrices == NULL ||
                          native_rw_word_capacity <
                              (size_t)player_gait_object_header.numRecords ||
                          native_matrix_capacity < 4u)) {
        return NULL;
    }
    gait = calloc(1, sizeof(*gait));
    if (gait == NULL) {
        if (status != NULL) {
            *status = GE_ORIGINAL_PLAYER_GAIT_ALLOCATION_FAILED;
        }
        return NULL;
    }
    gait->rw_words = (uint16_t)player_gait_object_header.numRecords;
    gait->animation = animation;
    gait->owns_rwdata = owns_storage;
    if (owns_storage) {
        gait->model = &gait->owned_model;
        gait->matrices = gait->owned_matrices;
        gait->rwdata = calloc(gait->rw_words, sizeof(*gait->rwdata));
        if (gait->rwdata == NULL) {
            free(gait);
            if (status != NULL) {
                *status = GE_ORIGINAL_PLAYER_GAIT_ALLOCATION_FAILED;
            }
            return NULL;
        }
    } else {
        gait->model = native_model;
        gait->matrices = native_matrices;
        gait->rwdata = native_rwdata;
        memset(gait->rwdata, 0, gait->rw_words * sizeof(*gait->rwdata));
        memset(gait->matrices, 0, 4u * sizeof(*gait->matrices));
    }

    animInit(gait->model, &player_gait_object_header, gait->rwdata);
    modelSetScale(gait->model, 0.1f);
    gait->model->render_pos = gait->matrices;
    modelSetAnimation(gait->model, native_animation, 0, 9.5f, 0.5f, 0.0f);
    if (status != NULL) {
        *status = GE_ORIGINAL_PLAYER_GAIT_OK;
    }
    return gait;
}

GeOriginalPlayerGait *ge_original_player_gait_create(
    const GeOriginalAnimationRoot *animation,
    GeOriginalPlayerGaitStatus *status)
{
    return create_with_storage(animation, NULL, NULL, 0, NULL, 0, 1,
                               status);
}

GeOriginalPlayerGait *ge_original_player_gait_create_bound(
    const GeOriginalAnimationRoot *animation,
    void *native_model,
    uint32_t *native_rwdata,
    size_t native_rw_word_capacity,
    void *native_matrices,
    size_t native_matrix_capacity,
    GeOriginalPlayerGaitStatus *status)
{
    return create_with_storage(animation, (Model *)native_model,
                               (u32 *)native_rwdata,
                               native_rw_word_capacity,
                               (RenderPosView *)native_matrices,
                               native_matrix_capacity, 0, status);
}

GeOriginalPlayerGait *ge_original_player_gait_create_current_player(
    const GeOriginalAnimationRoot *animation,
    GeOriginalPlayerGaitStatus *status)
{
    struct player *player = ge_original_spawn_player_get();
    GeOriginalPlayerGait *gait;
    size_t rw_capacity;
    if (player == NULL) {
        if (status != NULL) {
            *status = GE_ORIGINAL_PLAYER_GAIT_INVALID_ARGUMENT;
        }
        return NULL;
    }
    rw_capacity = ((uintptr_t)&player->field_6CC -
                   (uintptr_t)&player->field_654) / sizeof(u32);
#if UINTPTR_MAX > UINT32_MAX
    /* The original player stores Model followed immediately by its rwdata.
     * Native pointers enlarge Model on a 64-bit sanitizer host, so retain the
     * same current-player Model but keep rwdata in typed owned storage. */
    {
        u32 *host_rwdata = calloc(rw_capacity, sizeof(*host_rwdata));
        if (host_rwdata == NULL) {
            if (status != NULL) {
                *status = GE_ORIGINAL_PLAYER_GAIT_ALLOCATION_FAILED;
            }
            return NULL;
        }
        gait = ge_original_player_gait_create_bound(
            animation, &player->model, host_rwdata, rw_capacity,
            &player->bondheadmatrices[0],
            sizeof(player->bondheadmatrices) /
                sizeof(player->bondheadmatrices[0]), status);
        if (gait == NULL) {
            free(host_rwdata);
            return NULL;
        }
        gait->owns_rwdata = 1;
        g_current_player_gait = gait;
        return gait;
    }
#else
    gait = ge_original_player_gait_create_bound(
        animation, &player->model, (uint32_t *)(void *)&player->field_654,
        rw_capacity, &player->bondheadmatrices[0],
        sizeof(player->bondheadmatrices) /
            sizeof(player->bondheadmatrices[0]), status);
    g_current_player_gait = gait;
    return gait;
#endif
}

void ge_original_player_gait_destroy(GeOriginalPlayerGait *gait)
{
    if (gait != NULL) {
        if (g_current_player_gait == gait) {
            g_current_player_gait = NULL;
        }
        if (g_active_gait == gait) {
            g_active_gait = NULL;
            g_auto_selected_gait = 0;
        }
        if (gait->owns_rwdata) {
            free(gait->rwdata);
        }
        free(gait);
    }
}

int ge_original_player_gait_set_animation(
    GeOriginalPlayerGait *gait,
    const GeOriginalAnimationRoot *animation,
    int flip,
    float start_frame,
    float speed,
    float merge)
{
    ModelAnimation *native_animation;
    if (gait == NULL) {
        return 0;
    }
    native_animation = (ModelAnimation *)
        ge_original_animation_root_native_abi(animation);
    if (native_animation == NULL) {
        return 0;
    }
    modelSetAnimation(gait->model, native_animation, flip != 0,
                      start_frame, speed, merge);
    gait->animation = animation;
    gait->have_previous_root = 0;
    return 1;
}

void ge_original_player_gait_set_loop(GeOriginalPlayerGait *gait,
                                      float loop_frame,
                                      float end_frame,
                                      float loop_merge)
{
    if (gait == NULL) {
        return;
    }
    modelSetAnimLooping(gait->model, loop_frame, loop_merge);
    modelSetAnimEndFrame(gait->model, end_frame);
}

int ge_original_player_gait_tick_root(GeOriginalPlayerGait *gait,
                                      int32_t ticks,
                                      float matrices[4][4][4],
                                      float root_delta[3])
{
    ModelRenderData render_data;
    Mtxf base;
    Mtxf *root;
    coord3d current;
    if (gait == NULL || ticks < 0 || matrices == NULL || root_delta == NULL ||
        gait->model->anim == NULL) {
        return 0;
    }

    modelTickAnim(gait->model, ticks, 1);
    subcalcpos(gait->model);
    matrix_4x4_set_identity(&base);
    memset(&render_data, 0, sizeof(render_data));
    render_data.basemtx = &base;
    render_data.mtxlist = (Mtxf *)(void *)gait->matrices;
    gait->frame_slot_count = 0;
    gait->matrix_build_ok = 0;
    g_active_gait = gait;
    subcalcmatrices(&render_data, gait->model);
    g_active_gait = NULL;
    if (!gait->matrix_build_ok) {
        return 0;
    }

    root = &gait->matrices[0].pos;
    current.x = root->m[3][0];
    current.y = root->m[3][1];
    current.z = root->m[3][2];
    if (gait->have_previous_root) {
        root_delta[0] = current.x - gait->previous_root.x;
        root_delta[1] = current.y - gait->previous_root.y;
        root_delta[2] = current.z - gait->previous_root.z;
    } else {
        root_delta[0] = current.x;
        root_delta[1] = current.y;
        root_delta[2] = current.z;
        gait->have_previous_root = 1;
    }
    gait->previous_root = current;
    memcpy(matrices, gait->matrices, 4u * sizeof(*gait->matrices));
    return isfinite(current.x) && isfinite(current.y) && isfinite(current.z) &&
        isfinite(root_delta[0]) && isfinite(root_delta[1]) &&
        isfinite(root_delta[2]);
}

int ge_original_player_gait_current_player_movement_tick(
    GeOriginalPlayerGait *gait,
    int32_t clock_timer,
    float global_timer_delta,
    GeOriginalPlayerGaitTick *tick)
{
    struct player *player = ge_original_spawn_player_get();
    float matrices[4][4][4];
    float root_delta[3];
    coord3d offset = {0};
    float speed_forwards;
    float speed_sideways;
    float speed_theta;
    float abs_anim_speed;
    float amplitude;
    u32 merging_enabled;
    if (gait == NULL || tick == NULL || player == NULL ||
        gait->model != (Model *)(void *)&player->model || clock_timer < 0 ||
        !isfinite(global_timer_delta) || global_timer_delta < 0.0f ||
        (clock_timer > 0 && global_timer_delta == 0.0f) ||
        !isfinite(player->speedforwards) ||
        !isfinite(player->speedsideways) ||
        !isfinite(player->speedtheta) || player->bonddead != 0) {
        return 0;
    }

    memset(tick, 0, sizeof(*tick));
    speed_forwards = fabsf(player->speedforwards);
    speed_sideways = fabsf(player->speedsideways * 0.8f);
    speed_theta = fabsf(player->speedtheta * 0.8f);
    tick->max_speed = speed_forwards;
    if (tick->max_speed < speed_sideways) {
        tick->max_speed = speed_sideways;
    }
    if (tick->max_speed < speed_theta) {
        tick->max_speed = speed_theta;
    }
    if (tick->max_speed != 0.0f) {
        tick->percent_speed = player->speedforwards / tick->max_speed;
    }
    tick->sideways_motion = player->speedsideways *
        g_BondMoveAnimationSetup[1].speedMultiplier * 0.5f *
        global_timer_delta;

    /* field_5C0 aliases Model::animframe1 in the original 32-bit player
     * overlay. Keep the named host field synchronized before exact bhead. */
    player->field_5C0 = gait->model->animframe1;
    bheadAdjustAnimation(tick->max_speed);

    abs_anim_speed = fabsf(gait->model->speed);
    if (player->headanim == 0) {
        if (abs_anim_speed > 0.7f) {
            amplitude = 1.0f;
        } else if (abs_anim_speed > 0.1f) {
            amplitude = (((abs_anim_speed - 0.1f) * 0.6f) /
                         0.59999996f) + 0.4f;
        } else {
            amplitude = 0.4f;
        }
        player->sideamplitude = amplitude;
    } else if (player->headanim == 1) {
        amplitude = 0.9f;
        player->sideamplitude = 0.5f;
    } else {
        amplitude = 1.0f;
        player->sideamplitude = amplitude;
    }
    player->headamplitude = amplitude;
    player->resetheadtick = FALSE;

    merging_enabled = g_ModelAnimMergingEnabled;
    g_ModelAnimMergingEnabled = 0;
    if (!ge_original_player_gait_tick_root(
            gait, clock_timer, matrices, root_delta)) {
        g_ModelAnimMergingEnabled = merging_enabled;
        return 0;
    }
    g_ModelAnimMergingEnabled = merging_enabled;
    player->field_5C0 = gait->model->animframe1;

    player->headbodyoffset.f[0] = player->standbodyoffset.x;
    player->headbodyoffset.f[1] = player->standbodyoffset.y;
    player->headbodyoffset.f[2] = player->standbodyoffset.z;
    getsuboffset(gait->model, (struct float3 *)(void *)&offset);
    offset.f[0] -= matrices[0][3][0];
    offset.f[2] -= matrices[0][3][2];
    setsuboffset(gait->model, &offset);

    if (abs_anim_speed > 0.0f) {
        matrices[0][3][0] += tick->sideways_motion;
        matrices[0][3][2] *= tick->percent_speed;
        if (clock_timer > 0) {
            matrices[0][3][0] /= global_timer_delta;
            matrices[0][3][2] /= global_timer_delta;
        }
        tick->root_velocity[0] = matrices[0][3][0] * amplitude;
        tick->root_velocity[1] =
            ((matrices[0][3][1] - player->standheight) * amplitude) +
            player->standheight;
        tick->root_velocity[2] = matrices[0][3][2] * amplitude;
    }
    gait->matrices[0].pos.m[3][0] = matrices[0][3][0];
    gait->matrices[0].pos.m[3][1] = matrices[0][3][1];
    gait->matrices[0].pos.m[3][2] = matrices[0][3][2];
    memcpy(tick->matrix0, matrices[0], sizeof(tick->matrix0));
    return ge_original_bond_root_motion_apply_current_player(
        clock_timer, global_timer_delta, tick->root_velocity);
}

uint16_t ge_original_player_gait_rw_words(
    const GeOriginalPlayerGait *gait)
{
    return gait != NULL ? gait->rw_words : 0;
}

void *ge_original_player_gait_native_model(GeOriginalPlayerGait *gait)
{
    return gait != NULL ? gait->model : NULL;
}

const char *ge_original_player_gait_status_name(
    GeOriginalPlayerGaitStatus status)
{
    switch (status) {
    case GE_ORIGINAL_PLAYER_GAIT_OK:
        return "ok";
    case GE_ORIGINAL_PLAYER_GAIT_INVALID_ARGUMENT:
        return "invalid argument";
    case GE_ORIGINAL_PLAYER_GAIT_INVALID_EMBEDDED_MODEL:
        return "invalid embedded model";
    case GE_ORIGINAL_PLAYER_GAIT_ALLOCATION_FAILED:
        return "allocation failed";
    default:
        return "unknown";
    }
}
