#ifndef GE_ORIGINAL_FIRST_PERSON_POSE_H
#define GE_ORIGINAL_FIRST_PERSON_POSE_H

#include <stdint.h>

typedef enum GeOriginalFirstPersonPoseStatus {
    GE_ORIGINAL_FIRST_PERSON_POSE_OK = 0,
    GE_ORIGINAL_FIRST_PERSON_POSE_INVALID_ARGUMENT,
    GE_ORIGINAL_FIRST_PERSON_POSE_NO_PLAYER,
    GE_ORIGINAL_FIRST_PERSON_POSE_NO_MODEL,
    GE_ORIGINAL_FIRST_PERSON_POSE_ITEM_MISMATCH,
    GE_ORIGINAL_FIRST_PERSON_POSE_NO_VIEW_TO_WORLD,
    GE_ORIGINAL_FIRST_PERSON_POSE_INVALID_VIEWPORT,
    GE_ORIGINAL_FIRST_PERSON_POSE_HIDDEN
} GeOriginalFirstPersonPoseStatus;

typedef struct GeOriginalFirstPersonPosePublication {
    float gun_camera[4][4];
    float throw_world[4][4];
    float throw_world_previous[4][4];
    float muzzle_world[3];
    float camera_depth;
    int32_t item;
    uint32_t generation;
    int visible;
} GeOriginalFirstPersonPosePublication;

typedef struct GeOriginalFirstPersonPoseState {
    void *model_header[2];
    int32_t model_item[2];
    GeOriginalFirstPersonPosePublication publication[2];
    uint32_t ticks;
    uint32_t published_hands;
    GeOriginalFirstPersonPoseStatus hand_status[2];
    int initialized;
} GeOriginalFirstPersonPoseState;

void ge_original_first_person_pose_bind(GeOriginalFirstPersonPoseState *state);

/* Mirrors the successful original on-demand model handoff: the relocated
 * header is shallow-copied into player.copy_of_body_obj_header and the exact
 * hand visibility/item bookkeeping is committed.  A cache preload made while
 * the unchanged SWITCH_LOWER equip is pending preserves that canonical hand
 * state; gunTickGameplay performs its normal on-demand visibility commit. */
GeOriginalFirstPersonPoseStatus ge_original_first_person_pose_bind_hand_model(
    unsigned hand, int32_t item, const void *native_model_header);

/* Run after original input/hand state and camera matrices have updated, and
 * before gunUpdateAndFireBothHands consumes throw_item_pos_related/field_B58. */
GeOriginalFirstPersonPoseStatus ge_original_first_person_pose_tick(
    int32_t clock_timer, float global_timer_delta);

int ge_original_first_person_pose_ready(unsigned hand);
int32_t ge_original_first_person_pose_current_item(unsigned hand);
int ge_original_first_person_pose_snapshot(
    unsigned hand, GeOriginalFirstPersonPosePublication *publication);
const char *ge_original_first_person_pose_status_name(
    GeOriginalFirstPersonPoseStatus status);

#endif
