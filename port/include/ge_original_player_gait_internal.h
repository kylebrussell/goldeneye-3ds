#ifndef GE_ORIGINAL_PLAYER_GAIT_INTERNAL_H
#define GE_ORIGINAL_PLAYER_GAIT_INTERNAL_H

#include <stdlib.h>
#include <ultra64.h>
#include <bondtypes.h>
#include "game/matrixmath.h"
#include "game/model.h"
#include "game/quaternion.h"
#include "game/math_unk_05A9E0.h"

#ifndef M_TAU_F
#define M_TAU_F 6.2831855f
#endif
#ifndef M_U16_MAX_VALUE_F
#define M_U16_MAX_VALUE_F 65536.0f
#endif

extern coord3d D_80036244;
extern coord3d D_80036254;
extern u32 g_ModelAnimMergingEnabled;
extern ModelSkeleton skeleton_player_gait_object;
extern ModelSkeleton ge_port_player_gait_root_skeleton;
extern coord3d D_80036094;
extern coord3d D_800360A0;
extern coord3d D_800360AC;
extern coord3d D_800360B8;
extern void (*g_ModelJointPositionedFunc)(s32 mtxindex, Mtxf *mtx);

typedef struct ModelGroupMtxBuildArg {
    u16 flags;
    u16 pad;
    ModelRoData_GroupRecord *group;
    ModelNode *parentnode;
} ModelGroupMtxBuildArg;

void modelBuildGroupMatrices(Mtxf **parent_mtx, Model *model,
                             ModelGroupMtxBuildArg *args, coord3d *rotation);
void sub_GAME_7F06DEC0(s32 jointnum, s32 flip, ModelSkeleton *skeleton,
                       ModelAnimation *animation, u8 *bitstream,
                       coord3d *rotation);
void process_02_position(ModelRenderData *render_data, Model *model,
                         ModelNode *node);
void ge_port_player_gait_decode_joint_handle(
    s32 jointnum, s32 flip, ModelSkeleton *skeleton,
    ModelAnimation *animation, s32 frame_handle, coord3d *rotation);
void ge_port_player_gait_build_group_quaternion(
    ModelRenderData *render_data, Model *model, ModelNode *node,
    quatf rotation);
void sub_GAME_7F0062C0(void *animation, s32 start_frame, s32 end_frame,
                       s32 totals[3]);
s32 ge_port_player_gait_load_animation_frame(ModelAnimation *animation,
                                              s32 frame,
                                              ModelSkeleton *skeleton);
void ge_port_player_gait_reset_animation_frames(void);
void ge_port_player_gait_instcalcmatrices(ModelRenderData *render_data,
                                          Model *model);
void ge_port_player_gait_process_15_subposition(
    ModelRenderData *render_data, Model *model, ModelNode *node);
/* model.c's N64 ABI stores rwdata in 32-bit words even though Model::datas is
 * typed as a pointer array. The native adapter preserves word indexing and
 * the generated index pass aligns pointer-bearing records on 64-bit hosts. */
union ModelRwData *modelGetNodeRwData(Model *model, ModelNode *node);

void modelApplyReorderRelationsByArg(ModelNode *node, bool visible);
void modelApplyReorderRelations(Model *model, ModelNode *node);
void modelUpdateDistanceRelations(Model *model, ModelNode *node);
void modelApplyHeadRelations(Model *model, ModelNode *node);
void modelUpdateReorderRelations(Model *model, ModelNode *node);

#endif
