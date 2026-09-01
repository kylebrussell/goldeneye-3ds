#include "ge_original_animation_root.h"
#include "ge_original_animation_root_internal.h"

#include <stdlib.h>
#include <string.h>

#define GE_ANIMATION_DATA_SIZE 0xe7e0u
#define GE_ANIM_IDLE_RECORD 0x1cu
#define GE_ANIM_SPRINT_RECORD 0x4070u
#define GE_ANIM_WALK_RECORD 0x4144u
#define GE_SOURCE_RECORD_SIZE 0x14u
#define GE_ROOT_CHANNEL_COUNT 4u
#define GE_BOND_MODEL_SCALE 0.1f

struct GeOriginalAnimationRoot {
    ModelAnimation animation;
    ModelAnimBitField channels[GE_ROOT_CHANNEL_COUNT];
    ModelJoint root_joint;
    ModelSkeleton skeleton;
    const uint8_t *frame_bytes;
    size_t frame_bytes_size;
};

static uint16_t read_be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

GeOriginalAnimationRoot *ge_original_animation_root_create(
    const uint8_t *segment_bytes,
    size_t segment_size,
    GeOriginalBondAnimationId animation_id)
{
    size_t record_offset;
    uint32_t descriptor_offset;
    uint32_t stream_offset;
    GeOriginalAnimationRoot *root;
    size_t i;

    if (segment_bytes == NULL || segment_size != GE_ANIMATION_DATA_SIZE) {
        return NULL;
    }
    if (animation_id == GE_ORIGINAL_BOND_ANIMATION_IDLE) {
        record_offset = GE_ANIM_IDLE_RECORD;
    } else if (animation_id == GE_ORIGINAL_BOND_ANIMATION_SPRINTING) {
        record_offset = GE_ANIM_SPRINT_RECORD;
    } else if (animation_id == GE_ORIGINAL_BOND_ANIMATION_EYE_WALK) {
        record_offset = GE_ANIM_WALK_RECORD;
    } else {
        return NULL;
    }
    if (record_offset + GE_SOURCE_RECORD_SIZE > segment_size) {
        return NULL;
    }
    descriptor_offset = read_be32(segment_bytes + record_offset + 8u);
    stream_offset = read_be32(segment_bytes + record_offset + 16u);
    if ((size_t)descriptor_offset + GE_ROOT_CHANNEL_COUNT * 6u > segment_size ||
        stream_offset >= segment_size) {
        return NULL;
    }

    root = calloc(1, sizeof(*root));
    if (root == NULL) {
        return NULL;
    }
    root->animation.address = (s32)read_be32(segment_bytes + record_offset);
    root->animation.unk04 = read_be16(segment_bytes + record_offset + 4u);
    root->animation.unk06 = segment_bytes[record_offset + 6u];
    root->animation.unk07 = segment_bytes[record_offset + 7u];
    root->animation.unk0C = read_be16(segment_bytes + record_offset + 12u);
    root->animation.unk0E = read_be16(segment_bytes + record_offset + 14u);
    root->animation.bitDescriptors = root->channels;
    root->animation.bitStream = (u8 *)(uintptr_t)(segment_bytes + stream_offset);
    for (i = 0; i < GE_ROOT_CHANNEL_COUNT; i++) {
        const uint8_t *source = segment_bytes + descriptor_offset + i * 6u;
        root->channels[i].bitOffset = read_be16(source);
        root->channels[i].bitCount = source[2];
        root->channels[i].padding = source[3];
        root->channels[i].valueOffset = read_be16(source + 4u);
    }

    root->root_joint.NodeType = 0x401u;
    root->root_joint.mtxA = 0;
    root->root_joint.mtxB = 0;
    root->skeleton.numjoints = 1;
    root->skeleton.Joints = &root->root_joint;
    root->skeleton.SkeletonSize = 3;
    return root;
}

void ge_original_animation_root_destroy(GeOriginalAnimationRoot *root)
{
    free(root);
}

uint16_t ge_original_animation_root_frame_count(
    const GeOriginalAnimationRoot *root)
{
    return root != NULL ? root->animation.unk04 : 0;
}

void *ge_original_animation_root_native_abi(
    const GeOriginalAnimationRoot *root)
{
    return root != NULL ? (void *)&root->animation : NULL;
}

int ge_original_animation_root_bind_frames(GeOriginalAnimationRoot *root,
                                           const uint8_t *frame_bytes,
                                           size_t frame_size)
{
    size_t bytes_per_frame;
    if (root == NULL || frame_bytes == NULL) {
        return 0;
    }
    bytes_per_frame = (size_t)(root->animation.unk0E >> 3);
    if (bytes_per_frame == 0 ||
        frame_size != bytes_per_frame * root->animation.unk04) {
        return 0;
    }
    root->frame_bytes = frame_bytes;
    root->frame_bytes_size = frame_size;
    return 1;
}

const uint8_t *ge_original_animation_root_frame_data(
    const GeOriginalAnimationRoot *root,
    uint16_t frame,
    size_t *frame_size)
{
    size_t bytes_per_frame;
    if (root == NULL || root->frame_bytes == NULL ||
        frame >= root->animation.unk04) {
        return NULL;
    }
    bytes_per_frame = (size_t)(root->animation.unk0E >> 3);
    if (frame_size != NULL) {
        *frame_size = bytes_per_frame;
    }
    return root->frame_bytes + (size_t)frame * bytes_per_frame;
}

int ge_original_animation_root_decode(const GeOriginalAnimationRoot *root,
                                      uint16_t frame,
                                      int flip,
                                      float position[3],
                                      float *angle_radians)
{
    coord3d decoded;
    float angle;
    if (root == NULL || position == NULL || frame >= root->animation.unk04) {
        return 0;
    }
    angle = sub_GAME_7F06D3F4(0, flip != 0,
        (ModelSkeleton *)&root->skeleton,
        (ModelAnimation *)&root->animation, frame, &decoded);
    position[0] = decoded.x;
    position[1] = decoded.y;
    position[2] = decoded.z;
    if (angle_radians != NULL) {
        *angle_radians = angle;
    }
    return 1;
}

void ge_original_animation_root_sample_set(
    GeOriginalAnimationRootSample *sample,
    const GeOriginalAnimationRoot *root,
    uint16_t frame,
    int flip)
{
    if (sample == NULL) {
        return;
    }
    sample->root = root;
    sample->frame = frame;
    sample->flip = flip;
    sample->valid = root != NULL && frame < ge_original_animation_root_frame_count(root);
}

int ge_original_animation_root_sample_velocity(void *context,
                                               float speed_forwards,
                                               float speed_sideways,
                                               int32_t clock_timer,
                                               float global_timer_delta,
                                               float velocity[3])
{
    GeOriginalAnimationRootSample *sample = context;
    float decoded[3];
    (void)speed_forwards;
    (void)speed_sideways;
    (void)clock_timer;
    (void)global_timer_delta;
    if (sample == NULL || velocity == NULL || !sample->valid ||
        !ge_original_animation_root_decode(sample->root, sample->frame,
                                           sample->flip, decoded, NULL)) {
        return 0;
    }
    velocity[0] = decoded[0] * GE_BOND_MODEL_SCALE;
    velocity[1] = decoded[1] * GE_BOND_MODEL_SCALE;
    velocity[2] = decoded[2] * GE_BOND_MODEL_SCALE;
    sample->valid = 0;
    return 1;
}
