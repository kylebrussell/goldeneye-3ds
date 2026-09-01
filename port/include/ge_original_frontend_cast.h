#ifndef GE_ORIGINAL_FRONTEND_CAST_H
#define GE_ORIGINAL_FRONTEND_CAST_H

#include <stdint.h>

typedef enum GeOriginalFrontendCastStatus {
    GE_ORIGINAL_FRONTEND_CAST_OK = 0,
    GE_ORIGINAL_FRONTEND_CAST_INVALID_ARGUMENT,
    GE_ORIGINAL_FRONTEND_CAST_HEAD_UNAVAILABLE
} GeOriginalFrontendCastStatus;

typedef enum GeOriginalFrontendCastEvent {
    GE_ORIGINAL_FRONTEND_CAST_EVENT_NONE = 0,
    GE_ORIGINAL_FRONTEND_CAST_EVENT_RELOAD,
    GE_ORIGINAL_FRONTEND_CAST_EVENT_FILE_SELECT,
    GE_ORIGINAL_FRONTEND_CAST_EVENT_RAMROM,
    GE_ORIGINAL_FRONTEND_CAST_EVENT_MISSION_SELECT
} GeOriginalFrontendCastEvent;

typedef struct GeOriginalFrontendCastServices {
    void *context;
    uint32_t (*random_next)(void *context);
    int (*choose_random_head)(void *context, int32_t body, int32_t *head);
    int (*cradle_complete)(void *context);
    int (*aztec_secret_or_00_complete)(void *context);
    int (*egypt_00_complete)(void *context);
    void (*play_intro_music)(void *context);
} GeOriginalFrontendCastServices;

typedef struct GeOriginalFrontendCastSelection {
    int32_t character_index;
    int32_t body;
    int32_t head;
    int32_t weapon_prop;
    int32_t animation_id;
    uint32_t animation_record_offset;
    float animation_start_frame;
    float animation_playback_speed;
    uint8_t animation_camera_preset;
    uint8_t animation_flip;
    uint16_t text_id[3];
} GeOriginalFrontendCastSelection;

typedef struct GeOriginalFrontendCastFrame {
    GeOriginalFrontendCastSelection selection;
    uint32_t timer;
    uint32_t duration_frames;
    uint16_t logical_width;
    uint16_t logical_height;
    float projection_fov_y_degrees;
    float projection_aspect;
    float projection_near;
    float projection_far;
    float model_scale;
    float animation_translation_scale;
    float animation_tick_speed;
    float fade;
    float camera_eye[3];
    float camera_target[3];
    float camera_up[3];
    float reflection_camera_eye_z;
    uint8_t zbuffer_enabled;
    uint8_t model_cull_both;
    uint8_t model_lighting_enabled;
    uint8_t model_texture_gen_enabled;
    uint8_t full_actor_intro;
    uint8_t pose_applied;
} GeOriginalFrontendCastFrame;

typedef struct GeOriginalFrontendCast {
    GeOriginalFrontendCastServices services;
    GeOriginalFrontendCastSelection selection;
    uint32_t timer;
    float camera_dist_start;
    float camera_dist_end;
    float camera_angle_start;
    float camera_angle_end;
    float camera_height_start;
    float camera_height_end;
    float root_position_smoothed[3];
    float root_velocity_accumulator[3];
    float target_smoothed[3];
    float target_accumulator[3];
    uint8_t full_actor_intro;
    uint8_t camera_reset;
    uint8_t initialized;
    uint8_t pose_applied;
} GeOriginalFrontendCast;

GeOriginalFrontendCastStatus ge_original_frontend_cast_reset(
    GeOriginalFrontendCast *cast,
    const GeOriginalFrontendCastServices *services,
    int full_actor_intro);
GeOriginalFrontendCastStatus ge_original_frontend_cast_begin_current(
    GeOriginalFrontendCast *cast);
GeOriginalFrontendCastStatus ge_original_frontend_cast_tick(
    GeOriginalFrontendCast *cast, int any_input,
    GeOriginalFrontendCastEvent *event);
/* Applies constructor_menu18_displaycast's unchanged root/target damping.
 * transformed_target is cast_camera_offset transformed by the canonical
 * model matrices after modelTickAnim/subcalcpos/subcalcmatrices. */
GeOriginalFrontendCastStatus ge_original_frontend_cast_apply_pose(
    GeOriginalFrontendCast *cast, const float root_position[3],
    const float transformed_target[3], uint32_t clock_ticks,
    float timer_delta);
GeOriginalFrontendCastStatus ge_original_frontend_cast_snapshot(
    const GeOriginalFrontendCast *cast, GeOriginalFrontendCastFrame *frame);
const char *ge_original_frontend_cast_contract_sha256(void);

#endif
