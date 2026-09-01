#ifndef GE_ORIGINAL_MODEL_CLOCK_INTERNAL_H
#define GE_ORIGINAL_MODEL_CLOCK_INTERNAL_H

#include <ultra64.h>
#include <stdint.h>
#include <bondtypes.h>
#include "game/model.h"
#include "game/math_floor.h"
#include "game/math_ceil.h"

#ifndef M_TAU_F
#define M_TAU_F 6.2831855f
#endif
#ifndef M_U16_MAX_VALUE_F
#define M_U16_MAX_VALUE_F 65536.0f
#endif

f32 sub_GAME_7F06D0CC(f32 arg0, f32 angle, f32 mult);
f32 sub_GAME_7F06D3F4(s32 jointnum, s32 flip, ModelSkeleton *skeleton,
                      ModelAnimation *anim, s32 frame, coord3d *pos);
extern coord3d D_80036244;
extern coord3d D_80036254;
extern u32 g_ModelAnimMergingEnabled;

#if UINTPTR_MAX > UINT32_MAX
void ge_port_model_set_anim_flip_function(Model *model, void *callback);
void ge_port_model_invoke_anim_flip_function(Model *model);
#endif

#endif
