#ifndef GE_ORIGINAL_FRONTEND_START_H
#define GE_ORIGINAL_FRONTEND_START_H

#include <stddef.h>
#include <stdint.h>

#include "ge_original_frontend_cast.h"
#include "ge_original_frontend_cursor.h"

#define GE_ORIGINAL_FRONTEND_MAX_OBJECTIVES 10U
#define GE_ORIGINAL_FRONTEND_MAX_LINES 24U

typedef enum GeOriginalFrontendRenderer {
    GE_ORIGINAL_FRONTEND_RENDERER_NONE = 0,
    GE_ORIGINAL_FRONTEND_RENDERER_PITEM_MODEL,
    GE_ORIGINAL_FRONTEND_RENDERER_RAREWARE,
    GE_ORIGINAL_FRONTEND_RENDERER_GUNBARREL
} GeOriginalFrontendRenderer;

/* Constructor inputs copied from the unchanged NTSC-U front.c/title.c
 * startup path.  Model/display-list interpretation remains in the platform
 * renderer; the bridge only publishes the original authored resource and
 * per-frame values rather than replacing either intro renderer. */
typedef struct GeOriginalFrontendPresentation {
    uint8_t startup_active;
    uint8_t renderer;
    uint8_t opacity;
    uint8_t nintendo_ambient;
    int32_t model_prop;
    uint32_t frame;
    uint32_t duration_frames;
    float nintendo_rotation_radians;
    float nintendo_scale;
    float rareware_rotation_degrees;
    uint8_t title_texture_gen;
    uint8_t title_light_ambient;
    uint8_t title_light_diffuse;
    int8_t title_light_direction[3];
    /* Exact front.c base-model transform/state. The Nintendo scale remains
     * animated in nintendo_scale; these values cover legal/title and expose
     * the authored camera instead of requiring a geometry-fit heuristic. */
    float model_uniform_scale;
    float camera_eye_z;
    float camera_target_z;
    /* Exact VI/projection and look-at inputs used by each unchanged startup
     * constructor.  The scalar z fields above remain for existing consumers;
     * these complete vectors remove renderer-side fit/camera guesses. */
    float projection_fov_y_degrees;
    float projection_aspect;
    float projection_near;
    float projection_far;
    float camera_eye[3];
    float camera_target[3];
    float camera_up[3];
    float reflection_camera_eye_z;
    uint16_t logical_width;
    uint16_t logical_height;
    uint8_t model_cull_both;
    uint8_t model_cull_back;
    uint8_t model_zbuffer_enabled;
    uint8_t model_lighting_enabled;
    uint8_t model_texture_gen_enabled;
    uint8_t model_smooth_shading_enabled;
    /* front.c passes each pitem's authored coordinates through its base
     * matrix. Consumers must not bounds-center or geometry-fit these models. */
    uint8_t model_uses_authored_origin;
    uint8_t rareware_light_ambient;
    uint8_t rareware_light_diffuse;
    uint8_t rareware_primary_rgb[3];
    uint8_t rareware_secondary_rgb[3];
} GeOriginalFrontendPresentation;

typedef enum GeOriginalFrontendInput {
    GE_ORIGINAL_FRONTEND_INPUT_CONFIRM = 1U << 0,
    GE_ORIGINAL_FRONTEND_INPUT_BACK = 1U << 1,
    GE_ORIGINAL_FRONTEND_INPUT_START = 1U << 2,
    GE_ORIGINAL_FRONTEND_INPUT_UP = 1U << 3,
    GE_ORIGINAL_FRONTEND_INPUT_DOWN = 1U << 4,
    GE_ORIGINAL_FRONTEND_INPUT_LEFT = 1U << 5,
    GE_ORIGINAL_FRONTEND_INPUT_RIGHT = 1U << 6,
    GE_ORIGINAL_FRONTEND_INPUT_FILE_COPY = 1U << 7,
    GE_ORIGINAL_FRONTEND_INPUT_FILE_ERASE = 1U << 8
} GeOriginalFrontendInput;

typedef enum GeOriginalFrontendFileAction {
    GE_ORIGINAL_FRONTEND_FILE_SELECT = 0,
    GE_ORIGINAL_FRONTEND_FILE_COPY,
    GE_ORIGINAL_FRONTEND_FILE_ERASE
} GeOriginalFrontendFileAction;

typedef struct GeOriginalFrontendServices {
    void *context;
    int (*highest_unlocked_difficulty)(void *context,int32_t mission);
    int (*select_folder)(void *context,int32_t folder);
    void (*set_selected_difficulty)(void *context,int32_t difficulty);
    void (*request_stage)(void *context,int32_t stage);
    int (*folder_has_progress)(void *context,int32_t folder);
    int (*folder_summary)(void *context,int32_t folder,
                          int32_t *mission,int32_t *difficulty);
    int (*copy_folder_to_first_free)(void *context,int32_t folder);
    int (*erase_folder)(void *context,int32_t folder);
    void (*play_sfx)(void *context,uint32_t sfx_id);
    void (*play_music)(void *context,int32_t music_id);
    void (*set_007_sliders)(void *context,float reaction,float health,
                            float damage,float accuracy);
} GeOriginalFrontendServices;

typedef struct GeOriginalFrontendTextLine {
    uint16_t text_id;
    int16_t x;
    int16_t y;
    uint8_t selected;
    uint8_t objective;
    uint8_t status;
    uint8_t horizontal_align;
    uint8_t vertical_align;
    uint8_t has_authored_position;
    float value;
} GeOriginalFrontendTextLine;

typedef struct GeOriginalFrontendMissionResult {
    uint8_t bond_kia;
    uint8_t mission_failed_or_aborted;
    uint8_t all_objectives_complete_alive;
    uint8_t append_cheat_single_player;
    uint8_t objective_status[GE_ORIGINAL_FRONTEND_MAX_OBJECTIVES];
    int32_t mission_time_ticks;
    int32_t kill_count;
    int32_t shots_fired;
    int32_t head_hits;
    int32_t body_hits;
    int32_t limb_hits;
    int32_t gun_hits;
    int32_t hat_hits;
    int32_t object_hits;
    int32_t favorite_weapon_right;
    int32_t favorite_weapon_left;
    uint8_t favorite_weapon_dual;
    uint8_t new_cheat_unlocked;
    int32_t target_time_seconds;
    int32_t best_time_seconds;
} GeOriginalFrontendMissionResult;

typedef struct GeOriginalFrontendSnapshot {
    int32_t menu;
    int32_t folder;
    int32_t mission;
    int32_t stage;
    int32_t difficulty;
    int32_t briefing_page;
    uint8_t stage_requested;
    uint8_t result_valid;
    GeOriginalFrontendPresentation presentation;
    uint8_t file_action;
    uint8_t erase_pending;
    uint8_t erase_confirm_selected;
    uint8_t folder_has_progress[4];
    int32_t folder_mission[4];
    int32_t folder_difficulty[4];
    GeOriginalFrontendMissionResult result;
    GeOriginalFrontendCursor cursor;
    float slider_007_reaction;
    float slider_007_health;
    float slider_007_damage;
    float slider_007_accuracy;
    /* Exact print_current_solo_briefing_stage_name components. Strings are
     * the authored chapter/part numerals from mission_folder_setup_entries;
     * the platform composes them with localized TITLE strings. */
    const char *chapter_number;
    const char *part_number;
    uint16_t chapter_title;
    uint16_t part_title;
    uint16_t difficulty_title;
    size_t line_count;
    GeOriginalFrontendTextLine lines[GE_ORIGINAL_FRONTEND_MAX_LINES];
    size_t tab_count;
    GeOriginalFrontendTextLine tabs[3];
} GeOriginalFrontendSnapshot;

typedef struct GeOriginalFrontendStart {
    GeOriginalFrontendServices services;
    int32_t current_menu;
    int32_t maybe_prev_menu;
    int32_t menu_update;
    int32_t folder;
    int32_t mission;
    int32_t stage;
    int32_t difficulty;
    int32_t briefing_page;
    uint32_t menu_timer;
    uint8_t stage_requested;
    uint8_t result_valid;
    uint8_t logo_button_armed;
    uint8_t first_title_visit;
    uint8_t canonical_startup;
    uint8_t previous_keypresses;
    uint8_t sequence_complete;
    uint8_t file_action;
    uint8_t erase_pending;
    uint8_t erase_confirm_selected;
    uint8_t folder_has_progress[4];
    int32_t folder_mission[4];
    int32_t folder_difficulty[4];
    GeOriginalFrontendMissionResult result;
    GeOriginalFrontendCursor cursor;
    GeOriginalFrontendWalletBounds wallet_bounds[4];
    GeOriginalFrontendWalletBounds file_action_bounds[2];
    int8_t wallet_hover;
    int8_t file_action_hover;
    uint8_t wallet_bounds_ready;
    uint8_t file_action_bounds_ready;
    uint8_t cursor_previous_tab;
    float slider_007_reaction;
    float slider_007_health;
    float slider_007_damage;
    float slider_007_accuracy;
} GeOriginalFrontendStart;

/* Platform-neutral input/snapshot bridge pinned to the solo path in front.c:
 * GoldenEye title -> file -> solo mode -> mission -> difficulty -> briefing
 * -> init_menu0B_runstage, plus the canonical report/statistics/retry and
 * complete 20-mission campaign progression. It preserves frontChangeMenu -> next menu_init
 * -> interface ordering; inputs are rising edges matching the original
 * joyGetButtonsPressedThisFrame contract. Reload transitions retain the exact
 * four displayed MENU_SWITCH_SCREENS ticks before the destination init. The
 * N64 wallet/model renderer stays
 * outside this bridge and is represented only by authored text IDs. */
int ge_original_frontend_start_reset(
    GeOriginalFrontendStart *frontend,
    const GeOriginalFrontendServices *services);
/* First-boot entry from initgamedata.c/menu_init: legal -> Nintendo ->
 * Rareware -> gunbarrel -> GoldenEye.  The legacy reset above intentionally
 * remains a direct title entry for post-mission/menu callers. */
int ge_original_frontend_start_reset_canonical(
    GeOriginalFrontendStart *frontend,
    const GeOriginalFrontendServices *services);
int ge_original_frontend_start_tick(
    GeOriginalFrontendStart *frontend,uint32_t input_edges);
int ge_original_frontend_start_cursor_tick(
    GeOriginalFrontendStart *frontend,int8_t stick_x,int8_t stick_y,
    float timer_delta);
int ge_original_frontend_start_007_drag(
    GeOriginalFrontendStart *frontend,int confirm_held);
int ge_original_frontend_start_set_wallet_bounds(
    GeOriginalFrontendStart *frontend,
    const GeOriginalFrontendWalletBounds bounds[4]);
int ge_original_frontend_start_set_file_action_bounds(
    GeOriginalFrontendStart *frontend,
    const GeOriginalFrontendWalletBounds bounds[2]);
int ge_original_frontend_start_mission_caption(
    int32_t mission,const char **chapter_number,const char **part_number);
/* Called only by the unchanged authored Rareware/gunbarrel renderer when
 * isGunBarrelInMode2/isGunBarrelInMode9 becomes true. */
int ge_original_frontend_start_sequence_complete(
    GeOriginalFrontendStart *frontend);
/* Completes the exact interface_menu18_displaycast handoff selected by the
 * separately owned canonical cast scheduler. RELOAD is intentionally kept
 * inside that scheduler/model owner. RAMROM is installed through the exact
 * authored demo header service below before this event is dispatched. */
int ge_original_frontend_start_cast_event(
    GeOriginalFrontendStart *frontend,
    GeOriginalFrontendCastEvent event);
/* replay_recorded_ramrom_at_address's solo-stage/difficulty/menu handoff once
 * the platform has validated and retained the selected authored demo file. */
int ge_original_frontend_start_ramrom(
    GeOriginalFrontendStart *frontend,int32_t stage,int32_t difficulty);
/* Called when the canonical gameplay/title-stage handoff has completed.  It
 * queues MENU_MISSION_FAILED, which is the original report page regardless of
 * whether the outcome was failure or success. */
int ge_original_frontend_start_stage_ended(
    GeOriginalFrontendStart *frontend,
    const GeOriginalFrontendMissionResult *result);
int ge_original_frontend_start_snapshot(
    const GeOriginalFrontendStart *frontend,
    GeOriginalFrontendSnapshot *snapshot);

#endif
