#ifndef GE_ORIGINAL_DAM_MISSION_EXIT_SERVICES_H
#define GE_ORIGINAL_DAM_MISSION_EXIT_SERVICES_H

#include <stdint.h>

typedef struct CreditsEntry_s CreditsEntry;

#define GE_ORIGINAL_CREDITS_VISIBLE_LINE_CAPACITY 40U

typedef struct GeOriginalCreditsRenderLine {
    uint16_t text_id;
    int16_t position;
    int16_t y;
    int16_t alignment;
} GeOriginalCreditsRenderLine;

typedef struct GeOriginalCreditsRenderSnapshot {
    GeOriginalCreditsRenderLine
        lines[GE_ORIGINAL_CREDITS_VISIBLE_LINE_CAPACITY];
    uint32_t frame;
    uint16_t line_count;
    uint8_t visible;
    uint8_t complete;
} GeOriginalCreditsRenderSnapshot;

typedef struct GeOriginalPosendCameraSnapshot {
    float position[3];
    float look_direction[3];
    float up[3];
    float anchor[3];
    int32_t pad_id;
    uint8_t room;
    uint8_t valid;
} GeOriginalPosendCameraSnapshot;

typedef struct GeOriginalDamMissionExitSnapshot {
    uint64_t fade_ticks;
    uint32_t posend_camera_requests;
    uint32_t title_stage_requests;
    uint32_t briefing_frontiers;
    uint32_t briefing_commits;
    uint32_t death_starts;
    uint32_t death_blood_frames;
    uint32_t death_animation_finishes;
    uint32_t death_camera_starts;
    uint32_t death_title_requests;
    uint32_t death_service_frontiers;
    uint32_t fp_noinput_camera_requests;
    uint32_t camera_service_frontiers;
    int32_t stop_time;
    int32_t timer_active;
    int32_t camera_mode;
    int32_t fade_red;
    int32_t fade_green;
    int32_t fade_blue;
    float fade_fraction;
    float fade_time;
    float fade_time_max;
} GeOriginalDamMissionExitSnapshot;

/* Exact ai_24 service boundary.  The interpreter still owns every authored
 * branch and command order; this unit retains only bodies/state that the
 * sliced 3DS frontend otherwise did not link. */
void ge_original_dam_mission_exit_services_reset(void);
void ge_original_dam_mission_exit_services_tick(void);
/* Retains the post-MoveBond mission-exit portion of the canonical viewport
 * tick.  The platform supplies only the already translated N64 button mask;
 * stop/fade/title decisions remain in the original body. */
void ge_original_dam_mission_exit_process_input_exact(uint16_t buttons);
void ge_original_dam_mission_exit_services_snapshot(
    GeOriginalDamMissionExitSnapshot *snapshot);

void ge_original_dam_mission_set_camera_posend_exact(int32_t mode);
void ge_original_dam_mission_return_title_exact(void);
/* Exact AI_EndLevel command service shared by every campaign setup.  The
 * interpreter continues to own the command offset and scheduling; this body
 * preserves the original frame-buffer drain before requesting the title. */
void ge_original_campaign_end_level_dispatch_exact(void);

/* Native realization boundaries for unchanged bondview2.c bodies. The setup
 * loader supplies the endian-relocated INTROTYPE_CREDITS table; ai_24 still
 * owns when credits_state becomes 1/2 and AI_CameraOrbitPad still owns every
 * orbit parameter and camera-mode transition. */
void ge_original_campaign_credits_bind(
    const CreditsEntry *entries, uint32_t entry_count);
int ge_original_campaign_credits_render_tick_exact(
    int16_t view_top, int16_t view_height,
    GeOriginalCreditsRenderSnapshot *snapshot);
int ge_original_campaign_posend_camera_tick_exact(
    GeOriginalPosendCameraSnapshot *snapshot);

#endif
