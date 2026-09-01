#ifndef GE_ORIGINAL_ANIMATION_ROOT_INTERNAL_H
#define GE_ORIGINAL_ANIMATION_ROOT_INTERNAL_H

#include <stddef.h>
#include <ultra64.h>

#ifndef M_TAU_F
#define M_TAU_F 6.2831855f
#endif
#ifndef M_U16_MAX_VALUE_F
#define M_U16_MAX_VALUE_F 65536.0f
#endif

typedef struct coord3d {
    f32 x;
    f32 y;
    f32 z;
} coord3d;

typedef struct coord16 {
    s16 x;
    s16 y;
    s16 z;
} coord16;

typedef struct ModelAnimBitField {
    u16 bitOffset;
    u8 bitCount;
    u8 padding;
    u16 valueOffset;
} ModelAnimBitField;

typedef struct ModelAnimation {
    s32 address;
    u16 unk04;
    u8 unk06;
    u8 unk07;
    ModelAnimBitField *bitDescriptors;
    u16 unk0C;
    u16 unk0E;
    u8 *bitStream;
    s32 unused[10];
} ModelAnimation;

typedef struct ModelJoint {
    u16 NodeType;
    u16 mtxA;
    u16 mtxB;
} ModelJoint;

typedef struct ModelSkeleton {
    s16 numjoints;
    s16 pad1;
    ModelJoint *Joints;
    s16 SkeletonSize;
    s16 pad2;
} ModelSkeleton;

_Static_assert(sizeof(ModelAnimBitField) == 6,
               "native root-motion descriptor ABI changed");
_Static_assert(offsetof(ModelAnimBitField, bitOffset) == 0,
               "unexpected bitOffset ABI");
_Static_assert(offsetof(ModelAnimBitField, bitCount) == 2,
               "unexpected bitCount ABI");
_Static_assert(offsetof(ModelAnimBitField, valueOffset) == 4,
               "unexpected valueOffset ABI");
_Static_assert(sizeof(ModelJoint) == 6, "native model joint ABI changed");
_Static_assert(offsetof(ModelAnimation, bitDescriptors) >= 8,
               "unexpected native animation pointer alignment");
_Static_assert(offsetof(ModelAnimation, unk0C) ==
                   offsetof(ModelAnimation, bitDescriptors) + sizeof(void *),
               "unexpected native animation descriptor ABI");
_Static_assert(offsetof(ModelAnimation, bitStream) >=
                   offsetof(ModelAnimation, unk0E) + sizeof(u16),
               "unexpected native animation bitstream ABI");

u16 modelAnimReadRootMotionValue(ModelAnimation *anim, s32 fieldIndex,
                                 s32 extraBitOffset);
u16 sub_GAME_7F06D2E4(s32 jointnum, s32 flip, ModelSkeleton *skeleton,
                      ModelAnimation *anim, s32 frame, coord16 *out);
f32 sub_GAME_7F06D3F4(s32 jointnum, s32 flip, ModelSkeleton *skeleton,
                      ModelAnimation *anim, s32 frame, coord3d *pos);

#endif
