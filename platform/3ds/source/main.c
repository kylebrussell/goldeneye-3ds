#include <3ds.h>
#include <citro3d.h>
#include <tex3ds.h>

#include <assert.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ge_asset_pack.h"
#include "ge_blotter_model.h"
#include "ge_dam_camera.h"
#include "ge_dam_dynamic_scene.h"
#include "ge_draw_batch_visibility.h"
#include "ge_dam_environment.h"
#include "ge_dam_sky.h"
#include "ge_dam_preload_queue.h"
#include "ge_dam_room.h"
#include "ge_dam_visibility_runtime.h"
#include "ge_dam_world.h"
#include "ge_stan_collision.h"
#include "ge_stan_native.h"
#include "ge_3ds_audio.h"
#include "ge_3ds_original_autogun_beam.h"
#include "ge_3ds_fade_overlay.h"
#include "ge_3ds_original_frontend_cast.h"
#include "ge_3ds_original_hud.h"
#include "ge_3ds_save_provider.h"
#include "ge_3ds_material.h"
#include "ge_3ds_scene_texture.h"
#include "ge_audio_abi.h"
#include "ge_audio_output.h"
#include "ge_original_animation_root.h"
#include "ge_original_guard_animation_table.h"
#include "ge_original_gun_sight.h"
#include "ge_scene_part_replace.h"
#include "ge_original_player_gait.h"
#include "ge_gbi_clip.h"
#include "ge_gbi_pipeline.h"
#include "ge_libultra_audio.h"
#include "ge_original_dam_setup.h"
#include "ge_original_dam_intro.h"
#include "ge_original_dam_guard_runtime.h"
#include "ge_original_dam_guard_scene.h"
#include "ge_original_dam_guard_weapon_model.h"
#include "ge_original_dam_guards.h"
#include "ge_original_dam_mission_flow.h"
#include "ge_original_dam_mission_hud.h"
#include "ge_original_dam_mission_exit_services.h"
#include "ge_original_dam_objective_status.h"
#include "ge_original_dam_objective_models_runtime.h"
#include "ge_original_dam_world.h"
#include "ge_original_default_object.h"
#include "ge_original_door.h"
#include "ge_original_door_collision.h"
#include "ge_original_door_interaction.h"
#include "ge_original_door_runtime.h"
#include "ge_original_door_scene.h"
#include "ge_original_first_person_assets.h"
#include "ge_original_first_person_pose.h"
#include "ge_original_first_person_scene.h"
#include "ge_original_frontend_cast_model.h"
#include "ge_original_frontend_start.h"
#include "ge_original_frontend_statistics.h"
#include "ge_original_frontend_visuals.h"
#include "ge_original_ramrom_replay.h"
#include "ge_original_rareware_logo.h"
#include "ge_original_model104_runtime.h"
#include "ge_original_model178_runtime.h"
#include "ge_original_model_scene.h"
#include "ge_original_model62_runtime.h"
#include "ge_original_mission_result.h"
#include "ge_original_music_port.h"
#include "ge_original_music_runtime.h"
#include "ge_original_bond_camera.h"
#include "ge_original_bond_input_provider.h"
#include "ge_original_bond_live.h"
#include "ge_original_boss.h"
#include "ge_original_covert_modem_fire.h"
#include "ge_original_effect_buffers.h"
#include "ge_original_gameplay_services.h"
#include "ge_original_gun_live.h"
#include "ge_original_gunbarrel.h"
#include "ge_original_gunbarrel_blood.h"
#include "ge_original_gunbarrel_bond.h"
#include "ge_original_guard_bullet_hit.h"
#include "ge_original_guard_ai_trace.h"
#include "ge_original_language_text.h"
#include "ge_original_pp7_fire.h"
#include "ge_original_bond_movement.h"
#include "ge_original_player_spawn.h"
#include "ge_original_prop_state.h"
#include "ge_original_pitem_models.h"
#include "ge_original_character_appearance.h"
#include "ge_original_character_models.h"
#include "ge_original_player_body.h"
#include "ge_original_stage_guard_runtime.h"
#include "ge_original_stage_active_props.h"
#include "ge_original_stage_alarm_interaction.h"
#include "ge_original_stage_autogun_lifecycle.h"
#include "ge_original_stage_interactive_objects.h"
#include "ge_original_stage_items.h"
#include "ge_original_stage_mission_runtime.h"
#include "ge_original_stage_model_publication.h"
#include "ge_original_stage_music.h"
#include "ge_original_stage_monitor.h"
#include "ge_original_stage_monitor_surface.h"
#include "ge_original_stage_objectives.h"
#include "ge_original_stage_objective_live.h"
#include "ge_original_stage_objective_runtime.h"
#include "ge_original_stage_safe_runtime.h"
#include "ge_original_stage_security.h"
#include "ge_original_sfx_bank.h"
#include "ge_original_stage_prop_materializer.h"
#include "ge_original_stage_setup.h"
#include "ge_original_stage_special_objects.h"
#include "ge_original_stage_supplies.h"
#include "ge_original_stage_pickup.h"
#include "ge_original_stage_environment.h"
#include "ge_original_watch_mission_abort.h"
#include "ge_original_watch_mission_abort_services.h"
#include "ge_pica_material.h"
#include "ge_port.h"
#include "ge_retrace_scheduler.h"
#include "ge_stage_assets.h"
#include "ge_texture_cache.h"
#include "ge_texture_catalog.h"
#include "ge_visual_probe_tour.h"
#include "game/frametiming.h"
#include "ge_original_input.h"
#include "random.h"
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include "bondconstants.h"
#include "assets/obseg/text/LtitleE.h"
#include "vshader_shbin.h"

extern u32 weaponLoadProjectileModels(ITEM_IDS modelid);
extern struct AIRecord *ailistFindById(s32 id);
extern void *g_CurrentPlayer;
extern s32 getMissiontimer(void);
extern void sub_GAME_7F0C11FC(s32 stagenum);

#define CLEAR_COLOR 0x05070BFF
#define DAM_ENVIRONMENT_RGBA_COLOR 0xFF603010U
#define DISPLAY_TRANSFER_FLAGS                                                              \
    (GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) |       \
     GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) | \
     GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))

#if defined(GE_DAM_FULL_PROPS_LIVE)
extern void chraiUpdateOnscreenPropCount(void);
extern void chrpropUpdateAutoaimTarget(void);
#endif
extern s32 cur_player_get_autoaim(void);
extern u32 cur_player_get_lookahead(void);
extern u64 g_chrObjRandomSeed;
extern u32 cur_player_get_ammo_onscreen_setting(void);
extern u32 cur_player_get_sight_onscreen_control(void);
extern u32 cur_player_get_screen_setting(void);
extern SCREEN_RATIO_OPTION get_screen_ratio(void);
extern int cur_player_get_control_type(void);
extern u16 call_sndGetSfxSlotFirstNaturalVolume(void);
extern u16 get_mTrack2Vol(void);
extern void currentPlayerSetYAutoAimEnabled(bool enabled);
extern void currentPlayerSetXAutoAimEnabled(bool enabled);
extern void currentPlayerSetLookAheadSetting(bool enabled);
extern void gunSetGunAmmoVisible(s32 reason, bool enable);
extern void gunSetSightVisible(s32 reason, bool visible);
extern void shuffle_player_ids(void);
extern s32 lvlGetSelectedDifficulty(void);
extern s32 g_SelectedDifficulty;
extern f32 slider_007_mode_reaction;
extern f32 slider_007_mode_health;
extern f32 slider_007_mode_damage;
extern f32 slider_007_mode_accuracy;
extern void bossRunTitleStage(void);
extern void set_missionstate(MISSION_STATE_ID state);
extern s32 mission_failed_or_aborted;
extern u32 watch_screen_index;
extern s32 watch_item_is_actively_selected;
extern s32 D_800409A4;

/* Display-list decoding and original engine calls need more than libctru's
 * 32 KiB default while the port still returns large renderer snapshots. */
u32 __stacksize__ = 256U * 1024U;

typedef struct Vertex {
    float x;
    float y;
    float z;
    float u;
    float v;
    float r;
    float g;
    float b;
    float a;
} Vertex;

_Static_assert(sizeof(Vertex) == sizeof(Ge3dsOriginalHudVertex),
               "HUD platform vertices must match the shared PICA layout");
_Static_assert(sizeof(Vertex) == sizeof(Ge3dsOriginalAutogunBeamVertex),
               "autogun platform vertices must match the shared PICA layout");

#define CROSSHAIR_VERTEX_COUNT 6U
#define ICON_VERTEX_COUNT 6u
#define DAM_ENVIRONMENT_VERTEX_COUNT 6u
#define DAM_ENVIRONMENT_VERTEX_OFFSET \
    (CROSSHAIR_VERTEX_COUNT + ICON_VERTEX_COUNT)
#define DAM_CLOUD_VERTEX_CAPACITY GE_DAM_SKY_MAX_TRIANGLE_VERTICES
#define DAM_CLOUD_VERTEX_OFFSET \
    (DAM_ENVIRONMENT_VERTEX_OFFSET + DAM_ENVIRONMENT_VERTEX_COUNT)
#define RAREWARE_TRIANGLE_COUNT 8u
#define RAREWARE_VERTEX_CAPACITY (RAREWARE_TRIANGLE_COUNT * 3u)
#define RAREWARE_VERTEX_OFFSET \
    (DAM_CLOUD_VERTEX_OFFSET + DAM_CLOUD_VERTEX_CAPACITY)
#define RAREWARE_FRONT_TRIANGLE_COUNT GE_ORIGINAL_RAREWARE_FRONT_TRIANGLES
#define RAREWARE_FRONT_VERTEX_CAPACITY (RAREWARE_FRONT_TRIANGLE_COUNT * 3u)
#define RAREWARE_FRONT_VERTEX_OFFSET \
    (RAREWARE_VERTEX_OFFSET + RAREWARE_VERTEX_CAPACITY)
#define RAREWARE_BODY_TRIANGLE_COUNT 242u
#define RAREWARE_BODY_VERTEX_CAPACITY (RAREWARE_BODY_TRIANGLE_COUNT * 3u)
#define RAREWARE_BODY_VERTEX_OFFSET \
    (RAREWARE_FRONT_VERTEX_OFFSET + RAREWARE_FRONT_VERTEX_CAPACITY)
#define BLOTTER_VERTEX_COUNT GE_BLOTTER_MODEL_VERTEX_COUNT
#define BLOTTER_VERTEX_OFFSET \
    (RAREWARE_BODY_VERTEX_OFFSET + RAREWARE_BODY_VERTEX_CAPACITY)
#define DAM_SCENE_VERTEX_CAPACITY 16384U
#define DAM_SCENE_PROJECTED_VERTEX_CAPACITY 65536U
#define DAM_ROOM_VERTEX_COUNT DAM_SCENE_PROJECTED_VERTEX_CAPACITY
#define DAM_ROOM_VERTEX_OFFSET (BLOTTER_VERTEX_OFFSET + BLOTTER_VERTEX_COUNT)
#define FIRST_PERSON_VERTEX_CAPACITY 16384U
#define FIRST_PERSON_VERTEX_OFFSET \
    (DAM_ROOM_VERTEX_OFFSET + DAM_ROOM_VERTEX_COUNT)
#define AUTOGUN_BEAM_VERTEX_CAPACITY \
    (GE_3DS_ORIGINAL_AUTOGUN_BEAM_CAPACITY \
        * GE_3DS_ORIGINAL_AUTOGUN_BEAM_VERTICES)
#define AUTOGUN_BEAM_VERTEX_OFFSET \
    (FIRST_PERSON_VERTEX_OFFSET + FIRST_PERSON_VERTEX_CAPACITY)
#define GUARD_MUZZLE_FLASH_CAPACITY 256U
#define GUARD_MUZZLE_FLASH_VERTICES 6U
#define GUARD_MUZZLE_FLASH_VERTEX_CAPACITY \
    (GUARD_MUZZLE_FLASH_CAPACITY * GUARD_MUZZLE_FLASH_VERTICES)
#define GUARD_MUZZLE_FLASH_VERTEX_OFFSET \
    (AUTOGUN_BEAM_VERTEX_OFFSET + AUTOGUN_BEAM_VERTEX_CAPACITY)
#define FADE_OVERLAY_VERTEX_COUNT 6U
#define FADE_OVERLAY_VERTEX_OFFSET \
    (GUARD_MUZZLE_FLASH_VERTEX_OFFSET + GUARD_MUZZLE_FLASH_VERTEX_CAPACITY)
#define ORIGINAL_HUD_VERTEX_CAPACITY GE_3DS_ORIGINAL_HUD_VERTEX_CAPACITY
#define ORIGINAL_HUD_VERTEX_OFFSET \
    (FADE_OVERLAY_VERTEX_OFFSET + FADE_OVERLAY_VERTEX_COUNT)
#define ORIGINAL_GAMEPLAY_HUD_SOLID_VERTEX_OFFSET \
    (ORIGINAL_HUD_VERTEX_OFFSET + ORIGINAL_HUD_VERTEX_CAPACITY)
#define ORIGINAL_GAMEPLAY_HUD_FONT_VERTEX_OFFSET \
    (ORIGINAL_GAMEPLAY_HUD_SOLID_VERTEX_OFFSET \
        + GE_3DS_ORIGINAL_GAMEPLAY_HUD_SOLID_VERTEX_CAPACITY)
#define ORIGINAL_BOTTOM_HUD_VERTEX_OFFSET \
    (ORIGINAL_GAMEPLAY_HUD_FONT_VERTEX_OFFSET \
        + GE_3DS_ORIGINAL_GAMEPLAY_HUD_FONT_VERTEX_CAPACITY)
#define ORIGINAL_AMMO_ICON_VERTEX_CAPACITY 12U
#ifndef GE_3DS_LIVE_DIAGNOSTICS
#define GE_3DS_LIVE_DIAGNOSTICS 0
#endif
#define ORIGINAL_AMMO_ICON_VERTEX_OFFSET \
    (ORIGINAL_BOTTOM_HUD_VERTEX_OFFSET + ORIGINAL_HUD_VERTEX_CAPACITY)
#define ORIGINAL_FRONTEND_MODEL_VERTEX_CAPACITY 8192U
#define ORIGINAL_FRONTEND_EMBEDDED_TEXTURE_CAPACITY 8U
#define ORIGINAL_FRONTEND_GUNBARREL_BOND_VERTEX_OFFSET 96U
#define ORIGINAL_FRONTEND_MODEL_VERTEX_OFFSET \
    (ORIGINAL_AMMO_ICON_VERTEX_OFFSET + ORIGINAL_AMMO_ICON_VERTEX_CAPACITY)
#define TOTAL_VERTEX_COUNT \
    (ORIGINAL_FRONTEND_MODEL_VERTEX_OFFSET \
        + ORIGINAL_FRONTEND_MODEL_VERTEX_CAPACITY)
_Static_assert(ORIGINAL_BOTTOM_HUD_VERTEX_OFFSET
                   + ORIGINAL_HUD_VERTEX_CAPACITY
                   == ORIGINAL_AMMO_ICON_VERTEX_OFFSET,
               "bottom HUD vertex region must fit the shared buffer");
#define AUDIO_RING_FRAMES 4096U
/* Depot's authored world reaches 145 unique textures all-connected; its exact
 * supported ordinary Pitem models add at most 29 (174 before overlap). Keep a
 * small measured margin while remaining well below the available linear heap. */
#define DAM_SCENE_TEXTURE_CAPACITY 192U
#define DAM_WORLD_ROOM_LOAD_CAPACITY 10U
#define FIRST_PERSON_SOURCE_VERTEX_CAPACITY \
    GE_ORIGINAL_FIRST_PERSON_SUPPORTED_VERTEX_CAPACITY
#define FIRST_PERSON_BATCH_CAPACITY \
    GE_ORIGINAL_FIRST_PERSON_SUPPORTED_BATCH_CAPACITY
_Static_assert(FIRST_PERSON_SOURCE_VERTEX_CAPACITY
                   <= FIRST_PERSON_VERTEX_CAPACITY,
               "authored first-person scene exceeds GPU vertex capacity");
#define FIRST_PERSON_TEXTURE_CAPACITY \
    GE_ORIGINAL_FIRST_PERSON_SUPPORTED_TEXTURE_CAPACITY
#define VISUAL_PROBE_TOUR_CAPACITY 384U
#define VISUAL_PROBE_PATH_CAPACITY 160U
#define STAGE_SELECTION_PATH \
    "sdmc:/3ds/goldeneye-3ds/stage.cfg"
#define INPUT_PROBE_PATH \
    "sdmc:/3ds/goldeneye-3ds/dam-input-probe.cfg"
#define INPUT_PROBE_RESULT_PATH \
    "sdmc:/3ds/goldeneye-3ds/dam-input-probe.result"
#define FRONTEND_VISUAL_PROBE_PATH \
    "sdmc:/3ds/goldeneye-3ds/frontend-visual-probe.cfg"
#define SAVE_SLOT_PATH \
    "sdmc:/3ds/goldeneye-3ds/goldeneye.sav"

_Static_assert(CROSSHAIR_VERTEX_COUNT == 6U,
               "the canonical sight rectangle publishes six vertices");
_Static_assert(TOTAL_VERTEX_COUNT <= SIZE_MAX / sizeof(Vertex),
               "the shared vertex-buffer byte count must fit size_t");

static size_t renderer_vertex_flush_bytes(size_t vertex_count,
                                          size_t vertex_capacity)
{
    assert(vertex_count <= vertex_capacity);
    assert(vertex_capacity <= TOTAL_VERTEX_COUNT);
    return vertex_count * sizeof(Vertex);
}

static DVLB_s *shader_dvlb;
static shaderProgram_s shader_program;
static C3D_Mtx projection;
static int projection_uniform;
static void *vertex_buffer;
static C3D_Tex copy_icon_texture;
static bool copy_icon_loaded;
static C3D_Tex rareware_textures[4];
static bool rareware_textures_loaded[4];
static C3D_Tex rareware_body_texture;
static Tex3DS_SubTexture rareware_body_subtexture;
static bool rareware_body_texture_loaded;
static C3D_Tex rareware_front_texture;
static Tex3DS_SubTexture rareware_front_subtexture;
static bool rareware_front_texture_loaded;
static C3D_Tex blotter_texture;
static bool blotter_texture_loaded;
static C3D_Tex dam_cloud_texture;
static bool dam_cloud_texture_loaded;
static C3D_Tex autogun_beam_texture;
static Tex3DS_SubTexture autogun_beam_subtexture;
static bool autogun_beam_texture_loaded;
static C3D_Tex original_hud_font_texture;
static bool original_hud_font_texture_loaded;
static Ge3dsOriginalHudAtlas original_hud_atlas;
static C3D_Tex original_gameplay_hud_font_texture;
static bool original_gameplay_hud_font_texture_loaded;
static Ge3dsOriginalHudAtlas original_gameplay_hud_atlas;

typedef struct RuntimeFrontendSpriteTexture {
    C3D_Tex texture;
    Tex3DS_SubTexture subtexture;
    bool loaded;
} RuntimeFrontendSpriteTexture;

static RuntimeFrontendSpriteTexture original_frontend_sprite_textures[
    GE_3DS_ORIGINAL_FRONTEND_MAX_SPRITES];

typedef struct RuntimeAmmoIconTexture {
    const GeOriginalAmmoIconAsset *asset;
    C3D_Tex texture;
    Tex3DS_SubTexture subtexture;
    bool loaded;
} RuntimeAmmoIconTexture;

static RuntimeAmmoIconTexture
    original_ammo_icon_textures[GE_ORIGINAL_AMMO_ICON_ASSET_COUNT];

typedef struct RuntimeVisualProbeTour {
    GeVisualProbeView views[VISUAL_PROBE_TOUR_CAPACITY];
    GeVisualProbeTour tour;
    GeVisualProbeTourStatus status;
    size_t current_view;
    uint32_t view_elapsed_frames;
    size_t peak_resident_rooms;
    size_t peak_scene_textures;
    size_t peak_visible_rooms;
    size_t camera_failure_views;
    size_t visibility_failure_views;
    uint64_t native_actor_tick_count;
    size_t native_actor_prop_count;
    size_t native_actor_materializer_ready_count;
    size_t native_actor_materializer_constructed_count;
    size_t native_actor_materializer_failed_count;
    size_t native_actor_materialized_live_count;
    size_t native_owned_ordinary_embedded_count;
    size_t native_owned_ordinary_assigned_count;
    size_t native_owned_ordinary_pending_count;
    size_t native_actor_first_failed_command;
    uint32_t native_actor_first_failed_type;
    uint32_t native_actor_first_failed_construct_status;
    uint32_t native_actor_first_failed_placement_status;
    size_t native_actor_authored_weapon_count;
    size_t native_actor_attached_weapon_count;
    size_t native_actor_attached_hat_count;
    uint64_t native_actor_guard_overlay_updates;
    uint64_t native_actor_door_overlay_updates;
    uint64_t native_actor_overlay_full_rebuilds;
    uint64_t native_actor_guard_cache_builds;
    uint64_t native_actor_guard_cache_topology_rebuilds;
    uint64_t native_actor_door_cache_builds;
    uint64_t native_actor_door_cache_topology_rebuilds;
    uint64_t native_mission_ai_offset_hash;
    uint64_t native_mission_tick_count;
    size_t native_mission_actor_count;
    uint64_t native_monitor_tick_count;
    size_t native_monitor_count;
    size_t native_monitor_screen_count;
    uint64_t native_monitor_noop_tick_count;
    uint64_t native_monitor_surface_update_count;
    uint64_t native_monitor_surface_unchanged_count;
    uint64_t native_monitor_surface_failure_count;
    uint64_t native_articulated_scene_update_count;
    uint64_t native_articulated_scene_unchanged_count;
    uint64_t native_articulated_scene_topology_change_count;
    uint64_t native_articulated_scene_failure_count;
    size_t native_supply_count;
    size_t native_supply_slot_model_load_count;
    size_t native_tinted_glass_count;
    size_t native_cctv_count;
    size_t native_autogun_count;
    size_t native_gas_releasing_count;
    size_t native_safe_count;
    size_t native_safe_relation_count;
    uint32_t native_safe_status;
    uint32_t native_safe_relation_status;
    uint32_t native_stage_init_mask;
    uint64_t native_door_interaction_tick_count;
    uint32_t native_door_interaction_activation_count;
    size_t native_objective_count;
    size_t native_objective_criterion_count;
    size_t native_objective_blocked_tag_count;
    size_t native_objective_evaluation_ready_count;
    size_t native_objective_evaluation_blocked_count;
    uint64_t native_objective_evaluation_ticks;
    uint32_t native_actor_tick_status;
    uint32_t native_actor_service_status;
    uint64_t native_dam_guard_tick_count;
    uint64_t native_dam_guard_rejected_tick_count;
    uint32_t native_dam_guard_last_status;
    uint64_t native_dam_guard_weapon_fire_count;
    uint64_t native_dam_guard_player_damage_count;
    uint64_t native_dam_door_interaction_tick_count;
    uint32_t native_dam_door_interaction_activation_count;
    size_t native_dam_alarm_count;
    uint32_t native_dam_alarm_model_status;
    uint32_t native_dam_alarm_misc_status;
    uint32_t native_dam_alarm_construct_status;
    uint32_t native_dam_alarm_placement_status;
    uint32_t native_dam_alarm_instance_bits;
    size_t native_dam_alarm_scan_count;
    uint32_t native_dam_alarm_materialize_failure;
    size_t native_dam_alarm_scene_part_count;
    uint32_t native_dam_model_scene_status;
    uint32_t native_dam_model_scene_ready;
    uint32_t native_dam_scene_prerequisite_bits;
    uint32_t native_dam_scene_install_failure;
    uint32_t native_dam_alarm_status;
    uint64_t native_dam_alarm_interaction_tick_count;
    uint64_t native_dam_alarm_interaction_activation_count;
    size_t native_dam_objective_count;
    size_t native_dam_objective_blocked_tag_count;
    size_t native_dam_objective_evaluation_ready_count;
    size_t native_dam_objective_evaluation_blocked_count;
    uint64_t native_dam_objective_evaluation_ticks;
    size_t native_dam_objective_hud_message_count;
    uint64_t native_dam_guard_overlay_updates;
    uint64_t native_dam_door_overlay_updates;
    uint64_t native_dam_overlay_full_rebuilds;
    uint32_t native_dam_mission_tick_count;
    uint32_t native_dam_mission_ai_offset;
    uint32_t native_dam_mission_exit_ai_offset;
    uint32_t native_dam_mission_objective_registers;
    uint32_t native_dam_mission_hud_message_count;
    uint32_t native_dam_full_props_activated;
    uint64_t displayed_frame_count;
    uint64_t displayed_frame_total_ms;
    uint64_t displayed_frame_peak_ms;
    uint64_t simulation_total_ms;
    uint64_t gpu_total_ms;
    uint64_t diagnostic_attempts;
    uint64_t diagnostic_ordinary_resident_install_successes;
    uint64_t diagnostic_ordinary_resident_eviction_successes;
    uint64_t diagnostic_overlay_full_rebuilds;
    uint64_t diagnostic_door_overlay_failures;
    uint64_t diagnostic_guard_overlay_failures;
    uint64_t diagnostic_monitor_overlay_failures;
    uint64_t diagnostic_articulated_failures;
    uint32_t diagnostic_ordinary_overlay_status;
    uint8_t diagnostic_camera_updated;
    uint8_t diagnostic_ordinary_refresh_attempted;
    uint8_t diagnostic_ordinary_refresh_succeeded;
    uint8_t diagnostic_ordinary_scene_ready;
    char source_path[VISUAL_PROBE_PATH_CAPACITY];
    char result_path[VISUAL_PROBE_PATH_CAPACITY];
    char diagnostic_path[VISUAL_PROBE_PATH_CAPACITY];
    bool current_view_camera_failed;
    bool current_view_visibility_failed;
    bool current_view_ready;
    bool enabled;
} RuntimeVisualProbeTour;

static RuntimeVisualProbeTour visual_probe_tour;

typedef struct RuntimeFrameProfile {
    uint64_t frame_ms;
    uint64_t simulation_ms;
    uint64_t overlay_ms;
    uint64_t camera_ms;
    uint64_t first_person_ms;
    uint64_t gpu_ms;
    uint32_t samples;
} RuntimeFrameProfile;

static RuntimeFrameProfile frame_profile;
/* Unlike the rolling console profile above, this accumulator is never reset
 * by the 15-frame status refresh. Input-probe results therefore describe the
 * complete run rather than whichever short window happened to be last. */
static RuntimeFrameProfile frame_profile_total;

typedef struct RuntimeFineProfile {
    uint64_t guard_matrix_ticks;
    uint64_t guard_matrix_calls;
    uint64_t guard_scene_ticks;
    uint64_t guard_scene_calls;
    uint64_t guard_overlay_commit_ticks;
    uint64_t guard_overlay_commit_calls;
    uint64_t guard_gpu_upload_ticks;
    uint64_t guard_gpu_upload_calls;
    uint64_t guard_gpu_upload_vertices;
    uint64_t guard_gpu_full_upload_vertices;
    uint64_t guard_gpu_uv_remap_vertices;
    uint64_t world_gpu_flush_ticks;
    uint64_t world_gpu_flush_calls;
    uint64_t world_gpu_flush_vertices;
    uint64_t frame_begin_ticks;
    uint64_t renderer_draw_ticks;
    uint64_t frame_end_ticks;
    uint64_t rendered_frames;
    uint64_t world_draw_calls;
    uint64_t world_authored_batches;
    uint64_t first_person_draw_calls;
    uint64_t first_person_authored_batches;
    uint64_t world_material_apply_calls;
    uint64_t world_material_apply_reuses;
    uint64_t world_texture_lookups;
    uint64_t first_person_material_apply_calls;
    uint64_t first_person_material_apply_reuses;
    uint64_t first_person_texture_lookups;
    uint64_t world_material_prepare_hits;
    uint64_t world_material_prepare_misses;
    uint64_t first_person_material_prepare_hits;
    uint64_t first_person_material_prepare_misses;
    uint64_t world_frustum_tests;
    uint64_t world_frustum_culled_batches;
    uint64_t world_frustum_culled_vertices;
    uint64_t world_frustum_bounds_inside;
    uint64_t world_frustum_bounds_outside;
    uint64_t world_frustum_first_vertex_visible;
    uint64_t first_person_phase_ticks[4];
    uint64_t first_person_peak_phase_ticks[4];
    uint64_t first_person_peak_ticks;
    uint64_t guard_visibility_update_ticks;
    uint64_t guard_visibility_publish_ticks;
    uint64_t guard_texture_ticks;
    uint64_t guard_replace_ticks;
    uint64_t guard_import_ticks;
    uint64_t guard_refresh_peak[7];
    uint64_t idle_present_skips;
} RuntimeFineProfile;

static RuntimeFineProfile fine_profile;

static uint64_t runtime_profile_clock(void *context)
{
    (void)context;
    return svcGetSystemTick();
}

enum {
    /* Dam's authored objective route contains 102 waypoint/pad stops before
     * interaction dwell points are inserted. Keep this diagnostic boundary
     * large enough to exercise that whole route through normal input. */
    RUNTIME_INPUT_PROBE_SEGMENT_CAPACITY = 160,
    /* A controller-only traversal of all four Dam objectives covers roughly
     * 75k world units and includes combat/interaction dwells. At measured
     * live movement speed it needs about 45k original simulation ticks. */
    RUNTIME_INPUT_PROBE_MAX_FRAMES = 60000,
    RUNTIME_INPUT_PROBE_TRACE_CAPACITY = 128
};

typedef struct RuntimeInputProbeSegment {
    GePortInput input;
    uint32_t frames;
} RuntimeInputProbeSegment;

typedef struct RuntimeInputProbeTarget {
    float x;
    float z;
    float radius;
    uint32_t held;
    uint32_t dwell_frames;
    uint32_t pulse_period;
    float dwell_look_y;
    int32_t aim_chr;
} RuntimeInputProbeTarget;

typedef struct RuntimeInputProbeTrace {
    uint32_t frame;
    uint8_t target;
    float position_x;
    float position_z;
    float look_x;
    float look_z;
    float move_x;
    float move_y;
    float angle;
} RuntimeInputProbeTrace;

typedef struct RuntimeInputProbeSlowFrame {
    uint32_t displayed_frame;
    uint8_t room;
    uint64_t total_ms;
    uint64_t simulation_ms;
    uint64_t overlay_ms;
    uint64_t camera_ms;
    uint64_t first_person_ms;
    uint64_t gpu_ms;
    uint64_t topology_rebuilds;
    uint64_t topology_component_misses;
    uint64_t scene_generations;
    uint64_t overlay_full_rebuilds;
} RuntimeInputProbeSlowFrame;

typedef struct RuntimeInputProbe {
    GePortInput input;
    RuntimeInputProbeSegment segments[RUNTIME_INPUT_PROBE_SEGMENT_CAPACITY];
    size_t segment_count;
    RuntimeInputProbeTarget targets[RUNTIME_INPUT_PROBE_SEGMENT_CAPACITY];
    size_t target_count;
    size_t target_index;
    uint32_t last_sample_held;
    int8_t steer_direction;
    uint32_t target_reached_frames[RUNTIME_INPUT_PROBE_SEGMENT_CAPACITY];
    uint32_t target_dwell_remaining;
    RuntimeInputProbeTrace trace[RUNTIME_INPUT_PROBE_TRACE_CAPACITY];
    size_t trace_count;
    RuntimeInputProbeSlowFrame slow_frames[8];
    size_t slow_frame_count;
    uint32_t target_frames;
    uint32_t active_frames;
    uint32_t displayed_frames;
    uint32_t simulation_frames;
    uint32_t last_route_sample_frame;
    uint64_t displayed_total_ms;
    uint64_t displayed_peak_ms;
    uint64_t displayed_peak_after_warmup_ms;
    uint32_t displayed_samples_after_warmup;
    uint32_t displayed_over_16_ms;
    uint32_t displayed_over_25_ms;
    uint32_t displayed_over_33_ms;
    uint32_t displayed_over_50_ms;
    uint64_t move_tick_start;
    float start_position[3];
    float stop_position[3];
    float settle_position[3];
    float end_position[3];
    float start_look[3];
    float end_look[3];
    float minimum_y;
    float maximum_y;
    uint8_t start_room;
    uint8_t end_room;
    uint8_t last_room;
    uint8_t visited_rooms[256];
    uint32_t visited_room_count;
    uint32_t room_transition_count;
    uint32_t gate_start_generation[2];
    uint32_t gate_activation_count[2];
    uint32_t gate_last_interaction_activations;
    uint32_t gate_both_open_frames;
    float gate_max_open_position[2];
    uint32_t aim_resolver_calls;
    uint32_t aim_target_found;
    uint32_t aim_matrix_ready;
    uint32_t aim_resolver_successes;
    uint32_t aim_command_samples;
    int32_t last_aim_chr;
    int32_t last_aim_guard_index;
    uint32_t last_aim_target_index;
    uint32_t last_aim_route_frame;
    uint32_t last_aim_dwell_remaining;
    uint32_t last_aim_held;
    uint32_t last_aim_prop_flags;
    uint8_t last_aim_visible;
    uint8_t last_aim_matrices_ready;
    uint8_t last_aim_death_complete;
    float last_aim_world[3];
    float last_aim_camera_position[3];
    float last_aim_camera_look[3];
    float last_aim_command_look[2];
    float last_aim_prop_zdepth;
    float last_aim_model_size;
    uint8_t transition_rooms[RUNTIME_INPUT_PROBE_SEGMENT_CAPACITY];
    uint32_t transition_frames[RUNTIME_INPUT_PROBE_SEGMENT_CAPACITY];
    size_t transition_record_count;
    bool started;
    bool stop_captured;
    bool settle_captured;
    bool route_sampled;
    bool neutral_cutover_active;
    uint32_t neutral_until_frame;
    float maximum_player_armour;
    uint32_t first_armour_frame;
    bool armour_observed;
    bool enabled;
} RuntimeInputProbe;

static GePortInput input_probe_cache_sample(
    RuntimeInputProbe *runtime, GePortInput input)
{
    runtime->input = input;
    return input;
}

static uint32_t input_probe_route_frame(const RuntimeInputProbe *runtime)
{
    return runtime != NULL ? runtime->simulation_frames : 0U;
}

static bool load_input_probe(RuntimeInputProbe *runtime)
{
    FILE *stream;
    char magic[32];
    unsigned version;
    unsigned frames;
    unsigned active_frames;
    unsigned held;
    if (runtime == NULL) return false;
    memset(runtime, 0, sizeof(*runtime));
    stream = fopen(INPUT_PROBE_PATH, "rb");
    if (stream == NULL) return false;
    if (fscanf(stream, "%31s %u", magic, &version) != 2
            || strcmp(magic, "GE_INPUT_PROBE") != 0
            || (version != 1U && version != 2U && version != 3U
                && version != 4U && version != 5U && version != 6U
                && version != 7U)) {
        fclose(stream);
        return false;
    }
    if (version == 3U || version == 4U || version == 5U
            || version == 6U || version == 7U) {
        unsigned target_count;
        size_t index;
        if (fscanf(stream, " frames %u", &frames) != 1
                || fscanf(stream, " targets %u", &target_count) != 1
                || frames == 0U || frames > RUNTIME_INPUT_PROBE_MAX_FRAMES
                || target_count == 0U
                || target_count > RUNTIME_INPUT_PROBE_SEGMENT_CAPACITY) {
            fclose(stream);
            return false;
        }
        for (index = 0U; index < target_count; ++index) {
            RuntimeInputProbeTarget *target = &runtime->targets[index];
            unsigned dwell = 0U;
            unsigned pulse_period = 0U;
            int aim_chr = -1;
            const int read = version == 7U
                ? fscanf(stream, " target %f %f %f %u %u %f %u %d",
                    &target->x, &target->z, &target->radius, &held, &dwell,
                    &target->dwell_look_y, &pulse_period, &aim_chr)
                : version == 6U
                ? fscanf(stream, " target %f %f %f %u %u %f %u",
                    &target->x, &target->z, &target->radius, &held, &dwell,
                    &target->dwell_look_y, &pulse_period)
                : version == 5U
                ? fscanf(stream, " target %f %f %f %u %u %f",
                    &target->x, &target->z, &target->radius, &held, &dwell,
                    &target->dwell_look_y)
                : version == 4U
                ? fscanf(stream, " target %f %f %f %u %u",
                    &target->x, &target->z, &target->radius, &held, &dwell)
                : fscanf(stream, " target %f %f %f %u",
                    &target->x, &target->z, &target->radius, &held);
            if (read != (version == 7U ? 8 : version == 6U ? 7
                              : version == 5U ? 6
                              : version == 4U ? 5 : 4)
                    || !isfinite(target->x) || !isfinite(target->z)
                    || !isfinite(target->radius) || target->radius <= 0.0f
                    || !isfinite(target->dwell_look_y)
                    || target->dwell_look_y < -1.0f
                    || target->dwell_look_y > 1.0f || dwell > frames
                    || pulse_period > frames || aim_chr < -1
                    || aim_chr > INT16_MAX) {
                fclose(stream);
                return false;
            }
            target->held = held;
            target->dwell_frames = dwell;
            target->pulse_period = (uint32_t)pulse_period;
            target->aim_chr = (int32_t)aim_chr;
        }
        fclose(stream);
        runtime->target_count = target_count;
        runtime->target_frames = (uint32_t)frames;
        runtime->active_frames = runtime->target_frames;
        runtime->enabled = true;
        return true;
    }
    if (version == 2U) {
        unsigned segment_count;
        size_t index;
        uint64_t total_frames = 0U;
        if (fscanf(stream, " segments %u", &segment_count) != 1
                || segment_count == 0U
                || segment_count > RUNTIME_INPUT_PROBE_SEGMENT_CAPACITY) {
            fclose(stream);
            return false;
        }
        for (index = 0U; index < segment_count; ++index) {
            RuntimeInputProbeSegment *segment = &runtime->segments[index];
            unsigned segment_frames;
            if (fscanf(stream, " segment %u %f %f %f %f %u",
                    &segment_frames,
                    &segment->input.move_x, &segment->input.move_y,
                    &segment->input.look_x, &segment->input.look_y,
                    &held) != 6
                    || segment_frames == 0U
                    || !isfinite(segment->input.move_x)
                    || !isfinite(segment->input.move_y)
                    || !isfinite(segment->input.look_x)
                    || !isfinite(segment->input.look_y)) {
                fclose(stream);
                return false;
            }
            segment->frames = (uint32_t)segment_frames;
            segment->input.held = held;
            total_frames += segment->frames;
        }
        fclose(stream);
        if (total_frames == 0U || total_frames > 3600U) return false;
        runtime->segment_count = segment_count;
        runtime->target_frames = (uint32_t)total_frames;
        runtime->active_frames = runtime->target_frames;
        runtime->enabled = true;
        return true;
    }
    if (fscanf(stream, " frames %u", &frames) != 1
            || fscanf(stream, " active_frames %u", &active_frames) != 1
            || fscanf(stream, " move_x %f", &runtime->input.move_x) != 1
            || fscanf(stream, " move_y %f", &runtime->input.move_y) != 1
            || fscanf(stream, " look_x %f", &runtime->input.look_x) != 1
            || fscanf(stream, " look_y %f", &runtime->input.look_y) != 1
            || fscanf(stream, " held %u", &held) != 1
            || frames == 0U || frames > 3600U
            || active_frames == 0U || active_frames > frames
            || !isfinite(runtime->input.move_x)
            || !isfinite(runtime->input.move_y)
            || !isfinite(runtime->input.look_x)
            || !isfinite(runtime->input.look_y)) {
        fclose(stream);
        return false;
    }
    fclose(stream);
    runtime->target_frames = (uint32_t)frames;
    runtime->active_frames = (uint32_t)active_frames;
    runtime->input.held = held;
    runtime->enabled = true;
    return true;
}

static void input_probe_apply_live_aim(GePortInput *input,
    const GeOriginalPlayerViewState *player, const float *aim_position)
{
    float dx;
    float dy;
    float dz;
    float target_length;
    float horizontal_length;
    float look_length;
    float look_horizontal_length;
    float dot;
    float cross;
    float angle;
    float target_pitch;
    float look_pitch;
    if (input == NULL || player == NULL || !player->initialized
            || aim_position == NULL) return;
    dx = aim_position[0] - player->camera_position[0];
    dy = aim_position[1] - player->camera_position[1];
    dz = aim_position[2] - player->camera_position[2];
    target_length = sqrtf(dx * dx + dz * dz);
    look_length = sqrtf(player->camera_look[0] * player->camera_look[0]
                      + player->camera_look[2] * player->camera_look[2]);
    if (target_length <= 0.001f || look_length <= 0.001f) return;
    dx /= target_length;
    dz /= target_length;
    dot = (player->camera_look[0] * dx
         + player->camera_look[2] * dz) / look_length;
    cross = (player->camera_look[0] * dz
           - player->camera_look[2] * dx) / look_length;
    angle = atan2f(cross, dot);
    if (angle > M_PI_F) angle -= M_TAU_F;
    input->look_x = fmaxf(-1.0f, fminf(1.0f, angle * 1.5f));
    horizontal_length = target_length;
    look_horizontal_length = sqrtf(
        player->camera_look[0] * player->camera_look[0]
      + player->camera_look[2] * player->camera_look[2]);
    if (horizontal_length > 0.001f && look_horizontal_length > 0.001f) {
        target_pitch = atan2f(dy, horizontal_length);
        look_pitch = atan2f(player->camera_look[1], look_horizontal_length);
        /* HONEY initially maps U_CBUTTONS to speedVertaDown, but the stock
         * non-inverted-pitch branch swaps speedVertaDown/speedVertaUp before
         * applying vertical speed.  ge_port maps positive look_y to U, so a
         * positive target pitch error is the command that raises the view. */
        input->look_y = fmaxf(-1.0f,
            fminf(1.0f, (target_pitch - look_pitch) * 1.5f));
    }
}

static GePortInput input_probe_sample(RuntimeInputProbe *runtime,
    const GeOriginalPlayerViewState *player, const float *aim_position,
    bool aim_guard_complete)
{
    GePortInput input = {0};
    uint32_t first_frame = 0U;
    uint32_t route_frame;
    size_t index;
    if (runtime == NULL || !runtime->enabled) return input;
    route_frame = input_probe_route_frame(runtime);
    if (runtime->route_sampled
            && route_frame == runtime->last_route_sample_frame)
        return runtime->input;
    runtime->last_route_sample_frame = route_frame;
    runtime->route_sampled = true;
    if (runtime->neutral_cutover_active) {
        if (route_frame <= runtime->neutral_until_frame)
            return input_probe_cache_sample(runtime, input);
        runtime->neutral_cutover_active = false;
    }
    if (runtime->target_count != 0U) {
        const float *position;
        const float *look;
        RuntimeInputProbeTarget *target;
        float dx;
        float dz;
        float target_length;
        float look_length;
        float dot;
        float cross;
        float angle;
        if (player == NULL || !player->initialized)
            return input_probe_cache_sample(runtime, input);
        position = player->collision_position;
        look = player->camera_look;
        while (runtime->target_index < runtime->target_count) {
            target = &runtime->targets[runtime->target_index];
            dx = target->x - position[0];
            dz = target->z - position[2];
            if (dx * dx + dz * dz > target->radius * target->radius) break;
            if (runtime->target_dwell_remaining == 0U
                    && target->dwell_frames != 0U) {
                runtime->target_dwell_remaining = target->dwell_frames;
                target->dwell_frames = 0U;
            }
            if (runtime->target_dwell_remaining != 0U) {
                if (aim_guard_complete && target->aim_chr >= 0) {
                    runtime->target_dwell_remaining = 0U;
                    runtime->target_reached_frames[runtime->target_index] =
                        route_frame;
                    ++runtime->target_index;
                    runtime->steer_direction = 0;
                    runtime->last_sample_held = 0U;
                    runtime->neutral_cutover_active = true;
                    runtime->neutral_until_frame = route_frame == UINT32_MAX
                        ? UINT32_MAX : route_frame + 1U;
                    return input_probe_cache_sample(runtime, input);
                }
                input.held = target->pulse_period == 0U
                        || route_frame % target->pulse_period == 0U
                    ? target->held : 0U;
                input.look_y = target->dwell_look_y;
                input_probe_apply_live_aim(&input, player, aim_position);
                input.pressed = input.held & ~runtime->last_sample_held;
                runtime->last_sample_held = input.held;
                --runtime->target_dwell_remaining;
                if (runtime->target_dwell_remaining == 0U) {
                    runtime->target_reached_frames[runtime->target_index] =
                        route_frame;
                    ++runtime->target_index;
                    runtime->steer_direction = 0;
                }
                return input_probe_cache_sample(runtime, input);
            }
            runtime->target_reached_frames[runtime->target_index] =
                route_frame;
            ++runtime->target_index;
            runtime->steer_direction = 0;
        }
        if (runtime->target_index >= runtime->target_count)
            return input_probe_cache_sample(runtime, input);
        target = &runtime->targets[runtime->target_index];
        dx = target->x - position[0];
        dz = target->z - position[2];
        target_length = sqrtf(dx * dx + dz * dz);
        look_length = sqrtf(look[0] * look[0] + look[2] * look[2]);
        if (target_length <= 0.0f || look_length <= 0.001f)
            return input_probe_cache_sample(runtime, input);
        dx /= target_length;
        dz /= target_length;
        dot = (look[0] * dx + look[2] * dz) / look_length;
        cross = (look[0] * dz - look[2] * dx) / look_length;
        angle = atan2f(cross, dot);
        if (angle > M_PI_F) angle -= M_TAU_F;
        if (target->aim_chr >= 0) {
            input.held = target->pulse_period == 0U
                    || route_frame % target->pulse_period == 0U
                ? target->held : 0U;
        }
        if (runtime->steer_direction != 0 && dot < 0.0f) {
            input.move_x = (float)runtime->steer_direction;
        } else if (fabsf(angle) > 0.35f) {
            runtime->steer_direction = angle < 0.0f ? -1 : 1;
            input.move_x = (float)runtime->steer_direction;
        } else if (fabsf(angle) > 0.07f) {
            runtime->steer_direction = angle < 0.0f ? -1 : 1;
            input.move_x = angle < 0.0f ? -0.35f : 0.35f;
        } else {
            runtime->steer_direction = 0;
            input.move_y = 1.0f;
            input.move_x = angle * 0.8f;
        }
        if (route_frame % 30U == 0U
                && runtime->trace_count < RUNTIME_INPUT_PROBE_TRACE_CAPACITY) {
            RuntimeInputProbeTrace *trace =
                &runtime->trace[runtime->trace_count++];
            trace->frame = route_frame;
            trace->target = (uint8_t)runtime->target_index;
            trace->position_x = position[0];
            trace->position_z = position[2];
            trace->look_x = look[0];
            trace->look_z = look[2];
            trace->move_x = input.move_x;
            trace->move_y = input.move_y;
            trace->angle = angle;
        }
        input.pressed = input.held & ~runtime->last_sample_held;
        input_probe_apply_live_aim(&input, player, aim_position);
        runtime->last_sample_held = input.held;
        return input_probe_cache_sample(runtime, input);
    }
    if (runtime->segment_count == 0U) {
        if (route_frame < runtime->active_frames) {
            input = runtime->input;
            input.pressed = route_frame == 0U
                ? input.held : 0U;
        }
        return input_probe_cache_sample(runtime, input);
    }
    for (index = 0U; index < runtime->segment_count; ++index) {
        const RuntimeInputProbeSegment *segment = &runtime->segments[index];
        const uint32_t next_frame = first_frame + segment->frames;
        if (route_frame < next_frame) {
            const uint32_t previous_held = index == 0U
                ? 0U : runtime->segments[index - 1U].input.held;
            input = segment->input;
            input.pressed = route_frame == first_frame
                ? input.held & ~previous_held : 0U;
            return input_probe_cache_sample(runtime, input);
        }
        first_frame = next_frame;
    }
    return input_probe_cache_sample(runtime, input);
}

static const GeStageAssetDescriptor *load_stage_selection(void)
{
    char key[32];
    FILE *stream = fopen(STAGE_SELECTION_PATH, "rb");
    const GeStageAssetDescriptor *descriptor;
    size_t length;

    if (stream == NULL) return ge_stage_asset_dam();
    length = fread(key, 1U, sizeof(key) - 1U, stream);
    fclose(stream);
    key[length] = '\0';
    while (length != 0U && (key[length - 1U] == '\n'
            || key[length - 1U] == '\r' || key[length - 1U] == ' '
            || key[length - 1U] == '\t'))
        key[--length] = '\0';
    descriptor = ge_stage_asset_descriptor_by_key(key);
    return descriptor != NULL ? descriptor : ge_stage_asset_dam();
}

static int32_t stage_level_id(const GeStageAssetDescriptor *descriptor)
{
    return descriptor != NULL ? descriptor->level_id : LEVELID_DAM;
}

static const char *stage_music_asset_path(int32_t level_id)
{
    GeOriginalStageMusic music;
    return ge_original_stage_music_resolve(level_id, &music)
        ? ge_original_music_track_asset_path(music.main_track) : NULL;
}

typedef struct RuntimeOriginalMusicSync {
    uint64_t generation[3];
    int32_t track[3];
    uint16_t volume[3];
} RuntimeOriginalMusicSync;

static bool sync_original_gameplay_music(
    GeOriginalMusicRuntime *runtime,
    GeAssetPack *asset_pack,
    RuntimeOriginalMusicSync *sync)
{
    GeOriginalMusicPortSnapshot snapshot = {0};
    size_t layer;
    if (runtime == NULL || asset_pack == NULL || sync == NULL) return false;
    ge_original_music_port_snapshot(&snapshot);
    for (layer = 0U; layer < 3U; ++layer) {
        const int32_t track = snapshot.layer_track[layer];
        if (sync->generation[layer] != snapshot.layer_generation[layer]
                || sync->track[layer] != track) {
            if (track <= 0) {
                ge_original_music_runtime_stop_layer(runtime,
                    (unsigned)layer);
            } else {
                const char *path = ge_original_music_track_asset_path(track);
                if (path == NULL
                        || !ge_original_music_runtime_set_layer_asset_pack(
                            runtime, asset_pack, (unsigned)layer, path,
                            (int16_t)snapshot.layer_volume[layer])) {
                    printf("Could not switch original music layer %lu to %ld.\n",
                        (unsigned long)layer + 1UL, (long)track);
                    return false;
                }
            }
            sync->generation[layer] = snapshot.layer_generation[layer];
            sync->track[layer] = track;
            sync->volume[layer] = snapshot.layer_volume[layer];
        } else if (sync->volume[layer] != snapshot.layer_volume[layer]) {
            if (!ge_original_music_runtime_set_layer_volume(
                    runtime, (unsigned)layer,
                    (int16_t)snapshot.layer_volume[layer])) return false;
            sync->volume[layer] = snapshot.layer_volume[layer];
        }
    }
    return true;
}

static bool load_visual_probe_tour(
    RuntimeVisualProbeTour *runtime,
    const GeStageAssetDescriptor *stage_assets)
{
    FILE *stream;
    char *data;
    long length;
    size_t bytes_read;
    int path_length;

    if (runtime == NULL || stage_assets == NULL) return false;
    memset(runtime, 0, sizeof(*runtime));
    runtime->current_view = SIZE_MAX;
    runtime->status = GE_VISUAL_PROBE_TOUR_INVALID_ARGUMENT;
    path_length = snprintf(runtime->source_path, sizeof(runtime->source_path),
            "sdmc:/3ds/goldeneye-3ds/%s-visual-tour.geview",
            stage_assets->key);
    if (path_length < 0 || (size_t)path_length >= sizeof(runtime->source_path))
        return false;
    path_length = snprintf(runtime->result_path, sizeof(runtime->result_path),
            "sdmc:/3ds/goldeneye-3ds/%s-visual-tour.result",
            stage_assets->key);
    if (path_length < 0 || (size_t)path_length >= sizeof(runtime->result_path))
        return false;
    path_length = snprintf(runtime->diagnostic_path,
            sizeof(runtime->diagnostic_path),
            "sdmc:/3ds/goldeneye-3ds/%s-visual-tour.diag",
            stage_assets->key);
    if (path_length < 0
            || (size_t)path_length >= sizeof(runtime->diagnostic_path))
        return false;
    stream = fopen(runtime->source_path, "rb");
    if (stream == NULL) return false;
    if (fseek(stream, 0L, SEEK_END) != 0
            || (length = ftell(stream)) <= 0L
            || fseek(stream, 0L, SEEK_SET) != 0) {
        fclose(stream);
        return false;
    }
    data = malloc((size_t)length);
    if (data == NULL) {
        fclose(stream);
        return false;
    }
    bytes_read = fread(data, 1U, (size_t)length, stream);
    fclose(stream);
    if (bytes_read != (size_t)length) {
        free(data);
        return false;
    }
    runtime->status = ge_visual_probe_tour_parse(
        data, bytes_read, runtime->views, VISUAL_PROBE_TOUR_CAPACITY,
        &runtime->tour);
    free(data);
    runtime->enabled = runtime->status == GE_VISUAL_PROBE_TOUR_OK;
    return runtime->enabled;
}
static float dam_cloud_offset;
static Ge3dsSceneTextureSlot dam_scene_texture_slots[
    DAM_SCENE_TEXTURE_CAPACITY];
static Ge3dsSceneTextures dam_scene_textures;
static C3D_FogLut dam_environment_fog_lut;
static Ge3dsSceneTextureSlot first_person_texture_slots[
    FIRST_PERSON_TEXTURE_CAPACITY];
static Ge3dsSceneTextures first_person_scene_textures;
static uint16_t guard_muzzle_flash_images[GUARD_MUZZLE_FLASH_CAPACITY];
static C3D_Tex gun_sight_texture;
static Tex3DS_SubTexture gun_sight_subtexture;
static bool gun_sight_texture_loaded;
static uint64_t gun_sight_frames, gun_sight_visible_frames, gun_sight_failures;
static uint32_t gun_sight_suppression;
static const Vertex dam_environment_vertices[DAM_ENVIRONMENT_VERTEX_COUNT] = {
    {40.0f, 0.0f, 0.5f, 0.0f, 0.0f,
     16.0f / 255.0f, 48.0f / 255.0f, 96.0f / 255.0f, 1.0f},
    {360.0f, 0.0f, 0.5f, 0.0f, 0.0f,
     16.0f / 255.0f, 48.0f / 255.0f, 96.0f / 255.0f, 1.0f},
    {360.0f, 240.0f, 0.5f, 0.0f, 0.0f,
     16.0f / 255.0f, 48.0f / 255.0f, 96.0f / 255.0f, 1.0f},
    {40.0f, 0.0f, 0.5f, 0.0f, 0.0f,
     16.0f / 255.0f, 48.0f / 255.0f, 96.0f / 255.0f, 1.0f},
    {360.0f, 240.0f, 0.5f, 0.0f, 0.0f,
     16.0f / 255.0f, 48.0f / 255.0f, 96.0f / 255.0f, 1.0f},
    {40.0f, 240.0f, 0.5f, 0.0f, 0.0f,
     16.0f / 255.0f, 48.0f / 255.0f, 96.0f / 255.0f, 1.0f},
};
static int16_t audio_ring_storage[AUDIO_RING_FRAMES * 2U];

typedef struct RuntimeGbiMesh {
    bool loaded;
    bool textured;
    size_t commands;
    size_t draws;
    size_t triangles;
    size_t vertex_count;
    Vertex vertices[RAREWARE_VERTEX_CAPACITY];
} RuntimeGbiMesh;

typedef struct RuntimeModelVertex {
    float x;
    float y;
    float z;
    float normal_x;
    float normal_y;
    float normal_z;
} RuntimeModelVertex;

typedef struct RuntimeGbiModel {
    bool loaded;
    bool material_ready;
    size_t commands;
    size_t draws;
    size_t triangles;
    size_t vertex_count;
    GePicaMaterial material;
    RuntimeModelVertex vertices[RAREWARE_BODY_VERTEX_CAPACITY];
} RuntimeGbiModel;

typedef GeDamCameraBatch RuntimeDamRenderBatch;

typedef struct RuntimeDamPreview {
    const GeStageAssetDescriptor *stage_assets;
    GeOriginalStageEnvironment environment;
    bool environment_ready;
    bool loaded;
    bool setup_loaded;
    int32_t spawn_pad;
    float spawn_position[3];
    const char *spawn_plink;
    size_t rooms;
    size_t lists;
    size_t commands;
    size_t draws;
    size_t triangles;
    size_t vertex_count;
    size_t source_vertex_count;
    size_t batch_count;
    size_t material_groups;
    GeDamRoomWorldVertex *source_vertices;
    GeDamRoomDrawBatch *batches;
    RuntimeDamRenderBatch *render_batches;
    size_t render_batch_count;
    GeDrawBatchWorldBounds *gpu_batch_bounds;
    size_t gpu_batch_bounds_capacity;
    float spawn_screen_x;
    float spawn_screen_y;
    bool original_camera_ready;
    bool gpu_world_ready;
    C3D_Mtx gpu_world_projection;
    C3D_Mtx gpu_runtime_projection;
    C3D_Mtx gpu_first_person_projection;
    float authored_world_to_clip[4][4];
    float runtime_world_to_clip[4][4];
    float eye_to_clip[4][4];
    uint64_t gpu_uploaded_scene_generation;
    size_t gpu_uploaded_vertex_count;
    size_t gpu_dirty_vertex_offset;
    size_t gpu_dirty_vertex_count;
    float original_camera_view[4][4];
    float original_camera_view_to_world[4][4];
    float original_camera_projection[4][4];
    float original_camera_position[3];
    float original_camera_look[3];
    float original_camera_up[3];
    int16_t original_camera_viewport_scale[4];
    int16_t original_camera_viewport_translation[4];
    GeOriginalBondCameraStatus original_camera_status;
    uint32_t original_camera_matrices;
    uint32_t original_camera_lights;
    uint8_t original_camera_room;
    GeDamCameraStatus camera_handoff_status;
    size_t camera_input_triangles;
    size_t camera_visible_triangles;
    size_t camera_output_triangles;
    GeDamWorld world;
    GeDamWorldStatus world_status;
    GeDamVisibilityRuntime visibility_runtime;
    GeDamVisibilityRuntimeStatus visibility_status;
    GeOriginalBgVisibilityResult visibility_result;
    bool visibility_ready;
    uint8_t world_room_ids[DAM_WORLD_ROOM_LOAD_CAPACITY];
    size_t world_room_count;
    GeDamPreloadQueue preload_queue;
    GeDamPreloadStatus preload_status;
    GeOriginalBgVisibilityProviders visibility_providers;
    uint8_t portal_controls[GE_DAM_WORLD_MAX_PORTALS];
    GeDamDynamicScene dynamic_scene;
    GeDamDynamicSceneStatus dynamic_scene_status;
    Ge3dsSceneTextureStatus stream_texture_status;
    GeDamCameraStatus stream_camera_status;
    uint8_t stream_allocation_failed;
    uint32_t diagnostic_stream_phase;
    uint8_t diagnostic_transaction_room;
    GeTextureCache *texture_cache;
    const Ge3dsSceneTextures *scene_textures;
} RuntimeDamPreview;

static bool write_visual_probe_tour_result(
    const RuntimeVisualProbeTour *runtime,
    const RuntimeDamPreview *preview)
{
    FILE *stream;

    if (runtime == NULL || preview == NULL) return false;
    if (runtime->result_path[0] == '\0') return false;
    stream = fopen(runtime->result_path, "wb");
    if (stream == NULL) return false;
    fprintf(stream, "GE_VISUAL_PROBE_RESULT 2\n");
    fprintf(stream, "status=%s\n",
            runtime->camera_failure_views == 0U
                    && runtime->visibility_failure_views == 0U
                    && preview->dynamic_scene.install_failures == 0U
                ? "complete" : "failed");
    fprintf(stream, "views=%lu\n", (unsigned long)runtime->tour.count);
    fprintf(stream, "resident_peak=%lu\n",
            (unsigned long)runtime->peak_resident_rooms);
    fprintf(stream, "texture_peak=%lu\n",
            (unsigned long)runtime->peak_scene_textures);
    fprintf(stream, "visible_room_peak=%lu\n",
            (unsigned long)runtime->peak_visible_rooms);
    fprintf(stream, "camera_failure_views=%lu\n",
            (unsigned long)runtime->camera_failure_views);
    fprintf(stream, "visibility_failure_views=%lu\n",
            (unsigned long)runtime->visibility_failure_views);
    fprintf(stream, "stream_generation=%llu\n",
            (unsigned long long)preview->dynamic_scene.generation);
    fprintf(stream, "stream_successes=%llu\n",
            (unsigned long long)preview->dynamic_scene.install_successes);
    fprintf(stream, "stream_failures=%llu\n",
            (unsigned long long)preview->dynamic_scene.install_failures);
    fprintf(stream, "camera_status=%u\n",
            (unsigned int)preview->camera_handoff_status);
    fprintf(stream, "visibility_status=%u\n",
            (unsigned int)preview->visibility_status);
    fprintf(stream, "original_visibility_status=%u\n",
            (unsigned int)preview->visibility_runtime.last_original_status);
    fprintf(stream, "visibility_rooms=%lu\n",
            (unsigned long)preview->visibility_result.room_count);
    fprintf(stream, "world_portals=%lu\n",
            (unsigned long)preview->world.portal_count);
    fprintf(stream, "visibility_preloads=%lu\n",
            (unsigned long)preview->visibility_result.preload_request_count);
    fprintf(stream, "native_actor_tick_status=%u\n",
            (unsigned int)runtime->native_actor_tick_status);
    fprintf(stream, "native_actor_service_status=%u\n",
            (unsigned int)runtime->native_actor_service_status);
    fprintf(stream, "native_actor_prop_count=%lu\n",
            (unsigned long)runtime->native_actor_prop_count);
    fprintf(stream, "native_actor_materializer_ready_count=%lu\n",
            (unsigned long)runtime->native_actor_materializer_ready_count);
    fprintf(stream, "native_actor_materializer_constructed_count=%lu\n",
            (unsigned long)
                runtime->native_actor_materializer_constructed_count);
    fprintf(stream, "native_actor_materializer_failed_count=%lu\n",
            (unsigned long)runtime->native_actor_materializer_failed_count);
    fprintf(stream, "native_actor_materialized_live_count=%lu\n",
            (unsigned long)runtime->native_actor_materialized_live_count);
    fprintf(stream, "native_owned_ordinary_embedded_count=%lu\n",
            (unsigned long)runtime->native_owned_ordinary_embedded_count);
    fprintf(stream, "native_owned_ordinary_assigned_count=%lu\n",
            (unsigned long)runtime->native_owned_ordinary_assigned_count);
    fprintf(stream, "native_owned_ordinary_pending_count=%lu\n",
            (unsigned long)runtime->native_owned_ordinary_pending_count);
    fprintf(stream, "native_actor_first_failed_command=%lu\n",
            (unsigned long)runtime->native_actor_first_failed_command);
    fprintf(stream, "native_actor_first_failed_type=%u\n",
            (unsigned int)runtime->native_actor_first_failed_type);
    fprintf(stream, "native_actor_first_failed_construct_status=%u\n",
            (unsigned int)runtime->native_actor_first_failed_construct_status);
    fprintf(stream, "native_actor_first_failed_placement_status=%u\n",
            (unsigned int)runtime->native_actor_first_failed_placement_status);
    fprintf(stream, "native_actor_authored_weapon_count=%lu\n",
            (unsigned long)runtime->native_actor_authored_weapon_count);
    fprintf(stream, "native_actor_attached_weapon_count=%lu\n",
            (unsigned long)runtime->native_actor_attached_weapon_count);
    fprintf(stream, "native_actor_attached_hat_count=%lu\n",
            (unsigned long)runtime->native_actor_attached_hat_count);
    fprintf(stream, "native_actor_guard_overlay_updates=%llu\n",
            (unsigned long long)runtime->native_actor_guard_overlay_updates);
    fprintf(stream, "native_actor_door_overlay_updates=%llu\n",
            (unsigned long long)runtime->native_actor_door_overlay_updates);
    fprintf(stream, "native_actor_overlay_full_rebuilds=%llu\n",
            (unsigned long long)runtime->native_actor_overlay_full_rebuilds);
    fprintf(stream, "native_actor_guard_cache_builds=%llu\n",
            (unsigned long long)runtime->native_actor_guard_cache_builds);
    fprintf(stream, "native_actor_guard_cache_topology_rebuilds=%llu\n",
            (unsigned long long)
                runtime->native_actor_guard_cache_topology_rebuilds);
    fprintf(stream, "native_actor_door_cache_builds=%llu\n",
            (unsigned long long)runtime->native_actor_door_cache_builds);
    fprintf(stream, "native_actor_door_cache_topology_rebuilds=%llu\n",
            (unsigned long long)
                runtime->native_actor_door_cache_topology_rebuilds);
    fprintf(stream, "native_actor_tick_count=%llu\n",
            (unsigned long long)runtime->native_actor_tick_count);
    fprintf(stream, "native_mission_actor_count=%lu\n",
            (unsigned long)runtime->native_mission_actor_count);
    fprintf(stream, "native_mission_tick_count=%llu\n",
            (unsigned long long)runtime->native_mission_tick_count);
    fprintf(stream, "native_mission_ai_offset_hash=%llu\n",
            (unsigned long long)runtime->native_mission_ai_offset_hash);
    fprintf(stream, "native_monitor_count=%lu\n",
            (unsigned long)runtime->native_monitor_count);
    fprintf(stream, "native_monitor_screen_count=%lu\n",
            (unsigned long)runtime->native_monitor_screen_count);
    fprintf(stream, "native_monitor_tick_count=%llu\n",
            (unsigned long long)runtime->native_monitor_tick_count);
    fprintf(stream, "native_monitor_noop_tick_count=%llu\n",
            (unsigned long long)runtime->native_monitor_noop_tick_count);
    fprintf(stream, "native_monitor_surface_update_count=%llu\n",
            (unsigned long long)runtime->native_monitor_surface_update_count);
    fprintf(stream, "native_monitor_surface_unchanged_count=%llu\n",
            (unsigned long long)
                runtime->native_monitor_surface_unchanged_count);
    fprintf(stream, "native_monitor_surface_failure_count=%llu\n",
            (unsigned long long)runtime->native_monitor_surface_failure_count);
    fprintf(stream, "native_articulated_scene_update_count=%llu\n",
            (unsigned long long)runtime->native_articulated_scene_update_count);
    fprintf(stream, "native_articulated_scene_unchanged_count=%llu\n",
            (unsigned long long)
                runtime->native_articulated_scene_unchanged_count);
    fprintf(stream, "native_articulated_scene_topology_change_count=%llu\n",
            (unsigned long long)
                runtime->native_articulated_scene_topology_change_count);
    fprintf(stream, "native_articulated_scene_failure_count=%llu\n",
            (unsigned long long)runtime->native_articulated_scene_failure_count);
    fprintf(stream, "native_supply_count=%lu\n",
            (unsigned long)runtime->native_supply_count);
    fprintf(stream, "native_supply_slot_model_load_count=%lu\n",
            (unsigned long)runtime->native_supply_slot_model_load_count);
    fprintf(stream, "native_tinted_glass_count=%lu\n",
            (unsigned long)runtime->native_tinted_glass_count);
    fprintf(stream, "native_cctv_count=%lu\n",
            (unsigned long)runtime->native_cctv_count);
    fprintf(stream, "native_autogun_count=%lu\n",
            (unsigned long)runtime->native_autogun_count);
    fprintf(stream, "native_gas_releasing_count=%lu\n",
            (unsigned long)runtime->native_gas_releasing_count);
    fprintf(stream, "native_safe_count=%lu\n",
            (unsigned long)runtime->native_safe_count);
    fprintf(stream, "native_safe_relation_count=%lu\n",
            (unsigned long)runtime->native_safe_relation_count);
    fprintf(stream, "native_safe_status=%u\n",
            (unsigned int)runtime->native_safe_status);
    fprintf(stream, "native_safe_relation_status=%u\n",
            (unsigned int)runtime->native_safe_relation_status);
    fprintf(stream, "native_stage_init_mask=%u\n",
            (unsigned int)runtime->native_stage_init_mask);
    fprintf(stream, "native_door_interaction_tick_count=%llu\n",
            (unsigned long long)runtime->native_door_interaction_tick_count);
    fprintf(stream, "native_door_interaction_activation_count=%u\n",
            (unsigned int)runtime->native_door_interaction_activation_count);
    fprintf(stream, "native_objective_count=%lu\n",
            (unsigned long)runtime->native_objective_count);
    fprintf(stream, "native_objective_criterion_count=%lu\n",
            (unsigned long)runtime->native_objective_criterion_count);
    fprintf(stream, "native_objective_blocked_tag_count=%lu\n",
            (unsigned long)runtime->native_objective_blocked_tag_count);
    fprintf(stream, "native_objective_evaluation_ready_count=%lu\n",
            (unsigned long)runtime->native_objective_evaluation_ready_count);
    fprintf(stream, "native_objective_evaluation_blocked_count=%lu\n",
            (unsigned long)runtime->native_objective_evaluation_blocked_count);
    fprintf(stream, "native_objective_evaluation_ticks=%llu\n",
            (unsigned long long)runtime->native_objective_evaluation_ticks);
    fprintf(stream, "native_dam_guard_tick_count=%llu\n",
            (unsigned long long)runtime->native_dam_guard_tick_count);
    fprintf(stream, "native_dam_guard_rejected_tick_count=%llu\n",
            (unsigned long long)runtime->native_dam_guard_rejected_tick_count);
    fprintf(stream, "native_dam_guard_last_status=%u\n",
            (unsigned int)runtime->native_dam_guard_last_status);
    fprintf(stream, "native_dam_guard_weapon_fire_count=%llu\n",
            (unsigned long long)runtime->native_dam_guard_weapon_fire_count);
    fprintf(stream, "native_dam_guard_player_damage_count=%llu\n",
            (unsigned long long)runtime->native_dam_guard_player_damage_count);
    fprintf(stream, "native_dam_door_interaction_tick_count=%llu\n",
            (unsigned long long)
                runtime->native_dam_door_interaction_tick_count);
    fprintf(stream, "native_dam_door_interaction_activation_count=%u\n",
            (unsigned int)
                runtime->native_dam_door_interaction_activation_count);
    fprintf(stream, "native_dam_alarm_count=%lu\n",
            (unsigned long)runtime->native_dam_alarm_count);
    fprintf(stream, "native_dam_alarm_model_status=%u\n",
            (unsigned int)runtime->native_dam_alarm_model_status);
    fprintf(stream, "native_dam_alarm_misc_status=%u\n",
            (unsigned int)runtime->native_dam_alarm_misc_status);
    fprintf(stream, "native_dam_alarm_construct_status=%u\n",
            (unsigned int)runtime->native_dam_alarm_construct_status);
    fprintf(stream, "native_dam_alarm_placement_status=%u\n",
            (unsigned int)runtime->native_dam_alarm_placement_status);
    fprintf(stream, "native_dam_alarm_instance_bits=%u\n",
            (unsigned int)runtime->native_dam_alarm_instance_bits);
    fprintf(stream, "native_dam_alarm_scan_count=%lu\n",
            (unsigned long)runtime->native_dam_alarm_scan_count);
    fprintf(stream, "native_dam_alarm_materialize_failure=%u\n",
            (unsigned int)runtime->native_dam_alarm_materialize_failure);
    fprintf(stream, "native_dam_alarm_scene_part_count=%lu\n",
            (unsigned long)runtime->native_dam_alarm_scene_part_count);
    fprintf(stream, "native_dam_model_scene_status=%u\n",
            (unsigned int)runtime->native_dam_model_scene_status);
    fprintf(stream, "native_dam_model_scene_ready=%u\n",
            (unsigned int)runtime->native_dam_model_scene_ready);
    fprintf(stream, "native_dam_scene_prerequisite_bits=%u\n",
            (unsigned int)runtime->native_dam_scene_prerequisite_bits);
    fprintf(stream, "native_dam_scene_install_failure=%u\n",
            (unsigned int)runtime->native_dam_scene_install_failure);
    fprintf(stream, "native_dam_alarm_status=%u\n",
            (unsigned int)runtime->native_dam_alarm_status);
    fprintf(stream, "native_dam_alarm_interaction_tick_count=%llu\n",
            (unsigned long long)
                runtime->native_dam_alarm_interaction_tick_count);
    fprintf(stream, "native_dam_alarm_interaction_activation_count=%llu\n",
            (unsigned long long)
                runtime->native_dam_alarm_interaction_activation_count);
    fprintf(stream, "native_dam_objective_count=%lu\n",
            (unsigned long)runtime->native_dam_objective_count);
    fprintf(stream, "native_dam_objective_blocked_tag_count=%lu\n",
            (unsigned long)runtime->native_dam_objective_blocked_tag_count);
    fprintf(stream, "native_dam_objective_evaluation_ready_count=%lu\n",
            (unsigned long)
                runtime->native_dam_objective_evaluation_ready_count);
    fprintf(stream, "native_dam_objective_evaluation_blocked_count=%lu\n",
            (unsigned long)
                runtime->native_dam_objective_evaluation_blocked_count);
    fprintf(stream, "native_dam_objective_evaluation_ticks=%llu\n",
            (unsigned long long)
                runtime->native_dam_objective_evaluation_ticks);
    fprintf(stream, "native_dam_objective_hud_message_count=%lu\n",
            (unsigned long)runtime->native_dam_objective_hud_message_count);
    fprintf(stream, "native_dam_guard_overlay_updates=%llu\n",
            (unsigned long long)runtime->native_dam_guard_overlay_updates);
    fprintf(stream, "native_dam_door_overlay_updates=%llu\n",
            (unsigned long long)runtime->native_dam_door_overlay_updates);
    fprintf(stream, "native_dam_overlay_full_rebuilds=%llu\n",
            (unsigned long long)runtime->native_dam_overlay_full_rebuilds);
    fprintf(stream, "native_dam_mission_tick_count=%u\n",
            (unsigned int)runtime->native_dam_mission_tick_count);
    fprintf(stream, "native_dam_mission_ai_offset=%u\n",
            (unsigned int)runtime->native_dam_mission_ai_offset);
    fprintf(stream, "native_dam_mission_exit_ai_offset=%u\n",
            (unsigned int)runtime->native_dam_mission_exit_ai_offset);
    fprintf(stream, "native_dam_mission_objective_registers=%u\n",
            (unsigned int)runtime->native_dam_mission_objective_registers);
    fprintf(stream, "native_dam_mission_hud_message_count=%u\n",
            (unsigned int)runtime->native_dam_mission_hud_message_count);
    fprintf(stream, "native_dam_full_props_activated=%u\n",
            (unsigned int)runtime->native_dam_full_props_activated);
    fprintf(stream, "displayed_frame_count=%llu\n",
            (unsigned long long)runtime->displayed_frame_count);
    fprintf(stream, "displayed_frame_average_ms=%llu\n",
            (unsigned long long)(runtime->displayed_frame_count != 0U
                ? runtime->displayed_frame_total_ms
                    / runtime->displayed_frame_count
                : 0U));
    fprintf(stream, "displayed_frame_peak_ms=%llu\n",
            (unsigned long long)runtime->displayed_frame_peak_ms);
    fprintf(stream, "simulation_average_ms=%llu\n",
            (unsigned long long)(runtime->displayed_frame_count != 0U
                ? runtime->simulation_total_ms / runtime->displayed_frame_count
                : 0U));
    fprintf(stream, "gpu_average_ms=%llu\n",
            (unsigned long long)(runtime->displayed_frame_count != 0U
                ? runtime->gpu_total_ms / runtime->displayed_frame_count
                : 0U));
    return fclose(stream) == 0;
}

static bool write_visual_probe_tour_diagnostic(
    const RuntimeVisualProbeTour *runtime, const RuntimeDamPreview *preview)
{
    const GeVisualProbeView *view;
    FILE *stream;
    uint8_t next_room = UINT8_MAX;
    const bool has_next = runtime != NULL && preview != NULL
        && ge_dam_preload_queue_peek(
            &preview->preload_queue, &next_room) == GE_DAM_PRELOAD_OK;
    if (runtime == NULL || preview == NULL
            || runtime->diagnostic_path[0] == '\0'
            || runtime->current_view >= runtime->tour.count) return false;
    view = &runtime->tour.views[runtime->current_view];
    stream = fopen(runtime->diagnostic_path, "wb");
    if (stream == NULL) return false;
    fprintf(stream, "GE_VISUAL_PROBE_DIAGNOSTIC 1\n");
    fprintf(stream, "view=%lu\n", (unsigned long)runtime->current_view);
    fprintf(stream, "label=%s\n", view->label);
    fprintf(stream, "target_room=%u\n", (unsigned int)view->room);
    fprintf(stream, "attempts=%llu\n",
            (unsigned long long)runtime->diagnostic_attempts);
    fprintf(stream, "current_view_ready=%u\n",
            runtime->current_view_ready ? 1U : 0U);
    fprintf(stream, "target_room_state=%u\n", (unsigned int)
        ge_dam_preload_queue_room_state(
            &preview->preload_queue, view->room));
    fprintf(stream, "pending_count=%lu\n",
            (unsigned long)preview->preload_queue.pending_count);
    fprintf(stream, "loading_count=%lu\n",
            (unsigned long)preview->preload_queue.loading_count);
    fprintf(stream, "read_index=%lu\n",
            (unsigned long)preview->preload_queue.read_index);
    fprintf(stream, "write_index=%lu\n",
            (unsigned long)preview->preload_queue.write_index);
    fprintf(stream, "next_room=%d\n", has_next ? (int)next_room : -1);
    fprintf(stream, "next_room_state=%u\n", has_next ? (unsigned int)
        ge_dam_preload_queue_room_state(
            &preview->preload_queue, next_room) : UINT_MAX);
    fprintf(stream, "camera_updated=%u\n",
            (unsigned int)runtime->diagnostic_camera_updated);
    fprintf(stream, "camera_status=%u\n",
            (unsigned int)preview->camera_handoff_status);
    fprintf(stream, "stream_camera_status=%u\n",
            (unsigned int)preview->stream_camera_status);
    fprintf(stream, "stream_texture_status=%u\n",
            (unsigned int)preview->stream_texture_status);
    fprintf(stream, "stream_allocation_failed=%u\n",
            (unsigned int)preview->stream_allocation_failed);
    fprintf(stream, "stream_phase=%u\n",
            (unsigned int)preview->diagnostic_stream_phase);
    fprintf(stream, "transaction_room=%u\n",
            (unsigned int)preview->diagnostic_transaction_room);
    fprintf(stream, "visibility_ready=%u\n",
            preview->visibility_ready ? 1U : 0U);
    fprintf(stream, "visibility_status=%u\n",
            (unsigned int)preview->visibility_status);
    fprintf(stream, "dynamic_status=%u\n",
            (unsigned int)preview->dynamic_scene_status);
    fprintf(stream, "dynamic_generation=%llu\n",
            (unsigned long long)preview->dynamic_scene.generation);
    fprintf(stream, "dynamic_install_successes=%llu\n",
            (unsigned long long)preview->dynamic_scene.install_successes);
    fprintf(stream, "dynamic_install_failures=%llu\n",
            (unsigned long long)preview->dynamic_scene.install_failures);
    fprintf(stream, "dynamic_eviction_successes=%llu\n",
            (unsigned long long)preview->dynamic_scene.eviction_successes);
    fprintf(stream, "dynamic_room_count=%lu\n",
            (unsigned long)preview->dynamic_scene.room_count);
    fprintf(stream, "ordinary_refresh_attempted=%u\n",
            (unsigned int)runtime->diagnostic_ordinary_refresh_attempted);
    fprintf(stream, "ordinary_refresh_succeeded=%u\n",
            (unsigned int)runtime->diagnostic_ordinary_refresh_succeeded);
    fprintf(stream, "ordinary_scene_ready=%u\n",
            (unsigned int)runtime->diagnostic_ordinary_scene_ready);
    fprintf(stream, "ordinary_overlay_status=%u\n",
            (unsigned int)runtime->diagnostic_ordinary_overlay_status);
    fprintf(stream, "ordinary_resident_install_successes=%llu\n",
            (unsigned long long)
                runtime->diagnostic_ordinary_resident_install_successes);
    fprintf(stream, "ordinary_resident_eviction_successes=%llu\n",
            (unsigned long long)
                runtime->diagnostic_ordinary_resident_eviction_successes);
    fprintf(stream, "ordinary_overlay_full_rebuilds=%llu\n",
            (unsigned long long)runtime->diagnostic_overlay_full_rebuilds);
    fprintf(stream, "door_overlay_failures=%llu\n",
            (unsigned long long)runtime->diagnostic_door_overlay_failures);
    fprintf(stream, "guard_overlay_failures=%llu\n",
            (unsigned long long)runtime->diagnostic_guard_overlay_failures);
    fprintf(stream, "monitor_overlay_failures=%llu\n",
            (unsigned long long)runtime->diagnostic_monitor_overlay_failures);
    fprintf(stream, "articulated_failures=%llu\n",
            (unsigned long long)runtime->diagnostic_articulated_failures);
    return fclose(stream) == 0;
}

static void visual_probe_record_stream_phase(
    RuntimeDamPreview *preview, uint32_t phase, uint8_t room)
{
    FILE *stream;

    if (preview == NULL || !visual_probe_tour.enabled) return;
    preview->diagnostic_stream_phase = phase;
    preview->diagnostic_transaction_room = room;
    /* This marker must remain completion-independent.  In particular, do
     * not call the full diagnostic serializer here: it inspects the preload
     * queue and ordinary overlay state that this camera transaction may be
     * in the middle of mutating. */
    stream = fopen(visual_probe_tour.diagnostic_path, "wb");
    if (stream == NULL) return;
    fprintf(stream,
        "GE_VISUAL_PROBE_STREAM_PHASE 1\nphase=%u\nroom=%u\n",
        (unsigned int)phase, (unsigned int)room);
    (void)fclose(stream);
}

static void visual_probe_record_camera(
    RuntimeVisualProbeTour *runtime, const RuntimeDamPreview *preview,
    bool camera_updated)
{
    if (runtime == NULL || preview == NULL || !runtime->enabled) return;
    if (!camera_updated && !runtime->current_view_camera_failed) {
        runtime->current_view_camera_failed = true;
        ++runtime->camera_failure_views;
    }
    if (camera_updated && !preview->visibility_ready
            && !runtime->current_view_visibility_failed) {
        runtime->current_view_visibility_failed = true;
        ++runtime->visibility_failure_views;
    }
    if (preview->visibility_ready
            && preview->visibility_result.room_count
                > runtime->peak_visible_rooms) {
        runtime->peak_visible_rooms = preview->visibility_result.room_count;
    }
}

typedef struct RuntimeDamCollision {
    bool loaded;
    bool original_bound;
    bool original_spawn_matched;
    bool original_spawn_in_bounds;
    bool original_spawn_radius_clear;
    float original_spawn_floor_y;
    uint8_t *blob;
    size_t blob_size;
    uint8_t *native_storage;
    size_t native_size;
    GeStanCollisionSurface surface;
    GeStanNativeMap native;
} RuntimeDamCollision;

typedef struct RuntimeDamIntro {
    GeOriginalSetupPadState setup;
    GeOriginalIntroSpawnState spawn;
    GeOriginalPlayerViewState player;
    GeOriginalBondMovementStatus movement;
} RuntimeDamIntro;

typedef struct RuntimeOriginalSfxBank {
    uint8_t *control;
    uint8_t *samples;
    GeOriginalSfxBank bank;
    GeOriginalSfxBankStatus status;
    bool loaded;
} RuntimeOriginalSfxBank;

static bool load_original_sfx_bank(GeAssetPack *asset_pack,
                                   RuntimeOriginalSfxBank *runtime)
{
    static const char control_path[] = "music/sfx.ctl";
    static const char samples_path[] = "music/sfx.tbl";
    const GeAssetPackEntry *control_entry;
    const GeAssetPackEntry *samples_entry;
    if (asset_pack == NULL || runtime == NULL) return false;
    memset(runtime, 0, sizeof(*runtime));
    control_entry = ge_asset_pack_find(asset_pack, control_path);
    samples_entry = ge_asset_pack_find(asset_pack, samples_path);
    if (control_entry == NULL || samples_entry == NULL
            || control_entry->data_size > SIZE_MAX
            || samples_entry->data_size > SIZE_MAX) return false;
    runtime->control = malloc((size_t)control_entry->data_size);
    runtime->samples = malloc((size_t)samples_entry->data_size);
    if (runtime->control == NULL || runtime->samples == NULL
            || ge_asset_pack_read(asset_pack, control_path,
                                  runtime->control,
                                  (size_t)control_entry->data_size,
                                  NULL) != GE_ASSET_PACK_OK
            || ge_asset_pack_read(asset_pack, samples_path,
                                  runtime->samples,
                                  (size_t)samples_entry->data_size,
                                  NULL) != GE_ASSET_PACK_OK) {
        free(runtime->control);
        free(runtime->samples);
        memset(runtime, 0, sizeof(*runtime));
        return false;
    }
    runtime->status = ge_original_sfx_bank_init(
        &runtime->bank,
        runtime->control, (size_t)control_entry->data_size,
        runtime->samples, (size_t)samples_entry->data_size);
    runtime->loaded = runtime->status == GE_ORIGINAL_SFX_BANK_OK;
    if (!runtime->loaded) {
        free(runtime->control);
        free(runtime->samples);
        runtime->control = NULL;
        runtime->samples = NULL;
    }
    return runtime->loaded;
}

static void close_original_sfx_bank(RuntimeOriginalSfxBank *runtime)
{
    if (runtime == NULL) return;
    free(runtime->control);
    free(runtime->samples);
    memset(runtime, 0, sizeof(*runtime));
}

#define DAM_BASE_OBJECT_COUNT 4U
#define DAM_MATERIALIZED_OBJECT_CAPACITY \
    (DAM_BASE_OBJECT_COUNT + GE_ORIGINAL_DAM_SPAWN_WINDOW_COUNT)
#define DAM_MISSION_TAG_OBJECT_COUNT 2U
#define DAM_ALARM_OBJECT_COUNT 4U
#define DAM_NATIVE_OBJECT_CAPACITY \
    (DAM_MATERIALIZED_OBJECT_CAPACITY + DAM_MISSION_TAG_OBJECT_COUNT \
        + DAM_ALARM_OBJECT_COUNT)
#define DAM_FIXED_MODEL_SCENE_INPUT_COUNT \
    (DAM_MATERIALIZED_OBJECT_CAPACITY + 3U)
#define DAM_ALARM_SCENE_PART_CAPACITY 8U
#define DAM_MODEL_SCENE_INPUT_CAPACITY \
    (DAM_FIXED_MODEL_SCENE_INPUT_COUNT + DAM_ALARM_SCENE_PART_CAPACITY)
#define DAM_WINDOW_MODEL_INSTANCE_COUNT \
    (1U + GE_ORIGINAL_DAM_SPAWN_WINDOW_COUNT)
#define DAM_GREATGUARD2_ASSET_PATH \
    "converted/models/greatguard2/model.bin"
#define DAM_CHRKALASH_ASSET_PATH \
    "converted/models/chrkalash/model.bin"

typedef struct RuntimeDamOverlaySegment {
    GeDamRoomWorldVertex *vertices;
    GeDamRoomDrawBatch *batches;
    size_t vertex_offset;
    size_t vertex_count;
    size_t batch_offset;
    size_t batch_count;
} RuntimeDamOverlaySegment;

typedef struct RuntimeDamAlarmObject {
    void *definition;
    void *prop;
    GeOriginalDefaultObjectPrepared prepared;
    GeOriginalStageMiscInstance instance;
    GeOriginalStageMiscStatus status;
    GeOriginalDefaultObjectStatus construct_status;
    GeOriginalDefaultObjectStatus placement_status;
    size_t command_index;
    int32_t model_id;
    uint8_t room;
    bool live;
} RuntimeDamAlarmObject;

typedef struct RuntimeDamWorldObjects {
    GeOriginalDamWorldState state;
    GeOriginalDamMissionTagState mission_tags;
    GeOriginalPropState props;
    const GeOriginalStageSetupRuntime *setup;
    void *definitions[DAM_NATIVE_OBJECT_CAPACITY];
    size_t definition_count;
    GeOriginalPitemModelProvider *pitem_models;
    GeOriginalPitemModelStatus pitem_model_status;
    GeOriginalDefaultObjectProviders object_providers;
    uint8_t model62_blob[GE_ORIGINAL_MODEL62_BLOB_SIZE];
    GeOriginalModel62 *model62;
    GeOriginalModel62Status model62_status;
    uint8_t model104_blob[GE_ORIGINAL_MODEL104_BLOB_SIZE];
    GeOriginalModel104 *model104[DAM_WINDOW_MODEL_INSTANCE_COUNT];
    GeOriginalModel104Status model104_status;
    uint32_t model104_resolve_count;
    uint8_t model178_blob[GE_ORIGINAL_MODEL178_BLOB_SIZE];
    GeOriginalModel178 *model178[2];
    GeOriginalModel178Status model178_status;
    uint32_t model178_resolve_count;
    uint8_t modembox_blob[GE_ORIGINAL_MODEMBOX_BLOB_SIZE];
    uint8_t satdish_blob[GE_ORIGINAL_SATDISH_BLOB_SIZE];
    GeOriginalDamObjectiveModels *objective_models;
    GeOriginalDamObjectiveModelsStatus objective_models_status;
    GeOriginalDefaultObjectPrepared glass_object;
    GeOriginalDefaultObjectStatus glass_object_status;
    GeOriginalDefaultObjectStatus glass_placement_status;
    GeOriginalDefaultObjectPrepared spawn_windows[
        GE_ORIGINAL_DAM_SPAWN_WINDOW_COUNT];
    GeOriginalDefaultObjectStatus spawn_window_status[
        GE_ORIGINAL_DAM_SPAWN_WINDOW_COUNT];
    GeOriginalDefaultObjectStatus spawn_window_placement_status[
        GE_ORIGINAL_DAM_SPAWN_WINDOW_COUNT];
    GeOriginalDefaultObjectPrepared default_object;
    GeOriginalDefaultObjectStatus default_object_status;
    GeOriginalDefaultObjectStatus placement_status;
    GeOriginalDefaultObjectPrepared mission_objects[
        DAM_MISSION_TAG_OBJECT_COUNT];
    GeOriginalDefaultObjectStatus mission_object_status[
        DAM_MISSION_TAG_OBJECT_COUNT];
    GeOriginalDefaultObjectStatus mission_placement_status[
        DAM_MISSION_TAG_OBJECT_COUNT];
    GeOriginalDoorPrepared doors[2];
    GeOriginalDoorPrepared door_scratch;
    GeOriginalDoorStatus door_status[2];
    GeOriginalDoorRuntimeState door_runtime;
    GeOriginalDoorCollisionState door_collision;
    GeOriginalDoorInteractionState door_interaction;
    GeOriginalDoorScenePublication door_scenes[2];
    uint32_t installed_door_scene_generation[2];
    bool doors_linked;
    GeOriginalDamGuardStatus guard_status;
    GeOriginalDamGuardRuntimeStats guard_runtime;
    bool full_props_activated;
    uint8_t guard_model_blob[GE_ORIGINAL_DAM_GUARD_MODEL_BLOB_SIZE];
    bool guard_model_loaded;
    uint8_t guard_weapon_model_blob[
        GE_ORIGINAL_DAM_GUARD_WEAPON_MODEL_BLOB_SIZE];
    bool guard_weapon_model_loaded;
    GeOriginalDamGuardScene guard_scene;
    GeOriginalDamGuardSceneCache guard_scene_cache;
    GeOriginalDamMissionFlowState mission_flow;
    bool mission_flow_live;
    RuntimeDamAlarmObject alarms[DAM_ALARM_OBJECT_COUNT];
    size_t alarm_count;
    size_t alarm_scan_count;
    uint32_t alarm_materialize_failure;
    GeOriginalStageAlarmInteractionResult alarm_interaction;
    uint64_t alarm_interaction_ticks;
    uint64_t alarm_interaction_activations;
    GeOriginalStageObjectiveRegistry objectives;
    GeOriginalStageObjectiveRuntime objective_runtime;
    GeOriginalStageObjectiveEvaluation objective_evaluations[
        GE_ORIGINAL_STAGE_OBJECTIVE_MAX];
    size_t objective_evaluation_ready_count;
    size_t objective_evaluation_blocked_count;
    uint64_t objective_evaluation_ticks;
    GeOriginalModelSceneStatus model_scene_status;
    GeDamDynamicSceneStatus model_overlay_status;
    size_t model_scene_vertices;
    size_t model_scene_batches;
    size_t model_scene_triangles;
    bool model_scene_ready;
    uint32_t model_scene_install_failure;
    uint8_t model_input_rooms[DAM_MODEL_SCENE_INPUT_CAPACITY];
    RuntimeDamOverlaySegment door_overlay[2];
    RuntimeDamOverlaySegment guard_overlay;
    uint64_t guard_overlay_updates;
    uint64_t door_overlay_updates;
    uint64_t overlay_full_rebuilds;
    RuntimeDamCollision *collision;
    RuntimeDamPreview *preview;
    union {
        u64 alignment;
        uint8_t bytes[0x50];
    } object_collision[DAM_NATIVE_OBJECT_CAPACITY];
    uint32_t object_collision_count;
} RuntimeDamWorldObjects;

typedef struct RuntimeStageArticulatedPublication
    RuntimeStageArticulatedPublication;

typedef struct RuntimeStageOrdinaryEntry {
    void *definition;
    void *prop;
    GeOriginalDefaultObjectPrepared prepared;
    GeOriginalDefaultObjectStatus construct_status;
    GeOriginalDefaultObjectStatus placement_status;
    GeOriginalStageSecurityInstance security;
    void *stage_allocation;
    size_t command_index;
    size_t owner_command_index;
    int32_t model_id;
    size_t definition_size;
    GeOriginalDamMonitorRenderSnapshot monitor_screens[4];
    GeOriginalDamMonitorRenderSnapshot published_monitor_screens[4];
    uint64_t monitor_ticks;
    uint8_t room;
    uint8_t type;
    uint8_t monitor_screen_count;
    uint8_t published_monitor_mask;
    RuntimeStageArticulatedPublication *articulated;
    bool live;
    bool root_active;
    bool attached_monitor;
    bool pending_inside_owner;
} RuntimeStageOrdinaryEntry;

typedef GeScenePartRange RuntimeStageScenePartRange;

struct RuntimeStageArticulatedPublication {
    GeOriginalModelSceneCache cache;
    GeOriginalModelSceneInput *inputs;
    GeDamRoomWorldVertex *vertices;
    GeDamRoomDrawBatch *batches;
    size_t input_capacity;
    size_t vertex_capacity;
    size_t batch_capacity;
    uint64_t updates;
    uint64_t unchanged;
    uint64_t topology_changes;
    bool force_copy;
};

typedef enum RuntimeStageActorTickStatus {
    RUNTIME_STAGE_ACTOR_TICK_UNINITIALIZED = 0,
    RUNTIME_STAGE_ACTOR_TICK_READY,
    RUNTIME_STAGE_ACTOR_TICK_MISSING_PLAYER,
    RUNTIME_STAGE_ACTOR_TICK_MISSING_GUARDS,
    RUNTIME_STAGE_ACTOR_TICK_MISSING_OBJECTS,
    RUNTIME_STAGE_ACTOR_TICK_MISSING_DOORS,
    RUNTIME_STAGE_ACTOR_TICK_COMPOSE_FAILED,
    RUNTIME_STAGE_ACTOR_TICK_MISSION_FAILED,
    RUNTIME_STAGE_ACTOR_TICK_RUNTIME_FAILED
} RuntimeStageActorTickStatus;

typedef enum RuntimeStageSceneInstallPhase {
    RUNTIME_STAGE_SCENE_INSTALL_NONE = 0,
    RUNTIME_STAGE_SCENE_INSTALL_ARGUMENTS,
    RUNTIME_STAGE_SCENE_INSTALL_VISIBILITY,
    RUNTIME_STAGE_SCENE_INSTALL_GUARD_QUERY,
    RUNTIME_STAGE_SCENE_INSTALL_ALLOCATE_INPUTS,
    RUNTIME_STAGE_SCENE_INSTALL_ORDINARY_QUERY,
    RUNTIME_STAGE_SCENE_INSTALL_DOOR_QUERY,
    RUNTIME_STAGE_SCENE_INSTALL_ALLOCATE_OUTPUT,
    RUNTIME_STAGE_SCENE_INSTALL_ORDINARY_BUILD,
    RUNTIME_STAGE_SCENE_INSTALL_GUARD_BUILD,
    RUNTIME_STAGE_SCENE_INSTALL_OVERLAY,
    RUNTIME_STAGE_SCENE_INSTALL_TEXTURES
} RuntimeStageSceneInstallPhase;

typedef struct RuntimeStageOrdinaryObjects {
    GeOriginalPitemModelProvider *models;
    GeOriginalPitemModelStatus model_status;
    GeOriginalPitemModelStats model_stats;
    GeOriginalStagePropMaterializerReport report;
    GeOriginalStagePropMaterializerReport guard_report;
    GeOriginalStageGuardWeaponBindReport guard_weapon_report;
    GeOriginalStageGuardHatBindReport guard_hat_report;
    GeOriginalDefaultObjectProviders object_providers;
    RuntimeStageOrdinaryEntry *entries;
    void **definitions;
    uint8_t *collision_blocks;
    size_t entry_capacity;
    size_t collision_capacity;
    size_t entry_count;
    size_t live_count;
    size_t root_live_count;
    size_t collision_count;
    size_t model_dependencies;
    GeOriginalCharacterModelProvider *guard_models;
    GeOriginalStageGuardRuntime *guards;
    GeOriginalCharacterModelStatus guard_model_status;
    GeOriginalStageGuardRuntimeStatus guard_status;
    GeOriginalStageGuardScene guard_scene;
    GeOriginalStageInteractiveRuntime interactive;
    GeOriginalDoorPrepared door_scratch;
    GeOriginalDoorStatus door_status;
    GeOriginalDoorRuntimeState door_runtime;
    GeOriginalDoorCollisionState door_collision;
    GeOriginalDoorInteractionState door_interaction;
    GeOriginalStageActiveProps active_props;
    GeOriginalStageMissionRuntime mission_runtime;
    GeOriginalStageObjectiveRegistry objectives;
    GeOriginalStageObjectiveRuntime objective_runtime;
    GeOriginalStageSafeRuntime safe_runtime;
    GeOriginalStageMiscStatus safe_relation_status;
    GeOriginalStageObjectiveEvaluation objective_evaluations[
        GE_ORIGINAL_STAGE_OBJECTIVE_MAX];
    GeOriginalStageActivePropInput *active_prop_inputs;
    GeOriginalStageActivePropStatus active_prop_status;
    RuntimeStageActorTickStatus actor_tick_status;
    size_t active_prop_count;
    uint64_t actor_tick_count;
    uint64_t guard_weapon_fire_dispatches;
    uint64_t guard_weapon_sound_starts;
    uint64_t guard_player_damage_events;
    float guard_player_health_damage;
    float guard_player_armour_damage;
    size_t guard_count;
    size_t guard_weapon_count;
    size_t guard_hat_count;
    size_t door_count;
    size_t live_door_count;
    size_t monitor_count;
    size_t monitor_screen_count;
    uint64_t monitor_noop_tick_count;
    uint64_t monitor_tick_count;
    RuntimeStageScenePartRange *ordinary_scene_parts;
    size_t ordinary_scene_part_count;
    uint64_t monitor_surface_update_count;
    uint64_t monitor_surface_unchanged_count;
    uint64_t monitor_surface_failure_count;
    uint64_t articulated_scene_update_count;
    uint64_t articulated_scene_unchanged_count;
    uint64_t articulated_scene_topology_change_count;
    uint64_t articulated_scene_failure_count;
    uint64_t articulated_replace_successes;
    uint64_t articulated_replace_peak_ticks;
    size_t articulated_replace_command;
    size_t articulated_replace_parts;
    size_t supply_count;
    size_t supply_slot_model_load_count;
    size_t owned_ordinary_embedded_count;
    size_t owned_ordinary_assigned_count;
    size_t owned_ordinary_pending_count;
    size_t tinted_glass_count;
    size_t cctv_count;
    size_t autogun_count;
    size_t alarm_count;
    size_t gas_releasing_count;
    GeOriginalStageAlarmInteractionResult alarm_interaction;
    uint64_t alarm_interaction_ticks;
    uint64_t alarm_interaction_activations;
    size_t safe_count;
    size_t safe_relation_count;
    uint64_t objective_evaluation_ticks;
    size_t objective_evaluation_ready_count;
    size_t objective_evaluation_blocked_count;
    void *last_resolved_model;
    size_t scene_vertices;
    size_t scene_batches;
    size_t scene_triangles;
    GeOriginalModelSceneStatus scene_status;
    GeDamDynamicSceneStatus overlay_status;
    GeOriginalPropState *props;
    const GeOriginalStageSetupRuntime *setup;
    RuntimeDamCollision *collision;
    RuntimeDamPreview *preview;
    bool initialized;
    bool scene_ready;
    uint64_t resident_install_successes;
    uint64_t resident_eviction_successes;
    RuntimeDamOverlaySegment door_overlay;
    RuntimeDamOverlaySegment guard_overlay;
    GeOriginalModelSceneCache door_scene_cache;
    GeOriginalModelSceneCache guard_scene_cache;
    uint32_t *installed_door_scene_generations;
    size_t installed_door_scene_generation_count;
    uint64_t guard_overlay_updates;
    uint64_t door_overlay_updates;
    uint64_t overlay_full_rebuilds;
    uint64_t door_overlay_refresh_failures;
    uint64_t guard_overlay_refresh_failures;
    uint64_t monitor_overlay_refresh_failures;
    uint64_t guard_topology_replace_successes;
    uint64_t guard_topology_replace_failures;
    uint32_t last_guard_topology_replace_status;
    uint32_t init_mask;
    uint32_t last_mission_tick_status;
    uint32_t last_monitor_tick_ok;
    uint32_t last_objective_tick_ok;
    uint32_t last_guard_lighting_status;
    uint32_t last_guard_matrix_status;
    uint64_t guard_visibility_publish_calls;
    uint64_t guard_visibility_publish_successes;
    uint64_t guard_visibility_publish_visible_requests;
    uint64_t guard_visibility_publish_active_requests;
    uint64_t guard_visibility_publish_enabled_requests;
    uint64_t guard_visibility_publish_onscreen_outputs;
    uint64_t scene_install_attempts;
    uint64_t scene_install_successes;
    uint32_t scene_install_failure_phase;
    size_t scene_install_input_count;
    size_t scene_install_ordinary_input_count;
    size_t scene_install_required_vertices;
    size_t scene_install_required_batches;
    uint64_t scene_install_phase_ticks[5];
} RuntimeStageOrdinaryObjects;

static bool input_probe_live_guard_aim_position(
    RuntimeInputProbe *probe,
    const RuntimeStageOrdinaryObjects *objects,
    const float view_to_world[4][4], float position[3],
    bool *guard_complete)
{
    const RuntimeInputProbeTarget *target;
    size_t guard_index;
    ge_original_guard_bullet_hit_observe_guard(-1);
    if (guard_complete != NULL) *guard_complete = false;
    if (probe == NULL || objects == NULL || position == NULL
            || probe->target_index >= probe->target_count
            || objects->guards == NULL) return false;
    target = &probe->targets[probe->target_index];
    if (target->aim_chr < 0) return false;
    ++probe->aim_resolver_calls;
    probe->last_aim_chr = target->aim_chr;
    probe->last_aim_guard_index = -1;
    probe->last_aim_target_index = (uint32_t)probe->target_index;
    probe->last_aim_route_frame = input_probe_route_frame(probe);
    probe->last_aim_visible = 0U;
    probe->last_aim_matrices_ready = 0U;
    probe->last_aim_death_complete = 0U;
    for (guard_index = 0U;
            guard_index < ge_original_stage_guard_runtime_count(
                objects->guards);
            ++guard_index) {
        GeOriginalStageGuardSnapshot snapshot;
        if (ge_original_stage_guard_runtime_snapshot(
                objects->guards, guard_index, &snapshot)
                && snapshot.chr_id == target->aim_chr) {
            int resolved;
            ge_original_guard_bullet_hit_observe_guard((int32_t)guard_index);
            ++probe->aim_target_found;
            probe->last_aim_guard_index = (int32_t)guard_index;
            probe->last_aim_visible = snapshot.visible;
            probe->last_aim_matrices_ready = snapshot.matrices_ready;
            probe->last_aim_prop_flags = snapshot.prop_flags;
            probe->last_aim_prop_zdepth = snapshot.prop_zdepth;
            probe->last_aim_model_size = snapshot.model_size;
            if (snapshot.matrices_ready != 0U)
                ++probe->aim_matrix_ready;
            if (ge_original_stage_guard_snapshot_death_complete(&snapshot)) {
                probe->last_aim_death_complete = 1U;
                if (guard_complete != NULL) *guard_complete = true;
                return false;
            }
            resolved = ge_original_stage_guard_runtime_autoaim_world_position(
                objects->guards, guard_index, view_to_world, position);
            if (resolved) {
                ++probe->aim_resolver_successes;
                memcpy(probe->last_aim_world, position,
                    sizeof(probe->last_aim_world));
            }
            return resolved != 0;
        }
    }
    return false;
}

#define RUNTIME_STAGE_GUARD_COMBAT_AUDIT_CAPACITY 256U
typedef struct RuntimeStageGuardCombatAudit {
    int32_t firecount[RUNTIME_STAGE_GUARD_COMBAT_AUDIT_CAPACITY][2];
    size_t guard_count;
    uint32_t sound_play_calls;
    float health;
    float armour;
} RuntimeStageGuardCombatAudit;

static void stage_guard_combat_audit(
    RuntimeStageOrdinaryObjects *objects, RuntimeStageGuardCombatAudit *audit)
{
    GeOriginalGameplayServiceStats services = {0};
    size_t index;
    memset(audit, 0, sizeof(*audit));
    if (objects == NULL || objects->guards == NULL) return;
    audit->guard_count = ge_original_stage_guard_runtime_count(objects->guards);
    if (audit->guard_count > RUNTIME_STAGE_GUARD_COMBAT_AUDIT_CAPACITY)
        audit->guard_count = RUNTIME_STAGE_GUARD_COMBAT_AUDIT_CAPACITY;
    ge_original_gameplay_services_snapshot(&services);
    audit->sound_play_calls = services.sound_play_calls;
    ge_original_dam_guard_player_vitals_snapshot(
        &audit->health, &audit->armour);
    for (index = 0U; index < audit->guard_count; ++index) {
        /* This audit is a platform statistic, not part of chrpropTick.  The
         * public snapshot also walks the complete active-prop list to publish
         * its diagnostic active_linked bit; doing that twice for every guard
         * on every original tick was quadratic overhead in live gameplay.
         * Read the same canonical counters through the opaque runtime's O(1)
         * accessor instead. */
        (void)ge_original_stage_guard_runtime_firecount(
            objects->guards,index,audit->firecount[index]);
    }
}

static void stage_guard_combat_record(
    RuntimeStageOrdinaryObjects *objects,
    const RuntimeStageGuardCombatAudit *before,
    const RuntimeStageGuardCombatAudit *after)
{
    size_t index;
    size_t count;
    float health_damage;
    float armour_damage;
    if (objects == NULL) return;
    count = before->guard_count < after->guard_count
        ? before->guard_count : after->guard_count;
    for (index = 0U; index < count; ++index) {
        size_t hand;
        for (hand = 0U; hand < 2U; ++hand)
            if (before->firecount[index][hand]
                    != after->firecount[index][hand])
                objects->guard_weapon_fire_dispatches++;
    }
    if (after->sound_play_calls >= before->sound_play_calls)
        objects->guard_weapon_sound_starts +=
            after->sound_play_calls - before->sound_play_calls;
    health_damage = before->health - after->health;
    armour_damage = before->armour - after->armour;
    if (health_damage > 0.0f || armour_damage > 0.0f)
        objects->guard_player_damage_events++;
    if (health_damage > 0.0f)
        objects->guard_player_health_damage += health_damage;
    if (armour_damage > 0.0f)
        objects->guard_player_armour_damage += armour_damage;
}

static void dam_overlay_segment_close(RuntimeDamOverlaySegment *segment)
{
    if (segment == NULL) return;
    free(segment->batches);
    free(segment->vertices);
    memset(segment, 0, sizeof(*segment));
}

static bool dam_overlay_segment_capture(
    RuntimeDamOverlaySegment *segment,
    const GeDamRoomWorldVertex *vertices,
    size_t vertex_offset, size_t vertex_count,
    const GeDamRoomDrawBatch *batches,
    size_t batch_offset, size_t batch_count)
{
    size_t index;
    if (segment == NULL || vertices == NULL || batches == NULL
            || vertex_count == 0U || batch_count == 0U)
        return false;
    memset(segment, 0, sizeof(*segment));
    segment->vertices = malloc(vertex_count * sizeof(*segment->vertices));
    segment->batches = malloc(batch_count * sizeof(*segment->batches));
    if (segment->vertices == NULL || segment->batches == NULL) {
        dam_overlay_segment_close(segment);
        return false;
    }
    memcpy(segment->vertices, vertices + vertex_offset,
           vertex_count * sizeof(*segment->vertices));
    memcpy(segment->batches, batches + batch_offset,
           batch_count * sizeof(*segment->batches));
    for (index = 0U; index < batch_count; index++) {
        if (segment->batches[index].first_vertex < vertex_offset) {
            dam_overlay_segment_close(segment);
            return false;
        }
        segment->batches[index].first_vertex -= vertex_offset;
    }
    segment->vertex_offset = vertex_offset;
    segment->vertex_count = vertex_count;
    segment->batch_offset = batch_offset;
    segment->batch_count = batch_count;
    return true;
}

/* Compare only fields consumed by the 3DS world upload. `processed.eye` is an
 * intermediate N64 view-space value; the PICA path uploads authored world
 * coordinates and applies the live camera in its shader. A moving camera can
 * therefore change every eye-space matrix while producing byte-identical GPU
 * vertices. */
static bool dam_overlay_segment_matches_published(
    const RuntimeDamPreview *preview,
    const RuntimeDamOverlaySegment *segment)
{
    const GeDamDynamicScene *scene;
    size_t index;
    if (preview == NULL || segment == NULL) return false;
    scene = &preview->dynamic_scene;
    if (scene->overlay_vertices == NULL || scene->overlay_batches == NULL
            || segment->vertex_offset > scene->overlay_vertex_count
            || segment->vertex_count
                > scene->overlay_vertex_count - segment->vertex_offset
            || segment->batch_offset > scene->overlay_batch_count
            || segment->batch_count
                > scene->overlay_batch_count - segment->batch_offset)
        return false;
    for (index = 0U; index < segment->batch_count; ++index) {
        GeDamRoomDrawBatch published =
            scene->overlay_batches[segment->batch_offset + index];
        if (published.first_vertex < segment->vertex_offset) return false;
        published.first_vertex -= segment->vertex_offset;
        if (memcmp(&segment->batches[index], &published,
                   sizeof(published)) != 0) return false;
    }
    for (index = 0U; index < segment->vertex_count; ++index) {
        const GeDamRoomWorldVertex *candidate = &segment->vertices[index];
        const GeDamRoomWorldVertex *published =
            &scene->overlay_vertices[segment->vertex_offset + index];
        if (memcmp(&candidate->source, &published->source,
                   sizeof(candidate->source)) != 0
                || memcmp(candidate->world, published->world,
                          sizeof(candidate->world)) != 0
                || memcmp(candidate->processed.texture,
                          published->processed.texture,
                          sizeof(candidate->processed.texture)) != 0
                || memcmp(candidate->processed.rgba,
                          published->processed.rgba,
                          sizeof(candidate->processed.rgba)) != 0)
            return false;
    }
    return true;
}

static bool dam_visibility_contains_room(const RuntimeDamPreview *preview,
                                         uint32_t room);
static bool upload_dam_gpu_world_scene(RuntimeDamPreview *preview,
                                       Vertex *destination);
static bool upload_dam_gpu_world_scene_range(RuntimeDamPreview *preview,
    Vertex *destination, size_t vertex_offset, size_t vertex_count,
    size_t batch_offset, size_t batch_count, bool map_texture_uv);

#if defined(GE_DAM_FULL_PROPS_LIVE)
static void dam_publish_rendered_prop(RuntimeDamPreview *preview,
                                      void *opaque_prop, uint8_t room)
{
    (void)ge_original_prop_state_publish_scene_visibility(
        opaque_prop, dam_visibility_contains_room(preview, room),
        preview->original_camera_view);
}
#endif

#if defined(GE_DAM_FULL_PROPS_LIVE)
static bool dam_full_props_live_ready(const RuntimeDamWorldObjects *objects)
{
    size_t index;
    if (objects == NULL
            || objects->guard_status != GE_ORIGINAL_DAM_GUARD_OK
            || !objects->doors_linked || !objects->model_scene_ready
            || objects->glass_object_status
                != GE_ORIGINAL_DEFAULT_OBJECT_OK
            || objects->glass_placement_status
                != GE_ORIGINAL_DEFAULT_OBJECT_OK
            || objects->default_object_status
                != GE_ORIGINAL_DEFAULT_OBJECT_OK
            || objects->placement_status != GE_ORIGINAL_DEFAULT_OBJECT_OK
            || objects->door_status[0] != GE_ORIGINAL_DOOR_OK
            || objects->door_status[1] != GE_ORIGINAL_DOOR_OK
            || !objects->glass_object.object_initialized
            || !objects->glass_object.placement_completed
            || !objects->default_object.object_initialized
            || !objects->default_object.placement_completed
            || !objects->doors[0].constructed
            || !objects->doors[1].constructed
            || !ge_original_prop_state_is_active(objects->glass_object.prop)
            || !ge_original_prop_state_is_enabled(objects->glass_object.prop)
            || !ge_original_prop_state_is_active(objects->default_object.prop)
            || !ge_original_prop_state_is_enabled(objects->default_object.prop)
            || !ge_original_prop_state_is_active(objects->doors[0].prop)
            || !ge_original_prop_state_is_enabled(objects->doors[0].prop)
            || !ge_original_prop_state_is_active(objects->doors[1].prop)
            || !ge_original_prop_state_is_enabled(objects->doors[1].prop))
        return false;

    for (index = 0U; index < GE_ORIGINAL_DAM_SPAWN_WINDOW_COUNT; ++index) {
        if (objects->spawn_window_status[index]
                    != GE_ORIGINAL_DEFAULT_OBJECT_OK
                || objects->spawn_window_placement_status[index]
                    != GE_ORIGINAL_DEFAULT_OBJECT_OK
                || !objects->spawn_windows[index].object_initialized
                || !objects->spawn_windows[index].placement_completed
                || !ge_original_prop_state_is_active(
                    objects->spawn_windows[index].prop)
                || !ge_original_prop_state_is_enabled(
                    objects->spawn_windows[index].prop))
            return false;
    }
    for (index = 0U; index < DAM_ALARM_OBJECT_COUNT; ++index) {
        const RuntimeDamAlarmObject *alarm = &objects->alarms[index];
        if (!alarm->live || alarm->status != GE_ORIGINAL_STAGE_MISC_OK
                || alarm->construct_status
                    != GE_ORIGINAL_DEFAULT_OBJECT_OK
                || alarm->placement_status
                    != GE_ORIGINAL_DEFAULT_OBJECT_OK
                || !alarm->prepared.object_initialized
                || !alarm->prepared.placement_completed
                || !ge_original_prop_state_is_active(alarm->prop)
                || !ge_original_prop_state_is_enabled(alarm->prop))
            return false;
    }

    return true;
}
#endif

static int32_t dam_door_global_timer(void *context)
{
    const GePortState *port = context;
    return port != NULL ? (int32_t)port->original_global_timer : 0;
}

static int32_t dam_door_clock_timer(void *context)
{
    const GePortState *port = context;
    return port != NULL ? (int32_t)port->original_clock_timer : 0;
}

typedef struct RuntimeBondAnimations {
    uint8_t *segment;
    size_t segment_size;
    uint8_t *idle_frames;
    size_t idle_frames_size;
    uint8_t *sprinting_frames;
    size_t sprinting_frames_size;
    uint8_t *walking_frames;
    size_t walking_frames_size;
    GeOriginalAnimationRoot *idle;
    GeOriginalAnimationRoot *sprinting;
    GeOriginalAnimationRoot *walking;
    GeOriginalPlayerGait *gait;
    GeOriginalPlayerGaitStatus gait_status;
    bool decoder_verified;
    bool gait_verified;
    bool loaded;
} RuntimeBondAnimations;

typedef struct RuntimeFirstPersonModels {
    GeOriginalFirstPersonAssets assets;
    GeOriginalFirstPersonLoaderState loader;
    uint8_t *buffers[2];
    void *headers[2];
    GeOriginalFirstPersonAssetStatus status[2];
    GeOriginalBondLiveState bond_live;
    GeOriginalFirstPersonPoseState pose;
    GeOriginalFirstPersonPoseStatus pose_status;
    bool ready;
} RuntimeFirstPersonModels;

static void input_probe_capture_player(
    RuntimeInputProbe *runtime, const RuntimeDamIntro *intro)
{
    const float *position;
    uint32_t route_frame;
    if (runtime == NULL || !runtime->enabled || intro == NULL
            || !intro->player.initialized) return;
    position = intro->player.collision_position;
    route_frame = input_probe_route_frame(runtime);
    if (!runtime->started) {
        memcpy(runtime->start_position, position,
               sizeof(runtime->start_position));
        runtime->minimum_y = position[1];
        runtime->maximum_y = position[1];
        runtime->start_room = (uint8_t)intro->player.room;
        runtime->last_room = runtime->start_room;
        runtime->visited_rooms[runtime->start_room] = 1U;
        runtime->visited_room_count = 1U;
        runtime->transition_rooms[0] = runtime->start_room;
        runtime->transition_frames[0] = 0U;
        runtime->transition_record_count = 1U;
        memcpy(runtime->start_look, intro->player.camera_look,
               sizeof(runtime->start_look));
        runtime->started = true;
    }
    memcpy(runtime->end_position, position, sizeof(runtime->end_position));
    runtime->end_room = (uint8_t)intro->player.room;
    memcpy(runtime->end_look, intro->player.camera_look,
           sizeof(runtime->end_look));
    if (runtime->end_room != runtime->last_room) {
        runtime->last_room = runtime->end_room;
        ++runtime->room_transition_count;
        if (runtime->transition_record_count
                < RUNTIME_INPUT_PROBE_SEGMENT_CAPACITY) {
            const size_t transition = runtime->transition_record_count++;
            runtime->transition_rooms[transition] = runtime->end_room;
            runtime->transition_frames[transition] =
                route_frame;
        }
        if (runtime->visited_rooms[runtime->end_room] == 0U) {
            runtime->visited_rooms[runtime->end_room] = 1U;
            ++runtime->visited_room_count;
        }
    }
    if (!runtime->stop_captured
            && route_frame >= runtime->active_frames) {
        memcpy(runtime->stop_position, position,
               sizeof(runtime->stop_position));
        runtime->stop_captured = true;
    }
    if (!runtime->settle_captured && runtime->target_frames > 60U
            && route_frame >= runtime->target_frames - 60U) {
        memcpy(runtime->settle_position, position,
               sizeof(runtime->settle_position));
        runtime->settle_captured = true;
    }
    if (position[1] < runtime->minimum_y) runtime->minimum_y = position[1];
    if (position[1] > runtime->maximum_y) runtime->maximum_y = position[1];
}

static void input_probe_capture_armour(RuntimeInputProbe *runtime)
{
    float armour = 0.0f;
    if (runtime == NULL || !runtime->enabled) return;
    ge_original_dam_guard_player_vitals_snapshot(NULL, &armour);
    if (!isfinite(armour) || armour <= 0.0f) return;
    if (!runtime->armour_observed) {
        runtime->first_armour_frame = runtime->simulation_frames + 1U;
        runtime->armour_observed = true;
    }
    if (armour > runtime->maximum_player_armour)
        runtime->maximum_player_armour = armour;
}

static void input_probe_capture_gates(
    RuntimeInputProbe *runtime, const RuntimeStageOrdinaryObjects *objects,
    bool count_simulation_frame)
{
    GeOriginalDoorRuntimePublication gates[2];
    bool found[2] = {false, false};
    size_t index;
    uint32_t activation_delta;
    if (runtime == NULL || !runtime->enabled || objects == NULL) return;
    memset(gates, 0, sizeof(gates));
    for (index = 0U; index < objects->interactive.entry_count; ++index) {
        const GeOriginalStageInteractiveEntry *entry =
            ge_original_stage_interactive_entry(&objects->interactive, index);
        size_t gate_index;
        if (entry == NULL || !entry->constructed
                || entry->type != PROPDEF_DOOR
                || (entry->command_index != 267U
                    && entry->command_index != 268U)) continue;
        gate_index = entry->command_index - 267U;
        if (!ge_original_door_runtime_snapshot(
                entry->definition, &gates[gate_index])) continue;
        found[gate_index] = true;
        if (runtime->gate_start_generation[gate_index] == 0U)
            runtime->gate_start_generation[gate_index] =
                gates[gate_index].generation;
        if (gates[gate_index].open_position
                > runtime->gate_max_open_position[gate_index])
            runtime->gate_max_open_position[gate_index] =
                gates[gate_index].open_position;
    }
    if (count_simulation_frame && found[0] && found[1]
            && gates[0].open_position > 0.0f
            && gates[1].open_position > 0.0f)
        ++runtime->gate_both_open_frames;
    activation_delta = objects->door_interaction.activations
        - runtime->gate_last_interaction_activations;
    if (activation_delta != 0U) {
        for (index = 0U; index < objects->interactive.entry_count; ++index) {
            const GeOriginalStageInteractiveEntry *entry =
                ge_original_stage_interactive_entry(
                    &objects->interactive, index);
            if (entry == NULL || (entry->command_index != 267U
                    && entry->command_index != 268U)
                    || entry->prop != objects->door_interaction.last_prop)
                continue;
            runtime->gate_activation_count[entry->command_index - 267U]
                += activation_delta;
            break;
        }
    }
    runtime->gate_last_interaction_activations =
        objects->door_interaction.activations;
}

static bool write_input_probe_result(
    const RuntimeInputProbe *runtime,
    const RuntimeStageOrdinaryObjects *objects,
    const RuntimeFirstPersonModels *first_person_models,
    const GeOriginalFirstPersonSceneCache *first_person_cache,
    const RuntimeDamIntro *intro)
{
    GeOriginalMusicPortSnapshot music_port = {0};
    FILE *stream;
    float dx;
    float dy;
    float dz;
    float displacement;
    float idle_dx;
    float idle_dy;
    float idle_dz;
    float idle_drift;
    float settle_dx;
    float settle_dy;
    float settle_dz;
    float final_60_drift;
    uint16_t primary_offset = 0U;
    uint16_t exit_offset = 0U;
    GeOriginalGunLiveStats gun = {0};
    GeOriginalPp7FireStats pp7 = {0};
    GeOriginalGuardBulletHitStats guard_hits = {0};
    GeOriginalGuardAiLosStats guard_los = {0};
    GeOriginalPlayerCombatSnapshot player_combat = {0};
    GeOriginalBondAimSnapshot aim = {0};
    GeOriginalBondMotionSnapshot motion = {0};
    GeOriginalCovertModemFireStats modem = {0};
    GeOriginalGameplayServiceStats services = {0};
    GeOriginalDamMissionExitSnapshot mission_exit = {0};
    GeStanNativeRouteSearchStats route_search = {0};
    GeOriginalStagePathAudit path_audit = {0};
    GeOriginalDoorRuntimePublication gate_publication[2];
    GeOriginalBgRoomVisibilitySnapshot bg_camera_room = {0};
    GeOriginalBgRoomVisibilitySnapshot bg_player_room = {0};
    int bg_camera_room_valid = 0;
    int bg_player_room_valid = 0;
    bool gate_found[2] = {false, false};
    const GeAudioRefillState *audio_refill;
    size_t valid_guard_stans = 0U;
    size_t guard_stan_index;
    if (runtime == NULL || objects == NULL || first_person_models == NULL
            || intro == NULL
            || !runtime->enabled || !runtime->started) return false;
    memset(gate_publication, 0, sizeof(gate_publication));
    dx = runtime->end_position[0] - runtime->start_position[0];
    dy = runtime->end_position[1] - runtime->start_position[1];
    dz = runtime->end_position[2] - runtime->start_position[2];
    displacement = sqrtf(dx * dx + dy * dy + dz * dz);
    idle_dx = runtime->end_position[0] - runtime->stop_position[0];
    idle_dy = runtime->end_position[1] - runtime->stop_position[1];
    idle_dz = runtime->end_position[2] - runtime->stop_position[2];
    idle_drift = runtime->stop_captured
        ? sqrtf(idle_dx * idle_dx + idle_dy * idle_dy + idle_dz * idle_dz)
        : -1.0f;
    settle_dx = runtime->end_position[0] - runtime->settle_position[0];
    settle_dy = runtime->end_position[1] - runtime->settle_position[1];
    settle_dz = runtime->end_position[2] - runtime->settle_position[2];
    final_60_drift = runtime->settle_captured
        ? sqrtf(settle_dx * settle_dx + settle_dy * settle_dy
            + settle_dz * settle_dz) : -1.0f;
    (void)ge_original_stage_mission_runtime_actor_offset(
        &objects->mission_runtime, 0x1000, &primary_offset);
    (void)ge_original_stage_mission_runtime_actor_offset(
        &objects->mission_runtime, 0x1004, &exit_offset);
    ge_original_gun_live_snapshot(&gun);
    ge_original_pp7_fire_snapshot(&pp7);
    ge_original_guard_bullet_hit_snapshot(&guard_hits);
    ge_original_guard_ai_los_trace_snapshot(&guard_los);
    ge_original_dam_guard_player_combat_snapshot(&player_combat);
    (void)ge_original_bond_live_aim_snapshot(&aim);
    (void)ge_original_bond_live_motion_snapshot(&motion);
    ge_original_covert_modem_fire_snapshot(&modem);
    ge_original_gameplay_services_snapshot(&services);
    ge_original_dam_mission_exit_services_snapshot(&mission_exit);
    ge_stan_native_route_search_snapshot(&route_search);
    if (objects->preview != NULL) {
        bg_camera_room_valid = ge_original_bg_visibility_room_snapshot(
            objects->preview->original_camera_room, &bg_camera_room);
    }
    bg_player_room_valid = ge_original_bg_visibility_room_snapshot(
        runtime->end_room, &bg_player_room);
    (void)ge_original_stage_setup_path_audit(objects->setup, &path_audit);
    audio_refill = ge_3ds_audio_refill_stats();
    for (guard_stan_index = 0U;
            guard_stan_index < objects->interactive.entry_count;
            ++guard_stan_index) {
        const GeOriginalStageInteractiveEntry *entry =
            ge_original_stage_interactive_entry(
                &objects->interactive, guard_stan_index);
        size_t gate_index;
        if (entry == NULL || !entry->constructed
                || entry->type != PROPDEF_DOOR
                || (entry->command_index != 267U
                    && entry->command_index != 268U)) continue;
        gate_index = entry->command_index - 267U;
        gate_found[gate_index] = ge_original_door_runtime_snapshot(
            entry->definition, &gate_publication[gate_index]) != 0;
    }
    for (guard_stan_index = 0U;
            guard_stan_index < ge_original_stage_guard_runtime_count(
                objects->guards); ++guard_stan_index) {
        if (ge_stan_native_contains_tile(&objects->collision->native,
                ge_original_stage_guard_runtime_stan(
                    objects->guards, guard_stan_index)))
            ++valid_guard_stans;
    }
    stream = fopen(INPUT_PROBE_RESULT_PATH, "wb");
    if (stream == NULL) return false;
    fprintf(stream, "GE_INPUT_PROBE_RESULT 1\n");
    fprintf(stream, "level_id=%ld\n",
        (long)(objects->preview != NULL && objects->preview->stage_assets != NULL
            ? objects->preview->stage_assets->level_id : LEVELID_NONE));
    fprintf(stream, "idle_present_skips=%llu\n",
        (unsigned long long)fine_profile.idle_present_skips);
    fprintf(stream, "status=%s\n",
        objects->actor_tick_status == RUNTIME_STAGE_ACTOR_TICK_READY
                && objects->active_prop_status
                    == GE_ORIGINAL_STAGE_ACTIVE_PROP_OK
                && first_person_models->bond_live.move_tick_count
                    > runtime->move_tick_start
                && (runtime->target_count == 0U
                    || runtime->target_index == runtime->target_count)
            ? "complete" : "failed");
    fprintf(stream, "frames=%lu\n",
        (unsigned long)runtime->displayed_frames);
    fprintf(stream, "simulation_frames=%lu\n",
        (unsigned long)input_probe_route_frame(runtime));
    fprintf(stream, "route_targets=%lu,%lu\n",
        (unsigned long)runtime->target_index,
        (unsigned long)runtime->target_count);
    fprintf(stream, "target_trace=");
    {
        size_t target;
        for (target = 0U; target < runtime->target_index; ++target) {
            fprintf(stream, "%s%lu@%lu", target == 0U ? "" : ",",
                (unsigned long)target,
                (unsigned long)runtime->target_reached_frames[target]);
        }
    }
    fputc('\n', stream);
    {
        size_t trace;
        for (trace = 0U; trace < runtime->trace_count; ++trace) {
            const RuntimeInputProbeTrace *sample = &runtime->trace[trace];
            fprintf(stream,
                "steer=%lu,%u,%.1f,%.1f,%.3f,%.3f,%.2f,%.2f,%.3f\n",
                (unsigned long)sample->frame, (unsigned)sample->target,
                sample->position_x, sample->position_z,
                sample->look_x, sample->look_z,
                sample->move_x, sample->move_y, sample->angle);
        }
    }
    fprintf(stream, "start=%.6f,%.6f,%.6f\n",
        runtime->start_position[0], runtime->start_position[1],
        runtime->start_position[2]);
    fprintf(stream, "end=%.6f,%.6f,%.6f\n",
        runtime->end_position[0], runtime->end_position[1],
        runtime->end_position[2]);
    fprintf(stream, "displacement=%.6f\n", displacement);
    fprintf(stream, "idle_drift=%.6f\n", idle_drift);
    fprintf(stream, "idle_height_drift=%.6f\n", idle_dy);
    fprintf(stream, "final_60_drift=%.6f\n", final_60_drift);
    fprintf(stream, "final_60_height_drift=%.6f\n", settle_dy);
    fprintf(stream, "height_span=%.6f\n",
        runtime->maximum_y - runtime->minimum_y);
    fprintf(stream, "room=%u,%u\n",
        (unsigned)runtime->start_room, (unsigned)runtime->end_room);
    fprintf(stream, "rooms_visited=%lu\n",
        (unsigned long)runtime->visited_room_count);
    fprintf(stream, "room_transitions=%lu\n",
        (unsigned long)runtime->room_transition_count);
    fprintf(stream, "room_list=");
    {
        unsigned room;
        bool first = true;
        for (room = 0U; room < 256U; ++room) {
            if (runtime->visited_rooms[room] == 0U) continue;
            fprintf(stream, "%s%u", first ? "" : ",", room);
            first = false;
        }
    }
    fputc('\n', stream);
    fprintf(stream, "room_trace=");
    {
        size_t transition;
        for (transition = 0U;
                transition < runtime->transition_record_count;
                ++transition) {
            fprintf(stream, "%s%u@%lu", transition == 0U ? "" : ",",
                (unsigned)runtime->transition_rooms[transition],
                (unsigned long)runtime->transition_frames[transition]);
        }
    }
    fputc('\n', stream);
    fprintf(stream, "start_look=%.6f,%.6f,%.6f\n",
        runtime->start_look[0], runtime->start_look[1],
        runtime->start_look[2]);
    fprintf(stream, "end_look=%.6f,%.6f,%.6f\n",
        runtime->end_look[0], runtime->end_look[1],
        runtime->end_look[2]);
    fprintf(stream, "move_ticks=%llu\n",
        (unsigned long long)(first_person_models->bond_live.move_tick_count
            - runtime->move_tick_start));
    fprintf(stream, "movement_collision=%lu,%lu,%lu,%lu,%lu,%d\n",
        (unsigned long)intro->movement.collision_checks,
        (unsigned long)intro->movement.accepted_checks,
        (unsigned long)intro->movement.blocked_checks,
        (unsigned long)intro->movement.root_motion_ticks,
        (unsigned long)intro->movement.root_motion_samples_missing,
        (int)intro->movement.room);
    fprintf(stream, "movement_blocker=%lu,%lu,%08lx,%ld,%ld\n",
        (unsigned long)intro->movement.blocked_by_prop,
        (unsigned long)intro->movement.blocked_by_stan,
        (unsigned long)intro->movement.last_blocking_prop,
        (long)intro->movement.last_blocking_prop_type,
        (long)intro->movement.last_blocking_object_type);
    fprintf(stream,
        "motion=%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%ld,%ld,%ld,%ld,%ld\n",
        motion.speed_forwards, motion.speed_sideways, motion.speed_boost,
        motion.head_position[0], motion.head_position[1],
        motion.head_position[2], motion.stan_height, motion.eye_height,
        (long)motion.controls_locked, (long)motion.watch_animation_state,
        (long)motion.dead, (long)motion.camera_mode, (long)motion.in_tank);
    fprintf(stream, "actor_ticks=%llu\n",
        (unsigned long long)objects->actor_tick_count);
    fprintf(stream, "mission_offsets=%u,%u\n",
        (unsigned)primary_offset, (unsigned)exit_offset);
    fprintf(stream,
        "mission_exit=%ld,%ld,%ld,%.6f,%.6f,%.6f,%llu,%lu,%lu,%lu\n",
        (long)mission_exit.stop_time, (long)mission_exit.timer_active,
        (long)mission_exit.camera_mode, mission_exit.fade_fraction,
        mission_exit.fade_time, mission_exit.fade_time_max,
        (unsigned long long)mission_exit.fade_ticks,
        (unsigned long)mission_exit.posend_camera_requests,
        (unsigned long)mission_exit.title_stage_requests,
        (unsigned long)mission_exit.briefing_frontiers);
    fprintf(stream, "mission_result=%lu,%lu,%lu\n",
        (unsigned long)mission_exit.title_stage_requests,
        (unsigned long)mission_exit.briefing_frontiers,
        (unsigned long)mission_exit.briefing_commits);
    fprintf(stream, "player_death=%u,%lu,%lu,%lu,%lu,%lu,%lu\n",
        (unsigned)player_combat.dead,
        (unsigned long)mission_exit.death_starts,
        (unsigned long)mission_exit.death_blood_frames,
        (unsigned long)mission_exit.death_animation_finishes,
        (unsigned long)mission_exit.death_camera_starts,
        (unsigned long)mission_exit.death_title_requests,
        (unsigned long)mission_exit.death_service_frontiers);
    fprintf(stream, "objective_status=");
    {
        size_t menu;
        bool first = true;
        for (menu = 0U; menu < GE_ORIGINAL_STAGE_OBJECTIVE_MAX; ++menu) {
            const GeOriginalStageObjectiveEvaluation *evaluation;
            if (objects->objectives.objective_by_menu[menu] < 0) continue;
            evaluation = &objects->objective_evaluations[menu];
            fprintf(stream, "%s%lu:%u:%u:%lu", first ? "" : ",",
                (unsigned long)menu, (unsigned)evaluation->value,
                (unsigned)evaluation->blocker,
                (unsigned long)evaluation->criterion_index);
            first = false;
        }
    }
    fputc('\n', stream);
    fprintf(stream, "mission_objectives=0x%08lx\n",
        (unsigned long)objects->mission_runtime.objective_registers);
    fprintf(stream, "door_interaction=%lu,%lu,%lu,%lu,%lu,%u\n",
        (unsigned long)objects->door_interaction.ticks,
        (unsigned long)objects->door_interaction.activate_edges,
        (unsigned long)objects->door_interaction.interaction_tests,
        (unsigned long)objects->door_interaction.interaction_hits,
        (unsigned long)objects->door_interaction.activations,
        (unsigned)objects->door_interaction.result);
    {
        size_t gate_index;
        for (gate_index = 0U; gate_index < 2U; ++gate_index) {
            const GeOriginalDoorRuntimePublication *gate =
                &gate_publication[gate_index];
            fprintf(stream, "gate=%lu,%u,%ld,%.6f,%.6f,%ld,%lu\n",
                (unsigned long)(267U + gate_index),
                gate_found[gate_index] ? 1U : 0U,
                (long)gate->open_state, gate->open_position, gate->max_frac,
                (long)gate->portal_number, (unsigned long)gate->generation);
            fprintf(stream,
                "gate_route=%lu,%u,%lu,%lu,%lu,%.6f,%ld,%.6f,%.6f,%ld\n",
                (unsigned long)(267U + gate_index),
                gate_found[gate_index] ? 1U : 0U,
                (unsigned long)runtime->gate_start_generation[gate_index],
                (unsigned long)gate->generation,
                (unsigned long)runtime->gate_activation_count[gate_index],
                runtime->gate_max_open_position[gate_index],
                (long)gate->open_state, gate->open_position, gate->max_frac,
                (long)gate->portal_number);
        }
    }
    fprintf(stream, "gate_both_open_frames=%lu\n",
        (unsigned long)runtime->gate_both_open_frames);
    fprintf(stream, "actor_status=%u,%u,%u,%u,%u,%u,%u,%u\n",
        (unsigned)objects->actor_tick_status,
        (unsigned)objects->active_prop_status,
        (unsigned)objects->last_mission_tick_status,
        (unsigned)objects->last_monitor_tick_ok,
        (unsigned)objects->last_objective_tick_ok,
        (unsigned)objects->last_guard_lighting_status,
        (unsigned)objects->last_guard_matrix_status,
        (unsigned)objects->active_props.last_binding_mismatch);
    {
        size_t failure_line;
        int32_t failure_chr;
        ge_original_stage_guard_runtime_matrix_failure(
            objects->guards, &failure_line, &failure_chr);
        fprintf(stream, "guard_matrix_failure=%lu,%ld\n",
            (unsigned long)failure_line, (long)failure_chr);
        if (failure_line != 0U) {
            size_t matrix_index;
            int retained;
            float values[16];
            ge_original_stage_guard_runtime_matrix_failure_values(
                objects->guards, &matrix_index, &retained, values);
            fprintf(stream, "guard_matrix_values=%lu,%d",
                (unsigned long)matrix_index, retained);
            for (size_t value = 0U; value < 16U; ++value)
                fprintf(stream, ",%a", (double)values[value]);
            fprintf(stream, "\n");
            fprintf(stream, "guard_matrix_bits=");
            for (size_t value = 0U; value < 16U; ++value) {
                uint32_t bits;
                memcpy(&bits, &values[value], sizeof(bits));
                fprintf(stream, "%s%08lx", value ? "," : "", (unsigned long)bits);
            }
            fprintf(stream, "\n");
            {
                float camera[32], model[32];
                ge_original_stage_guard_runtime_matrix_failure_state(
                    objects->guards, camera, model);
                fprintf(stream, "guard_matrix_camera=");
                for (size_t value = 0U; value < 32U; ++value)
                    fprintf(stream, "%s%a", value ? "," : "", (double)camera[value]);
                fprintf(stream, "\nguard_matrix_model=");
                for (size_t value = 0U; value < 32U; ++value)
                    fprintf(stream, "%s%a", value ? "," : "", (double)model[value]);
                fprintf(stream, "\n");
            }
        }
    }
    fprintf(stream,
        "guard_visibility_publish=%llu,%llu,%llu,%llu,%llu,%llu\n",
        (unsigned long long)objects->guard_visibility_publish_calls,
        (unsigned long long)objects->guard_visibility_publish_successes,
        (unsigned long long)objects->guard_visibility_publish_visible_requests,
        (unsigned long long)objects->guard_visibility_publish_active_requests,
        (unsigned long long)objects->guard_visibility_publish_enabled_requests,
        (unsigned long long)objects->guard_visibility_publish_onscreen_outputs);
    fprintf(stream,
        "bg_visibility_shared=%u,%ld,%ld,%ld,%u,%u,%u,%u,%ld,%ld,%ld,%u,%u,%u\n",
        bg_camera_room_valid != 0 ? 1U : 0U,
        (long)bg_camera_room.current_room,
        (long)bg_camera_room.maximum_room_count,
        (long)bg_camera_room.rooms_drawn,
        objects->preview != NULL
            ? (unsigned)objects->preview->original_camera_room : 0U,
        (unsigned)bg_camera_room.rendered,
        (unsigned)bg_camera_room.neighbor_to_rendered,
        bg_player_room_valid != 0 ? 1U : 0U,
        (long)bg_player_room.current_room,
        (long)bg_player_room.maximum_room_count,
        (long)bg_player_room.rooms_drawn,
        (unsigned)runtime->end_room,
        (unsigned)bg_player_room.rendered,
        (unsigned)bg_player_room.neighbor_to_rendered);
    fprintf(stream,
        "stage_scene_install=%llu,%llu,%lu,%lu,%lu,%lu,%lu,%u,%u,%u\n",
        (unsigned long long)objects->scene_install_attempts,
        (unsigned long long)objects->scene_install_successes,
        (unsigned long)objects->scene_install_input_count,
        (unsigned long)objects->scene_install_ordinary_input_count,
        (unsigned long)objects->scene_install_required_vertices,
        (unsigned long)objects->scene_install_required_batches,
        (unsigned long)objects->scene_status,
        (unsigned)objects->guard_status,
        (unsigned)objects->overlay_status,
        (unsigned)objects->scene_install_failure_phase);
    fprintf(stream, "stage_scene_install_ticks=%llu,%llu,%llu,%llu,%llu\n",
        (unsigned long long)objects->scene_install_phase_ticks[0],
        (unsigned long long)objects->scene_install_phase_ticks[1],
        (unsigned long long)objects->scene_install_phase_ticks[2],
        (unsigned long long)objects->scene_install_phase_ticks[3],
        (unsigned long long)objects->scene_install_phase_ticks[4]);
    fprintf(stream, "articulated_publication=%llu,%llu,%llu,%llu\n",
        (unsigned long long)objects->articulated_scene_update_count,
        (unsigned long long)objects->articulated_scene_unchanged_count,
        (unsigned long long)objects->articulated_scene_topology_change_count,
        (unsigned long long)objects->articulated_scene_failure_count);
    fprintf(stream, "articulated_replacement=%llu,%llu,%lu,%lu\n",
        (unsigned long long)objects->articulated_replace_successes,
        (unsigned long long)objects->articulated_replace_peak_ticks,
        (unsigned long)objects->articulated_replace_command,
        (unsigned long)objects->articulated_replace_parts);
    fprintf(stream, "guard_scene_cache=%llu,%llu,%llu,%llu,%llu\n",
        (unsigned long long)objects->guard_scene_cache.build_attempts,
        (unsigned long long)objects->guard_scene_cache.cached_builds,
        (unsigned long long)objects->guard_scene_cache.unchanged_builds,
        (unsigned long long)objects->guard_scene_cache
            .identity_outer_vertices_published,
        (unsigned long long)objects->guard_scene_cache.topology_rebuilds);
    fprintf(stream, "guard_scene_reuse=%llu,%llu\n",
        (unsigned long long)objects->guard_scene_cache
            .static_vertex_copies_avoided,
        (unsigned long long)objects->guard_scene_cache
            .static_batch_copies_avoided);
    fprintf(stream, "guard_scene_variants=%llu,%llu,%lu\n",
        (unsigned long long)objects->guard_scene_cache.topology_variant_hits,
        (unsigned long long)objects->guard_scene_cache
            .topology_variant_evictions,
        (unsigned long)objects->guard_scene_cache.topology_variant_count);
    fprintf(stream, "guard_scene_components=%llu,%llu,%lu,%lu\n",
        (unsigned long long)objects->guard_scene_cache
            .topology_component_hits,
        (unsigned long long)objects->guard_scene_cache
            .topology_component_misses,
        (unsigned long)objects->guard_scene_cache.topology_component_count,
        (unsigned long)objects->guard_scene_cache.topology_component_bytes);
    fprintf(stream, "setup_ptrs=%08lx,%08lx,%08lx\n",
        (unsigned long)(uintptr_t)objects->setup->pads_storage,
        (unsigned long)(uintptr_t)objects->setup->waypoints_storage,
        (unsigned long)ge_original_stage_setup_publication_mask(
            objects->setup));
    fprintf(stream, "stan_bind=%u,%lu,%lu\n",
        ge_stan_native_original_binding_matches(
            &objects->collision->native) ? 1U : 0U,
        (unsigned long)valid_guard_stans,
        (unsigned long)ge_original_stage_guard_runtime_count(
            objects->guards));
    fprintf(stream, "stan_route_search=%lu,%lu,%lu\n",
        (unsigned long)route_search.calls,
        (unsigned long)route_search.rejected_start_tiles,
        (unsigned long)route_search.rejected_result_tiles);
    fprintf(stream, "path_audit=%lu,%lu,%lu,%ld,%ld\n",
        (unsigned long)path_audit.waypoint_count,
        (unsigned long)path_audit.valid_waypoints,
        (unsigned long)path_audit.first_invalid_waypoint,
        (long)path_audit.first_invalid_pad_id,
        (long)path_audit.first_invalid_group);
    for (guard_stan_index = 0U;
            guard_stan_index < ge_original_stage_guard_runtime_count(
                objects->guards); ++guard_stan_index) {
        GeOriginalStageGuardSnapshot guard;
        if (!ge_original_stage_guard_runtime_snapshot(
                objects->guards, guard_stan_index, &guard)) continue;
        fprintf(stream, "guard=%lu,%ld,%u,%u,%.3f,%.3f,%.3f,%u,%u,%.6f,%.6f\n",
            (unsigned long)guard_stan_index, (long)guard.chr_id,
            (unsigned)guard.room_id, (unsigned)guard.action_type,
            guard.position[0], guard.position[1], guard.position[2],
            (unsigned)guard.visible, (unsigned)guard.active_linked,
            guard.angle, guard.model_angle);
        if (guard.chr_record != NULL) {
            fprintf(stream,
                "guard_ai=%lu,%ld,%ld,%u,%u,%d,%08lx,%04x,%.2f,%ld,%ld,%.3f,%.3f,%u,%u,%u,%u\n",
                (unsigned long)guard_stan_index, (long)guard.chr_id,
                (long)guard.ai_list_id, (unsigned)guard.ai_offset,
                (unsigned)guard.ai_opcode, (int)guard.sleep,
                (unsigned long)guard.chr_flags, (unsigned)guard.hidden,
                guard.vision_range, (long)guard.last_seen_target_60,
                (long)guard.last_heard_target_60, guard.damage,
                guard.max_damage, (unsigned)guard.alertness,
                (unsigned)guard.morale, (unsigned)guard.stand_prestand,
                (unsigned)guard.stand_reaim);
            fprintf(stream, "guard_fire=%lu,%ld,%ld,%.6f\n",
                (unsigned long)guard_stan_index,
                (long)guard.firecount[0], (long)guard.firecount[1],
                guard.shotbondsum);
        }
    }
    for (guard_stan_index = 0U;
            guard_stan_index < objects->guard_weapon_count;
            ++guard_stan_index) {
        GeOriginalStageGuardWeaponSnapshot weapon;
        if (!ge_original_stage_guard_runtime_weapon_snapshot(
                objects->guards, guard_stan_index, &weapon)) continue;
        fprintf(stream, "guard_weapon=%lu,%ld,%ld,%ld,%u,%u\n",
            (unsigned long)guard_stan_index,
            (long)weapon.owner_chr_id, (long)weapon.weapon_id,
            (long)weapon.model_id, (unsigned)weapon.hand,
            (unsigned)weapon.matrices_ready);
    }
    fprintf(stream, "gun_ticks=%llu\n", (unsigned long long)gun.ticks);
    fprintf(stream, "pp7=%lu,%lu,%lu,%lu,%lu\n",
        (unsigned long)pp7.pp7_shots,
        (unsigned long)pp7.stan_hits,
        (unsigned long)pp7.clear_stan_paths,
        (unsigned long)pp7.guard_hits_registered,
        (unsigned long)pp7.guard_damage_applied);
    fprintf(stream, "pp7_object=%lu,%lu,%lu,%ld,%u\n",
        (unsigned long)pp7.object_hits_registered,
        (unsigned long)pp7.object_damage_applied,
        (unsigned long)pp7.object_destroyed,
        (long)pp7.last_object_type,
        (unsigned)pp7.last_object_destroyed_level);
    fprintf(stream,
        "pp7_ray=%.3f,%.3f,%.3f,%.6f,%.6f,%.6f,%.3f,%.3f,%.3f\n",
        pp7.last_origin[0], pp7.last_origin[1], pp7.last_origin[2],
        pp7.last_direction[0], pp7.last_direction[1],
        pp7.last_direction[2], pp7.last_endpoint[0],
        pp7.last_endpoint[1], pp7.last_endpoint[2]);
    fprintf(stream,
        "guard_hit_test=%lu,%lu,%lu,%lu,%ld,%ld,%.3f,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu\n",
        (unsigned long)guard_hits.rays_tested,
        (unsigned long)guard_hits.bounding_sphere_hits,
        (unsigned long)guard_hits.registered_hits,
        (unsigned long)guard_hits.damage_applied,
        (long)guard_hits.last_guard_index,
        (long)guard_hits.last_hitpart, guard_hits.last_distance,
        (unsigned long)guard_hits.guard_candidates,
        (unsigned long)guard_hits.onscreen_gate_passes,
        (unsigned long)guard_hits.depth_gate_passes,
        (unsigned long)guard_hits.sphere_gate_passes,
        (unsigned long)guard_hits.valid_room_lists,
        (unsigned long)guard_hits.rendered_room_gate_passes,
        (unsigned long)guard_hits.fog_gate_passes,
        (unsigned long)guard_hits.near_fog_gate_passes,
        (unsigned long)guard_hits.frustum_gate_passes);
    fprintf(stream,
        "guard_hit_rooms=%ld,%lu,%lu,%ld,%ld,0x%08lx,%.3f,"
        "%u:%ld:%u,%u:%ld:%u,%u:%ld:%u,%u:%ld:%u\n",
        (long)guard_hits.observed_guard_index,
        (unsigned long)guard_hits.observed_guard_samples,
        (unsigned long)guard_hits.observed_guard_onscreen_samples,
        (long)guard_hits.observed_shared_current_room,
        (long)guard_hits.observed_shared_rooms_drawn,
        (unsigned long)guard_hits.observed_prop_flags,
        guard_hits.observed_prop_zdepth,
        (unsigned)guard_hits.observed_prop_rooms[0],
        (long)guard_hits.observed_chrai_rooms[0],
        (unsigned)guard_hits.observed_room_rendered[0],
        (unsigned)guard_hits.observed_prop_rooms[1],
        (long)guard_hits.observed_chrai_rooms[1],
        (unsigned)guard_hits.observed_room_rendered[1],
        (unsigned)guard_hits.observed_prop_rooms[2],
        (long)guard_hits.observed_chrai_rooms[2],
        (unsigned)guard_hits.observed_room_rendered[2],
        (unsigned)guard_hits.observed_prop_rooms[3],
        (long)guard_hits.observed_chrai_rooms[3],
        (unsigned)guard_hits.observed_room_rendered[3]);
    fprintf(stream,
        "combat_aim=%lu,%lu,%lu,%lu,%lu,%ld,%ld,%lu,%lu,%lu,%u,%u,%u\n",
        (unsigned long)runtime->aim_resolver_calls,
        (unsigned long)runtime->aim_target_found,
        (unsigned long)runtime->aim_matrix_ready,
        (unsigned long)runtime->aim_resolver_successes,
        (unsigned long)runtime->aim_command_samples,
        (long)runtime->last_aim_chr,
        (long)runtime->last_aim_guard_index,
        (unsigned long)runtime->last_aim_target_index,
        (unsigned long)runtime->last_aim_route_frame,
        (unsigned long)runtime->last_aim_dwell_remaining,
        (unsigned)runtime->last_aim_visible,
        (unsigned)runtime->last_aim_matrices_ready,
        (unsigned)runtime->last_aim_death_complete);
    fprintf(stream, "combat_aim_world=%.3f,%.3f,%.3f\n",
        runtime->last_aim_world[0], runtime->last_aim_world[1],
        runtime->last_aim_world[2]);
    fprintf(stream,
        "combat_aim_camera=%.3f,%.3f,%.3f,%.6f,%.6f,%.6f\n",
        runtime->last_aim_camera_position[0],
        runtime->last_aim_camera_position[1],
        runtime->last_aim_camera_position[2],
        runtime->last_aim_camera_look[0],
        runtime->last_aim_camera_look[1],
        runtime->last_aim_camera_look[2]);
    fprintf(stream, "combat_aim_command=%.6f,%.6f,%lu\n",
        runtime->last_aim_command_look[0],
        runtime->last_aim_command_look[1],
        (unsigned long)runtime->last_aim_held);
    fprintf(stream, "combat_aim_model=%08lx,%.3f,%.3f\n",
        (unsigned long)runtime->last_aim_prop_flags,
        runtime->last_aim_prop_zdepth,
        runtime->last_aim_model_size);
    fprintf(stream,
        "guard_los=%llu,%llu,%llu,%llu,%08lx,%08lx,%08lx,%u,%ld,%.3f,%.3f,%.3f,%.3f,%.6f\n",
        (unsigned long long)guard_los.calls,
        (unsigned long long)guard_los.clear_results,
        (unsigned long long)guard_los.blocked_results,
        (unsigned long long)guard_los.destination_tile_matches,
        (unsigned long)(uintptr_t)guard_los.last_start_tile,
        (unsigned long)(uintptr_t)guard_los.last_result_tile,
        (unsigned long)(uintptr_t)guard_los.last_collision_prop,
        (unsigned)guard_los.last_collision_prop_type,
        (long)guard_los.last_cdtypes, guard_los.last_start_x,
        guard_los.last_start_z, guard_los.last_destination_x,
        guard_los.last_destination_z, guard_los.last_fraction);
    fprintf(stream,
        "guard_los_shortest=%llu,%.3f,%ld,%08lx,%08lx,%08lx,%08lx,%u,%.3f,%.3f,%.3f,%.3f,%.6f\n",
        (unsigned long long)guard_los.shortest_calls,
        guard_los.shortest_distance_squared,
        (long)guard_los.shortest_result,
        (unsigned long)(uintptr_t)guard_los.shortest_start_tile,
        (unsigned long)(uintptr_t)guard_los.shortest_result_tile,
        (unsigned long)(uintptr_t)guard_los.shortest_player_tile,
        (unsigned long)(uintptr_t)guard_los.shortest_collision_prop,
        (unsigned)guard_los.shortest_collision_prop_type,
        guard_los.shortest_start_x, guard_los.shortest_start_z,
        guard_los.shortest_destination_x,
        guard_los.shortest_destination_z,
        guard_los.shortest_fraction);
    fprintf(stream, "guard_sight_checks=%llu,%llu,%llu,%llu,%ld\n",
        (unsigned long long)guard_los.sight_check_calls,
        (unsigned long long)guard_los.sight_check_passes,
        (unsigned long long)guard_los.chr7_sight_check_calls,
        (unsigned long long)guard_los.chr7_sight_check_passes,
        (long)guard_los.last_sight_check_chr);
    fprintf(stream, "guard_stopped_checks=%llu,%llu,%llu,%llu\n",
        (unsigned long long)guard_los.stopped_check_calls,
        (unsigned long long)guard_los.stopped_check_passes,
        (unsigned long long)guard_los.chr7_stopped_check_calls,
        (unsigned long long)guard_los.chr7_stopped_check_passes);
    fprintf(stream, "guard_action_ticks=%llu,%llu\n",
        (unsigned long long)guard_los.action_tick_calls,
        (unsigned long long)guard_los.chr7_action_tick_calls);
    fprintf(stream, "guard_ai_unknown=%llu,%llu,%ld,%ld,%u,%08lx\n",
        (unsigned long long)guard_los.unknown_opcode_calls,
        (unsigned long long)guard_los.chr7_unknown_opcode_calls,
        (long)guard_los.last_unknown_chr,
        (long)guard_los.last_unknown_offset,
        (unsigned)guard_los.last_unknown_opcode,
        (unsigned long)(uintptr_t)guard_los.last_unknown_list);
    fprintf(stream, "guard_ai_unknown_opcodes=");
    bool first_unknown_opcode = true;
    for (size_t opcode = 0; opcode < 256U; ++opcode) {
        if (guard_los.unknown_opcode_histogram[opcode] == 0U) continue;
        fprintf(stream, "%s%lu:%llu",
            first_unknown_opcode ? "" : ",",
            (unsigned long)opcode,
            (unsigned long long)guard_los.unknown_opcode_histogram[opcode]);
        first_unknown_opcode = false;
    }
    fputc('\n', stream);
    fprintf(stream, "guard_combat=%llu,%llu,%llu,%.3f,%.3f\n",
        (unsigned long long)objects->guard_weapon_fire_dispatches,
        (unsigned long long)objects->guard_weapon_sound_starts,
        (unsigned long long)objects->guard_player_damage_events,
        objects->guard_player_health_damage,
        objects->guard_player_armour_damage);
    fprintf(stream, "player_combat=%.6f,%.6f,%.6f,%.6f,%ld,%u,%u\n",
        player_combat.health, player_combat.armour,
        player_combat.actual_health, player_combat.actual_armour,
        (long)player_combat.damage_show_time,
        (unsigned)player_combat.dead, (unsigned)player_combat.invincible);
    fprintf(stream, "armour_probe=%.6f,%lu\n",
        runtime->maximum_player_armour,
        (unsigned long)runtime->first_armour_frame);
    fprintf(stream,
        "aim=%.3f,%.3f,%.5f,%.5f,%08lx,%08lx,%ld,%ld,%.1f,%.1f,%.1f,%.1f\n",
        aim.crosshair[0], aim.crosshair[1], aim.autoaim[0], aim.autoaim[1],
        (unsigned long)(uintptr_t)aim.target_x,
        (unsigned long)(uintptr_t)aim.target_y,
        (long)aim.target_time_x, (long)aim.target_time_y,
        aim.screen_left, aim.screen_top, aim.screen_width,
        aim.screen_height);
    fprintf(stream, "modem=%lu,%lu,%lu\n",
        (unsigned long)modem.throw_attempts,
        (unsigned long)modem.successful_throws,
        (unsigned long)modem.pose_rejections);
    fprintf(stream, "sound=%lu,%lu,%lu,%lu\n",
        (unsigned long)services.sound_play_calls,
        (unsigned long)services.decoded_sound_starts,
        (unsigned long)services.sound_decode_failures,
        (unsigned long)services.active_sounds);
    ge_original_music_port_snapshot(&music_port);
    fprintf(stream, "music_port=%llu,%ld,%ld\n",
        (unsigned long long)music_port.unavailable_play_requests,
        (long)music_port.last_layer, (long)music_port.last_track);
    fprintf(stream,
        "music_layers=%ld,%u,%u;%ld,%u,%u;%ld,%u,%u\n",
        (long)music_port.layer_track[0], music_port.layer_volume[0],
        music_port.layer_fading[0],
        (long)music_port.layer_track[1], music_port.layer_volume[1],
        music_port.layer_fading[1],
        (long)music_port.layer_track[2], music_port.layer_volume[2],
        music_port.layer_fading[2]);
    fprintf(stream, "ndsp=%u,%08lx,%llu,%llu\n",
        ge_3ds_audio_is_active() ? 1U : 0U,
        (unsigned long)(uint32_t)ge_3ds_audio_last_error(),
        (unsigned long long)(audio_refill != NULL
            ? audio_refill->blocks_prepared : 0U),
        (unsigned long long)(audio_refill != NULL
            ? audio_refill->silent_frames : 0U));
    fprintf(stream, "frame_average_ms=%llu\n",
        (unsigned long long)(runtime->displayed_frames != 0U
            ? runtime->displayed_total_ms / runtime->displayed_frames : 0U));
    fprintf(stream, "gun_sight=%u,%llu,%llu,%llu,%u\n",
        gun_sight_texture_loaded ? 1U : 0U,
        (unsigned long long)gun_sight_frames,
        (unsigned long long)gun_sight_visible_frames,
        (unsigned long long)gun_sight_failures,
        (unsigned)gun_sight_suppression);
    fprintf(stream, "first_person_phases=%llu,%llu,%llu,%llu;%llu,%llu,%llu,%llu\n",
        (unsigned long long)fine_profile.first_person_phase_ticks[0],
        (unsigned long long)fine_profile.first_person_phase_ticks[1],
        (unsigned long long)fine_profile.first_person_phase_ticks[2],
        (unsigned long long)fine_profile.first_person_phase_ticks[3],
        (unsigned long long)fine_profile.first_person_peak_phase_ticks[0],
        (unsigned long long)fine_profile.first_person_peak_phase_ticks[1],
        (unsigned long long)fine_profile.first_person_peak_phase_ticks[2],
        (unsigned long long)fine_profile.first_person_peak_phase_ticks[3]);
    fprintf(stream, "guard_visibility_ticks=%llu,%llu\n",
        (unsigned long long)fine_profile.guard_visibility_update_ticks,
        (unsigned long long)fine_profile.guard_visibility_publish_ticks);
    fprintf(stream, "guard_refresh_peak=%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",
        (unsigned long long)fine_profile.guard_refresh_peak[0],
        (unsigned long long)fine_profile.guard_refresh_peak[1],
        (unsigned long long)fine_profile.guard_refresh_peak[2],
        (unsigned long long)fine_profile.guard_refresh_peak[3],
        (unsigned long long)fine_profile.guard_refresh_peak[4],
        (unsigned long long)fine_profile.guard_refresh_peak[5],
        (unsigned long long)fine_profile.guard_refresh_peak[6]);
    fprintf(stream, "frame_peak_ms=%llu\n",
        (unsigned long long)runtime->displayed_peak_ms);
    fprintf(stream, "frame_tail_ms=%llu,%lu,%lu,%lu,%lu,%lu\n",
        (unsigned long long)runtime->displayed_peak_after_warmup_ms,
        (unsigned long)runtime->displayed_samples_after_warmup,
        (unsigned long)runtime->displayed_over_16_ms,
        (unsigned long)runtime->displayed_over_25_ms,
        (unsigned long)runtime->displayed_over_33_ms,
        (unsigned long)runtime->displayed_over_50_ms);
    fprintf(stream,
        "overlay_refresh=%llu,%llu,%llu,%llu,%llu,%llu,%u,%llu,%llu,%u\n",
        (unsigned long long)objects->overlay_full_rebuilds,
        (unsigned long long)objects->door_overlay_refresh_failures,
        (unsigned long long)objects->guard_overlay_refresh_failures,
        (unsigned long long)objects->monitor_overlay_refresh_failures,
        (unsigned long long)objects->preview->dynamic_scene
            .overlay_update_successes,
        (unsigned long long)objects->preview->dynamic_scene
            .overlay_update_failures,
        (unsigned)objects->overlay_status,
        (unsigned long long)objects->guard_topology_replace_successes,
        (unsigned long long)objects->guard_topology_replace_failures,
        (unsigned)objects->last_guard_topology_replace_status);
    for (guard_stan_index = 0U;
            guard_stan_index < runtime->slow_frame_count;
            ++guard_stan_index) {
        const RuntimeInputProbeSlowFrame *slow =
            &runtime->slow_frames[guard_stan_index];
        fprintf(stream,
            "slow_frame=%lu,%u,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",
            (unsigned long)slow->displayed_frame, (unsigned)slow->room,
            (unsigned long long)slow->total_ms,
            (unsigned long long)slow->simulation_ms,
            (unsigned long long)slow->overlay_ms,
            (unsigned long long)slow->camera_ms,
            (unsigned long long)slow->first_person_ms,
            (unsigned long long)slow->gpu_ms,
            (unsigned long long)slow->topology_rebuilds,
            (unsigned long long)slow->topology_component_misses,
            (unsigned long long)slow->scene_generations,
            (unsigned long long)slow->overlay_full_rebuilds);
    }
    fprintf(stream,
        "frame_profile_ms=%llu,%llu,%llu,%llu,%llu,%llu,%lu\n",
        (unsigned long long)frame_profile.frame_ms,
        (unsigned long long)frame_profile.simulation_ms,
        (unsigned long long)frame_profile.overlay_ms,
        (unsigned long long)frame_profile.camera_ms,
        (unsigned long long)frame_profile.first_person_ms,
        (unsigned long long)frame_profile.gpu_ms,
        (unsigned long)frame_profile.samples);
    if (first_person_cache != NULL) {
        fprintf(stream, "first_person_topology=%llu,%llu,%llu,%llu\n",
            (unsigned long long)first_person_cache->build_attempts,
            (unsigned long long)first_person_cache->topology_rebuilds,
            (unsigned long long)first_person_cache->topology_reuses,
            (unsigned long long)first_person_cache->topology_publications);
        fprintf(stream, "first_person_components=%llu,%llu\n",
            (unsigned long long)first_person_cache->component_reuses,
            (unsigned long long)first_person_cache->component_decodes);
        fprintf(stream, "first_person_profile_ticks=%llu,%llu,%llu,%llu,%llu\n",
            (unsigned long long)first_person_cache->profile_build_ticks,
            (unsigned long long)first_person_cache->profile_input_topology_ticks,
            (unsigned long long)first_person_cache->profile_matrix_signature_ticks,
            (unsigned long long)first_person_cache->profile_vertex_transform_ticks,
            (unsigned long long)first_person_cache->profile_batch_publication_ticks);
    }
    fprintf(stream,
        "frame_profile_total_ms=%llu,%llu,%llu,%llu,%llu,%llu,%lu\n",
        (unsigned long long)(frame_profile_total.frame_ms
            + frame_profile.frame_ms),
        (unsigned long long)(frame_profile_total.simulation_ms
            + frame_profile.simulation_ms),
        (unsigned long long)(frame_profile_total.overlay_ms
            + frame_profile.overlay_ms),
        (unsigned long long)(frame_profile_total.camera_ms
            + frame_profile.camera_ms),
        (unsigned long long)(frame_profile_total.first_person_ms
            + frame_profile.first_person_ms),
        (unsigned long long)(frame_profile_total.gpu_ms
            + frame_profile.gpu_ms),
        (unsigned long)(frame_profile_total.samples
            + frame_profile.samples));
    {
        const uint64_t cache_ticks =
            objects->guard_scene_cache.profile_build_ticks;
        const uint64_t collect_ticks = fine_profile.guard_scene_ticks
                >= cache_ticks
            ? fine_profile.guard_scene_ticks - cache_ticks : 0U;
        fprintf(stream, "runtime_profile_tick_hz=%lu\n",
            (unsigned long)SYSCLOCK_ARM11);
        fprintf(stream,
            "guard_profile_ticks=%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",
            (unsigned long long)fine_profile.guard_matrix_ticks,
            (unsigned long long)fine_profile.guard_scene_ticks,
            (unsigned long long)collect_ticks,
            (unsigned long long)cache_ticks,
            (unsigned long long)objects->guard_scene_cache
                .profile_topology_ticks,
            (unsigned long long)objects->guard_scene_cache
                .profile_publication_signature_ticks,
            (unsigned long long)objects->guard_scene_cache
                .profile_matrix_quantization_ticks,
            (unsigned long long)objects->guard_scene_cache
                .profile_vertex_transform_ticks,
            (unsigned long long)objects->guard_scene_cache
                .profile_batch_publication_ticks,
            (unsigned long long)fine_profile.guard_overlay_commit_ticks,
            (unsigned long long)fine_profile.guard_gpu_upload_ticks);
        fprintf(stream,
            "guard_profile_calls=%llu,%llu,%llu,%llu,%llu,%llu\n",
            (unsigned long long)fine_profile.guard_matrix_calls,
            (unsigned long long)fine_profile.guard_scene_calls,
            (unsigned long long)objects->guard_scene_cache
                .profile_build_calls,
            (unsigned long long)fine_profile.guard_overlay_commit_calls,
            (unsigned long long)fine_profile.guard_gpu_upload_calls,
            (unsigned long long)objects->guard_scene_cache.cached_builds);
        fprintf(stream,
            "render_profile_ticks=%llu,%llu,%llu,%llu,%llu,%llu,%llu\n",
            (unsigned long long)fine_profile.world_gpu_flush_ticks,
            (unsigned long long)fine_profile.frame_begin_ticks,
            (unsigned long long)fine_profile.renderer_draw_ticks,
            (unsigned long long)fine_profile.frame_end_ticks,
            (unsigned long long)fine_profile.rendered_frames,
            (unsigned long long)fine_profile.world_gpu_flush_calls,
            (unsigned long long)fine_profile.world_gpu_flush_vertices);
        fprintf(stream, "guard_gpu_range_vertices=%llu,%llu,%llu\n",
            (unsigned long long)fine_profile.guard_gpu_upload_vertices,
            (unsigned long long)fine_profile.guard_gpu_full_upload_vertices,
            (unsigned long long)fine_profile.guard_gpu_uv_remap_vertices);
        fprintf(stream, "draw_profile_calls=%llu,%llu,%llu,%llu\n",
            (unsigned long long)fine_profile.world_draw_calls,
            (unsigned long long)fine_profile.world_authored_batches,
            (unsigned long long)fine_profile.first_person_draw_calls,
            (unsigned long long)fine_profile.first_person_authored_batches);
        fprintf(stream, "draw_profile_state=%llu,%llu,%llu,%llu,%llu,%llu\n",
            (unsigned long long)fine_profile.world_material_apply_calls,
            (unsigned long long)fine_profile.world_material_apply_reuses,
            (unsigned long long)fine_profile.world_texture_lookups,
            (unsigned long long)fine_profile.first_person_material_apply_calls,
            (unsigned long long)fine_profile.first_person_material_apply_reuses,
            (unsigned long long)fine_profile.first_person_texture_lookups);
        fprintf(stream, "draw_profile_prepare=%llu,%llu,%llu,%llu\n",
            (unsigned long long)fine_profile.world_material_prepare_hits,
            (unsigned long long)fine_profile.world_material_prepare_misses,
            (unsigned long long)fine_profile
                .first_person_material_prepare_hits,
            (unsigned long long)fine_profile
                .first_person_material_prepare_misses);
        fprintf(stream, "draw_profile_frustum=%llu,%llu,%llu\n",
            (unsigned long long)fine_profile.world_frustum_tests,
            (unsigned long long)fine_profile.world_frustum_culled_batches,
            (unsigned long long)fine_profile.world_frustum_culled_vertices);
        fprintf(stream, "draw_profile_bounds=%llu,%llu\n",
            (unsigned long long)fine_profile.world_frustum_bounds_inside,
            (unsigned long long)fine_profile.world_frustum_bounds_outside);
        fprintf(stream, "draw_profile_first_vertex=%llu\n",
            (unsigned long long)fine_profile.world_frustum_first_vertex_visible);
    }
    return fclose(stream) == 0;
}

typedef struct RuntimeFirstPersonScene {
    GeOriginalFirstPersonSceneCache cache;
    GeDamRoomWorldVertex *source_vertices;
    GeDamRoomDrawBatch *batches;
    GeDamCameraVertex *projected_vertices;
    RuntimeDamRenderBatch *render_batches;
    size_t vertex_count;
    size_t batch_count;
    size_t render_vertex_count;
    size_t render_batch_count;
    uint64_t generation;
    GeOriginalFirstPersonModel loaded_model;
    GeOriginalFirstPersonScene scene;
    GeDamCameraStatus camera_status;
    bool textures_ready;
    bool uv_ready;
    bool ready;
} RuntimeFirstPersonScene;

static void initialize_first_person_models(RuntimeFirstPersonModels *models,
                                           GeAssetPack *asset_pack)
{
    unsigned hand;
    memset(models, 0, sizeof(*models));
    models->status[0] = GE_ORIGINAL_FIRST_PERSON_ASSET_INVALID_ARGUMENT;
    models->status[1] = GE_ORIGINAL_FIRST_PERSON_ASSET_INVALID_ARGUMENT;
    if (asset_pack == NULL) return;
    for (hand = 0U; hand < 2U; ++hand) {
        models->buffers[hand] = malloc(
            GE_ORIGINAL_FIRST_PERSON_HAND_BUFFER_SIZE);
        if (models->buffers[hand] == NULL) return;
    }
    if (ge_original_first_person_assets_init(
            &models->assets, asset_pack,
            models->buffers[0], GE_ORIGINAL_FIRST_PERSON_HAND_BUFFER_SIZE,
            models->buffers[1], GE_ORIGINAL_FIRST_PERSON_HAND_BUFFER_SIZE)
            != GE_ORIGINAL_FIRST_PERSON_ASSET_OK) return;
    models->status[0] = ge_original_first_person_assets_load_item_native(
        &models->assets, 0U, ITEM_WPPK, &models->headers[0]);
    models->status[1] = ge_original_first_person_assets_load_item_native(
        &models->assets, 1U, ITEM_WPPKSIL, &models->headers[1]);
    models->ready = models->status[0] == GE_ORIGINAL_FIRST_PERSON_ASSET_OK
        && models->status[1] == GE_ORIGINAL_FIRST_PERSON_ASSET_OK;
    if (models->ready)
        ge_original_first_person_assets_bind_loader(
            &models->assets, &models->loader);
}

static void close_first_person_models(RuntimeFirstPersonModels *models)
{
    ge_original_first_person_assets_bind_loader(NULL, NULL);
    ge_original_first_person_assets_close(&models->assets);
    free(models->buffers[0]);
    free(models->buffers[1]);
    memset(models, 0, sizeof(*models));
}

static void sync_first_person_pose_model(RuntimeFirstPersonModels *models)
{
    int32_t item;
    void *header = NULL;
    unsigned asset_slot = 0U;

    if (models == NULL || !models->ready || !models->pose.initialized) return;
    item = ge_original_first_person_pose_current_item(0U);
    if (item == models->pose.model_item[0]) return;
    if (!ge_original_first_person_assets_supports_item(item)) return;
    models->status[0] = ge_original_first_person_assets_acquire_item_native(
        &models->assets, 0U, item, &header, &asset_slot);
    if (models->status[0] != GE_ORIGINAL_FIRST_PERSON_ASSET_OK
            || asset_slot >= 2U || header == NULL) return;
    models->headers[asset_slot] = header;
    models->pose_status = ge_original_first_person_pose_bind_hand_model(
        0U, item, header);
}

static void bind_first_person_loadout(RuntimeFirstPersonModels *models)
{
    unsigned hand;
    if (models == NULL || !models->ready
            || !models->bond_live.initialized) return;
    for (hand = 0U; hand < 2U; ++hand) {
        const int32_t item = models->bond_live.loadout.starting_weapon[hand];
        void *header = NULL;
        unsigned asset_slot = hand;
        if (!ge_original_first_person_assets_supports_item(item)) continue;
        models->status[hand] =
            ge_original_first_person_assets_acquire_loadout_hand(
                &models->assets,
                models->bond_live.loadout.starting_weapon,
                hand, &header, &asset_slot);
        if (models->status[hand] != GE_ORIGINAL_FIRST_PERSON_ASSET_OK
                || asset_slot >= 2U || header == NULL) continue;
        models->headers[asset_slot] = header;
        models->pose_status = ge_original_first_person_pose_bind_hand_model(
            hand, item, header);
    }
}

static const float runtime_eye_space_identity[4][4] = {
    {1.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 1.0f},
};

static int ensure_first_person_scene_texture(void *context, uint16_t texture_id)
{
    const Ge3dsSceneTextureStatus status = ge_3ds_scene_textures_ensure_image(
        context, &first_person_scene_textures, texture_id);
    return status == GE_3DS_SCENE_TEXTURE_OK
        || status == GE_3DS_SCENE_TEXTURE_PARTIAL;
}

static bool update_first_person_scene(RuntimeFirstPersonModels *models,
                                      RuntimeFirstPersonScene *runtime,
                                      RuntimeDamPreview *dam_preview,
                                      GeTextureCache *texture_cache,
                                      Vertex *destination)
{
    GeDamRoomSceneStorage source_storage;
    Ge3dsSceneTextureStatus texture_status;
    GeOriginalFirstPersonModel loaded_model;
    const void *published_header;
    unsigned asset_slot = 0U;
    uint64_t unchanged_builds_before;
    uint64_t topology_publications_before;
    bool publication_unchanged;
    bool uv_updated = false;
    size_t batch_index;
    size_t vertex_index;
    uint64_t phase_ticks[5];

    if (models == NULL || runtime == NULL || dam_preview == NULL
            || texture_cache == NULL || destination == NULL
            || !models->ready || !dam_preview->original_camera_ready)
        return false;
    if (runtime->source_vertices == NULL)
        runtime->source_vertices = calloc(
            FIRST_PERSON_SOURCE_VERTEX_CAPACITY,
            sizeof(*runtime->source_vertices));
    if (runtime->batches == NULL)
        runtime->batches = calloc(FIRST_PERSON_BATCH_CAPACITY,
                                  sizeof(*runtime->batches));
    if (runtime->source_vertices == NULL || runtime->batches == NULL
            || (!runtime->cache.initialized
                && !ge_original_first_person_scene_cache_init(
                    &runtime->cache)))
        return false;
    source_storage = (GeDamRoomSceneStorage){
        runtime->source_vertices, FIRST_PERSON_SOURCE_VERTEX_CAPACITY,
        runtime->batches, FIRST_PERSON_BATCH_CAPACITY,
    };
    unchanged_builds_before = runtime->cache.unchanged_builds;
    phase_ticks[0] = svcGetSystemTick();
    topology_publications_before = runtime->cache.topology_publications;
    ge_original_first_person_scene_cache_bind_profile_clock(
        &runtime->cache, runtime_profile_clock, NULL);
    runtime->scene.status = ge_original_first_person_scene_build_cached(
        &runtime->cache, &models->assets, 0U,
        runtime_eye_space_identity,
        &source_storage, &runtime->scene);
    if (runtime->scene.status != GE_ORIGINAL_FIRST_PERSON_SCENE_OK) {
        runtime->ready = false;
        return false;
    }
    phase_ticks[1] = svcGetSystemTick();
    publication_unchanged = runtime->cache.unchanged_builds
        != unchanged_builds_before;
    runtime->vertex_count = runtime->scene.vertex_count;
    runtime->batch_count = runtime->scene.batch_count;
    published_header = models->pose.model_header[0];
    if (published_header == NULL
            || !ge_original_first_person_assets_slot_for_header(
                &models->assets, published_header, &asset_slot)
            || asset_slot >= 2U) return false;
    loaded_model = models->assets.loaded_model[asset_slot];
    if (runtime->cache.topology_publications != topology_publications_before) {
        /* Original reload/fire switches can change vertex order and texture
         * coordinates without selecting a different weapon resource. */
        runtime->uv_ready = false;
        if (runtime->textures_ready && runtime->loaded_model == loaded_model
                && !ge_original_model_scene_visit_textures(
                    runtime->batches, runtime->batch_count, texture_cache,
                    ensure_first_person_scene_texture)) return false;
    }
    if (!runtime->textures_ready || runtime->loaded_model != loaded_model) {
        ge_3ds_scene_textures_close(&first_person_scene_textures);
        memset(first_person_texture_slots, 0,
               sizeof(first_person_texture_slots));
        texture_status = ge_3ds_scene_textures_load(
            texture_cache, runtime->batches, runtime->batch_count,
            first_person_texture_slots, FIRST_PERSON_TEXTURE_CAPACITY,
            &first_person_scene_textures);
        runtime->textures_ready = texture_status == GE_3DS_SCENE_TEXTURE_OK
            || texture_status == GE_3DS_SCENE_TEXTURE_PARTIAL;
        runtime->loaded_model = loaded_model;
        runtime->uv_ready = false;
        if (!runtime->textures_ready) return false;
        /* Import the authored resource's inactive switch images when the
         * weapon is first published, not on the first muzzle/reload switch. */
        if (!ge_original_first_person_assets_visit_texture_ids(
                &models->assets, asset_slot, texture_cache,
                ensure_first_person_scene_texture)) {
            runtime->textures_ready = false;
            return false;
        }
    }

    phase_ticks[2] = svcGetSystemTick();
    if (!runtime->uv_ready) {
        for (batch_index = 0U; batch_index < runtime->batch_count;
                ++batch_index) {
            const GeDamRoomDrawBatch *batch = &runtime->batches[batch_index];
            const Ge3dsSceneTextureSlot *slot = ge_3ds_scene_textures_find(
                &first_person_scene_textures, batch->texture.texture_id);
            if (slot == NULL) continue;
            for (vertex_index = batch->first_vertex;
                    vertex_index < batch->first_vertex + batch->vertex_count;
                    ++vertex_index) {
                GeTextureUv uv;
                if (ge_3ds_scene_texture_map_uv(
                        slot,
                        runtime->source_vertices[vertex_index]
                            .source.texture_s,
                        runtime->source_vertices[vertex_index]
                            .source.texture_t,
                        &batch->material, &uv) == GE_TEXTURE_UV_OK) {
                    destination[vertex_index].u = uv.u;
                    destination[vertex_index].v = uv.v;
                }
            }
        }
        runtime->uv_ready = true;
        uv_updated = true;
    }

    phase_ticks[3] = svcGetSystemTick();
    runtime->camera_status = GE_DAM_CAMERA_OK;
    runtime->render_vertex_count = runtime->vertex_count;
    runtime->render_batch_count = runtime->batch_count;
    runtime->generation = runtime->scene.generation;
    runtime->ready = true;
    for (vertex_index = 0U; !(publication_unchanged && !uv_updated)
            && vertex_index < runtime->vertex_count;
            ++vertex_index) {
        const GeDamRoomWorldVertex *source =
            &runtime->source_vertices[vertex_index];
        destination[vertex_index].x = source->world[0];
        destination[vertex_index].y = source->world[1];
        destination[vertex_index].z = source->world[2];
        /* Cached first-person RGBA is immutable display-list data, like UVs.
         * Pose changes only republish positions; every layout switch above
         * invalidates UVs and therefore republishes these colors as well. */
        if (uv_updated) {
            destination[vertex_index].r = (float)source->processed.rgba[0] / 255.0f;
            destination[vertex_index].g = (float)source->processed.rgba[1] / 255.0f;
            destination[vertex_index].b = (float)source->processed.rgba[2] / 255.0f;
            destination[vertex_index].a = (float)source->processed.rgba[3] / 255.0f;
        }
    }
    phase_ticks[4] = svcGetSystemTick();
    for (batch_index = 0U; batch_index < 4U; ++batch_index)
        fine_profile.first_person_phase_ticks[batch_index] +=
            phase_ticks[batch_index + 1U] - phase_ticks[batch_index];
    if (topology_publications_before != 0U
            && phase_ticks[4] - phase_ticks[0] > fine_profile.first_person_peak_ticks) {
        fine_profile.first_person_peak_ticks = phase_ticks[4] - phase_ticks[0];
        for (batch_index = 0U; batch_index < 4U; ++batch_index)
            fine_profile.first_person_peak_phase_ticks[batch_index] =
                phase_ticks[batch_index + 1U] - phase_ticks[batch_index];
    }
    return !publication_unchanged || uv_updated;
}

static void close_first_person_scene(RuntimeFirstPersonScene *runtime)
{
    if (runtime == NULL) return;
    ge_3ds_scene_textures_close(&first_person_scene_textures);
    ge_original_first_person_scene_cache_close(&runtime->cache);
    free(runtime->projected_vertices);
    free(runtime->render_batches);
    free(runtime->batches);
    free(runtime->source_vertices);
    memset(runtime, 0, sizeof(*runtime));
}

typedef struct RuntimeDamIntroContext {
    RuntimeDamCollision *collision;
    RuntimeDamIntro *intro;
    GeOriginalPropState *props;
    stagesetup *setup;
    int32_t stage_id;
    int32_t demo_slot;
} RuntimeDamIntroContext;

static stagesetup *dam_intro_load_setup(void *context, int32_t stage_id)
{
    RuntimeDamIntroContext *intro = context;
    return intro != NULL && stage_id == intro->stage_id
        ? intro->setup : NULL;
}

static float dam_intro_room_scale_reciprocal(void *context)
{
    RuntimeDamIntroContext *intro = context;
    return intro->collision->native.inverse_level_scale;
}

static void *dam_intro_resolve_stan(void *context, const char *name)
{
    RuntimeDamIntroContext *intro = context;
    return ge_original_stan_match_tile_name(&intro->collision->native, name);
}

static int32_t dam_intro_demo_slot(void *context)
{
    const RuntimeDamIntroContext *intro = context;
    return intro != NULL ? intro->demo_slot : 0;
}

static float dam_intro_floor_y(void *context, void *stan, float x, float z)
{
    RuntimeDamIntroContext *intro = context;
    return ge_original_stan_get_position_y(&intro->collision->native,
        (const GeStanNativeTile *)stan, x, z);
}

static float dam_intro_eye_height(void *context)
{
    (void)context;
    /* bondviewPlayerBeginLife: 185 * the normal 1.0 perspective - 10. */
    return 175.0f;
}

static void *dam_intro_allocate_player_prop(void *context)
{
    RuntimeDamIntroContext *intro = context;
    return ge_original_prop_state_allocate_player(intro->props);
}

static void dam_intro_activate_player_prop(void *context, void *prop)
{
    RuntimeDamIntroContext *intro = context;
    ge_original_prop_state_activate(intro->props, prop);
}

static void dam_intro_enable_player_prop(void *context, void *prop)
{
    RuntimeDamIntroContext *intro = context;
    ge_original_prop_state_enable(intro->props, prop);
}

static void dam_intro_deregister_player_room(void *context, void *prop,
                                             int16_t room)
{
    /* Reset establishes registeredroom=-1; the original deregister body also
     * returns immediately for this one spawn-time call. */
    (void)context;
    (void)prop;
    (void)room;
}

static void dam_intro_register_player_room(void *context, void *prop,
                                           int16_t room)
{
    RuntimeDamIntroContext *intro = context;
    ge_original_prop_state_register_room(intro->props, prop, room);
}

static int32_t dam_intro_commit_player_spawn(
    void *context, const float position[3], float floor_y,
    float eye_height, float look_angle_radians, void *stan)
{
    RuntimeDamIntroContext *intro = context;
    const GeStanNativeTile *tile = stan;
    GeOriginalPlayerSpawnConfig config = {
        .position = {position[0], position[1], position[2]},
        .floor_y = floor_y,
        .eye_height = eye_height,
        .look_angle_radians = look_angle_radians,
        .stan = stan,
        .room = tile != NULL ? (int16_t)tile->room : -1,
    };
    (void)intro;
    return ge_original_player_spawn_commit(&config);
}

static void initialize_original_stage_intro(RuntimeDamCollision *collision,
                                            RuntimeDamIntro *intro,
                                            GeOriginalPropState *props,
                                            stagesetup *setup,
                                            int32_t stage_id,
                                            bool demo_playback)
{
    RuntimeDamIntroContext context = {
        collision, intro, props, setup, stage_id,
        demo_playback ? 1 : 0
    };
    GeOriginalSetupPadProviders setup_providers = {
        .context = &context,
        .load_setup = dam_intro_load_setup,
        .get_room_scale_reciprocal = dam_intro_room_scale_reciprocal,
        .resolve_stan = dam_intro_resolve_stan,
    };
    GeOriginalIntroProviders intro_providers = {
        .context = &context,
        .get_demo_slot = dam_intro_demo_slot,
        .get_floor_y = dam_intro_floor_y,
        .get_eye_height = dam_intro_eye_height,
        .commit_player_spawn = dam_intro_commit_player_spawn,
    };
    GeOriginalPlayerSpawnProviders player_providers = {
        .context = &context,
        .allocate_prop = dam_intro_allocate_player_prop,
        .activate_prop = dam_intro_activate_player_prop,
        .enable_prop = dam_intro_enable_player_prop,
        .deregister_room = dam_intro_deregister_player_room,
        .register_room = dam_intro_register_player_room,
    };

    memset(intro, 0, sizeof(*intro));
    if (collision == NULL || !collision->original_bound || setup == NULL)
        return;
    ge_original_setup_pad_bind(&setup_providers, &intro->setup);
    ge_original_setup_pad_load(stage_id);
    ge_original_player_spawn_bind(&player_providers, &intro->player);
    ge_original_bond_intro_bind(&intro_providers, &intro->spawn);
    bondviewLoadSetupIntroSpawnSlice();
    if (intro->spawn.player_committed) {
        ge_original_spawn_player_initialize_idle_roll();
        GeOriginalBondMovementProviders movement_providers = {
            .collision_types =
                ge_original_bond_movement_normal_collision_types,
            .set_prop_collision =
                ge_original_bond_movement_set_current_player_collision,
        };
        ge_original_bond_movement_bind(&movement_providers,
                                       &intro->movement);
    }
}

static void *dam_world_allocate_definition(void *context, uint8_t type,
                                           size_t size_bytes)
{
    RuntimeDamWorldObjects *objects = context;
    void *definition;

    (void)type;
    if (objects->definition_count >= DAM_NATIVE_OBJECT_CAPACITY)
        return NULL;
    definition = calloc(1U, size_bytes);
    if (definition != NULL)
        objects->definitions[objects->definition_count++] = definition;
    return definition;
}

static void *dam_world_allocate_prop(void *context, void *definition)
{
    RuntimeDamWorldObjects *objects = context;
    return ge_original_prop_state_allocate(&objects->props, definition);
}

static void dam_world_activate_prop(void *context, void *prop)
{
    RuntimeDamWorldObjects *objects = context;
    ge_original_prop_state_activate(&objects->props, prop);
}

static void dam_world_enable_prop(void *context, void *prop)
{
    RuntimeDamWorldObjects *objects = context;
    ge_original_prop_state_enable(&objects->props, prop);
}

static void dam_world_register_room(void *context, void *prop, int16_t room)
{
    RuntimeDamWorldObjects *objects = context;
    ge_original_prop_state_register_room(&objects->props, prop, room);
}

static RuntimeDamAlarmObject *dam_alarm_for_definition(
    RuntimeDamWorldObjects *objects, const void *definition)
{
    size_t index;
    if (objects == NULL || definition == NULL) return NULL;
    for (index = 0U; index < DAM_ALARM_OBJECT_COUNT; ++index)
        if (objects->alarms[index].definition == definition)
            return &objects->alarms[index];
    return NULL;
}

static int dam_alarm_construct_standard(
    void *context, void *definition, int32_t command_index)
{
    RuntimeDamWorldObjects *objects = context;
    RuntimeDamAlarmObject *alarm = dam_alarm_for_definition(
        objects, definition);
    if (alarm == NULL || alarm->command_index != (size_t)command_index)
        return 0;
    alarm->prop = ge_original_prop_state_allocate(&objects->props, definition);
    if (alarm->prop == NULL) return 0;
    if (!ge_original_prop_state_bind_object(definition, alarm->prop)) return 0;
    ge_original_default_object_bind(
        &objects->object_providers, &alarm->prepared);
    alarm->construct_status = ge_original_default_object_construct_standard(
        definition, command_index);
    return alarm->construct_status == GE_ORIGINAL_DEFAULT_OBJECT_OK;
}

static int dam_alarm_place_standard(void *context, void *definition)
{
    RuntimeDamAlarmObject *alarm = dam_alarm_for_definition(
        context, definition);
    if (alarm == NULL || alarm->prop == NULL) return 0;
    alarm->placement_status = ge_original_default_object_place_standard(
        definition);
    return alarm->placement_status == GE_ORIGINAL_DEFAULT_OBJECT_OK;
}

static int dam_alarm_update_room(void *context, void *definition)
{
    RuntimeDamWorldObjects *objects = context;
    RuntimeDamAlarmObject *alarm = dam_alarm_for_definition(
        objects, definition);
    if (alarm == NULL || alarm->prop == NULL) return 0;
    if (!ge_original_prop_state_set_primary_room(
            alarm->prop, (int16_t)alarm->room)) return 0;
    ge_original_prop_state_register_room(
        &objects->props, alarm->prop, (int16_t)alarm->room);
    return ge_original_prop_state_room_contains(
        (int16_t)alarm->room, alarm->prop);
}

static int dam_alarm_activate_prop(void *context, void *prop)
{
    RuntimeDamWorldObjects *objects = context;
    if (objects == NULL || prop == NULL) return 0;
    ge_original_prop_state_activate(&objects->props, prop);
    return ge_original_prop_state_is_active(prop);
}

static int dam_alarm_enable_prop(void *context, void *prop)
{
    RuntimeDamWorldObjects *objects = context;
    if (objects == NULL || prop == NULL) return 0;
    ge_original_prop_state_enable(&objects->props, prop);
    return ge_original_prop_state_is_enabled(prop);
}

static bool materialize_dam_alarm_objects(
    RuntimeDamWorldObjects *objects,
    const GeOriginalStageSetupRuntime *setup)
{
    GeOriginalStageMiscProviders providers = {0};
    size_t command_index;
    if (objects == NULL || setup == NULL || objects->pitem_models == NULL)
        return false;
    providers.context = objects;
    providers.construct_standard = dam_alarm_construct_standard;
    providers.place_standard = dam_alarm_place_standard;
    providers.update_room_position = dam_alarm_update_room;
    providers.activate_prop = dam_alarm_activate_prop;
    providers.enable_prop = dam_alarm_enable_prop;
    for (command_index = 0U; command_index < setup->prop_record_count;
            ++command_index) {
        GeOriginalStagePropConstructionRequest request;
        RuntimeDamAlarmObject *alarm;
        size_t definition_size;
        if (setup->prop_records[command_index].type != PROPDEF_ALARM)
            continue;
        ++objects->alarm_scan_count;
        if (objects->alarm_count >= DAM_ALARM_OBJECT_COUNT
                || !ge_original_stage_prop_construction_request(
                    setup, command_index, &request)) {
            objects->alarm_materialize_failure = 1U;
            return false;
        }
        alarm = &objects->alarms[objects->alarm_count];
        definition_size = ge_original_stage_prop_native_definition_size(
            &request);
        alarm->definition = dam_world_allocate_definition(
            objects, request.record->type, definition_size);
        if (alarm->definition == NULL) {
            objects->alarm_materialize_failure = 2U;
            return false;
        }
        alarm->command_index = command_index;
        alarm->model_id = request.model_id;
        alarm->room = (uint8_t)request.placement.room;
        alarm->construct_status = GE_ORIGINAL_DEFAULT_OBJECT_INVALID_ARGUMENT;
        alarm->placement_status =
            GE_ORIGINAL_DEFAULT_OBJECT_NOT_CONSTRUCTED;
        alarm->status = ge_original_stage_misc_construct_exact(
            &request, alarm->definition, definition_size,
            &providers, &alarm->instance);
        if (alarm->status != GE_ORIGINAL_STAGE_MISC_OK) {
            objects->alarm_materialize_failure = 3U;
            return false;
        }
        if (alarm->instance.prop != alarm->prop
                || alarm->instance.model == NULL
                || !alarm->instance.constructed
                || !alarm->instance.activated) {
            objects->alarm_materialize_failure = 4U;
            return false;
        }
        alarm->live = true;
        ++objects->alarm_count;
    }
    return objects->alarm_count == DAM_ALARM_OBJECT_COUNT;
}

static void *dam_objective_definition_by_command(
    void *context, size_t command_index,
    const GeOriginalStagePropRecord *record)
{
    RuntimeDamWorldObjects *objects = context;
    size_t index;
    (void)record;
    if (objects == NULL) return NULL;
    for (index = 0U; index < objects->alarm_count; ++index)
        if (objects->alarms[index].command_index == command_index)
            return objects->alarms[index].definition;
    if ((size_t)objects->mission_tags.tag5_object.command_index
            == command_index)
        return objects->mission_tags.tag5_object.definition;
    if ((size_t)objects->mission_tags.tag4_object.command_index
            == command_index)
        return objects->mission_tags.tag4_object.definition;
    return NULL;
}

static bool initialize_dam_objectives(RuntimeDamWorldObjects *objects)
{
    GeOriginalStageObjectiveProviders providers = {0};
    GeOriginalStageObjectiveRuntimeProviders runtime_providers = {0};
    if (objects == NULL || objects->setup == NULL) return false;
    providers.context = objects;
    providers.object_definition_by_command =
        dam_objective_definition_by_command;
    if (ge_original_stage_objectives_build(
            &objects->objectives, objects->setup, &providers)
            != GE_ORIGINAL_STAGE_OBJECTIVE_OK) return false;
    runtime_providers.context = g_CurrentPlayer;
    runtime_providers.prop_in_inventory =
        ge_original_stage_objective_prop_in_inventory_exact;
    runtime_providers.stage_flag_set =
        ge_original_stage_objective_stage_flag_set_exact;
    runtime_providers.key_analyzer_complete =
        ge_original_stage_objective_key_analyzer_complete_exact;
    runtime_providers.photograph_bounds_inside_view =
        ge_original_stage_objective_photograph_bounds_inside_view_exact;
    return ge_original_stage_objective_runtime_begin(
        &objects->objective_runtime, &objects->objectives,
        &runtime_providers) == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK;
}

static bool tick_dam_objectives(RuntimeDamWorldObjects *objects)
{
    size_t menu;
    size_t ready = 0U;
    size_t blocked = 0U;
    if (objects == NULL || !objects->objective_runtime.bound) return false;
    for (menu = 0U; menu < GE_ORIGINAL_STAGE_OBJECTIVE_MAX; ++menu) {
        GeOriginalStageObjectiveRuntimeStatus status;
        if (objects->objectives.objective_by_menu[menu] < 0) continue;
        status = ge_original_stage_objective_runtime_evaluate(
            &objects->objective_runtime, (uint8_t)menu,
            &objects->objective_evaluations[menu]);
        if (status == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK)
            ++ready;
        else if (status == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_BLOCKED)
            ++blocked;
        else
            return false;
    }
    objects->objective_evaluation_ready_count = ready;
    objects->objective_evaluation_blocked_count = blocked;
    ++objects->objective_evaluation_ticks;
    return true;
}

static void dam_alarm_play_sfx(void *context, uint32_t sfx_id)
{
    (void)context;
    ge_original_gameplay_services_play_sfx(sfx_id);
}

static void tick_dam_interaction(RuntimeDamWorldObjects *objects)
{
    GeOriginalDoorInteractionResult door_result;
    void *alarm_props[DAM_ALARM_OBJECT_COUNT];
    void *selected;
    size_t index;
    if (objects == NULL) return;
    ++objects->alarm_interaction_ticks;
    door_result = ge_original_door_interaction_tick();
    if (door_result != GE_ORIGINAL_DOOR_INTERACTION_RELOAD_REQUESTED)
        return;
    for (index = 0U; index < objects->alarm_count; ++index)
        alarm_props[index] = objects->alarms[index].prop;
    selected = ge_original_gameplay_services_find_interactable(
        alarm_props, objects->alarm_count);
    if (selected != NULL) {
        GeOriginalStageAlarmInteractionProviders providers = {0};
        providers.play_sfx = dam_alarm_play_sfx;
        if (ge_original_stage_alarm_interact_exact(
                selected, &providers, &objects->alarm_interaction)
                == GE_ORIGINAL_STAGE_ALARM_INTERACTION_OK)
            ++objects->alarm_interaction_activations;
    }
}

static int initialize_dam_prop_state(RuntimeDamWorldObjects *objects)
{
    size_t index;
    memset(objects, 0, sizeof(*objects));
    objects->model62_status = GE_ORIGINAL_MODEL62_INVALID_ARGUMENT;
    objects->model104_status = GE_ORIGINAL_MODEL104_INVALID_ARGUMENT;
    objects->model178_status = GE_ORIGINAL_MODEL178_INVALID_ARGUMENT;
    objects->objective_models_status =
        GE_ORIGINAL_DAM_OBJECTIVE_MODELS_INVALID_ARGUMENT;
    objects->alarm_interaction.status =
        GE_ORIGINAL_STAGE_ALARM_INTERACTION_INVALID_ARGUMENT;
    objects->glass_object_status = GE_ORIGINAL_DEFAULT_OBJECT_INVALID_ARGUMENT;
    objects->glass_placement_status = GE_ORIGINAL_DEFAULT_OBJECT_NOT_CONSTRUCTED;
    objects->default_object_status = GE_ORIGINAL_DEFAULT_OBJECT_INVALID_ARGUMENT;
    objects->placement_status = GE_ORIGINAL_DEFAULT_OBJECT_NOT_CONSTRUCTED;
    for (index = 0U; index < DAM_MISSION_TAG_OBJECT_COUNT; ++index) {
        objects->mission_object_status[index] =
            GE_ORIGINAL_DEFAULT_OBJECT_INVALID_ARGUMENT;
        objects->mission_placement_status[index] =
            GE_ORIGINAL_DEFAULT_OBJECT_NOT_CONSTRUCTED;
    }
    objects->door_status[0] = GE_ORIGINAL_DOOR_INVALID_ARGUMENT;
    objects->door_status[1] = GE_ORIGINAL_DOOR_INVALID_ARGUMENT;
    for (index = 0U; index < GE_ORIGINAL_DAM_SPAWN_WINDOW_COUNT; ++index) {
        objects->spawn_window_status[index] =
            GE_ORIGINAL_DEFAULT_OBJECT_INVALID_ARGUMENT;
        objects->spawn_window_placement_status[index] =
            GE_ORIGINAL_DEFAULT_OBJECT_NOT_CONSTRUCTED;
    }
    return ge_original_prop_state_reset(&objects->props, 137U);
}

static int32_t dam_world_model_load(void *context, int32_t model_id)
{
    RuntimeDamWorldObjects *objects = context;
    if ((model_id == GE_ORIGINAL_MODEMBOX_MODEL_ID
            || model_id == GE_ORIGINAL_SATDISH_MODEL_ID)
            && objects->objective_models != NULL) {
        return ge_original_dam_objective_models_model_load(
            objects->objective_models, model_id);
    }
    if (model_id == GE_ORIGINAL_MODEL104_ID) {
        size_t index;
        for (index = 0U; index < DAM_WINDOW_MODEL_INSTANCE_COUNT; ++index)
            if (!ge_original_model104_model_load(
                    objects->model104[index], model_id)) return 0;
        return 1;
    }
    if (model_id == GE_ORIGINAL_MODEL178_ID)
        return ge_original_model178_model_load(objects->model178[0], model_id)
            && ge_original_model178_model_load(objects->model178[1], model_id);
    if (model_id == GE_ORIGINAL_MODEL62_ID)
        return ge_original_model62_model_load(objects->model62, model_id);
    return objects->pitem_models != NULL
        ? ge_original_pitem_model_load(objects->pitem_models, model_id) : 0;
}

static int dam_world_resolve_model(void *context, int32_t model_id,
                                   void **model_header,
                                   void **model_instance,
                                   float *pitem_scale)
{
    RuntimeDamWorldObjects *objects = context;
    if ((model_id == GE_ORIGINAL_MODEMBOX_MODEL_ID
            || model_id == GE_ORIGINAL_SATDISH_MODEL_ID)
            && objects->objective_models != NULL) {
        return ge_original_dam_objective_models_resolve_instance(
            objects->objective_models, model_id, model_header,
            model_instance, pitem_scale);
    }
    if (model_id == GE_ORIGINAL_MODEL104_ID) {
        const uint32_t instance = objects->model104_resolve_count
                < DAM_WINDOW_MODEL_INSTANCE_COUNT
            ? objects->model104_resolve_count++
            : DAM_WINDOW_MODEL_INSTANCE_COUNT - 1U;
        return ge_original_model104_resolve_instance(
            objects->model104[instance], model_id, model_header, model_instance,
            pitem_scale);
    }
    if (model_id == GE_ORIGINAL_MODEL178_ID) {
        const uint32_t instance = objects->model178_resolve_count < 2U
            ? objects->model178_resolve_count++ : 1U;
        return ge_original_model178_resolve_instance(
            objects->model178[instance], model_id, model_header, model_instance,
            pitem_scale);
    }
    if (model_id == GE_ORIGINAL_MODEL62_ID)
        return ge_original_model62_resolve_instance(objects->model62, model_id,
            model_header, model_instance, pitem_scale);
    return objects->pitem_models != NULL
        ? ge_original_pitem_model_resolve_instance(
            objects->pitem_models, model_id, model_header, model_instance,
            pitem_scale)
        : 0;
}

static int32_t dam_world_player_count(void *context)
{
    (void)context;
    return 1;
}

static int32_t dam_world_scenario(void *context)
{
    (void)context;
    return 0;
}

static void *dam_world_allocate_collision(void *context, uint32_t size_bytes)
{
    RuntimeDamWorldObjects *objects = context;

    if (size_bytes != sizeof(objects->object_collision[0].bytes)
            || objects->object_collision_count
                >= DAM_NATIVE_OBJECT_CAPACITY) return NULL;
    memset(objects->object_collision[objects->object_collision_count].bytes,
           0, sizeof(objects->object_collision[0].bytes));
    return objects->object_collision[objects->object_collision_count++].bytes;
}

static int dam_world_floor_y(void *context, void *stan, float x, float z,
                             float *floor_y)
{
    RuntimeDamWorldObjects *objects = context;
    if (objects->collision == NULL || !objects->collision->original_bound
            || stan == NULL || floor_y == NULL) return -1;
    *floor_y = ge_original_stan_get_position_y(
        &objects->collision->native, (const GeStanNativeTile *)stan, x, z);
    return 1;
}

static int dam_world_walk_tiles(void *context, void **stan, float start_x,
                                float start_z, float destination_x,
                                float destination_z)
{
    RuntimeDamWorldObjects *objects = context;
    if (objects->collision == NULL || !objects->collision->original_bound
            || stan == NULL || *stan == NULL) return -1;
    return ge_original_stan_walk_tiles_between_points(
        &objects->collision->native, (GeStanNativeTile **)stan,
        start_x, start_z, destination_x, destination_z);
}

static int dam_world_tile_rgb(void *context, void *stan, float x, float z,
                              uint8_t rgb[3])
{
    RuntimeDamWorldObjects *objects = context;
    const GeStanNativeTile *tile = stan;
    uint16_t mid;
    (void)x; (void)z;
    if (objects->collision == NULL || !objects->collision->original_bound
            || tile == NULL || rgb == NULL) return -1;
    mid = (uint16_t)tile->mid;
    rgb[0] = (uint8_t)(((mid >> 8) & 0xfU) * 0x11U);
    rgb[1] = (uint8_t)(((mid >> 4) & 0xfU) * 0x11U);
    rgb[2] = (uint8_t)((mid & 0xfU) * 0x11U);
    return 1;
}

static int dam_world_room_object_bounds(
    void *context, const float position[3], int16_t room,
    float *top, float *bottom)
{
    RuntimeDamWorldObjects *objects = context;
    (void)objects;
    return ge_original_prop_state_room_object_at_position(
        position, room, top, bottom) != NULL;
}

static int32_t dam_world_find_portal(void *context, int32_t room_a,
                                     int32_t room_b,
                                     const float point_a[3],
                                     const float point_b[3])
{
    RuntimeDamWorldObjects *objects = context;
    if (objects->preview == NULL) return -1;
    return ge_original_bg_find_portal_between_rooms(
        room_a, room_b, point_a, point_b);
}

static void dam_world_set_portal_open(void *context, int32_t portal, int open)
{
    RuntimeDamWorldObjects *objects = context;
    RuntimeDamPreview *preview = objects->preview;
    if (preview == NULL || !ge_original_bg_set_portal_open(
            portal, open, preview->portal_controls,
            preview->world.portal_count)) return;
    preview->world.portals[portal].control_bytes1 =
        preview->portal_controls[portal];
}

static int32_t stage_ordinary_model_load(void *context, int32_t model_id)
{
    RuntimeStageOrdinaryObjects *objects = context;
    return objects != NULL && ge_original_pitem_model_load(
        objects->models, model_id);
}

static int stage_ordinary_model_available(void *context, int32_t model_id)
{
    return stage_ordinary_model_load(context, model_id) != 0;
}

static int stage_ordinary_resolve_model(
    void *context, int32_t model_id, void **model_header,
    void **model_instance, float *pitem_scale)
{
    RuntimeStageOrdinaryObjects *objects = context;
    int result;

    if (objects == NULL) return 0;
    result = ge_original_pitem_model_resolve_instance(
        objects->models, model_id, model_header, model_instance, pitem_scale);
    if (result != 0) objects->last_resolved_model = *model_instance;
    return result;
}

static int stage_ordinary_object_hit_ready(
    void *context, const void *model_instance)
{
    RuntimeStageOrdinaryObjects *objects = context;
    return objects != NULL && ge_original_pitem_model_hit_ready(
        objects->models, model_instance);
}

static int32_t stage_ordinary_player_count(void *context)
{
    (void)context;
    return 1;
}

static int32_t stage_ordinary_scenario(void *context)
{
    (void)context;
    return 0;
}

static void *stage_ordinary_allocate_collision(
    void *context, uint32_t size_bytes)
{
    RuntimeStageOrdinaryObjects *objects = context;
    uint8_t *result;
    if (objects == NULL || size_bytes != 0x50U
            || objects->collision_count >= objects->collision_capacity)
        return NULL;
    result = objects->collision_blocks + objects->collision_count * 0x50U;
    ++objects->collision_count;
    memset(result, 0, 0x50U);
    return result;
}

static int stage_ordinary_floor_y(void *context, void *stan,
                                  float x, float z, float *floor_y)
{
    RuntimeStageOrdinaryObjects *objects = context;
    if (objects == NULL || objects->collision == NULL
            || !objects->collision->original_bound || stan == NULL
            || floor_y == NULL) return -1;
    *floor_y = ge_original_stan_get_position_y(
        &objects->collision->native, (const GeStanNativeTile *)stan, x, z);
    return 1;
}

static int stage_ordinary_walk_tiles(
    void *context, void **stan, float start_x, float start_z,
    float destination_x, float destination_z)
{
    RuntimeStageOrdinaryObjects *objects = context;
    if (objects == NULL || objects->collision == NULL
            || !objects->collision->original_bound || stan == NULL
            || *stan == NULL) return -1;
    return ge_original_stan_walk_tiles_between_points(
        &objects->collision->native, (GeStanNativeTile **)stan,
        start_x, start_z, destination_x, destination_z);
}

static int stage_ordinary_tile_rgb(void *context, void *stan,
                                   float x, float z, uint8_t rgb[3])
{
    const GeStanNativeTile *tile = stan;
    uint16_t mid;
    RuntimeStageOrdinaryObjects *objects = context;
    (void)x; (void)z;
    if (objects == NULL || objects->collision == NULL
            || !objects->collision->original_bound || tile == NULL
            || rgb == NULL) return -1;
    mid = (uint16_t)tile->mid;
    rgb[0] = (uint8_t)(((mid >> 8) & 0xfU) * 0x11U);
    rgb[1] = (uint8_t)(((mid >> 4) & 0xfU) * 0x11U);
    rgb[2] = (uint8_t)((mid & 0xfU) * 0x11U);
    return 1;
}

static int32_t stage_door_find_portal(
    void *context, int32_t room_a, int32_t room_b,
    const float point_a[3], const float point_b[3])
{
    RuntimeStageOrdinaryObjects *objects = context;

    if (objects == NULL || objects->preview == NULL) return -1;
    return ge_original_bg_find_portal_between_rooms(
        room_a, room_b, point_a, point_b);
}

static void stage_door_set_portal_open(
    void *context, int32_t portal, int open)
{
    RuntimeStageOrdinaryObjects *objects = context;
    RuntimeDamPreview *preview;

    if (objects == NULL || objects->preview == NULL) return;
    preview = objects->preview;
    if (!ge_original_bg_set_portal_open(
            portal, open, preview->portal_controls,
            preview->world.portal_count)) return;
    preview->world.portals[portal].control_bytes1 =
        preview->portal_controls[portal];
}

static void stage_door_register_room(
    void *context, void *prop, int16_t room)
{
    RuntimeStageOrdinaryObjects *objects = context;

    if (objects != NULL && objects->props != NULL)
        ge_original_prop_state_register_room(objects->props, prop, room);
}

static int construct_stage_door(
    void *context, const GeOriginalStagePropConstructionRequest *request,
    void *definition, size_t definition_size,
    void **prop_out, void **model_instance_out)
{
    RuntimeStageOrdinaryObjects *objects = context;
    void *prop;

    if (objects == NULL || request == NULL || definition == NULL
            || definition_size
                != ge_original_stage_prop_native_definition_size(request)
            || prop_out == NULL || model_instance_out == NULL) return 0;
    prop = ge_original_prop_state_allocate(objects->props, definition);
    if (prop == NULL || !ge_original_stage_prop_native_bind_prop(
            request, definition, prop,
            ge_original_stage_prop_native_prop_size())) return 0;
    objects->last_resolved_model = NULL;
    objects->door_status = ge_original_door_construct(
        definition, (int32_t)request->command_index);
    if (objects->door_status != GE_ORIGINAL_DOOR_OK
            || objects->last_resolved_model == NULL) return 0;
    ge_original_prop_state_activate(objects->props, prop);
    ge_original_prop_state_enable(objects->props, prop);
    *prop_out = prop;
    *model_instance_out = objects->last_resolved_model;
    ++objects->live_door_count;
    return 1;
}

static int link_stage_doors(
    void *context, void *first_definition, void *second_definition)
{
    (void)context;
    return ge_original_door_runtime_link_pair(
        first_definition, second_definition);
}

static int resolve_stage_guard_assigned_item(
    RuntimeStageOrdinaryObjects *objects, size_t command_index,
    void **prop_out, void **model_instance_out)
{
    size_t index;

    if (objects == NULL || objects->guards == NULL || prop_out == NULL
            || model_instance_out == NULL) return 0;
    for (index = 0U; index < objects->guard_weapon_count; ++index) {
        GeOriginalStageGuardWeaponSnapshot snapshot;
        if (ge_original_stage_guard_runtime_weapon_snapshot(
                objects->guards, index, &snapshot)
                && snapshot.command_index == command_index) {
            *prop_out = snapshot.prop_record;
            *model_instance_out = snapshot.model_instance;
            return *prop_out != NULL && *model_instance_out != NULL;
        }
    }
    for (index = 0U; index < objects->guard_hat_count; ++index) {
        GeOriginalStageGuardHatSnapshot snapshot;
        if (ge_original_stage_guard_runtime_hat_snapshot(
                objects->guards, index, &snapshot)
                && snapshot.command_index == command_index) {
            *prop_out = snapshot.prop_record;
            *model_instance_out = snapshot.model_instance;
            return *prop_out != NULL && *model_instance_out != NULL;
        }
    }
    return 0;
}

static void *resolve_stage_owner_prop(
    RuntimeStageOrdinaryObjects *objects, size_t command_index)
{
    size_t index;
    void *prop = NULL;
    void *model = NULL;

    if (objects == NULL) return NULL;
    for (index = 0U; index < objects->entry_count; ++index)
        if (objects->entries[index].live
                && objects->entries[index].command_index == command_index)
            return objects->entries[index].prop;
    if (resolve_stage_guard_assigned_item(
            objects, command_index, &prop, &model)) return prop;
    if (objects->guards != NULL) {
        const size_t count =
            ge_original_stage_guard_runtime_active_prop_count(objects->guards);
        for (index = 0U; index < count; ++index) {
            size_t actor_command;
            if (ge_original_stage_guard_runtime_active_prop(
                    objects->guards, index, &actor_command, &prop)
                    && actor_command == command_index)
                return prop;
        }
    }
    return NULL;
}

static RuntimeStageOrdinaryEntry *resolve_stage_owner_entry(
    RuntimeStageOrdinaryObjects *objects, size_t command_index)
{
    size_t index;
    if (objects == NULL) return NULL;
    for (index = 0U; index < objects->entry_count; ++index)
        if (objects->entries[index].live
                && objects->entries[index].command_index == command_index)
            return &objects->entries[index];
    return NULL;
}

static int construct_stage_standard_item(
    void *context, const GeOriginalStagePropConstructionRequest *request,
    void *definition, size_t definition_size,
    void **prop_out, void **model_instance_out)
{
    RuntimeStageOrdinaryObjects *objects = context;
    GeOriginalDefaultObjectPrepared prepared = {0};
    void *prop;

    if (objects == NULL || request == NULL || definition == NULL
            || definition_size
                != ge_original_stage_prop_native_definition_size(request)
            || prop_out == NULL || model_instance_out == NULL) return 0;
    prop = ge_original_prop_state_allocate(objects->props, definition);
    if (prop == NULL
            || ge_original_stage_item_construct_standard_exact(
                request, definition, prop,
                ge_original_stage_prop_native_prop_size(),
                &objects->object_providers, &prepared)
                != GE_ORIGINAL_STAGE_ITEM_OK
            || prepared.model_instance == NULL) return 0;
    ge_original_prop_state_register_room(
        objects->props, prop, request->placement.room);
    ge_original_prop_state_activate(objects->props, prop);
    *prop_out = prop;
    *model_instance_out = prepared.model_instance;
    return 1;
}

static int resolve_stage_assigned_item(
    void *context, const GeOriginalStagePropConstructionRequest *request,
    void **prop_out, void **model_instance_out)
{
    return request != NULL && resolve_stage_guard_assigned_item(
        context, request->command_index, prop_out, model_instance_out);
}

static int construct_stage_embedded_item(
    void *context, const GeOriginalStagePropConstructionRequest *request,
    int32_t owner_command_index, void *definition, size_t definition_size,
    void **prop_out, void **model_instance_out)
{
    RuntimeStageOrdinaryObjects *objects = context;
    void *owner;
    void *prop;
    void *collision;

    if (objects == NULL || request == NULL || definition == NULL
            || definition_size
                != ge_original_stage_prop_native_definition_size(request)
            || owner_command_index < 0 || prop_out == NULL
            || model_instance_out == NULL) return 0;
    owner = resolve_stage_owner_prop(objects, (size_t)owner_command_index);
    if (owner == NULL) return 0;
    prop = ge_original_prop_state_allocate(objects->props, definition);
    collision = stage_ordinary_allocate_collision(objects, 0x50U);
    if (prop == NULL || collision == NULL
            || ge_original_stage_item_construct_embedded_exact(
                request, definition, prop,
                ge_original_stage_prop_native_prop_size(), objects->models,
                owner, collision, model_instance_out)
                != GE_ORIGINAL_STAGE_ITEM_OK) return 0;
    /* domakedefaultobj's PROPFLAG_INSIDEANOTHEROBJ branch only initializes
     * the object.  The canonical second setup pass reparents it and leaves it
     * off the top-level active list: PropRecord prev/next are sibling links
     * once chrpropReparent has run.  Activating here overwrote those links and
     * made chrpropTick walk owner children as unrelated doors/objects. */
    ge_original_prop_state_enable(objects->props, prop);
    *prop_out = prop;
    return 1;
}

static void release_stage_interactive_object(
    void *context, void *definition, void *prop, void *model_instance)
{
    RuntimeStageOrdinaryObjects *objects = context;
    uint8_t type = 0U;

    (void)prop;
    if (objects == NULL) return;
    if (ge_original_stage_prop_native_definition_header(
            definition, NULL, NULL, &type) && type == PROPDEF_DOOR)
        (void)ge_original_door_release(definition);
    (void)ge_original_pitem_model_release_instance(
        objects->models, model_instance);
}

static int stage_ordinary_room_object_bounds(
    void *context, const float position[3], int16_t room,
    float *top, float *bottom)
{
    (void)context;
    return ge_original_prop_state_room_object_at_position(
        position, room, top, bottom) != NULL;
}

static int stage_guard_room_resident(void *context, uint8_t room_id)
{
    RuntimeStageOrdinaryObjects *objects = context;
    return objects != NULL && objects->preview != NULL
        && ge_dam_dynamic_scene_is_resident(
            &objects->preview->dynamic_scene, room_id);
}

static void *stage_guard_allocate_prop(void *context)
{
    RuntimeStageOrdinaryObjects *objects = context;
    return objects != NULL && objects->props != NULL
        ? ge_original_prop_state_allocate_player(objects->props) : NULL;
}

static int stage_guard_load_projectile_models(
    void *context, int32_t weapon_id)
{
    (void)context;
    /* setupWeapon calls this exact body for its loading side effect and does
     * not interpret the zero returned by ordinary non-projectile weapons. */
    (void)weaponLoadProjectileModels((ITEM_IDS)weapon_id);
    return 1;
}

static int stage_item_load_projectile_models(
    void *context, int8_t weapon_id)
{
    return stage_guard_load_projectile_models(context, (int32_t)weapon_id);
}

static int construct_stage_ordinary_object(
    void *context, const GeOriginalStagePropConstructionRequest *request)
{
    RuntimeStageOrdinaryObjects *objects = context;
    RuntimeStageOrdinaryEntry *entry;
    size_t definition_size;
    void *prop;
    void *model = NULL;
    void *collision = NULL;
    uint32_t owner_flags;
    bool embedded_default;
    if (objects == NULL || request == NULL
            || objects->entry_count >= objects->entry_capacity) return 0;
    entry = &objects->entries[objects->entry_count];
    definition_size = ge_original_stage_prop_native_definition_size(request);
    if (definition_size == 0U) return 0;
    entry->definition = calloc(1U, definition_size);
    if (entry->definition == NULL) return 0;
    objects->definitions[objects->entry_count] = entry->definition;
    entry->command_index = request->command_index;
    entry->definition_size = definition_size;
    entry->type = request->record->type;
    ++objects->entry_count;
    if (!ge_original_stage_prop_native_definition_init(
            request, entry->definition, definition_size)) return 0;
    prop = ge_original_prop_state_allocate(objects->props, entry->definition);
    owner_flags = request->record->words[2]
        & (PROPFLAG_INSIDEANOTHEROBJ | PROPFLAG_ASSIGNEDTOCHR);
    embedded_default = (request->record->type == PROPDEF_PROP
            || request->record->type == PROPDEF_GLASS)
        && owner_flags == PROPFLAG_INSIDEANOTHEROBJ;
    /* The exact domakedefaultobj INSIDE branch calls objInitWithModelDef
     * without looking up pad placement. Here pad is the signed setup-command
     * owner offset, so leave the fresh prop to objInit rather than applying
     * the root-only room/STAN binder. */
    if (prop == NULL || (!embedded_default
            && !ge_original_stage_prop_native_bind_prop(
                request, entry->definition, prop,
                ge_original_stage_prop_native_prop_size()))) return 0;
    entry->prop = prop;
    entry->model_id = request->model_id;
    if (embedded_default) {
        const int32_t owner_command_index =
            (int32_t)request->command_index + request->record->pad_id;
        if (owner_command_index < 0) return 0;
        if ((request->record->words[2] & PROPFLAG_00000100) != 0U) {
            collision = stage_ordinary_allocate_collision(objects, 0x50U);
            if (collision == NULL) return 0;
        }
        if (!ge_original_stage_monitor_construct_owned_exact(
                request, entry->definition, definition_size, prop,
                ge_original_stage_prop_native_prop_size(), objects->models,
                NULL, NULL, collision, stage_ordinary_player_count(objects),
                0, &model)) return 0;
        entry->prepared.model_instance = model;
        entry->prepared.prop = prop;
        entry->prepared.collision_data = collision;
        entry->prepared.object_initialized = 1;
        entry->owner_command_index = (size_t)owner_command_index;
        entry->pending_inside_owner = true;
        return 1;
    }
    entry->room = (uint8_t)request->placement.room;
    ge_original_prop_state_register_room(objects->props, prop,
                                         request->placement.room);
    ge_original_default_object_bind(&objects->object_providers,
                                    &entry->prepared);
    entry->construct_status = ge_original_default_object_construct_standard(
        entry->definition, (int32_t)request->command_index);
    if (entry->construct_status != GE_ORIGINAL_DEFAULT_OBJECT_OK) return 0;
    entry->placement_status = ge_original_default_object_place_standard(
        entry->definition);
    if (entry->placement_status != GE_ORIGINAL_DEFAULT_OBJECT_OK) return 0;
    ge_original_prop_state_activate(objects->props, prop);
    ge_original_prop_state_enable(objects->props, prop);
    entry->live = true;
    entry->root_active = true;
    ++objects->live_count;
    ++objects->root_live_count;
    return 1;
}

static RuntimeStageOrdinaryEntry *stage_ordinary_entry_for_definition(
    RuntimeStageOrdinaryObjects *objects, void *definition)
{
    size_t index;
    if (objects == NULL || definition == NULL) return NULL;
    for (index = 0U; index < objects->entry_count; ++index)
        if (objects->entries[index].definition == definition)
            return &objects->entries[index];
    return NULL;
}

static int stage_monitor_construct_standard(
    void *context, void *definition, int32_t command_index)
{
    RuntimeStageOrdinaryObjects *objects = context;
    RuntimeStageOrdinaryEntry *entry =
        stage_ordinary_entry_for_definition(objects, definition);
    GeOriginalStagePropConstructionRequest request;
    void *prop;
    if (entry == NULL || command_index < 0
            || entry->command_index != (size_t)command_index
            || !ge_original_stage_prop_construction_request(
                objects->setup, entry->command_index, &request)) return 0;
    prop = ge_original_prop_state_allocate(objects->props, definition);
    if (prop == NULL || !ge_original_stage_prop_native_bind_prop(
            &request, definition, prop,
            ge_original_stage_prop_native_prop_size())) return 0;
    entry->prop = prop;
    ge_original_default_object_bind(
        &objects->object_providers, &entry->prepared);
    entry->construct_status = ge_original_default_object_construct_standard(
        definition, command_index);
    return entry->construct_status == GE_ORIGINAL_DEFAULT_OBJECT_OK;
}

static int stage_monitor_place_standard(void *context, void *definition)
{
    RuntimeStageOrdinaryEntry *entry = stage_ordinary_entry_for_definition(
        context, definition);
    if (entry == NULL || entry->prop == NULL) return 0;
    entry->placement_status = ge_original_default_object_place_standard(
        definition);
    return entry->placement_status == GE_ORIGINAL_DEFAULT_OBJECT_OK;
}

static int stage_monitor_construct_owned(
    void *context, const GeOriginalStagePropConstructionRequest *request,
    void *definition, size_t definition_size,
    int32_t owner_command_index, int embedded)
{
    RuntimeStageOrdinaryObjects *objects = context;
    RuntimeStageOrdinaryEntry *entry =
        stage_ordinary_entry_for_definition(objects, definition);
    RuntimeStageOrdinaryEntry *owner;
    void *prop;
    void *model = NULL;
    void *collision = NULL;

    if (objects == NULL || request == NULL || entry == NULL
            || definition_size
                != ge_original_stage_prop_native_definition_size(request)
            || owner_command_index < 0) return 0;
    owner = resolve_stage_owner_entry(objects, (size_t)owner_command_index);
    if (embedded && (owner == NULL || owner->definition == NULL
            || owner->prop == NULL)) return 0;
    prop = ge_original_prop_state_allocate(objects->props, definition);
    if (prop == NULL || !ge_original_stage_monitor_bind_owned_prop_exact(
            request, definition, prop,
            ge_original_stage_prop_native_prop_size())) return 0;
    if ((request->record->words[2] & PROPFLAG_00000100) != 0U) {
        collision = stage_ordinary_allocate_collision(objects, 0x50U);
        if (collision == NULL) return 0;
    }
    if (!ge_original_stage_monitor_construct_owned_exact(
            request, definition, definition_size, prop,
            ge_original_stage_prop_native_prop_size(), objects->models,
            owner != NULL ? owner->definition : NULL,
            owner != NULL ? owner->prop : NULL, collision,
            stage_ordinary_player_count(objects),
            embedded, &model)) return 0;
    entry->prop = prop;
    entry->prepared.model_instance = model;
    entry->prepared.prop = prop;
    entry->prepared.collision_data = collision;
    entry->prepared.object_initialized = 1;
    entry->owner_command_index = (size_t)owner_command_index;
    entry->attached_monitor = embedded != 0;
    entry->pending_inside_owner = owner == NULL;
    if (owner != NULL) entry->room = owner->room;
    if (!entry->pending_inside_owner)
        ge_original_prop_state_enable(objects->props, prop);
    return 1;
}

static int construct_stage_monitor_object(
    void *context, const GeOriginalStagePropConstructionRequest *request)
{
    RuntimeStageOrdinaryObjects *objects = context;
    RuntimeStageOrdinaryEntry *entry;
    GeOriginalStageMonitorProviders providers = {0};
    GeOriginalStageMonitorStatus status;
    size_t definition_size;
    if (objects == NULL || request == NULL
            || (request->record->type != PROPDEF_MONITOR
                && request->record->type != PROPDEF_MULTI_MONITOR)
            || objects->entry_count >= objects->entry_capacity) return 0;
    definition_size = ge_original_stage_prop_native_definition_size(request);
    if (definition_size == 0U) return 0;
    entry = &objects->entries[objects->entry_count];
    entry->definition = calloc(1U, definition_size);
    if (entry->definition == NULL) return 0;
    objects->definitions[objects->entry_count] = entry->definition;
    entry->command_index = request->command_index;
    entry->definition_size = definition_size;
    entry->model_id = request->model_id;
    entry->room = (uint8_t)request->placement.room;
    entry->type = request->record->type;
    entry->monitor_screen_count = request->record->type == PROPDEF_MONITOR
        ? 1U : 4U;
    ++objects->entry_count;
    providers.context = objects;
    providers.construct_standard = stage_monitor_construct_standard;
    providers.place_standard = stage_monitor_place_standard;
    providers.construct_owned = stage_monitor_construct_owned;
    status = ge_original_stage_monitor_construct(
        request, entry->definition, definition_size, &providers);
    if (status != GE_ORIGINAL_STAGE_MONITOR_OK || entry->prop == NULL
            || entry->prepared.model_instance == NULL) return 0;
    if (!(request->record->type == PROPDEF_MONITOR
            && request->record->pad_id < 0
            && (request->record->words[2] & PROPFLAG_INSIDEANOTHEROBJ) == 0U)
            && (request->record->words[2]
                & PROPFLAG_INSIDEANOTHEROBJ) == 0U) {
        ge_original_prop_state_register_room(
            objects->props, entry->prop, request->placement.room);
        ge_original_prop_state_activate(objects->props, entry->prop);
        entry->root_active = true;
        ++objects->root_live_count;
    }
    if (!entry->pending_inside_owner)
        ge_original_prop_state_enable(objects->props, entry->prop);
    else
        return 1;
    entry->live = true;
    ++objects->live_count;
    ++objects->monitor_count;
    objects->monitor_screen_count += entry->monitor_screen_count;
    return 1;
}

static bool tick_stage_monitors(RuntimeStageOrdinaryObjects *objects)
{
    size_t entry_index;
    if (objects == NULL) return false;
    for (entry_index = 0U; entry_index < objects->entry_count; ++entry_index) {
        RuntimeStageOrdinaryEntry *entry = &objects->entries[entry_index];
        size_t screen;
        if (!entry->live || entry->monitor_screen_count == 0U) continue;
        for (screen = 0U; screen < entry->monitor_screen_count; ++screen) {
            if (ge_original_stage_monitor_tick(
                    entry->definition, entry->definition_size, screen,
                    &entry->monitor_screens[screen])) {
                ++entry->monitor_ticks;
                ++objects->monitor_tick_count;
            } else {
                /* setupMultiMonitor dispatches every authored slot. Models
                 * with unused non-DLCOLLISION switch parts are canonical
                 * process_monitor_animation_microcode no-ops (24 slots in
                 * the campaign audit), not runtime failures. */
                ++objects->monitor_noop_tick_count;
            }
        }
    }
    return true;
}

static int stage_supply_update_room_position(
    void *context, void *definition)
{
    RuntimeStageOrdinaryObjects *objects = context;
    RuntimeStageOrdinaryEntry *entry =
        stage_ordinary_entry_for_definition(objects, definition);
    if (entry == NULL || entry->prop == NULL) return 0;
    ge_original_prop_state_register_room(
        objects->props, entry->prop, (int16_t)entry->room);
    return ge_original_prop_state_room_contains(
        (int16_t)entry->room, entry->prop);
}

static int stage_supply_activate_prop(void *context, void *prop)
{
    RuntimeStageOrdinaryObjects *objects = context;
    if (objects == NULL || objects->props == NULL || prop == NULL) return 0;
    ge_original_prop_state_activate(objects->props, prop);
    return ge_original_prop_state_is_active(prop);
}

static int stage_supply_enable_prop(void *context, void *prop)
{
    RuntimeStageOrdinaryObjects *objects = context;
    if (objects == NULL || objects->props == NULL || prop == NULL) return 0;
    ge_original_prop_state_enable(objects->props, prop);
    return ge_original_prop_state_is_enabled(prop);
}

static int construct_stage_supply_object(
    void *context, const GeOriginalStagePropConstructionRequest *request)
{
    RuntimeStageOrdinaryObjects *objects = context;
    RuntimeStageOrdinaryEntry *entry;
    GeOriginalStageSupplyProviders providers = {0};
    GeOriginalStageSupplyInstance instance;
    GeOriginalStageSupplyStatus status;
    size_t definition_size;
    void *prop;
    const bool owned = request != NULL && request->record != NULL
        && request->record->type == PROPDEF_MAGAZINE
        && request->pad_id < 0
        && (request->record->words[2]
            & (PROPFLAG_INSIDEANOTHEROBJ | PROPFLAG_ASSIGNEDTOCHR))
                == PROPFLAG_INSIDEANOTHEROBJ;
    if (objects == NULL || request == NULL
            || (request->record->type != PROPDEF_MAGAZINE
                && request->record->type != PROPDEF_AMMO
                && request->record->type != PROPDEF_ARMOUR)
            || objects->entry_count >= objects->entry_capacity) return 0;
    definition_size = ge_original_stage_prop_native_definition_size(request);
    if (definition_size == 0U) return 0;
    entry = &objects->entries[objects->entry_count];
    entry->definition = calloc(1U, definition_size);
    if (entry->definition == NULL) return 0;
    objects->definitions[objects->entry_count] = entry->definition;
    entry->command_index = request->command_index;
    entry->definition_size = definition_size;
    entry->model_id = request->model_id;
    entry->room = (uint8_t)request->placement.room;
    entry->type = request->record->type;
    ++objects->entry_count;
    prop = ge_original_prop_state_allocate(objects->props, entry->definition);
    if (prop == NULL) return 0;
    entry->prop = prop;
    if (owned) {
        const int64_t owner_command = (int64_t)request->command_index
            + request->pad_id;
        void *owner_prop;
        void *collision;
        void *model_instance = NULL;
        if (owner_command < 0
                || owner_command >= (int64_t)objects->setup->prop_record_count)
            return 0;
        owner_prop = resolve_stage_owner_prop(
            objects, (size_t)owner_command);
        collision = stage_ordinary_allocate_collision(objects, 0x50U);
        if (owner_prop == NULL || collision == NULL
                || ge_original_stage_item_construct_embedded_exact(
                    request, entry->definition, prop,
                    ge_original_stage_prop_native_prop_size(),
                    objects->models, owner_prop, collision,
                    &model_instance) != GE_ORIGINAL_STAGE_ITEM_OK
                || model_instance == NULL) return 0;
        /* The second canonical setup pass reparents this magazine beneath
         * its already-created owner. It is a live tagged/object-graph node,
         * but never a room root or an independent propsTick/render input. */
        entry->prepared.model_instance = model_instance;
        ge_original_prop_state_enable(objects->props, prop);
        entry->room = UINT8_MAX;
        entry->live = true;
        ++objects->live_count;
        ++objects->supply_count;
        return 1;
    }
    providers.default_object = &objects->object_providers;
    providers.prepared = &entry->prepared;
    providers.update_room_position = stage_supply_update_room_position;
    providers.activate_prop = stage_supply_activate_prop;
    providers.enable_prop = stage_supply_enable_prop;
    status = ge_original_stage_supply_construct_exact(
        request, entry->definition, definition_size, prop,
        ge_original_stage_prop_native_prop_size(), &providers, &instance);
    if (status != GE_ORIGINAL_STAGE_SUPPLY_OK
            || instance.prop != prop || entry->prepared.model_instance == NULL)
        return 0;
    entry->live = true;
    entry->root_active = true;
    ++objects->live_count;
    ++objects->root_live_count;
    ++objects->supply_count;
    objects->supply_slot_model_load_count += instance.slot_model_loads;
    return 1;
}

static int32_t stage_tinted_glass_find_portal(
    void *context, const float point_a[3], const float point_b[3])
{
    (void)context;
    return ge_original_bg_find_portal_on_line(point_a, point_b);
}

static int construct_stage_tinted_glass_object(
    void *context, const GeOriginalStagePropConstructionRequest *request)
{
    RuntimeStageOrdinaryObjects *objects = context;
    RuntimeStageOrdinaryEntry *entry;
    GeOriginalStageTintedGlassProviders providers = {0};
    GeOriginalStageTintedGlassStatus status;
    size_t definition_size;
    if (objects == NULL || request == NULL
            || request->record->type != PROPDEF_TINTED_GLASS
            || objects->entry_count >= objects->entry_capacity) return 0;
    definition_size = ge_original_stage_prop_native_definition_size(request);
    if (definition_size == 0U) return 0;
    entry = &objects->entries[objects->entry_count];
    entry->definition = calloc(1U, definition_size);
    if (entry->definition == NULL) return 0;
    objects->definitions[objects->entry_count] = entry->definition;
    entry->command_index = request->command_index;
    entry->definition_size = definition_size;
    entry->model_id = request->model_id;
    entry->room = (uint8_t)request->placement.room;
    entry->type = request->record->type;
    ++objects->entry_count;
    providers.context = objects;
    providers.find_portal = stage_tinted_glass_find_portal;
    providers.construct_standard = stage_monitor_construct_standard;
    providers.place_standard = stage_monitor_place_standard;
    status = ge_original_stage_tinted_glass_construct(
        request, entry->definition, definition_size, &providers);
    if (status != GE_ORIGINAL_STAGE_TINTED_GLASS_OK || entry->prop == NULL
            || entry->prepared.model_instance == NULL) return 0;
    ge_original_prop_state_register_room(
        objects->props, entry->prop, request->placement.room);
    ge_original_prop_state_activate(objects->props, entry->prop);
    ge_original_prop_state_enable(objects->props, entry->prop);
    entry->live = true;
    entry->root_active = true;
    ++objects->live_count;
    ++objects->root_live_count;
    ++objects->tinted_glass_count;
    return 1;
}

static int construct_stage_cctv_object(
    void *context, const GeOriginalStagePropConstructionRequest *request)
{
    RuntimeStageOrdinaryObjects *objects = context;
    RuntimeStageOrdinaryEntry *entry;
    GeOriginalStageSecurityProviders providers = {0};
    GeOriginalStageSecurityInstance instance;
    GeOriginalStageSecurityStatus status;
    size_t definition_size;

    if (objects == NULL || request == NULL
            || request->record->type != PROPDEF_CCTV
            || objects->entry_count >= objects->entry_capacity) return 0;
    definition_size = ge_original_stage_prop_native_definition_size(request);
    if (definition_size == 0U) return 0;
    entry = &objects->entries[objects->entry_count];
    entry->definition = calloc(1U, definition_size);
    if (entry->definition == NULL) return 0;
    objects->definitions[objects->entry_count] = entry->definition;
    entry->command_index = request->command_index;
    entry->definition_size = definition_size;
    entry->model_id = request->model_id;
    entry->room = (uint8_t)request->placement.room;
    entry->type = request->record->type;
    ++objects->entry_count;

    /* Every required CCTV service is already supplied by the unchanged
     * active-prop/objTick graph: timer and player publication, STAN line
     * tests, authored model relations, alarm state, aim/camera matrices,
     * object damage/effects, and common object lighting. */
    providers.context = objects;
    providers.runtime_capabilities = GE_ORIGINAL_STAGE_SECURITY_CCTV_REQUIRED;
    providers.construct_standard = stage_monitor_construct_standard;
    providers.place_standard = stage_monitor_place_standard;
    providers.update_room_position = stage_supply_update_room_position;
    providers.activate_prop = stage_supply_activate_prop;
    providers.enable_prop = stage_supply_enable_prop;
    status = ge_original_stage_security_construct(
        request, entry->definition, definition_size, &providers, &instance);
    if (status != GE_ORIGINAL_STAGE_SECURITY_OK
            || !instance.constructed || !instance.runtime_ready
            || instance.prop == NULL || instance.model == NULL) return 0;
    entry->prop = instance.prop;
    entry->live = true;
    entry->root_active = true;
    ++objects->live_count;
    ++objects->root_live_count;
    ++objects->cctv_count;
    return 1;
}

static void *stage_security_allocate_stage(void *context, size_t size_bytes)
{
    RuntimeStageOrdinaryObjects *objects = context;
    RuntimeStageOrdinaryEntry *entry;
    if (objects == NULL || objects->entry_count == 0U
            || size_bytes != 0x30U) return NULL;
    entry = &objects->entries[objects->entry_count - 1U];
    if (entry->stage_allocation != NULL) return NULL;
    /* setupAutogun owns this from MEMPOOL_STAGE. Preserve that lifetime by
     * releasing the allocation only when the complete stage runtime closes;
     * the exact constructor initializes the canonical beam-age byte. */
    entry->stage_allocation = malloc(size_bytes);
    return entry->stage_allocation;
}

static int construct_stage_autogun_object(
    void *context, const GeOriginalStagePropConstructionRequest *request)
{
    RuntimeStageOrdinaryObjects *objects = context;
    RuntimeStageOrdinaryEntry *entry;
    GeOriginalStageSecurityProviders providers = {0};
    GeOriginalStageSecurityStatus status;
    size_t definition_size;

    if (objects == NULL || request == NULL
            || request->record->type != PROPDEF_AUTOGUN
            || objects->entry_count >= objects->entry_capacity) return 0;
    definition_size = ge_original_stage_prop_native_definition_size(request);
    if (definition_size == 0U) return 0;
    entry = &objects->entries[objects->entry_count];
    entry->definition = calloc(1U, definition_size);
    if (entry->definition == NULL) return 0;
    objects->definitions[objects->entry_count] = entry->definition;
    entry->command_index = request->command_index;
    entry->definition_size = definition_size;
    entry->model_id = request->model_id;
    entry->room = (uint8_t)request->placement.room;
    entry->type = request->record->type;
    ++objects->entry_count;

    providers.context = objects;
    providers.runtime_capabilities =
        GE_ORIGINAL_STAGE_SECURITY_AUTOGUN_REQUIRED;
    providers.construct_standard = stage_monitor_construct_standard;
    providers.place_standard = stage_monitor_place_standard;
    providers.update_room_position = stage_supply_update_room_position;
    providers.activate_prop = stage_supply_activate_prop;
    providers.enable_prop = stage_supply_enable_prop;
    providers.allocate_stage = stage_security_allocate_stage;
    status = ge_original_stage_autogun_lifecycle_construct(
        request, entry->definition, definition_size, &providers,
        &entry->security);
    if (status != GE_ORIGINAL_STAGE_SECURITY_OK
            || !ge_original_stage_autogun_lifecycle_is_live(
                &entry->security)) return 0;
    entry->prop = entry->security.prop;
    entry->live = true;
    entry->root_active = true;
    ++objects->live_count;
    ++objects->root_live_count;
    ++objects->autogun_count;
    return 1;
}

static int stage_misc_resolve_ai_list(
    void *context, int32_t list_id, void **resolved_list)
{
    struct AIRecord *list;
    (void)context;
    if (resolved_list == NULL) return 0;
    list = ailistFindById((s32)list_id);
    *resolved_list = list;
    return list != NULL;
}

static int stage_misc_set_model_switch(
    void *context, void *model, uint32_t switch_index, int enabled)
{
    RuntimeStageOrdinaryObjects *objects = context;
    const void *node;
    if (objects == NULL || objects->models == NULL || model == NULL)
        return 0;
    node = ge_original_pitem_model_instance_switch_node(
        objects->models, model, switch_index);
    /* proplvreset2 changes switch 5 only when the authored model actually
     * supplies that node. Its absence is the canonical no-op branch, not a
     * failed vehicle construction. */
    return node == NULL || ge_original_pitem_model_instance_set_switch(
        objects->models, model, switch_index, enabled);
}

static int stage_misc_load_tank_projectiles(void *context)
{
    (void)context;
    return weaponLoadProjectileModels(ITEM_TANKSHELLS) != 0U;
}

static int construct_stage_misc_object(
    void *context, const GeOriginalStagePropConstructionRequest *request)
{
    RuntimeStageOrdinaryObjects *objects = context;
    RuntimeStageOrdinaryEntry *entry;
    GeOriginalStageMiscProviders providers = {0};
    GeOriginalStageMiscInstance instance;
    GeOriginalStageMiscStatus status;
    size_t definition_size;

    if (objects == NULL || request == NULL
            || (request->record->type != PROPDEF_RACK
                && request->record->type != PROPDEF_SAFE
                && request->record->type != PROPDEF_ALARM
                && request->record->type != PROPDEF_GAS_RELEASING
                && request->record->type != PROPDEF_VEHICHLE
                && request->record->type != PROPDEF_AIRCRAFT
                && request->record->type != PROPDEF_TANK)
            || objects->entry_count >= objects->entry_capacity) return 0;
    definition_size = ge_original_stage_prop_native_definition_size(request);
    if (definition_size == 0U) return 0;
    entry = &objects->entries[objects->entry_count];
    entry->definition = calloc(1U, definition_size);
    if (entry->definition == NULL) return 0;
    objects->definitions[objects->entry_count] = entry->definition;
    entry->command_index = request->command_index;
    entry->definition_size = definition_size;
    entry->model_id = request->model_id;
    entry->room = (uint8_t)request->placement.room;
    entry->type = request->record->type;
    ++objects->entry_count;
    providers.context = objects;
    providers.construct_standard = stage_monitor_construct_standard;
    providers.place_standard = stage_monitor_place_standard;
    providers.update_room_position = stage_supply_update_room_position;
    providers.activate_prop = stage_supply_activate_prop;
    providers.enable_prop = stage_supply_enable_prop;
    providers.resolve_ai_list = stage_misc_resolve_ai_list;
    providers.set_model_switch = stage_misc_set_model_switch;
    providers.load_tank_projectiles = stage_misc_load_tank_projectiles;
    providers.get_floor_y = stage_ordinary_floor_y;
    status = ge_original_stage_misc_construct_exact(
        request, entry->definition, definition_size, &providers, &instance);
    if (status != GE_ORIGINAL_STAGE_MISC_OK || instance.prop == NULL
            || instance.model == NULL || !instance.constructed
            || !instance.activated) return 0;
    entry->prop = instance.prop;
    entry->live = true;
    entry->root_active = true;
    ++objects->live_count;
    ++objects->root_live_count;
    if (request->record->type == PROPDEF_SAFE)
        ++objects->safe_count;
    else if (request->record->type == PROPDEF_ALARM)
        ++objects->alarm_count;
    else if (request->record->type == PROPDEF_GAS_RELEASING)
        ++objects->gas_releasing_count;
    return 1;
}

static int construct_stage_special_object(
    void *context, const GeOriginalStagePropConstructionRequest *request)
{
    if (request == NULL || request->record == NULL) return 0;
    switch (request->record->type) {
    case PROPDEF_MONITOR:
    case PROPDEF_MULTI_MONITOR:
        return construct_stage_monitor_object(context, request);
    case PROPDEF_MAGAZINE:
    case PROPDEF_AMMO:
    case PROPDEF_ARMOUR:
        return construct_stage_supply_object(context, request);
    case PROPDEF_TINTED_GLASS:
        return construct_stage_tinted_glass_object(context, request);
    case PROPDEF_CCTV:
        return construct_stage_cctv_object(context, request);
    case PROPDEF_AUTOGUN:
        return construct_stage_autogun_object(context, request);
    case PROPDEF_ALARM:
    case PROPDEF_RACK:
    case PROPDEF_SAFE:
    case PROPDEF_GAS_RELEASING:
    case PROPDEF_VEHICHLE:
    case PROPDEF_AIRCRAFT:
    case PROPDEF_TANK:
        return construct_stage_misc_object(context, request);
    default:
        return 0;
    }
}

static int stage_owned_ordinary_kind(
    const GeOriginalStagePropRecord *record)
{
    uint32_t owner_flags;
    if (record == NULL || (record->type != PROPDEF_PROP
            && record->type != PROPDEF_GLASS)) return 0;
    owner_flags = record->words[2]
        & (PROPFLAG_INSIDEANOTHEROBJ | PROPFLAG_ASSIGNEDTOCHR);
    if (owner_flags == PROPFLAG_INSIDEANOTHEROBJ) return 1;
    if (owner_flags == PROPFLAG_ASSIGNEDTOCHR) return 2;
    return 0;
}

static int stage_owned_ordinary_already_materialized(
    const RuntimeStageOrdinaryObjects *objects, size_t command_index)
{
    size_t entry_index;
    if (objects == NULL) return 0;
    for (entry_index = 0U; entry_index < objects->entry_count; ++entry_index)
        if (objects->entries[entry_index].command_index == command_index)
            return 1;
    return 0;
}

static void stage_owned_ordinary_promote_report(
    RuntimeStageOrdinaryObjects *objects)
{
    /* Owned ordinary records remain unsupported until their exact owner
     * exists. Promote only after unchanged chrpropReparent succeeds. */
    if (objects->report.unsupported_branch != 0U)
        --objects->report.unsupported_branch;
    ++objects->report.ready;
    ++objects->report.constructed;
}

static int stage_owned_ordinary_demote_pending_report(
    RuntimeStageOrdinaryObjects *objects)
{
    if (objects == NULL || objects->report.ready == 0U
            || objects->report.constructed == 0U) return 0;
    --objects->report.ready;
    --objects->report.constructed;
    ++objects->report.unsupported_branch;
    return 1;
}

static int construct_stage_owned_ordinary_exact(
    RuntimeStageOrdinaryObjects *objects, size_t command_index,
    int owner_kind, void *owner_prop, uint8_t owner_room,
    size_t owner_command_index)
{
    GeOriginalStagePropConstructionRequest request;
    RuntimeStageOrdinaryEntry *entry;
    void *model_instance = NULL;
    void *collision;
    void *prop;
    size_t definition_size;
    GeOriginalStageItemStatus status;

    if (objects == NULL || objects->setup == NULL || owner_prop == NULL
            || owner_kind != 2
            || objects->entry_count >= objects->entry_capacity
            || stage_owned_ordinary_already_materialized(
                objects, command_index)
            || !ge_original_stage_prop_construction_request(
                objects->setup, command_index, &request)
            || stage_owned_ordinary_kind(request.record) != owner_kind)
        return 0;
    definition_size = ge_original_stage_prop_native_definition_size(&request);
    if (definition_size == 0U) return 0;
    entry = &objects->entries[objects->entry_count];
    entry->definition = calloc(1U, definition_size);
    if (entry->definition == NULL) return 0;
    objects->definitions[objects->entry_count] = entry->definition;
    entry->command_index = command_index;
    entry->owner_command_index = owner_command_index;
    entry->definition_size = definition_size;
    entry->model_id = request.model_id;
    entry->type = request.record->type;
    entry->room = owner_room;
    ++objects->entry_count;
    if (!ge_original_stage_prop_native_definition_init(
            &request, entry->definition, definition_size)) return 0;
    prop = ge_original_prop_state_allocate(objects->props, entry->definition);
    collision = stage_ordinary_allocate_collision(objects, 0x50U);
    if (prop == NULL || collision == NULL) return 0;
    status = ge_original_stage_item_construct_assigned_exact(
        &request, entry->definition, prop,
        ge_original_stage_prop_native_prop_size(), objects->models,
        owner_prop, collision, &model_instance);
    if (status != GE_ORIGINAL_STAGE_ITEM_OK || model_instance == NULL)
        return 0;
    entry->prop = prop;
    entry->prepared.prop = prop;
    entry->prepared.model_instance = model_instance;
    entry->prepared.collision_data = collision;
    entry->prepared.object_initialized = 1;
    /* Parented props are canonical object-graph nodes. chrpropReparent uses
     * prev/next for sibling links, so they must never enter the root active
     * list or independent room/renderer publication. */
    ge_original_prop_state_enable(objects->props, prop);
    entry->live = true;
    ++objects->live_count;
    ++objects->owned_ordinary_assigned_count;
    stage_owned_ordinary_promote_report(objects);
    return 1;
}

static int stage_guard_owner_by_literal_id(
    RuntimeStageOrdinaryObjects *objects, int32_t chr_id,
    void **owner_prop, uint8_t *owner_room)
{
    size_t guard_index;
    if (objects == NULL || objects->guards == NULL || owner_prop == NULL
            || owner_room == NULL) return 0;
    for (guard_index = 0U;
            guard_index < ge_original_stage_guard_runtime_count(
                objects->guards); ++guard_index) {
        GeOriginalStageGuardSnapshot snapshot;
        void *prop = NULL;
        void *chr = NULL;
        if (ge_original_stage_guard_runtime_snapshot(
                objects->guards, guard_index, &snapshot)
                && snapshot.chr_id == chr_id
                && snapshot.model_instance != NULL
                && ge_original_stage_guard_runtime_actor(
                    objects->guards, guard_index, &prop, &chr)
                && prop != NULL && chr != NULL) {
            *owner_prop = prop;
            *owner_room = snapshot.room_id;
            return 1;
        }
    }
    return 0;
}

static int materialize_stage_assigned_ordinary_exact(
    RuntimeStageOrdinaryObjects *objects)
{
    size_t expected = 0U;
    size_t constructed = 0U;
    size_t command_index;
    if (objects == NULL || objects->setup == NULL) return 0;
    for (command_index = 0U;
            command_index < objects->setup->prop_record_count;
            ++command_index) {
        const GeOriginalStagePropRecord *record =
            &objects->setup->prop_records[command_index];
        void *owner_prop = NULL;
        uint8_t owner_room = UINT8_MAX;
        if (stage_owned_ordinary_kind(record) != 2) continue;
        ++expected;
        if (stage_guard_owner_by_literal_id(
                objects, record->pad_id, &owner_prop, &owner_room)
                && construct_stage_owned_ordinary_exact(
                    objects, command_index, 2, owner_prop, owner_room,
                    SIZE_MAX)) ++constructed;
    }
    objects->owned_ordinary_pending_count += expected - constructed;
    return constructed == expected;
}

static int bind_stage_pending_inside_owner_exact(
    RuntimeStageOrdinaryObjects *objects, RuntimeStageOrdinaryEntry *entry,
    void *owner_definition, void *owner_prop, uint8_t owner_room)
{
    if (objects == NULL || entry == NULL || !entry->pending_inside_owner
            || owner_definition == NULL || owner_prop == NULL
            || !ge_original_stage_monitor_bind_inside_owner_exact(
                entry->definition, owner_definition, owner_prop)) return 0;
    entry->room = owner_room;
    entry->pending_inside_owner = false;
    ge_original_prop_state_enable(objects->props, entry->prop);
    entry->live = true;
    ++objects->live_count;
    if (stage_owned_ordinary_kind(
            &objects->setup->prop_records[entry->command_index]) == 1)
        ++objects->owned_ordinary_embedded_count;
    if (entry->monitor_screen_count != 0U) {
        ++objects->monitor_count;
        objects->monitor_screen_count += entry->monitor_screen_count;
    }
    stage_owned_ordinary_promote_report(objects);
    return 1;
}

static bool materialize_stage_ordinary_resident(
    RuntimeStageOrdinaryObjects *objects)
{
    GeOriginalStagePropMaterializerProviders materializer = {0};
    if (objects == NULL || objects->setup == NULL || objects->models == NULL)
        return false;
    materializer.context = objects;
    materializer.capabilities = GE_ORIGINAL_STAGE_PROP_CAP_DEFAULT_OBJECT
        | GE_ORIGINAL_STAGE_PROP_CAP_MONITOR
        | GE_ORIGINAL_STAGE_PROP_CAP_SUPPLY
        | GE_ORIGINAL_STAGE_PROP_CAP_TINTED_GLASS
        | GE_ORIGINAL_STAGE_PROP_CAP_CCTV
        | GE_ORIGINAL_STAGE_PROP_CAP_AUTOGUN
        | GE_ORIGINAL_STAGE_PROP_CAP_ALARM
        | GE_ORIGINAL_STAGE_PROP_CAP_GAS_RELEASING
        | GE_ORIGINAL_STAGE_PROP_CAP_SAFE
        | GE_ORIGINAL_STAGE_PROP_CAP_MISC_OBJECT;
    materializer.model_available = stage_ordinary_model_available;
    materializer.construct_default_object = construct_stage_ordinary_object;
    materializer.construct_special_object = construct_stage_special_object;
    (void)ge_original_stage_prop_materialize_ready(
        objects->setup, &materializer, &objects->report);
    {
        size_t pending = 0U;
        bool progressed;
        size_t entry_index;
        for (entry_index = 0U; entry_index < objects->entry_count;
                ++entry_index)
            if (objects->entries[entry_index].pending_inside_owner) {
                ++pending;
                if (!stage_owned_ordinary_demote_pending_report(objects))
                    return false;
            }
        do {
            progressed = false;
            for (entry_index = 0U; entry_index < objects->entry_count;
                    ++entry_index) {
                RuntimeStageOrdinaryEntry *entry =
                    &objects->entries[entry_index];
                RuntimeStageOrdinaryEntry *owner;
                if (!entry->pending_inside_owner) continue;
                owner = resolve_stage_owner_entry(
                    objects, entry->owner_command_index);
                if (owner == NULL) continue;
                if (!bind_stage_pending_inside_owner_exact(
                        objects, entry, owner->definition, owner->prop,
                        owner->room))
                    return false;
                --pending;
                progressed = true;
            }
        } while (pending != 0U && progressed);
        objects->owned_ordinary_pending_count += pending;
        if (pending != 0U) {
            for (entry_index = 0U; entry_index < objects->entry_count;
                    ++entry_index) {
                const RuntimeStageOrdinaryEntry *entry =
                    &objects->entries[entry_index];
                if (!entry->pending_inside_owner) continue;
                if (entry->owner_command_index
                            >= objects->setup->prop_record_count
                        || objects->setup->prop_records[
                            entry->owner_command_index].type != PROPDEF_DOOR)
                    return false;
            }
        }
    }
    ge_original_pitem_model_get_stats(objects->models, &objects->model_stats);
    objects->model_status = ge_original_pitem_model_last_status(
        objects->models);
    return objects->live_count != 0U;
}

static bool initialize_stage_ordinary_objects(
    RuntimeStageOrdinaryObjects *objects, GeAssetPack *asset_pack,
    const GeOriginalStageSetupRuntime *setup, GeOriginalPropState *props,
    RuntimeDamCollision *collision, RuntimeDamPreview *preview)
{
    size_t default_count;
    size_t interactive_count;
    size_t monitor_record_count;
    size_t supply_record_count;
    size_t tinted_glass_record_count;
    size_t cctv_record_count;
    size_t autogun_record_count;
    size_t alarm_record_count;
    size_t gas_releasing_record_count;
    size_t safe_record_count;
    size_t rack_record_count;
    size_t vehicle_record_count;
    size_t aircraft_record_count;
    size_t tank_record_count;
    size_t safe_relation_record_count;
    size_t entry_capacity;
    size_t model_capacity;
    size_t instance_capacity;
    bool ordinary_ready = true;
    GeOriginalDoorProviders door_providers = {0};
    if (objects == NULL || asset_pack == NULL || setup == NULL
            || setup->loaded == 0U || props == NULL || collision == NULL
            || preview == NULL) return false;
    memset(objects, 0, sizeof(*objects));
    objects->props = props;
    objects->setup = setup;
    objects->collision = collision;
    objects->preview = preview;
    /* lvlStageLoad publishes the fully relocated setup before any authored
     * object constructor reads g_CurrentSetup. Dam's earlier dedicated
     * bootstrap had only rebound ailists, which left the canonical object
     * body looking at a separate bound-pad table. */
    if (!ge_original_stage_setup_publish(setup)) return false;
    ge_original_stage_safe_runtime_bind(&objects->safe_runtime);
    objects->model_dependencies = ge_original_stage_prop_model_dependencies(
        setup, GE_ORIGINAL_STAGE_PROP_SERVICE_DEFAULT_OBJECT, NULL, 0U);
    model_capacity = objects->model_dependencies
        + ge_original_stage_prop_model_dependencies(
            setup, GE_ORIGINAL_STAGE_PROP_SERVICE_DOOR, NULL, 0U)
        + ge_original_stage_prop_model_dependencies(
            setup, GE_ORIGINAL_STAGE_PROP_SERVICE_ITEM, NULL, 0U)
        + ge_original_stage_prop_model_dependencies(
            setup, GE_ORIGINAL_STAGE_PROP_SERVICE_SPECIAL_OBJECT, NULL, 0U);
    default_count = ge_original_stage_setup_prop_type_count(setup, PROPDEF_PROP)
        + ge_original_stage_setup_prop_type_count(setup, PROPDEF_GLASS);
    objects->door_count = ge_original_stage_setup_prop_type_count(
        setup, PROPDEF_DOOR);
    monitor_record_count = ge_original_stage_setup_prop_type_count(
        setup, PROPDEF_MONITOR)
        + ge_original_stage_setup_prop_type_count(
            setup, PROPDEF_MULTI_MONITOR);
    supply_record_count = ge_original_stage_setup_prop_type_count(
        setup, PROPDEF_MAGAZINE)
        + ge_original_stage_setup_prop_type_count(setup, PROPDEF_AMMO)
        + ge_original_stage_setup_prop_type_count(setup, PROPDEF_ARMOUR);
    tinted_glass_record_count = ge_original_stage_setup_prop_type_count(
        setup, PROPDEF_TINTED_GLASS);
    cctv_record_count = ge_original_stage_setup_prop_type_count(
        setup, PROPDEF_CCTV);
    autogun_record_count = ge_original_stage_setup_prop_type_count(
        setup, PROPDEF_AUTOGUN);
    alarm_record_count = ge_original_stage_setup_prop_type_count(
        setup, PROPDEF_ALARM);
    gas_releasing_record_count = ge_original_stage_setup_prop_type_count(
        setup, PROPDEF_GAS_RELEASING);
    safe_record_count = ge_original_stage_setup_prop_type_count(
        setup, PROPDEF_SAFE);
    rack_record_count = ge_original_stage_setup_prop_type_count(
        setup, PROPDEF_RACK);
    vehicle_record_count = ge_original_stage_setup_prop_type_count(
        setup, PROPDEF_VEHICHLE);
    aircraft_record_count = ge_original_stage_setup_prop_type_count(
        setup, PROPDEF_AIRCRAFT);
    tank_record_count = ge_original_stage_setup_prop_type_count(
        setup, PROPDEF_TANK);
    safe_relation_record_count = ge_original_stage_setup_prop_type_count(
        setup, PROPDEF_SAFE_ITEM);
    /* Each authored multi-ammo crate can request at most one additional
     * Pitem resource in the exact single-player setup branch.  Instances are
     * still allocated only for the crate's own common object model. */
    model_capacity += ge_original_stage_setup_prop_type_count(
        setup, PROPDEF_AMMO);
    entry_capacity = default_count + monitor_record_count
        + supply_record_count + tinted_glass_record_count
        + cctv_record_count + autogun_record_count
        + alarm_record_count + gas_releasing_record_count
        + safe_record_count + rack_record_count + vehicle_record_count
        + aircraft_record_count + tank_record_count
        + safe_relation_record_count;
    interactive_count = objects->door_count
        + ge_original_stage_setup_prop_type_count(setup, PROPDEF_KEY)
        + ge_original_stage_setup_prop_type_count(setup, PROPDEF_COLLECTABLE)
        + ge_original_stage_setup_prop_type_count(setup, PROPDEF_HAT);
    /* init_guards reserves ten dynamic character slots. Their script-created
     * hats/weapons use the same recyclable model manager on N64; reserve the
     * matching native instance headroom in the shared Pitem provider. */
    instance_capacity = entry_capacity + objects->door_count
        + interactive_count + 10U;
    if (model_capacity == 0U || instance_capacity == 0U) {
        objects->initialized = true;
        return true;
    }
    if (entry_capacity != 0U) {
        objects->entries = calloc(entry_capacity, sizeof(*objects->entries));
        objects->definitions = calloc(
            entry_capacity, sizeof(*objects->definitions));
    }
    objects->collision_capacity = instance_capacity;
    objects->collision_blocks = calloc(instance_capacity, 0x50U);
    objects->models = ge_original_pitem_model_provider_create(
        asset_pack, model_capacity, instance_capacity,
        &objects->model_status);
    if ((entry_capacity != 0U
            && (objects->entries == NULL || objects->definitions == NULL))
            || objects->collision_blocks == NULL || objects->models == NULL)
        return false;
    objects->entry_capacity = entry_capacity;
    objects->object_providers.context = objects;
    objects->object_providers.model_load = stage_ordinary_model_load;
    objects->object_providers.get_player_count = stage_ordinary_player_count;
    objects->object_providers.get_scenario = stage_ordinary_scenario;
    objects->object_providers.resolve_model_instance =
        stage_ordinary_resolve_model;
    objects->object_providers.allocate_collision =
        stage_ordinary_allocate_collision;
    objects->object_providers.get_floor_y = stage_ordinary_floor_y;
    objects->object_providers.get_room_object_bounds =
        stage_ordinary_room_object_bounds;
    objects->object_providers.walk_tiles = stage_ordinary_walk_tiles;
    objects->object_providers.get_tile_rgb = stage_ordinary_tile_rgb;
    ge_original_gameplay_services_bind_model_loader(
        objects, stage_ordinary_model_load);
    objects->initialized = true;
    if (entry_capacity != 0U)
        ordinary_ready = materialize_stage_ordinary_resident(objects);

    door_providers.context = objects;
    door_providers.model_load = stage_ordinary_model_load;
    door_providers.resolve_model_instance = stage_ordinary_resolve_model;
    door_providers.allocate_collision = stage_ordinary_allocate_collision;
    door_providers.walk_tiles = stage_ordinary_walk_tiles;
    door_providers.get_tile_rgb = stage_ordinary_tile_rgb;
    door_providers.find_portal = stage_door_find_portal;
    door_providers.set_portal_open = stage_door_set_portal_open;
    door_providers.register_room = stage_door_register_room;
    ge_original_door_bind(&door_providers, &objects->door_scratch);
    ge_original_pp7_fire_bind_object_hit_ready(
        objects, stage_ordinary_object_hit_ready);

    return ordinary_ready;
}

static size_t stage_guard_bullet_hit_count(void *context)
{
    return ge_original_stage_guard_runtime_count(context);
}

static int stage_guard_bullet_hit_actor(
    void *context, size_t index, void **prop_record, void **chr_record)
{
    return ge_original_stage_guard_runtime_actor(
        context, index, prop_record, chr_record);
}

static bool initialize_stage_guard_objects(
    RuntimeStageOrdinaryObjects *objects, GeAssetPack *asset_pack)
{
    GeOriginalStageGuardRuntimeServices services = {0};
    GeOriginalStagePropMaterializerProviders materializer;
    if (objects == NULL || !objects->initialized || asset_pack == NULL
            || objects->setup == NULL || objects->setup->loaded == 0U)
        return false;
    objects->guard_count = ge_original_stage_setup_prop_type_count(
        objects->setup, PROPDEF_GUARD);
    if (objects->guard_count == 0U) return true;
    objects->guard_models = ge_original_character_model_provider_create(
        asset_pack, ge_original_character_model_dependency_count(),
        objects->guard_count + 10U, &objects->guard_model_status);
    if (objects->guard_models == NULL) return false;
    ge_original_character_appearance_begin_stage();
    services.context = objects;
    services.allocate_prop = stage_guard_allocate_prop;
    services.choose_head = ge_original_character_appearance_choose_head;
    services.choose_sunglasses =
        ge_original_character_appearance_choose_sunglasses;
    services.room_resident = stage_guard_room_resident;
    services.tile_rgb = stage_ordinary_tile_rgb;
    objects->guards = ge_original_stage_guard_runtime_create(
        objects->guard_models, objects->guard_count + 10U, &services,
        &objects->guard_status);
    if (objects->guards == NULL
            || !ge_original_stage_guard_runtime_materializer(
                objects->guards, &materializer)) goto fail;
    ge_original_player_body_bind(objects->guards,
        ge_original_stage_guard_runtime_load_player_body);
    ge_original_player_body_bind_held_item(objects->guards,
        ge_original_stage_guard_runtime_attach_player_held_item);
    if (!ge_original_stage_prop_materialize_ready(
            objects->setup, &materializer, &objects->guard_report))
        goto fail;
    if (objects->guard_report.constructed != objects->guard_count
            || ge_original_stage_guard_runtime_count(objects->guards)
                != objects->guard_count) goto fail;
    objects->guard_status =
        ge_original_stage_guard_runtime_bind_authored_weapons(
            objects->guards, objects->setup, objects->models,
            stage_guard_load_projectile_models, objects,
            &objects->guard_weapon_report);
    if (objects->guard_status != GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK)
        goto fail;
    objects->guard_weapon_count =
        ge_original_stage_guard_runtime_weapon_count(objects->guards);
    objects->guard_status =
        ge_original_stage_guard_runtime_bind_authored_hats(
            objects->guards, objects->setup, objects->models,
            &objects->guard_hat_report);
    if (objects->guard_status != GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK)
        goto fail;
    objects->guard_hat_count =
        ge_original_stage_guard_runtime_hat_count(objects->guards);
    if (!materialize_stage_assigned_ordinary_exact(objects)) goto fail;
    ge_original_guard_bullet_hit_bind_stage_guards(
        objects->guards, stage_guard_bullet_hit_count,
        stage_guard_bullet_hit_actor);
    return true;
fail:
    ge_original_player_body_unbind(objects->guards);
    ge_original_stage_guard_runtime_destroy(objects->guards);
    ge_original_character_model_provider_destroy(objects->guard_models);
    objects->guards = NULL;
    objects->guard_models = NULL;
    return false;
}

static bool initialize_stage_interactive_objects(
    RuntimeStageOrdinaryObjects *objects)
{
    GeOriginalStageInteractiveProviders providers = {0};
    const size_t count = objects != NULL && objects->setup != NULL
        ? ge_original_stage_setup_prop_type_count(
            objects->setup, PROPDEF_DOOR)
            + ge_original_stage_setup_prop_type_count(
                objects->setup, PROPDEF_KEY)
            + ge_original_stage_setup_prop_type_count(
                objects->setup, PROPDEF_COLLECTABLE)
            + ge_original_stage_setup_prop_type_count(
                objects->setup, PROPDEF_HAT)
        : 0U;

    if (objects == NULL || !objects->initialized || objects->guards == NULL)
        return false;
    if (count == 0U) return true;
    providers.context = objects;
    providers.difficulty = (uint8_t)lvlGetSelectedDifficulty();
    providers.player_count = 1U;
    providers.model_available = stage_ordinary_model_available;
    providers.load_projectile_models = stage_item_load_projectile_models;
    providers.construct_default_object = construct_stage_standard_item;
    providers.construct_door = construct_stage_door;
    providers.resolve_assigned_item = resolve_stage_assigned_item;
    providers.construct_embedded_item = construct_stage_embedded_item;
    providers.link_doors = link_stage_doors;
    providers.release_object = release_stage_interactive_object;
    if (!ge_original_stage_interactive_materialize(
            objects->setup, &providers, &objects->interactive)) return false;
    if (objects->owned_ordinary_pending_count != 0U) {
        size_t ordinary_index;
        for (ordinary_index = 0U; ordinary_index < objects->entry_count;
                ++ordinary_index) {
            RuntimeStageOrdinaryEntry *ordinary =
                &objects->entries[ordinary_index];
            size_t interactive_index;
            if (!ordinary->pending_inside_owner) continue;
            for (interactive_index = 0U;
                    interactive_index < objects->interactive.entry_count;
                    ++interactive_index) {
                const GeOriginalStageInteractiveEntry *owner =
                    ge_original_stage_interactive_entry(
                        &objects->interactive, interactive_index);
                if (owner == NULL || !owner->constructed
                        || owner->type != PROPDEF_DOOR
                        || owner->command_index
                            != ordinary->owner_command_index)
                    continue;
                if (!bind_stage_pending_inside_owner_exact(
                        objects, ordinary, owner->definition, owner->prop,
                        (uint8_t)owner->room)) return false;
                --objects->owned_ordinary_pending_count;
                break;
            }
            if (ordinary->pending_inside_owner) return false;
        }
    }
    return objects->owned_ordinary_pending_count == 0U;
}

static void *stage_definition_by_command(
    RuntimeStageOrdinaryObjects *objects, size_t command_index)
{
    size_t index;
    if (objects == NULL) return NULL;
    for (index = 0U; index < objects->entry_count; ++index)
        if (objects->entries[index].live
                && objects->entries[index].command_index == command_index)
            return objects->entries[index].definition;
    for (index = 0U; index < objects->interactive.entry_count; ++index) {
        const GeOriginalStageInteractiveEntry *entry =
            ge_original_stage_interactive_entry(&objects->interactive, index);
        if (entry != NULL && entry->constructed
                && entry->command_index == command_index) {
            size_t assigned_index;
            if (!entry->externally_owned) return entry->definition;
            for (assigned_index = 0U;
                    assigned_index < objects->guard_weapon_count;
                    ++assigned_index) {
                GeOriginalStageGuardWeaponSnapshot snapshot;
                if (ge_original_stage_guard_runtime_weapon_snapshot(
                        objects->guards, assigned_index, &snapshot)
                        && snapshot.command_index == command_index)
                    return snapshot.weapon_record;
            }
            for (assigned_index = 0U;
                    assigned_index < objects->guard_hat_count;
                    ++assigned_index) {
                GeOriginalStageGuardHatSnapshot snapshot;
                if (ge_original_stage_guard_runtime_hat_snapshot(
                        objects->guards, assigned_index, &snapshot)
                        && snapshot.command_index == command_index)
                    return snapshot.hat_record;
            }
        }
    }
    return NULL;
}

static void *stage_objective_definition_by_command(
    void *context, size_t command_index,
    const GeOriginalStagePropRecord *record)
{
    (void)record;
    return stage_definition_by_command(context, command_index);
}

static void *stage_safe_definition_by_command(
    void *context, size_t command_index)
{
    return stage_definition_by_command(context, command_index);
}

static int stage_safe_register_relation(void *context, void *relation)
{
    RuntimeStageOrdinaryObjects *objects = context;
    return objects != NULL && ge_original_stage_safe_runtime_register_relation(
        &objects->safe_runtime, relation);
}

static bool initialize_stage_safe_relations(
    RuntimeStageOrdinaryObjects *objects)
{
    GeOriginalStageSafeItemProviders providers = {0};
    size_t command_index;
    size_t expected;

    if (objects == NULL || objects->setup == NULL) return false;
    expected = ge_original_stage_setup_prop_type_count(
        objects->setup, PROPDEF_SAFE_ITEM);
    if (expected == 0U) return true;
    providers.context = objects;
    providers.find_definition = stage_safe_definition_by_command;
    providers.register_relation = stage_safe_register_relation;
    for (command_index = 0U;
            command_index < objects->setup->prop_record_count;
            ++command_index) {
        GeOriginalStagePropConstructionRequest request;
        RuntimeStageOrdinaryEntry *entry;
        size_t definition_size;
        if (objects->setup->prop_records[command_index].type
                != PROPDEF_SAFE_ITEM) continue;
        if (objects->entry_count >= objects->entry_capacity
                || !ge_original_stage_prop_construction_request(
                    objects->setup, command_index, &request)) return false;
        definition_size = ge_original_stage_prop_native_definition_size(
            &request);
        if (definition_size == 0U) return false;
        entry = &objects->entries[objects->entry_count];
        entry->definition = calloc(1U, definition_size);
        if (entry->definition == NULL) return false;
        objects->definitions[objects->entry_count] = entry->definition;
        entry->command_index = command_index;
        entry->definition_size = definition_size;
        entry->model_id = request.model_id;
        entry->type = request.record->type;
        ++objects->entry_count;
        objects->safe_relation_status =
            ge_original_stage_safe_item_link_exact(
                &request, entry->definition, definition_size, &providers);
        if (objects->safe_relation_status != GE_ORIGINAL_STAGE_MISC_OK)
            return false;
        ++objects->safe_relation_count;
    }
    return objects->safe_relation_count == expected
        && objects->safe_runtime.relation_count == expected;
}

static bool initialize_stage_objectives(RuntimeStageOrdinaryObjects *objects)
{
    GeOriginalStageObjectiveProviders providers = {0};
    GeOriginalStageObjectiveRuntimeProviders runtime_providers = {0};
    if (objects == NULL || objects->setup == NULL) return false;
    providers.context = objects;
    providers.object_definition_by_command =
        stage_objective_definition_by_command;
    if (ge_original_stage_objectives_build(
            &objects->objectives, objects->setup, &providers)
            != GE_ORIGINAL_STAGE_OBJECTIVE_OK) return false;
    runtime_providers.context = g_CurrentPlayer;
    runtime_providers.prop_in_inventory =
        ge_original_stage_objective_prop_in_inventory_exact;
    runtime_providers.stage_flag_set =
        ge_original_stage_objective_stage_flag_set_exact;
    runtime_providers.key_analyzer_complete =
        ge_original_stage_objective_key_analyzer_complete_exact;
    return ge_original_stage_objective_runtime_begin(
        &objects->objective_runtime, &objects->objectives,
        &runtime_providers) == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK;
}

static bool tick_stage_objectives(RuntimeStageOrdinaryObjects *objects)
{
    size_t menu;
    size_t ready = 0U;
    size_t blocked = 0U;
    if (objects == NULL || !objects->objective_runtime.bound) return false;
    for (menu = 0U; menu < GE_ORIGINAL_STAGE_OBJECTIVE_MAX; ++menu) {
        GeOriginalStageObjectiveRuntimeStatus status;
        if (objects->objectives.objective_by_menu[menu] < 0) continue;
        status = ge_original_stage_objective_runtime_evaluate(
            &objects->objective_runtime, (uint8_t)menu,
            &objects->objective_evaluations[menu]);
        if (status == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK) {
            ++ready;
        } else if (status == GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_BLOCKED) {
            ++blocked;
        } else {
            return false;
        }
    }
    objects->objective_evaluation_ready_count = ready;
    objects->objective_evaluation_blocked_count = blocked;
    ++objects->objective_evaluation_ticks;
    return true;
}

static void stage_alarm_play_sfx(void *context, uint32_t sfx_id)
{
    (void)context;
    ge_original_gameplay_services_play_sfx(sfx_id);
}

static void tick_stage_interaction(RuntimeStageOrdinaryObjects *objects)
{
    GeOriginalDoorInteractionResult door_result;
    GeOriginalStageAlarmInteractionProviders providers = {0};
    void **object_props = NULL;
    void *selected;
    size_t entry_index;
    size_t object_index = 0U;

    if (objects == NULL || objects->actor_tick_status
            != RUNTIME_STAGE_ACTOR_TICK_READY) return;
    ++objects->alarm_interaction_ticks;
    door_result = ge_original_door_interaction_tick();
    if (door_result != GE_ORIGINAL_DOOR_INTERACTION_RELOAD_REQUESTED
            || objects->entry_count == 0U) return;
    object_props = calloc(objects->entry_count, sizeof(*object_props));
    if (object_props == NULL) {
        objects->alarm_interaction.status =
            GE_ORIGINAL_STAGE_ALARM_INTERACTION_INVALID_ARGUMENT;
        return;
    }
    for (entry_index = 0U; entry_index < objects->entry_count; ++entry_index) {
        RuntimeStageOrdinaryEntry *entry = &objects->entries[entry_index];
        if (!entry->root_active || entry->prop == NULL) continue;
        object_props[object_index++] = entry->prop;
    }
    if (object_index == 0U) { free(object_props); return; }
    selected = ge_original_gameplay_services_find_interactable(
        object_props, object_index);
    free(object_props);
    if (selected == NULL) return;
    providers.play_sfx = stage_alarm_play_sfx;
    if (ge_original_stage_object_interact_exact(
            selected, &providers, &objects->alarm_interaction)
            == GE_ORIGINAL_STAGE_ALARM_INTERACTION_OK)
        ++objects->alarm_interaction_activations;
}

static bool initialize_stage_active_props(
    RuntimeStageOrdinaryObjects *objects, const RuntimeDamIntro *intro)
{
    size_t input_index = 0U;
    size_t index;
    size_t expected_doors;
    size_t guard_root_count;
    size_t item_active_count = 0U;
    struct ChrRecord *chr_pool = NULL;

    if (objects == NULL || objects->setup == NULL
            || objects->guards == NULL) {
        if (objects != NULL)
            objects->actor_tick_status =
                RUNTIME_STAGE_ACTOR_TICK_MISSING_GUARDS;
        return false;
    }
    if (intro == NULL || !intro->player.initialized
            || intro->player.prop == NULL) {
        objects->actor_tick_status =
            RUNTIME_STAGE_ACTOR_TICK_MISSING_PLAYER;
        return false;
    }
    /* The exact materializer excludes negative-pad, guard-assigned and
     * embedded default-object branches until their canonical owner/reparent
     * passes are live.  Those authored records are not active props yet; gate
     * on the complete ready subset rather than the raw setup record count. */
    if (objects->report.failed != 0U
            || objects->live_count != objects->report.ready
            || objects->report.constructed != objects->report.ready) {
        objects->actor_tick_status =
            RUNTIME_STAGE_ACTOR_TICK_MISSING_OBJECTS;
        return false;
    }
    expected_doors = ge_original_stage_interactive_expected_door_count(
        &objects->interactive);
    if (objects->live_door_count != expected_doors
            || (expected_doors != 0U
                && (objects->interactive.loaded == 0U
                    || objects->interactive.report.constructed_doors
                        != expected_doors))) {
        objects->actor_tick_status =
            RUNTIME_STAGE_ACTOR_TICK_MISSING_DOORS;
        return false;
    }
    guard_root_count = ge_original_stage_guard_runtime_root_prop_count(
        objects->guards);
    item_active_count = ge_original_stage_interactive_root_item_count(
        &objects->interactive);
    objects->active_prop_count = objects->root_live_count + guard_root_count
        + objects->live_door_count + item_active_count;
    if (objects->active_prop_count == 0U) {
        objects->actor_tick_status =
            RUNTIME_STAGE_ACTOR_TICK_MISSING_OBJECTS;
        return false;
    }
    objects->active_prop_inputs = calloc(objects->active_prop_count,
        sizeof(*objects->active_prop_inputs));
    if (objects->active_prop_inputs == NULL) {
        objects->actor_tick_status =
            RUNTIME_STAGE_ACTOR_TICK_COMPOSE_FAILED;
        return false;
    }
    for (index = 0U; index < objects->entry_count; ++index) {
        RuntimeStageOrdinaryEntry *entry = &objects->entries[index];

        if (!entry->root_active) continue;
        objects->active_prop_inputs[input_index++] =
            (GeOriginalStageActivePropInput) {
                entry->command_index, entry->prop,
                GE_ORIGINAL_STAGE_ACTIVE_PROP_AUTHORED
            };
    }
    for (index = 0U; index < objects->interactive.entry_count; ++index) {
        const GeOriginalStageInteractiveEntry *entry =
            ge_original_stage_interactive_entry(
                &objects->interactive, index);

        if (entry == NULL || !entry->constructed
                || entry->type != PROPDEF_DOOR) continue;
        objects->active_prop_inputs[input_index++] =
            (GeOriginalStageActivePropInput) {
                entry->command_index, entry->prop,
                GE_ORIGINAL_STAGE_ACTIVE_PROP_AUTHORED
            };
    }
    for (index = 0U; index < item_active_count; ++index) {
        size_t command_index;
        void *prop;
        if (!ge_original_stage_interactive_root_item(
                &objects->interactive, index, &command_index, &prop)) {
            objects->actor_tick_status =
                RUNTIME_STAGE_ACTOR_TICK_MISSING_OBJECTS;
            return false;
        }
        objects->active_prop_inputs[input_index++] =
            (GeOriginalStageActivePropInput) {
                command_index, prop,
                GE_ORIGINAL_STAGE_ACTIVE_PROP_AUTHORED
            };
    }
    if (objects->guard_count != 0U) {
        void *prop;
        void *chr;
        if (!ge_original_stage_guard_runtime_actor(
                objects->guards, 0U, &prop, &chr)) {
            objects->actor_tick_status =
                RUNTIME_STAGE_ACTOR_TICK_MISSING_GUARDS;
            return false;
        }
        chr_pool = (struct ChrRecord *)chr;
    }
    for (index = 0U; index < guard_root_count; ++index) {
        size_t command_index;
        void *prop;
        if (!ge_original_stage_guard_runtime_root_prop(
                objects->guards, index, &command_index, &prop)) {
            objects->actor_tick_status =
                RUNTIME_STAGE_ACTOR_TICK_MISSING_GUARDS;
            return false;
        }
        objects->active_prop_inputs[input_index++] =
            (GeOriginalStageActivePropInput) {
                command_index, prop,
                GE_ORIGINAL_STAGE_ACTIVE_PROP_AUTHORED
            };
    }
    if (input_index != objects->active_prop_count || chr_pool == NULL) {
        objects->actor_tick_status =
            RUNTIME_STAGE_ACTOR_TICK_MISSING_GUARDS;
        return false;
    }
    objects->active_prop_status = ge_original_stage_active_props_compose(
        &objects->active_props, objects->setup, intro->player.prop, chr_pool,
        objects->guard_count, objects->active_prop_inputs,
        objects->active_prop_count);
    if (objects->active_prop_status != GE_ORIGINAL_STAGE_ACTIVE_PROP_OK) {
        objects->actor_tick_status =
            RUNTIME_STAGE_ACTOR_TICK_COMPOSE_FAILED;
        return false;
    }
    if (ge_original_stage_mission_runtime_begin(
            &objects->mission_runtime, objects->setup)
            != GE_ORIGINAL_STAGE_MISSION_RUNTIME_OK) {
        objects->actor_tick_status =
            RUNTIME_STAGE_ACTOR_TICK_MISSION_FAILED;
        return false;
    }
    objects->actor_tick_status = RUNTIME_STAGE_ACTOR_TICK_READY;
    return true;
}

static void __attribute__((unused)) materialize_dam_world_objects(
                                          RuntimeDamWorldObjects *objects,
                                          GeAssetPack *asset_pack,
                                          const GeOriginalStageSetupRuntime *setup,
                                          RuntimeDamCollision *collision,
                                          RuntimeDamPreview *preview)
{
    GeOriginalDamWorldProviders providers;
    GeOriginalDefaultObjectProviders object_providers;
    GeOriginalDoorProviders door_providers;
    const GeAssetPackEntry *model_entry;
    const GeAssetPackEntry *modembox_entry;
    const GeAssetPackEntry *satdish_entry;

    objects->collision = collision;
    objects->preview = preview;
    objects->setup = setup;
    if (asset_pack != NULL)
        objects->pitem_models = ge_original_pitem_model_provider_create(
            asset_pack, 1U, DAM_ALARM_OBJECT_COUNT,
            &objects->pitem_model_status);
    model_entry = asset_pack != NULL
        ? ge_asset_pack_find(asset_pack, DAM_GREATGUARD2_ASSET_PATH) : NULL;
    objects->guard_model_loaded = model_entry != NULL
        && model_entry->data_size == sizeof(objects->guard_model_blob)
        && ge_asset_pack_read(asset_pack, DAM_GREATGUARD2_ASSET_PATH,
                              objects->guard_model_blob,
                              sizeof(objects->guard_model_blob), 0U)
               == GE_ASSET_PACK_OK;
    model_entry = asset_pack != NULL
        ? ge_asset_pack_find(asset_pack, DAM_CHRKALASH_ASSET_PATH) : NULL;
    objects->guard_weapon_model_loaded = model_entry != NULL
        && model_entry->data_size == sizeof(objects->guard_weapon_model_blob)
        && ge_asset_pack_read(asset_pack, DAM_CHRKALASH_ASSET_PATH,
                              objects->guard_weapon_model_blob,
                              sizeof(objects->guard_weapon_model_blob), 0U)
               == GE_ASSET_PACK_OK;
    model_entry = asset_pack != NULL
        ? ge_asset_pack_find(asset_pack, GE_ORIGINAL_MODEL62_ASSET_PATH) : NULL;
    if (model_entry != NULL
            && model_entry->data_size == sizeof(objects->model62_blob)
            && ge_asset_pack_read(asset_pack, GE_ORIGINAL_MODEL62_ASSET_PATH,
                                  objects->model62_blob,
                                  sizeof(objects->model62_blob), 0U)
                   == GE_ASSET_PACK_OK) {
        objects->model62 = ge_original_model62_create(
            objects->model62_blob, sizeof(objects->model62_blob),
            &objects->model62_status);
    }
    model_entry = asset_pack != NULL
        ? ge_asset_pack_find(asset_pack, GE_ORIGINAL_MODEL104_ASSET_PATH) : NULL;
    if (model_entry != NULL
            && model_entry->data_size == sizeof(objects->model104_blob)
            && ge_asset_pack_read(asset_pack, GE_ORIGINAL_MODEL104_ASSET_PATH,
                                  objects->model104_blob,
                                  sizeof(objects->model104_blob), 0U)
                   == GE_ASSET_PACK_OK) {
        size_t index;
        for (index = 0U; index < DAM_WINDOW_MODEL_INSTANCE_COUNT; ++index) {
            objects->model104[index] = ge_original_model104_create(
                objects->model104_blob, sizeof(objects->model104_blob),
                &objects->model104_status);
            if (objects->model104[index] == NULL) break;
        }
    }
    model_entry = asset_pack != NULL
        ? ge_asset_pack_find(asset_pack, GE_ORIGINAL_MODEL178_ASSET_PATH) : NULL;
    if (model_entry != NULL
            && model_entry->data_size == sizeof(objects->model178_blob)
            && ge_asset_pack_read(asset_pack, GE_ORIGINAL_MODEL178_ASSET_PATH,
                                  objects->model178_blob,
                                  sizeof(objects->model178_blob), 0U)
                   == GE_ASSET_PACK_OK) {
        objects->model178[0] = ge_original_model178_create(
            objects->model178_blob, sizeof(objects->model178_blob),
            &objects->model178_status);
        if (objects->model178[0] != NULL)
            objects->model178[1] = ge_original_model178_create(
                objects->model178_blob, sizeof(objects->model178_blob),
                &objects->model178_status);
    }
    modembox_entry = asset_pack != NULL
        ? ge_asset_pack_find(
            asset_pack, GE_ORIGINAL_MODEMBOX_ASSET_PATH) : NULL;
    satdish_entry = asset_pack != NULL
        ? ge_asset_pack_find(
            asset_pack, GE_ORIGINAL_SATDISH_ASSET_PATH) : NULL;
    if (modembox_entry != NULL && satdish_entry != NULL
            && modembox_entry->data_size == sizeof(objects->modembox_blob)
            && satdish_entry->data_size == sizeof(objects->satdish_blob)
            && ge_asset_pack_read(
                asset_pack, GE_ORIGINAL_MODEMBOX_ASSET_PATH,
                objects->modembox_blob, sizeof(objects->modembox_blob), 0U)
                == GE_ASSET_PACK_OK
            && ge_asset_pack_read(
                asset_pack, GE_ORIGINAL_SATDISH_ASSET_PATH,
                objects->satdish_blob, sizeof(objects->satdish_blob), 0U)
                == GE_ASSET_PACK_OK) {
        objects->objective_models =
            ge_original_dam_objective_models_create(
                objects->modembox_blob, sizeof(objects->modembox_blob),
                objects->satdish_blob, sizeof(objects->satdish_blob),
                &objects->objective_models_status);
    }
    memset(&providers, 0, sizeof(providers));
    providers.context = objects;
    providers.allocate_definition = dam_world_allocate_definition;
    providers.allocate_prop = dam_world_allocate_prop;
    providers.activate_prop = dam_world_activate_prop;
    providers.enable_prop = dam_world_enable_prop;
    providers.register_room = dam_world_register_room;
    ge_dam_setup_world_materializer_bind(&providers, &objects->state);
    ge_dam_setup_world_materialize_first_authored();
    (void)ge_dam_setup_world_materialize_linked_door();
    (void)ge_dam_setup_world_materialize_spawn_windows();
    (void)ge_dam_setup_world_materialize_mission_tags(
        &objects->mission_tags);

    memset(&object_providers, 0, sizeof(object_providers));
    object_providers.context = objects;
    object_providers.model_load = dam_world_model_load;
    object_providers.get_player_count = dam_world_player_count;
    object_providers.get_scenario = dam_world_scenario;
    object_providers.resolve_model_instance = dam_world_resolve_model;
    object_providers.allocate_collision = dam_world_allocate_collision;
    object_providers.get_floor_y = dam_world_floor_y;
    object_providers.get_room_object_bounds = dam_world_room_object_bounds;
    object_providers.walk_tiles = dam_world_walk_tiles;
    object_providers.get_tile_rgb = dam_world_tile_rgb;
    objects->object_providers = object_providers;
    ge_original_default_object_bind(&object_providers,
                                    &objects->glass_object);
    objects->glass_object_status = ge_original_default_object_construct_standard(
        objects->state.first_glass.definition,
        objects->state.first_glass.command_index);
    if (objects->glass_object_status == GE_ORIGINAL_DEFAULT_OBJECT_OK) {
        objects->glass_placement_status = ge_original_default_object_place_standard(
            objects->state.first_glass.definition);
        if (objects->glass_placement_status == GE_ORIGINAL_DEFAULT_OBJECT_OK)
            ge_dam_setup_world_activate_entry(&objects->state.first_glass);
    }
    {
        size_t index;
        for (index = 0U; index < GE_ORIGINAL_DAM_SPAWN_WINDOW_COUNT;
                ++index) {
            ge_original_default_object_bind(
                &object_providers, &objects->spawn_windows[index]);
            objects->spawn_window_status[index] =
                ge_original_default_object_construct_standard(
                    objects->state.spawn_windows[index].definition,
                    objects->state.spawn_windows[index].command_index);
            if (objects->spawn_window_status[index]
                    != GE_ORIGINAL_DEFAULT_OBJECT_OK) continue;
            objects->spawn_window_placement_status[index] =
                ge_original_default_object_place_standard(
                    objects->state.spawn_windows[index].definition);
            if (objects->spawn_window_placement_status[index]
                    == GE_ORIGINAL_DEFAULT_OBJECT_OK)
                ge_dam_setup_world_activate_entry(
                    &objects->state.spawn_windows[index]);
        }
    }
    ge_original_default_object_bind(&object_providers,
                                    &objects->default_object);
    objects->default_object_status = ge_original_default_object_construct_standard(
        objects->state.first_object.definition,
        objects->state.first_object.command_index);
    if (objects->default_object_status == GE_ORIGINAL_DEFAULT_OBJECT_OK) {
        objects->placement_status = ge_original_default_object_place_standard(
            objects->state.first_object.definition);
        if (objects->placement_status == GE_ORIGINAL_DEFAULT_OBJECT_OK)
            ge_dam_setup_world_activate_entry(&objects->state.first_object);
    }
    {
        GeOriginalDamWorldEntry *mission_entries[
            DAM_MISSION_TAG_OBJECT_COUNT] = {
                &objects->mission_tags.tag5_object,
                &objects->mission_tags.tag4_object,
            };
        size_t index;

        for (index = 0U; index < DAM_MISSION_TAG_OBJECT_COUNT; ++index) {
            ge_original_default_object_bind(
                &object_providers, &objects->mission_objects[index]);
            objects->mission_object_status[index] =
                ge_original_default_object_construct_standard(
                    mission_entries[index]->definition,
                    mission_entries[index]->command_index);
            if (objects->mission_object_status[index]
                    != GE_ORIGINAL_DEFAULT_OBJECT_OK) continue;
            objects->mission_placement_status[index] =
                ge_original_default_object_place_standard(
                    mission_entries[index]->definition);
        }
    }
    if (materialize_dam_alarm_objects(objects, setup))
        (void)initialize_dam_objectives(objects);
    memset(&door_providers, 0, sizeof(door_providers));
    door_providers.context = objects;
    door_providers.model_load = dam_world_model_load;
    door_providers.resolve_model_instance = dam_world_resolve_model;
    door_providers.allocate_collision = dam_world_allocate_collision;
    door_providers.walk_tiles = dam_world_walk_tiles;
    door_providers.get_tile_rgb = dam_world_tile_rgb;
    door_providers.register_room = dam_world_register_room;
    door_providers.find_portal = dam_world_find_portal;
    door_providers.set_portal_open = dam_world_set_portal_open;
    ge_original_door_bind(&door_providers, &objects->door_scratch);
    objects->door_status[0] = ge_original_door_construct(
        objects->state.first_door.definition,
        objects->state.first_door.command_index);
    objects->doors[0] = objects->door_scratch;
    if (objects->door_status[0] == GE_ORIGINAL_DOOR_OK)
        ge_dam_setup_world_activate_entry(&objects->state.first_door);
    objects->door_status[1] = ge_original_door_construct(
        objects->state.second_door.definition,
        objects->state.second_door.command_index);
    objects->doors[1] = objects->door_scratch;
    if (objects->door_status[1] == GE_ORIGINAL_DOOR_OK)
        ge_dam_setup_world_activate_entry(&objects->state.second_door);
    objects->doors_linked = ge_dam_setup_world_link_authored_doors()
        && ge_original_door_runtime_link_pair(
            objects->state.first_door.definition,
            objects->state.second_door.definition);
}

static bool install_dam_world_object_scenes(RuntimeDamPreview *preview,
                                            RuntimeDamWorldObjects *objects)
{
    GeOriginalModelSceneInput inputs[DAM_MODEL_SCENE_INPUT_CAPACITY];
    GeOriginalModelScene queries[DAM_MODEL_SCENE_INPUT_CAPACITY];
    GeOriginalDamGuardScene guard_query;
    GeDamRoomWorldVertex *vertices = NULL;
    GeDamRoomDrawBatch *batches = NULL;
    Ge3dsSceneTextureSlot *candidate_slots = NULL;
    Ge3dsSceneTextures candidate_textures = {0};
    size_t vertex_count = 0U;
    size_t batch_count = 0U;
    size_t triangle_count = 0U;
    size_t input_index;
    size_t input_count = DAM_FIXED_MODEL_SCENE_INPUT_COUNT;
    size_t input_vertex_offsets[DAM_MODEL_SCENE_INPUT_CAPACITY] = {0};
    size_t input_batch_offsets[DAM_MODEL_SCENE_INPUT_CAPACITY] = {0};
    size_t guard_vertex_offset = 0U;
    size_t guard_batch_offset = 0U;
    RuntimeDamOverlaySegment door_candidates[2] = {{0}};
    RuntimeDamOverlaySegment guard_candidate = {0};

    if (objects != NULL) objects->model_scene_install_failure = 0U;
    if (preview == NULL || objects == NULL
            || preview->dynamic_scene.initialized == 0U
            || preview->texture_cache == NULL
            || objects->model62 == NULL || objects->model104[0] == NULL
            || objects->model104[DAM_WINDOW_MODEL_INSTANCE_COUNT - 1U] == NULL
            || objects->model178[0] == NULL || objects->model178[1] == NULL
            || objects->objective_models == NULL
            || !objects->guard_model_loaded
            || !objects->guard_weapon_model_loaded
            || !preview->original_camera_ready
            || !objects->glass_object.placement_completed
            || !objects->default_object.placement_completed
            || !objects->mission_objects[0].placement_completed
            || !objects->mission_objects[1].placement_completed
            || objects->alarm_count != DAM_ALARM_OBJECT_COUNT
            || !objects->doors[0].constructed
            || !objects->doors[1].constructed) {
        if (objects != NULL) objects->model_scene_install_failure = 1U;
        return false;
    }
    memset(inputs, 0, sizeof(inputs));
    inputs[0].blob = objects->model104_blob;
    inputs[0].blob_size = sizeof(objects->model104_blob);
    inputs[0].primary_offset = UINT32_C(0x138);
    inputs[0].secondary_offset = UINT32_C(0x150);
    inputs[0].segment4_offset = UINT32_C(0x90);
    inputs[1].blob = objects->model62_blob;
    inputs[1].blob_size = sizeof(objects->model62_blob);
    inputs[1].primary_offset = UINT32_C(0x5c8);
    inputs[1].secondary_offset = UINT32_C(0x6b8);
    inputs[1].segment4_offset = GE_ORIGINAL_MODEL_SCENE_NO_LIST;
    if (ge_original_door_scene_prepare(
            objects->state.first_door.definition,
            objects->model178_blob, sizeof(objects->model178_blob),
            &objects->door_scenes[0]) != GE_ORIGINAL_DOOR_SCENE_OK
            || ge_original_door_scene_prepare(
                objects->state.second_door.definition,
                objects->model178_blob, sizeof(objects->model178_blob),
                &objects->door_scenes[1]) != GE_ORIGINAL_DOOR_SCENE_OK) {
        objects->model_scene_install_failure = 2U;
        return false;
    }
    inputs[2] = objects->door_scenes[0].input;
    inputs[3] = objects->door_scenes[1].input;
    inputs[DAM_MATERIALIZED_OBJECT_CAPACITY].blob =
        objects->modembox_blob;
    inputs[DAM_MATERIALIZED_OBJECT_CAPACITY].blob_size =
        sizeof(objects->modembox_blob);
    inputs[DAM_MATERIALIZED_OBJECT_CAPACITY].primary_offset =
        GE_ORIGINAL_MODEMBOX_PRIMARY_GDL_OFFSET;
    inputs[DAM_MATERIALIZED_OBJECT_CAPACITY].secondary_offset =
        GE_ORIGINAL_MODEL_SCENE_NO_LIST;
    inputs[DAM_MATERIALIZED_OBJECT_CAPACITY].segment4_offset =
        GE_ORIGINAL_MODEMBOX_PRIMARY_VERTEX_OFFSET;
    inputs[DAM_MATERIALIZED_OBJECT_CAPACITY + 1U].blob =
        objects->modembox_blob;
    inputs[DAM_MATERIALIZED_OBJECT_CAPACITY + 1U].blob_size =
        sizeof(objects->modembox_blob);
    inputs[DAM_MATERIALIZED_OBJECT_CAPACITY + 1U].primary_offset =
        GE_ORIGINAL_MODEMBOX_SCREEN_GDL_OFFSET;
    inputs[DAM_MATERIALIZED_OBJECT_CAPACITY + 1U].secondary_offset =
        GE_ORIGINAL_MODEL_SCENE_NO_LIST;
    inputs[DAM_MATERIALIZED_OBJECT_CAPACITY + 1U].segment4_offset =
        GE_ORIGINAL_MODEMBOX_SCREEN_VERTEX_OFFSET;
    inputs[DAM_MATERIALIZED_OBJECT_CAPACITY + 2U].blob =
        objects->satdish_blob;
    inputs[DAM_MATERIALIZED_OBJECT_CAPACITY + 2U].blob_size =
        sizeof(objects->satdish_blob);
    inputs[DAM_MATERIALIZED_OBJECT_CAPACITY + 2U].primary_offset =
        GE_ORIGINAL_SATDISH_PRIMARY_GDL_OFFSET;
    inputs[DAM_MATERIALIZED_OBJECT_CAPACITY + 2U].secondary_offset =
        GE_ORIGINAL_SATDISH_SECONDARY_GDL_OFFSET;
    inputs[DAM_MATERIALIZED_OBJECT_CAPACITY + 2U].segment4_offset =
        GE_ORIGINAL_SATDISH_VERTEX_OFFSET;
    {
        void *definitions[DAM_MODEL_SCENE_INPUT_CAPACITY] = {
            objects->state.first_glass.definition,
            objects->state.first_object.definition,
            objects->state.first_door.definition,
            objects->state.second_door.definition,
        };
        void *props[DAM_MODEL_SCENE_INPUT_CAPACITY] = {
            objects->state.first_glass.prop,
            objects->state.first_object.prop,
            objects->state.first_door.prop,
            objects->state.second_door.prop,
        };
        for (input_index = DAM_BASE_OBJECT_COUNT;
                input_index < DAM_MATERIALIZED_OBJECT_CAPACITY;
                ++input_index) {
            const size_t window_index = input_index - DAM_BASE_OBJECT_COUNT;
            definitions[input_index] =
                objects->state.spawn_windows[window_index].definition;
            props[input_index] = objects->state.spawn_windows[window_index].prop;
            inputs[input_index] = inputs[0];
        }
        definitions[DAM_MATERIALIZED_OBJECT_CAPACITY] =
            objects->mission_tags.tag5_object.definition;
        props[DAM_MATERIALIZED_OBJECT_CAPACITY] =
            objects->mission_tags.tag5_object.prop;
        definitions[DAM_MATERIALIZED_OBJECT_CAPACITY + 1U] =
            objects->mission_tags.tag5_object.definition;
        props[DAM_MATERIALIZED_OBJECT_CAPACITY + 1U] =
            objects->mission_tags.tag5_object.prop;
        definitions[DAM_MATERIALIZED_OBJECT_CAPACITY + 2U] =
            objects->mission_tags.tag4_object.definition;
        props[DAM_MATERIALIZED_OBJECT_CAPACITY + 2U] =
            objects->mission_tags.tag4_object.prop;
        for (input_index = 0U;
                input_index < DAM_FIXED_MODEL_SCENE_INPUT_COUNT;
                ++input_index) {
            uint8_t room;
            if (input_index == 2U || input_index == 3U) continue;
            if (!ge_original_prop_state_object_scene_transform(
                    definitions[input_index], props[input_index],
                    inputs[input_index].matrix,
                    inputs[input_index].position, &room)) {
                objects->model_scene_install_failure = 3U;
                return false;
            }
            inputs[input_index].room_id = room;
        }
    }
    for (input_index = 0U; input_index < DAM_ALARM_OBJECT_COUNT;
            ++input_index) {
        RuntimeDamAlarmObject *alarm = &objects->alarms[input_index];
        float matrix[4][4];
        float position[3];
        uint8_t room;
        size_t part_index;
        size_t part_count;
        if (!alarm->live || alarm->definition == NULL || alarm->prop == NULL
                || !ge_original_prop_state_object_scene_transform(
                    alarm->definition, alarm->prop, matrix, position, &room)) {
            objects->model_scene_install_failure = 4U;
            return false;
        }
        part_count = ge_original_pitem_model_scene_part_count(
            objects->pitem_models, alarm->model_id);
        if (part_count == 0U
                || part_count > DAM_MODEL_SCENE_INPUT_CAPACITY - input_count) {
            objects->model_scene_install_failure = 5U;
            return false;
        }
        for (part_index = 0U; part_index < part_count; ++part_index) {
            GeOriginalPitemModelScenePart part;
            GeOriginalModelSceneInput *input = &inputs[input_count];
            if (!ge_original_pitem_model_scene_part(
                    objects->pitem_models, alarm->model_id,
                    part_index, &part)) {
                objects->model_scene_install_failure = 6U;
                return false;
            }
            input->blob = part.blob;
            input->blob_size = part.blob_size;
            input->primary_offset = part.primary_offset;
            input->secondary_offset = part.secondary_offset;
            input->segment4_offset = part.segment4_offset;
            input->room_id = room;
            memcpy(input->matrix, matrix, sizeof(input->matrix));
            memcpy(input->position, position, sizeof(input->position));
            ++input_count;
        }
    }
    for (input_index = 0U;
            input_index < input_count; ++input_index) {
        input_vertex_offsets[input_index] = vertex_count;
        input_batch_offsets[input_index] = batch_count;
        objects->model_scene_status = ge_original_model_scene_build(
            &inputs[input_index], NULL, &queries[input_index]);
        if (objects->model_scene_status
                != GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED
                || queries[input_index].required_vertex_count
                    > SIZE_MAX - vertex_count
                || queries[input_index].required_batch_count
                    > SIZE_MAX - batch_count
                || queries[input_index].triangle_count
                    > SIZE_MAX - triangle_count) {
            objects->model_scene_install_failure = 7U;
            return false;
        }
        vertex_count += queries[input_index].required_vertex_count;
        batch_count += queries[input_index].required_batch_count;
        triangle_count += queries[input_index].triangle_count;
    }
    guard_vertex_offset = vertex_count;
    guard_batch_offset = batch_count;
    /* Preserve the exact insertion point even when every guard is currently
     * culled. A later authored visibility/model-relation change can then
     * install just this final overlay segment without rebuilding the rooms or
     * immutable ordinary props. */
    guard_candidate.vertex_offset = guard_vertex_offset;
    guard_candidate.batch_offset = guard_batch_offset;
    if (ge_original_dam_guard_scene_build_cached_with_weapons(
            &objects->guard_scene_cache,
            objects->guard_model_blob, sizeof(objects->guard_model_blob),
            objects->guard_weapon_model_blob,
            sizeof(objects->guard_weapon_model_blob),
            preview->original_camera_view_to_world, NULL, &guard_query)
            != GE_ORIGINAL_DAM_GUARD_SCENE_CAPACITY_EXCEEDED
            || guard_query.required_vertex_count > SIZE_MAX - vertex_count
            || guard_query.required_batch_count > SIZE_MAX - batch_count
            || guard_query.triangle_count > SIZE_MAX - triangle_count)
        return false;
    vertex_count += guard_query.required_vertex_count;
    batch_count += guard_query.required_batch_count;
    triangle_count += guard_query.triangle_count;
    vertices = malloc(vertex_count * sizeof(*vertices));
    batches = malloc(batch_count * sizeof(*batches));
    if (!objects->model_scene_ready)
        candidate_slots = calloc(DAM_SCENE_TEXTURE_CAPACITY,
                                 sizeof(*candidate_slots));
    if (vertices == NULL || batches == NULL
            || (!objects->model_scene_ready && candidate_slots == NULL))
        goto fail;
    vertex_count = 0U;
    batch_count = 0U;
    for (input_index = 0U;
            input_index < input_count; ++input_index) {
        GeDamRoomSceneStorage storage = {
            vertices + vertex_count,
            queries[input_index].required_vertex_count,
            batches + batch_count,
            queries[input_index].required_batch_count,
        };
        GeOriginalModelScene built;
        size_t local_batch;
        objects->model_scene_status = ge_original_model_scene_build(
            &inputs[input_index], &storage, &built);
        if (objects->model_scene_status != GE_ORIGINAL_MODEL_SCENE_OK)
            goto fail;
        for (local_batch = 0U; local_batch < built.batch_count; ++local_batch)
            batches[batch_count + local_batch].first_vertex += vertex_count;
        vertex_count += built.vertex_count;
        batch_count += built.batch_count;
    }
    {
        GeDamRoomSceneStorage guard_storage = {
            vertices + vertex_count,
            guard_query.required_vertex_count,
            batches + batch_count,
            guard_query.required_batch_count,
        };
        const size_t guard_vertex_base = vertex_count;
        const size_t guard_batch_base = batch_count;
        size_t local_batch;
        if (ge_original_dam_guard_scene_build_cached_with_weapons(
                &objects->guard_scene_cache,
                objects->guard_model_blob, sizeof(objects->guard_model_blob),
                objects->guard_weapon_model_blob,
                sizeof(objects->guard_weapon_model_blob),
                preview->original_camera_view_to_world, &guard_storage,
                &objects->guard_scene) != GE_ORIGINAL_DAM_GUARD_SCENE_OK)
            goto fail;
        for (local_batch = 0U;
                local_batch < objects->guard_scene.batch_count; ++local_batch)
            batches[guard_batch_base + local_batch].first_vertex
                += guard_vertex_base;
        vertex_count += objects->guard_scene.vertex_count;
        batch_count += objects->guard_scene.batch_count;
    }
    if (!dam_overlay_segment_capture(
            &door_candidates[0], vertices,
            input_vertex_offsets[2], queries[2].required_vertex_count,
            batches, input_batch_offsets[2],
            queries[2].required_batch_count)
            || !dam_overlay_segment_capture(
                &door_candidates[1], vertices,
                input_vertex_offsets[3], queries[3].required_vertex_count,
                batches, input_batch_offsets[3],
                queries[3].required_batch_count)
            || ((objects->guard_scene.vertex_count != 0U
                    || objects->guard_scene.batch_count != 0U)
                && !dam_overlay_segment_capture(
                    &guard_candidate, vertices, guard_vertex_offset,
                    objects->guard_scene.vertex_count, batches,
                    guard_batch_offset, objects->guard_scene.batch_count)))
        goto fail;
    objects->model_overlay_status = ge_dam_dynamic_scene_set_overlay(
        &preview->dynamic_scene, vertices, vertex_count, batches, batch_count);
    if (objects->model_overlay_status != GE_DAM_DYNAMIC_SCENE_OK) goto fail;
    /* set_overlay swaps and frees the prior room-only publication. Keep the
     * preview aliases valid even if texture residency is only partial. */
    preview->source_vertices = preview->dynamic_scene.vertices;
    preview->batches = preview->dynamic_scene.batches;
    preview->source_vertex_count = preview->dynamic_scene.scene.vertex_count;
    preview->batch_count = preview->dynamic_scene.scene.batch_count;
    preview->triangles = preview->dynamic_scene.scene.triangle_count;
    preview->draws = preview->dynamic_scene.scene.batch_count;
    if (!objects->model_scene_ready) {
        Ge3dsSceneTextureStatus texture_status = ge_3ds_scene_textures_load(
            preview->texture_cache, preview->dynamic_scene.batches,
            preview->dynamic_scene.scene.batch_count, candidate_slots,
            DAM_SCENE_TEXTURE_CAPACITY, &candidate_textures);
        if (texture_status != GE_3DS_SCENE_TEXTURE_OK
                && texture_status != GE_3DS_SCENE_TEXTURE_PARTIAL) goto fail;
        ge_3ds_scene_textures_close(&dam_scene_textures);
        memcpy(dam_scene_texture_slots, candidate_slots,
               DAM_SCENE_TEXTURE_CAPACITY * sizeof(*candidate_slots));
        dam_scene_textures = candidate_textures;
        dam_scene_textures.slots = dam_scene_texture_slots;
        candidate_textures.slots = NULL;
    }
    preview->scene_textures = &dam_scene_textures;
    objects->model_scene_vertices = vertex_count;
    objects->model_scene_batches = batch_count;
    objects->model_scene_triangles = triangle_count;
    objects->model_scene_ready = true;
    for (input_index = 0U; input_index < input_count; input_index++)
        objects->model_input_rooms[input_index] = inputs[input_index].room_id;
    dam_overlay_segment_close(&objects->door_overlay[0]);
    dam_overlay_segment_close(&objects->door_overlay[1]);
    dam_overlay_segment_close(&objects->guard_overlay);
    objects->door_overlay[0] = door_candidates[0];
    objects->door_overlay[1] = door_candidates[1];
    objects->guard_overlay = guard_candidate;
    memset(door_candidates, 0, sizeof(door_candidates));
    memset(&guard_candidate, 0, sizeof(guard_candidate));
    objects->overlay_full_rebuilds++;
    objects->installed_door_scene_generation[0] =
        objects->door_scenes[0].runtime.generation;
    objects->installed_door_scene_generation[1] =
        objects->door_scenes[1].runtime.generation;
#if defined(GE_DAM_FULL_PROPS_LIVE)
    /* These are the four model inputs just accepted by the dynamic room
     * publication. Mirror the original renderer contract only for inputs in
     * the portal-visible room set; the unchanged onscreen-list body below
     * remains the sole owner of list membership and zDepth ordering. */
    dam_publish_rendered_prop(preview, objects->state.first_glass.prop,
                              inputs[0].room_id);
    dam_publish_rendered_prop(preview, objects->state.first_object.prop,
                              inputs[1].room_id);
    dam_publish_rendered_prop(preview, objects->state.first_door.prop,
                              inputs[2].room_id);
    dam_publish_rendered_prop(preview, objects->state.second_door.prop,
                              inputs[3].room_id);
    for (input_index = 0U;
            input_index < GE_ORIGINAL_DAM_SPAWN_WINDOW_COUNT; ++input_index)
        dam_publish_rendered_prop(
            preview, objects->state.spawn_windows[input_index].prop,
            inputs[DAM_BASE_OBJECT_COUNT + input_index].room_id);
    dam_publish_rendered_prop(
        preview, objects->mission_tags.tag5_object.prop,
        inputs[DAM_MATERIALIZED_OBJECT_CAPACITY].room_id);
    dam_publish_rendered_prop(
        preview, objects->mission_tags.tag4_object.prop,
        inputs[DAM_MATERIALIZED_OBJECT_CAPACITY + 2U].room_id);
    for (input_index = 0U; input_index < DAM_ALARM_OBJECT_COUNT;
            ++input_index)
        dam_publish_rendered_prop(
            preview, objects->alarms[input_index].prop,
            objects->alarms[input_index].room);
#endif
    (void)ge_dam_setup_world_activate_entry(
        &objects->mission_tags.tag5_object);
    (void)ge_dam_setup_world_activate_entry(
        &objects->mission_tags.tag4_object);
    ge_3ds_scene_textures_close(&candidate_textures);
    free(candidate_slots);
    free(batches);
    free(vertices);
    return true;

fail:
    dam_overlay_segment_close(&door_candidates[0]);
    dam_overlay_segment_close(&door_candidates[1]);
    dam_overlay_segment_close(&guard_candidate);
    ge_3ds_scene_textures_close(&candidate_textures);
    free(candidate_slots);
    free(batches);
    free(vertices);
    return false;
}

static bool update_stage_guard_visibility(
    RuntimeStageOrdinaryObjects *objects, bool *changed)
{
    const uint64_t started = svcGetSystemTick();
    size_t guard_index;
    size_t guard_count;
    if (changed != NULL) *changed = false;
    if (objects == NULL || objects->guards == NULL
            || objects->preview == NULL
            || !objects->preview->original_camera_ready) return true;
    guard_count = ge_original_stage_guard_runtime_count(objects->guards);
    for (guard_index = 0U; guard_index < guard_count; ++guard_index) {
        uint8_t room, was_visible;
        const int visible = ge_original_stage_guard_runtime_room_visibility(
                objects->guards, guard_index, &room, &was_visible)
            ? dam_visibility_contains_room(objects->preview, room)
            : -1;
        if (visible < 0) return false;
        if ((was_visible != 0U) == (visible != 0)) continue;
        if (!ge_original_stage_guard_runtime_set_visibility(
                objects->guards, guard_index, visible, room))
            return false;
        if (changed != NULL) *changed = true;
    }
    fine_profile.guard_visibility_update_ticks += svcGetSystemTick() - started;
    return true;
}

static bool stage_interactive_scene_transform(
    const GeOriginalStageInteractiveEntry *entry,
    GeOriginalDoorRuntimePublication *door,
    float matrix[4][4], float position[3], uint8_t *room)
{
    if (entry == NULL || !entry->constructed || entry->type != PROPDEF_DOOR
            || door == NULL || matrix == NULL || position == NULL
            || room == NULL
            || !ge_original_door_runtime_snapshot(entry->definition, door)
            || door->room < 0 || door->room > UINT8_MAX) return false;
    /* The published matrix bank is already the canonical object-to-world
     * segment-3 state.  The model flattener's outer transform therefore stays
     * identity; applying door->matrix/position again would double the authored
     * placement for articulated eye and iris children. */
    memset(matrix, 0, sizeof(door->matrix));
    matrix[0][0] = 1.0f;
    matrix[1][1] = 1.0f;
    matrix[2][2] = 1.0f;
    matrix[3][3] = 1.0f;
    memset(position, 0, sizeof(door->position));
    *room = (uint8_t)door->room;
    return true;
}

static bool stage_ordinary_uses_live_model_matrices(uint8_t type)
{
    switch (type) {
    case PROPDEF_CCTV:
    case PROPDEF_AUTOGUN:
    case PROPDEF_RACK:
    case PROPDEF_VEHICHLE:
    case PROPDEF_AIRCRAFT:
    case PROPDEF_TANK:
        return true;
    default:
        return false;
    }
}

static size_t stage_ordinary_scene_part_count(
    const RuntimeStageOrdinaryObjects *objects,
    const RuntimeStageOrdinaryEntry *entry)
{
    if (objects == NULL || objects->models == NULL || entry == NULL)
        return 0U;
    if (stage_ordinary_uses_live_model_matrices(entry->type))
        return ge_original_pitem_model_instance_scene_part_count(
            objects->models, entry->prepared.model_instance);
    return ge_original_pitem_model_scene_part_count(
        objects->models, entry->model_id);
}

static int stage_ordinary_scene_part(
    const RuntimeStageOrdinaryObjects *objects,
    const RuntimeStageOrdinaryEntry *entry, size_t part_index,
    GeOriginalPitemModelScenePart *part)
{
    if (objects == NULL || objects->models == NULL || entry == NULL
            || part == NULL) return 0;
    if (stage_ordinary_uses_live_model_matrices(entry->type))
        return ge_original_pitem_model_instance_scene_part(
            objects->models, entry->prepared.model_instance,
            part_index, part);
    return ge_original_pitem_model_scene_part(
        objects->models, entry->model_id, part_index, part);
}

typedef struct RuntimeGuardTextureResidency {
    GeTextureCache *cache;
    Ge3dsSceneTextures *candidate;
    Ge3dsSceneTextureReconcileStats *stats;
} RuntimeGuardTextureResidency;

static int include_stage_guard_texture(void *context, uint16_t image_id)
{
    RuntimeGuardTextureResidency *residency = context;
    const Ge3dsSceneTextureStatus status =
        ge_3ds_scene_textures_reconcile_include_image(residency->cache,
            &dam_scene_textures, residency->candidate, image_id,
            residency->stats);
    return status == GE_3DS_SCENE_TEXTURE_OK
        || status == GE_3DS_SCENE_TEXTURE_PARTIAL;
}

static bool prepare_stage_guard_texture_residency(
    RuntimeStageOrdinaryObjects *objects, Ge3dsSceneTextures *candidate,
    Ge3dsSceneTextureReconcileStats *stats)
{
    RuntimeGuardTextureResidency residency = {
        objects->preview->texture_cache, candidate, stats};
    /* Loaded body/head tables describe this stage's actual chosen guards.
     * Import hidden parts during level installation, and borrow them through
     * room-residency changes. No pose, visibility, AI or RNG is advanced. */
    return objects->guard_models == NULL
        || ge_original_character_models_visit_texture_ids(
            objects->guard_models, &residency, include_stage_guard_texture);
}

static bool install_stage_ordinary_object_scenes(
    RuntimeStageOrdinaryObjects *objects)
{
    GeOriginalModelSceneInput *inputs = NULL;
    GeOriginalModelScene *queries = NULL;
    RuntimeStageScenePartRange *candidate_scene_parts = NULL;
    GeOriginalDoorRuntimePublication *door_publications = NULL;
    uint32_t *door_generations = NULL;
    GeDamRoomWorldVertex *vertices = NULL;
    GeDamRoomDrawBatch *batches = NULL;
    Ge3dsSceneTextureSlot *candidate_slots = NULL;
    Ge3dsSceneTextures candidate_textures = {0};
    GeOriginalStageGuardScene guard_query = {0};
    RuntimeDamOverlaySegment door_candidate = {0};
    RuntimeDamOverlaySegment guard_candidate = {0};
    size_t input_count = 0U;
    size_t ordinary_input_count = 0U;
    size_t input_index = 0U;
    size_t vertex_count = 0U;
    size_t batch_count = 0U;
    size_t triangle_count = 0U;
    size_t door_vertex_offset = 0U;
    size_t door_batch_offset = 0U;
    size_t guard_vertex_offset = 0U;
    size_t guard_batch_offset = 0U;
    size_t entry_index;
    /* Last nonempty install: query, ordinary build, guard build, overlay
     * transaction, textures/metadata. Keep these outside per-frame hot loops. */
    uint64_t phase_ticks[6] = {svcGetSystemTick()};
    if (objects != NULL) {
        ++objects->scene_install_attempts;
        objects->scene_install_failure_phase =
            RUNTIME_STAGE_SCENE_INSTALL_ARGUMENTS;
        objects->scene_install_input_count = 0U;
        objects->scene_install_ordinary_input_count = 0U;
        objects->scene_install_required_vertices = 0U;
        objects->scene_install_required_batches = 0U;
        memset(objects->scene_install_phase_ticks, 0,
               sizeof(objects->scene_install_phase_ticks));
    }
    if (objects == NULL || !objects->initialized
            || objects->preview == NULL
            || objects->preview->dynamic_scene.initialized == 0U
            || objects->preview->texture_cache == NULL) return false;
    objects->scene_install_failure_phase =
        RUNTIME_STAGE_SCENE_INSTALL_VISIBILITY;
    if (!update_stage_guard_visibility(objects, NULL)) goto fail_stage_scene;
    for (entry_index = 0U; objects->models != NULL
            && entry_index < objects->entry_count; ++entry_index) {
        if ((objects->entries[entry_index].root_active
                || objects->entries[entry_index].attached_monitor)
                && ge_dam_dynamic_scene_is_resident(
                    &objects->preview->dynamic_scene,
                    objects->entries[entry_index].room))
            input_count += stage_ordinary_scene_part_count(
                objects, &objects->entries[entry_index]);
    }
    for (entry_index = 0U; objects->models != NULL
            && entry_index < objects->interactive.entry_count;
            ++entry_index) {
        const GeOriginalStageInteractiveEntry *entry =
            ge_original_stage_interactive_entry(
                &objects->interactive, entry_index);

        if (entry != NULL && entry->constructed
                && entry->type == PROPDEF_DOOR && entry->room >= 0
                && entry->room <= UINT8_MAX
                && ge_dam_dynamic_scene_is_resident(
                    &objects->preview->dynamic_scene,
                    (uint8_t)entry->room))
            input_count += ge_original_pitem_model_scene_part_count(
                objects->models, entry->model_id);
    }
    objects->scene_install_input_count = input_count;
    objects->scene_install_failure_phase =
        RUNTIME_STAGE_SCENE_INSTALL_GUARD_QUERY;
    if (objects->guards != NULL && objects->preview->original_camera_ready) {
        objects->guard_status =
            ge_original_stage_guard_runtime_update_matrices(
                objects->guards,
                objects->preview->original_camera_view);
        if (objects->guard_status != GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK)
            goto fail_stage_scene;
        objects->guard_status =
            ge_original_stage_guard_runtime_build_scene_cached(
                objects->guards, &objects->guard_scene_cache,
                runtime_eye_space_identity,
                NULL, &guard_query);
        if (objects->guard_status
                != GE_ORIGINAL_STAGE_GUARD_RUNTIME_SCENE_CAPACITY_EXCEEDED)
            goto fail_stage_scene;
    }
    if (input_count == 0U && guard_query.required_vertex_count == 0U
            && guard_query.required_batch_count == 0U) {
        objects->overlay_status = ge_dam_dynamic_scene_set_overlay(
            &objects->preview->dynamic_scene, NULL, 0U, NULL, 0U);
        if (objects->overlay_status != GE_DAM_DYNAMIC_SCENE_OK)
            return false;
        objects->preview->source_vertices =
            objects->preview->dynamic_scene.vertices;
        objects->preview->batches = objects->preview->dynamic_scene.batches;
        objects->preview->source_vertex_count =
            objects->preview->dynamic_scene.scene.vertex_count;
        objects->preview->batch_count =
            objects->preview->dynamic_scene.scene.batch_count;
        objects->preview->triangles =
            objects->preview->dynamic_scene.scene.triangle_count;
        objects->preview->draws =
            objects->preview->dynamic_scene.scene.batch_count;
        candidate_slots = calloc(DAM_SCENE_TEXTURE_CAPACITY,
                                 sizeof(*candidate_slots));
        if (candidate_slots == NULL) goto fail_stage_scene;
        {
            Ge3dsSceneTextureReconcileStats texture_stats = {0};
            Ge3dsSceneTextureStatus texture_status =
                ge_3ds_scene_textures_reconcile_prepare(
                    objects->preview->texture_cache,
                    objects->preview->dynamic_scene.batches,
                    objects->preview->dynamic_scene.scene.batch_count,
                    &dam_scene_textures,
                    candidate_slots, DAM_SCENE_TEXTURE_CAPACITY,
                    &candidate_textures, &texture_stats);
            if (texture_status != GE_3DS_SCENE_TEXTURE_OK
                    && texture_status != GE_3DS_SCENE_TEXTURE_PARTIAL)
                goto fail_stage_scene;
            if (!prepare_stage_guard_texture_residency(
                    objects, &candidate_textures, &texture_stats))
                goto fail_stage_scene;
            texture_status = ge_3ds_scene_textures_reconcile_commit(
                &dam_scene_textures, &candidate_textures, &texture_stats);
            if (texture_status != GE_3DS_SCENE_TEXTURE_OK
                    && texture_status != GE_3DS_SCENE_TEXTURE_PARTIAL)
                goto fail_stage_scene;
        }
        memcpy(dam_scene_texture_slots, candidate_slots,
               DAM_SCENE_TEXTURE_CAPACITY * sizeof(*candidate_slots));
        dam_scene_textures = candidate_textures;
        dam_scene_textures.slots = dam_scene_texture_slots;
        candidate_textures.slots = NULL;
        objects->preview->scene_textures = &dam_scene_textures;
        objects->scene_vertices = 0U;
        objects->scene_batches = 0U;
        objects->scene_triangles = 0U;
        objects->scene_ready = true;
        objects->resident_install_successes =
            objects->preview->dynamic_scene.install_successes;
        objects->resident_eviction_successes =
            objects->preview->dynamic_scene.eviction_successes;
        free(objects->ordinary_scene_parts);
        objects->ordinary_scene_parts = NULL;
        objects->ordinary_scene_part_count = 0U;
        dam_overlay_segment_close(&objects->door_overlay);
        dam_overlay_segment_close(&objects->guard_overlay);
        memset(&objects->guard_scene, 0, sizeof(objects->guard_scene));
        objects->guard_scene.status = GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK;
        ge_3ds_scene_textures_close(&candidate_textures);
        free(candidate_slots);
        ++objects->scene_install_successes;
        objects->scene_install_failure_phase =
            RUNTIME_STAGE_SCENE_INSTALL_NONE;
        return true;
    }
    objects->scene_install_failure_phase =
        RUNTIME_STAGE_SCENE_INSTALL_ALLOCATE_INPUTS;
    if (input_count != 0U) {
        inputs = calloc(input_count, sizeof(*inputs));
        queries = calloc(input_count, sizeof(*queries));
        candidate_scene_parts = calloc(
            input_count, sizeof(*candidate_scene_parts));
        if (inputs == NULL || queries == NULL
                || candidate_scene_parts == NULL) goto fail_stage_scene;
    }
    if (objects->interactive.entry_count != 0U) {
        door_publications = calloc(objects->interactive.entry_count,
                                   sizeof(*door_publications));
        door_generations = calloc(objects->interactive.entry_count,
                                  sizeof(*door_generations));
        if (door_publications == NULL || door_generations == NULL)
            goto fail_stage_scene;
    }
    objects->scene_install_failure_phase =
        RUNTIME_STAGE_SCENE_INSTALL_ORDINARY_QUERY;
    for (entry_index = 0U; entry_index < objects->entry_count; ++entry_index) {
        RuntimeStageOrdinaryEntry *entry = &objects->entries[entry_index];
        float matrix[4][4];
        float position[3];
        uint8_t room;
        size_t part_count;
        size_t part_index;
        if ((!entry->root_active && !entry->attached_monitor)
                || !ge_dam_dynamic_scene_is_resident(
                &objects->preview->dynamic_scene, entry->room)) continue;
        if (entry->attached_monitor) {
            RuntimeStageOrdinaryEntry *owner = resolve_stage_owner_entry(
                objects, entry->owner_command_index);
            GeOriginalStageMonitorAttachmentPublication publication;
            if (owner == NULL
                    || !ge_original_prop_state_object_scene_transform(
                        owner->definition, owner->prop,
                        matrix, position, &room)
                    || !ge_original_stage_monitor_publish_attachment_exact(
                        entry->definition, matrix, position, room,
                        &publication)) goto fail_stage_scene;
            memset(matrix, 0, sizeof(matrix));
            matrix[0][0] = matrix[1][1] = matrix[2][2] = matrix[3][3] = 1.0f;
            memset(position, 0, sizeof(position));
            room = publication.room;
            part_count = stage_ordinary_scene_part_count(objects, entry);
            for (part_index = 0U; part_index < part_count; ++part_index) {
                GeOriginalPitemModelScenePart part;
                GeOriginalModelSceneInput *input = &inputs[input_index];
                if (!ge_original_pitem_model_scene_part(
                        objects->models, entry->model_id, part_index, &part))
                    goto fail_stage_scene;
                candidate_scene_parts[input_index].entry_index = entry_index;
                candidate_scene_parts[input_index].part_index = part_index;
                candidate_scene_parts[input_index].node = part.node;
                input->blob = part.blob;
                input->blob_size = part.blob_size;
                input->primary_offset = part.primary_offset;
                input->secondary_offset = part.secondary_offset;
                input->segment4_offset = part.segment4_offset;
                input->segment3_matrices = publication.segment3_matrices;
                input->segment3_matrix_count = publication.segment3_matrix_count;
                input->room_id = room;
                memcpy(input->matrix, matrix, sizeof(input->matrix));
                memcpy(input->position, position, sizeof(input->position));
                objects->scene_status = ge_original_model_scene_build(
                    input, NULL, &queries[input_index]);
                if (objects->scene_status
                        != GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED
                        || queries[input_index].required_vertex_count
                            > SIZE_MAX - vertex_count
                        || queries[input_index].required_batch_count
                            > SIZE_MAX - batch_count
                        || queries[input_index].triangle_count
                            > SIZE_MAX - triangle_count)
                    goto fail_stage_scene;
                vertex_count += queries[input_index].required_vertex_count;
                batch_count += queries[input_index].required_batch_count;
                triangle_count += queries[input_index].triangle_count;
                ++input_index;
            }
            continue;
        }
        if (!ge_original_prop_state_object_scene_transform(
                entry->definition, entry->prop, matrix, position, &room))
            goto fail_stage_scene;
        part_count = stage_ordinary_scene_part_count(objects, entry);
        for (part_index = 0U; part_index < part_count; ++part_index) {
            GeOriginalPitemModelScenePart part;
            GeOriginalModelSceneInput *input = &inputs[input_index];
            if (!stage_ordinary_scene_part(
                    objects, entry, part_index, &part))
                goto fail_stage_scene;
            candidate_scene_parts[input_index].entry_index = entry_index;
            candidate_scene_parts[input_index].part_index = part_index;
            candidate_scene_parts[input_index].node = part.node;
            if (stage_ordinary_uses_live_model_matrices(entry->type)) {
                if (ge_original_stage_model_publication_resident_input(
                        objects->models, entry->definition, part_index, room,
                        objects->preview->original_camera_view_to_world,
                        input)
                        != GE_ORIGINAL_STAGE_MODEL_PUBLICATION_OK)
                    goto fail_stage_scene;
            } else {
                input->blob = part.blob;
                input->blob_size = part.blob_size;
                input->primary_offset = part.primary_offset;
                input->secondary_offset = part.secondary_offset;
                input->segment4_offset = part.segment4_offset;
                input->room_id = room;
                memcpy(input->matrix, matrix, sizeof(input->matrix));
                memcpy(input->position, position, sizeof(input->position));
            }
            objects->scene_status = ge_original_model_scene_build(
                input, NULL, &queries[input_index]);
            if (objects->scene_status
                    != GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED
                    || queries[input_index].required_vertex_count
                        > SIZE_MAX - vertex_count
                    || queries[input_index].required_batch_count
                        > SIZE_MAX - batch_count
                    || queries[input_index].triangle_count
                        > SIZE_MAX - triangle_count)
                goto fail_stage_scene;
            vertex_count += queries[input_index].required_vertex_count;
            batch_count += queries[input_index].required_batch_count;
            triangle_count += queries[input_index].triangle_count;
            ++input_index;
        }
    }
    ordinary_input_count = input_index;
    objects->scene_install_ordinary_input_count = ordinary_input_count;
    objects->scene_install_failure_phase =
        RUNTIME_STAGE_SCENE_INSTALL_DOOR_QUERY;
    for (entry_index = 0U;
            entry_index < objects->interactive.entry_count; ++entry_index) {
        const GeOriginalStageInteractiveEntry *entry =
            ge_original_stage_interactive_entry(
                &objects->interactive, entry_index);
        float matrix[4][4];
        float position[3];
        uint8_t room;
        size_t part_count;
        size_t part_index;

        if (entry == NULL || !entry->constructed
                || entry->type != PROPDEF_DOOR || entry->room < 0
                || entry->room > UINT8_MAX
                || !ge_dam_dynamic_scene_is_resident(
                    &objects->preview->dynamic_scene,
                    (uint8_t)entry->room)) continue;
        if (!stage_interactive_scene_transform(entry,
                &door_publications[entry_index], matrix, position, &room))
            goto fail_stage_scene;
        door_generations[entry_index] =
            door_publications[entry_index].generation;
        part_count = ge_original_pitem_model_scene_part_count(
            objects->models, entry->model_id);
        for (part_index = 0U; part_index < part_count; ++part_index) {
            GeOriginalPitemModelScenePart part;
            GeOriginalModelSceneInput *input = &inputs[input_index];

            if (!ge_original_pitem_model_scene_part(
                    objects->models, entry->model_id, part_index, &part))
                goto fail_stage_scene;
            input->blob = part.blob;
            input->blob_size = part.blob_size;
            input->primary_offset = part.primary_offset;
            input->secondary_offset = part.secondary_offset;
            input->segment4_offset = part.segment4_offset;
            input->segment3_matrices =
                door_publications[entry_index].matrices;
            input->segment3_matrix_count =
                door_publications[entry_index].matrix_count;
            input->room_id = room;
            memcpy(input->matrix, matrix, sizeof(input->matrix));
            memcpy(input->position, position, sizeof(input->position));
            objects->scene_status = ge_original_model_scene_build(
                input, NULL, &queries[input_index]);
            if (objects->scene_status
                    != GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED
                    || queries[input_index].required_vertex_count
                        > SIZE_MAX - vertex_count
                    || queries[input_index].required_batch_count
                        > SIZE_MAX - batch_count
                    || queries[input_index].triangle_count
                        > SIZE_MAX - triangle_count)
                goto fail_stage_scene;
            vertex_count += queries[input_index].required_vertex_count;
            batch_count += queries[input_index].required_batch_count;
            triangle_count += queries[input_index].triangle_count;
            ++input_index;
        }
    }
    if (guard_query.required_vertex_count > SIZE_MAX - vertex_count
            || guard_query.required_batch_count > SIZE_MAX - batch_count
            || guard_query.triangle_count > SIZE_MAX - triangle_count)
        goto fail_stage_scene;
    vertex_count += guard_query.required_vertex_count;
    batch_count += guard_query.required_batch_count;
    triangle_count += guard_query.triangle_count;
    objects->scene_install_required_vertices = vertex_count;
    objects->scene_install_required_batches = batch_count;
    phase_ticks[1] = svcGetSystemTick();
    objects->scene_install_failure_phase =
        RUNTIME_STAGE_SCENE_INSTALL_ALLOCATE_OUTPUT;
    if (vertex_count == 0U || batch_count == 0U) goto fail_stage_scene;
    vertices = malloc(vertex_count * sizeof(*vertices));
    batches = malloc(batch_count * sizeof(*batches));
    candidate_slots = calloc(DAM_SCENE_TEXTURE_CAPACITY,
                             sizeof(*candidate_slots));
    if (vertices == NULL || batches == NULL || candidate_slots == NULL)
        goto fail_stage_scene;
    vertex_count = 0U;
    batch_count = 0U;
    objects->scene_install_failure_phase =
        RUNTIME_STAGE_SCENE_INSTALL_ORDINARY_BUILD;
    for (input_index = 0U; input_index < input_count; ++input_index) {
        GeDamRoomSceneStorage storage = {
            vertices + vertex_count,
            queries[input_index].required_vertex_count,
            batches + batch_count,
            queries[input_index].required_batch_count,
        };
        GeOriginalModelScene built;
        size_t local_batch;
        if (input_index == ordinary_input_count) {
            door_vertex_offset = vertex_count;
            door_batch_offset = batch_count;
        }
        objects->scene_status = ge_original_model_scene_build_preflighted(
            &inputs[input_index], &queries[input_index], &storage, &built);
        if (objects->scene_status != GE_ORIGINAL_MODEL_SCENE_OK)
            goto fail_stage_scene;
        if (input_index < ordinary_input_count) {
            RuntimeStageScenePartRange *range =
                &candidate_scene_parts[input_index];
            range->vertex_offset = vertex_count;
            range->vertex_count = built.vertex_count;
            range->batch_offset = batch_count;
            range->batch_count = built.batch_count;
        }
        for (local_batch = 0U; local_batch < built.batch_count; ++local_batch)
            batches[batch_count + local_batch].first_vertex += vertex_count;
        vertex_count += built.vertex_count;
        batch_count += built.batch_count;
    }
    if (ordinary_input_count == input_count) {
        door_vertex_offset = vertex_count;
        door_batch_offset = batch_count;
    }
    guard_vertex_offset = vertex_count;
    guard_batch_offset = batch_count;
    /* Empty doors still separate the ordinary prefix from the guard tail.
     * Topology replacement must rebase this insertion point too. */
    door_candidate.vertex_offset = door_vertex_offset;
    door_candidate.batch_offset = door_batch_offset;
    /* Even an empty guard set owns the tail insertion point. A later visible
     * guard must be appended after ordinary props/doors; refresh assumes that
     * order when publishing the new segment offsets. */
    guard_candidate.vertex_offset = guard_vertex_offset;
    guard_candidate.batch_offset = guard_batch_offset;
    phase_ticks[2] = svcGetSystemTick();
    objects->scene_install_failure_phase =
        RUNTIME_STAGE_SCENE_INSTALL_GUARD_BUILD;
    if (guard_query.required_vertex_count != 0U
            || guard_query.required_batch_count != 0U) {
        GeDamRoomSceneStorage guard_storage = {
            vertices + vertex_count, guard_query.required_vertex_count,
            batches + batch_count, guard_query.required_batch_count,
        };
        GeOriginalStageGuardScene guard_built;
        size_t local_batch;
        objects->guard_status =
            ge_original_stage_guard_runtime_build_scene_cached(
            objects->guards, &objects->guard_scene_cache,
            runtime_eye_space_identity,
            &guard_storage, &guard_built);
        if (objects->guard_status != GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK)
            goto fail_stage_scene;
        for (local_batch = 0U; local_batch < guard_built.batch_count;
                ++local_batch) {
            batches[batch_count + local_batch].first_vertex += vertex_count;
            batches[batch_count + local_batch].coordinate_space =
                GE_DAM_ROOM_COORDINATE_EYE;
        }
        vertex_count += guard_built.vertex_count;
        batch_count += guard_built.batch_count;
        objects->guard_scene = guard_built;
    } else {
        memset(&objects->guard_scene, 0, sizeof(objects->guard_scene));
        objects->guard_scene.status = GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK;
    }
    phase_ticks[3] = svcGetSystemTick();
    objects->scene_install_failure_phase =
        RUNTIME_STAGE_SCENE_INSTALL_OVERLAY;
    if ((guard_vertex_offset > vertex_count
            || guard_batch_offset > batch_count)
            || ((door_vertex_offset != guard_vertex_offset
                    || door_batch_offset != guard_batch_offset)
                && !dam_overlay_segment_capture(
                    &door_candidate, vertices, door_vertex_offset,
                    guard_vertex_offset - door_vertex_offset, batches,
                    door_batch_offset,
                    guard_batch_offset - door_batch_offset))
            || ((guard_vertex_offset != vertex_count
                    || guard_batch_offset != batch_count)
                && !dam_overlay_segment_capture(
                    &guard_candidate, vertices, guard_vertex_offset,
                    vertex_count - guard_vertex_offset, batches,
                    guard_batch_offset,
                    batch_count - guard_batch_offset)))
        goto fail_stage_scene;
    objects->overlay_status = ge_dam_dynamic_scene_set_overlay(
        &objects->preview->dynamic_scene, vertices, vertex_count,
        batches, batch_count);
    if (objects->overlay_status != GE_DAM_DYNAMIC_SCENE_OK)
        goto fail_stage_scene;
    objects->preview->source_vertices =
        objects->preview->dynamic_scene.vertices;
    objects->preview->batches = objects->preview->dynamic_scene.batches;
    objects->preview->source_vertex_count =
        objects->preview->dynamic_scene.scene.vertex_count;
    objects->preview->batch_count =
        objects->preview->dynamic_scene.scene.batch_count;
    objects->preview->triangles =
        objects->preview->dynamic_scene.scene.triangle_count;
    objects->preview->draws =
        objects->preview->dynamic_scene.scene.batch_count;
    phase_ticks[4] = svcGetSystemTick();
    objects->scene_install_failure_phase =
        RUNTIME_STAGE_SCENE_INSTALL_TEXTURES;
    {
        Ge3dsSceneTextureReconcileStats texture_stats = {0};
        Ge3dsSceneTextureStatus texture_status =
            ge_3ds_scene_textures_reconcile_prepare(
            objects->preview->texture_cache,
            objects->preview->dynamic_scene.batches,
            objects->preview->dynamic_scene.scene.batch_count,
            &dam_scene_textures,
            candidate_slots, DAM_SCENE_TEXTURE_CAPACITY,
            &candidate_textures, &texture_stats);
        if (texture_status != GE_3DS_SCENE_TEXTURE_OK
                && texture_status != GE_3DS_SCENE_TEXTURE_PARTIAL)
            goto fail_stage_scene;
        if (!prepare_stage_guard_texture_residency(
                objects, &candidate_textures, &texture_stats))
            goto fail_stage_scene;
        texture_status = ge_3ds_scene_textures_reconcile_commit(
            &dam_scene_textures, &candidate_textures, &texture_stats);
        if (texture_status != GE_3DS_SCENE_TEXTURE_OK
                && texture_status != GE_3DS_SCENE_TEXTURE_PARTIAL)
            goto fail_stage_scene;
    }
    memcpy(dam_scene_texture_slots, candidate_slots,
           DAM_SCENE_TEXTURE_CAPACITY * sizeof(*candidate_slots));
    dam_scene_textures = candidate_textures;
    dam_scene_textures.slots = dam_scene_texture_slots;
    candidate_textures.slots = NULL;
    objects->preview->scene_textures = &dam_scene_textures;
    objects->scene_vertices = vertex_count;
    objects->scene_batches = batch_count;
    objects->scene_triangles = triangle_count;
    objects->scene_ready = true;
    objects->resident_install_successes =
        objects->preview->dynamic_scene.install_successes;
    objects->resident_eviction_successes =
        objects->preview->dynamic_scene.eviction_successes;
    dam_overlay_segment_close(&objects->door_overlay);
    dam_overlay_segment_close(&objects->guard_overlay);
    objects->door_overlay = door_candidate;
    objects->guard_overlay = guard_candidate;
    memset(&door_candidate, 0, sizeof(door_candidate));
    memset(&guard_candidate, 0, sizeof(guard_candidate));
    free(objects->installed_door_scene_generations);
    objects->installed_door_scene_generations = door_generations;
    objects->installed_door_scene_generation_count =
        objects->interactive.entry_count;
    door_generations = NULL;
    free(objects->ordinary_scene_parts);
    objects->ordinary_scene_parts = candidate_scene_parts;
    objects->ordinary_scene_part_count = ordinary_input_count;
    candidate_scene_parts = NULL;
    for (entry_index = 0U; entry_index < objects->entry_count; ++entry_index)
        if (objects->entries[entry_index].articulated != NULL)
            objects->entries[entry_index].articulated->force_copy = true;
    /* A topology/residency rebuild replaces the monitor surface storage with
     * the authored base model, so every live monitor must publish its current
     * exact output once into the new overlay before equality skips resume. */
    for (entry_index = 0U; entry_index < objects->entry_count; ++entry_index)
        objects->entries[entry_index].published_monitor_mask = 0U;
    objects->overlay_full_rebuilds++;
    ++objects->scene_install_successes;
    objects->scene_install_failure_phase = RUNTIME_STAGE_SCENE_INSTALL_NONE;
    phase_ticks[5] = svcGetSystemTick();
    for (entry_index = 0U; entry_index < 5U; ++entry_index)
        objects->scene_install_phase_ticks[entry_index] =
            phase_ticks[entry_index + 1U] - phase_ticks[entry_index];
    ge_3ds_scene_textures_close(&candidate_textures);
    free(candidate_slots); free(batches); free(vertices);
    free(door_generations); free(door_publications);
    free(candidate_scene_parts); free(queries); free(inputs);
    return true;

fail_stage_scene:
    dam_overlay_segment_close(&door_candidate);
    dam_overlay_segment_close(&guard_candidate);
    ge_3ds_scene_textures_close(&candidate_textures);
    free(candidate_slots); free(batches); free(vertices);
    free(door_generations); free(door_publications);
    free(candidate_scene_parts); free(queries); free(inputs);
    return false;
}

static bool refresh_stage_door_overlay(
    RuntimeStageOrdinaryObjects *objects, bool *updated)
{
    RuntimeDamOverlaySegment *segment;
    uint32_t *candidate_generations = NULL;
    GeOriginalDoorRuntimePublication *publications = NULL;
    GeOriginalModelSceneInput *inputs = NULL;
    GeOriginalModelScene built;
    GeDamRoomSceneStorage storage;
    size_t input_count = 0U;
    size_t input_index = 0U;
    size_t entry_index;
    bool changed = false;

    if (updated != NULL) *updated = false;
    if (objects == NULL || objects->models == NULL
            || objects->preview == NULL) return false;
    segment = &objects->door_overlay;
    if (objects->interactive.entry_count != 0U
            && (objects->installed_door_scene_generations == NULL
                || objects->installed_door_scene_generation_count
                    != objects->interactive.entry_count)) return false;
    /* Snapshot generations before allocating publication/model arrays. The
     * exact object tick may run every retrace, but a closed/idle authored door
     * republishes the same generation and requires no renderer-side work. */
    for (entry_index = 0U;
            entry_index < objects->interactive.entry_count; ++entry_index) {
        const GeOriginalStageInteractiveEntry *entry =
            ge_original_stage_interactive_entry(
                &objects->interactive, entry_index);
        GeOriginalDoorRuntimePublication publication;
        if (entry == NULL || !entry->constructed
                || entry->type != PROPDEF_DOOR || entry->room < 0
                || entry->room > UINT8_MAX
                || !ge_dam_dynamic_scene_is_resident(
                    &objects->preview->dynamic_scene,
                    (uint8_t)entry->room)) continue;
        if (!ge_original_door_runtime_snapshot(
                entry->definition, &publication)) return false;
        if (publication.generation
                != objects->installed_door_scene_generations[entry_index]) {
            changed = true;
            break;
        }
    }
    if (!changed) return true;
    changed = false;
    if (objects->interactive.entry_count != 0U) {
        candidate_generations = malloc(objects->interactive.entry_count
            * sizeof(*candidate_generations));
        publications = calloc(objects->interactive.entry_count,
                              sizeof(*publications));
        if (candidate_generations == NULL || publications == NULL) goto fail;
        memcpy(candidate_generations,
               objects->installed_door_scene_generations,
               objects->interactive.entry_count
                    * sizeof(*candidate_generations));
    }
    for (entry_index = 0U;
            entry_index < objects->interactive.entry_count; ++entry_index) {
        const GeOriginalStageInteractiveEntry *entry =
            ge_original_stage_interactive_entry(
                &objects->interactive, entry_index);
        GeOriginalDoorRuntimePublication *publication =
            &publications[entry_index];
        if (entry == NULL || !entry->constructed
                || entry->type != PROPDEF_DOOR || entry->room < 0
                || entry->room > UINT8_MAX
                || !ge_dam_dynamic_scene_is_resident(
                    &objects->preview->dynamic_scene,
                    (uint8_t)entry->room)) continue;
        if (!ge_original_door_runtime_snapshot(
                entry->definition, publication)) goto fail;
        candidate_generations[entry_index] = publication->generation;
        if (publication->generation
                != objects->installed_door_scene_generations[entry_index])
            changed = true;
        input_count += ge_original_pitem_model_scene_part_count(
            objects->models, entry->model_id);
    }
    if (!changed) {
        free(publications);
        free(candidate_generations);
        return true;
    }
    if (segment->vertices == NULL || segment->batches == NULL
            || segment->vertex_count == 0U || segment->batch_count == 0U)
        goto fail;
    if (input_count != 0U) {
        inputs = calloc(input_count, sizeof(*inputs));
        if (inputs == NULL) goto fail;
    }
    for (entry_index = 0U;
            entry_index < objects->interactive.entry_count; ++entry_index) {
        const GeOriginalStageInteractiveEntry *entry =
            ge_original_stage_interactive_entry(
                &objects->interactive, entry_index);
        GeOriginalDoorRuntimePublication *publication =
            &publications[entry_index];
        float matrix[4][4];
        float position[3];
        uint8_t room;
        size_t part_count;
        size_t part_index;
        if (entry == NULL || !entry->constructed
                || entry->type != PROPDEF_DOOR || entry->room < 0
                || entry->room > UINT8_MAX
                || !ge_dam_dynamic_scene_is_resident(
                    &objects->preview->dynamic_scene,
                    (uint8_t)entry->room)) continue;
        if (!stage_interactive_scene_transform(
                entry, publication, matrix, position, &room)) goto fail;
        part_count = ge_original_pitem_model_scene_part_count(
            objects->models, entry->model_id);
        for (part_index = 0U; part_index < part_count; ++part_index) {
            GeOriginalPitemModelScenePart part;
            GeOriginalModelSceneInput *input;
            if (!ge_original_pitem_model_scene_part(
                    objects->models, entry->model_id, part_index, &part))
                goto fail;
            if (input_index >= input_count) goto fail;
            input = &inputs[input_index++];
            input->blob = part.blob;
            input->blob_size = part.blob_size;
            input->primary_offset = part.primary_offset;
            input->secondary_offset = part.secondary_offset;
            input->segment4_offset = part.segment4_offset;
            input->segment3_matrices = publication->matrices;
            input->segment3_matrix_count = publication->matrix_count;
            input->room_id = room;
            memcpy(input->matrix, matrix, sizeof(input->matrix));
            memcpy(input->position, position, sizeof(input->position));
        }
    }
    if (input_index != input_count) goto fail;
    storage.vertices = segment->vertices;
    storage.vertex_capacity = segment->vertex_count;
    storage.batches = segment->batches;
    storage.batch_capacity = segment->batch_count;
    if (ge_original_model_scene_cache_build(
            &objects->door_scene_cache, inputs, input_count,
            &storage, &built) != GE_ORIGINAL_MODEL_SCENE_OK
            || built.vertex_count != segment->vertex_count
            || built.batch_count != segment->batch_count) goto fail;
    if (dam_overlay_segment_matches_published(objects->preview, segment)) {
        memcpy(objects->installed_door_scene_generations,
               candidate_generations,
               objects->interactive.entry_count
                    * sizeof(*candidate_generations));
        free(inputs);
        free(publications);
        free(candidate_generations);
        return true;
    }
    objects->overlay_status = ge_dam_dynamic_scene_update_overlay_segment(
        &objects->preview->dynamic_scene, segment->vertex_offset,
        segment->vertices, segment->vertex_count, segment->batch_offset,
        segment->batches, segment->batch_count);
    if (objects->overlay_status != GE_DAM_DYNAMIC_SCENE_OK) goto fail;
    memcpy(objects->installed_door_scene_generations,
           candidate_generations,
           objects->interactive.entry_count
                * sizeof(*candidate_generations));
    objects->door_overlay_updates++;
    if (updated != NULL) *updated = true;
    free(inputs);
    free(publications);
    free(candidate_generations);
    return true;
fail:
    free(inputs);
    free(publications);
    free(candidate_generations);
    return false;
}

static int ensure_stage_guard_scene_texture(void *context,
                                             uint16_t texture_id)
{
    RuntimeStageOrdinaryObjects *objects = context;
    Ge3dsSceneTextureStatus status;

    if (objects == NULL || objects->preview == NULL
            || objects->preview->texture_cache == NULL
            || objects->preview->scene_textures == NULL) return 0;
    if (ge_3ds_scene_textures_find(
            objects->preview->scene_textures, texture_id) != NULL) return 1;
    const uint64_t started = svcGetSystemTick();
    status = ge_3ds_scene_textures_ensure_image(
        objects->preview->texture_cache, &dam_scene_textures, texture_id);
    fine_profile.guard_import_ticks += svcGetSystemTick() - started;
    return status == GE_3DS_SCENE_TEXTURE_OK
        || status == GE_3DS_SCENE_TEXTURE_PARTIAL;
}

static bool ensure_stage_guard_overlay_textures(
    RuntimeStageOrdinaryObjects *objects,
    const GeDamRoomDrawBatch *batches, size_t batch_count)
{
    const uint64_t started = svcGetSystemTick();
    const bool result = ge_original_model_scene_visit_textures(
        batches, batch_count, objects, ensure_stage_guard_scene_texture) != 0;
    fine_profile.guard_texture_ticks += svcGetSystemTick() - started;
    return result;
}

static bool refresh_stage_guard_overlay_impl(
    RuntimeStageOrdinaryObjects *objects, bool *updated)
{
    RuntimeDamOverlaySegment *segment;
    GeDamDynamicScene *dynamic_scene;
    GeDamRoomSceneStorage storage;
    GeOriginalStageGuardScene scene;
    GeOriginalStageGuardRuntimeStatus scene_status;
    uint64_t cached_before;
    uint64_t unchanged_before;
    if (updated != NULL) *updated = false;
    if (objects == NULL || objects->guards == NULL
            || objects->preview == NULL) return false;
    segment = &objects->guard_overlay;
    dynamic_scene = &objects->preview->dynamic_scene;
    if (segment->vertex_count == 0U || segment->batch_count == 0U) {
        scene_status = ge_original_stage_guard_runtime_build_scene_cached(
                objects->guards, &objects->guard_scene_cache,
                runtime_eye_space_identity,
                NULL, &scene);
        if (scene_status
                != GE_ORIGINAL_STAGE_GUARD_RUNTIME_SCENE_CAPACITY_EXCEEDED)
            return false;
        if (scene.required_vertex_count == 0U
                && scene.required_batch_count == 0U) return true;
        goto replace_topology;
    }
    if (dynamic_scene->overlay_vertices == NULL
            || segment->vertex_offset > dynamic_scene->overlay_vertex_count
            || segment->vertex_count > dynamic_scene->overlay_vertex_count
                - segment->vertex_offset
            || segment->batch_offset > dynamic_scene->overlay_batch_count
            || segment->batch_count > dynamic_scene->overlay_batch_count
                - segment->batch_offset) return false;
    /* The dynamic overlay is already the tail of the combined renderer
     * scene. Publish the exact model-cache output there directly instead of
     * transforming into segment->vertices and memcpying the whole animated
     * guard range a second time every displayed frame. */
    storage.vertices = dynamic_scene->overlay_vertices
        + segment->vertex_offset;
    storage.vertex_capacity = segment->vertex_count;
    storage.batches = segment->batches;
    storage.batch_capacity = segment->batch_count;
    cached_before = objects->guard_scene_cache.cached_builds;
    unchanged_before = objects->guard_scene_cache.unchanged_builds;
    {
        const uint64_t scene_start = svcGetSystemTick();
        scene_status =
            ge_original_stage_guard_runtime_build_scene_cached(
                objects->guards, &objects->guard_scene_cache,
                runtime_eye_space_identity, &storage, &scene);
        fine_profile.guard_scene_ticks +=
            svcGetSystemTick() - scene_start;
        fine_profile.guard_scene_calls++;
        if (scene_status != GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK
                || scene.vertex_count != segment->vertex_count
                || scene.batch_count != segment->batch_count) {
            if (scene_status
                        != GE_ORIGINAL_STAGE_GUARD_RUNTIME_SCENE_CAPACITY_EXCEEDED
                    && scene_status != GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK)
                return false;
            goto replace_topology;
        }
    }
    objects->guard_scene = scene;
    if (objects->guard_scene_cache.unchanged_builds != unchanged_before)
        return true;
    if (objects->guard_scene_cache.cached_builds == cached_before)
        return false;
    /* A topology-stable switch can still publish a head/hat/weapon texture
     * which was absent from the initial resident-room scene.  Make the exact
     * batch texture set resident before committing it; this is renderer-only
     * and never writes PropRecord visibility or character state. */
    /* Immutable textures need residency checks only on topology changes. */
    {
        const uint64_t commit_start = svcGetSystemTick();
        size_t range_index;
        for (range_index = 0U;
                range_index < objects->guard_scene_cache.publication_range_count;
                ++range_index) {
            const GeOriginalModelScenePublicationRange *range =
                &objects->guard_scene_cache.publication_ranges[range_index];
            size_t batch_index;
            if (!range->static_data_changed) {
                /* Pose-only cache publications retain topology, coordinate
                 * space, materials and texture bindings in both scene views.
                 * Only authored room IDs can differ; preserve the existing
                 * generation notification for the changed vertex positions. */
                if (range->batch_count != 0U) {
                    objects->overlay_status =
                        ge_dam_dynamic_scene_commit_overlay_rooms(
                            dynamic_scene, segment->batch_offset + range->batch_offset,
                            segment->batches + range->batch_offset, range->batch_count);
                    if (objects->overlay_status != GE_DAM_DYNAMIC_SCENE_OK)
                        return false;
                }
                continue;
            }
            if (range->static_data_changed
                    && !ensure_stage_guard_overlay_textures(objects,
                        segment->batches + range->batch_offset,
                        range->batch_count)) return false;
            for (batch_index = range->batch_offset;
                    batch_index < range->batch_offset + range->batch_count;
                    ++batch_index) {
                GeDamRoomDrawBatch batch = segment->batches[batch_index];
                batch.coordinate_space = GE_DAM_ROOM_COORDINATE_EYE;
                segment->batches[batch_index].coordinate_space =
                    GE_DAM_ROOM_COORDINATE_EYE;
                batch.first_vertex += segment->vertex_offset;
                dynamic_scene->overlay_batches[
                    segment->batch_offset + batch_index] = batch;
            }
            if (range->batch_count != 0U) {
                objects->overlay_status =
                    ge_dam_dynamic_scene_commit_overlay_batches(
                        dynamic_scene, segment->batch_offset + range->batch_offset,
                        range->batch_count);
                if (objects->overlay_status != GE_DAM_DYNAMIC_SCENE_OK)
                    return false;
            }
        }
        fine_profile.guard_overlay_commit_ticks +=
            svcGetSystemTick() - commit_start;
        fine_profile.guard_overlay_commit_calls++;
    }
    if (objects->overlay_status != GE_DAM_DYNAMIC_SCENE_OK) return false;
    objects->guard_overlay_updates++;
    if (updated != NULL) *updated = true;
    return true;

replace_topology:
    {
        GeDamRoomWorldVertex *replacement_vertices = NULL;
        GeDamRoomDrawBatch *replacement_batches = NULL;
        GeDamRoomSceneStorage replacement_storage;
        GeOriginalStageGuardScene replacement_scene;
        size_t batch_index;

        if (scene.required_vertex_count == 0U
                || scene.required_batch_count == 0U) {
            if (scene.required_vertex_count != 0U
                    || scene.required_batch_count != 0U) return false;
        } else {
            replacement_vertices = malloc(scene.required_vertex_count
                * sizeof(*replacement_vertices));
            replacement_batches = malloc(scene.required_batch_count
                * sizeof(*replacement_batches));
            if (replacement_vertices == NULL || replacement_batches == NULL) {
                free(replacement_batches);
                free(replacement_vertices);
                return false;
            }
            replacement_storage.vertices = replacement_vertices;
            replacement_storage.vertex_capacity = scene.required_vertex_count;
            replacement_storage.batches = replacement_batches;
            replacement_storage.batch_capacity = scene.required_batch_count;
            scene_status = ge_original_stage_guard_runtime_build_scene_cached(
                objects->guards, &objects->guard_scene_cache,
                runtime_eye_space_identity, &replacement_storage,
                &replacement_scene);
            if (scene_status != GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK) {
                free(replacement_batches);
                free(replacement_vertices);
                return false;
            }
            for (batch_index = 0U;
                    batch_index < replacement_scene.batch_count;
                    ++batch_index)
                replacement_batches[batch_index].coordinate_space =
                    GE_DAM_ROOM_COORDINATE_EYE;
            if (!ensure_stage_guard_overlay_textures(
                    objects, replacement_batches,
                    replacement_scene.batch_count)) {
                free(replacement_batches);
                free(replacement_vertices);
                return false;
            }
        }
        const uint64_t replace_start = svcGetSystemTick();
        objects->overlay_status =
            ge_dam_dynamic_scene_replace_overlay_segment(
                dynamic_scene, segment->vertex_offset,
                segment->vertex_count, replacement_vertices,
                scene.required_vertex_count, segment->batch_offset,
                segment->batch_count, replacement_batches,
                scene.required_batch_count);
        fine_profile.guard_replace_ticks += svcGetSystemTick() - replace_start;
        if (objects->overlay_status != GE_DAM_DYNAMIC_SCENE_OK) {
            objects->guard_topology_replace_failures++;
            objects->last_guard_topology_replace_status =
                (uint32_t)objects->overlay_status;
            free(replacement_batches);
            free(replacement_vertices);
            return false;
        }
        objects->guard_topology_replace_successes++;
        dam_overlay_segment_close(segment);
        segment->vertices = replacement_vertices;
        segment->batches = replacement_batches;
        segment->vertex_offset = dynamic_scene->overlay_vertex_count
            - scene.required_vertex_count;
        segment->vertex_count = scene.required_vertex_count;
        segment->batch_offset = dynamic_scene->overlay_batch_count
            - scene.required_batch_count;
        segment->batch_count = scene.required_batch_count;
        objects->guard_scene = scene.required_vertex_count != 0U
            ? replacement_scene : scene;
        objects->scene_vertices = dynamic_scene->overlay_vertex_count;
        objects->scene_batches = dynamic_scene->overlay_batch_count;
        objects->scene_triangles = dynamic_scene->scene.triangle_count;
        objects->guard_overlay_updates++;
        if (updated != NULL) *updated = true;
        return true;
    }
}

static bool refresh_stage_guard_overlay(
    RuntimeStageOrdinaryObjects *objects, bool *updated)
{
    if (objects == NULL) return false;
    const uint64_t before[7] = {svcGetSystemTick(),
        objects->guard_scene_cache.profile_build_ticks,
        objects->guard_scene_cache.profile_topology_ticks,
        objects->guard_scene_cache.profile_vertex_transform_ticks,
        fine_profile.guard_texture_ticks, fine_profile.guard_replace_ticks,
        fine_profile.guard_import_ticks};
    const bool result = refresh_stage_guard_overlay_impl(objects, updated);
    const uint64_t after[7] = {svcGetSystemTick(),
        objects->guard_scene_cache.profile_build_ticks,
        objects->guard_scene_cache.profile_topology_ticks,
        objects->guard_scene_cache.profile_vertex_transform_ticks,
        fine_profile.guard_texture_ticks, fine_profile.guard_replace_ticks,
        fine_profile.guard_import_ticks};
    if (fine_profile.rendered_frames >= 120U
            && after[0] - before[0] > fine_profile.guard_refresh_peak[0])
        for (size_t i = 0U; i < 7U; ++i)
            fine_profile.guard_refresh_peak[i] = after[i] - before[i];
    return result;
}

static const RuntimeStageScenePartRange *stage_monitor_scene_part_range(
    const RuntimeStageOrdinaryObjects *objects, size_t entry_index,
    size_t part_index)
{
    size_t index;
    if (objects == NULL) return NULL;
    for (index = 0U; index < objects->ordinary_scene_part_count; ++index) {
        const RuntimeStageScenePartRange *range =
            &objects->ordinary_scene_parts[index];
        if (range->entry_index == entry_index
                && range->part_index == part_index) return range;
    }
    return NULL;
}

static bool stage_articulated_reserve(
    RuntimeStageArticulatedPublication *publication,
    size_t input_count, size_t vertex_count, size_t batch_count)
{
    if (publication == NULL) return false;
    if (input_count > publication->input_capacity) {
        GeOriginalModelSceneInput *candidate = realloc(
            publication->inputs, input_count * sizeof(*candidate));
        if (candidate == NULL) return false;
        publication->inputs = candidate;
        publication->input_capacity = input_count;
    }
    if (vertex_count > publication->vertex_capacity) {
        GeDamRoomWorldVertex *candidate = realloc(
            publication->vertices, vertex_count * sizeof(*candidate));
        if (candidate == NULL) return false;
        publication->vertices = candidate;
        publication->vertex_capacity = vertex_count;
    }
    if (batch_count > publication->batch_capacity) {
        GeDamRoomDrawBatch *candidate = realloc(
            publication->batches, batch_count * sizeof(*candidate));
        if (candidate == NULL) return false;
        publication->batches = candidate;
        publication->batch_capacity = batch_count;
    }
    return true;
}

static bool replace_stage_articulated_topology(
    RuntimeStageOrdinaryObjects *objects, size_t entry_index,
    size_t part_count, Vertex *gpu_destination)
{
    const uint64_t started = svcGetSystemTick();
    RuntimeStageOrdinaryEntry *entry = &objects->entries[entry_index];
    RuntimeStageArticulatedPublication *publication;
    RuntimeDamPreview *preview = objects->preview;
    GeScenePartRange *parts = NULL;
    GeSceneOverlaySpan changed;
    GeSceneOverlaySpan tails[2] = {
        {objects->door_overlay.vertex_offset, objects->door_overlay.vertex_count,
         objects->door_overlay.batch_offset, objects->door_overlay.batch_count},
        {objects->guard_overlay.vertex_offset, objects->guard_overlay.vertex_count,
         objects->guard_overlay.batch_offset, objects->guard_overlay.batch_count}
    };
    GeOriginalModelScene query, built;
    GeDamRoomSceneStorage storage;
    size_t i;
    if (entry->articulated == NULL)
        entry->articulated = calloc(1U, sizeof(*entry->articulated));
    publication = entry->articulated;
    if (publication == NULL || part_count > SIZE_MAX / sizeof(*parts)
            || !stage_articulated_reserve(publication, part_count, 0U, 0U))
        return false;
    if (part_count != 0U) {
        parts = calloc(part_count, sizeof(*parts));
        if (parts == NULL) return false;
    }
    for (i = 0U; i < part_count; ++i) {
        GeOriginalPitemModelScenePart part;
        if (!stage_ordinary_scene_part(objects, entry, i, &part)
                || ge_original_stage_model_publication_resident_input(
                    objects->models, entry->definition, i, entry->room,
                    preview->original_camera_view_to_world,
                    &publication->inputs[i]) != GE_ORIGINAL_STAGE_MODEL_PUBLICATION_OK)
            goto fail;
        parts[i].entry_index = entry_index;
        parts[i].part_index = i;
        parts[i].node = part.node;
    }
    if (ge_original_model_scene_cache_build(&publication->cache,
            publication->inputs, part_count, NULL, &query)
            != GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED
            || !stage_articulated_reserve(publication, part_count,
                query.required_vertex_count, query.required_batch_count)) goto fail;
    storage = (GeDamRoomSceneStorage){publication->vertices, publication->vertex_capacity,
        publication->batches, publication->batch_capacity};
    if (ge_original_model_scene_cache_build(&publication->cache,
            publication->inputs, part_count, &storage, &built)
            != GE_ORIGINAL_MODEL_SCENE_OK) goto fail;
    for (i = 0U; i < part_count; ++i) {
        parts[i].vertex_offset = publication->cache.input_vertex_offsets[i];
        parts[i].vertex_count = publication->cache.queries[i].required_vertex_count;
        parts[i].batch_offset = publication->cache.input_batch_offsets[i];
        parts[i].batch_count = publication->cache.queries[i].required_batch_count;
    }
    if (!ensure_stage_guard_overlay_textures(objects, publication->batches,
            built.batch_count)) goto fail;
    objects->overlay_status = ge_scene_part_replace(&preview->dynamic_scene,
        &objects->ordinary_scene_parts, &objects->ordinary_scene_part_count,
        tails, 2U, entry_index, parts, part_count,
        publication->vertices, built.vertex_count,
        publication->batches, built.batch_count, &changed);
    if (objects->overlay_status != GE_DAM_DYNAMIC_SCENE_OK) goto fail;
    objects->door_overlay.vertex_offset = tails[0].vertex_offset;
    objects->door_overlay.batch_offset = tails[0].batch_offset;
    objects->guard_overlay.vertex_offset = tails[1].vertex_offset;
    objects->guard_overlay.batch_offset = tails[1].batch_offset;
    preview->source_vertices = preview->dynamic_scene.vertices;
    preview->batches = preview->dynamic_scene.batches;
    preview->source_vertex_count = preview->dynamic_scene.scene.vertex_count;
    preview->vertex_count = preview->source_vertex_count;
    preview->batch_count = preview->dynamic_scene.scene.batch_count;
    preview->triangles = preview->dynamic_scene.scene.triangle_count;
    preview->draws = preview->batch_count;
    objects->scene_vertices = preview->dynamic_scene.overlay_vertex_count;
    objects->scene_batches = preview->dynamic_scene.overlay_batch_count;
    objects->scene_triangles = preview->triangles;
    /* Rebase the GPU suffix as well as CPU metadata. Room geometry and all
     * earlier props remain resident; only shifted overlay data needs upload. */
    if (!upload_dam_gpu_world_scene_range(preview, gpu_destination,
            preview->source_vertex_count - preview->dynamic_scene.overlay_vertex_count
                + changed.vertex_offset, changed.vertex_count,
            preview->batch_count - preview->dynamic_scene.overlay_batch_count
                + changed.batch_offset, changed.batch_count, true)) goto fail;
    publication->force_copy = false;
    ++publication->updates;
    ++objects->articulated_scene_update_count;
    ++objects->articulated_replace_successes;
    {
        const uint64_t elapsed = svcGetSystemTick() - started;
        if (elapsed > objects->articulated_replace_peak_ticks) {
            objects->articulated_replace_peak_ticks = elapsed;
            objects->articulated_replace_command = entry->command_index;
            objects->articulated_replace_parts = part_count;
        }
    }
    free(parts);
    return true;
fail:
    free(parts);
    return false;
}

static bool refresh_stage_articulated_objects(
    RuntimeStageOrdinaryObjects *objects, Vertex *gpu_destination,
    bool *updated)
{
    size_t entry_index;
    if (updated != NULL) *updated = false;
    if (objects == NULL || objects->preview == NULL
            || objects->models == NULL || gpu_destination == NULL
            || !objects->preview->original_camera_ready) return false;
    for (entry_index = 0U; entry_index < objects->entry_count;
            ++entry_index) {
        RuntimeStageOrdinaryEntry *entry = &objects->entries[entry_index];
        RuntimeStageArticulatedPublication *publication;
        size_t first_range = SIZE_MAX;
        size_t range_count = 0U;
        size_t part_count;
        size_t part_index;
        size_t vertex_count = 0U;
        size_t batch_count = 0U;
        size_t vertex_offset;
        size_t batch_offset;
        size_t cursor_vertex = 0U;
        size_t cursor_batch = 0U;
        float placement_matrix[4][4];
        float placement_position[3];
        uint8_t room;
        uint64_t unchanged_before;
        GeOriginalModelScene built;
        GeDamRoomSceneStorage storage;
        GeOriginalStageModelPublicationStatus input_status =
            GE_ORIGINAL_STAGE_MODEL_PUBLICATION_INVALID_ARGUMENT;

        if (!entry->root_active
                || !stage_ordinary_uses_live_model_matrices(entry->type))
            continue;
        if (!ge_original_prop_state_object_scene_transform(
                entry->definition, entry->prop,
                placement_matrix, placement_position, &room)) return false;
        entry->room = room;
        if (!ge_dam_dynamic_scene_is_resident(
                &objects->preview->dynamic_scene, room)) continue;
        part_count = stage_ordinary_scene_part_count(objects, entry);
        for (part_index = 0U;
                part_index < objects->ordinary_scene_part_count;
                ++part_index) {
            const RuntimeStageScenePartRange *range =
                &objects->ordinary_scene_parts[part_index];
            if (range->entry_index != entry_index) continue;
            if (first_range == SIZE_MAX) first_range = part_index;
            ++range_count;
            if (range->vertex_count > SIZE_MAX - vertex_count
                    || range->batch_count > SIZE_MAX - batch_count)
                return false;
            vertex_count += range->vertex_count;
            batch_count += range->batch_count;
        }
        if (part_count != range_count) {
            ++objects->articulated_scene_topology_change_count;
            if (entry->articulated != NULL)
                ++entry->articulated->topology_changes;
            if (!replace_stage_articulated_topology(
                    objects, entry_index, part_count, gpu_destination)) return false;
            if (updated != NULL) *updated = true;
            continue;
        }
        if (part_count == 0U) continue;
        if (first_range == SIZE_MAX) return false;
        if (entry->articulated == NULL) {
            entry->articulated = calloc(1U, sizeof(*entry->articulated));
            if (entry->articulated == NULL) return false;
            entry->articulated->force_copy = true;
        }
        publication = entry->articulated;
        if (!stage_articulated_reserve(
                publication, part_count, vertex_count, batch_count))
            return false;
        vertex_offset = objects->ordinary_scene_parts[first_range]
            .vertex_offset;
        batch_offset = objects->ordinary_scene_parts[first_range]
            .batch_offset;
        if (objects->preview->dynamic_scene.overlay_vertices == NULL
                || objects->preview->dynamic_scene.overlay_batches == NULL
                || vertex_offset
                    > objects->preview->dynamic_scene.overlay_vertex_count
                || vertex_count
                    > objects->preview->dynamic_scene.overlay_vertex_count
                        - vertex_offset
                || batch_offset
                    > objects->preview->dynamic_scene.overlay_batch_count
                || batch_count
                    > objects->preview->dynamic_scene.overlay_batch_count
                        - batch_offset) return false;
        for (part_index = 0U; part_index < part_count; ++part_index) {
            const RuntimeStageScenePartRange *range =
                &objects->ordinary_scene_parts[first_range + part_index];
            GeOriginalPitemModelScenePart part;
            if (range->entry_index != entry_index
                    || range->part_index != part_index
                    || range->vertex_offset != vertex_offset + cursor_vertex
                    || range->batch_offset != batch_offset + cursor_batch
                    || !ge_original_pitem_model_instance_scene_part(
                        objects->models, entry->prepared.model_instance,
                        part_index, &part)
                    || range->node != part.node) {
                ++objects->articulated_scene_topology_change_count;
                ++publication->topology_changes;
                break;
            }
            input_status = ge_original_stage_model_publication_input(
                objects->models, entry->definition, part_index, room,
                objects->preview->original_camera_view_to_world,
                &publication->inputs[part_index]);
            if (input_status
                    == GE_ORIGINAL_STAGE_MODEL_PUBLICATION_NOT_VISIBLE)
                break;
            if (input_status != GE_ORIGINAL_STAGE_MODEL_PUBLICATION_OK)
                return false;
            cursor_vertex += range->vertex_count;
            cursor_batch += range->batch_count;
        }
        if (part_index != part_count
                && input_status != GE_ORIGINAL_STAGE_MODEL_PUBLICATION_NOT_VISIBLE) {
            if (!replace_stage_articulated_topology(
                    objects, entry_index, part_count, gpu_destination)) return false;
            if (updated != NULL) *updated = true;
            continue;
        }
        if (input_status
                == GE_ORIGINAL_STAGE_MODEL_PUBLICATION_NOT_VISIBLE)
            continue;
        if (cursor_vertex != vertex_count || cursor_batch != batch_count)
            return false;
        storage = (GeDamRoomSceneStorage){
            publication->vertices, vertex_count,
            publication->batches, batch_count,
        };
        unchanged_before = publication->cache.unchanged_builds;
        if (ge_original_model_scene_cache_build(
                &publication->cache, publication->inputs, part_count,
                &storage, &built) != GE_ORIGINAL_MODEL_SCENE_OK
                || built.vertex_count != vertex_count
                || built.batch_count != batch_count) {
            ++objects->articulated_scene_topology_change_count;
            ++publication->topology_changes;
            if (!replace_stage_articulated_topology(
                    objects, entry_index, part_count, gpu_destination)) return false;
            if (updated != NULL) *updated = true;
            continue;
        }
        if (!publication->force_copy
                && publication->cache.unchanged_builds != unchanged_before) {
            ++publication->unchanged;
            ++objects->articulated_scene_unchanged_count;
            continue;
        }
        memcpy(objects->preview->dynamic_scene.overlay_vertices
                + vertex_offset,
            publication->vertices, vertex_count * sizeof(*publication->vertices));
        for (part_index = 0U; part_index < batch_count; ++part_index) {
            GeDamRoomDrawBatch batch = publication->batches[part_index];
            batch.first_vertex += vertex_offset;
            objects->preview->dynamic_scene.overlay_batches[
                batch_offset + part_index] = batch;
        }
        objects->overlay_status =
            ge_dam_dynamic_scene_commit_overlay_batches(
                &objects->preview->dynamic_scene,
                batch_offset, batch_count);
        if (objects->overlay_status != GE_DAM_DYNAMIC_SCENE_OK)
            return false;
        {
            const size_t room_vertex_count =
                objects->preview->dynamic_scene.scene.vertex_count
                - objects->preview->dynamic_scene.overlay_vertex_count;
            const size_t room_batch_count =
                objects->preview->dynamic_scene.scene.batch_count
                - objects->preview->dynamic_scene.overlay_batch_count;
            if (!upload_dam_gpu_world_scene_range(
                    objects->preview, gpu_destination,
                    room_vertex_count + vertex_offset, vertex_count,
                    room_batch_count + batch_offset, batch_count, false))
                return false;
        }
        publication->force_copy = false;
        ++publication->updates;
        ++objects->articulated_scene_update_count;
        if (updated != NULL) *updated = true;
    }
    return true;
}

static bool refresh_stage_monitor_surfaces(
    RuntimeStageOrdinaryObjects *objects, Vertex *gpu_destination,
    bool *updated)
{
    GeDamRoomSceneStorage overlay_scene;
    size_t entry_index;
    size_t first_vertex = SIZE_MAX;
    size_t last_vertex = 0U;
    size_t first_batch = SIZE_MAX;
    size_t last_batch = 0U;
    if (updated != NULL) *updated = false;
    if (objects == NULL || objects->preview == NULL
            || objects->models == NULL || gpu_destination == NULL)
        return false;
    overlay_scene.vertices =
        objects->preview->dynamic_scene.overlay_vertices;
    overlay_scene.vertex_capacity =
        objects->preview->dynamic_scene.overlay_vertex_count;
    overlay_scene.batches =
        objects->preview->dynamic_scene.overlay_batches;
    overlay_scene.batch_capacity =
        objects->preview->dynamic_scene.overlay_batch_count;
    for (entry_index = 0U; entry_index < objects->entry_count;
            ++entry_index) {
        RuntimeStageOrdinaryEntry *entry = &objects->entries[entry_index];
        size_t screen;
        if (!entry->live || entry->monitor_screen_count == 0U
                || !ge_dam_dynamic_scene_is_resident(
                    &objects->preview->dynamic_scene, entry->room)) continue;
        for (screen = 0U; screen < entry->monitor_screen_count; ++screen) {
            GeOriginalPitemModelScenePart part;
            GeOriginalStageMonitorSurfaceResult result;
            const RuntimeStageScenePartRange *range;
            size_t part_index;
            const GeOriginalDamMonitorRenderSnapshot *snapshot =
                &entry->monitor_screens[screen];
            if (snapshot->switch_node == NULL) continue;
            if ((entry->published_monitor_mask & (UINT8_C(1) << screen)) != 0U
                    && ge_original_stage_monitor_surface_output_equal(
                        snapshot,
                        &entry->published_monitor_screens[screen])) {
                ++objects->monitor_surface_unchanged_count;
                continue;
            }
            if (!ge_original_pitem_model_scene_part_for_node(
                    objects->models, entry->model_id,
                    snapshot->switch_node, &part_index, &part)
                    || (range = stage_monitor_scene_part_range(
                        objects, entry_index, part_index)) == NULL
                    || ge_original_stage_monitor_surface_apply_part(
                        snapshot, &overlay_scene, range->batch_offset,
                        range->batch_count, &result)
                        != GE_ORIGINAL_STAGE_MONITOR_SURFACE_OK
                    || result.first_vertex < range->vertex_offset
                    || result.first_vertex + result.vertex_count
                        > range->vertex_offset + range->vertex_count) {
                ++objects->monitor_surface_failure_count;
                return false;
            }
            if (ge_3ds_scene_textures_find(
                    objects->preview->scene_textures,
                    (uint16_t)snapshot->texture_id) == NULL) {
                const Ge3dsSceneTextureStatus texture_status =
                    ge_3ds_scene_textures_ensure_image(
                        objects->preview->texture_cache,
                        &dam_scene_textures,
                        (uint16_t)snapshot->texture_id);
                if (texture_status != GE_3DS_SCENE_TEXTURE_OK
                        && texture_status != GE_3DS_SCENE_TEXTURE_PARTIAL) {
                    ++objects->monitor_surface_failure_count;
                    return false;
                }
            }
            if (result.first_vertex < first_vertex)
                first_vertex = result.first_vertex;
            if (result.first_vertex + result.vertex_count > last_vertex)
                last_vertex = result.first_vertex + result.vertex_count;
            if (result.batch_index < first_batch)
                first_batch = result.batch_index;
            if (result.batch_index + 1U > last_batch)
                last_batch = result.batch_index + 1U;
            entry->published_monitor_screens[screen] = *snapshot;
            entry->published_monitor_mask |= (uint8_t)(UINT8_C(1) << screen);
            ++objects->monitor_surface_update_count;
        }
    }
    if (first_vertex == SIZE_MAX) return true;
    objects->overlay_status = ge_dam_dynamic_scene_commit_overlay_batches(
        &objects->preview->dynamic_scene, first_batch,
        last_batch - first_batch);
    if (objects->overlay_status != GE_DAM_DYNAMIC_SCENE_OK) {
        ++objects->monitor_surface_failure_count;
        return false;
    }
    objects->preview->source_vertices =
        objects->preview->dynamic_scene.vertices;
    objects->preview->batches = objects->preview->dynamic_scene.batches;
    objects->preview->source_vertex_count =
        objects->preview->dynamic_scene.scene.vertex_count;
    objects->preview->batch_count =
        objects->preview->dynamic_scene.scene.batch_count;
    {
        const size_t room_vertex_count =
            objects->preview->source_vertex_count
            - objects->preview->dynamic_scene.overlay_vertex_count;
        const size_t room_batch_count = objects->preview->batch_count
            - objects->preview->dynamic_scene.overlay_batch_count;
        if (!upload_dam_gpu_world_scene_range(
                objects->preview, gpu_destination,
                room_vertex_count + first_vertex,
                last_vertex - first_vertex,
                room_batch_count + first_batch,
                last_batch - first_batch, true)) return false;
    }
    if (updated != NULL) *updated = true;
    return true;
}

static bool refresh_stage_live_overlays(
    RuntimeStageOrdinaryObjects *objects, Vertex *gpu_destination)
{
    bool door_updated = false;
    bool guard_updated = false;
    bool articulated_updated = false;
    bool monitor_updated = false;
    bool full_rebuild = false;
    size_t overlay_scene_vertex_base;
    size_t overlay_scene_batch_base;
    if (objects == NULL || !objects->scene_ready || objects->preview == NULL
            || gpu_destination == NULL) return false;
    if (!refresh_stage_door_overlay(objects, &door_updated)) {
        objects->door_overlay_refresh_failures++;
        full_rebuild = true;
    } else if (!refresh_stage_guard_overlay(objects, &guard_updated)) {
        objects->guard_overlay_refresh_failures++;
        full_rebuild = true;
    } else if (!refresh_stage_articulated_objects(
            objects, gpu_destination, &articulated_updated)) {
        objects->articulated_scene_failure_count++;
        full_rebuild = true;
    } else if (!refresh_stage_monitor_surfaces(
            objects, gpu_destination, &monitor_updated)) {
        objects->monitor_overlay_refresh_failures++;
        full_rebuild = true;
    }
    if (full_rebuild) {
        if (!install_stage_ordinary_object_scenes(objects)
                || !refresh_stage_articulated_objects(
                    objects, gpu_destination, &articulated_updated)
                || !refresh_stage_monitor_surfaces(
                    objects, gpu_destination, &monitor_updated)
                || !upload_dam_gpu_world_scene(
                    objects->preview, gpu_destination)) return false;
    } else {
        objects->preview->source_vertices =
            objects->preview->dynamic_scene.vertices;
        objects->preview->batches =
            objects->preview->dynamic_scene.batches;
        objects->preview->source_vertex_count =
            objects->preview->dynamic_scene.scene.vertex_count;
        objects->preview->batch_count =
            objects->preview->dynamic_scene.scene.batch_count;
        objects->preview->triangles =
            objects->preview->dynamic_scene.scene.triangle_count;
        objects->preview->draws = objects->preview->batch_count;
        overlay_scene_vertex_base = objects->preview->source_vertex_count
            - objects->preview->dynamic_scene.overlay_vertex_count;
        overlay_scene_batch_base = objects->preview->batch_count
            - objects->preview->dynamic_scene.overlay_batch_count;
        if (door_updated && !upload_dam_gpu_world_scene_range(
                objects->preview, gpu_destination,
                overlay_scene_vertex_base
                    + objects->door_overlay.vertex_offset,
                objects->door_overlay.vertex_count,
                overlay_scene_batch_base
                    + objects->door_overlay.batch_offset,
                objects->door_overlay.batch_count, false)) return false;
        if (guard_updated && objects->guard_overlay.vertex_count != 0U) {
            const uint64_t upload_start = svcGetSystemTick();
            size_t range_index;
            fine_profile.guard_gpu_full_upload_vertices +=
                objects->guard_overlay.vertex_count;
            /* Carry the model cache's changed-input ranges to GPU storage.
             * Topology changes must remap UVs even when their counts match
             * the previous scene; unchanged peers retain their GPU data. */
            for (range_index = 0U;
                    range_index
                        < objects->guard_scene_cache.publication_range_count;
                    ++range_index) {
                const GeOriginalModelScenePublicationRange *range =
                    &objects->guard_scene_cache.publication_ranges[range_index];
                if (!upload_dam_gpu_world_scene_range(
                        objects->preview, gpu_destination,
                        overlay_scene_vertex_base
                            + objects->guard_overlay.vertex_offset
                            + range->vertex_offset,
                        range->vertex_count,
                        overlay_scene_batch_base
                            + objects->guard_overlay.batch_offset
                            + range->batch_offset,
                        range->batch_count,
                        range->static_data_changed != 0U)) return false;
                fine_profile.guard_gpu_upload_calls++;
                fine_profile.guard_gpu_upload_vertices += range->vertex_count;
                if (range->static_data_changed)
                    fine_profile.guard_gpu_uv_remap_vertices +=
                        range->vertex_count;
            }
            fine_profile.guard_gpu_upload_ticks +=
                svcGetSystemTick() - upload_start;
        }
        objects->preview->gpu_uploaded_scene_generation =
            objects->preview->dynamic_scene.generation;
        objects->preview->gpu_uploaded_vertex_count =
            objects->preview->source_vertex_count;
    }
    return true;
}

static bool refresh_stage_ordinary_object_scenes(
    RuntimeStageOrdinaryObjects *objects, Vertex *gpu_destination)
{
    if (objects == NULL || !objects->initialized || objects->preview == NULL
            || gpu_destination == NULL) return false;
    if (!update_stage_guard_visibility(objects, NULL)) return false;
    if (objects->resident_install_successes
                == objects->preview->dynamic_scene.install_successes
            && objects->resident_eviction_successes
                == objects->preview->dynamic_scene.eviction_successes)
        return true;
    if (!install_stage_ordinary_object_scenes(objects)) return false;
    return upload_dam_gpu_world_scene(objects->preview, gpu_destination);
}

static void publish_stage_ordinary_visibility(
    RuntimeStageOrdinaryObjects *objects,
    bool guard_visibility_is_current)
{
    const uint64_t started = svcGetSystemTick();
    size_t index;
    if (objects == NULL || !objects->scene_ready || objects->preview == NULL
            || !objects->preview->original_camera_ready) return;
    for (index = 0U; index < objects->entry_count; ++index) {
        RuntimeStageOrdinaryEntry *entry = &objects->entries[index];
        if (!entry->root_active) continue;
        (void)ge_original_prop_state_publish_scene_visibility(
            entry->prop,
            dam_visibility_contains_room(objects->preview, entry->room),
            objects->preview->original_camera_view);
    }
    for (index = 0U; index < objects->interactive.entry_count; ++index) {
        const GeOriginalStageInteractiveEntry *entry =
            ge_original_stage_interactive_entry(
                &objects->interactive, index);

        if (entry == NULL || !entry->constructed
                || entry->type != PROPDEF_DOOR || entry->room < 0
                || entry->room > UINT8_MAX) continue;
        (void)ge_original_prop_state_publish_scene_visibility(
            entry->prop,
            dam_visibility_contains_room(
                objects->preview, (uint8_t)entry->room),
            objects->preview->original_camera_view);
    }
    /* Camera/residency publication already evaluates the authored guard room
     * set before rebuilding object scenes.  Repeating that full snapshot /
     * set_visibility walk here made every moving-camera tick publish all
     * guards twice.  The post-propsTick call passes false because a canonical
     * chr tick can genuinely cross a room boundary without moving Bond. */
    if (!guard_visibility_is_current)
        (void)update_stage_guard_visibility(objects, NULL);
    if (objects->guards != NULL) {
        const size_t guard_count =
            ge_original_stage_guard_runtime_count(objects->guards);
        for (index = 0U; index < guard_count; ++index) {
            GeOriginalStageGuardSnapshot snapshot;
            GeOriginalCharacterSceneState scene_state = {0};
            void *prop = NULL;
            void *chr = NULL;
            if (!ge_original_stage_guard_runtime_snapshot(
                    objects->guards, index, &snapshot)
                    || !ge_original_stage_guard_runtime_actor(
                        objects->guards, index, &prop, &chr)) continue;
            ++objects->guard_visibility_publish_calls;
            if (snapshot.visible != 0U)
                ++objects->guard_visibility_publish_visible_requests;
            if (snapshot.active_linked != 0U)
                ++objects->guard_visibility_publish_active_requests;
            if ((snapshot.prop_flags & PROPFLAG_ENABLED) != 0U)
                ++objects->guard_visibility_publish_enabled_requests;
            /* The unchanged chrTick -> posIsOnScreen path has already
             * evaluated rendered rooms, fog and frustum and owns both fields.
             * Portal residency below is renderer-side only; writing ONSCREEN
             * here made room-visible guards appear through occluding walls. */
            if (ge_original_prop_state_observe_character_scene_state(
                    prop, &scene_state))
                ++objects->guard_visibility_publish_successes;
            if ((scene_state.flags & PROPFLAG_ONSCREEN) != 0U)
                ++objects->guard_visibility_publish_onscreen_outputs;
        }
    }
#if defined(GE_DAM_FULL_PROPS_LIVE)
    /* This adapter only publishes renderer visibility. The unchanged
     * lvlRender-order list/autoaim pass runs once after the final overlay
     * publication below; doing it here repeated both walks during movement. */
    (void)ge_original_door_interaction_bind_onscreen_doors();
#endif
    fine_profile.guard_visibility_publish_ticks += svcGetSystemTick() - started;
}

static void close_stage_ordinary_objects(
    RuntimeStageOrdinaryObjects *objects)
{
    size_t index;
    if (objects == NULL) return;
    /* objFree owns the canonical autogun sound-handle and PropRecord
     * teardown. Run it while the shared active list and Pitem provider are
     * still bound, then release the provider's captured model slot exactly
     * once through the typed ownership adapter. */
    for (index = 0U; index < objects->entry_count; ++index) {
        RuntimeStageOrdinaryEntry *entry = &objects->entries[index];
        GeOriginalStageAutogunLifecycleStatus status;
        if (!entry->live || entry->type != PROPDEF_AUTOGUN) continue;
        status = ge_original_stage_autogun_lifecycle_cleanup_pitem_exact(
            &entry->security, objects->models);
        if (status == GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_OK
                || status
                    == GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_MODEL_RELEASE_FAILED) {
            if (entry->root_active && objects->root_live_count != 0U)
                --objects->root_live_count;
            if (objects->live_count != 0U) --objects->live_count;
            entry->prop = NULL;
            entry->prepared.model_instance = NULL;
            entry->live = false;
            entry->root_active = false;
        } else {
            printf("Autogun cleanup command %lu: %s\n",
                (unsigned long)entry->command_index,
                ge_original_stage_autogun_lifecycle_status_name(status));
        }
    }
    ge_original_stage_active_props_close(&objects->active_props);
    ge_original_stage_mission_runtime_close(&objects->mission_runtime);
    ge_original_stage_objectives_close(&objects->objectives);
    ge_original_stage_safe_runtime_close(&objects->safe_runtime);
    free(objects->active_prop_inputs);
    free(objects->ordinary_scene_parts);
    dam_overlay_segment_close(&objects->door_overlay);
    dam_overlay_segment_close(&objects->guard_overlay);
    ge_original_model_scene_cache_close(&objects->door_scene_cache);
    ge_original_model_scene_cache_close(&objects->guard_scene_cache);
    free(objects->installed_door_scene_generations);
    ge_original_stage_interactive_close(&objects->interactive);
    ge_original_pp7_fire_bind_object_hit_ready(NULL, NULL);
    ge_original_gameplay_services_bind_model_loader(NULL, NULL);
    ge_original_door_bind(NULL, NULL);
    for (index = 0U; index < objects->entry_count; ++index) {
        RuntimeStageArticulatedPublication *publication =
            objects->entries[index].articulated;
        if (objects->entries[index].live
                && !objects->entries[index].root_active)
            ge_original_stage_monitor_release_owned_exact(
                objects->entries[index].definition, objects->models);
        if (publication != NULL) {
            ge_original_model_scene_cache_close(&publication->cache);
            free(publication->batches);
            free(publication->vertices);
            free(publication->inputs);
            free(publication);
        }
        /* MEMPOOL_STAGE allocations, including setupAutogun's canonical
         * 0x30-byte BeamRecord, are discarded with the owning stage. */
        free(objects->entries[index].stage_allocation);
        free(objects->definitions[index]);
    }
    if (objects->guards != NULL)
        ge_original_guard_bullet_hit_bind_stage_guards(NULL, NULL, NULL);
    ge_original_player_body_unbind(objects->guards);
    ge_original_stage_guard_runtime_destroy(objects->guards);
    ge_original_character_model_provider_destroy(objects->guard_models);
    ge_original_pitem_model_provider_destroy(objects->models);
    free(objects->collision_blocks);
    free(objects->definitions);
    free(objects->entries);
    memset(objects, 0, sizeof(*objects));
}

#if !defined(GE_DAM_FULL_PROPS_LIVE)
static bool dam_world_object_scenes_need_refresh(
    const RuntimeDamWorldObjects *objects)
{
    GeOriginalDoorRuntimePublication publication;
    size_t index;
    const void *definitions[2];
    if (objects == NULL || !objects->model_scene_ready) return false;
    definitions[0] = objects->state.first_door.definition;
    definitions[1] = objects->state.second_door.definition;
    for (index = 0U; index < 2U; ++index) {
        if (ge_original_door_runtime_snapshot(
                definitions[index], &publication)
                && publication.generation
                    != objects->installed_door_scene_generation[index])
            return true;
    }
    return false;
}
#endif

#if defined(GE_DAM_FULL_PROPS_LIVE)
static bool refresh_dam_door_overlay(RuntimeDamPreview *preview,
                                     RuntimeDamWorldObjects *objects,
                                     size_t index, bool *updated)
{
    GeOriginalDoorScenePublication publication;
    RuntimeDamOverlaySegment *segment;
    GeDamRoomSceneStorage storage;
    GeOriginalModelScene built;
    void *definition;

    if (updated != NULL) *updated = false;
    if (preview == NULL || objects == NULL || index >= 2U) return false;
    segment = &objects->door_overlay[index];
    definition = index == 0U ? objects->state.first_door.definition
                             : objects->state.second_door.definition;
    if (segment->vertices == NULL || segment->batches == NULL
            || ge_original_door_scene_prepare(
                definition, objects->model178_blob,
                sizeof(objects->model178_blob), &publication)
                != GE_ORIGINAL_DOOR_SCENE_OK)
        return false;
    storage.vertices = segment->vertices;
    storage.vertex_capacity = segment->vertex_count;
    storage.batches = segment->batches;
    storage.batch_capacity = segment->batch_count;
    if (ge_original_model_scene_build(
            &publication.input, &storage, &built)
            != GE_ORIGINAL_MODEL_SCENE_OK
            || built.vertex_count != segment->vertex_count
            || built.batch_count != segment->batch_count)
        return false;
    if (dam_overlay_segment_matches_published(preview, segment)) {
        objects->door_scenes[index] = publication;
        objects->installed_door_scene_generation[index] =
            publication.runtime.generation;
        objects->model_input_rooms[index + 2U] = publication.input.room_id;
        return true;
    }
    objects->model_overlay_status =
        ge_dam_dynamic_scene_update_overlay_segment(
            &preview->dynamic_scene, segment->vertex_offset,
            segment->vertices, built.vertex_count, segment->batch_offset,
            segment->batches, built.batch_count);
    if (objects->model_overlay_status != GE_DAM_DYNAMIC_SCENE_OK)
        return false;
    objects->door_scenes[index] = publication;
    objects->installed_door_scene_generation[index] =
        publication.runtime.generation;
    objects->model_input_rooms[index + 2U] = publication.input.room_id;
    objects->door_overlay_updates++;
    if (updated != NULL) *updated = true;
    return true;
}

static bool refresh_dam_guard_overlay(RuntimeDamPreview *preview,
                                      RuntimeDamWorldObjects *objects,
                                      bool *updated)
{
    RuntimeDamOverlaySegment *segment;
    GeDamRoomSceneStorage storage;
    GeOriginalDamGuardScene scene;

    if (updated != NULL) *updated = false;
    if (preview == NULL || objects == NULL) return false;
    segment = &objects->guard_overlay;
    if (segment->vertex_count == 0U || segment->batch_count == 0U)
        return ge_original_dam_guards_live_count() == 0U;
    storage.vertices = segment->vertices;
    storage.vertex_capacity = segment->vertex_count;
    storage.batches = segment->batches;
    storage.batch_capacity = segment->batch_count;
    if (ge_original_dam_guard_scene_build_cached_with_weapons(
            &objects->guard_scene_cache,
            objects->guard_model_blob, sizeof(objects->guard_model_blob),
            objects->guard_weapon_model_blob,
            sizeof(objects->guard_weapon_model_blob),
            preview->original_camera_view_to_world, &storage, &scene)
            != GE_ORIGINAL_DAM_GUARD_SCENE_OK
            || scene.vertex_count != segment->vertex_count
            || scene.batch_count != segment->batch_count)
        return false;
    objects->guard_scene = scene;
    if (dam_overlay_segment_matches_published(preview, segment)) return true;
    objects->model_overlay_status =
        ge_dam_dynamic_scene_update_overlay_segment(
            &preview->dynamic_scene, segment->vertex_offset,
            segment->vertices, scene.vertex_count, segment->batch_offset,
            segment->batches, scene.batch_count);
    if (objects->model_overlay_status != GE_DAM_DYNAMIC_SCENE_OK)
        return false;
    objects->guard_overlay_updates++;
    if (updated != NULL) *updated = true;
    return true;
}

static bool refresh_dam_live_overlays(RuntimeDamPreview *preview,
                                      RuntimeDamWorldObjects *objects,
                                      Vertex *gpu_destination)
{
    size_t index;
    size_t overlay_scene_vertex_base;
    size_t overlay_scene_batch_base;
    bool full_rebuild = false;
    bool door_updated[2] = { false, false };
    bool guard_updated = false;

    if (preview == NULL || objects == NULL || !objects->model_scene_ready)
        return false;
    for (index = 0U; index < 2U; index++) {
        GeOriginalDoorRuntimePublication publication;
        void *definition = index == 0U
            ? objects->state.first_door.definition
            : objects->state.second_door.definition;
        if (ge_original_door_runtime_snapshot(definition, &publication)
                && publication.generation
                    != objects->installed_door_scene_generation[index]) {
            if (!refresh_dam_door_overlay(
                    preview, objects, index, &door_updated[index]))
                full_rebuild = true;
        }
    }
    if (!full_rebuild && !refresh_dam_guard_overlay(
            preview, objects, &guard_updated))
        full_rebuild = true;
    if (full_rebuild
            && !install_dam_world_object_scenes(preview, objects))
        return false;

    preview->source_vertices = preview->dynamic_scene.vertices;
    preview->batches = preview->dynamic_scene.batches;
    preview->source_vertex_count = preview->dynamic_scene.scene.vertex_count;
    preview->batch_count = preview->dynamic_scene.scene.batch_count;
    preview->triangles = preview->dynamic_scene.scene.triangle_count;
    preview->draws = preview->dynamic_scene.scene.batch_count;
    if (full_rebuild) {
        if (!upload_dam_gpu_world_scene(preview, gpu_destination))
            return false;
    } else {
        overlay_scene_vertex_base = preview->source_vertex_count
            - preview->dynamic_scene.overlay_vertex_count;
        overlay_scene_batch_base = preview->batch_count
            - preview->dynamic_scene.overlay_batch_count;
        for (index = 0U; index < 2U; ++index) {
            const RuntimeDamOverlaySegment *segment =
                &objects->door_overlay[index];
            if (door_updated[index] && !upload_dam_gpu_world_scene_range(
                    preview, gpu_destination,
                    overlay_scene_vertex_base + segment->vertex_offset,
                    segment->vertex_count,
                    overlay_scene_batch_base + segment->batch_offset,
                    segment->batch_count, false)) return false;
        }
        if (guard_updated && !upload_dam_gpu_world_scene_range(
                preview, gpu_destination,
                overlay_scene_vertex_base
                    + objects->guard_overlay.vertex_offset,
                objects->guard_overlay.vertex_count,
                overlay_scene_batch_base
                    + objects->guard_overlay.batch_offset,
                objects->guard_overlay.batch_count, false)) return false;
        preview->gpu_uploaded_scene_generation =
            preview->dynamic_scene.generation;
        preview->gpu_uploaded_vertex_count = preview->source_vertex_count;
    }
    dam_publish_rendered_prop(preview, objects->state.first_glass.prop,
                              objects->model_input_rooms[0]);
    dam_publish_rendered_prop(preview, objects->state.first_object.prop,
                              objects->model_input_rooms[1]);
    dam_publish_rendered_prop(preview, objects->state.first_door.prop,
                              objects->model_input_rooms[2]);
    dam_publish_rendered_prop(preview, objects->state.second_door.prop,
                              objects->model_input_rooms[3]);
    for (index = 0U; index < GE_ORIGINAL_DAM_SPAWN_WINDOW_COUNT; ++index)
        dam_publish_rendered_prop(
            preview, objects->state.spawn_windows[index].prop,
            objects->model_input_rooms[DAM_BASE_OBJECT_COUNT + index]);
    return true;
}
#endif

static void close_dam_world_objects(RuntimeDamWorldObjects *objects)
{
    size_t index;

    ge_original_stage_objective_runtime_close(&objects->objective_runtime);
    ge_original_stage_objectives_close(&objects->objectives);
    ge_original_pitem_model_provider_destroy(objects->pitem_models);
    for (index = 0U; index < objects->definition_count; index++)
        free(objects->definitions[index]);
    ge_original_model62_destroy(objects->model62);
    for (index = 0U; index < DAM_WINDOW_MODEL_INSTANCE_COUNT; ++index)
        ge_original_model104_destroy(objects->model104[index]);
    ge_original_model178_destroy(objects->model178[0]);
    ge_original_model178_destroy(objects->model178[1]);
    ge_original_dam_objective_models_destroy(objects->objective_models);
    dam_overlay_segment_close(&objects->door_overlay[0]);
    dam_overlay_segment_close(&objects->door_overlay[1]);
    dam_overlay_segment_close(&objects->guard_overlay);
    ge_original_dam_guard_scene_cache_close(&objects->guard_scene_cache);
    memset(objects, 0, sizeof(*objects));
}

static void load_bond_animations(GeAssetPack *asset_pack,
                                 RuntimeBondAnimations *animations)
{
    static const char path[] =
        "converted/animations/bond/animation_data.bin";
    static const char sprinting_path[] =
        "converted/animations/bond/sprinting.entry.bin";
    static const char walking_path[] =
        "converted/animations/bond/bond_eye_walk.entry.bin";
    static const char idle_path[] =
        "converted/animations/bond/idle.entry.bin";
    const GeAssetPackEntry *entry;
    const GeAssetPackEntry *idle_entry;
    const GeAssetPackEntry *sprinting_entry;
    const GeAssetPackEntry *walking_entry;
    float position[3];
    float angle;

    memset(animations, 0, sizeof(*animations));
    if (asset_pack == NULL || (entry = ge_asset_pack_find(asset_pack, path)) == NULL
            || entry->data_size == 0U || entry->data_size > SIZE_MAX) return;
    animations->segment_size = (size_t)entry->data_size;
    idle_entry = ge_asset_pack_find(asset_pack, idle_path);
    sprinting_entry = ge_asset_pack_find(asset_pack, sprinting_path);
    walking_entry = ge_asset_pack_find(asset_pack, walking_path);
    if (idle_entry == NULL || sprinting_entry == NULL || walking_entry == NULL ||
            idle_entry->data_size == 0U ||
            sprinting_entry->data_size == 0U ||
            walking_entry->data_size == 0U ||
            idle_entry->data_size > SIZE_MAX ||
            sprinting_entry->data_size > SIZE_MAX ||
            walking_entry->data_size > SIZE_MAX) return;
    animations->idle_frames_size = (size_t)idle_entry->data_size;
    animations->sprinting_frames_size =
        (size_t)sprinting_entry->data_size;
    animations->walking_frames_size = (size_t)walking_entry->data_size;
    animations->segment = malloc(animations->segment_size);
    animations->idle_frames = malloc(animations->idle_frames_size);
    animations->sprinting_frames = malloc(animations->sprinting_frames_size);
    animations->walking_frames = malloc(animations->walking_frames_size);
    if (animations->segment == NULL || animations->idle_frames == NULL ||
            animations->sprinting_frames == NULL ||
            animations->walking_frames == NULL
            || ge_asset_pack_read(asset_pack, path, animations->segment,
                                  animations->segment_size, NULL)
                != GE_ASSET_PACK_OK
            || ge_asset_pack_read(asset_pack, sprinting_path,
                                  animations->sprinting_frames,
                                  animations->sprinting_frames_size, NULL)
                != GE_ASSET_PACK_OK
            || ge_asset_pack_read(asset_pack, idle_path,
                                  animations->idle_frames,
                                  animations->idle_frames_size, NULL)
                != GE_ASSET_PACK_OK
            || ge_asset_pack_read(asset_pack, walking_path,
                                  animations->walking_frames,
                                  animations->walking_frames_size, NULL)
                != GE_ASSET_PACK_OK) goto fail;
    animations->idle = ge_original_animation_root_create(
        animations->segment, animations->segment_size,
        GE_ORIGINAL_BOND_ANIMATION_IDLE);
    animations->sprinting = ge_original_animation_root_create(
        animations->segment, animations->segment_size,
        GE_ORIGINAL_BOND_ANIMATION_SPRINTING);
    animations->walking = ge_original_animation_root_create(
        animations->segment, animations->segment_size,
        GE_ORIGINAL_BOND_ANIMATION_EYE_WALK);
    if (animations->idle == NULL || animations->sprinting == NULL ||
            animations->walking == NULL)
        goto fail;
    if (!ge_original_animation_root_bind_frames(
            animations->idle, animations->idle_frames,
            animations->idle_frames_size) ||
        !ge_original_animation_root_bind_frames(
            animations->sprinting, animations->sprinting_frames,
            animations->sprinting_frames_size) ||
        !ge_original_animation_root_bind_frames(
            animations->walking, animations->walking_frames,
            animations->walking_frames_size)) goto fail;
    if (ge_original_animation_root_frame_count(animations->idle) != 163U
            || ge_original_animation_root_frame_count(animations->walking) != 35U
            || ge_original_animation_root_frame_count(
                animations->sprinting) != 19U
            || !ge_original_animation_root_decode(
                animations->walking, 9U, 0, position, &angle)
            || position[0] != -3.0f || position[1] != 1033.0f
            || position[2] != 59.0f || angle != 0.0f
            || !ge_original_animation_root_decode(
                animations->sprinting, 7U, 0, position, &angle)
            || position[0] != -6.0f || position[1] != 1011.0f
            || position[2] != 157.0f) goto fail;
    animations->decoder_verified = true;
    animations->loaded = true;
    return;

fail:
    ge_original_animation_root_destroy(animations->idle);
    ge_original_animation_root_destroy(animations->walking);
    ge_original_animation_root_destroy(animations->sprinting);
    free(animations->idle_frames);
    free(animations->walking_frames);
    free(animations->sprinting_frames);
    free(animations->segment);
    memset(animations, 0, sizeof(*animations));
}

static void close_bond_animations(RuntimeBondAnimations *animations)
{
    ge_original_player_gait_destroy(animations->gait);
    ge_original_animation_root_destroy(animations->idle);
    ge_original_animation_root_destroy(animations->walking);
    ge_original_animation_root_destroy(animations->sprinting);
    free(animations->idle_frames);
    free(animations->walking_frames);
    free(animations->sprinting_frames);
    free(animations->segment);
    memset(animations, 0, sizeof(*animations));
}

static void initialize_current_player_gait(RuntimeBondAnimations *animations)
{
    if (animations == NULL || !animations->loaded ||
            animations->idle == NULL || animations->walking == NULL) return;
    ge_original_player_gait_bind_bond_animations(
        animations->walking, animations->sprinting);
    animations->gait = ge_original_player_gait_create_current_player(
        animations->walking, &animations->gait_status);
    if (animations->gait == NULL) return;
    animations->gait_verified =
        ge_original_player_gait_calibrate_current_player_standing(
            animations->gait, animations->idle, animations->walking) != 0;
}

typedef struct RuntimeRendererMaterialCache {
    GePicaMaterial material;
    Ge3dsMaterialResult result;
    C3D_Tex *texture;
    Ge3dsMaterialTextureFallback fallback;
    bool valid;
} RuntimeRendererMaterialCache;

enum {
    RENDERER_PREPARED_MATERIAL_CACHE_CAPACITY = 256,
    RENDERER_PREPARED_MATERIAL_CACHE_WAYS = 2,
    RENDERER_PREPARED_MATERIAL_CACHE_SETS =
        RENDERER_PREPARED_MATERIAL_CACHE_CAPACITY
            / RENDERER_PREPARED_MATERIAL_CACHE_WAYS
};

/* Exact input dependency set of ge_pica_apply_compile/material_prepare.
 * Texture identity, ST mapping, lights, and original mux words are consumed
 * elsewhere, not by preparation. The actual texture is still bound per draw.
 * Tests audit compiler field reads and compare cached results byte-for-byte. */
typedef struct RuntimeRendererPreparedMaterialKey {
    uint32_t fallback_flags;
    GePicaColor primitive_color;
    GePicaColor environment_color;
    GePicaCullMode cull_mode;
    GePicaTextureWrap wrap_s, wrap_t;
    GePicaTextureFilter min_filter, mag_filter;
    GePicaCombineMode color_combine;
    GePicaAlphaMode alpha_combine;
    GePicaAlphaTest alpha_test;
    GePicaDepthMode depth_mode;
    Ge3dsMaterialTextureFallback fallback;
    uint8_t fog_enabled, depth_test_enabled, depth_write_enabled;
    uint8_t blend_enabled, alpha_threshold, texture_present;
} RuntimeRendererPreparedMaterialKey;

static void renderer_material_key(const GePicaMaterial *material,
    const Ge3dsMaterialBinding *binding, RuntimeRendererPreparedMaterialKey *key)
{
    memset(key, 0, sizeof(*key));
    key->fallback_flags = material->fallback_flags;
    key->primitive_color = material->primitive_color;
    key->environment_color = material->environment_color;
    key->cull_mode = material->cull_mode;
    key->wrap_s = material->wrap_s;
    key->wrap_t = material->wrap_t;
    key->min_filter = material->min_filter;
    key->mag_filter = material->mag_filter;
    key->color_combine = material->color_combine;
    key->alpha_combine = material->alpha_combine;
    key->alpha_test = material->alpha_test;
    key->depth_mode = material->depth_mode;
    key->fog_enabled = material->fog_enabled;
    key->depth_test_enabled = material->depth_test_enabled;
    key->depth_write_enabled = material->depth_write_enabled;
    key->blend_enabled = material->blend_enabled;
    key->alpha_threshold = material->alpha_threshold;
    key->fallback = binding->missing_texture_fallback;
    key->texture_present = binding->texture0 != NULL;
}

static bool dam_batch_materials_compatible(
    const GeDamRoomDrawBatch *left, const GeDamRoomDrawBatch *right)
{
    return left != NULL && right != NULL
        && left->coordinate_space == right->coordinate_space
        && left->texture_valid == right->texture_valid
        && left->texture.texture_id == right->texture.texture_id
        && memcmp(&left->material, &right->material,
                  sizeof(left->material)) == 0;
}

static bool dam_batches_compatible(const GeDamRoomDrawBatch *left,
                                   const GeDamRoomDrawBatch *right)
{
    return left != NULL && right != NULL
        && left->first_vertex <= SIZE_MAX - left->vertex_count
        && left->first_vertex + left->vertex_count == right->first_vertex
        && dam_batch_materials_compatible(left, right);
}

typedef struct RuntimeRendererPreparedMaterialEntry {
    RuntimeRendererPreparedMaterialKey key;
    Ge3dsMaterialResult result;
    bool valid;
} RuntimeRendererPreparedMaterialEntry;

typedef struct RuntimeRendererPreparedMaterialCache {
    RuntimeRendererPreparedMaterialEntry
        entries[RENDERER_PREPARED_MATERIAL_CACHE_CAPACITY];
    uint8_t least_recent[RENDERER_PREPARED_MATERIAL_CACHE_SETS];
} RuntimeRendererPreparedMaterialCache;

static size_t renderer_material_hash(
    const RuntimeRendererPreparedMaterialKey *key)
{
    const uint8_t *bytes = (const uint8_t *)key;
    uint32_t hash = UINT32_C(2166136261);
    size_t index;

    /* Hash words using memcpy so unaligned material storage and strict
     * aliasing remain valid. Exact byte comparison still decides every hit. */
    for (index = 0U; index + sizeof(uint32_t) <= sizeof(*key);
            index += sizeof(uint32_t)) {
        uint32_t word;
        memcpy(&word, bytes + index, sizeof(word));
        hash ^= word;
        hash *= UINT32_C(16777619);
    }
    for (; index < sizeof(*key); ++index) {
        hash ^= bytes[index];
        hash *= UINT32_C(16777619);
    }
    /* Mix high bits into the set index; authored enum fields commonly have
     * correlated low bits. */
    hash ^= hash >> 16U;
    hash *= UINT32_C(0x85ebca6b);
    hash ^= hash >> 13U;
    hash *= UINT32_C(0xc2b2ae35);
    hash ^= hash >> 16U;
    return hash & (RENDERER_PREPARED_MATERIAL_CACHE_SETS - 1U);
}

static Ge3dsMaterialStatus renderer_prepare_material_cached(
    RuntimeRendererPreparedMaterialCache *cache,
    const GePicaMaterial *material, const Ge3dsMaterialBinding *binding,
    Ge3dsMaterialResult *result, uint64_t *hits, uint64_t *misses)
{
    RuntimeRendererPreparedMaterialEntry *entry;
    RuntimeRendererPreparedMaterialKey key;
    size_t set;
    size_t way;
    size_t victim = RENDERER_PREPARED_MATERIAL_CACHE_WAYS;

    if (cache == NULL || material == NULL || binding == NULL
            || result == NULL) return GE_3DS_MATERIAL_INVALID_ARGUMENT;
    renderer_material_key(material, binding, &key);
    set = renderer_material_hash(&key);
    /* Two ways retain colliding authored materials without increasing the
     * entry budget. Hashes only select candidates: the entire key must match.
     * Recency belongs to this preparation cache, never to GPU draw ordering. */
    for (way = 0U; way < RENDERER_PREPARED_MATERIAL_CACHE_WAYS; ++way) {
        entry = &cache->entries[
            set * RENDERER_PREPARED_MATERIAL_CACHE_WAYS + way];
        if (entry->valid && memcmp(&entry->key, &key, sizeof(key)) == 0) {
            *result = entry->result;
            cache->least_recent[set] = (uint8_t)(way ^ 1U);
            if (hits != NULL) (*hits)++;
            return GE_3DS_MATERIAL_OK;
        }
        if (!entry->valid) victim = way;
    }
    if (victim == RENDERER_PREPARED_MATERIAL_CACHE_WAYS)
        victim = cache->least_recent[set];
    entry = &cache->entries[
        set * RENDERER_PREPARED_MATERIAL_CACHE_WAYS + victim];
    if (misses != NULL) (*misses)++;
    if (ge_3ds_material_prepare(material, binding, result)
            != GE_3DS_MATERIAL_OK) {
        entry->valid = false;
        return GE_3DS_MATERIAL_INVALID_ARGUMENT;
    }
    memcpy(&entry->key, &key, sizeof(key));
    entry->result = *result;
    entry->valid = true;
    cache->least_recent[set] = (uint8_t)(victim ^ 1U);
    return GE_3DS_MATERIAL_OK;
}

static bool renderer_material_result_gpu_equal(
    const Ge3dsMaterialResult *left, const Ge3dsMaterialResult *right)
{
    GePicaApplyState left_state;
    GePicaApplyState right_state;

    if (left == NULL || right == NULL
            || left->texture_bound != right->texture_bound) return false;
    left_state = left->state;
    right_state = right->state;
    /* These two fields are diagnostic provenance only; they never emit PICA
     * commands. Distinct N64 fallback histories with identical effective
     * state can therefore reuse the already-installed GPU state exactly. */
    left_state.material_fallback_flags = 0U;
    left_state.apply_fallback_flags = 0U;
    right_state.material_fallback_flags = 0U;
    right_state.apply_fallback_flags = 0U;
    return memcmp(&left_state, &right_state, sizeof(left_state)) == 0;
}

/* Draw-range gaps and coordinate-space boundaries can prevent geometry
 * coalescing even when the immediately preceding draw left byte-identical
 * material and texture state installed. Reusing that exact state emits no
 * fewer authored draws and changes no ordering; it only avoids submitting
 * the same Citro3D state commands twice. Callers keep one cache per pass so
 * direct sky/HUD/first-person state changes can never cross-contaminate it. */
static Ge3dsMaterialStatus renderer_apply_material_cached(
    RuntimeRendererMaterialCache *cache,
    RuntimeRendererPreparedMaterialCache *prepared_cache,
    const GePicaMaterial *material,
    const Ge3dsMaterialBinding *binding, Ge3dsMaterialResult *result,
    uint64_t *apply_calls, uint64_t *reuse_calls,
    uint64_t *prepare_hits, uint64_t *prepare_misses)
{
    if (cache == NULL || material == NULL || binding == NULL
            || result == NULL) return GE_3DS_MATERIAL_INVALID_ARGUMENT;
    if (cache->valid && cache->texture == binding->texture0
            && cache->fallback == binding->missing_texture_fallback
            && memcmp(&cache->material, material,
                      sizeof(cache->material)) == 0) {
        *result = cache->result;
        if (reuse_calls != NULL) (*reuse_calls)++;
        return GE_3DS_MATERIAL_OK;
    }
    if (renderer_prepare_material_cached(
            prepared_cache, material, binding, result,
            prepare_hits, prepare_misses) != GE_3DS_MATERIAL_OK) {
        cache->valid = false;
        return GE_3DS_MATERIAL_INVALID_ARGUMENT;
    }
    if (cache->valid && cache->texture == binding->texture0
            && renderer_material_result_gpu_equal(&cache->result, result)) {
        if (reuse_calls != NULL) (*reuse_calls)++;
    } else {
        if (apply_calls != NULL) (*apply_calls)++;
        const Ge3dsMaterialBinding previous_binding = {
            cache->texture,
            cache->fallback,
        };
        if (ge_3ds_material_apply_prepared_delta(
                result, binding,
                cache->valid ? &cache->result : NULL,
                cache->valid ? &previous_binding : NULL)
                != GE_3DS_MATERIAL_OK) {
            cache->valid = false;
            return GE_3DS_MATERIAL_INVALID_ARGUMENT;
        }
    }
    cache->material = *material;
    cache->result = *result;
    cache->texture = binding->texture0;
    cache->fallback = binding->missing_texture_fallback;
    cache->valid = true;
    return GE_3DS_MATERIAL_OK;
}

static bool renderer_world_batch_may_draw(
    const RuntimeDamPreview *preview, size_t source_index,
    uint8_t *visibility_cache, size_t visibility_cache_count)
{
    const GeDamRoomDrawBatch *batch;
    const float (*object_to_clip)[4];
    bool visible;

    if (preview == NULL || preview->source_vertices == NULL
            || preview->batches == NULL
            || preview->batch_count < preview->dynamic_scene.overlay_batch_count)
        return true;
    if (visibility_cache != NULL
            && source_index < visibility_cache_count
            && visibility_cache[source_index] != 0U)
        return visibility_cache[source_index] == 1U;
    batch = &preview->batches[source_index];
    object_to_clip = preview->authored_world_to_clip;
    if (batch->coordinate_space == GE_DAM_ROOM_COORDINATE_RUNTIME)
        object_to_clip = preview->runtime_world_to_clip;
    else if (batch->coordinate_space == GE_DAM_ROOM_COORDINATE_EYE)
        object_to_clip = preview->eye_to_clip;
    fine_profile.world_frustum_tests++;
    if (ge_draw_batch_world_first_vertex_visible(
            preview->source_vertices, preview->source_vertex_count,
            batch, object_to_clip)) {
        ++fine_profile.world_frustum_first_vertex_visible;
        visible = true;
    } else {
        const GeDrawBatchBoundsVisibility bounded =
            preview->gpu_batch_bounds != NULL
                    && source_index < preview->gpu_batch_bounds_capacity
                ? ge_draw_batch_world_bounds_classify(
                    &preview->gpu_batch_bounds[source_index], object_to_clip)
                : GE_DRAW_BATCH_BOUNDS_UNCERTAIN;
        if (bounded == GE_DRAW_BATCH_BOUNDS_INSIDE)
            fine_profile.world_frustum_bounds_inside++;
        else if (bounded == GE_DRAW_BATCH_BOUNDS_OUTSIDE)
            fine_profile.world_frustum_bounds_outside++;
        visible = bounded == GE_DRAW_BATCH_BOUNDS_INSIDE
            || (bounded == GE_DRAW_BATCH_BOUNDS_UNCERTAIN
                && ge_draw_batch_world_may_intersect_clip_frustum(
                    preview->source_vertices, preview->source_vertex_count,
                    batch, object_to_clip));
    }
    if (!visible) {
        fine_profile.world_frustum_culled_batches++;
        fine_profile.world_frustum_culled_vertices += batch->vertex_count;
    }
    if (visibility_cache != NULL && source_index < visibility_cache_count)
        visibility_cache[source_index] = visible ? 1U : 2U;
    return visible;
}

static RuntimeGbiModel rareware_body_model;
static RuntimeGbiModel rareware_front_model;

static bool verify_clip_stage(void)
{
    GeGbiProcessedVertex triangle[3] = {0};
    GeGbiClipResult clipped;

    triangle[0].clip[0] = -0.5f;
    triangle[0].clip[1] = -0.5f;
    triangle[1].clip[0] = 0.5f;
    triangle[1].clip[1] = -0.5f;
    triangle[2].clip[1] = 0.5f;
    triangle[0].clip[3] = 1.0f;
    triangle[1].clip[3] = 1.0f;
    triangle[2].clip[3] = 1.0f;
    return ge_gbi_clip_triangle(triangle, &clipped) == GE_GBI_CLIP_OK
        && clipped.triangle_count == 1U;
}

typedef struct AudioAbiSmokeMemory {
    uint8_t codebook[32];
    uint8_t state[32];
} AudioAbiSmokeMemory;

static void *resolve_audio_abi_smoke(void *context, uint32_t address,
                                     size_t size_bytes)
{
    AudioAbiSmokeMemory *memory = context;

    if (memory == NULL) {
        return NULL;
    }
    if (address == UINT32_C(0x1000) && size_bytes <= sizeof(memory->codebook)) {
        return memory->codebook;
    }
    if (address == UINT32_C(0x2000) && size_bytes <= sizeof(memory->state)) {
        return memory->state;
    }
    return NULL;
}

static bool verify_audio_abi_stage(void)
{
    static GeAudioAbiState abi;
    static const uint8_t packed[8] = {
        0x1f, 0x2e, 0x3d, 0x4c, 0x5b, 0x6a, 0x79, 0x80
    };
    static const int16_t expected[16] = {
        4, -4, 8, -8, 12, -12, 16, -16,
        20, -20, 24, -24, 28, -28, -32, 0
    };
    const GeAudioAbiCommand commands[] = {
        {(UINT32_C(11) << 24U) | UINT32_C(32), UINT32_C(0x1000)},
        {(UINT32_C(8) << 24U) | UINT32_C(0x300),
            (UINT32_C(0x400) << 16U) | UINT32_C(32)},
        {(UINT32_C(1) << 24U) | (UINT32_C(1) << 16U), UINT32_C(0x2000)},
    };
    AudioAbiSmokeMemory memory = {0};
    size_t sample;

    ge_audio_abi_init(&abi);
    abi.dmem[0x300] = 0x20U;
    memcpy(abi.dmem + 0x301, packed, sizeof(packed));
    if (ge_audio_abi_execute(&abi, commands,
            sizeof(commands) / sizeof(commands[0]),
            resolve_audio_abi_smoke, &memory) != GE_AUDIO_ABI_OK
            || abi.commands_executed != 3U) {
        return false;
    }
    for (sample = 0U; sample < 16U; sample++) {
        uint16_t expected_bits = (uint16_t)expected[sample];
        size_t output = 0x420U + sample * sizeof(int16_t);

        if (abi.dmem[output] != (uint8_t)(expected_bits >> 8U)
                || abi.dmem[output + 1U] != (uint8_t)expected_bits
                || memory.state[sample * 2U]
                    != (uint8_t)(expected_bits >> 8U)
                || memory.state[sample * 2U + 1U]
                    != (uint8_t)expected_bits) {
            return false;
        }
    }
    return true;
}

typedef struct RuntimeBlotterPreview {
    bool loaded;
    bool textured;
    size_t commands;
    size_t triangles;
    uint16_t texture_id;
    Vertex vertices[BLOTTER_VERTEX_COUNT];
} RuntimeBlotterPreview;

static size_t build_original_sight_vertices(Vertex *vertices,
    GePicaTextureRectangleDraw *draw)
{
    GeOriginalGunSightSnapshot snapshot;
    uint8_t visible;
    float tl_u, tl_v, tr_u, tr_v, bl_u, bl_v, br_u, br_v;
    size_t index;
    ++gun_sight_frames;
    if (!ge_original_gun_sight_snapshot(&snapshot)
            || !ge_original_gun_sight_build_draw(&snapshot, draw, &visible)) {
        ++gun_sight_failures;
        return 0U;
    }
    gun_sight_suppression = snapshot.suppression_reasons;
    if (!visible) return 0U;
    if (!gun_sight_texture_loaded) {
        ++gun_sight_failures;
        return 0U;
    }
    Tex3DS_SubTextureTopLeft(&gun_sight_subtexture, &tl_u, &tl_v);
    Tex3DS_SubTextureTopRight(&gun_sight_subtexture, &tr_u, &tr_v);
    Tex3DS_SubTextureBottomLeft(&gun_sight_subtexture, &bl_u, &bl_v);
    Tex3DS_SubTextureBottomRight(&gun_sight_subtexture, &br_u, &br_v);
    for (index = 0U; index < CROSSHAIR_VERTEX_COUNT; ++index) {
        const GePicaScreenVertex *source = &draw->vertices[index];
        const float u = source->texture_u;
        const float v = source->texture_v;
        const float top_u = tl_u + (tr_u - tl_u) * u;
        const float top_v = tl_v + (tr_v - tl_v) * u;
        const float bottom_u = bl_u + (br_u - bl_u) * u;
        const float bottom_v = bl_v + (br_v - bl_v) * u;
        vertices[index] = (Vertex){
            source->x, source->y, source->z,
            top_u + (bottom_u - top_u) * v,
            top_v + (bottom_v - top_v) * v,
            source->red, source->green, source->blue, source->alpha,
        };
    }
    ++gun_sight_visible_frames;
    return CROSSHAIR_VERTEX_COUNT;
}

static void set_copy_icon_vertices(Vertex *vertices, const Tex3DS_SubTexture *subtexture)
{
    float bottom_left_u;
    float bottom_left_v;
    float bottom_right_u;
    float bottom_right_v;
    float top_left_u;
    float top_left_v;
    float top_right_u;
    float top_right_v;

    Tex3DS_SubTextureBottomLeft(subtexture, &bottom_left_u, &bottom_left_v);
    Tex3DS_SubTextureBottomRight(subtexture, &bottom_right_u, &bottom_right_v);
    Tex3DS_SubTextureTopLeft(subtexture, &top_left_u, &top_left_v);
    Tex3DS_SubTextureTopRight(subtexture, &top_right_u, &top_right_v);
    vertices[0] = (Vertex){16.0f, 16.0f, 0.5f, top_left_u, top_left_v,
                           1.0f, 1.0f, 1.0f, 1.0f};
    vertices[1] = (Vertex){80.0f, 16.0f, 0.5f, top_right_u, top_right_v,
                           1.0f, 1.0f, 1.0f, 1.0f};
    vertices[2] = (Vertex){80.0f, 72.0f, 0.5f, bottom_right_u, bottom_right_v,
                           1.0f, 1.0f, 1.0f, 1.0f};
    vertices[3] = vertices[0];
    vertices[4] = vertices[2];
    vertices[5] = (Vertex){16.0f, 72.0f, 0.5f, bottom_left_u, bottom_left_v,
                           1.0f, 1.0f, 1.0f, 1.0f};
}

static void set_original_ammo_icon_vertices(
    Vertex *vertices, const Tex3DS_SubTexture *subtexture,
    float center_x, float center_y, float width, float height)
{
    float bottom_left_u;
    float bottom_left_v;
    float bottom_right_u;
    float bottom_right_v;
    float top_left_u;
    float top_left_v;
    float top_right_u;
    float top_right_v;
    const float left = center_x - width * 0.5f;
    const float right = center_x + width * 0.5f;
    /* Canonical VI coordinates are top-down; the current Mtx_OrthoTilt
     * screen projection consumes bottom-up Y coordinates. */
    const float pica_center_y =
        ge_3ds_original_hud_screen_y(center_y);
    const float top = pica_center_y + height * 0.5f;
    const float bottom = pica_center_y - height * 0.5f;

    Tex3DS_SubTextureBottomLeft(subtexture, &bottom_left_u, &bottom_left_v);
    Tex3DS_SubTextureBottomRight(subtexture, &bottom_right_u, &bottom_right_v);
    Tex3DS_SubTextureTopLeft(subtexture, &top_left_u, &top_left_v);
    Tex3DS_SubTextureTopRight(subtexture, &top_right_u, &top_right_v);
    vertices[0] = (Vertex){left, top, 0.5f, top_left_u, top_left_v,
                           1.0f, 1.0f, 1.0f, 1.0f};
    vertices[1] = (Vertex){right, top, 0.5f, top_right_u, top_right_v,
                           1.0f, 1.0f, 1.0f, 1.0f};
    vertices[2] = (Vertex){right, bottom, 0.5f,
                           bottom_right_u, bottom_right_v,
                           1.0f, 1.0f, 1.0f, 1.0f};
    vertices[3] = vertices[0];
    vertices[4] = vertices[2];
    vertices[5] = (Vertex){left, bottom, 0.5f,
                           bottom_left_u, bottom_left_v,
                           1.0f, 1.0f, 1.0f, 1.0f};
}

static void load_copy_icon(GeTextureCache *texture_cache,
                           Vertex *vertices)
{
    const GeTextureCacheEntry *entry;
    Tex3DS_Texture imported;
    const Tex3DS_SubTexture *subtexture;

    if (texture_cache == NULL
            || ge_texture_cache_acquire(texture_cache, "COPYICON.bin", 0U,
                                        GE_TEXTURE_CATALOG_EXACT, &entry)
                != GE_TEXTURE_CACHE_OK) {
        return;
    }
    imported = Tex3DS_TextureImport(entry->data, entry->data_size,
                                    &copy_icon_texture, NULL, false);
    if (imported == NULL || Tex3DS_GetNumSubTextures(imported) == 0) {
        if (imported != NULL) {
            Tex3DS_TextureFree(imported);
        }
        (void)ge_texture_cache_release_entry(texture_cache, entry);
        return;
    }
    subtexture = Tex3DS_GetSubTexture(imported, 0);
    set_copy_icon_vertices(vertices + CROSSHAIR_VERTEX_COUNT, subtexture);
    Tex3DS_TextureFree(imported);
    (void)ge_texture_cache_release_entry(texture_cache, entry);
    C3D_TexSetFilter(&copy_icon_texture, GPU_NEAREST, GPU_NEAREST);
    copy_icon_loaded = true;
}

static bool import_packed_texture(GeAssetPack *asset_pack, const char *path,
                                  C3D_Tex *texture,
                                  Tex3DS_SubTexture *subtexture_copy)
{
    const GeAssetPackEntry *entry;
    Tex3DS_Texture imported;
    const Tex3DS_SubTexture *subtexture;
    void *data;

    if (asset_pack == NULL || path == NULL || texture == NULL
            || subtexture_copy == NULL
            || (entry = ge_asset_pack_find(asset_pack, path)) == NULL
            || entry->data_size > SIZE_MAX) {
        return false;
    }
    data = malloc((size_t)entry->data_size);
    if (data == NULL
            || ge_asset_pack_read(asset_pack, path, data,
                                  (size_t)entry->data_size, NULL)
                != GE_ASSET_PACK_OK) {
        free(data);
        return false;
    }
    imported = Tex3DS_TextureImport(data, (size_t)entry->data_size,
                                    texture, NULL, false);
    free(data);
    if (imported == NULL || Tex3DS_GetNumSubTextures(imported) == 0U) {
        if (imported != NULL) {
            Tex3DS_TextureFree(imported);
        }
        return false;
    }
    subtexture = Tex3DS_GetSubTexture(imported, 0U);
    *subtexture_copy = *subtexture;
    Tex3DS_TextureFree(imported);
    C3D_TexSetFilter(texture, GPU_LINEAR, GPU_LINEAR);
    return true;
}

static bool read_packed_exact(GeAssetPack *asset_pack, const char *path,
                              void *destination, size_t expected_size)
{
    const GeAssetPackEntry *entry;

    return asset_pack != NULL && path != NULL && destination != NULL
        && (entry = ge_asset_pack_find(asset_pack, path)) != NULL
        && entry->data_size == expected_size
        && ge_asset_pack_read(asset_pack, path, destination, expected_size,
                              NULL) == GE_ASSET_PACK_OK;
}

static bool import_cached_texture(GeTextureCache *texture_cache,
                                  const char *source,
                                  C3D_Tex *texture,
                                  Tex3DS_SubTexture *subtexture_copy)
{
    const GeTextureCacheEntry *entry;
    Tex3DS_Texture imported;
    const Tex3DS_SubTexture *subtexture;

    if (texture_cache == NULL || source == NULL || texture == NULL
            || subtexture_copy == NULL
            || ge_texture_cache_acquire(texture_cache, source, 0U,
                                        GE_TEXTURE_CATALOG_EXACT, &entry)
                != GE_TEXTURE_CACHE_OK) {
        return false;
    }
    imported = Tex3DS_TextureImport(entry->data, entry->data_size,
                                    texture, NULL, false);
    if (imported == NULL || Tex3DS_GetNumSubTextures(imported) == 0U) {
        if (imported != NULL) Tex3DS_TextureFree(imported);
        (void)ge_texture_cache_release_entry(texture_cache, entry);
        return false;
    }
    subtexture = Tex3DS_GetSubTexture(imported, 0U);
    *subtexture_copy = *subtexture;
    Tex3DS_TextureFree(imported);
    (void)ge_texture_cache_release_entry(texture_cache, entry);
    C3D_TexSetFilter(texture, GPU_LINEAR, GPU_LINEAR);
    return true;
}

static size_t update_stage_autogun_beam_vertices(
    const RuntimeStageOrdinaryObjects *objects,
    const RuntimeDamPreview *preview, Vertex *destination)
{
    GeOriginalStageAutogunBeamSnapshot snapshots[
        GE_3DS_ORIGINAL_AUTOGUN_BEAM_CAPACITY];
    Ge3dsOriginalAutogunBeamTextureUv texture_uv;
    Ge3dsOriginalAutogunBeamDrawList draw_list;
    float viewer_position[3];
    size_t entry_index;
    size_t snapshot_count = 0U;

    if (objects == NULL || preview == NULL || destination == NULL
            || !preview->original_camera_ready
            || !autogun_beam_texture_loaded) return 0U;
    for (entry_index = 0U; entry_index < objects->entry_count; ++entry_index) {
        const RuntimeStageOrdinaryEntry *entry = &objects->entries[entry_index];
        if (!entry->live || entry->type != PROPDEF_AUTOGUN) continue;
        if (snapshot_count >= GE_3DS_ORIGINAL_AUTOGUN_BEAM_CAPACITY
                || !ge_original_stage_autogun_lifecycle_beam_snapshot(
                    &entry->security, &snapshots[snapshot_count])) return 0U;
        ++snapshot_count;
    }
    Tex3DS_SubTextureTopLeft(
        &autogun_beam_subtexture,
        &texture_uv.top_left[0], &texture_uv.top_left[1]);
    Tex3DS_SubTextureTopRight(
        &autogun_beam_subtexture,
        &texture_uv.top_right[0], &texture_uv.top_right[1]);
    Tex3DS_SubTextureBottomLeft(
        &autogun_beam_subtexture,
        &texture_uv.bottom_left[0], &texture_uv.bottom_left[1]);
    Tex3DS_SubTextureBottomRight(
        &autogun_beam_subtexture,
        &texture_uv.bottom_right[0], &texture_uv.bottom_right[1]);
    memcpy(viewer_position, preview->original_camera_position,
           sizeof(viewer_position));
    if (!ge_3ds_original_autogun_beams_build_draw_list(
            snapshots, snapshot_count, viewer_position,
            &texture_uv, &draw_list)
            || draw_list.vertex_count > AUTOGUN_BEAM_VERTEX_CAPACITY)
        return 0U;
    if (draw_list.vertex_count != 0U) {
        memcpy(destination, draw_list.vertices,
               draw_list.vertex_count * sizeof(*destination));
        GSPGPU_FlushDataCache(
            destination,
            renderer_vertex_flush_bytes(
                draw_list.vertex_count, AUTOGUN_BEAM_VERTEX_CAPACITY));
    }
    return draw_list.vertex_count;
}

static size_t update_stage_guard_muzzle_flash_vertices(
    const RuntimeStageOrdinaryObjects *objects,
    const RuntimeDamPreview *preview, Vertex *destination)
{
    size_t weapon_index, flash_count = 0U;
    if(objects==NULL||objects->guards==NULL||preview==NULL
            ||preview->texture_cache==NULL||destination==NULL)return 0U;
    for(weapon_index=0U;
            weapon_index<ge_original_stage_guard_runtime_muzzle_flash_count(
                objects->guards)
                &&flash_count<GUARD_MUZZLE_FLASH_CAPACITY;
            ++weapon_index){
        GeOriginalGuardMuzzleFlashPublication publication;
        GePicaMaterial uv_material;
        const Ge3dsSceneTextureSlot *slot;
        size_t vertex;
        if(!ge_original_stage_guard_runtime_muzzle_flash(
                objects->guards,weapon_index,&publication))continue;
        if(ge_3ds_scene_textures_ensure_image(preview->texture_cache,
                &dam_scene_textures,publication.gunfire.image_id)
                    !=GE_3DS_SCENE_TEXTURE_OK)continue;
        slot=ge_3ds_scene_textures_find(&dam_scene_textures,
                                       publication.gunfire.image_id);
        if(slot==NULL)continue;
        memset(&uv_material,0,sizeof(uv_material));
        uv_material.texture_scale_s=UINT16_MAX;
        uv_material.texture_scale_t=UINT16_MAX;
        for(vertex=0U;vertex<GUARD_MUZZLE_FLASH_VERTICES;++vertex){
            GeTextureUv uv;
            const GeOriginalGuardMuzzleFlashVertex *source=
                &publication.vertices[vertex];
            Vertex *target=&destination[flash_count
                *GUARD_MUZZLE_FLASH_VERTICES+vertex];
            if(ge_3ds_scene_texture_map_uv(slot,source->texture_s,
                    source->texture_t,&uv_material,&uv)!=GE_TEXTURE_UV_OK)
                return 0U;
            target->x=source->position[0];target->y=source->position[1];
            target->z=source->position[2];target->u=uv.u;target->v=uv.v;
            target->r=target->g=target->b=target->a=1.0f;
        }
        guard_muzzle_flash_images[flash_count++]=publication.gunfire.image_id;
    }
    if(flash_count!=0U){
        const size_t vertex_count=flash_count*GUARD_MUZZLE_FLASH_VERTICES;
        GSPGPU_FlushDataCache(destination,renderer_vertex_flush_bytes(
            vertex_count,GUARD_MUZZLE_FLASH_VERTEX_CAPACITY));
        return vertex_count;
    }
    return 0U;
}

static void load_original_ammo_icon_textures(GeTextureCache *texture_cache)
{
    size_t index;

    for (index = 0U; index < GE_ORIGINAL_AMMO_ICON_ASSET_COUNT; ++index) {
        RuntimeAmmoIconTexture *runtime =
            &original_ammo_icon_textures[index];
        runtime->asset = ge_original_ammo_icon_asset_at(index);
        runtime->loaded = runtime->asset != NULL
            && import_cached_texture(texture_cache, runtime->asset->source,
                                     &runtime->texture,
                                     &runtime->subtexture);
        if (runtime->loaded) {
            C3D_TexSetFilter(&runtime->texture,
                             GPU_NEAREST, GPU_NEAREST);
            C3D_TexSetWrap(&runtime->texture,
                           GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        }
    }
}

static void load_original_frontend_sprite_textures(
    GeTextureCache *texture_cache)
{
    size_t index;

    for (index = 0U; index < GE_3DS_ORIGINAL_FRONTEND_MAX_SPRITES;
            ++index) {
        RuntimeFrontendSpriteTexture *runtime =
            &original_frontend_sprite_textures[index];
        const char *source = ge_3ds_original_frontend_sprite_resource(
            (uint8_t)index);
        runtime->loaded = source != NULL
            && import_cached_texture(texture_cache, source,
                                     &runtime->texture,
                                     &runtime->subtexture);
        if (runtime->loaded) {
            C3D_TexSetFilter(&runtime->texture, GPU_NEAREST, GPU_NEAREST);
            C3D_TexSetWrap(&runtime->texture,
                           GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        }
    }
}

static size_t append_original_frontend_sprite_vertices(
    Ge3dsOriginalHudDrawList *draw_list,
    const Ge3dsOriginalFrontendSpriteList *sprites)
{
    size_t sprite_index;
    size_t vertex_count;

    if (draw_list == NULL || sprites == NULL) return 0U;
    vertex_count = draw_list->box_vertex_count
        + draw_list->glyph_vertex_count;
    if (sprites->count > (GE_3DS_ORIGINAL_HUD_VERTEX_CAPACITY
            - vertex_count) / 6U) return 0U;
    for (sprite_index = 0U; sprite_index < sprites->count; ++sprite_index) {
        const Ge3dsOriginalFrontendSprite *sprite =
            &sprites->sprites[sprite_index];
        RuntimeFrontendSpriteTexture *texture;
        Vertex *vertices;
        size_t vertex;
        if ((size_t)sprite->image
                    >= GE_3DS_ORIGINAL_FRONTEND_MAX_SPRITES
                || !(texture = &original_frontend_sprite_textures[
                        sprite->image])->loaded)
            continue;
        vertices = (Vertex *)draw_list->vertices + vertex_count;
        set_original_ammo_icon_vertices(vertices, &texture->subtexture,
            40.0f+(float)sprite->center_x*(320.0f/440.0f),
            (float)sprite->center_y*(240.0f/330.0f),
            (float)sprite->width*(320.0f/440.0f),
            (float)sprite->height*(240.0f/330.0f));
        for (vertex = 0U; vertex < 6U; ++vertex) {
            vertices[vertex].r = (float)sprite->red / 255.0f;
            vertices[vertex].g = (float)sprite->green / 255.0f;
            vertices[vertex].b = (float)sprite->blue / 255.0f;
            vertices[vertex].a = (float)sprite->alpha / 255.0f;
        }
        vertex_count += 6U;
    }
    return vertex_count - draw_list->box_vertex_count
        - draw_list->glyph_vertex_count;
}

static RuntimeAmmoIconTexture *find_original_ammo_icon_texture(
    uint32_t segmented_address)
{
    size_t index;

    for (index = 0U; index < GE_ORIGINAL_AMMO_ICON_ASSET_COUNT; ++index) {
        RuntimeAmmoIconTexture *runtime =
            &original_ammo_icon_textures[index];
        if (runtime->loaded && runtime->asset != NULL
                && runtime->asset->segmented_address == segmented_address) {
            return runtime;
        }
    }
    return NULL;
}

static RuntimeBlotterPreview load_blotter_preview(
    GeAssetPack *asset_pack, GeTextureCache *texture_cache)
{
    uint8_t display_list[GE_BLOTTER_MODEL_DISPLAY_LIST_BYTES];
    uint8_t vertices[GE_BLOTTER_MODEL_VERTEX_BYTES];
    uint8_t matrix[GE_BLOTTER_MODEL_MATRIX_BYTES];
    RuntimeBlotterPreview preview = {0};
    GeBlotterModelBlobs blobs;
    GeBlotterModel *model;
    Tex3DS_SubTexture subtexture;
    float top_left_u = 0.0f;
    float top_left_v = 0.0f;
    float top_right_u = 0.0f;
    float top_right_v = 0.0f;
    float bottom_left_u = 0.0f;
    float bottom_left_v = 0.0f;
    float bottom_right_u = 0.0f;
    float bottom_right_v = 0.0f;
    size_t triangle_index;
    size_t output_index = 0U;

    if (!read_packed_exact(asset_pack, GE_BLOTTER_MODEL_DISPLAY_LIST_PATH,
                           display_list, sizeof(display_list))
            || !read_packed_exact(asset_pack, GE_BLOTTER_MODEL_VERTICES_PATH,
                                  vertices, sizeof(vertices))
            || !read_packed_exact(asset_pack,
                                  GE_BLOTTER_MODEL_PREVIEW_MATRIX_PATH,
                                  matrix, sizeof(matrix))) {
        return preview;
    }
    model = malloc(sizeof(*model));
    if (model == NULL) return preview;
    blobs = (GeBlotterModelBlobs){
        display_list, sizeof(display_list), vertices, sizeof(vertices),
        matrix, sizeof(matrix),
    };
    if (ge_blotter_model_build(&blobs, model) != GE_BLOTTER_MODEL_OK) {
        free(model);
        return preview;
    }

    preview.textured = import_cached_texture(
        texture_cache, "BLOTTER.bin", &blotter_texture, &subtexture);
    if (preview.textured) {
        Tex3DS_SubTextureTopLeft(&subtexture, &top_left_u, &top_left_v);
        Tex3DS_SubTextureTopRight(&subtexture, &top_right_u, &top_right_v);
        Tex3DS_SubTextureBottomLeft(&subtexture, &bottom_left_u,
                                   &bottom_left_v);
        Tex3DS_SubTextureBottomRight(&subtexture, &bottom_right_u,
                                    &bottom_right_v);
        blotter_texture_loaded = true;
    }
    for (triangle_index = 0U; triangle_index < model->triangle_count;
            triangle_index++) {
        size_t vertex_index;

        for (vertex_index = 0U; vertex_index < 3U; vertex_index++) {
            const GeGbiVertex *source =
                &model->triangles[triangle_index].vertices[vertex_index].source;
            const float texture_s = (float)source->texture_s / 2048.0f;
            const float texture_t = (float)source->texture_t / 1024.0f;
            const float top_u = top_left_u
                + (top_right_u - top_left_u) * texture_s;
            const float top_v = top_left_v
                + (top_right_v - top_left_v) * texture_s;
            const float bottom_u = bottom_left_u
                + (bottom_right_u - bottom_left_u) * texture_s;
            const float bottom_v = bottom_left_v
                + (bottom_right_v - bottom_left_v) * texture_s;

            preview.vertices[output_index++] = (Vertex){
                305.0f + (float)source->x * (70.0f / 360.0f),
                188.0f + (float)source->z * (34.0f / 240.0f),
                0.55f,
                top_u + (bottom_u - top_u) * texture_t,
                top_v + (bottom_v - top_v) * texture_t,
                1.0f, 1.0f, 1.0f, 1.0f,
            };
        }
    }
    preview.loaded = output_index == BLOTTER_VERTEX_COUNT;
    preview.commands = model->pipeline.traversal.commands_visited;
    preview.triangles = model->pipeline.triangles;
    preview.texture_id = model->material.texture_id;
    free(model);
    return preview;
}

typedef struct DamOpeningRoomData {
    uint8_t *point_table;
    size_t point_table_size;
    uint8_t *primary_gdl;
    size_t primary_gdl_size;
    uint8_t *secondary_gdl;
    size_t secondary_gdl_size;
} DamOpeningRoomData;

static uint8_t *read_packed_alloc(GeAssetPack *asset_pack, const char *path,
                                  size_t *size_out)
{
    const GeAssetPackEntry *entry;
    uint8_t *data;

    if (size_out != NULL) *size_out = 0U;
    if (asset_pack == NULL || path == NULL || size_out == NULL
            || (entry = ge_asset_pack_find(asset_pack, path)) == NULL
            || entry->data_size == 0U || entry->data_size > SIZE_MAX) {
        return NULL;
    }
    data = malloc((size_t)entry->data_size);
    if (data == NULL || ge_asset_pack_read(asset_pack, path, data,
                                           (size_t)entry->data_size, NULL)
            != GE_ASSET_PACK_OK) {
        free(data);
        return NULL;
    }
    *size_out = (size_t)entry->data_size;
    return data;
}

static unsigned stage_collision_boot_step;
static int stage_collision_boot_status;
static size_t stage_collision_boot_native_size;

static bool load_stage_collision(GeAssetPack *asset_pack,
                                 const GeStageAssetDescriptor *stage_assets,
                                 RuntimeDamCollision *collision)
{
    if (stage_assets == NULL || collision == NULL) return false;
    memset(collision, 0, sizeof(*collision));
    stage_collision_boot_step = 1U;
    collision->blob = read_packed_alloc(
        asset_pack, stage_assets->collision_path,
        &collision->blob_size);
    if (collision->blob == NULL) {
        free(collision->blob);
        memset(collision, 0, sizeof(*collision));
        return false;
    }
    stage_collision_boot_step = 2U;
    stage_collision_boot_status = ge_stan_collision_open(
        collision->blob, collision->blob_size, &collision->surface);
    if (stage_collision_boot_status != GE_STAN_COLLISION_OK) {
        free(collision->blob);
        memset(collision, 0, sizeof(*collision));
        return false;
    }
    stage_collision_boot_step = 3U;
    stage_collision_boot_status = ge_stan_native_required_size(
        &collision->surface, &collision->native_size);
    stage_collision_boot_native_size = collision->native_size;
    if (stage_collision_boot_status != GE_STAN_COLLISION_OK) {
        free(collision->blob);
        memset(collision, 0, sizeof(*collision));
        return false;
    }
    stage_collision_boot_step = 4U;
    collision->native_storage = malloc(collision->native_size);
    if (collision->native_storage == NULL) {
        free(collision->blob);
        memset(collision, 0, sizeof(*collision));
        return false;
    }
    stage_collision_boot_step = 5U;
    stage_collision_boot_status = ge_stan_native_materialize(
        &collision->surface, stage_assets->level_scale,
        collision->native_storage, collision->native_size,
        &collision->native);
    if (stage_collision_boot_status != GE_STAN_COLLISION_OK) {
        free(collision->native_storage);
        free(collision->blob);
        memset(collision, 0, sizeof(*collision));
        return false;
    }
    stage_collision_boot_step = 6U;
    collision->original_bound = ge_stan_native_bind_original(
        &collision->native) == GE_STAN_COLLISION_OK;
    collision->loaded = true;
    stage_collision_boot_step = 7U;
    return true;
}

static void validate_stage_spawn(RuntimeDamCollision *collision,
                                 const GeOriginalStageSpawn *setup_spawn)
{
    GeStanNativeTile *spawn;
    GeStanNativeTile *radius_tile;
    float original_x;
    float original_z;

    if (collision == NULL || !collision->original_bound
            || setup_spawn == NULL || setup_spawn->plink == NULL)
        return;
    spawn = ge_original_stan_match_tile_name(
        &collision->native, setup_spawn->plink);
    original_x = setup_spawn->position[0] / collision->native.level_scale;
    original_z = setup_spawn->position[2] / collision->native.level_scale;
    collision->original_spawn_matched = spawn == collision->native.spawn_tile;
    collision->original_spawn_in_bounds = spawn != NULL
        && ge_original_stan_test_point_within_bounds(
            &collision->native, spawn, original_x, original_z);
    if (spawn == NULL) return;
    radius_tile = spawn;
    collision->original_spawn_radius_clear = ge_original_stan_test_radius(
        &collision->native, &radius_tile, original_x, original_z, 1.0f)
        == GE_ORIGINAL_STAN_COLLISION_NONE;
    collision->original_spawn_floor_y = ge_original_stan_get_position_y(
        &collision->native, spawn, original_x, original_z)
        * collision->native.level_scale;
}

static GeOriginalStageSetupStatus stage_setup_load_audit =
    GE_ORIGINAL_STAGE_SETUP_INVALID_ARGUMENT;
static GeOriginalStageSetupStatus stage_setup_stan_audit =
    GE_ORIGINAL_STAGE_SETUP_INVALID_ARGUMENT;
static bool stage_setup_spawn_audit = false;

static bool load_original_stage_setup(
    GeAssetPack *asset_pack, const GeStageAssetDescriptor *stage_assets,
    RuntimeDamCollision *collision, GeOriginalStageSetupRuntime *runtime,
    GeOriginalStageSpawn *spawn, stagesetup **setup)
{
    if (stage_assets == NULL || collision == NULL || runtime == NULL
            || spawn == NULL || setup == NULL)
        return false;
    memset(runtime, 0, sizeof(*runtime));
    memset(spawn, 0, sizeof(*spawn));
    *setup = NULL;
    stage_setup_load_audit = ge_original_stage_setup_load(
        asset_pack, stage_assets, runtime);
    if (stage_setup_load_audit != GE_ORIGINAL_STAGE_SETUP_OK) {
        ge_original_stage_setup_close(runtime);
        return false;
    }
    stage_setup_stan_audit = ge_original_stage_setup_bind_stan(
        runtime, &collision->native);
    if (stage_setup_stan_audit != GE_ORIGINAL_STAGE_SETUP_OK) {
        ge_original_stage_setup_close(runtime);
        return false;
    }
    if (stage_assets->stage == GE_STAGE_DAM) {
        GeOriginalDamSpawn dam_spawn;
        /* Dam's dedicated bootstrap still owns its canonical setup pointer
         * and intro selection. Also parse the same packaged authored setup so
         * later native systems (objectives, alarms, and subsequent complete
         * prop materialization) consume the common command/relationship ABI
         * used by every other stage. */
        stage_setup_spawn_audit =
            ge_original_dam_setup_normal_spawn(&dam_spawn) != 0;
        if (!stage_setup_spawn_audit) {
            ge_original_stage_setup_close(runtime);
            return false;
        }
        spawn->pad_id = dam_spawn.pad_id;
        memcpy(spawn->position, dam_spawn.position, sizeof(spawn->position));
        memcpy(spawn->up, dam_spawn.up, sizeof(spawn->up));
        memcpy(spawn->look, dam_spawn.look, sizeof(spawn->look));
        spawn->plink = dam_spawn.plink;
        spawn->stan = ge_original_stan_match_tile_name(
            &collision->native, spawn->plink);
        *setup = ge_original_dam_setup_get();
    } else {
        stage_setup_spawn_audit =
            ge_original_stage_setup_normal_spawn(runtime, spawn) != 0;
        if (!stage_setup_spawn_audit) {
            ge_original_stage_setup_close(runtime);
            return false;
        }
        *setup = ge_original_stage_setup_get(runtime);
    }
    validate_stage_spawn(collision, spawn);
    return *setup != NULL && spawn->stan != NULL;
}

static void close_dam_collision(RuntimeDamCollision *collision)
{
    if (collision == NULL) return;
    free(collision->native_storage);
    free(collision->blob);
    memset(collision, 0, sizeof(*collision));
}

static void free_dam_opening_data(DamOpeningRoomData *rooms,
                                  size_t room_count)
{
    size_t index;

    for (index = 0U; index < room_count; ++index) {
        free(rooms[index].point_table);
        free(rooms[index].primary_gdl);
        free(rooms[index].secondary_gdl);
    }
}

static void load_dam_room_preview(GeAssetPack *asset_pack,
                                  GeTextureCache *texture_cache,
                                  const GeStageAssetDescriptor *stage_assets,
                                  const GeOriginalStageSpawn *stage_spawn,
                                  RuntimeDamPreview *preview,
                                  Vertex *destination)
{
    DamOpeningRoomData owned[DAM_WORLD_ROOM_LOAD_CAPACITY] = {0};
    GeDamRoomBlobDescriptor descriptors[DAM_WORLD_ROOM_LOAD_CAPACITY] = {0};
    GeDamRoomWorldVertex *world_vertices = NULL;
    GeDamRoomDrawBatch *batches = NULL;
    GeDamRoomSceneStorage storage = {0};
    GeDamRoomScene scene;
    float minimum_x = INFINITY;
    float maximum_x = -INFINITY;
    float minimum_y = INFINITY;
    float maximum_y = -INFINITY;
    float minimum_depth = INFINITY;
    float maximum_depth = -INFINITY;
    float scale;
    size_t room_index;
    size_t index;
    uint8_t *background;
    size_t background_size;
    size_t room_count = 0U;

    (void)texture_cache;
    if (stage_assets == NULL || preview == NULL || destination == NULL) return;
    memset(preview, 0, sizeof(*preview));
    preview->stage_assets = stage_assets;
    preview->world_status = GE_DAM_WORLD_INVALID_BACKGROUND;
    preview->preload_status = GE_DAM_PRELOAD_INVALID_ARGUMENT;
    preview->dynamic_scene_status = GE_DAM_DYNAMIC_SCENE_INVALID_ARGUMENT;
    preview->texture_cache = texture_cache;
    preview->scene_textures = &dam_scene_textures;
    background = read_packed_alloc(
        asset_pack, stage_assets->background_path, &background_size);
    if (background == NULL) return;
    preview->world_status = ge_dam_world_parse(
        background, background_size, &preview->world);
    free(background);
    if (preview->world_status != GE_DAM_WORLD_OK) return;
    preview->visibility_status = ge_dam_visibility_runtime_load_for_stage(
        asset_pack, stage_assets, &preview->visibility_runtime);
    preview->environment_ready = ge_original_stage_environment_select(
        stage_assets->level_id, 0, &preview->environment);
    if (preview->environment_ready) {
        const float near_distance = preview->environment.fog_enabled != 0U
            ? preview->environment.blend_multiplier : 15.0f;
        const float far_distance = preview->environment.fog_enabled != 0U
            ? preview->environment.far_fog : 10000.0f;
        (void)ge_dam_visibility_runtime_set_zrange(
            &preview->visibility_runtime, near_distance, far_distance);
    }
    preview->world_status = ge_dam_world_collect_connected(
        &preview->world, stage_assets->expected_spawn_room,
        preview->world_room_ids,
        DAM_WORLD_ROOM_LOAD_CAPACITY, &room_count);
    if (preview->world_status != GE_DAM_WORLD_OK || room_count == 0U) return;
    preview->world_room_count = room_count;
    for (index = 0U; index < preview->world.portal_count; ++index) {
        preview->portal_controls[index] =
            preview->world.portals[index].control_bytes1;
    }
    preview->preload_status = ge_dam_preload_queue_init(
        &preview->preload_queue, preview->world.room_count,
        GE_DAM_PRELOAD_MAX_ROOMS, preview->world_room_ids,
        preview->world_room_count);
    if (preview->preload_status == GE_DAM_PRELOAD_OK) {
        preview->visibility_providers = ge_dam_preload_queue_providers(
            &preview->preload_queue, preview->portal_controls,
            preview->world.portal_count);
    }
    preview->setup_loaded = stage_spawn != NULL && stage_spawn->plink != NULL;
    if (preview->setup_loaded) {
        preview->spawn_pad = stage_spawn->pad_id;
        memcpy(preview->spawn_position, stage_spawn->position,
               sizeof(preview->spawn_position));
        preview->spawn_plink = stage_spawn->plink;
    }
    for (room_index = 0U; room_index < room_count; ++room_index) {
        char path[GE_STAGE_ASSET_PATH_CAPACITY];
        DamOpeningRoomData *room = &owned[room_index];
        GeDamRoomBlobDescriptor *descriptor = &descriptors[room_index];
        const uint8_t room_id = preview->world_room_ids[room_index];
        const GeDamWorldRoom *world_room = ge_dam_world_room(
            &preview->world, room_id);

        if (world_room == NULL) {
            free_dam_opening_data(owned, room_count);
            return;
        }

        if (ge_stage_asset_room_path(
                stage_assets, room_id, GE_STAGE_ROOM_POINTS,
                path, sizeof(path)) != GE_STAGE_ASSET_OK) {
            free_dam_opening_data(owned, room_count);
            return;
        }
        room->point_table = read_packed_alloc(asset_pack, path,
                                              &room->point_table_size);
        if (ge_stage_asset_room_path(
                stage_assets, room_id, GE_STAGE_ROOM_PRIMARY_GDL,
                path, sizeof(path)) != GE_STAGE_ASSET_OK) {
            free_dam_opening_data(owned, room_count);
            return;
        }
        room->primary_gdl = read_packed_alloc(asset_pack, path,
                                              &room->primary_gdl_size);
        if (ge_stage_asset_room_path(
                stage_assets, room_id, GE_STAGE_ROOM_SECONDARY_GDL,
                path, sizeof(path)) != GE_STAGE_ASSET_OK) {
            free_dam_opening_data(owned, room_count);
            return;
        }
        room->secondary_gdl = read_packed_alloc(asset_pack, path,
                                                &room->secondary_gdl_size);
        if (room->point_table == NULL || room->primary_gdl == NULL) {
            free_dam_opening_data(owned, room_count);
            return;
        }
        descriptor->room_id = room_id;
        memcpy(descriptor->origin, world_room->origin,
               sizeof(descriptor->origin));
        descriptor->point_table = room->point_table;
        descriptor->point_table_size = room->point_table_size;
        descriptor->primary_gdl = room->primary_gdl;
        descriptor->primary_gdl_size = room->primary_gdl_size;
        descriptor->secondary_gdl = room->secondary_gdl;
        descriptor->secondary_gdl_size = room->secondary_gdl_size;
    }

    if (ge_dam_rooms_build(descriptors, room_count, NULL, NULL, &scene)
            != GE_DAM_ROOM_CAPACITY_EXCEEDED
            || scene.required_vertex_count == 0U
            || scene.required_vertex_count > DAM_SCENE_VERTEX_CAPACITY
            || scene.required_batch_count == 0U) {
        free_dam_opening_data(owned, room_count);
        return;
    }
    world_vertices = malloc(scene.required_vertex_count
                            * sizeof(*world_vertices));
    batches = malloc(scene.required_batch_count * sizeof(*batches));
    if (world_vertices == NULL || batches == NULL) {
        free(world_vertices);
        free(batches);
        free_dam_opening_data(owned, room_count);
        return;
    }
    storage = (GeDamRoomSceneStorage){
        world_vertices, scene.required_vertex_count,
        batches, scene.required_batch_count,
    };
    if (ge_dam_rooms_build(descriptors, room_count, NULL, &storage, &scene)
            != GE_DAM_ROOM_OK) {
        free(world_vertices);
        free(batches);
        free_dam_opening_data(owned, room_count);
        return;
    }
    (void)ge_3ds_scene_textures_load(
        texture_cache, batches, scene.batch_count, dam_scene_texture_slots,
        DAM_SCENE_TEXTURE_CAPACITY, &dam_scene_textures);

    for (index = 0U; index < scene.vertex_count; ++index) {
        const float projected_x = world_vertices[index].world[0]
            - world_vertices[index].world[2];
        const float projected_y = 0.5f * (world_vertices[index].world[0]
            + world_vertices[index].world[2]) - world_vertices[index].world[1];
        const float depth = 0.5f * (world_vertices[index].world[0]
            + world_vertices[index].world[2]) + world_vertices[index].world[1];
        if (projected_x < minimum_x) minimum_x = projected_x;
        if (projected_x > maximum_x) maximum_x = projected_x;
        if (projected_y < minimum_y) minimum_y = projected_y;
        if (projected_y > maximum_y) maximum_y = projected_y;
        if (depth < minimum_depth) minimum_depth = depth;
        if (depth > maximum_depth) maximum_depth = depth;
    }
    scale = fminf(360.0f / (maximum_x - minimum_x),
                  210.0f / (maximum_y - minimum_y));
    for (index = 0U; index < scene.vertex_count; ++index) {
        const GeGbiVertex *source = &world_vertices[index].source;
        const float projected_x = world_vertices[index].world[0]
            - world_vertices[index].world[2];
        const float projected_y = 0.5f * (world_vertices[index].world[0]
            + world_vertices[index].world[2]) - world_vertices[index].world[1];
        const float depth = 0.5f * (world_vertices[index].world[0]
            + world_vertices[index].world[2]) + world_vertices[index].world[1];
        const float shade = 0.48f + 0.52f
            * fabsf((float)(int8_t)source->green / 127.0f);

        destination[index] = (Vertex){
            200.0f + (projected_x - 0.5f * (minimum_x + maximum_x)) * scale,
            120.0f - (projected_y - 0.5f * (minimum_y + maximum_y)) * scale,
            0.10f + 0.80f * (depth - minimum_depth)
                / (maximum_depth - minimum_depth),
            (float)source->texture_s / 2048.0f,
            (float)source->texture_t / 2048.0f,
            shade * 0.68f, shade * 0.76f, shade * 0.62f, 1.0f,
        };
    }
    for (index = 0U; index < scene.batch_count; ++index) {
        const GeDamRoomDrawBatch *batch = &batches[index];
        const Ge3dsSceneTextureSlot *slot = ge_3ds_scene_textures_find(
            &dam_scene_textures, batch->texture.texture_id);
        size_t vertex_index;

        if (slot == NULL) continue;
        for (vertex_index = batch->first_vertex;
                vertex_index < batch->first_vertex + batch->vertex_count;
                ++vertex_index) {
            GeTextureUv uv;

            if (ge_3ds_scene_texture_map_uv(
                    slot, world_vertices[vertex_index].source.texture_s,
                    world_vertices[vertex_index].source.texture_t,
                    &batch->material, &uv) == GE_TEXTURE_UV_OK) {
                destination[vertex_index].u = uv.u;
                destination[vertex_index].v = uv.v;
            }
        }
    }
    {
        if (preview->setup_loaded) {
            const float spawn_projected_x = preview->spawn_position[0]
                - preview->spawn_position[2];
            const float spawn_projected_y = 0.5f * (preview->spawn_position[0]
                + preview->spawn_position[2]) - preview->spawn_position[1];

            preview->spawn_screen_x = 200.0f
                + (spawn_projected_x - 0.5f * (minimum_x + maximum_x)) * scale;
            preview->spawn_screen_y = 120.0f
                - (spawn_projected_y - 0.5f * (minimum_y + maximum_y)) * scale;
        } else {
            preview->spawn_screen_x = 200.0f;
            preview->spawn_screen_y = 120.0f;
        }
    }
    preview->loaded = true;
    preview->rooms = scene.room_count;
    preview->lists = scene.list_count;
    preview->commands = scene.commands_visited;
    preview->draws = scene.batch_count;
    preview->triangles = scene.triangle_count;
    preview->vertex_count = scene.vertex_count;
    preview->source_vertex_count = scene.vertex_count;
    preview->batch_count = scene.batch_count;
    preview->material_groups = scene.batch_count != 0U ? 1U : 0U;
    for (index = 1U; index < scene.batch_count; ++index) {
        if (!dam_batches_compatible(&batches[index - 1U], &batches[index])) {
            preview->material_groups++;
        }
    }
    preview->source_vertices = world_vertices;
    preview->batches = batches;
    {
        const GeDamDynamicSceneLimits dynamic_limits = {
            GE_DAM_WORLD_MAX_ROOMS,
            DAM_SCENE_PROJECTED_VERTEX_CAPACITY,
            DAM_SCENE_PROJECTED_VERTEX_CAPACITY,
        };

        preview->dynamic_scene_status = ge_dam_dynamic_scene_init_for_stage(
            &preview->dynamic_scene, asset_pack, stage_assets, &preview->world,
            preview->world_room_ids, preview->world_room_count,
            &dynamic_limits);
        if (preview->dynamic_scene_status == GE_DAM_DYNAMIC_SCENE_OK) {
            free(batches);
            free(world_vertices);
            preview->source_vertices = preview->dynamic_scene.vertices;
            preview->batches = preview->dynamic_scene.batches;
            preview->source_vertex_count =
                preview->dynamic_scene.scene.vertex_count;
            preview->batch_count = preview->dynamic_scene.scene.batch_count;
        }
    }
    free_dam_opening_data(owned, room_count);
}

static bool dam_visibility_contains_room(const RuntimeDamPreview *preview,
                                         uint32_t room)
{
    size_t index;

    if (preview == NULL || !preview->visibility_ready) return true;
    for (index = 0U; index < preview->visibility_result.room_count; ++index) {
        if (preview->visibility_result.rooms[index].room == room) return true;
    }
    return false;
}

static float c3d_matrix_element(const C3D_Mtx *matrix, size_t row,
                                size_t column)
{
    switch (column) {
    case 0U: return matrix->r[row].x;
    case 1U: return matrix->r[row].y;
    case 2U: return matrix->r[row].z;
    default: return matrix->r[row].w;
    }
}

static void matrix4_multiply(float output[4][4], const float left[4][4],
                             const float right[4][4])
{
    size_t row;
    size_t column;
    size_t inner;

    for (row = 0U; row < 4U; ++row) {
        for (column = 0U; column < 4U; ++column) {
            output[row][column] = 0.0f;
            for (inner = 0U; inner < 4U; ++inner)
                output[row][column] +=
                    left[row][inner] * right[inner][column];
        }
    }
}

static bool prepare_dam_gpu_world_projection(
    RuntimeDamPreview *preview,
    const GeOriginalBondCameraResult *original)
{
    const float level_scale = preview != NULL && preview->stage_assets != NULL
        ? preview->stage_assets->level_scale : 0.0f;
    GeDamCamera runtime_camera;
    GeDamCamera authored_camera;
    float row_view_projection[4][4];
    float world_to_clip[4][4];
    float clip_to_screen[4][4] = {{0.0f}};
    float ortho_tilt[4][4];
    float screen_world[4][4];
    float final[4][4];
    size_t row;
    size_t column;

    if (preview == NULL || original == NULL || level_scale <= 0.0f)
        return false;
    if (ge_dam_camera_prepare_rsp_viewport(
            original->view, original->projection, original->viewport_scale,
            original->viewport_translation, &runtime_camera)
            != GE_DAM_CAMERA_OK
            || ge_dam_camera_scale_world(
                &runtime_camera, 1.0f / level_scale, &authored_camera)
                != GE_DAM_CAMERA_OK) {
        return false;
    }

    /* Original matrices consume row vectors. The PICA shader consumes column
     * vectors, so transpose authored view*projection before composing the
     * exact RSP viewport and the already-live Citro3D tilted framebuffer map. */
    matrix4_multiply(row_view_projection, authored_camera.view,
                     authored_camera.projection);
    memcpy(preview->authored_world_to_clip, row_view_projection,
           sizeof(preview->authored_world_to_clip));
    for (row = 0U; row < 4U; ++row)
        for (column = 0U; column < 4U; ++column)
            world_to_clip[row][column] =
                row_view_projection[column][row];

    clip_to_screen[0][0] = authored_camera.viewport_scale[0];
    clip_to_screen[0][3] = authored_camera.viewport_translation[0];
    clip_to_screen[1][1] = authored_camera.viewport_scale[1];
    clip_to_screen[1][3] = authored_camera.viewport_translation[1];
    clip_to_screen[2][2] = 0.5f;
    clip_to_screen[2][3] = 0.5f;
    clip_to_screen[3][3] = 1.0f;
    for (row = 0U; row < 4U; ++row) {
        for (column = 0U; column < 4U; ++column)
            ortho_tilt[row][column] =
                c3d_matrix_element(&projection, row, column);
    }
    matrix4_multiply(screen_world, clip_to_screen, world_to_clip);
    matrix4_multiply(final, ortho_tilt, screen_world);
    for (row = 0U; row < 4U; ++row) {
        preview->gpu_world_projection.r[row] = FVec4_New(
            final[row][0], final[row][1], final[row][2], final[row][3]);
    }
    /* Hand/model scene vertices are already in original runtime space through
     * the canonical model matrices, so their GPU projection must not apply
     * the authored-background scale used by Dam room vertices. */
    matrix4_multiply(row_view_projection, runtime_camera.view,
                     runtime_camera.projection);
    memcpy(preview->runtime_world_to_clip, row_view_projection,
           sizeof(preview->runtime_world_to_clip));
    for (row = 0U; row < 4U; ++row)
        for (column = 0U; column < 4U; ++column)
            world_to_clip[row][column] =
                row_view_projection[column][row];
    matrix4_multiply(screen_world, clip_to_screen, world_to_clip);
    matrix4_multiply(final, ortho_tilt, screen_world);
    for (row = 0U; row < 4U; ++row) {
        preview->gpu_runtime_projection.r[row] = FVec4_New(
            final[row][0], final[row][1], final[row][2], final[row][3]);
    }
    /* The original first-person model renderer has already applied
     * hand->mtxlist and published eye-space coordinates.  Compose its exact
     * projection and viewport without the world camera view; this is
     * algebraically the same hand path as view_to_world followed by the
     * world shader's view transform, without two inverse CPU/GPU transforms. */
    for (row = 0U; row < 4U; ++row)
        for (column = 0U; column < 4U; ++column)
            world_to_clip[row][column] =
                runtime_camera.projection[column][row];
    memcpy(preview->eye_to_clip, runtime_camera.projection,
           sizeof(preview->eye_to_clip));
    matrix4_multiply(screen_world, clip_to_screen, world_to_clip);
    matrix4_multiply(final, ortho_tilt, screen_world);
    for (row = 0U; row < 4U; ++row) {
        preview->gpu_first_person_projection.r[row] = FVec4_New(
            final[row][0], final[row][1], final[row][2], final[row][3]);
    }
    preview->spawn_screen_x = runtime_camera.viewport_translation[0];
    preview->spawn_screen_y = runtime_camera.viewport_translation[1];
    /* GPU world projection is the live camera handoff.  The legacy CPU
     * projection path also publishes this status, but may not run on a frame
     * with no pending room transaction. */
    preview->camera_handoff_status = GE_DAM_CAMERA_OK;
    preview->gpu_world_ready = true;
    return true;
}

static bool upload_dam_gpu_world_scene_range(RuntimeDamPreview *preview,
    Vertex *destination, size_t vertex_offset, size_t vertex_count,
    size_t batch_offset, size_t batch_count, bool map_texture_uv)
{
    size_t batch_index;
    size_t vertex_index;

    if (preview == NULL || destination == NULL
            || preview->source_vertices == NULL || preview->batches == NULL
            || vertex_offset > preview->source_vertex_count
            || vertex_count > preview->source_vertex_count - vertex_offset
            || batch_offset > preview->batch_count
            || batch_count > preview->batch_count - batch_offset) {
        return false;
    }
    if (preview->gpu_batch_bounds_capacity < preview->batch_count) {
        GeDrawBatchWorldBounds *bounds = realloc(preview->gpu_batch_bounds,
            preview->batch_count * sizeof(*bounds));
        if (bounds != NULL) {
            memset(bounds + preview->gpu_batch_bounds_capacity, 0,
                (preview->batch_count - preview->gpu_batch_bounds_capacity)
                    * sizeof(*bounds));
            preview->gpu_batch_bounds = bounds;
            preview->gpu_batch_bounds_capacity = preview->batch_count;
        } else {
            /* Bounds are an optional acceleration, never a reason to lose
             * geometry or retain stale indices after a topology change. */
            free(preview->gpu_batch_bounds);
            preview->gpu_batch_bounds = NULL;
            preview->gpu_batch_bounds_capacity = 0U;
        }
    }
    for (vertex_index = vertex_offset;
            vertex_index < vertex_offset + vertex_count;
            ++vertex_index) {
        const GeDamRoomWorldVertex *source =
            &preview->source_vertices[vertex_index];
        destination[vertex_index] = (Vertex){
            source->world[0], source->world[1], source->world[2],
            map_texture_uv ? source->processed.texture[0]
                           : destination[vertex_index].u,
            map_texture_uv ? source->processed.texture[1]
                           : destination[vertex_index].v,
            (float)source->processed.rgba[0] / 255.0f,
            (float)source->processed.rgba[1] / 255.0f,
            (float)source->processed.rgba[2] / 255.0f,
            (float)source->processed.rgba[3] / 255.0f,
        };
    }
    for (batch_index = batch_offset;
            batch_index < batch_offset + batch_count;
            ++batch_index) {
        const GeDamRoomDrawBatch *batch = &preview->batches[batch_index];
        const Ge3dsSceneTextureSlot *slot;

        if (preview->gpu_batch_bounds != NULL) {
            GeDrawBatchWorldBounds *bounds =
                &preview->gpu_batch_bounds[batch_index];
            bounds->valid = 0;
            /* Static rooms amortize the bound construction across camera
             * updates. Short batches and per-tick animated overlays keep
             * the original vertex test, avoiding more work than it saves. */
            if (preview->batch_count >= preview->dynamic_scene.overlay_batch_count
                    && batch_index < preview->batch_count
                        - preview->dynamic_scene.overlay_batch_count
                    && batch->vertex_count >= 12U)
                (void)ge_draw_batch_world_bounds_build(
                    preview->source_vertices, preview->source_vertex_count,
                    batch, bounds);
        }
        if (!map_texture_uv) continue;
        slot = ge_3ds_scene_textures_find(
            preview->scene_textures, batch->texture.texture_id);

        if (slot == NULL) continue;
        for (vertex_index = batch->first_vertex;
                vertex_index < batch->first_vertex + batch->vertex_count;
                ++vertex_index) {
            GeTextureUv uv;
            if (ge_3ds_scene_texture_map_uv(
                    slot,
                    preview->source_vertices[vertex_index].source.texture_s,
                    preview->source_vertices[vertex_index].source.texture_t,
                    &batch->material, &uv) == GE_TEXTURE_UV_OK) {
                destination[vertex_index].u = uv.u;
                destination[vertex_index].v = uv.v;
            }
        }
    }
    if (vertex_count != 0U) {
        if (preview->gpu_dirty_vertex_count == 0U) {
            preview->gpu_dirty_vertex_offset = vertex_offset;
            preview->gpu_dirty_vertex_count = vertex_count;
        } else {
            const size_t old_end = preview->gpu_dirty_vertex_offset
                + preview->gpu_dirty_vertex_count;
            const size_t new_end = vertex_offset + vertex_count;
            const size_t combined_start = vertex_offset
                    < preview->gpu_dirty_vertex_offset
                ? vertex_offset : preview->gpu_dirty_vertex_offset;
            const size_t combined_end = new_end > old_end
                ? new_end : old_end;
            preview->gpu_dirty_vertex_offset = combined_start;
            preview->gpu_dirty_vertex_count =
                combined_end - combined_start;
        }
    }
    return true;
}

static bool upload_dam_gpu_world_scene(RuntimeDamPreview *preview,
                                       Vertex *destination)
{
    if (preview == NULL || destination == NULL
            || preview->source_vertex_count > DAM_ROOM_VERTEX_COUNT) {
        return false;
    }
    if (preview->gpu_uploaded_vertex_count == preview->source_vertex_count
            && preview->gpu_uploaded_scene_generation
                == preview->dynamic_scene.generation) {
        preview->vertex_count = preview->source_vertex_count;
        return true;
    }
    if (!upload_dam_gpu_world_scene_range(
            preview, destination, 0U, preview->source_vertex_count,
            0U, preview->batch_count, true)) {
        return false;
    }
    preview->vertex_count = preview->source_vertex_count;
    preview->gpu_uploaded_scene_generation =
        preview->dynamic_scene.generation;
    preview->gpu_uploaded_vertex_count = preview->source_vertex_count;
    return true;
}

static bool project_dam_with_original_camera(
    RuntimeDamPreview *preview,
    const GeOriginalBondCameraResult *original,
    Vertex *destination)
{
    const float level_scale = preview != NULL && preview->stage_assets != NULL
        ? preview->stage_assets->level_scale : 0.0f;
    GeDamCamera runtime_camera;
    GeDamCamera authored_camera;
    GeDamRoomWorldVertex *camera_vertices = NULL;
    GeDamCameraVertex *projected = NULL;
    RuntimeDamRenderBatch *render_batches = NULL;
    GeDamRoomDrawBatch *visible_batches = NULL;
    size_t *visible_source_indices = NULL;
    const GeDamRoomDrawBatch *camera_batches = preview->batches;
    size_t camera_batch_count = preview->batch_count;
    GeDamCameraSceneResult scene_result;
    GeDamCameraSceneStorage scene_storage;
    size_t batch_index;
    size_t vertex_index;

    if (level_scale <= 0.0f) return false;
    if (preview->visibility_ready) {
        camera_batch_count = 0U;
        for (batch_index = 0U; batch_index < preview->batch_count;
                ++batch_index) {
            if (dam_visibility_contains_room(
                    preview, preview->batches[batch_index].room_id)) {
                camera_batch_count++;
            }
        }
        if (camera_batch_count == 0U) {
            /* A canonical portal/logic room may have no vertex table and a
             * visibility set containing only other no-geometry rooms (the
             * Streets outdoor partitions are the first live example).  The
             * PICA world path below correctly draws only the environment in
             * this state.  Treat an empty clipped scene as a successful
             * camera result so room residency can still commit; rejecting it
             * deadlocks the preload queue waiting for fabricated geometry. */
            preview->camera_handoff_status = GE_DAM_CAMERA_OK;
            preview->render_batch_count = 0U;
            preview->vertex_count = 0U;
            preview->camera_input_triangles = 0U;
            preview->camera_visible_triangles = 0U;
            preview->camera_output_triangles = 0U;
            return true;
        }
        visible_batches = malloc(camera_batch_count * sizeof(*visible_batches));
        visible_source_indices = malloc(
            camera_batch_count * sizeof(*visible_source_indices));
        if (visible_batches == NULL || visible_source_indices == NULL)
            goto fail;
        camera_batch_count = 0U;
        for (batch_index = 0U; batch_index < preview->batch_count;
                ++batch_index) {
            if (dam_visibility_contains_room(
                    preview, preview->batches[batch_index].room_id)) {
                visible_batches[camera_batch_count] =
                    preview->batches[batch_index];
                visible_source_indices[camera_batch_count] = batch_index;
                camera_batch_count++;
            }
        }
        camera_batches = visible_batches;
    }

    preview->camera_handoff_status = ge_dam_camera_prepare_rsp_viewport(
        original->view, original->projection, original->viewport_scale,
        original->viewport_translation, &runtime_camera);
    if (preview->camera_handoff_status != GE_DAM_CAMERA_OK) return false;
    preview->camera_handoff_status = ge_dam_camera_scale_world(
        &runtime_camera, 1.0f / level_scale, &authored_camera);
    if (preview->camera_handoff_status != GE_DAM_CAMERA_OK) return false;

    camera_vertices = malloc(preview->source_vertex_count
                             * sizeof(*camera_vertices));
    if (camera_vertices == NULL) goto fail;
    memcpy(camera_vertices, preview->source_vertices,
           preview->source_vertex_count * sizeof(*camera_vertices));

    /* UV conversion happens on a projection-only copy. The decoded source
     * vertices and material batches remain available in authored form. */
    for (batch_index = 0U; batch_index < preview->batch_count; ++batch_index) {
        const GeDamRoomDrawBatch *batch = &preview->batches[batch_index];
        const Ge3dsSceneTextureSlot *slot = ge_3ds_scene_textures_find(
            preview->scene_textures, batch->texture.texture_id);
        size_t vertex_index;

        if (slot == NULL) continue;
        for (vertex_index = batch->first_vertex;
                vertex_index < batch->first_vertex + batch->vertex_count;
                ++vertex_index) {
            GeTextureUv uv;

            if (ge_3ds_scene_texture_map_uv(
                    slot, camera_vertices[vertex_index].source.texture_s,
                    camera_vertices[vertex_index].source.texture_t,
                    &batch->material, &uv) == GE_TEXTURE_UV_OK) {
                camera_vertices[vertex_index].processed.texture[0] = uv.u;
                camera_vertices[vertex_index].processed.texture[1] = uv.v;
            }
        }
    }

    /* The projection adapter already performs a sizing pass before emission.
     * Supplying the frontend's fixed capacities directly avoids a redundant
     * third traversal of every visible Dam triangle on each camera update. */
    projected = malloc(DAM_SCENE_PROJECTED_VERTEX_CAPACITY
                       * sizeof(*projected));
    render_batches = malloc(camera_batch_count * sizeof(*render_batches));
    if (projected == NULL || render_batches == NULL) {
        preview->camera_handoff_status = GE_DAM_CAMERA_INVALID_CONFIG;
        goto fail;
    }
    scene_storage = (GeDamCameraSceneStorage){
        projected, DAM_SCENE_PROJECTED_VERTEX_CAPACITY,
        render_batches, camera_batch_count,
    };
    preview->camera_handoff_status = ge_dam_camera_project_batches_bounded(
        &authored_camera, camera_vertices, preview->source_vertex_count,
        camera_batches, camera_batch_count, &scene_storage, &scene_result);
    if (preview->camera_handoff_status != GE_DAM_CAMERA_OK) goto fail;
    if (visible_source_indices != NULL) {
        for (batch_index = 0U; batch_index < scene_result.batch_count;
                ++batch_index) {
            if (render_batches[batch_index].source_batch
                    >= camera_batch_count) goto fail;
            render_batches[batch_index].source_batch =
                visible_source_indices[
                    render_batches[batch_index].source_batch];
        }
    }
    for (vertex_index = 0U; vertex_index < scene_result.vertex_count;
            ++vertex_index) {
        const GeDamCameraVertex *source = &projected[vertex_index];

        destination[vertex_index] = (Vertex){
            source->screen[0], source->screen[1], source->screen[2],
            source->texture[0], source->texture[1],
            (float)source->rgba[0] / 255.0f,
            (float)source->rgba[1] / 255.0f,
            (float)source->rgba[2] / 255.0f,
            (float)source->rgba[3] / 255.0f,
        };
    }

    free(preview->render_batches);
    preview->render_batches = render_batches;
    preview->render_batch_count = scene_result.batch_count;
    preview->vertex_count = scene_result.vertex_count;
    preview->camera_input_triangles = scene_result.input_triangle_count;
    preview->camera_visible_triangles =
        scene_result.visible_input_triangle_count;
    preview->camera_output_triangles = scene_result.output_triangle_count;
    preview->spawn_screen_x = runtime_camera.viewport_translation[0];
    preview->spawn_screen_y = runtime_camera.viewport_translation[1];
    free(projected);
    free(camera_vertices);
    free(visible_source_indices);
    free(visible_batches);
    return true;

fail:
    free(visible_source_indices);
    free(visible_batches);
    free(render_batches);
    free(projected);
    free(camera_vertices);
    return false;
}

/* Reprojection boundary for the next original bondview/player integration:
 * callers supply runtime-space camera state; this function reruns the exact
 * decompiled matrix producer and atomically publishes clipped render batches.
 * A failed update leaves a previously valid camera frame intact. */
static bool update_original_dam_camera(
    RuntimeDamPreview *preview,
    const float camera_position[3],
    const float camera_look_direction[3],
    const float camera_up[3],
    uint8_t room,
    Vertex *destination,
    bool project_scene)
{
    const float level_scale = preview != NULL && preview->stage_assets != NULL
        ? preview->stage_assets->level_scale : 0.0f;
    const float visibility_scale = preview != NULL
            && preview->stage_assets != NULL
        ? preview->stage_assets->visibility_scale : 0.0f;
    GeOriginalBondCameraConfig config;
    GeOriginalBondCameraResult camera;
    const GeDamWorldRoom *camera_room;
    const bool previous_ready = preview != NULL
        && preview->original_camera_ready;
    bool dynamic_projected = false;
    size_t axis;

    if (preview == NULL || camera_position == NULL
            || camera_look_direction == NULL || camera_up == NULL
            || destination == NULL || !preview->loaded
            || level_scale <= 0.0f || visibility_scale <= 0.0f) return false;
    visual_probe_record_stream_phase(preview, 1U, room);
    preview->original_camera_status =
        GE_ORIGINAL_BOND_CAMERA_INVALID_CONFIG;
    preview->camera_handoff_status = GE_DAM_CAMERA_INVALID_CONFIG;
    camera_room = ge_dam_world_room(&preview->world, room);
    if (camera_room == NULL) return false;

    memset(&config, 0, sizeof(config));
    for (axis = 0U; axis < 3U; ++axis) {
        config.camera_position[axis] = camera_position[axis];
        config.camera_look_direction[axis] = camera_look_direction[axis];
        config.camera_up[axis] = camera_up[axis];
        config.room_origin[axis] =
            camera_room->origin[axis] / level_scale;
    }
    config.room_position_scale = level_scale;
    config.camera_local_scale = visibility_scale;
    config.visibility_scale = visibility_scale;
    config.viewport_scale[0] = 640;
    config.viewport_scale[1] = 480;
    config.viewport_scale[2] = 511;
    /* Keep the original 320x240 viewport and center it on the 400x240 top
     * screen, producing faithful 40-pixel pillarboxes. */
    config.viewport_translation[0] = 800;
    config.viewport_translation[1] = 480;
    config.viewport_translation[2] = 511;
    config.room = room;
    /* fogLoadLevelEnvironment owns the authored stage Z range. */
    preview->original_camera_status =
        ge_original_bond_camera_set_perspective(
            &config, 60.0f, 4.0f / 3.0f,
            preview->visibility_runtime.near_distance,
            preview->visibility_runtime.far_distance);
    if (preview->original_camera_status != GE_ORIGINAL_BOND_CAMERA_OK) {
        preview->original_camera_ready = previous_ready;
        return false;
    }

    preview->original_camera_status =
        ge_original_bond_camera_run(&config, &camera);
    if (preview->original_camera_status != GE_ORIGINAL_BOND_CAMERA_OK) {
        preview->original_camera_ready = previous_ready;
        return false;
    }
    visual_probe_record_stream_phase(preview, 2U, room);
    (void)ge_original_bond_camera_publish_live_player(&config, &camera);
    preview->original_camera_matrices = camera.matrix_allocations;
    preview->original_camera_lights = camera.light_allocations;
    preview->original_camera_room = camera.room;
    memcpy(preview->original_camera_view, camera.view,
           sizeof(preview->original_camera_view));
    memcpy(preview->original_camera_view_to_world, camera.view_to_world,
           sizeof(preview->original_camera_view_to_world));
    memcpy(preview->original_camera_projection, camera.projection,
           sizeof(preview->original_camera_projection));
    memcpy(preview->original_camera_position, camera_position,
           sizeof(preview->original_camera_position));
    memcpy(preview->original_camera_look, camera_look_direction,
           sizeof(preview->original_camera_look));
    memcpy(preview->original_camera_up, camera_up,
           sizeof(preview->original_camera_up));
    memcpy(preview->original_camera_viewport_scale, camera.viewport_scale,
           sizeof(preview->original_camera_viewport_scale));
    memcpy(preview->original_camera_viewport_translation,
           camera.viewport_translation,
           sizeof(preview->original_camera_viewport_translation));
    preview->visibility_status = preview->preload_status == GE_DAM_PRELOAD_OK
        ? ge_dam_visibility_runtime_run_with_providers(
            &preview->visibility_runtime, &camera, camera_position,
            &preview->visibility_providers, &preview->visibility_result)
        : ge_dam_visibility_runtime_run(
            &preview->visibility_runtime, &camera, camera_position,
            &preview->visibility_result);
    preview->visibility_ready = preview->visibility_status
        == GE_DAM_VISIBILITY_RUNTIME_OK;
    visual_probe_record_stream_phase(preview, 3U, room);
    if (preview->visibility_ready
            && preview->dynamic_scene.initialized != 0U) {
        uint8_t rendered_rooms[GE_ORIGINAL_BG_VISIBILITY_MAX_VISIBLE];
        size_t rendered_room_count;
        for (rendered_room_count = 0U;
                rendered_room_count < preview->visibility_result.room_count;
                ++rendered_room_count) {
            rendered_rooms[rendered_room_count] =
                preview->visibility_result.rooms[rendered_room_count].room;
        }
        preview->dynamic_scene_status =
            ge_dam_dynamic_scene_age_visibility(
                &preview->dynamic_scene, rendered_rooms,
                preview->visibility_result.room_count);
    }
    visual_probe_record_stream_phase(preview, 4U, room);
    if (!prepare_dam_gpu_world_projection(preview, &camera)) {
        preview->original_camera_ready = previous_ready;
        return false;
    }
    visual_probe_record_stream_phase(preview, 5U, room);
    if (!project_scene) {
        preview->original_camera_ready = true;
        return true;
    }
    if (preview->dynamic_scene.initialized != 0U
            && preview->preload_queue.pending_count != 0U) {
        GeDamDynamicSceneTransaction transaction;
        RuntimeDamPreview *candidate_preview = NULL;
        Ge3dsSceneTextureSlot *candidate_slots = NULL;
        Ge3dsSceneTextures candidate_textures = {0};
        Vertex *candidate_destination = NULL;
        Ge3dsSceneTextureStatus texture_status;
        bool candidate_projected = false;

        preview->stream_texture_status =
            GE_3DS_SCENE_TEXTURE_INVALID_ARGUMENT;
        preview->stream_camera_status = GE_DAM_CAMERA_INVALID_CONFIG;
        preview->stream_allocation_failed = 0U;
        visual_probe_record_stream_phase(preview, 6U, room);
        preview->dynamic_scene_status = ge_dam_dynamic_scene_prepare_next(
            &preview->dynamic_scene, &preview->preload_queue, &transaction);
        visual_probe_record_stream_phase(preview, 7U,
            preview->dynamic_scene_status == GE_DAM_DYNAMIC_SCENE_OK
                ? transaction.room : room);
        if (preview->dynamic_scene_status == GE_DAM_DYNAMIC_SCENE_OK) {
            /* RuntimeDamPreview contains the complete world/visibility
             * scratch state. Keeping this transaction copy on the 3DS main
             * thread stack consumed over 18 KiB and overflowed only when a
             * nonresident authored room first entered this branch. */
            candidate_preview = malloc(sizeof(*candidate_preview));
            candidate_slots = calloc(DAM_SCENE_TEXTURE_CAPACITY,
                                     sizeof(*candidate_slots));
            /* The live PICA path consumes authored world vertices with the
             * original camera matrix and hardware clipping.  A second full
             * CPU-clipped copy is only needed by the legacy fallback; keeping
             * it alive during streaming duplicated several megabytes and
             * caused valid Streets room transactions to abort under heap
             * pressure. */
            if (!preview->gpu_world_ready) {
                candidate_destination = malloc(
                    DAM_SCENE_PROJECTED_VERTEX_CAPACITY
                        * sizeof(*candidate_destination));
            }
            if (candidate_preview != NULL && candidate_slots != NULL
                    && (preview->gpu_world_ready
                        || candidate_destination != NULL)) {
                visual_probe_record_stream_phase(preview, 8U,
                    transaction.room);
                texture_status = ge_3ds_scene_textures_load(
                    preview->texture_cache, transaction.batches,
                    transaction.scene.batch_count, candidate_slots,
                    DAM_SCENE_TEXTURE_CAPACITY, &candidate_textures);
                if (texture_status == GE_3DS_SCENE_TEXTURE_OK
                        || texture_status == GE_3DS_SCENE_TEXTURE_PARTIAL) {
                    *candidate_preview = *preview;
                    candidate_preview->camera_handoff_status =
                        GE_DAM_CAMERA_INVALID_CONFIG;
                    candidate_preview->source_vertices = transaction.vertices;
                    candidate_preview->batches = transaction.batches;
                    candidate_preview->source_vertex_count =
                        transaction.scene.vertex_count;
                    candidate_preview->batch_count =
                        transaction.scene.batch_count;
                    candidate_preview->render_batches = NULL;
                    candidate_preview->render_batch_count = 0U;
                    candidate_preview->scene_textures = &candidate_textures;
                    visual_probe_record_stream_phase(preview, 9U,
                        transaction.room);
                    if (candidate_preview->gpu_world_ready) {
                        candidate_preview->camera_handoff_status =
                            transaction.scene.vertex_count
                                    <= DAM_ROOM_VERTEX_COUNT
                                ? GE_DAM_CAMERA_OK
                                : GE_DAM_CAMERA_CAPACITY_EXCEEDED;
                        candidate_projected =
                            candidate_preview->camera_handoff_status
                                == GE_DAM_CAMERA_OK;
                    } else {
                        candidate_projected =
                            project_dam_with_original_camera(
                                candidate_preview, &camera,
                                candidate_destination);
                    }
                    visual_probe_record_stream_phase(preview, 10U,
                        transaction.room);
                    preview->stream_camera_status =
                        candidate_preview->camera_handoff_status;
                }
                preview->stream_texture_status = texture_status;
            } else {
                preview->stream_allocation_failed = 1U;
            }
            if (candidate_projected) {
                visual_probe_record_stream_phase(preview, 11U,
                    transaction.room);
                preview->dynamic_scene_status = ge_dam_dynamic_scene_commit(
                    &preview->dynamic_scene, &preview->preload_queue,
                    &transaction);
                visual_probe_record_stream_phase(preview, 12U,
                    transaction.room);
            }
            if (candidate_projected
                    && preview->dynamic_scene_status
                        == GE_DAM_DYNAMIC_SCENE_OK) {
                GeDamRoomScene *published = &preview->dynamic_scene.scene;

                ge_3ds_scene_textures_close(&dam_scene_textures);
                memcpy(dam_scene_texture_slots, candidate_slots,
                    DAM_SCENE_TEXTURE_CAPACITY * sizeof(*candidate_slots));
                dam_scene_textures = candidate_textures;
                dam_scene_textures.slots = dam_scene_texture_slots;
                candidate_textures.slots = NULL;
                if (candidate_destination != NULL) {
                    memcpy(destination, candidate_destination,
                        candidate_preview->vertex_count
                            * sizeof(*destination));
                }
                free(preview->render_batches);
                preview->render_batches = candidate_preview->render_batches;
                preview->render_batch_count =
                    candidate_preview->render_batch_count;
                preview->camera_handoff_status =
                    candidate_preview->camera_handoff_status;
                preview->camera_input_triangles =
                    candidate_preview->camera_input_triangles;
                preview->camera_visible_triangles =
                    candidate_preview->camera_visible_triangles;
                preview->camera_output_triangles =
                    candidate_preview->camera_output_triangles;
                preview->source_vertices = preview->dynamic_scene.vertices;
                preview->batches = preview->dynamic_scene.batches;
                preview->rooms = published->room_count;
                preview->lists = published->list_count;
                preview->commands = published->commands_visited;
                preview->draws = published->batch_count;
                preview->triangles = published->triangle_count;
                preview->vertex_count = candidate_preview->vertex_count;
                preview->source_vertex_count = published->vertex_count;
                preview->batch_count = published->batch_count;
                preview->scene_textures = &dam_scene_textures;
                preview->material_groups = published->batch_count != 0U
                    ? 1U : 0U;
                for (axis = 1U; axis < published->batch_count; ++axis) {
                    if (!dam_batches_compatible(
                            &preview->batches[axis - 1U],
                            &preview->batches[axis])) {
                        ++preview->material_groups;
                    }
                }
                dynamic_projected = true;
                visual_probe_record_stream_phase(preview, 13U,
                    transaction.room);
            } else {
                if (candidate_preview != NULL)
                    free(candidate_preview->render_batches);
                ge_dam_dynamic_scene_abort(
                    &preview->dynamic_scene, &transaction);
            }
            ge_3ds_scene_textures_close(&candidate_textures);
            free(candidate_destination);
            free(candidate_slots);
            free(candidate_preview);
        }
    }
    (void)dynamic_projected;
    visual_probe_record_stream_phase(preview, 14U, room);
    if (!upload_dam_gpu_world_scene(preview, destination)) {
        preview->original_camera_ready = previous_ready;
        return false;
    }
    preview->original_camera_ready = true;
    visual_probe_record_stream_phase(preview, 15U, room);
    return true;
}

static void initialize_original_dam_camera(
    RuntimeDamPreview *preview, const GeOriginalStageSpawn *spawn,
    Vertex *destination)
{
    const float level_scale = preview != NULL && preview->stage_assets != NULL
        ? preview->stage_assets->level_scale : 0.0f;
    float runtime_position[3];
    size_t axis;

    if (preview == NULL || destination == NULL || !preview->loaded
            || spawn == NULL || level_scale <= 0.0f) return;
    for (axis = 0U; axis < 3U; ++axis) {
        /* proplvreset2 moves setup coordinates into runtime space. */
        runtime_position[axis] = spawn->position[axis] / level_scale;
    }
    (void)update_original_dam_camera(preview, runtime_position, spawn->look,
        spawn->up, preview->world_room_ids[0], destination, true);
}

static void map_rareware_quad_uv(RuntimeGbiMesh *mesh, size_t texture_index,
                                 const Tex3DS_SubTexture *subtexture)
{
    const size_t first = texture_index * 6U;
    float minimum_s = mesh->vertices[first].u;
    float maximum_s = minimum_s;
    float minimum_t = mesh->vertices[first].v;
    float maximum_t = minimum_t;
    float top_left_u;
    float top_left_v;
    float top_right_u;
    float top_right_v;
    float bottom_left_u;
    float bottom_left_v;
    float bottom_right_u;
    float bottom_right_v;
    size_t index;

    for (index = first + 1U; index < first + 6U; index++) {
        const Vertex *vertex = &mesh->vertices[index];
        if (vertex->u < minimum_s) minimum_s = vertex->u;
        if (vertex->u > maximum_s) maximum_s = vertex->u;
        if (vertex->v < minimum_t) minimum_t = vertex->v;
        if (vertex->v > maximum_t) maximum_t = vertex->v;
    }
    Tex3DS_SubTextureTopLeft(subtexture, &top_left_u, &top_left_v);
    Tex3DS_SubTextureTopRight(subtexture, &top_right_u, &top_right_v);
    Tex3DS_SubTextureBottomLeft(subtexture, &bottom_left_u, &bottom_left_v);
    Tex3DS_SubTextureBottomRight(subtexture, &bottom_right_u, &bottom_right_v);

    for (index = first; index < first + 6U; index++) {
        Vertex *vertex = &mesh->vertices[index];
        const float s = (vertex->u - minimum_s) / (maximum_s - minimum_s);
        const float t = (vertex->v - minimum_t) / (maximum_t - minimum_t);
        const float top_u = top_left_u + (top_right_u - top_left_u) * s;
        const float top_v = top_left_v + (top_right_v - top_left_v) * s;
        const float bottom_u = bottom_left_u + (bottom_right_u - bottom_left_u) * s;
        const float bottom_v = bottom_left_v + (bottom_right_v - bottom_left_v) * s;

        vertex->u = top_u + (bottom_u - top_u) * t;
        vertex->v = top_v + (bottom_v - top_v) * t;
        vertex->r = 1.0f;
        vertex->g = 1.0f;
        vertex->b = 1.0f;
        vertex->a = 1.0f;
    }
}

static bool load_rareware_textures(GeAssetPack *asset_pack,
                                   RuntimeGbiMesh *mesh)
{
    static const char *const paths[] = {
        "converted/runtime/rareware-textures/rare-r.t3x",
        "converted/runtime/rareware-textures/rare-a.t3x",
        "converted/runtime/rareware-textures/rare-r2.t3x",
        "converted/runtime/rareware-textures/rare-e.t3x",
    };
    size_t index;

    if (mesh == NULL || !mesh->loaded) {
        return false;
    }
    for (index = 0U; index < sizeof(paths) / sizeof(paths[0]); index++) {
        Tex3DS_SubTexture subtexture;

        if (!import_packed_texture(asset_pack, paths[index],
                                   &rareware_textures[index], &subtexture)) {
            size_t cleanup;
            for (cleanup = 0U; cleanup < index; cleanup++) {
                C3D_TexDelete(&rareware_textures[cleanup]);
                rareware_textures_loaded[cleanup] = false;
            }
            return false;
        }
        rareware_textures_loaded[index] = true;
        map_rareware_quad_uv(mesh, index, &subtexture);
    }
    if (!import_packed_texture(asset_pack,
            "converted/runtime/rareware-textures/rare-body.t3x",
            &rareware_body_texture, &rareware_body_subtexture)) {
        for (index = 0U; index < sizeof(paths) / sizeof(paths[0]); ++index) {
            C3D_TexDelete(&rareware_textures[index]);
            rareware_textures_loaded[index] = false;
        }
        return false;
    }
    C3D_TexSetWrap(&rareware_body_texture, GPU_REPEAT, GPU_REPEAT);
    C3D_TexSetFilter(&rareware_body_texture, GPU_LINEAR, GPU_LINEAR);
    rareware_body_texture_loaded = true;
    if (!import_packed_texture(asset_pack,
            "converted/runtime/rareware-textures/rare-front.t3x",
            &rareware_front_texture, &rareware_front_subtexture)) {
        C3D_TexDelete(&rareware_body_texture);
        memset(&rareware_body_texture, 0, sizeof(rareware_body_texture));
        rareware_body_texture_loaded = false;
        for (index = 0U; index < sizeof(paths) / sizeof(paths[0]); ++index) {
            C3D_TexDelete(&rareware_textures[index]);
            rareware_textures_loaded[index] = false;
        }
        return false;
    }
    C3D_TexSetWrap(&rareware_front_texture, GPU_REPEAT, GPU_REPEAT);
    C3D_TexSetFilter(&rareware_front_texture, GPU_LINEAR, GPU_LINEAR);
    rareware_front_texture_loaded = true;
    return true;
}

static float normalize_axis(s16 value)
{
    const float normalized = (float)value / 156.0f;
    const float magnitude = fabsf(normalized);

    if (magnitude < 0.12f) {
        return 0.0f;
    }
    if (normalized < -1.0f) {
        return -1.0f;
    }
    if (normalized > 1.0f) {
        return 1.0f;
    }
    return normalized;
}

static uint32_t map_actions(u32 keys)
{
    uint32_t actions = 0;

    if ((keys & KEY_R) != 0) actions |= GE_PORT_ACTION_FIRE;
    if ((keys & KEY_B) != 0) actions |= GE_PORT_ACTION_USE;
    if ((keys & KEY_Y) != 0) actions |= GE_PORT_ACTION_RELOAD;
    if ((keys & KEY_ZR) != 0) actions |= GE_PORT_ACTION_CROUCH;
    if ((keys & KEY_L) != 0) actions |= GE_PORT_ACTION_AIM;
    if ((keys & KEY_A) != 0) actions |= GE_PORT_ACTION_NEXT_WEAPON;
    if ((keys & KEY_X) != 0) actions |= GE_PORT_ACTION_PREV_WEAPON;
    if ((keys & KEY_START) != 0) actions |= GE_PORT_ACTION_PAUSE;

    return actions;
}

static GePortInput read_input(bool cstick_available)
{
    circlePosition circle = {0};
    circlePosition cstick = {0};
    GePortInput input = {0};

    hidScanInput();
    hidCircleRead(&circle);
    if (cstick_available) {
        hidCstickRead(&cstick);
    }

    input.move_x = normalize_axis(circle.dx);
    input.move_y = normalize_axis(circle.dy);
    input.look_x = normalize_axis(cstick.dx);
    input.look_y = normalize_axis(cstick.dy);
    input.held = map_actions(hidKeysHeld());
    input.pressed = map_actions(hidKeysDown());
    return input;
}

typedef struct RuntimeFrontendEmbeddedTexture {
    Ge3dsSceneTextureSlot slot;
    int32_t model_id;
    uint32_t segmented_address;
    uint8_t image_format;
    uint8_t image_size;
} RuntimeFrontendEmbeddedTexture;

typedef struct RuntimeOriginalFrontend {
    int32_t requested_stage;
    uint32_t axis_held;
    Ge3dsSaveProvider *save_provider;
    const RuntimeGbiMesh *rareware_mesh;
    const RuntimeGbiModel *rareware_front;
    const RuntimeGbiModel *rareware_body;
    GeOriginalPitemModelProvider *logo_models;
    GeDamRoomWorldVertex *logo_source_vertices;
    GeDamRoomDrawBatch *logo_batches;
    size_t logo_vertex_count;
    size_t logo_batch_count;
    int32_t logo_scene_prop;
    bool logo_ready;
    RuntimeFrontendEmbeddedTexture embedded_textures[
        ORIGINAL_FRONTEND_EMBEDDED_TEXTURE_CAPACITY];
    size_t embedded_texture_count;
    void *wallet_instances[4];
    GeDamRoomWorldVertex *wallet_source_vertices;
    GeDamRoomDrawBatch *wallet_batches;
    size_t wallet_vertex_count;
    size_t wallet_batch_count;
    int32_t wallet_scene_key;
    bool wallet_ready;
    GeOriginalFrontendWalletBounds wallet_bounds[4];
    float wallet_centers[4][2];
    bool wallet_bounds_ready;
    C3D_Tex folder_background_texture;
    bool folder_background_ready;
    C3D_Tex gunbarrel_sight_texture;
    bool gunbarrel_sight_texture_loaded;
    GeOriginalGunbarrelState gunbarrel_state;
    GeOriginalGunbarrelFrame gunbarrel_frame;
    GeOriginalGunbarrelBloodFrame gunbarrel_blood;
    GeOriginalGunbarrelBond *gunbarrel_bond;
    GeOriginalGunbarrelBondScene gunbarrel_bond_scene;
    GeOriginalFrontendCast cast;
    GeOriginalFrontendCastFrame cast_frame;
    GeOriginalFrontendCastModel *cast_model;
    GeOriginalFrontendCastModelScene cast_scene;
    GeDamRoomDrawBatch *cast_projected_batches;
    size_t cast_projected_batch_capacity;
    size_t cast_projected_vertex_count;
    int32_t previous_menu;
    uint8_t gunbarrel_started;
    uint8_t cast_started;
    uint8_t cast_terminal;
    GeAssetPack *asset_pack;
    uint8_t *ramrom_data;
    size_t ramrom_data_size;
    GeOriginalRamromReplay ramrom_replay;
    GeOriginalRamromBlock ramrom_block;
    uint8_t ramrom_active;
    uint8_t ramrom_block_ready;
    uint8_t ramrom_return_to_title;
    int32_t requested_music_track;
    uint32_t music_request_generation;
    uint32_t applied_music_request_generation;
    size_t gunbarrel_hole_vertex_count;
    bool gunbarrel_sight_rect_visible;
    C3D_Tex gunbarrel_blood_texture;
    bool gunbarrel_blood_texture_loaded;
    uint32_t gunbarrel_blood_texture_generation;
} RuntimeOriginalFrontend;

static void close_original_frontend_model(RuntimeOriginalFrontend *runtime)
{
    size_t embedded_index;
    if (runtime == NULL) return;
    if (runtime->folder_background_ready)
        C3D_TexDelete(&runtime->folder_background_texture);
    if (runtime->gunbarrel_sight_texture_loaded)
        C3D_TexDelete(&runtime->gunbarrel_sight_texture);
    if (runtime->gunbarrel_blood_texture_loaded)
        C3D_TexDelete(&runtime->gunbarrel_blood_texture);
    for (embedded_index = 0U;
            embedded_index < runtime->embedded_texture_count;
            ++embedded_index) {
        if (runtime->embedded_textures[embedded_index].slot.loaded)
            C3D_TexDelete(
                &runtime->embedded_textures[embedded_index].slot.texture);
    }
    ge_original_frontend_cast_model_destroy(runtime->cast_model);
    ge_original_gunbarrel_bond_destroy(runtime->gunbarrel_bond);
    ge_original_pitem_model_provider_destroy(runtime->logo_models);
    free(runtime->logo_source_vertices);
    free(runtime->logo_batches);
    free(runtime->wallet_source_vertices);
    free(runtime->wallet_batches);
    free(runtime->cast_projected_batches);
    runtime->logo_models = NULL;
    runtime->logo_source_vertices = NULL;
    runtime->logo_batches = NULL;
    runtime->logo_vertex_count = 0U;
    runtime->logo_batch_count = 0U;
    runtime->logo_scene_prop = -1;
    runtime->logo_ready = false;
    memset(runtime->embedded_textures, 0,
           sizeof(runtime->embedded_textures));
    runtime->embedded_texture_count = 0U;
    memset(runtime->wallet_instances, 0,
           sizeof(runtime->wallet_instances));
    runtime->wallet_source_vertices = NULL;
    runtime->wallet_batches = NULL;
    runtime->wallet_vertex_count = 0U;
    runtime->wallet_batch_count = 0U;
    runtime->wallet_scene_key = INT32_MIN;
    runtime->wallet_ready = false;
    memset(runtime->wallet_bounds, 0, sizeof(runtime->wallet_bounds));
    memset(runtime->wallet_centers, 0, sizeof(runtime->wallet_centers));
    runtime->wallet_bounds_ready = false;
    memset(&runtime->folder_background_texture, 0,
           sizeof(runtime->folder_background_texture));
    runtime->folder_background_ready = false;
    memset(&runtime->gunbarrel_sight_texture, 0,
           sizeof(runtime->gunbarrel_sight_texture));
    runtime->gunbarrel_sight_texture_loaded = false;
    memset(&runtime->gunbarrel_blood_texture, 0,
           sizeof(runtime->gunbarrel_blood_texture));
    runtime->gunbarrel_blood_texture_loaded = false;
    runtime->gunbarrel_blood_texture_generation = 0U;
    runtime->gunbarrel_bond = NULL;
    memset(&runtime->gunbarrel_bond_scene, 0,
           sizeof(runtime->gunbarrel_bond_scene));
    runtime->cast_model = NULL;
    memset(&runtime->cast_scene, 0, sizeof(runtime->cast_scene));
    runtime->cast_projected_batches = NULL;
    runtime->asset_pack = NULL;
    runtime->cast_projected_batch_capacity = 0U;
    runtime->cast_projected_vertex_count = 0U;
    runtime->cast_started = 0U;
    runtime->cast_terminal = 0U;
}

static void close_original_frontend_ramrom(RuntimeOriginalFrontend *runtime)
{
    if (runtime == NULL) return;
    free(runtime->ramrom_data);
    runtime->ramrom_data = NULL;
    runtime->ramrom_data_size = 0U;
    memset(&runtime->ramrom_replay, 0, sizeof(runtime->ramrom_replay));
    memset(&runtime->ramrom_block, 0, sizeof(runtime->ramrom_block));
    runtime->ramrom_active = 0U;
    runtime->ramrom_block_ready = 0U;
    runtime->ramrom_return_to_title = 0U;
}

static size_t original_frontend_morton8(size_t x, size_t y)
{
    return (x & 1U) | ((y & 1U) << 1U)
        | ((x & 2U) << 1U) | ((y & 2U) << 2U)
        | ((x & 4U) << 2U) | ((y & 4U) << 3U);
}

static size_t original_frontend_swizzled_offset(
    size_t width, size_t x, size_t y)
{
    return (y & ~(size_t)7U) * width
        + (x & ~(size_t)7U) * 8U
        + original_frontend_morton8(x & 7U, y & 7U);
}

static size_t original_frontend_texture_dimension(size_t value)
{
    size_t dimension = 8U;
    while (dimension < value && dimension < 1024U) dimension <<= 1U;
    return dimension >= value ? dimension : 0U;
}

static bool original_frontend_decode_embedded_pixel(
    const GeOriginalPitemEmbeddedTexture *source, uint8_t format,
    uint8_t size, size_t pixel, uint16_t *rgba4)
{
    enum {
        N64_IMAGE_FORMAT_RGBA = 0,
        N64_IMAGE_FORMAT_IA = 3,
        N64_IMAGE_FORMAT_I = 4,
        N64_IMAGE_SIZE_4B = 0,
        N64_IMAGE_SIZE_8B = 1,
        N64_IMAGE_SIZE_16B = 2,
        N64_IMAGE_SIZE_32B = 3,
    };
    const uint8_t *bytes;
    uint8_t red = 0U, green = 0U, blue = 0U, alpha = 0xffU;
    size_t required;
    if (source == NULL || source->pixels == NULL || rgba4 == NULL)
        return false;
    bytes = source->pixels;
    switch (size) {
    case N64_IMAGE_SIZE_4B:
        required = pixel / 2U + 1U;
        break;
    case N64_IMAGE_SIZE_8B:
        required = pixel + 1U;
        break;
    case N64_IMAGE_SIZE_16B:
        required = pixel * 2U + 2U;
        break;
    case N64_IMAGE_SIZE_32B:
        required = pixel * 4U + 4U;
        break;
    default:
        return false;
    }
    if (required > source->available_bytes) return false;
    if (format == N64_IMAGE_FORMAT_RGBA && size == N64_IMAGE_SIZE_16B) {
        const uint16_t packed = (uint16_t)(
            (uint16_t)bytes[pixel * 2U] << 8U
            | bytes[pixel * 2U + 1U]);
        red = (uint8_t)(((packed >> 11U) & 0x1fU) * 255U / 31U);
        green = (uint8_t)(((packed >> 6U) & 0x1fU) * 255U / 31U);
        blue = (uint8_t)(((packed >> 1U) & 0x1fU) * 255U / 31U);
        alpha = (packed & 1U) != 0U ? 0xffU : 0U;
    } else if (format == N64_IMAGE_FORMAT_RGBA
            && size == N64_IMAGE_SIZE_32B) {
        red = bytes[pixel * 4U];
        green = bytes[pixel * 4U + 1U];
        blue = bytes[pixel * 4U + 2U];
        alpha = bytes[pixel * 4U + 3U];
    } else if (format == N64_IMAGE_FORMAT_I && size == N64_IMAGE_SIZE_4B) {
        const uint8_t packed = bytes[pixel / 2U];
        const uint8_t intensity = (pixel & 1U) == 0U
            ? packed >> 4U : packed & 0x0fU;
        red = green = blue = alpha = (uint8_t)(intensity * 0x11U);
    } else if (format == N64_IMAGE_FORMAT_I && size == N64_IMAGE_SIZE_8B) {
        red = green = blue = alpha = bytes[pixel];
    } else if (format == N64_IMAGE_FORMAT_IA && size == N64_IMAGE_SIZE_4B) {
        const uint8_t packed = bytes[pixel / 2U];
        const uint8_t value = (pixel & 1U) == 0U
            ? packed >> 4U : packed & 0x0fU;
        red = green = blue = (uint8_t)(((value >> 1U) & 7U) * 255U / 7U);
        alpha = (value & 1U) != 0U ? 0xffU : 0U;
    } else if (format == N64_IMAGE_FORMAT_IA && size == N64_IMAGE_SIZE_8B) {
        const uint8_t value = bytes[pixel];
        red = green = blue = (uint8_t)((value >> 4U) * 0x11U);
        alpha = (uint8_t)((value & 0x0fU) * 0x11U);
    } else if (format == N64_IMAGE_FORMAT_IA
            && size == N64_IMAGE_SIZE_16B) {
        red = green = blue = bytes[pixel * 2U];
        alpha = bytes[pixel * 2U + 1U];
    } else {
        return false;
    }
    *rgba4 = (uint16_t)(((uint16_t)(red >> 4U) << 12U)
        | ((uint16_t)(green >> 4U) << 8U)
        | ((uint16_t)(blue >> 4U) << 4U)
        | (uint16_t)(alpha >> 4U));
    return true;
}

static const Ge3dsSceneTextureSlot *
original_frontend_ensure_embedded_texture(
    RuntimeOriginalFrontend *runtime, int32_t model_id,
    const GePicaMaterial *material)
{
    GeOriginalPitemEmbeddedTexture source;
    RuntimeFrontendEmbeddedTexture *entry;
    size_t index;
    size_t texture_width, texture_height;
    u32 image_bytes = 0U;
    uint16_t *image;
    uint8_t image_size;
    if (runtime == NULL || runtime->logo_models == NULL || material == NULL
            || material->texture_source
                != GE_PICA_TEXTURE_SOURCE_GBI_IMAGE)
        return NULL;
    for (index = 0U; index < runtime->embedded_texture_count; ++index) {
        entry = &runtime->embedded_textures[index];
        if (entry->model_id == model_id
                && entry->segmented_address
                    == material->texture_image_address)
            return &entry->slot;
    }
    if (runtime->embedded_texture_count
            >= ORIGINAL_FRONTEND_EMBEDDED_TEXTURE_CAPACITY
            || !ge_original_pitem_model_embedded_texture(
                runtime->logo_models, model_id,
                material->texture_image_address, &source)) return NULL;
    texture_width = original_frontend_texture_dimension(source.width);
    texture_height = original_frontend_texture_dimension(source.height);
    image_size = source.render_depth;
    if (texture_width == 0U || texture_height == 0U) return NULL;
    entry = &runtime->embedded_textures[runtime->embedded_texture_count];
    memset(entry, 0, sizeof(*entry));
    if (!C3D_TexInit(&entry->slot.texture,
            (u16)texture_width, (u16)texture_height, GPU_RGBA4))
        return NULL;
    image = C3D_Tex2DGetImagePtr(
        &entry->slot.texture, 0, &image_bytes);
    if (image == NULL
            || image_bytes != texture_width * texture_height * 2U) {
        C3D_TexDelete(&entry->slot.texture);
        memset(entry, 0, sizeof(*entry));
        return NULL;
    }
    memset(image, 0, image_bytes);
    for (index = 0U; index < (size_t)source.width * source.height;
            ++index) {
        uint16_t rgba4;
        const size_t x = index % source.width;
        const size_t y = index / source.width;
        if (!original_frontend_decode_embedded_pixel(
                &source, material->texture_image_format,
                image_size, index, &rgba4)) {
            C3D_TexDelete(&entry->slot.texture);
            memset(entry, 0, sizeof(*entry));
            return NULL;
        }
        image[original_frontend_swizzled_offset(
            texture_width, x, y)] = rgba4;
    }
    GSPGPU_FlushDataCache(image, image_bytes);
    C3D_TexSetFilter(&entry->slot.texture, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(&entry->slot.texture,
        GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    entry->slot.subtexture = (Tex3DS_SubTexture){
        source.width, source.height, 0.0f, 1.0f,
        (float)source.width / (float)texture_width,
        1.0f - (float)source.height / (float)texture_height,
    };
    entry->slot.width = source.width;
    entry->slot.height = source.height;
    entry->slot.loaded = 1U;
    entry->slot.owned = 1U;
    entry->model_id = model_id;
    entry->segmented_address = material->texture_image_address;
    entry->image_format = material->texture_image_format;
    entry->image_size = image_size;
    ++runtime->embedded_texture_count;
    return &entry->slot;
}

static const Ge3dsSceneTextureSlot *original_frontend_batch_texture(
    const RuntimeOriginalFrontend *runtime, int32_t model_id,
    const GeDamRoomDrawBatch *batch)
{
    size_t index;
    if (runtime == NULL || batch == NULL) return NULL;
    if (batch->material.texture_source
            == GE_PICA_TEXTURE_SOURCE_GBI_IMAGE) {
        for (index = 0U; index < runtime->embedded_texture_count; ++index) {
            const RuntimeFrontendEmbeddedTexture *entry =
                &runtime->embedded_textures[index];
            if (entry->model_id == model_id
                    && entry->segmented_address
                        == batch->material.texture_image_address)
                return &entry->slot;
        }
        return NULL;
    }
    return ge_3ds_scene_textures_find(
        &dam_scene_textures, batch->texture.texture_id);
}

static bool initialize_original_frontend_folder_background(
    RuntimeOriginalFrontend *runtime, GeAssetPack *asset_pack,
    Vertex *destination)
{
    static const char source_path[] =
        "ge007.u.2A4D50.usedby7F008DE4.bin";
    const GeAssetPackEntry *entry;
    uint8_t *compressed = NULL;
    uint8_t *image;
    size_t image_size = 0U;
    size_t source_cursor = 10U;
    size_t output_cursor = 0U;
    const size_t width = 440U;
    const size_t height = 299U;
    bool ready = false;
    if (runtime == NULL || asset_pack == NULL || destination == NULL)
        return false;
    {
        Tex3DS_SubTexture subtexture;
        if (import_packed_texture(asset_pack,
                "converted/frontend/folder-background.t3x",
                &runtime->folder_background_texture, &subtexture)) {
            float top_left_u, top_left_v;
            float top_right_u, top_right_v;
            float bottom_left_u, bottom_left_v;
            float bottom_right_u, bottom_right_v;
            const float left = 40.0f;
            const float right = 360.0f;
            const float top = 240.0f - 16.0f * (240.0f / 330.0f);
            const float bottom = 240.0f - 315.0f * (240.0f / 330.0f);
            Tex3DS_SubTextureTopLeft(
                &subtexture, &top_left_u, &top_left_v);
            Tex3DS_SubTextureTopRight(
                &subtexture, &top_right_u, &top_right_v);
            Tex3DS_SubTextureBottomLeft(
                &subtexture, &bottom_left_u, &bottom_left_v);
            Tex3DS_SubTextureBottomRight(
                &subtexture, &bottom_right_u, &bottom_right_v);
            destination[0] = (Vertex){left, top, 0.5f,
                top_left_u, top_left_v, 1, 1, 1, 1};
            destination[1] = (Vertex){right, top, 0.5f,
                top_right_u, top_right_v, 1, 1, 1, 1};
            destination[2] = (Vertex){right, bottom, 0.5f,
                bottom_right_u, bottom_right_v, 1, 1, 1, 1};
            destination[3] = destination[0];
            destination[4] = destination[2];
            destination[5] = (Vertex){left, bottom, 0.5f,
                bottom_left_u, bottom_left_v, 1, 1, 1, 1};
            GSPGPU_FlushDataCache(destination, 6U * sizeof(*destination));
            C3D_TexSetFilter(&runtime->folder_background_texture,
                             GPU_NEAREST, GPU_NEAREST);
            C3D_TexSetWrap(&runtime->folder_background_texture,
                           GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
            runtime->folder_background_ready = true;
            printf("Frontend folder backdrop: exact ROM T3X\n");
            return true;
        }
    }
    entry = ge_asset_pack_find(asset_pack, source_path);
    if (entry == NULL || entry->data_size < 12U
            || entry->data_size > SIZE_MAX) return false;
    compressed = malloc((size_t)entry->data_size);
    if (compressed == NULL
            || ge_asset_pack_read(asset_pack, source_path, compressed,
                (size_t)entry->data_size, NULL) != GE_ASSET_PACK_OK
            || ((size_t)compressed[0] << 8U | compressed[1]) != width
            || ((size_t)compressed[2] << 8U | compressed[3]) != height
            || !C3D_TexInit(&runtime->folder_background_texture,
                            512U, 512U, GPU_RGBA4))
        goto done;
    image = C3D_Tex2DGetImagePtr(
        &runtime->folder_background_texture, 0, (u32 *)&image_size);
    if (image == NULL || image_size != 512U * 512U * 2U) goto done;
    memset(image, 0, image_size);
    while (output_cursor < width * height) {
        size_t count;
        uint8_t value;
        uint8_t packed;
        if (source_cursor + 2U > (size_t)entry->data_size) goto done;
        count = compressed[source_cursor++];
        value = compressed[source_cursor++];
        if (count == 0U || count > width * height - output_cursor)
            goto done;
        while (count-- != 0U) {
            const size_t x = output_cursor % width;
            const size_t y = output_cursor / width;
            const unsigned primitive = 20U
                + (unsigned)(30U * y / (height - 1U));
            const unsigned combined = (unsigned)value * 20U / 255U
                + primitive * 235U / 255U;
            const size_t offset = original_frontend_swizzled_offset(
                512U, x, y) * 2U;
            packed = (uint8_t)(((combined > 255U ? 255U : combined)
                >> 4U) * 0x11U);
            image[offset] = packed;
            image[offset + 1U] = packed;
            ++output_cursor;
        }
    }
    GSPGPU_FlushDataCache(image, image_size);
    C3D_TexSetFilter(&runtime->folder_background_texture,
                     GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&runtime->folder_background_texture,
                   GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    {
        const float left = 40.0f;
        const float right = 360.0f;
        const float top = 240.0f - 16.0f * (240.0f / 330.0f);
        const float bottom = 240.0f - 315.0f * (240.0f / 330.0f);
        const float u1 = 440.0f / 512.0f;
        const float v1 = 299.0f / 512.0f;
        /* Direct PICA texture memory is bottom-addressed. Match the font
         * atlas convention: source row zero is sampled at v=1. */
        destination[0] = (Vertex){left, top, 0.5f, 0.0f, 1.0f,
            1.0f, 1.0f, 1.0f, 1.0f};
        destination[1] = (Vertex){right, top, 0.5f, u1, 1.0f,
            1.0f, 1.0f, 1.0f, 1.0f};
        destination[2] = (Vertex){right, bottom, 0.5f, u1, 1.0f - v1,
            1.0f, 1.0f, 1.0f, 1.0f};
        destination[3] = destination[0];
        destination[4] = destination[2];
        destination[5] = (Vertex){left, bottom, 0.5f, 0.0f, 1.0f - v1,
            1.0f, 1.0f, 1.0f, 1.0f};
        GSPGPU_FlushDataCache(destination, 6U * sizeof(*destination));
    }
    runtime->folder_background_ready = true;
    printf("Frontend folder backdrop: %lux%lu exact ROM I8\n",
        (unsigned long)width, (unsigned long)height);
    ready = true;

done:
    if (!ready && runtime->folder_background_texture.data != NULL) {
        C3D_TexDelete(&runtime->folder_background_texture);
        memset(&runtime->folder_background_texture, 0,
               sizeof(runtime->folder_background_texture));
    }
    free(compressed);
    return ready;
}

static bool initialize_original_frontend_gunbarrel_sight(
    RuntimeOriginalFrontend *runtime, GeAssetPack *asset_pack)
{
    static const char source_path[] =
        "ge007.u.2A4D50.usedby7F008DE4.bin";
    const GeAssetPackEntry *entry;
    uint8_t *compressed = NULL;
    uint8_t *image;
    u32 image_size = 0U;
    size_t source_cursor = 10U;
    size_t output_cursor = 0U;
    const size_t width = 440U;
    const size_t height = 299U;
    bool ready = false;

    if (runtime == NULL || asset_pack == NULL
            || (entry = ge_asset_pack_find(asset_pack, source_path)) == NULL
            || entry->data_size < 12U || entry->data_size > SIZE_MAX)
        return false;
    compressed = malloc((size_t)entry->data_size);
    if (compressed == NULL
            || ge_asset_pack_read(asset_pack, source_path, compressed,
                (size_t)entry->data_size, NULL) != GE_ASSET_PACK_OK
            || ((size_t)compressed[0] << 8U | compressed[1]) != width
            || ((size_t)compressed[2] << 8U | compressed[3]) != height
            || !C3D_TexInit(&runtime->gunbarrel_sight_texture,
                            512U, 512U, GPU_RGBA4))
        goto done;
    image = C3D_Tex2DGetImagePtr(
        &runtime->gunbarrel_sight_texture, 0, &image_size);
    if (image == NULL || image_size != 512U * 512U * 2U) goto done;
    memset(image, 0, image_size);
    while (output_cursor < width * height) {
        size_t count;
        uint8_t value;
        uint8_t packed;
        if (source_cursor + 2U > (size_t)entry->data_size) goto done;
        count = compressed[source_cursor];
        value = compressed[source_cursor + 1U];
        packed = (uint8_t)((value >> 4U) * 0x11U);
        source_cursor += 2U;
        if (count == 0U || count > width * height - output_cursor)
            goto done;
        while (count-- != 0U) {
            const size_t x = output_cursor % width;
            const size_t y = output_cursor / width;
            const size_t offset = original_frontend_swizzled_offset(
                512U, x, y) * 2U;
            image[offset] = packed;
            image[offset + 1U] = packed;
            ++output_cursor;
        }
    }
    GSPGPU_FlushDataCache(image, image_size);
    C3D_TexSetFilter(&runtime->gunbarrel_sight_texture,
                     GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&runtime->gunbarrel_sight_texture,
                   GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    runtime->gunbarrel_sight_texture_loaded = true;
    ready = true;

done:
    if (!ready && runtime->gunbarrel_sight_texture.data != NULL) {
        C3D_TexDelete(&runtime->gunbarrel_sight_texture);
        memset(&runtime->gunbarrel_sight_texture, 0,
               sizeof(runtime->gunbarrel_sight_texture));
    }
    free(compressed);
    return ready;
}

static bool prepare_original_frontend_pitem_scene(
    RuntimeOriginalFrontend *runtime, int32_t prop,
    const GeOriginalFrontendPresentation *presentation,
    GeTextureCache *texture_cache, Vertex *destination);

static bool initialize_original_frontend_model(
    RuntimeOriginalFrontend *runtime, GeAssetPack *asset_pack,
    GeTextureCache *texture_cache, Vertex *destination)
{
    GeOriginalPitemModelStatus provider_status;
    GeOriginalGunbarrelBondStatus gunbarrel_bond_status;
    GeOriginalFrontendCastModelStatus cast_model_status;
    GeOriginalPitemModelScenePart *parts = NULL;
    GeOriginalModelSceneInput *inputs = NULL;
    GeOriginalModelScene *queries = NULL;
    size_t part_count;
    size_t vertex_count = 0U;
    size_t batch_count = 0U;
    size_t part_index;

    if (runtime == NULL || asset_pack == NULL || texture_cache == NULL
            || destination == NULL) return false;
    close_original_frontend_model(runtime);
    runtime->asset_pack = asset_pack;
    (void)initialize_original_frontend_folder_background(
        runtime, asset_pack,
        destination + ORIGINAL_FRONTEND_MODEL_VERTEX_CAPACITY - 6U);
    (void)initialize_original_frontend_gunbarrel_sight(
        runtime, asset_pack);
    runtime->gunbarrel_bond = ge_original_gunbarrel_bond_create(
        asset_pack, &gunbarrel_bond_status);
    if (runtime->gunbarrel_bond == NULL) {
        printf("Could not initialize original gunbarrel Bond: %s\n",
            ge_original_gunbarrel_bond_status_name(
                gunbarrel_bond_status));
        goto fail;
    }
    runtime->cast_model = ge_original_frontend_cast_model_create(
        asset_pack, &cast_model_status);
    if (runtime->cast_model == NULL) {
        printf("Could not initialize original frontend cast: %s\n",
            ge_original_frontend_cast_model_status_name(
                cast_model_status));
        goto fail;
    }
    runtime->logo_models = ge_original_pitem_model_provider_create(
        asset_pack, 4U, 4U, &provider_status);
    if (runtime->logo_models == NULL
            || !ge_original_pitem_model_load(
                runtime->logo_models, PROP_LEGALPAGE)
            || !ge_original_pitem_model_load(
                runtime->logo_models, PROP_NINTENDOLOGO)
            || !ge_original_pitem_model_load(
                runtime->logo_models, PROP_GOLDENEYELOGO)
            || !ge_original_pitem_model_load(
                runtime->logo_models, PROP_WALLETBOND))
        goto fail;
    part_count = ge_original_pitem_model_scene_part_count(
        runtime->logo_models, PROP_GOLDENEYELOGO);
    if (part_count == 0U) goto fail;
    parts = calloc(part_count, sizeof(*parts));
    inputs = calloc(part_count, sizeof(*inputs));
    queries = calloc(part_count, sizeof(*queries));
    if (parts == NULL || inputs == NULL || queries == NULL) goto fail;
    for (part_index = 0U; part_index < part_count; ++part_index) {
        GeOriginalModelSceneStatus query_status;
        size_t diagonal;
        if (!ge_original_pitem_model_scene_part(
                runtime->logo_models, PROP_GOLDENEYELOGO,
                part_index, &parts[part_index])) goto fail;
        inputs[part_index] = (GeOriginalModelSceneInput){
            parts[part_index].blob,
            parts[part_index].blob_size,
            parts[part_index].primary_offset,
            parts[part_index].secondary_offset,
            parts[part_index].segment4_offset,
            0U, 0U, NULL, 0U, {{0}}, {0},
        };
        for (diagonal = 0U; diagonal < 4U; ++diagonal)
            inputs[part_index].matrix[diagonal][diagonal] = 1.0f;
        query_status = ge_original_model_scene_build(
            &inputs[part_index], NULL, &queries[part_index]);
        if (query_status != GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED
                && query_status != GE_ORIGINAL_MODEL_SCENE_OK) goto fail;
        if (queries[part_index].required_vertex_count
                    > ORIGINAL_FRONTEND_MODEL_VERTEX_CAPACITY - vertex_count
                || SIZE_MAX - batch_count
                    < queries[part_index].required_batch_count) goto fail;
        vertex_count += queries[part_index].required_vertex_count;
        batch_count += queries[part_index].required_batch_count;
    }
    runtime->logo_source_vertices = calloc(
        vertex_count, sizeof(*runtime->logo_source_vertices));
    runtime->logo_batches = calloc(
        batch_count, sizeof(*runtime->logo_batches));
    if (runtime->logo_source_vertices == NULL
            || runtime->logo_batches == NULL) goto fail;
    vertex_count = 0U;
    batch_count = 0U;
    for (part_index = 0U; part_index < part_count; ++part_index) {
        const GeDamRoomSceneStorage storage = {
            runtime->logo_source_vertices + vertex_count,
            queries[part_index].required_vertex_count,
            runtime->logo_batches + batch_count,
            queries[part_index].required_batch_count,
        };
        GeOriginalModelScene built;
        size_t batch;
        if (ge_original_model_scene_build(
                &inputs[part_index], &storage, &built)
                != GE_ORIGINAL_MODEL_SCENE_OK) goto fail;
        for (batch = 0U; batch < built.batch_count; ++batch)
            runtime->logo_batches[batch_count + batch].first_vertex +=
                vertex_count;
        vertex_count += built.vertex_count;
        batch_count += built.batch_count;
    }
    for (part_index = 0U; part_index < batch_count; ++part_index) {
        const GeDamRoomDrawBatch *batch =
            &runtime->logo_batches[part_index];
        const uint16_t image_id = batch->texture.texture_id;
        if (batch->material.texture_source
                == GE_PICA_TEXTURE_SOURCE_GBI_IMAGE) {
            if (original_frontend_ensure_embedded_texture(
                    runtime, PROP_GOLDENEYELOGO,
                    &batch->material) == NULL) goto fail;
        } else if (ge_3ds_scene_textures_find(
                &dam_scene_textures, image_id) == NULL) {
            (void)ge_3ds_scene_textures_ensure_image(
                texture_cache, &dam_scene_textures, image_id);
        }
    }
    runtime->logo_vertex_count = vertex_count;
    runtime->logo_batch_count = batch_count;
    runtime->logo_scene_prop = PROP_GOLDENEYELOGO;
    runtime->logo_ready = vertex_count != 0U && batch_count != 0U;
    for (part_index = 0U;
            part_index < sizeof(runtime->wallet_instances)
                / sizeof(runtime->wallet_instances[0]); ++part_index) {
        void *header = NULL;
        float pitem_scale = 0.0f;
        if (!ge_original_pitem_model_resolve_instance(
                runtime->logo_models, PROP_WALLETBOND, &header,
                &runtime->wallet_instances[part_index], &pitem_scale)
                || header == NULL || pitem_scale <= 0.0f)
            goto fail;
    }
    if (runtime->logo_ready) {
        GeOriginalFrontendPresentation presentation = {0};
        presentation.title_light_ambient = 0x96U;
        presentation.title_light_diffuse = 0xffU;
        presentation.title_light_direction[0] = 77;
        presentation.title_light_direction[1] = 77;
        presentation.title_light_direction[2] = 46;
        if (!prepare_original_frontend_pitem_scene(
                runtime, PROP_GOLDENEYELOGO, &presentation,
                texture_cache, destination)) goto fail;
        printf("Frontend GoldenEye logo: %lu parts, %lu triangles\n",
            (unsigned long)part_count,
            (unsigned long)(vertex_count / 3U));
    }
    free(parts);
    free(inputs);
    free(queries);
    return runtime->logo_ready;

fail:
    free(parts);
    free(inputs);
    free(queries);
    close_original_frontend_model(runtime);
    return false;
}

static bool prepare_original_frontend_pitem_scene(
    RuntimeOriginalFrontend *runtime, int32_t prop,
    const GeOriginalFrontendPresentation *presentation,
    GeTextureCache *texture_cache, Vertex *destination)
{
    GeOriginalPitemModelScenePart *parts = NULL;
    GeOriginalModelSceneInput *inputs = NULL;
    GeOriginalModelScene *queries = NULL;
    size_t part_count;
    size_t vertex_count = 0U;
    size_t batch_count = 0U;
    size_t part_index;
    bool built = false;
    if (runtime == NULL || runtime->logo_models == NULL
            || presentation == NULL || texture_cache == NULL
            || destination == NULL) return false;
    if (runtime->logo_scene_prop != prop || !runtime->logo_ready) {
        part_count = ge_original_pitem_model_scene_part_count(
            runtime->logo_models, prop);
        if (part_count == 0U) return false;
        parts = calloc(part_count, sizeof(*parts));
        inputs = calloc(part_count, sizeof(*inputs));
        queries = calloc(part_count, sizeof(*queries));
        if (parts == NULL || inputs == NULL || queries == NULL) goto done;
        for (part_index = 0U; part_index < part_count; ++part_index) {
            GeOriginalModelSceneStatus status;
            size_t diagonal;
            if (!ge_original_pitem_model_scene_part(
                    runtime->logo_models, prop, part_index,
                    &parts[part_index])) goto done;
            inputs[part_index] = (GeOriginalModelSceneInput){
                parts[part_index].blob, parts[part_index].blob_size,
                parts[part_index].primary_offset,
                parts[part_index].secondary_offset,
                parts[part_index].segment4_offset,
                0U, 0U, NULL, 0U, {{0}}, {0},
            };
            for (diagonal = 0U; diagonal < 4U; ++diagonal)
                inputs[part_index].matrix[diagonal][diagonal] = 1.0f;
            status = ge_original_model_scene_build(
                &inputs[part_index], NULL, &queries[part_index]);
            if (status != GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED
                    && status != GE_ORIGINAL_MODEL_SCENE_OK) goto done;
            if (queries[part_index].required_vertex_count
                        > ORIGINAL_FRONTEND_MODEL_VERTEX_CAPACITY - 6U
                            - vertex_count
                    || SIZE_MAX - batch_count
                        < queries[part_index].required_batch_count) goto done;
            vertex_count += queries[part_index].required_vertex_count;
            batch_count += queries[part_index].required_batch_count;
        }
        {
            GeDamRoomWorldVertex *source = calloc(
                vertex_count, sizeof(*source));
            GeDamRoomDrawBatch *batches = calloc(
                batch_count, sizeof(*batches));
            size_t next_vertex = 0U;
            size_t next_batch = 0U;
            if (source == NULL || batches == NULL) {
                free(source);
                free(batches);
                goto done;
            }
            for (part_index = 0U; part_index < part_count; ++part_index) {
                const GeDamRoomSceneStorage storage = {
                    source + next_vertex,
                    queries[part_index].required_vertex_count,
                    batches + next_batch,
                    queries[part_index].required_batch_count,
                };
                GeOriginalModelScene scene;
                size_t batch;
                if (ge_original_model_scene_build(
                        &inputs[part_index], &storage, &scene)
                        != GE_ORIGINAL_MODEL_SCENE_OK) {
                    free(source);
                    free(batches);
                    goto done;
                }
                for (batch = 0U; batch < scene.batch_count; ++batch)
                    batches[next_batch + batch].first_vertex += next_vertex;
                next_vertex += scene.vertex_count;
                next_batch += scene.batch_count;
            }
            free(runtime->logo_source_vertices);
            free(runtime->logo_batches);
            runtime->logo_source_vertices = source;
            runtime->logo_batches = batches;
            runtime->logo_vertex_count = vertex_count;
            runtime->logo_batch_count = batch_count;
            runtime->logo_scene_prop = prop;
            runtime->logo_ready = vertex_count != 0U && batch_count != 0U;
            for (part_index = 0U; part_index < batch_count; ++part_index) {
                const GeDamRoomDrawBatch *batch = &batches[part_index];
                const uint16_t image_id = batch->texture.texture_id;
                if (batch->material.texture_source
                        == GE_PICA_TEXTURE_SOURCE_GBI_IMAGE) {
                    if (original_frontend_ensure_embedded_texture(
                            runtime, prop, &batch->material) == NULL)
                        goto done;
                } else if (ge_3ds_scene_textures_find(
                        &dam_scene_textures, image_id) == NULL) {
                    (void)ge_3ds_scene_textures_ensure_image(
                        texture_cache, &dam_scene_textures, image_id);
                }
            }
        }
    }
    if (!runtime->logo_ready) goto done;
    {
        const float cosine = cosf(presentation->nintendo_rotation_radians);
        const float sine = sinf(presentation->nintendo_rotation_radians);
        for (part_index = 0U; part_index < runtime->logo_vertex_count;
                ++part_index) {
            const GeDamRoomWorldVertex *source =
                &runtime->logo_source_vertices[part_index];
            float x = source->world[0];
            const float y = source->world[1];
            float z = source->world[2];
            float red = (float)source->source.red / 255.0f;
            float green = (float)source->source.green / 255.0f;
            float blue = (float)source->source.blue / 255.0f;
            float scale = 1.0f;
            if (prop == PROP_NINTENDOLOGO) {
                const float rotated_x = x * cosine + z * sine;
                z = -x * sine + z * cosine;
                x = rotated_x;
                scale = 207.8460969f * presentation->nintendo_scale
                    / fmaxf(1.0f,
                        4000.0f - z * presentation->nintendo_scale);
                red = green = blue =
                    (float)presentation->nintendo_ambient / 255.0f;
            } else if (prop == PROP_LEGALPAGE) {
                /* constructor_menu00_legalscreen uses the authored model at
                 * unit scale and the standard 60-degree camera at z=4000. */
                scale = 207.8460969f / fmaxf(1.0f, 4000.0f - z);
            } else if (prop == PROP_GOLDENEYELOGO) {
                /* constructor_menu04_goldeneyelogo applies a uniform 1.2
                 * model scale to the authored origin under the standard
                 * look-at camera at z=3000.  Bounds fitting/recentering here
                 * changes both the logo framing and its reflection mapping. */
                scale = 207.8460969f * 1.2f
                    / fmaxf(1.0f, 3000.0f - z * 1.2f);
            }
            destination[part_index] = (Vertex){
                200.0f + x * scale,
                120.0f - y * scale,
                0.5f + z * 0.0f, 0.0f, 0.0f,
                red, green, blue,
                (float)source->source.alpha / 255.0f,
            };
        }
        for (part_index = 0U; part_index < runtime->logo_batch_count;
                ++part_index) {
            const GeDamRoomDrawBatch *batch =
                &runtime->logo_batches[part_index];
            const Ge3dsSceneTextureSlot *slot =
                original_frontend_batch_texture(runtime, prop, batch);
            size_t vertex;
            if (slot == NULL) continue;
            for (vertex = batch->first_vertex;
                    vertex < batch->first_vertex + batch->vertex_count;
                    ++vertex) {
                GeTextureUv uv;
                const GeDamRoomWorldVertex *source =
                    &runtime->logo_source_vertices[vertex];
                if ((prop == PROP_GOLDENEYELOGO
                            || prop == PROP_NINTENDOLOGO)
                        && batch->material.lighting_enabled != 0U) {
                    uint8_t ambient_rgb[3];
                    uint8_t diffuse_rgb[3];
                    const uint8_t packed_normal[3] = {
                        source->source.red,
                        source->source.green,
                        source->source.blue,
                    };
                    GeOriginalFrontendGeneratedVertex generated;
                    float tl_u, tl_v, tr_u, tr_v;
                    float bl_u, bl_v, br_u, br_v;
                    float u;
                    float v;
                    float top_u, top_v, bottom_u, bottom_v;
                    /* guLookAtReflect's +X/+Y axes drive the logo's exact
                     * G_TEXTURE_GEN reflection coordinates. */
                    if (prop == PROP_NINTENDOLOGO) {
                        ambient_rgb[0] = presentation->nintendo_ambient;
                        ambient_rgb[1] = presentation->nintendo_ambient;
                        ambient_rgb[2] = presentation->nintendo_ambient;
                        diffuse_rgb[0] = 0U;
                        diffuse_rgb[1] = 0U;
                        diffuse_rgb[2] = 0U;
                    } else {
                        ambient_rgb[0] = presentation->title_light_ambient;
                        ambient_rgb[1] = presentation->title_light_ambient;
                        ambient_rgb[2] = presentation->title_light_ambient;
                        diffuse_rgb[0] = presentation->title_light_diffuse;
                        diffuse_rgb[1] = presentation->title_light_diffuse;
                        diffuse_rgb[2] = presentation->title_light_diffuse;
                    }
                    if (!ge_original_frontend_generate_lit_vertex(
                            packed_normal, source->source.alpha,
                            prop == PROP_NINTENDOLOGO
                                ? presentation->nintendo_rotation_radians
                                : 0.0f,
                            ambient_rgb, diffuse_rgb,
                            presentation->title_light_direction,
                            &generated))
                        goto done;
                    destination[vertex].r =
                        (float)generated.lit_rgba[0] / 255.0f;
                    destination[vertex].g =
                        (float)generated.lit_rgba[1] / 255.0f;
                    destination[vertex].b =
                        (float)generated.lit_rgba[2] / 255.0f;
                    u = generated.generated_uv[0];
                    v = generated.generated_uv[1];
                    Tex3DS_SubTextureTopLeft(
                        &slot->subtexture, &tl_u, &tl_v);
                    Tex3DS_SubTextureTopRight(
                        &slot->subtexture, &tr_u, &tr_v);
                    Tex3DS_SubTextureBottomLeft(
                        &slot->subtexture, &bl_u, &bl_v);
                    Tex3DS_SubTextureBottomRight(
                        &slot->subtexture, &br_u, &br_v);
                    top_u = tl_u + (tr_u - tl_u) * u;
                    top_v = tl_v + (tr_v - tl_v) * u;
                    bottom_u = bl_u + (br_u - bl_u) * u;
                    bottom_v = bl_v + (br_v - bl_v) * u;
                    destination[vertex].u =
                        top_u + (bottom_u - top_u) * v;
                    destination[vertex].v =
                        top_v + (bottom_v - top_v) * v;
                } else if (ge_3ds_scene_texture_map_uv(
                        slot,
                        source->source.texture_s,
                        source->source.texture_t,
                        &batch->material, &uv) == GE_TEXTURE_UV_OK) {
                    destination[vertex].u = uv.u;
                    destination[vertex].v = uv.v;
                }
            }
        }
    }
    GSPGPU_FlushDataCache(destination,
        runtime->logo_vertex_count * sizeof(*destination));
    built = true;

done:
    free(parts);
    free(inputs);
    free(queries);
    return built;
}

static bool original_frontend_set_wallet_switch(
    RuntimeOriginalFrontend *runtime, size_t instance_index,
    size_t switch_index)
{
    return runtime != NULL
        && instance_index < sizeof(runtime->wallet_instances)
            / sizeof(runtime->wallet_instances[0])
        && ge_original_pitem_model_instance_set_switch(
            runtime->logo_models, runtime->wallet_instances[instance_index],
            switch_index, 1) != 0;
}

static uint32_t original_frontend_read_be32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24U)
        | ((uint32_t)bytes[1] << 16U)
        | ((uint32_t)bytes[2] << 8U)
        | (uint32_t)bytes[3];
}

static void original_frontend_write_be32(uint8_t *bytes, uint32_t value)
{
    bytes[0] = (uint8_t)(value >> 24U);
    bytes[1] = (uint8_t)(value >> 16U);
    bytes[2] = (uint8_t)(value >> 8U);
    bytes[3] = (uint8_t)value;
}

static bool original_frontend_apply_wallet_lut(
    uint8_t *blob, size_t blob_size, uint32_t primary_offset)
{
    /* Exact DL_LUT_WALLETBOND pairs consumed by load_walletbond's unchanged
     * bgApplyDynamicCCRMLUT call. The ROM list is big-endian and ends at the
     * first G_ENDDL command. */
    static const uint32_t lut[][4] = {
        {UINT32_C(0xba001402), UINT32_C(0x00000000),
         UINT32_C(0xba001402), UINT32_C(0x00100000)},
        {UINT32_C(0xb900031d), UINT32_C(0x00502048),
         UINT32_C(0xb900031d), UINT32_C(0x08d02048)},
        {UINT32_C(0xfc127e24), UINT32_C(0xfffff9fc),
         UINT32_C(0xfc127fff), UINT32_C(0xfffff838)},
    };
    size_t offset;
    size_t replacements = 0U;
    if (blob == NULL || primary_offset > blob_size
            || blob_size - primary_offset < 8U) return false;
    for (offset = primary_offset; offset <= blob_size - 8U; offset += 8U) {
        const uint32_t w0 = original_frontend_read_be32(blob + offset);
        const uint32_t w1 = original_frontend_read_be32(blob + offset + 4U);
        size_t index;
        if ((w0 >> 24U) == UINT32_C(0xb8)) break;
        for (index = 0U; index < sizeof(lut) / sizeof(lut[0]); ++index) {
            if (w0 != lut[index][0] || w1 != lut[index][1]) continue;
            original_frontend_write_be32(blob + offset, lut[index][2]);
            original_frontend_write_be32(blob + offset + 4U, lut[index][3]);
            ++replacements;
            break;
        }
    }
    return replacements == sizeof(lut) / sizeof(lut[0]);
}

static bool original_frontend_configure_wallet(
    RuntimeOriginalFrontend *runtime,
    const GeOriginalFrontendSnapshot *snapshot,
    size_t instance_index)
{
    void *instance;
    if (runtime == NULL || snapshot == NULL
            || instance_index >= sizeof(runtime->wallet_instances)
                / sizeof(runtime->wallet_instances[0])) return false;
    instance = runtime->wallet_instances[instance_index];
    if (!ge_original_pitem_model_instance_disable_switches(
            runtime->logo_models, instance)) return false;
    if (snapshot->menu == MENU_FILE_SELECT) {
        /* interface_menu05_fileselect: select_load_bond_picture followed by
         * switch 0xe (photocover) and 0xd (cover). The retail build always
         * selects Brosnan when ALL_BONDS is disabled. */
        return original_frontend_set_wallet_switch(
                runtime, instance_index, SW_BROSNAN)
            && original_frontend_set_wallet_switch(
                runtime, instance_index, SW_BROSNANCOVER)
            && original_frontend_set_wallet_switch(
                runtime, instance_index, SW_PHOTOCOVER)
            && original_frontend_set_wallet_switch(
                runtime, instance_index, SW_COVER);
    }
    if (snapshot->menu == MENU_MODE_SELECT)
        return original_frontend_set_wallet_switch(
                runtime, instance_index, SW_BROSNAN)
            && original_frontend_set_wallet_switch(
                runtime, instance_index, SW_BROSNANCOVER)
            && original_frontend_set_wallet_switch(
                runtime, instance_index, SW_TABS)
            && original_frontend_set_wallet_switch(
                runtime, instance_index, SW_PAPER)
            && original_frontend_set_wallet_switch(
                runtime, instance_index, SW_OHMSS)
            && original_frontend_set_wallet_switch(
                runtime, instance_index, SW_PHOTOBOND)
            && original_frontend_set_wallet_switch(
                runtime, instance_index, SW_EYESONLY);
    if (snapshot->menu == MENU_MISSION_SELECT)
        return original_frontend_set_wallet_switch(
                runtime, instance_index, SW_TABS)
            && original_frontend_set_wallet_switch(
                runtime, instance_index, SW_SLIDES)
            && original_frontend_set_wallet_switch(
                runtime, instance_index, SW_PICS);
    if (snapshot->menu == MENU_DIFFICULTY)
        return original_frontend_set_wallet_switch(
                runtime, instance_index, SW_TABS)
            && original_frontend_set_wallet_switch(
                runtime, instance_index, SW_PAPER)
            && original_frontend_set_wallet_switch(
                runtime, instance_index, SW_OHMSS)
            && original_frontend_set_wallet_switch(
                runtime, instance_index, SW_CONFIDENTIAL);
    if (snapshot->menu == MENU_BRIEFING) {
        bool configured = original_frontend_set_wallet_switch(
                runtime, instance_index, SW_TABS)
            && original_frontend_set_wallet_switch(
                runtime, instance_index, SW_PAPER)
            && original_frontend_set_wallet_switch(
                runtime, instance_index, SW_OHMSS)
            && original_frontend_set_wallet_switch(
                runtime, instance_index, SW_CLASSIFIED);
        if (configured && snapshot->briefing_page == BRIEFING_TITLE) {
            configured = original_frontend_set_wallet_switch(
                    runtime, instance_index, SW_PHOTOBRIEF)
                && snapshot->mission >= SP_LEVEL_DAM
                && snapshot->mission <= SP_LEVEL_EGYPT
                && original_frontend_set_wallet_switch(
                    runtime, instance_index,
                    SW_BRIEF1 + (size_t)snapshot->mission);
        }
        return configured;
    }
    if (snapshot->menu == MENU_MISSION_FAILED
            || snapshot->menu == MENU_MISSION_COMPLETE)
        return original_frontend_set_wallet_switch(
                runtime, instance_index, SW_TABS)
            && original_frontend_set_wallet_switch(
                runtime, instance_index, SW_PAPER)
            && original_frontend_set_wallet_switch(
                runtime, instance_index, SW_OHMSS)
            && original_frontend_set_wallet_switch(
                runtime, instance_index, SW_CLASSIFIED);
    return false;
}

static bool prepare_original_frontend_wallet(
    RuntimeOriginalFrontend *runtime,
    const GeOriginalFrontendSnapshot *snapshot,
    GeTextureCache *texture_cache, Vertex *destination)
{
    typedef struct WalletInstanceRange {
        size_t first_vertex;
        size_t vertex_count;
    } WalletInstanceRange;
    static const float folder_positions[4][2] = {
        {-900.0f, 800.0f}, {1800.0f, 800.0f},
        {-1800.0f, -200.0f}, {900.0f, -200.0f},
    };
    GeOriginalPitemModelScenePart *parts = NULL;
    GeOriginalModelSceneInput *inputs = NULL;
    GeOriginalModelScene *queries = NULL;
    uint8_t *dynamic_mission_blob = NULL;
    GeOriginalPitemModelScenePart mission_picture_part = {0};
    bool mission_picture_part_ready = false;
    WalletInstanceRange ranges[4] = {{0}};
    size_t instance_count;
    size_t part_count = 0U;
    size_t vertex_count = 0U;
    size_t batch_count = 0U;
    size_t instance_index;
    size_t input_index = 0U;
    int32_t scene_key;
    bool result = false;
    if (runtime == NULL || snapshot == NULL || texture_cache == NULL
            || destination == NULL || runtime->logo_models == NULL
            || snapshot->menu == MENU_GOLDENEYE_LOGO) return false;
    scene_key = snapshot->menu * 4096
        + snapshot->briefing_page * 64 + snapshot->mission;
    if (runtime->wallet_ready && runtime->wallet_scene_key == scene_key)
        return true;
    instance_count = snapshot->menu == MENU_FILE_SELECT ? 4U : 1U;
    for (instance_index = 0U; instance_index < instance_count;
            ++instance_index) {
        if (!original_frontend_configure_wallet(
                runtime, snapshot, instance_index)) goto done;
        part_count += ge_original_pitem_model_instance_scene_part_count(
            runtime->logo_models, runtime->wallet_instances[instance_index]);
    }
    if (part_count == 0U) goto done;
    if (snapshot->menu == MENU_MISSION_SELECT) {
        size_t mission_picture_part_index;
        const void *mission_picture_node =
            ge_original_pitem_model_instance_switch_node(
                runtime->logo_models, runtime->wallet_instances[0],
                GFXHIT0_PICS);
        mission_picture_part_ready = mission_picture_node != NULL
            && ge_original_pitem_model_scene_part_for_node(
                runtime->logo_models, PROP_WALLETBOND,
                mission_picture_node, &mission_picture_part_index,
                &mission_picture_part);
        if (!mission_picture_part_ready
                || mission_picture_part.vertex_count != 80U)
            goto done;
    }
    parts = calloc(part_count, sizeof(*parts));
    inputs = calloc(part_count, sizeof(*inputs));
    queries = calloc(part_count, sizeof(*queries));
    if (parts == NULL || inputs == NULL || queries == NULL) goto done;
    for (instance_index = 0U; instance_index < instance_count;
            ++instance_index) {
        size_t local_part_count;
        size_t local_part;
        if (!original_frontend_configure_wallet(
                runtime, snapshot, instance_index)) goto done;
        local_part_count =
            ge_original_pitem_model_instance_scene_part_count(
                runtime->logo_models,
                runtime->wallet_instances[instance_index]);
        for (local_part = 0U; local_part < local_part_count;
                ++local_part, ++input_index) {
            GeOriginalModelSceneStatus query_status;
            size_t diagonal;
            if (!ge_original_pitem_model_instance_scene_part(
                    runtime->logo_models,
                    runtime->wallet_instances[instance_index],
                    local_part, &parts[input_index])) goto done;
            inputs[input_index] = (GeOriginalModelSceneInput){
                parts[input_index].blob, parts[input_index].blob_size,
                parts[input_index].primary_offset,
                parts[input_index].secondary_offset,
                parts[input_index].segment4_offset,
                0U, 0U, NULL, 0U, {{0}}, {0},
            };
            if (snapshot->menu == MENU_MISSION_SELECT
                    && instance_index == 0U
                    && mission_picture_part_ready
                    && parts[input_index].segment4_offset
                        == mission_picture_part.segment4_offset) {
                const size_t picture_vertices =
                    parts[input_index].vertex_count;
                size_t picture_vertex;
                if (dynamic_mission_blob != NULL
                        || parts[input_index].segment4_offset
                            > parts[input_index].blob_size
                        || picture_vertices
                            > (parts[input_index].blob_size
                                - parts[input_index].segment4_offset) / 16U)
                    goto done;
                dynamic_mission_blob = malloc(parts[input_index].blob_size);
                if (dynamic_mission_blob == NULL) goto done;
                memcpy(dynamic_mission_blob, parts[input_index].blob,
                       parts[input_index].blob_size);
                if (!original_frontend_apply_wallet_lut(
                        dynamic_mission_blob,
                        parts[input_index].blob_size,
                        parts[input_index].primary_offset)) goto done;
                for (picture_vertex = 0U;
                        picture_vertex < picture_vertices;
                        ++picture_vertex) {
                    const size_t mission = picture_vertex / 4U;
                    const bool unlocked = mission + 1U
                            < snapshot->line_count
                        && snapshot->lines[mission + 1U].status != 3U;
                    const bool selected = unlocked
                        && (size_t)snapshot->mission == mission;
                    const uint8_t shade = selected ? 0xffU
                        : unlocked ? 0x6eU : 0x0fU;
                    const size_t colour =
                        parts[input_index].segment4_offset
                        + picture_vertex * 16U + 12U;
                    dynamic_mission_blob[colour + 0U] = shade;
                    dynamic_mission_blob[colour + 1U] = shade;
                    dynamic_mission_blob[colour + 2U] = shade;
                    dynamic_mission_blob[colour + 3U] =
                        selected ? 0xf5U : 0xffU;
                }
                inputs[input_index].blob = dynamic_mission_blob;
            }
            for (diagonal = 0U; diagonal < 4U; ++diagonal)
                inputs[input_index].matrix[diagonal][diagonal] = 1.0f;
            query_status = ge_original_model_scene_build(
                &inputs[input_index], NULL, &queries[input_index]);
            if (query_status != GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED
                    && query_status != GE_ORIGINAL_MODEL_SCENE_OK) goto done;
            if (queries[input_index].required_vertex_count
                        > ORIGINAL_FRONTEND_MODEL_VERTEX_CAPACITY - 6U
                            - vertex_count
                    || SIZE_MAX - batch_count
                        < queries[input_index].required_batch_count) goto done;
            vertex_count += queries[input_index].required_vertex_count;
            batch_count += queries[input_index].required_batch_count;
        }
    }
    free(runtime->wallet_source_vertices);
    free(runtime->wallet_batches);
    runtime->wallet_source_vertices = calloc(
        vertex_count, sizeof(*runtime->wallet_source_vertices));
    runtime->wallet_batches = calloc(
        batch_count, sizeof(*runtime->wallet_batches));
    if (runtime->wallet_source_vertices == NULL
            || runtime->wallet_batches == NULL) goto done;
    vertex_count = 0U;
    batch_count = 0U;
    input_index = 0U;
    for (instance_index = 0U; instance_index < instance_count;
            ++instance_index) {
        const size_t local_part_count =
            ge_original_pitem_model_instance_scene_part_count(
                runtime->logo_models,
                runtime->wallet_instances[instance_index]);
        size_t local_part;
        ranges[instance_index].first_vertex = vertex_count;
        for (local_part = 0U; local_part < local_part_count;
                ++local_part, ++input_index) {
            const GeDamRoomSceneStorage storage = {
                runtime->wallet_source_vertices + vertex_count,
                queries[input_index].required_vertex_count,
                runtime->wallet_batches + batch_count,
                queries[input_index].required_batch_count,
            };
            GeOriginalModelScene built;
            size_t batch;
            if (ge_original_model_scene_build(
                    &inputs[input_index], &storage, &built)
                    != GE_ORIGINAL_MODEL_SCENE_OK) goto done;
            for (batch = 0U; batch < built.batch_count; ++batch)
                runtime->wallet_batches[batch_count + batch].first_vertex +=
                    vertex_count;
            vertex_count += built.vertex_count;
            batch_count += built.batch_count;
        }
        ranges[instance_index].vertex_count =
            vertex_count - ranges[instance_index].first_vertex;
    }
    for (instance_index = 0U; instance_index < instance_count;
            ++instance_index) {
        /* FOV_Y_F is 60 degrees. After the original 440x330 viewport is
         * centered/scaled to 320x240, its perspective focal length is
         * 120*cot(30 degrees). */
        const float focal = 207.8460969f;
        size_t vertex;
        for (vertex = ranges[instance_index].first_vertex;
                vertex < ranges[instance_index].first_vertex
                    + ranges[instance_index].vertex_count; ++vertex) {
            const GeDamRoomWorldVertex *source =
                &runtime->wallet_source_vertices[vertex];
            float camera_x;
            float camera_y;
            float camera_z;
            if (instance_count == 4U) {
                /* interface_menu05_fileselect: look at the origin from
                 * (0,0,4000), then translate each authored folder position
                 * and scale its model basis by 0.37. */
                camera_x = folder_positions[instance_index][0]
                    + source->world[0] * 0.37f;
                camera_y = folder_positions[instance_index][1]
                    + source->world[1] * 0.37f;
                camera_z = source->world[2] * 0.37f - 4000.0f;
            } else {
                /* frontSetupMenuBackground: selected folder origin cancels
                 * from the eye/target pair. The camera is 190 units above
                 * and 700 units forward, with the model basis scaled 0.25. */
                const float inverse_length = 1.0f
                    / sqrtf(190.0f * 190.0f + 700.0f * 700.0f);
                const float backward_y = 190.0f * inverse_length;
                const float backward_z = 700.0f * inverse_length;
                const float local_y = source->world[1] * 0.25f - 190.0f;
                const float local_z = source->world[2] * 0.25f - 700.0f;
                camera_x = source->world[0] * 0.25f;
                camera_y = local_y * backward_z
                    - local_z * backward_y;
                camera_z = local_y * backward_y
                    + local_z * backward_z;
            }
            if (!(camera_z < -1.0f)) goto done;
            destination[vertex] = (Vertex){
                200.0f + camera_x * focal / -camera_z,
                120.0f - camera_y * focal / -camera_z,
                0.5f, 0.0f, 0.0f,
                (float)source->source.red / 255.0f,
                (float)source->source.green / 255.0f,
                (float)source->source.blue / 255.0f,
                (float)source->source.alpha / 255.0f,
            };
        }
        if (instance_count == 4U) {
            float left = FLT_MAX;
            float top = FLT_MAX;
            float right = -FLT_MAX;
            float bottom = -FLT_MAX;
            for (vertex = ranges[instance_index].first_vertex;
                    vertex < ranges[instance_index].first_vertex
                        + ranges[instance_index].vertex_count; ++vertex) {
                if (destination[vertex].x < left)
                    left = destination[vertex].x;
                if (destination[vertex].x > right)
                    right = destination[vertex].x;
                if (destination[vertex].y < top)
                    top = destination[vertex].y;
                if (destination[vertex].y > bottom)
                    bottom = destination[vertex].y;
            }
            if (!ge_original_frontend_wallet_bounds_from_top_screen(
                    left, top, right, bottom,
                    &runtime->wallet_bounds[instance_index])) goto done;
            runtime->wallet_centers[instance_index][0] =
                (200.0f + folder_positions[instance_index][0]
                    * focal / 4000.0f - 40.0f) * (440.0f / 320.0f);
            runtime->wallet_centers[instance_index][1] =
                (120.0f - folder_positions[instance_index][1]
                    * focal / 4000.0f) * (330.0f / 240.0f);
        }
    }
    for (instance_index = 0U; instance_index < batch_count;
            ++instance_index) {
        const GeDamRoomDrawBatch *batch =
            &runtime->wallet_batches[instance_index];
        const uint16_t image_id = batch->texture.texture_id;
        const Ge3dsSceneTextureSlot *slot;
        size_t vertex;
        if (ge_3ds_scene_textures_find(
                &dam_scene_textures, image_id) == NULL)
            (void)ge_3ds_scene_textures_ensure_image(
                texture_cache, &dam_scene_textures, image_id);
        slot = ge_3ds_scene_textures_find(
            &dam_scene_textures, image_id);
        if (slot == NULL) continue;
        for (vertex = batch->first_vertex;
                vertex < batch->first_vertex + batch->vertex_count;
                ++vertex) {
            GeTextureUv uv;
            if (ge_3ds_scene_texture_map_uv(
                    slot,
                    runtime->wallet_source_vertices[vertex]
                        .source.texture_s,
                    runtime->wallet_source_vertices[vertex]
                        .source.texture_t,
                    &batch->material, &uv) == GE_TEXTURE_UV_OK) {
                destination[vertex].u = uv.u;
                destination[vertex].v = uv.v;
            }
        }
    }
    runtime->wallet_vertex_count = vertex_count;
    runtime->wallet_batch_count = batch_count;
    runtime->wallet_scene_key = scene_key;
    runtime->wallet_ready = vertex_count != 0U && batch_count != 0U;
    runtime->wallet_bounds_ready = runtime->wallet_ready
        && instance_count == 4U;
    if (runtime->wallet_ready) {
        GSPGPU_FlushDataCache(destination,
            vertex_count * sizeof(*destination));
        printf("Frontend wallet page %ld: %lu parts, %lu triangles\n",
            (long)snapshot->menu, (unsigned long)part_count,
            (unsigned long)(vertex_count / 3U));
    }
    result = runtime->wallet_ready;

done:
    free(dynamic_mission_blob);
    free(parts);
    free(inputs);
    free(queries);
    if (!result) {
        runtime->wallet_ready = false;
        runtime->wallet_bounds_ready = false;
        runtime->wallet_scene_key = INT32_MIN;
    }
    return result;
}

static int original_frontend_highest_difficulty(void *context,
                                                int32_t mission)
{
    const RuntimeOriginalFrontend *runtime = context;
    int32_t difficulty;
    int32_t maximum;
    if (runtime == NULL || runtime->save_provider == NULL) return -1;
    maximum = ge_3ds_save_provider_007_unlocked(runtime->save_provider)
        ? DIFFICULTY_007 : DIFFICULTY_00;
    for (difficulty = maximum;
            difficulty >= DIFFICULTY_AGENT; --difficulty)
        if (ge_3ds_save_provider_stage_status(
                runtime->save_provider, mission, difficulty)
                != STAGESTATUS_LOCKED)
            return difficulty;
    return -1;
}

static const char *const original_frontend_save_paths[MAX_FOLDER_COUNT] = {
    "sdmc:/3ds/goldeneye-3ds/goldeneye.sav",
    "sdmc:/3ds/goldeneye-3ds/goldeneye-2.sav",
    "sdmc:/3ds/goldeneye-3ds/goldeneye-3.sav",
    "sdmc:/3ds/goldeneye-3ds/goldeneye-4.sav",
};

static int original_frontend_select_folder(void *context, int32_t folder)
{
    RuntimeOriginalFrontend *runtime = context;
    if (runtime == NULL || runtime->save_provider == NULL
            || folder < FOLDER1 || folder >= MAX_FOLDER_COUNT) return 0;
    return ge_3ds_save_provider_select(
        runtime->save_provider, original_frontend_save_paths[folder], folder)
        == GE_3DS_SAVE_PROVIDER_OK;
}

static int original_frontend_folder_has_progress(void *context,
                                                  int32_t folder)
{
    RuntimeOriginalFrontend *runtime = context;
    Ge3dsSaveProvider candidate = {0};
    Ge3dsSaveProvider *provider = &candidate;
    int32_t mission = 0;
    int32_t difficulty = 0;
    int result;
    if (runtime == NULL || runtime->save_provider == NULL
            || folder < FOLDER1 || folder >= MAX_FOLDER_COUNT) return 0;
    if (runtime->save_provider->ready
            && runtime->save_provider->folder == folder)
        provider = runtime->save_provider;
    else if (ge_3ds_save_provider_init(
            &candidate, original_frontend_save_paths[folder], folder)
            != GE_3DS_SAVE_PROVIDER_OK)
        return 0;
    result = ge_3ds_save_provider_highest_completed(
        provider, &mission, &difficulty);
    if (provider == &candidate) ge_3ds_save_provider_close(&candidate);
    return result;
}

static int original_frontend_folder_summary(void *context, int32_t folder,
                                            int32_t *mission,
                                            int32_t *difficulty)
{
    RuntimeOriginalFrontend *runtime = context;
    Ge3dsSaveProvider candidate = {0};
    Ge3dsSaveProvider *provider = &candidate;
    int result;
    if (runtime == NULL || runtime->save_provider == NULL
            || mission == NULL || difficulty == NULL
            || folder < FOLDER1 || folder >= MAX_FOLDER_COUNT) return 0;
    if (runtime->save_provider->ready
            && runtime->save_provider->folder == folder)
        provider = runtime->save_provider;
    else if (ge_3ds_save_provider_init(
            &candidate, original_frontend_save_paths[folder], folder)
            != GE_3DS_SAVE_PROVIDER_OK)
        return 0;
    result = ge_3ds_save_provider_highest_completed(
        provider, mission, difficulty);
    if (provider == &candidate) ge_3ds_save_provider_close(&candidate);
    return result;
}

static int original_frontend_copy_folder_to_first_free(
    void *context, int32_t folder)
{
    RuntimeOriginalFrontend *runtime = context;
    Ge3dsSaveProvider source = {0};
    Ge3dsSaveProvider destination = {0};
    Ge3dsSaveProvider *source_provider = &source;
    int32_t destination_folder;
    int result = 0;
    if (runtime == NULL || runtime->save_provider == NULL
            || folder < FOLDER1 || folder >= MAX_FOLDER_COUNT) return 0;
    if (runtime->save_provider->ready
            && runtime->save_provider->folder == folder)
        source_provider = runtime->save_provider;
    else if (ge_3ds_save_provider_init(
            &source, original_frontend_save_paths[folder], folder)
            != GE_3DS_SAVE_PROVIDER_OK)
        return 0;
    for (destination_folder = FOLDER1;
            destination_folder < MAX_FOLDER_COUNT; ++destination_folder) {
        int32_t mission = 0;
        int32_t difficulty = 0;
        if (destination_folder == folder) continue;
        if (ge_3ds_save_provider_init(
                &destination,
                original_frontend_save_paths[destination_folder],
                destination_folder) != GE_3DS_SAVE_PROVIDER_OK)
            continue;
        if (!ge_3ds_save_provider_highest_completed(
                    &destination, &mission, &difficulty)
                && ge_3ds_save_provider_copy_if_empty(
                    source_provider, &destination)
                    == GE_3DS_SAVE_PROVIDER_OK) {
            result = 1;
            ge_3ds_save_provider_close(&destination);
            break;
        }
        ge_3ds_save_provider_close(&destination);
    }
    if (source_provider == &source) ge_3ds_save_provider_close(&source);
    return result;
}

static int original_frontend_erase_folder(void *context, int32_t folder)
{
    RuntimeOriginalFrontend *runtime = context;
    Ge3dsSaveProvider candidate = {0};
    Ge3dsSaveProvider *provider = &candidate;
    Ge3dsSaveProviderStatus status;
    if (runtime == NULL || runtime->save_provider == NULL
            || folder < FOLDER1 || folder >= MAX_FOLDER_COUNT) return 0;
    if (runtime->save_provider->ready
            && runtime->save_provider->folder == folder)
        provider = runtime->save_provider;
    else if (ge_3ds_save_provider_init(
            &candidate, original_frontend_save_paths[folder], folder)
            != GE_3DS_SAVE_PROVIDER_OK)
        return 0;
    status = ge_3ds_save_provider_erase(provider);
    if (provider == &candidate) ge_3ds_save_provider_close(&candidate);
    return status == GE_3DS_SAVE_PROVIDER_OK;
}

static void original_frontend_play_sfx(void *context, uint32_t sfx_id)
{
    (void)context;
    ge_original_gameplay_services_play_sfx(sfx_id);
}

typedef struct RuntimeWatchMissionAbort {
    Ge3dsSaveProvider *save_provider;
    uint32_t persistence_frontiers;
} RuntimeWatchMissionAbort;

static int original_watch_persist_settings_fields(
    void *context, uint8_t music_volume, uint8_t sfx_volume,
    uint16_t options)
{
    Ge3dsSaveProvider *provider = context;
    return provider != NULL
        && ge_3ds_save_provider_persist_settings(
            provider, music_volume, sfx_volume, options)
            == GE_3DS_SAVE_PROVIDER_OK;
}

static void original_watch_abort_set_mission_state_zero(void *context)
{
    (void)context;
    set_missionstate(MISSION_STATE_0);
}

static void original_watch_abort_request_title(void *context)
{
    (void)context;
    bossRunTitleStage();
}

static void original_watch_abort_mark_aborted(void *context)
{
    (void)context;
    mission_failed_or_aborted = TRUE;
}

static void original_watch_abort_persist_settings(void *context)
{
    RuntimeWatchMissionAbort *runtime = context;
    GeOriginalBondInputProvider *input_provider =
        ge_original_bond_input_provider();
    uint16_t options = 0U;

    /* Exact fileSaveSettingsForFolder option packing.  The retained option
     * getters and Bond input provider own the values; the durable provider
     * owns only fileClearSavefileForFolder's compare/write boundary. */
    if (input_provider->look_vertical_inverted)
        options |= GE_3DS_SAVE_OPTION_INVERTLOOK;
    if (cur_player_get_autoaim())
        options |= GE_3DS_SAVE_OPTION_AUTOAIM;
    if (input_provider->aim_control)
        options |= GE_3DS_SAVE_OPTION_AIMCONTROL;
    if (cur_player_get_sight_onscreen_control())
        options |= GE_3DS_SAVE_OPTION_SIGHTONSCREEN;
    if (cur_player_get_lookahead())
        options |= GE_3DS_SAVE_OPTION_LOOKAHEAD;
    if (cur_player_get_ammo_onscreen_setting())
        options |= GE_3DS_SAVE_OPTION_DISPLAYAMMO;
    if (cur_player_get_screen_setting() == SCREEN_SIZE_WIDESCREEN)
        options |= GE_3DS_SAVE_OPTION_SCREENWIDE;
    else if (cur_player_get_screen_setting() == SCREEN_SIZE_CINEMA)
        options |= GE_3DS_SAVE_OPTION_SCREENCINEMA;
    if (get_screen_ratio() != SCREEN_RATIO_NORMAL)
        options |= GE_3DS_SAVE_OPTION_SCREENRATIO;
    options |= (uint16_t)((cur_player_get_control_type() << 8)
        & GE_3DS_SAVE_OPTION_CONTROLTYPE);

    if (runtime == NULL || runtime->save_provider == NULL
            || ge_3ds_save_provider_persist_settings(
                runtime->save_provider,
                (uint8_t)(get_mTrack2Vol() >> 7),
                (uint8_t)(call_sndGetSfxSlotFirstNaturalVolume() >> 7),
                options) != GE_3DS_SAVE_PROVIDER_OK) {
        if (runtime != NULL) ++runtime->persistence_frontiers;
    }
}

static void original_watch_abort_play_beep(void *context)
{
    (void)context;
    ge_original_gameplay_services_play_sfx(CAMERA_BEEP1_SFX);
}

static void original_frontend_play_music(void *context, int32_t music_id)
{
    RuntimeOriginalFrontend *runtime = context;
    ge_original_gameplay_services_play_music(music_id);
    if (runtime != NULL) {
        runtime->requested_music_track = music_id;
        ++runtime->music_request_generation;
    }
}

static void original_frontend_stop_music(void *context)
{
    RuntimeOriginalFrontend *runtime = context;
    if (runtime != NULL) {
        runtime->requested_music_track = -1;
        ++runtime->music_request_generation;
    }
}

static uint32_t original_frontend_cast_random(void *context)
{
    (void)context;
    return randomGetNext();
}

static bool original_frontend_begin_ramrom(
    RuntimeOriginalFrontend *runtime)
{
    static const char *const paths[] = {
        "ramrom/ramrom_Dam_1.bin",
        "ramrom/ramrom_Dam_2.bin",
        "ramrom/ramrom_Facility_1.bin",
        "ramrom/ramrom_Facility_2.bin",
        "ramrom/ramrom_Facility_3.bin",
        "ramrom/ramrom_Runway_1.bin",
        "ramrom/ramrom_Runway_2.bin",
        "ramrom/ramrom_BunkerI_1.bin",
        "ramrom/ramrom_BunkerI_2.bin",
        "ramrom/ramrom_Silo_1.bin",
        "ramrom/ramrom_Silo_2.bin",
        "ramrom/ramrom_Frigate_1.bin",
        "ramrom/ramrom_Frigate_2.bin",
        "ramrom/ramrom_Train.bin",
    };
    const GeAssetPackEntry *entry;
    const char *path;
    uint8_t *data;
    size_t bytes_read = 0U;
    GeOriginalRamromReplay replay;
    if (runtime == NULL || runtime->asset_pack == NULL) return false;
    /* select_ramrom_to_play uses randomGetNext() % i after walking this exact
     * zero-lock US table. The cast scheduler has already consumed its own
     * authored random calls before reaching this service boundary. */
    path = paths[original_frontend_cast_random(runtime)
        % (sizeof(paths) / sizeof(paths[0]))];
    entry = ge_asset_pack_find(runtime->asset_pack, path);
    if (entry == NULL || entry->data_size > SIZE_MAX) return false;
    data = malloc((size_t)entry->data_size);
    if (data == NULL) return false;
    if (ge_asset_pack_read(runtime->asset_pack, path, data,
            (size_t)entry->data_size, &bytes_read) != GE_ASSET_PACK_OK
            || bytes_read != (size_t)entry->data_size
            || ge_original_ramrom_replay_begin(
                &replay, data, bytes_read) != GE_ORIGINAL_RAMROM_OK
            || ge_stage_asset_descriptor_by_level_id(
                replay.header.stage_id) == NULL) {
        free(data);
        return false;
    }
    free(runtime->ramrom_data);
    runtime->ramrom_data = data;
    runtime->ramrom_data_size = bytes_read;
    runtime->ramrom_replay = replay;
    memset(&runtime->ramrom_block, 0, sizeof(runtime->ramrom_block));
    runtime->ramrom_active = 1U;
    runtime->ramrom_block_ready = 0U;
    return true;
}

static int original_frontend_cast_completed(
    RuntimeOriginalFrontend *runtime, int32_t mission,
    int32_t first_difficulty, int32_t last_difficulty)
{
    int32_t folder;
    if (runtime == NULL || runtime->save_provider == NULL) return 0;
    for (folder = FOLDER1; folder < MAX_FOLDER_COUNT; ++folder) {
        Ge3dsSaveProvider candidate = {0};
        Ge3dsSaveProvider *provider = &candidate;
        int32_t difficulty;
        if (runtime->save_provider->ready
                && runtime->save_provider->folder == folder)
            provider = runtime->save_provider;
        else if (ge_3ds_save_provider_init(
                &candidate, original_frontend_save_paths[folder], folder)
                    != GE_3DS_SAVE_PROVIDER_OK)
            continue;
        for (difficulty = first_difficulty;
                difficulty <= last_difficulty; ++difficulty) {
            if (ge_3ds_save_provider_stage_status(
                    provider, mission, difficulty)
                        == STAGESTATUS_COMPLETED) {
                if (provider == &candidate)
                    ge_3ds_save_provider_close(&candidate);
                return 1;
            }
        }
        if (provider == &candidate)
            ge_3ds_save_provider_close(&candidate);
    }
    return 0;
}

static int original_frontend_cast_cradle_complete(void *context)
{
    return original_frontend_cast_completed(context, SP_LEVEL_CRADLE,
        DIFFICULTY_AGENT, DIFFICULTY_00);
}

static int original_frontend_cast_aztec_complete(void *context)
{
    return original_frontend_cast_completed(context, SP_LEVEL_AZTEC,
        DIFFICULTY_SECRET, DIFFICULTY_00);
}

static int original_frontend_cast_egypt_complete(void *context)
{
    return original_frontend_cast_completed(context, SP_LEVEL_EGYPT,
        DIFFICULTY_00, DIFFICULTY_00);
}

static void original_frontend_cast_play_intro_music(void *context)
{
    original_frontend_play_music(context, M_INTRO);
}

static void original_frontend_set_difficulty(void *context,
                                             int32_t difficulty)
{
    (void)context;
    /* Exact lvlSetSelectedDifficulty body from lv.c. The live getter and
     * difficulty runtime are retained in the guard/difficulty slice that owns
     * this canonical global; only the otherwise-dead one-line setter is
     * supplied at the frontend boundary. */
    g_SelectedDifficulty = difficulty;
}

static void original_frontend_set_007_sliders(void *context,float reaction,
                                              float health,float damage,
                                              float accuracy)
{
    (void)context;
    slider_007_mode_reaction=reaction;
    slider_007_mode_health=health;
    slider_007_mode_damage=damage;
    slider_007_mode_accuracy=accuracy;
}

static void original_frontend_request_stage(void *context, int32_t stage)
{
    RuntimeOriginalFrontend *runtime = context;
    runtime->requested_stage = stage;
    ge_original_boss_request_stage(stage);
}

static uint32_t original_frontend_input_edges(
    RuntimeOriginalFrontend *runtime, const GePortInput *input,
    int32_t current_menu)
{
    uint32_t edges = 0U;
    uint32_t axis = 0U;
    if ((input->pressed & (GE_PORT_ACTION_NEXT_WEAPON
                          | GE_PORT_ACTION_FIRE)) != 0U)
        edges |= GE_ORIGINAL_FRONTEND_INPUT_CONFIRM;
    if ((input->pressed & GE_PORT_ACTION_USE) != 0U)
        edges |= GE_ORIGINAL_FRONTEND_INPUT_BACK;
    if ((input->pressed & GE_PORT_ACTION_PAUSE) != 0U)
        edges |= GE_ORIGINAL_FRONTEND_INPUT_START;
    if ((input->pressed & GE_PORT_ACTION_PREV_WEAPON) != 0U)
        edges |= GE_ORIGINAL_FRONTEND_INPUT_FILE_COPY;
    if ((input->pressed & GE_PORT_ACTION_RELOAD) != 0U)
        edges |= GE_ORIGINAL_FRONTEND_INPUT_FILE_ERASE;
    /* File selection still consumes the bridge's grid accessibility edges;
     * mission/difficulty/report pages use the exact continuous N64 cursor
     * and authored hit tests below. */
    if (current_menu == MENU_FILE_SELECT) {
        if (input->move_y > 0.55f) axis |= GE_ORIGINAL_FRONTEND_INPUT_UP;
        if (input->move_y < -0.55f) axis |= GE_ORIGINAL_FRONTEND_INPUT_DOWN;
        if (input->move_x < -0.55f) axis |= GE_ORIGINAL_FRONTEND_INPUT_LEFT;
        if (input->move_x > 0.55f) axis |= GE_ORIGINAL_FRONTEND_INPUT_RIGHT;
    }
    edges |= axis & ~runtime->axis_held;
    runtime->axis_held = axis;
    return edges;
}

static void original_frontend_apply_visual_probe(
    GeOriginalFrontendStart *frontend)
{
    char page[32];
    FILE *stream;
    int32_t menu = MENU_INVALID;
    if (frontend == NULL
            || (stream = fopen(FRONTEND_VISUAL_PROBE_PATH, "rb")) == NULL)
        return;
    if (fgets(page, sizeof(page), stream) != NULL) {
        page[strcspn(page, "\r\n")] = '\0';
        if (strcmp(page, "file") == 0) menu = MENU_FILE_SELECT;
        else if (strcmp(page, "mode") == 0) menu = MENU_MODE_SELECT;
        else if (strcmp(page, "mission") == 0) menu = MENU_MISSION_SELECT;
        else if (strcmp(page, "difficulty") == 0) menu = MENU_DIFFICULTY;
        else if (strcmp(page, "briefing") == 0) menu = MENU_BRIEFING;
        else if (strcmp(page, "report") == 0) menu = MENU_MISSION_FAILED;
        else if (strcmp(page, "statistics") == 0)
            menu = MENU_MISSION_COMPLETE;
    }
    fclose(stream);
    if (menu == MENU_INVALID) return;
    frontend->current_menu = menu;
    frontend->maybe_prev_menu = MENU_INVALID;
    frontend->menu_timer = 0U;
    frontend->first_title_visit = 0U;
    frontend->logo_button_armed = 0U;
    frontend->stage_requested = 0U;
    if (menu == MENU_DIFFICULTY)
        frontend->difficulty = DIFFICULTY_AGENT;
    if (menu == MENU_BRIEFING) {
        frontend->difficulty = DIFFICULTY_AGENT;
        frontend->briefing_page = BRIEFING_TITLE;
    }
    if (menu == MENU_MISSION_FAILED || menu == MENU_MISSION_COMPLETE)
        frontend->difficulty = DIFFICULTY_AGENT;
    printf("Frontend visual probe: %s\n", page);
}

static const char *original_frontend_text(uint16_t text_id)
{
    return ge_original_language_text(text_id);
}

static const char *original_campaign_text(uint16_t text_id)
{
    return ge_original_language_text(text_id);
}

static bool runtime_diagnostics_requested(
    const GeStageAssetDescriptor *stage_assets)
{
    char visual_path[192];
    FILE *stream = fopen(INPUT_PROBE_PATH, "rb");
    if (stream != NULL) {
        fclose(stream);
        return true;
    }
    if (stage_assets == NULL
            || snprintf(visual_path, sizeof(visual_path),
                "sdmc:/3ds/goldeneye-3ds/%s-visual-tour.geview",
                stage_assets->key) < 0)
        return false;
    stream = fopen(visual_path, "rb");
    if (stream == NULL) return false;
    fclose(stream);
    return true;
}

static void configure_original_font_texture_environment(C3D_TexEnv *env)
{
    /* The ROM I8 coverage is replicated through GPU_RGBA4, so the normal
     * PICA modulation path applies it to both the vertex ink and alpha. */
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both,
                  GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_MODULATE);
}

static bool prepare_original_frontend_rareware(
    const RuntimeOriginalFrontend *runtime,
    const GeOriginalFrontendPresentation *presentation)
{
    const RuntimeGbiModel *front;
    const RuntimeGbiModel *body;
    Vertex *destination;
    const float radians = presentation != NULL
        ? presentation->rareware_rotation_degrees
            * (3.14159265358979323846f / 180.0f)
        : 0.0f;
    size_t index;
    if (runtime == NULL || presentation == NULL
            || (front = runtime->rareware_front) == NULL || !front->loaded
            || (body = runtime->rareware_body) == NULL || !body->loaded)
        return false;
    destination = (Vertex *)vertex_buffer + RAREWARE_FRONT_VERTEX_OFFSET;
    for (index = 0U; index < front->vertex_count; ++index) {
        static const uint8_t diffuse_rgb[3] = {255U, 255U, 255U};
        static const int8_t direction[3] = {0, 127, 0};
        const RuntimeModelVertex *source = &front->vertices[index];
        const uint8_t packed_normal[3] = {
            (uint8_t)(int8_t)lrintf(source->normal_x * 127.0f),
            (uint8_t)(int8_t)lrintf(source->normal_y * 127.0f),
            (uint8_t)(int8_t)lrintf(source->normal_z * 127.0f),
        };
        const uint8_t ambient_rgb[3] = {
            presentation->rareware_light_ambient,
            presentation->rareware_light_ambient,
            presentation->rareware_light_ambient,
        };
        const float authored[3] = {source->x, source->y, source->z};
        GeOriginalFrontendGeneratedVertex generated;
        float projected[3];
        float tl_u, tl_v, tr_u, tr_v;
        float bl_u, bl_v, br_u, br_v;
        float top_u, top_v, bottom_u, bottom_v;
        if (!ge_original_frontend_generate_lit_vertex(
                packed_normal, 255U, radians, ambient_rgb, diffuse_rgb,
                direction, &generated))
            return false;
        ge_original_frontend_rareware_project(
            authored, presentation->rareware_rotation_degrees,
            presentation->camera_eye[2], projected);
        Tex3DS_SubTextureTopLeft(
            &rareware_front_subtexture, &tl_u, &tl_v);
        Tex3DS_SubTextureTopRight(
            &rareware_front_subtexture, &tr_u, &tr_v);
        Tex3DS_SubTextureBottomLeft(
            &rareware_front_subtexture, &bl_u, &bl_v);
        Tex3DS_SubTextureBottomRight(
            &rareware_front_subtexture, &br_u, &br_v);
        top_u = tl_u + (tr_u - tl_u) * generated.generated_uv[0];
        top_v = tl_v + (tr_v - tl_v) * generated.generated_uv[0];
        bottom_u = bl_u + (br_u - bl_u) * generated.generated_uv[0];
        bottom_v = bl_v + (br_v - bl_v) * generated.generated_uv[0];
        destination[index] = (Vertex){
            projected[0], projected[1], projected[2],
            top_u + (bottom_u - top_u) * generated.generated_uv[1],
            top_v + (bottom_v - top_v) * generated.generated_uv[1],
            (float)presentation->rareware_primary_rgb[0] / 255.0f,
            (float)presentation->rareware_primary_rgb[1] / 255.0f,
            (float)presentation->rareware_primary_rgb[2] / 255.0f,
            1.0f,
        };
    }
    GSPGPU_FlushDataCache(destination,
        front->vertex_count * sizeof(*destination));
    destination = (Vertex *)vertex_buffer + RAREWARE_BODY_VERTEX_OFFSET;
    for (index = 0U; index < body->vertex_count; ++index) {
        const RuntimeModelVertex *source = &body->vertices[index];
        static const uint8_t diffuse_rgb[3] = {255U, 255U, 255U};
        static const int8_t direction[3] = {0, 127, 0};
        const uint8_t packed_normal[3] = {
            (uint8_t)(int8_t)lrintf(source->normal_x * 127.0f),
            (uint8_t)(int8_t)lrintf(source->normal_y * 127.0f),
            (uint8_t)(int8_t)lrintf(source->normal_z * 127.0f),
        };
        const uint8_t ambient_rgb[3] = {
            presentation->rareware_light_ambient,
            presentation->rareware_light_ambient,
            presentation->rareware_light_ambient,
        };
        GeOriginalFrontendGeneratedVertex generated;
        float generated_uv[2];
        float tl_u, tl_v, tr_u, tr_v;
        float bl_u, bl_v, br_u, br_v;
        float top_u, top_v, bottom_u, bottom_v;
        const float authored[3] = {source->x, source->y, source->z};
        float projected[3];
        if (!ge_original_frontend_generate_lit_vertex(
                packed_normal, 255U, radians, ambient_rgb, diffuse_rgb,
                direction, &generated))
            return false;
        ge_original_frontend_rareware_body_uv(&generated, generated_uv);
        ge_original_frontend_rareware_project(
            authored, presentation->rareware_rotation_degrees,
            presentation->camera_eye[2], projected);
        Tex3DS_SubTextureTopLeft(
            &rareware_body_subtexture, &tl_u, &tl_v);
        Tex3DS_SubTextureTopRight(
            &rareware_body_subtexture, &tr_u, &tr_v);
        Tex3DS_SubTextureBottomLeft(
            &rareware_body_subtexture, &bl_u, &bl_v);
        Tex3DS_SubTextureBottomRight(
            &rareware_body_subtexture, &br_u, &br_v);
        top_u = tl_u + (tr_u - tl_u) * generated_uv[0];
        top_v = tl_v + (tr_v - tl_v) * generated_uv[0];
        bottom_u = bl_u + (br_u - bl_u) * generated_uv[0];
        bottom_v = bl_v + (br_v - bl_v) * generated_uv[0];
        destination[index] = (Vertex){
            projected[0], projected[1], projected[2],
            top_u + (bottom_u - top_u) * generated_uv[1],
            top_v + (bottom_v - top_v) * generated_uv[1],
            (float)presentation->rareware_secondary_rgb[0] / 255.0f,
            (float)presentation->rareware_secondary_rgb[1] / 255.0f,
            (float)presentation->rareware_secondary_rgb[2] / 255.0f,
            1.0f,
        };
    }
    GSPGPU_FlushDataCache(destination,
        body->vertex_count * sizeof(*destination));
    if (runtime->rareware_mesh != NULL
            && runtime->rareware_mesh->loaded) {
        Vertex *letters = (Vertex *)vertex_buffer + RAREWARE_VERTEX_OFFSET;
        for (index = 0U; index < runtime->rareware_mesh->vertex_count;
                ++index) {
            const float authored[3] = {
                runtime->rareware_mesh->vertices[index].x,
                runtime->rareware_mesh->vertices[index].y,
                runtime->rareware_mesh->vertices[index].z,
            };
            float projected[3];
            letters[index] = runtime->rareware_mesh->vertices[index];
            ge_original_frontend_rareware_project(
                authored, presentation->rareware_rotation_degrees,
                presentation->camera_eye[2], projected);
            letters[index].x = projected[0];
            letters[index].y = projected[1];
            letters[index].z = projected[2];
            letters[index].r = (float)presentation->rareware_primary_rgb[0]
                / 255.0f;
            letters[index].g = (float)presentation->rareware_primary_rgb[1]
                / 255.0f;
            letters[index].b = (float)presentation->rareware_primary_rgb[2]
                / 255.0f;
            letters[index].a = 1.0f;
        }
        GSPGPU_FlushDataCache(letters,
            runtime->rareware_mesh->vertex_count * sizeof(*letters));
    }
    return true;
}

static bool import_original_font_texture(
    const Ge3dsOriginalHudAtlas *atlas, C3D_Tex *texture)
{
    /* tex3ds -z none -f rgba4 for a 128x128 image produces these exact
     * container fields before the already tiled payload. Import through the
     * same path as the working world textures so Citro3D owns the transfer
     * and texture-unit lifetime. The payload remains the ROM-derived atlas. */
    static const uint8_t header[] = {
        0x01,0x00,0x24,0x04,0x00,0x80,0x00,0x80,0x00,0x00,0x00,
        0x00,0x04,0x00,0x04,0x00,0x00,0x00,0x00,0x80,0x00,
    };
    const size_t container_size = sizeof(header) + sizeof(atlas->pixels);
    u32 image_size = 0U;
    void *image;
    uint8_t *container;
    Tex3DS_Texture imported;
    if (atlas == NULL || texture == NULL || !atlas->ready) return false;
    container = malloc(container_size);
    if (container == NULL) return false;
    memcpy(container, header, sizeof(header));
    memcpy(container + sizeof(header), atlas->pixels, sizeof(atlas->pixels));
    imported = Tex3DS_TextureImport(
        container, container_size, texture, NULL, false);
    free(container);
    if (imported == NULL || Tex3DS_GetNumSubTextures(imported) != 1U) {
        if (imported != NULL) Tex3DS_TextureFree(imported);
        return false;
    }
    Tex3DS_TextureFree(imported);
    /* Tex3DS establishes the canonical format and sampler metadata.  Publish
     * the already PICA-tiled ROM atlas directly into its linear backing store;
     * using the display-transfer upload path for this runtime-built buffer can
     * leave texture memory at the allocator's white fill on real 3DS/Azahar. */
    image = C3D_Tex2DGetImagePtr(texture, 0, &image_size);
    if (image == NULL || image_size != sizeof(atlas->pixels)) {
        C3D_TexDelete(texture);
        return false;
    }
    memcpy(image, atlas->pixels, sizeof(atlas->pixels));
    GSPGPU_FlushDataCache(image, image_size);
    C3D_TexSetFilter(texture, GPU_LINEAR, GPU_LINEAR);
    C3D_TexSetWrap(texture, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    return true;
}

static bool prepare_original_frontend_gunbarrel(
    RuntimeOriginalFrontend *runtime)
{
    GeOriginalGunbarrelLayerHoleVertex authored[
        GE_ORIGINAL_GUNBARREL_MAX_LAYER_HOLE_VERTICES];
    GeOriginalGunbarrelSightRect sight_rect;
    Vertex *destination = (Vertex *)vertex_buffer
        + ORIGINAL_FRONTEND_MODEL_VERTEX_OFFSET;
    const GeOriginalGunbarrelFrame *frame;
    uint32_t hole_count;
    size_t vertex;

    if (runtime == NULL || vertex_buffer == NULL) return false;
    frame = &runtime->gunbarrel_frame;
    hole_count = ge_original_gunbarrel_build_frame_holes(frame, authored,
        GE_ORIGINAL_GUNBARREL_MAX_LAYER_HOLE_VERTICES);
    for (vertex = 0U; vertex < hole_count; ++vertex) {
        const GeOriginalGunbarrelLayerHoleVertex *source = &authored[vertex];
        destination[vertex] = (Vertex){
            40.0f + source->x * 0.25f,
            240.0f - source->y * 0.25f,
            0.5f, 0.0f, 0.0f,
            (float)source->red / 255.0f,
            (float)source->green / 255.0f,
            (float)source->blue / 255.0f,
            (float)source->alpha / 255.0f,
        };
    }
    runtime->gunbarrel_hole_vertex_count = hole_count;
    if (runtime->gunbarrel_blood.ready
            && runtime->gunbarrel_blood_texture_generation
                != runtime->gunbarrel_blood.generation) {
        u32 image_size = 0U;
        uint8_t *image;
        size_t y;
        if (!runtime->gunbarrel_blood_texture_loaded) {
            if (!C3D_TexInit(&runtime->gunbarrel_blood_texture,
                             128U, 128U, GPU_RGBA4))
                return false;
            runtime->gunbarrel_blood_texture_loaded = true;
            C3D_TexSetFilter(&runtime->gunbarrel_blood_texture,
                             GPU_LINEAR, GPU_LINEAR);
            C3D_TexSetWrap(&runtime->gunbarrel_blood_texture,
                           GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        }
        image = C3D_Tex2DGetImagePtr(
            &runtime->gunbarrel_blood_texture, 0, &image_size);
        if (image == NULL || image_size != 128U * 128U * 2U)
            return false;
        memset(image, 0, image_size);
        for (y = 0U; y < GE_ORIGINAL_GUNBARREL_BLOOD_HEIGHT; ++y) {
            size_t x;
            for (x = 0U; x < GE_ORIGINAL_GUNBARREL_BLOOD_WIDTH; ++x) {
                const size_t source_pixel = y
                    * GE_ORIGINAL_GUNBARREL_BLOOD_WIDTH + x;
                const uint8_t source = runtime->gunbarrel_blood.pixels[
                    source_pixel >> 1U];
                const uint8_t intensity = (source_pixel & 1U) == 0U
                    ? source >> 4U : source & 0x0fU;
                const uint8_t packed = (uint8_t)(intensity * 0x11U);
                const size_t offset = original_frontend_swizzled_offset(
                    128U, x, y) * 2U;
                image[offset] = packed;
                image[offset + 1U] = packed;
            }
        }
        GSPGPU_FlushDataCache(image, image_size);
        runtime->gunbarrel_blood_texture_generation =
            runtime->gunbarrel_blood.generation;
    }
    if (runtime->gunbarrel_blood_texture_loaded) {
        const float u1 = (float)GE_ORIGINAL_GUNBARREL_BLOOD_WIDTH / 128.0f;
        const float v1 = (float)GE_ORIGINAL_GUNBARREL_BLOOD_HEIGHT / 128.0f;
        Vertex *quad = destination + 64U;
        const float red = 150.0f / 255.0f;
        const float alpha = 180.0f / 255.0f;
        quad[0] = (Vertex){40, 240, 0.5f, 0, 1, red, 0, 0, alpha};
        quad[1] = (Vertex){360, 240, 0.5f, u1, 1, red, 0, 0, alpha};
        quad[2] = (Vertex){360, 0, 0.5f, u1, 1 - v1,
                           red, 0, 0, alpha};
        quad[3] = quad[0];
        quad[4] = quad[2];
        quad[5] = (Vertex){40, 0, 0.5f, 0, 1 - v1,
                           red, 0, 0, alpha};
    }
    {
        Vertex *quad = destination + 70U;
        const float alpha = (float)frame->fade_alpha / 255.0f;
        quad[0] = (Vertex){40, 240, 0.5f, 0, 0, 0, 0, 0, alpha};
        quad[1] = (Vertex){360, 240, 0.5f, 0, 0, 0, 0, 0, alpha};
        quad[2] = (Vertex){360, 0, 0.5f, 0, 0, 0, 0, 0, alpha};
        quad[3] = quad[0];
        quad[4] = quad[2];
        quad[5] = (Vertex){40, 0, 0.5f, 0, 0, 0, 0, 0, alpha};
    }
    runtime->gunbarrel_sight_rect_visible =
        runtime->gunbarrel_sight_texture_loaded
        && ge_original_gunbarrel_sight_rect(frame, &sight_rect);
    if (runtime->gunbarrel_sight_rect_visible) {
        const float native_scale = 320.0f / 440.0f;
        const float left = 40.0f
            + (float)sight_rect.destination_left * native_scale;
        const float right = 40.0f
            + (float)sight_rect.destination_right * native_scale;
        const float top = 240.0f
            - (float)sight_rect.destination_top * native_scale;
        const float bottom = 240.0f
            - (float)sight_rect.destination_bottom * native_scale;
        const float u0 = (float)sight_rect.source_left / 512.0f;
        const float u1 = (float)sight_rect.source_right / 512.0f;
        const float v0 = 1.0f
            - (float)sight_rect.source_top / 512.0f;
        const float v1 = 1.0f
            - (float)sight_rect.source_bottom / 512.0f;
        Vertex *quad = destination + 76U;
        quad[0] = (Vertex){left, top, 0.5f, u0, v0, 0, 0, 0, 1};
        quad[1] = (Vertex){right, top, 0.5f, u1, v0, 0, 0, 0, 1};
        quad[2] = (Vertex){right, bottom, 0.5f, u1, v1,
                           1, 1, 1, 1};
        quad[3] = quad[0];
        quad[4] = quad[2];
        quad[5] = (Vertex){left, bottom, 0.5f, u0, v1,
                           1, 1, 1, 1};
    }
    GSPGPU_FlushDataCache(destination,
        82U * sizeof(*destination));
    return true;
}

static bool prepare_original_frontend_gunbarrel_bond(
    RuntimeOriginalFrontend *runtime, GeTextureCache *texture_cache)
{
    const GeOriginalGunbarrelBondScene *scene;
    Vertex *destination;
    const float focal = 120.0f
        / tanf(46.0f * 0.5f * (3.14159265358979323846f / 180.0f));
    size_t batch_index;
    size_t vertex_index;

    if (runtime == NULL || texture_cache == NULL || vertex_buffer == NULL)
        return false;
    scene = &runtime->gunbarrel_bond_scene;
    if (scene->vertex_count == 0U) return true;
    if (scene->vertices == NULL || scene->batches == NULL
            || scene->vertex_count
                > ORIGINAL_FRONTEND_MODEL_VERTEX_CAPACITY
                    - ORIGINAL_FRONTEND_GUNBARREL_BOND_VERTEX_OFFSET)
        return false;
    destination = (Vertex *)vertex_buffer
        + ORIGINAL_FRONTEND_MODEL_VERTEX_OFFSET
        + ORIGINAL_FRONTEND_GUNBARREL_BOND_VERTEX_OFFSET;
    for (batch_index = 0U; batch_index < scene->batch_count;
            ++batch_index) {
        const GeDamRoomDrawBatch *batch = &scene->batches[batch_index];
        const uint16_t image_id = batch->texture.texture_id;
        if (batch->first_vertex > scene->vertex_count
                || batch->vertex_count
                    > scene->vertex_count - batch->first_vertex)
            return false;
        if (ge_3ds_scene_textures_find(
                &dam_scene_textures, image_id) == NULL) {
            const Ge3dsSceneTextureStatus texture_status =
                ge_3ds_scene_textures_ensure_image(
                    texture_cache, &dam_scene_textures, image_id);
            if (texture_status != GE_3DS_SCENE_TEXTURE_OK
                    && texture_status != GE_3DS_SCENE_TEXTURE_PARTIAL)
                return false;
        }
    }
    for (vertex_index = 0U; vertex_index < scene->vertex_count;
            ++vertex_index) {
        const GeDamRoomWorldVertex *source = &scene->vertices[vertex_index];
        const float eye_x = source->world[0];
        const float eye_y = source->world[1];
        const float eye_z = source->world[2];
        const float depth = -eye_z;
        if (!isfinite(eye_x) || !isfinite(eye_y) || !isfinite(eye_z)
                || depth <= 0.001f)
            return false;
        destination[vertex_index] = (Vertex){
            200.0f + eye_x * focal / depth,
            120.0f - eye_y * focal / depth,
            fminf(0.999f, fmaxf(0.001f, depth / 10000.0f)),
            source->processed.texture[0],
            source->processed.texture[1],
            (float)source->processed.rgba[0] / 255.0f,
            (float)source->processed.rgba[1] / 255.0f,
            (float)source->processed.rgba[2] / 255.0f,
            (float)source->processed.rgba[3] / 255.0f,
        };
    }
    for (batch_index = 0U; batch_index < scene->batch_count;
            ++batch_index) {
        const GeDamRoomDrawBatch *batch = &scene->batches[batch_index];
        const Ge3dsSceneTextureSlot *slot = ge_3ds_scene_textures_find(
            &dam_scene_textures, batch->texture.texture_id);
        if (slot == NULL) continue;
        for (vertex_index = batch->first_vertex;
                vertex_index < batch->first_vertex + batch->vertex_count;
                ++vertex_index) {
            GeTextureUv uv;
            if (ge_3ds_scene_texture_map_uv(
                    slot,
                    scene->vertices[vertex_index].source.texture_s,
                    scene->vertices[vertex_index].source.texture_t,
                    &batch->material, &uv) == GE_TEXTURE_UV_OK) {
                destination[vertex_index].u = uv.u;
                destination[vertex_index].v = uv.v;
            }
        }
    }
    GSPGPU_FlushDataCache(destination,
        scene->vertex_count * sizeof(*destination));
    return true;
}

static bool prepare_original_frontend_cast(
    RuntimeOriginalFrontend *runtime, GeTextureCache *texture_cache)
{
    const GeOriginalFrontendCastModelScene *scene;
    Vertex *destination;
    Vertex *fade_quad;
    size_t batch_index;
    size_t output_vertex_count = 0U;
    if (runtime == NULL || texture_cache == NULL || vertex_buffer == NULL)
        return false;
    scene = &runtime->cast_scene;
    if (scene->vertices == NULL || scene->batches == NULL
            || scene->vertex_count == 0U || scene->batch_count == 0U)
        return false;
    if (scene->batch_count > runtime->cast_projected_batch_capacity) {
        GeDamRoomDrawBatch *batches = realloc(
            runtime->cast_projected_batches,
            scene->batch_count * sizeof(*batches));
        if (batches == NULL) return false;
        runtime->cast_projected_batches = batches;
        runtime->cast_projected_batch_capacity = scene->batch_count;
    }
    destination = (Vertex *)vertex_buffer
        + ORIGINAL_FRONTEND_MODEL_VERTEX_OFFSET;
    for (batch_index = 0U; batch_index < scene->batch_count;
            ++batch_index) {
        const GeDamRoomDrawBatch *batch = &scene->batches[batch_index];
        const uint16_t image_id = batch->texture.texture_id;
        if (batch->first_vertex > scene->vertex_count
                || batch->vertex_count
                    > scene->vertex_count - batch->first_vertex
                || batch->vertex_count % 3U != 0U)
            return false;
        if (ge_3ds_scene_textures_find(
                &dam_scene_textures, image_id) == NULL) {
            const Ge3dsSceneTextureStatus texture_status =
                ge_3ds_scene_textures_ensure_image(
                    texture_cache, &dam_scene_textures, image_id);
            if (texture_status != GE_3DS_SCENE_TEXTURE_OK
                    && texture_status != GE_3DS_SCENE_TEXTURE_PARTIAL)
                return false;
        }
        {
            GeDamRoomDrawBatch *projected_batch =
                &runtime->cast_projected_batches[batch_index];
            const Ge3dsSceneTextureSlot *slot =
                ge_3ds_scene_textures_find(
                    &dam_scene_textures, batch->texture.texture_id);
            size_t first;
            *projected_batch = *batch;
            projected_batch->first_vertex = output_vertex_count;
            projected_batch->vertex_count = 0U;
            for (first = batch->first_vertex;
                    first < batch->first_vertex + batch->vertex_count;
                    first += 3U) {
                Ge3dsOriginalFrontendCastClipVertex input[3];
                Ge3dsOriginalFrontendCastProjectedVertex output[6];
                size_t output_count;
                size_t corner;
                for (corner = 0U; corner < 3U; ++corner) {
                    const GeDamRoomWorldVertex *source =
                        &scene->vertices[first + corner];
                    GeTextureUv uv;
                    size_t channel;
                    memcpy(input[corner].camera_space, source->world,
                        sizeof(input[corner].camera_space));
                    input[corner].texture[0] =
                        source->processed.texture[0];
                    input[corner].texture[1] =
                        source->processed.texture[1];
                    if (slot != NULL && ge_3ds_scene_texture_map_uv(
                            slot, source->source.texture_s,
                            source->source.texture_t,
                            &batch->material, &uv)
                                == GE_TEXTURE_UV_OK) {
                        input[corner].texture[0] = uv.u;
                        input[corner].texture[1] = uv.v;
                    }
                    for (channel = 0U; channel < 4U; ++channel)
                        input[corner].rgba[channel] =
                            (float)source->processed.rgba[channel] / 255.0f;
                }
                output_count =
                    ge_3ds_original_frontend_cast_clip_project_triangle(
                        input, output);
                if (output_vertex_count + output_count
                        > ORIGINAL_FRONTEND_MODEL_VERTEX_CAPACITY - 6U)
                    return false;
                for (corner = 0U; corner < output_count; ++corner) {
                    destination[output_vertex_count++] = (Vertex){
                        output[corner].projected[0],
                        output[corner].projected[1],
                        output[corner].projected[2],
                        output[corner].texture[0],
                        output[corner].texture[1],
                        output[corner].rgba[0],
                        output[corner].rgba[1],
                        output[corner].rgba[2],
                        output[corner].rgba[3],
                    };
                }
                projected_batch->vertex_count += output_count;
            }
        }
    }
    runtime->cast_projected_vertex_count = output_vertex_count;
    fade_quad = destination + ORIGINAL_FRONTEND_MODEL_VERTEX_CAPACITY - 6U;
    {
        const float alpha = 1.0f - scene->fade;
        fade_quad[0] = (Vertex){40, 240, 1, 0, 0, 0, 0, 0, alpha};
        fade_quad[1] = (Vertex){360, 240, 1, 0, 0, 0, 0, 0, alpha};
        fade_quad[2] = (Vertex){360, 0, 1, 0, 0, 0, 0, 0, alpha};
        fade_quad[3] = fade_quad[0];
        fade_quad[4] = fade_quad[2];
        fade_quad[5] = (Vertex){40, 0, 1, 0, 0, 0, 0, 0, alpha};
    }
    GSPGPU_FlushDataCache(destination,
        output_vertex_count * sizeof(*destination));
    GSPGPU_FlushDataCache(fade_quad, 6U * sizeof(*fade_quad));
    return true;
}

static void draw_original_frontend_list(
    C3D_RenderTarget *top_target,
    const RuntimeOriginalFrontend *runtime,
    int32_t menu,
    const GeOriginalFrontendPresentation *presentation,
    Ge3dsOriginalFrontendPage page,
    const Ge3dsOriginalHudDrawList *draw_list,
    const Ge3dsOriginalFrontendSpriteList *sprite_list,
    size_t sprite_vertex_count)
{
    C3D_TexEnv *texture_environment;
    const size_t text_vertex_count = draw_list->box_vertex_count
        + draw_list->glyph_vertex_count;
    memcpy((Vertex *)vertex_buffer + ORIGINAL_HUD_VERTEX_OFFSET,
           draw_list->vertices,
           (text_vertex_count + sprite_vertex_count)
               * sizeof(Vertex));
    GSPGPU_FlushDataCache(
        (Vertex *)vertex_buffer + ORIGINAL_HUD_VERTEX_OFFSET,
        renderer_vertex_flush_bytes(
            text_vertex_count + sprite_vertex_count,
            ORIGINAL_HUD_VERTEX_CAPACITY));
    C3D_FrameBegin(0);
    C3D_RenderTargetClear(top_target, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
    C3D_FrameDrawOn(top_target);
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, projection_uniform, &projection);
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0U, 0U, 0U, 0U);
    C3D_FogGasMode(GPU_NO_FOG, GPU_PLAIN_DENSITY, false);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_AlphaTest(false, GPU_ALWAYS, 0U);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ZERO);
    texture_environment = C3D_GetTexEnv(0);
    if (page == GE_3DS_ORIGINAL_FRONTEND_PAGE_FILE_SELECT
            && runtime != NULL && runtime->folder_background_ready) {
        C3D_TexBind(0, (C3D_Tex *)&runtime->folder_background_texture);
        C3D_TexEnvInit(texture_environment);
        C3D_TexEnvSrc(texture_environment, C3D_Both,
                      GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
        C3D_TexEnvFunc(texture_environment, C3D_Both, GPU_MODULATE);
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
            GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
        C3D_DrawArrays(GPU_TRIANGLES,
            ORIGINAL_FRONTEND_MODEL_VERTEX_OFFSET
                + ORIGINAL_FRONTEND_MODEL_VERTEX_CAPACITY - 6U,
            6U);
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
            GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ZERO);
    }
    C3D_TexBind(0, draw_list->frontend_glyph_font != 0U
        ? &original_gameplay_hud_font_texture
        : &original_hud_font_texture);
    configure_original_font_texture_environment(texture_environment);
    if (menu != MENU_GOLDENEYE_LOGO
            && menu != MENU_LEGAL_SCREEN
            && menu != MENU_NINTENDO_LOGO
            && menu != MENU_RAREWARE_LOGO
            && menu != MENU_EYE_INTRO
            && menu != MENU_DISPLAY_CAST
            && runtime != NULL && runtime->wallet_ready) {
        C3D_DrawArrays(GPU_TRIANGLES,
            ORIGINAL_HUD_VERTEX_OFFSET
                + draw_list->background_vertex_count,
            draw_list->box_vertex_count
                - draw_list->background_vertex_count);
    } else {
        C3D_DrawArrays(GPU_TRIANGLES, ORIGINAL_HUD_VERTEX_OFFSET,
                       draw_list->box_vertex_count);
    }
    if ((menu == MENU_GOLDENEYE_LOGO
                || menu == MENU_LEGAL_SCREEN
                || menu == MENU_NINTENDO_LOGO)
            && runtime != NULL && runtime->logo_ready) {
        RuntimeRendererMaterialCache material_cache = {0};
        RuntimeRendererPreparedMaterialCache prepared_cache = {0};
        size_t batch_index;
        for (batch_index = 0U; batch_index < runtime->logo_batch_count;
                ++batch_index) {
            const GeDamRoomDrawBatch *batch =
                &runtime->logo_batches[batch_index];
            const Ge3dsSceneTextureSlot *slot =
                original_frontend_batch_texture(
                    runtime, runtime->logo_scene_prop, batch);
            const Ge3dsMaterialBinding binding = {
                slot != NULL ? (C3D_Tex *)&slot->texture : NULL,
                GE_3DS_MATERIAL_TEXTURE_FALLBACK_SHADE,
            };
            Ge3dsMaterialResult material_result;
            if (renderer_apply_material_cached(
                    &material_cache, &prepared_cache,
                    &batch->material, &binding, &material_result,
                    NULL, NULL, NULL, NULL) == GE_3DS_MATERIAL_OK
                    && material_result.state.draw_enabled != 0U) {
                /* CULLMODE_BOTH is zero in the original ModelRenderData and
                 * therefore deliberately does not call modelApplyCullMode.
                 * The display list remains authoritative for culling, alpha,
                 * and any explicit render-mode changes.  The translated
                 * material already includes the constructor's inherited
                 * zbufferenabled=false state.  CPU projection above maps
                 * authored +Y to screen -Y, reversing triangle winding, so
                 * invert only the effective display-list cull face at this
                 * final screen-space boundary. */
                if (material_result.state.cull
                        == GE_PICA_APPLY_CULL_FRONT) {
                    C3D_CullFace(GPU_CULL_BACK_CCW);
                } else if (material_result.state.cull
                        == GE_PICA_APPLY_CULL_BACK) {
                    C3D_CullFace(GPU_CULL_FRONT_CCW);
                }
                C3D_DrawArrays(GPU_TRIANGLES,
                    ORIGINAL_FRONTEND_MODEL_VERTEX_OFFSET
                        + batch->first_vertex,
                    batch->vertex_count);
            }
        }
    } else if (menu == MENU_RAREWARE_LOGO
            && runtime != NULL && presentation != NULL
            && runtime->rareware_front != NULL
            && runtime->rareware_body != NULL
            && rareware_front_texture_loaded
            && rareware_body_texture_loaded
            && runtime->rareware_front->loaded
            && runtime->rareware_body->loaded) {
        size_t letter;
        /* load_display_rare_logo publishes these passes in this exact order:
         * textured front, four authored letter quads, textured body. */
        C3D_TexBind(0, &rareware_front_texture);
        C3D_TexEnvInit(texture_environment);
        C3D_TexEnvSrc(texture_environment, C3D_RGB,
                      GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
        C3D_TexEnvFunc(texture_environment, C3D_RGB, GPU_MODULATE);
        C3D_TexEnvSrc(texture_environment, C3D_Alpha,
                      GPU_PRIMARY_COLOR, 0, 0);
        C3D_TexEnvFunc(texture_environment, C3D_Alpha, GPU_REPLACE);
        C3D_DrawArrays(GPU_TRIANGLES, RAREWARE_FRONT_VERTEX_OFFSET,
                       runtime->rareware_front->vertex_count);
        if (runtime->rareware_mesh != NULL
                && runtime->rareware_mesh->loaded
                && runtime->rareware_mesh->textured) {
            for (letter = 0U; letter < 4U; ++letter) {
                C3D_TexBind(0, &rareware_textures[letter]);
                C3D_TexEnvInit(texture_environment);
                C3D_TexEnvSrc(texture_environment, C3D_RGB,
                    GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
                C3D_TexEnvFunc(texture_environment, C3D_RGB, GPU_MODULATE);
                C3D_TexEnvSrc(texture_environment, C3D_Alpha,
                    GPU_PRIMARY_COLOR, 0, 0);
                C3D_TexEnvFunc(texture_environment, C3D_Alpha, GPU_REPLACE);
                C3D_DrawArrays(GPU_TRIANGLES,
                    RAREWARE_VERTEX_OFFSET + letter * 6U, 6U);
            }
        }
        C3D_TexBind(0, &rareware_body_texture);
        C3D_TexEnvInit(texture_environment);
        C3D_TexEnvSrc(texture_environment, C3D_RGB,
                      GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
        C3D_TexEnvFunc(texture_environment, C3D_RGB, GPU_MODULATE);
        C3D_TexEnvSrc(texture_environment, C3D_Alpha,
                      GPU_PRIMARY_COLOR, 0, 0);
        C3D_TexEnvFunc(texture_environment, C3D_Alpha, GPU_REPLACE);
        C3D_DrawArrays(GPU_TRIANGLES, RAREWARE_BODY_VERTEX_OFFSET,
                       runtime->rareware_body->vertex_count);
    } else if (menu == MENU_DISPLAY_CAST && runtime != NULL
            && runtime->cast_projected_batches != NULL
            && runtime->cast_projected_vertex_count != 0U) {
        const GeOriginalFrontendCastModelScene *scene =
            &runtime->cast_scene;
        RuntimeRendererMaterialCache material_cache = {0};
        RuntimeRendererPreparedMaterialCache prepared_cache = {0};
        size_t batch_index;
        for (batch_index = 0U; batch_index < scene->batch_count;
                ++batch_index) {
            const GeDamRoomDrawBatch *batch =
                &runtime->cast_projected_batches[batch_index];
            const Ge3dsSceneTextureSlot *slot = ge_3ds_scene_textures_find(
                &dam_scene_textures, batch->texture.texture_id);
            const Ge3dsMaterialBinding binding = {
                slot != NULL ? (C3D_Tex *)&slot->texture : NULL,
                GE_3DS_MATERIAL_TEXTURE_FALLBACK_SHADE,
            };
            Ge3dsMaterialResult material_result;
            if (batch->vertex_count != 0U
                    && renderer_apply_material_cached(
                    &material_cache, &prepared_cache,
                    &batch->material, &binding, &material_result,
                    NULL, NULL, NULL, NULL) == GE_3DS_MATERIAL_OK
                    && material_result.state.draw_enabled != 0U) {
                /* constructor_menu18 passes raw PROP_TYPE_EXPLOSION (7).
                 * modelApplyRenderModeType3/4 compares against
                 * PROP_TYPE_EXPLOSION + 1, so the cast deliberately takes
                 * the generic TRILERP/MODULATEIA path. The gunbarrel also
                 * stores 7, but there it is the encoded VIEWER + 1 selector;
                 * applying that title-only vertex-alpha lighting here made
                 * skin and clothing triangles look detached or black. */
                C3D_CullFace(GPU_CULL_NONE);
                C3D_DepthTest(true, GPU_GEQUAL,
                    (GPU_WRITEMASK)(GPU_WRITE_COLOR | GPU_WRITE_DEPTH));
                C3D_DrawArrays(GPU_TRIANGLES,
                    ORIGINAL_FRONTEND_MODEL_VERTEX_OFFSET
                        + batch->first_vertex,
                    batch->vertex_count);
            }
        }
        C3D_TexEnvInit(C3D_GetTexEnv(1));
        if (scene->fade < 1.0f) {
            C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
            C3D_TexEnvInit(texture_environment);
            C3D_TexEnvSrc(texture_environment, C3D_Both,
                          GPU_PRIMARY_COLOR, 0, 0);
            C3D_TexEnvFunc(texture_environment, C3D_Both, GPU_REPLACE);
            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
                GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
                GPU_ONE, GPU_ZERO);
            C3D_DrawArrays(GPU_TRIANGLES,
                ORIGINAL_FRONTEND_MODEL_VERTEX_OFFSET
                    + ORIGINAL_FRONTEND_MODEL_VERTEX_CAPACITY - 6U,
                6U);
        }
    } else if (menu == MENU_EYE_INTRO && runtime != NULL) {
        const GeOriginalGunbarrelFrame *frame = &runtime->gunbarrel_frame;
        if (runtime->gunbarrel_sight_rect_visible
                && (runtime->gunbarrel_sight_texture_loaded
                    || runtime->folder_background_ready)) {
            C3D_TexBind(0, runtime->gunbarrel_sight_texture_loaded
                ? (C3D_Tex *)&runtime->gunbarrel_sight_texture
                : (C3D_Tex *)&runtime->folder_background_texture);
            C3D_TexEnvInit(texture_environment);
            C3D_TexEnvSrc(texture_environment, C3D_Both,
                          GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
            C3D_TexEnvFunc(texture_environment, C3D_Both, GPU_MODULATE);
            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
                GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
            C3D_DrawArrays(GPU_TRIANGLES,
                runtime->gunbarrel_sight_texture_loaded
                    ? ORIGINAL_FRONTEND_MODEL_VERTEX_OFFSET + 76U
                    : ORIGINAL_FRONTEND_MODEL_VERTEX_OFFSET
                        + ORIGINAL_FRONTEND_MODEL_VERTEX_CAPACITY - 6U,
                6U);
        }
        if (runtime->gunbarrel_hole_vertex_count != 0U) {
            size_t hole;
            C3D_TexEnvInit(texture_environment);
            C3D_TexEnvSrc(texture_environment, C3D_Both,
                          GPU_PRIMARY_COLOR, 0, 0);
            C3D_TexEnvFunc(texture_environment, C3D_Both, GPU_REPLACE);
            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
                GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
            for (hole = 0U;
                    hole < runtime->gunbarrel_hole_vertex_count;
                    hole += GE_ORIGINAL_GUNBARREL_HOLE_VERTEX_COUNT)
                C3D_DrawArrays(GPU_TRIANGLE_STRIP,
                    ORIGINAL_FRONTEND_MODEL_VERTEX_OFFSET + hole,
                    GE_ORIGINAL_GUNBARREL_HOLE_VERTEX_COUNT);
        }
        if ((frame->layers & GE_ORIGINAL_GUNBARREL_LAYER_BOND) != 0U
                && runtime->gunbarrel_bond_scene.vertex_count != 0U) {
            RuntimeRendererMaterialCache material_cache = {0};
            RuntimeRendererPreparedMaterialCache prepared_cache = {0};
            size_t batch_index;
            for (batch_index = 0U;
                    batch_index
                        < runtime->gunbarrel_bond_scene.batch_count;
                    ++batch_index) {
                const GeDamRoomDrawBatch *batch =
                    &runtime->gunbarrel_bond_scene.batches[batch_index];
                const Ge3dsSceneTextureSlot *slot =
                    ge_3ds_scene_textures_find(
                        &dam_scene_textures,
                        batch->texture.texture_id);
                const Ge3dsMaterialBinding binding = {
                    slot != NULL ? (C3D_Tex *)&slot->texture : NULL,
                    GE_3DS_MATERIAL_TEXTURE_FALLBACK_SHADE,
                };
                Ge3dsMaterialResult material_result;
                if (renderer_apply_material_cached(
                        &material_cache, &prepared_cache,
                        &batch->material, &binding, &material_result,
                        NULL, NULL, NULL, NULL) == GE_3DS_MATERIAL_OK
                        && material_result.state.draw_enabled != 0U) {
                    const GeOriginalGunbarrelBondScene *bond_scene =
                        &runtime->gunbarrel_bond_scene;
                    const int16_t model_type =
                        bond_scene->batch_model_types != NULL
                            ? bond_scene->batch_model_types[batch_index] : -1;
                    if (bond_scene->viewer_uses_vertex_alpha_lighting != 0U
                            && bond_scene->render_prop_type == 7U
                            && (model_type == 3 || model_type == 4)) {
                        C3D_TexEnv *lighting_environment =
                            C3D_GetTexEnv(1);

                        /* modelApplyRenderModeType3/4's VIEWER branch has a
                         * black environment, so its two-cycle colour LERP is
                         * exactly TEXEL0 * SHADE.rgb * SHADE.a. Preserve its
                         * first multiplication in stage 0, then consume the
                         * vertex alpha as RGB intensity in stage 1. */
                        C3D_TexEnvInit(texture_environment);
                        if (material_result.texture_bound != 0U) {
                            C3D_TexEnvSrc(texture_environment, C3D_RGB,
                                GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
                            C3D_TexEnvFunc(texture_environment, C3D_RGB,
                                GPU_MODULATE);
                            C3D_TexEnvSrc(texture_environment, C3D_Alpha,
                                GPU_TEXTURE0, GPU_CONSTANT,
                                GPU_PRIMARY_COLOR);
                            C3D_TexEnvFunc(texture_environment, C3D_Alpha,
                                GPU_INTERPOLATE);
                            /* The VIEWER branch forces environment alpha to
                             * 0xff even though gunbarrelRenderData's packed
                             * environment colour is zero. Alpha cycle one is
                             * therefore lerp(1, TEXEL0.a, SHADE.a). */
                            C3D_TexEnvColor(texture_environment,
                                UINT32_C(0xff000000));
                        } else {
                            C3D_TexEnvSrc(texture_environment, C3D_Both,
                                GPU_PRIMARY_COLOR, 0, 0);
                            C3D_TexEnvFunc(texture_environment, C3D_Both,
                                GPU_REPLACE);
                        }
                        C3D_TexEnvInit(lighting_environment);
                        C3D_TexEnvSrc(lighting_environment, C3D_RGB,
                            GPU_PREVIOUS, GPU_PRIMARY_COLOR, 0);
                        C3D_TexEnvOpRgb(lighting_environment,
                            GPU_TEVOP_RGB_SRC_COLOR,
                            GPU_TEVOP_RGB_SRC_ALPHA,
                            GPU_TEVOP_RGB_SRC_COLOR);
                        C3D_TexEnvFunc(lighting_environment, C3D_RGB,
                            GPU_MODULATE);
                        C3D_TexEnvSrc(lighting_environment, C3D_Alpha,
                            GPU_PREVIOUS, 0, 0);
                        C3D_TexEnvFunc(lighting_environment, C3D_Alpha,
                            GPU_REPLACE);

                        C3D_CullFace(GPU_CULL_NONE);
                        C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
                        if (batch->list_kind
                                == GE_DAM_ROOM_LIST_SECONDARY) {
                            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
                                GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
                                GPU_ONE, GPU_ZERO);
                        } else {
                            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
                                GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);
                        }
                    }
                    C3D_DrawArrays(GPU_TRIANGLES,
                        ORIGINAL_FRONTEND_MODEL_VERTEX_OFFSET
                            + ORIGINAL_FRONTEND_GUNBARREL_BOND_VERTEX_OFFSET
                            + batch->first_vertex,
                        batch->vertex_count);
                }
            }
            /* Later title layers configure stage 0 independently. Restore
             * stage 1 to Citro3D's canonical pass-through before blood/HUD. */
            C3D_TexEnvInit(C3D_GetTexEnv(1));
        }
        if ((frame->layers & GE_ORIGINAL_GUNBARREL_LAYER_BLOOD_IMAGE)
                    != 0U
                && runtime->gunbarrel_blood_texture_loaded) {
            C3D_TexBind(0,
                (C3D_Tex *)&runtime->gunbarrel_blood_texture);
            C3D_TexEnvInit(texture_environment);
            C3D_TexEnvSrc(texture_environment, C3D_Both,
                          GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
            C3D_TexEnvFunc(texture_environment, C3D_Both, GPU_MODULATE);
            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
                GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
                GPU_ONE, GPU_ZERO);
            C3D_DrawArrays(GPU_TRIANGLES,
                ORIGINAL_FRONTEND_MODEL_VERTEX_OFFSET + 64U, 6U);
        } else if ((frame->layers
                    & GE_ORIGINAL_GUNBARREL_LAYER_BLOOD_COLOUR) != 0U) {
            C3D_TexEnvInit(texture_environment);
            C3D_TexEnvSrc(texture_environment, C3D_Both,
                          GPU_PRIMARY_COLOR, 0, 0);
            C3D_TexEnvFunc(texture_environment, C3D_Both, GPU_REPLACE);
            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
                GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
                GPU_ONE, GPU_ZERO);
            C3D_DrawArrays(GPU_TRIANGLES,
                ORIGINAL_FRONTEND_MODEL_VERTEX_OFFSET + 64U, 6U);
        }
        if ((frame->layers & GE_ORIGINAL_GUNBARREL_LAYER_FADE_BLACK)
                != 0U && frame->fade_alpha != 0U) {
            C3D_TexEnvInit(texture_environment);
            C3D_TexEnvSrc(texture_environment, C3D_Both,
                          GPU_PRIMARY_COLOR, 0, 0);
            C3D_TexEnvFunc(texture_environment, C3D_Both, GPU_REPLACE);
            C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
                GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
                GPU_ONE, GPU_ZERO);
            C3D_DrawArrays(GPU_TRIANGLES,
                ORIGINAL_FRONTEND_MODEL_VERTEX_OFFSET + 70U, 6U);
        }
    } else if (menu != MENU_LEGAL_SCREEN
            && menu != MENU_SWITCH_SCREENS
            && menu != MENU_NINTENDO_LOGO
            && menu != MENU_RAREWARE_LOGO
            && menu != MENU_EYE_INTRO
            && menu != MENU_DISPLAY_CAST
            && runtime != NULL && runtime->wallet_ready) {
        RuntimeRendererMaterialCache material_cache = {0};
        RuntimeRendererPreparedMaterialCache prepared_cache = {0};
        size_t batch_index;
        for (batch_index = 0U; batch_index < runtime->wallet_batch_count;
                ++batch_index) {
            const GeDamRoomDrawBatch *batch =
                &runtime->wallet_batches[batch_index];
            const Ge3dsSceneTextureSlot *slot = ge_3ds_scene_textures_find(
                &dam_scene_textures, batch->texture.texture_id);
            const Ge3dsMaterialBinding binding = {
                slot != NULL ? (C3D_Tex *)&slot->texture : NULL,
                GE_3DS_MATERIAL_TEXTURE_FALLBACK_SHADE,
            };
            Ge3dsMaterialResult material_result;
            if (renderer_apply_material_cached(
                    &material_cache, &prepared_cache,
                    &batch->material, &binding, &material_result,
                    NULL, NULL, NULL, NULL) == GE_3DS_MATERIAL_OK
                    && material_result.state.draw_enabled != 0U) {
                /* D_8002AF84/unknown_folderselect both use CULLMODE_BOTH
                 * and disable the Z buffer. Preserve the authored model-level
                 * state after applying each display-list material. */
                C3D_CullFace(GPU_CULL_NONE);
                C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
                C3D_DrawArrays(GPU_TRIANGLES,
                    ORIGINAL_FRONTEND_MODEL_VERTEX_OFFSET
                        + batch->first_vertex,
                    batch->vertex_count);
            }
        }
    }
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
    C3D_CullFace(GPU_CULL_NONE);
    C3D_AlphaTest(false, GPU_ALWAYS, 0U);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ZERO);
    C3D_TexBind(0, draw_list->frontend_glyph_font != 0U
        ? &original_gameplay_hud_font_texture
        : &original_hud_font_texture);
    configure_original_font_texture_environment(texture_environment);
    {
        const size_t normal_glyph_count=draw_list->tab_glyph_vertex_count!=0U
            ? draw_list->tab_glyph_vertex_offset-draw_list->box_vertex_count
            : draw_list->glyph_vertex_count;
        C3D_DrawArrays(GPU_TRIANGLES,
                       ORIGINAL_HUD_VERTEX_OFFSET
                           + draw_list->box_vertex_count,
                       normal_glyph_count);
        if(draw_list->tab_glyph_vertex_count!=0U){
            C3D_TexBind(0,&original_gameplay_hud_font_texture);
            configure_original_font_texture_environment(texture_environment);
            C3D_DrawArrays(GPU_TRIANGLES,
                ORIGINAL_HUD_VERTEX_OFFSET
                    +draw_list->tab_glyph_vertex_offset,
                draw_list->tab_glyph_vertex_count);
        }
    }
    if (sprite_list != NULL && sprite_vertex_count != 0U) {
        size_t sprite_index;
        size_t first_vertex = ORIGINAL_HUD_VERTEX_OFFSET
            + text_vertex_count;
        for (sprite_index = 0U; sprite_index < sprite_list->count;
                ++sprite_index) {
            const Ge3dsOriginalFrontendSprite *sprite =
                &sprite_list->sprites[sprite_index];
            RuntimeFrontendSpriteTexture *texture;
            if ((size_t)sprite->image
                        >= GE_3DS_ORIGINAL_FRONTEND_MAX_SPRITES
                    || !(texture = &original_frontend_sprite_textures[
                            sprite->image])->loaded)
                continue;
            C3D_TexBind(0, &texture->texture);
            C3D_TexEnvInit(texture_environment);
            C3D_TexEnvSrc(texture_environment, C3D_Both,
                          GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
            C3D_TexEnvFunc(texture_environment, C3D_Both, GPU_MODULATE);
            C3D_DrawArrays(GPU_TRIANGLES, first_vertex, 6U);
            first_vertex += 6U;
        }
    }
    C3D_FrameEnd(0);
}

static Ge3dsOriginalFrontendPage original_frontend_page(int32_t menu)
{
    switch (menu) {
    case MENU_SWITCH_SCREENS:
        return GE_3DS_ORIGINAL_FRONTEND_PAGE_TITLE;
    case MENU_LEGAL_SCREEN:
        return GE_3DS_ORIGINAL_FRONTEND_PAGE_LEGAL;
    case MENU_NINTENDO_LOGO:
    case MENU_RAREWARE_LOGO:
    case MENU_EYE_INTRO:
    case MENU_GOLDENEYE_LOGO:
    case MENU_DISPLAY_CAST:
        return GE_3DS_ORIGINAL_FRONTEND_PAGE_TITLE;
    case MENU_FILE_SELECT:
        return GE_3DS_ORIGINAL_FRONTEND_PAGE_FILE_SELECT;
    case MENU_MODE_SELECT:
        return GE_3DS_ORIGINAL_FRONTEND_PAGE_MODE_SELECT;
    case MENU_MISSION_SELECT:
        return GE_3DS_ORIGINAL_FRONTEND_PAGE_MISSION_SELECT;
    case MENU_DIFFICULTY:
        return GE_3DS_ORIGINAL_FRONTEND_PAGE_DIFFICULTY;
    case MENU_007_OPTIONS:
        return GE_3DS_ORIGINAL_FRONTEND_PAGE_007_OPTIONS;
    case MENU_BRIEFING:
        return GE_3DS_ORIGINAL_FRONTEND_PAGE_BRIEFING;
    case MENU_MISSION_FAILED:
        return GE_3DS_ORIGINAL_FRONTEND_PAGE_REPORT;
    case MENU_MISSION_COMPLETE:
        return GE_3DS_ORIGINAL_FRONTEND_PAGE_STATISTICS;
    default:
        return GE_3DS_ORIGINAL_FRONTEND_PAGE_BRIEFING;
    }
}

static bool apply_original_frontend_music_request(
    RuntimeOriginalFrontend *runtime,
    GeOriginalMusicRuntime **music_runtime,
    GeAudioOutput *audio_output, bool audio_active)
{
    const char *music_path;

    if (runtime == NULL || music_runtime == NULL || audio_output == NULL)
        return false;
    if (runtime->applied_music_request_generation
            == runtime->music_request_generation)
        return true;
    if (audio_active)
        (void)ge_3ds_audio_bind_secondary(NULL);
    ge_original_music_runtime_close(*music_runtime);
    *music_runtime = NULL;
    if (!audio_active) {
        runtime->applied_music_request_generation =
            runtime->music_request_generation;
        return true;
    }
    if (runtime->requested_music_track >= 0) {
        music_path = ge_original_music_track_asset_path(
            runtime->requested_music_track);
        if (runtime->asset_pack == NULL || music_path == NULL
                || (*music_runtime =
                    ge_original_music_runtime_open_asset_pack(
                        runtime->asset_pack, music_path, INT16_MAX,
                        audio_output)) == NULL) {
            printf("Could not initialize original frontend music %ld.\n",
                   (long)runtime->requested_music_track);
            return false;
        }
        if (audio_active && ge_3ds_audio_bind_secondary(
                ge_original_music_runtime_output(*music_runtime)) != 0) {
            printf("Could not bind original frontend music %ld.\n",
                   (long)runtime->requested_music_track);
            return false;
        }
    }
    runtime->applied_music_request_generation =
        runtime->music_request_generation;
    return true;
}

static bool run_original_frontend(C3D_RenderTarget *top_target,
                                  bool cstick_available,
                                  GeTextureCache *texture_cache,
                                  RuntimeOriginalFrontend *runtime,
                                  GeOriginalFrontendStart *frontend,
                                  GeOriginalMusicRuntime **music_runtime,
                                  GeAudioOutput *audio_output,
                                  bool audio_active,
                                  bool reset_frontend,
                                  int32_t *selected_level_id)
{
    bool music_failed = false;
    GeOriginalFrontendServices services = {
        .context = runtime,
        .highest_unlocked_difficulty = original_frontend_highest_difficulty,
        .select_folder = original_frontend_select_folder,
        .set_selected_difficulty = original_frontend_set_difficulty,
        .request_stage = original_frontend_request_stage,
        .folder_has_progress = original_frontend_folder_has_progress,
        .folder_summary = original_frontend_folder_summary,
        .copy_folder_to_first_free =
            original_frontend_copy_folder_to_first_free,
        .erase_folder = original_frontend_erase_folder,
        .play_sfx = original_frontend_play_sfx,
        .play_music = original_frontend_play_music,
        .stop_music = original_frontend_stop_music,
        .set_007_sliders = original_frontend_set_007_sliders,
    };
    GeOriginalFrontendCastServices cast_services = {
        .context = runtime,
        .random_next = original_frontend_cast_random,
        .choose_random_head =
            ge_original_character_appearance_choose_head,
        .cradle_complete = original_frontend_cast_cradle_complete,
        .aztec_secret_or_00_complete =
            original_frontend_cast_aztec_complete,
        .egypt_00_complete = original_frontend_cast_egypt_complete,
        .play_intro_music = original_frontend_cast_play_intro_music,
    };
    if (top_target == NULL || texture_cache == NULL
            || runtime == NULL || frontend == NULL
            || selected_level_id == NULL)
        return false;
    if (reset_frontend) {
        Ge3dsSaveProvider *save_provider = runtime->save_provider;
        const bool return_from_ramrom =
            runtime->ramrom_return_to_title != 0U;
        if (return_from_ramrom)
            close_original_frontend_ramrom(runtime);
        runtime->save_provider = save_provider;
        runtime->requested_stage = LEVELID_NONE;
        runtime->axis_held = 0U;
        runtime->previous_menu = INT32_MIN;
        runtime->gunbarrel_started = 0U;
        runtime->cast_started = 0U;
        runtime->cast_terminal = 0U;
        memset(&runtime->cast_scene, 0, sizeof(runtime->cast_scene));
        runtime->cast_projected_vertex_count = 0U;
        ge_original_gunbarrel_blood_reset(&runtime->gunbarrel_blood);
        ge_original_character_appearance_begin_stage();
        if (!(return_from_ramrom
                ? ge_original_frontend_start_reset(frontend, &services)
                : ge_original_frontend_start_reset_canonical(
                    frontend, &services)))
            return false;
        original_frontend_apply_visual_probe(frontend);
    }
    if (!apply_original_frontend_music_request(
            runtime, music_runtime, audio_output, audio_active))
        return false;
    while (aptMainLoop()) {
        GePortInput input = read_input(cstick_available);
        GeOriginalFrontendSnapshot snapshot;
        Ge3dsOriginalFrontendLine lines[GE_ORIGINAL_FRONTEND_MAX_LINES];
        char formatted[GE_ORIGINAL_FRONTEND_MAX_LINES][192];
        char mission_header[3][128];
        char statistics_supplemental[3][160];
        char wallet_caption[8][48];
        static Ge3dsOriginalHudDrawList draw_list;
        Ge3dsOriginalFrontendSpriteList sprite_list;
        size_t sprite_vertex_count = 0U;
        uint8_t completed_difficulties = 0U;
        size_t line_count;
        size_t line;
        GeOriginalGunbarrelTickResult gunbarrel_result =
            GE_ORIGINAL_GUNBARREL_TICK_INVALID;
        const uint32_t input_edges =
            original_frontend_input_edges(
                runtime, &input, frontend->current_menu);
        if (frontend->current_menu == MENU_FILE_SELECT
                && runtime->wallet_bounds_ready
                && !ge_original_frontend_start_set_wallet_bounds(
                    frontend, runtime->wallet_bounds)) return false;
        if (frontend->current_menu == MENU_FILE_SELECT) {
            GeOriginalFrontendWalletBounds action_bounds[2];
            if (!ge_3ds_original_frontend_file_action_bounds(
                    &original_hud_atlas,
                    original_frontend_text(getStringID(
                        LTITLE,TITLE_STR_27_COPY)),
                    original_frontend_text(getStringID(
                        LTITLE,TITLE_STR_28_ERASE)),action_bounds)
                    || !ge_original_frontend_start_set_file_action_bounds(
                        frontend,action_bounds)) return false;
        }
        if (!ge_original_frontend_start_cursor_tick(frontend,
                (int8_t)lrintf(input.move_x * 80.0f),
                (int8_t)lrintf(input.move_y * 80.0f), 1.0f)
                || !ge_original_frontend_start_007_drag(frontend,
                    (input.held & (GE_PORT_ACTION_NEXT_WEAPON
                        | GE_PORT_ACTION_FIRE)) != 0U)
                || !ge_original_frontend_start_tick(
                frontend, input_edges)
                || !ge_original_frontend_start_snapshot(
                    frontend, &snapshot)) return false;
        if (!apply_original_frontend_music_request(
                runtime, music_runtime, audio_output, audio_active))
            return false;
        if (audio_active && *music_runtime != NULL && !music_failed
                && ge_original_music_runtime_tick_60hz(*music_runtime)
                    != GE_AUDIO_ABI_OK) {
            printf("Original frontend music render failed.\n");
            music_failed = true;
        }
        if (audio_active) ge_3ds_audio_pump();
        if (snapshot.menu == MENU_EYE_INTRO) {
            if (runtime->previous_menu != MENU_EYE_INTRO
                    || !runtime->gunbarrel_started) {
                ge_original_gunbarrel_reset(&runtime->gunbarrel_state);
                ge_original_gunbarrel_blood_reset(
                    &runtime->gunbarrel_blood);
                if (runtime->gunbarrel_bond != NULL
                        && ge_original_gunbarrel_bond_reset(
                            runtime->gunbarrel_bond)
                            != GE_ORIGINAL_GUNBARREL_BOND_OK)
                    return false;
                runtime->gunbarrel_started = 1U;
            }
            gunbarrel_result = ge_original_gunbarrel_tick(
                &runtime->gunbarrel_state,
                ge_original_gunbarrel_blood_tick,
                &runtime->gunbarrel_blood,
                &runtime->gunbarrel_frame);
            if ((runtime->gunbarrel_frame.layers
                        & GE_ORIGINAL_GUNBARREL_LAYER_BOND) != 0U
                    && runtime->gunbarrel_bond != NULL) {
                const GeOriginalGunbarrelBondStatus bond_status =
                    ge_original_gunbarrel_bond_tick(
                        runtime->gunbarrel_bond,
                        &runtime->gunbarrel_frame,
                        &runtime->gunbarrel_bond_scene);
                if (bond_status != GE_ORIGINAL_GUNBARREL_BOND_OK
                        || !prepare_original_frontend_gunbarrel_bond(
                            runtime, texture_cache)) {
                    printf("Frontend gunbarrel Bond tick failed: %s\n",
                        ge_original_gunbarrel_bond_status_name(
                            bond_status));
                    return false;
                }
            } else {
                memset(&runtime->gunbarrel_bond_scene, 0,
                       sizeof(runtime->gunbarrel_bond_scene));
            }
            if (gunbarrel_result == GE_ORIGINAL_GUNBARREL_TICK_INVALID
                    || gunbarrel_result
                        == GE_ORIGINAL_GUNBARREL_TICK_NEEDS_BLOOD_DECODER
                    || !prepare_original_frontend_gunbarrel(runtime))
                return false;
        }
        if (snapshot.menu == MENU_DISPLAY_CAST) {
            GeOriginalFrontendCastEvent event =
                GE_ORIGINAL_FRONTEND_CAST_EVENT_NONE;
            GeOriginalFrontendCastModelStatus model_status =
                GE_ORIGINAL_FRONTEND_CAST_MODEL_OK;
            if (runtime->previous_menu != MENU_DISPLAY_CAST
                    || !runtime->cast_started) {
                if (runtime->cast_model == NULL
                        || ge_original_frontend_cast_reset(
                            &runtime->cast, &cast_services, 0)
                                != GE_ORIGINAL_FRONTEND_CAST_OK) {
                    printf("Frontend cast scheduler reset failed.\n");
                    return false;
                }
                model_status =
                    ge_original_frontend_cast_model_begin_selection(
                        runtime->cast_model,
                        &runtime->cast.selection);
                if (model_status != GE_ORIGINAL_FRONTEND_CAST_MODEL_OK) {
                    printf("Frontend cast actor init failed: %s\n",
                        ge_original_frontend_cast_model_status_name(
                            model_status));
                    return false;
                }
                runtime->cast_started = 1U;
                runtime->cast_terminal = 0U;
            }
            if (!runtime->cast_terminal) {
                if (ge_original_frontend_cast_tick(
                        &runtime->cast, input_edges != 0U, &event)
                            != GE_ORIGINAL_FRONTEND_CAST_OK)
                    return false;
                if (event == GE_ORIGINAL_FRONTEND_CAST_EVENT_RELOAD) {
                    if (ge_original_frontend_cast_begin_current(
                            &runtime->cast)
                                != GE_ORIGINAL_FRONTEND_CAST_OK)
                        return false;
                    model_status =
                        ge_original_frontend_cast_model_begin_selection(
                            runtime->cast_model,
                            &runtime->cast.selection);
                    if (model_status
                            != GE_ORIGINAL_FRONTEND_CAST_MODEL_OK) {
                        printf("Frontend cast actor reload failed: %s\n",
                            ge_original_frontend_cast_model_status_name(
                                model_status));
                        return false;
                    }
                } else if (event
                        == GE_ORIGINAL_FRONTEND_CAST_EVENT_RAMROM) {
                    if (!original_frontend_begin_ramrom(runtime)
                            || !ge_original_frontend_start_ramrom(
                                frontend,
                                runtime->ramrom_replay.header.stage_id,
                                runtime->ramrom_replay.header.difficulty)) {
                        /* A corrupt/missing authored demo is a real asset
                         * failure, not permission to invent a substitute
                         * controller stream or silently select a mission. */
                        return false;
                    }
                    runtime->cast_terminal = 1U;
                } else if (event
                        == GE_ORIGINAL_FRONTEND_CAST_EVENT_MISSION_SELECT) {
                    if (!ge_original_frontend_start_cast_event(
                            frontend, event)) return false;
                    runtime->cast_terminal = 1U;
                } else if (event
                        != GE_ORIGINAL_FRONTEND_CAST_EVENT_NONE) {
                    if (!ge_original_frontend_start_cast_event(
                            frontend, event)) return false;
                }
                if (!runtime->cast_terminal) {
                    model_status = ge_original_frontend_cast_model_tick(
                        runtime->cast_model, &runtime->cast,
                        1U, 1.0f, &runtime->cast_scene);
                    if (model_status
                            != GE_ORIGINAL_FRONTEND_CAST_MODEL_OK
                            || ge_original_frontend_cast_snapshot(
                                &runtime->cast,
                                &runtime->cast_frame)
                                    != GE_ORIGINAL_FRONTEND_CAST_OK
                            || !prepare_original_frontend_cast(
                                runtime, texture_cache)) {
                        printf("Frontend cast tick failed: %s\n",
                            ge_original_frontend_cast_model_status_name(
                                model_status));
                        return false;
                    }
                }
            }
        }
        if (snapshot.stage_requested) {
            *selected_level_id = runtime->requested_stage;
            return ge_stage_asset_descriptor_by_level_id(
                runtime->requested_stage) != NULL;
        }
        line_count = snapshot.line_count;
        for (line = 0U; line < line_count; ++line) {
            const char *text = original_frontend_text(
                snapshot.lines[line].text_id);
            bool formatted_is_value = false;
            formatted[line][0] = '\0';
            if (snapshot.result_valid && snapshot.lines[line].objective) {
                const uint16_t status_id = snapshot.lines[line].status
                        == OBJECTIVESTATUS_COMPLETE
                    ? getStringID(LTITLE, TITLE_STR_91_COMPLETED)
                    : getStringID(LTITLE, TITLE_STR_92_FAILED);
                (void)snprintf(formatted[line], sizeof(formatted[line]),
                    "%s", original_frontend_text(status_id));
                formatted_is_value = true;
            } else if (snapshot.result_valid
                    && snapshot.menu == MENU_MISSION_COMPLETE) {
                if (line == 2U)
                    (void)snprintf(formatted[line], sizeof(formatted[line]),
                        "%02ld:%02ld",
                        (long)(snapshot.result.mission_time_ticks / 3600),
                        (long)((snapshot.result.mission_time_ticks / 60) % 60));
                else if (line == 3U) {
                    const int32_t all_hits = snapshot.result.head_hits
                        + snapshot.result.body_hits + snapshot.result.limb_hits
                        + snapshot.result.gun_hits + snapshot.result.hat_hits;
                    const int32_t hit_shots = all_hits
                        + snapshot.result.object_hits;
                    const float accuracy = snapshot.result.shots_fired > 0
                        ? (float)hit_shots * 100.0f
                            / (float)snapshot.result.shots_fired
                        : 0.0f;
                    (void)snprintf(formatted[line], sizeof(formatted[line]),
                        "%.1f%%",
                        (double)accuracy);
                } else if (line == 4U) {
                    uint16_t weapon_text_id = 0U;
                    const char *weapon = "";
                    if (ge_original_bond_live_weapon_choice_text(
                            snapshot.result.favorite_weapon_right,
                            &weapon_text_id))
                        weapon = original_frontend_text(weapon_text_id);
                    const size_t weapon_length = weapon != NULL
                        ? strcspn(weapon, "\r\n") : 0U;
                    (void)snprintf(formatted[line], sizeof(formatted[line]),
                        "%.*s%s",
                        (int)weapon_length, weapon != NULL ? weapon : "",
                        snapshot.result.favorite_weapon_dual ? " x 2" : "");
                } else if (line == 5U)
                    (void)snprintf(formatted[line], sizeof(formatted[line]),
                        "%ld",
                        (long)snapshot.result.shots_fired);
                else if (line >= 6U && line <= 9U) {
                    const int32_t all_hits_raw = snapshot.result.head_hits
                        + snapshot.result.body_hits + snapshot.result.limb_hits
                        + snapshot.result.gun_hits + snapshot.result.hat_hits;
                    const int32_t all_hits = all_hits_raw > 0
                        ? all_hits_raw : 1;
                    int32_t count = snapshot.result.head_hits;
                    if (line == 7U) count = snapshot.result.body_hits;
                    else if (line == 8U) count = snapshot.result.limb_hits;
                    else if (line == 9U) count = snapshot.result.gun_hits
                        + snapshot.result.hat_hits;
                    (void)snprintf(formatted[line], sizeof(formatted[line]),
                        "%ld (%ld%%)",
                        (long)count,
                        (long)floorf((float)count * 100.0f
                            / (float)all_hits + 0.5f));
                } else if (line == 10U)
                    (void)snprintf(formatted[line], sizeof(formatted[line]),
                        "%ld",
                        (long)snapshot.result.kill_count);
                formatted_is_value = formatted[line][0] != '\0';
            } else if (snapshot.menu == MENU_007_OPTIONS && line >= 2U) {
                const float percent = line == 4U
                    ? snapshot.lines[line].value * 10.0f
                    : snapshot.lines[line].value * 100.0f;
                (void)snprintf(formatted[line], sizeof(formatted[line]),
                    "%ld%%",
                    (long)percent);
                formatted_is_value = true;
            }
            lines[line].text = formatted[line][0] != '\0'
                    && !formatted_is_value
                ? formatted[line] : text;
            lines[line].value_text = formatted_is_value
                ? formatted[line] : NULL;
            lines[line].x = snapshot.lines[line].x;
            lines[line].y = snapshot.lines[line].y;
            lines[line].selected = snapshot.lines[line].selected;
            lines[line].objective = snapshot.lines[line].objective;
            lines[line].locked = snapshot.lines[line].status;
            lines[line].horizontal_align =
                snapshot.lines[line].horizontal_align;
            lines[line].vertical_align =
                snapshot.lines[line].vertical_align;
            lines[line].has_authored_position =
                snapshot.lines[line].has_authored_position;
            lines[line].value = snapshot.lines[line].value;
        }
        if (snapshot.menu == MENU_FILE_SELECT
                && runtime->wallet_bounds_ready) {
            if (snapshot.erase_pending && line_count >= 3U) {
                const float center_x =
                    runtime->wallet_centers[snapshot.folder][0];
                const float center_y =
                    runtime->wallet_centers[snapshot.folder][1];
                lines[0].x = (int16_t)floorf(center_x - 47.0f);
                lines[0].y = (int16_t)floorf(center_y + 30.0f);
                lines[1].x = lines[0].x;
                lines[1].y = (int16_t)floorf(center_y + 50.0f);
                lines[2].x = (int16_t)floorf(center_x - 2.0f);
                lines[2].y = lines[1].y;
                for (line = 0U; line < 3U; ++line) {
                    lines[line].has_authored_position = 1U;
                    lines[line].has_authored_color = 1U;
                    lines[line].red = 235U;
                    lines[line].green = 216U;
                    lines[line].blue = 121U;
                }
            } else if (!snapshot.erase_pending) {
                int32_t folder;
                size_t caption = 0U;
                for (folder = FOLDER1; folder < MAX_FOLDER_COUNT; ++folder) {
                    const char *chapter_number;
                    const char *part_number;
                    const int32_t difficulty =
                        snapshot.folder_difficulty[folder];
                    if (!snapshot.folder_has_progress[folder]
                            || difficulty < DIFFICULTY_AGENT
                            || difficulty > DIFFICULTY_007) continue;
                    if (line_count >= GE_ORIGINAL_FRONTEND_MAX_LINES)
                        return false;
                    (void)snprintf(wallet_caption[caption],
                        sizeof(wallet_caption[caption]), "%s\n",
                        original_frontend_text(getStringID(LTITLE,
                            TITLE_STR_19_AGENT + difficulty)));
                    {
                        Ge3dsOriginalFrontendLine *caption_line =
                            &lines[line_count++];
                        memset(caption_line, 0, sizeof(*caption_line));
                        caption_line->text = wallet_caption[caption++];
                        caption_line->x = (int16_t)lrintf(
                            runtime->wallet_centers[folder][0]);
                        caption_line->y = (int16_t)floorf(
                            runtime->wallet_centers[folder][1] + 21.0f);
                        caption_line->horizontal_align = CENTER_ALIGN;
                        caption_line->has_authored_position = 1U;
                        caption_line->has_authored_color = 1U;
                        caption_line->red = 235U;
                        caption_line->green = 216U;
                        caption_line->blue = 121U;
                    }
                    if (difficulty == DIFFICULTY_007
                            || !ge_original_frontend_start_mission_caption(
                                snapshot.folder_mission[folder],
                                &chapter_number,&part_number)) continue;
                    if (line_count >= GE_ORIGINAL_FRONTEND_MAX_LINES
                            || caption >= 8U) return false;
                    (void)snprintf(wallet_caption[caption],
                        sizeof(wallet_caption[caption]), "%s%s.%s\n",
                        original_frontend_text(getStringID(
                            LTITLE,TITLE_STR_26_MISSION)),
                        chapter_number,part_number);
                    {
                        Ge3dsOriginalFrontendLine *caption_line =
                            &lines[line_count++];
                        memset(caption_line, 0, sizeof(*caption_line));
                        caption_line->text = wallet_caption[caption++];
                        caption_line->x = (int16_t)lrintf(
                            runtime->wallet_centers[folder][0]);
                        caption_line->y = (int16_t)floorf(
                            runtime->wallet_centers[folder][1] + 45.0f);
                        caption_line->horizontal_align = CENTER_ALIGN;
                        caption_line->has_authored_position = 1U;
                        caption_line->has_authored_color = 1U;
                        caption_line->red = 235U;
                        caption_line->green = 216U;
                        caption_line->blue = 121U;
                    }
                }
            }
        }
        if (snapshot.chapter_number != NULL
                && snapshot.part_number != NULL
                && (snapshot.menu == MENU_DIFFICULTY
                    || snapshot.menu == MENU_007_OPTIONS
                    || snapshot.menu == MENU_BRIEFING
                    || snapshot.menu == MENU_MISSION_FAILED
                    || snapshot.menu == MENU_MISSION_COMPLETE)) {
            const char *difficulty_text = original_frontend_text(
                snapshot.difficulty_title);
            const char *chapter_text = original_frontend_text(
                snapshot.chapter_title);
            const char *part_text = original_frontend_text(
                snapshot.part_title);
            (void)snprintf(mission_header[0], sizeof(mission_header[0]),
                "%s%s", difficulty_text != NULL ? difficulty_text : "",
                original_frontend_text(
                    getStringID(LTITLE, TITLE_STR_32_JB)));
            (void)snprintf(mission_header[1], sizeof(mission_header[1]),
                "%s%s: %s\n",
                original_frontend_text(
                    getStringID(LTITLE, TITLE_STR_33_MISSION2)),
                snapshot.chapter_number,
                chapter_text != NULL ? chapter_text : "");
            (void)snprintf(mission_header[2], sizeof(mission_header[2]),
                "%s%s: %s\n",
                original_frontend_text(
                    getStringID(LTITLE, TITLE_STR_34_PART)),
                snapshot.part_number,
                part_text != NULL ? part_text : "");
            if (snapshot.menu != MENU_DIFFICULTY && line_count > 0U)
                lines[0].text = "";
            for (line = 0U; line < 3U
                    && line_count < GE_ORIGINAL_FRONTEND_MAX_LINES; ++line) {
                Ge3dsOriginalFrontendLine *header = &lines[line_count++];
                memset(header, 0, sizeof(*header));
                header->text = mission_header[line];
                header->x = 55;
                header->y = (int16_t)(87 + (int)line * 16);
                header->has_authored_position = 1U;
            }
        }
        if (snapshot.menu == MENU_MISSION_COMPLETE) {
            if (snapshot.result.new_cheat_unlocked
                    && line_count < GE_ORIGINAL_FRONTEND_MAX_LINES) {
                Ge3dsOriginalFrontendLine *notice = &lines[line_count++];
                memset(notice, 0, sizeof(*notice));
                (void)snprintf(statistics_supplemental[0],
                    sizeof(statistics_supplemental[0]), "     [%s]",
                    original_frontend_text(getStringID(
                        LTITLE, TITLE_STR_275_NEWCHEATAVAILABLE)));
                notice->text = statistics_supplemental[0];
                notice->x = 130;
                notice->y = 167;
                notice->has_authored_position = 1U;
                notice->has_authored_color = 1U;
                notice->red = 160U;
            }
            if ((snapshot.result.target_time_seconds > 0
                        && snapshot.difficulty != DIFFICULTY_007)
                    || snapshot.result.best_time_seconds >= 0) {
                Ge3dsOriginalFrontendLine *time_line;
                const bool show_target =
                    snapshot.result.target_time_seconds > 0
                    && snapshot.difficulty != DIFFICULTY_007;
                if (line_count >= GE_ORIGINAL_FRONTEND_MAX_LINES)
                    return false;
                time_line = &lines[line_count++];
                memset(time_line, 0, sizeof(*time_line));
                time_line->text = original_frontend_text(getStringID(
                    LTITLE, show_target ? TITLE_STR_274_TARGET
                        : TITLE_STR_273_BESTTIME));
                if (show_target) {
                    (void)snprintf(statistics_supplemental[1],
                        sizeof(statistics_supplemental[1]), "%02ld:%02ld",
                        (long)(snapshot.result.target_time_seconds / 60),
                        (long)(snapshot.result.target_time_seconds % 60));
                    if (snapshot.result.best_time_seconds >= 0) {
                        const size_t used = strlen(statistics_supplemental[1]);
                        (void)snprintf(statistics_supplemental[1] + used,
                            sizeof(statistics_supplemental[1]) - used,
                            "     (%s  %02ld:%02ld)",
                            original_frontend_text(getStringID(
                                LTITLE, TITLE_STR_273_BESTTIME)),
                            (long)(snapshot.result.best_time_seconds / 60),
                            (long)(snapshot.result.best_time_seconds % 60));
                    }
                } else {
                    (void)snprintf(statistics_supplemental[1],
                        sizeof(statistics_supplemental[1]), "%02ld:%02ld",
                        (long)(snapshot.result.best_time_seconds / 60),
                        (long)(snapshot.result.best_time_seconds % 60));
                }
                time_line->value_text = statistics_supplemental[1];
                time_line->x = 55;
                time_line->value_x = 130;
                time_line->y = 181;
                time_line->has_authored_position = 1U;
            }
        }
        for (line = 0U; line < snapshot.tab_count
                && line_count < GE_ORIGINAL_FRONTEND_MAX_LINES; ++line) {
            Ge3dsOriginalFrontendLine *tab_line=&lines[line_count++];
            memset(tab_line,0,sizeof(*tab_line));
            tab_line->text=original_frontend_text(
                snapshot.tabs[line].text_id);
            tab_line->selected=snapshot.tabs[line].selected;
            tab_line->tab=snapshot.tabs[line].status;
        }
        if (snapshot.menu == MENU_DISPLAY_CAST
                && runtime->cast_started) {
            static const int16_t cast_y[3] = {108, 152, 174};
            const size_t first = runtime->cast_frame.full_actor_intro
                ? 1U : 0U;
            line_count = 0U;
            for (line = first; line < 3U; ++line) {
                Ge3dsOriginalFrontendLine *cast_line =
                    &lines[line_count++];
                memset(cast_line, 0, sizeof(*cast_line));
                cast_line->text = original_frontend_text(
                    runtime->cast_frame.selection.text_id[line]);
                cast_line->x = 315;
                cast_line->y = cast_y[line];
                cast_line->horizontal_align = 1U;
                cast_line->has_authored_position = 1U;
            }
        }
        if (!ge_3ds_original_frontend_build_draw_list_exact(
                &original_hud_atlas, &original_gameplay_hud_atlas,
                original_frontend_page(snapshot.menu),
                lines, line_count, &draw_list))
            return false;
        if (snapshot.menu == MENU_DISPLAY_CAST) {
            const float fade = runtime->cast_frame.fade;
            const size_t glyph_end = draw_list.box_vertex_count
                + draw_list.glyph_vertex_count;
            size_t vertex;
            for (vertex = draw_list.box_vertex_count;
                    vertex < glyph_end; ++vertex)
                draw_list.vertices[vertex].a *= fade;
        }
        if (runtime->save_provider != NULL
                && snapshot.menu == MENU_DIFFICULTY) {
            int32_t difficulty;
            for (difficulty = DIFFICULTY_AGENT;
                    difficulty <= DIFFICULTY_00; ++difficulty)
                if (ge_3ds_save_provider_stage_status(
                        runtime->save_provider, snapshot.mission,
                        difficulty) == STAGESTATUS_COMPLETED)
                    completed_difficulties |= (uint8_t)(1U << difficulty);
        }
        if (!ge_3ds_original_frontend_build_sprite_list(
                original_frontend_page(snapshot.menu),
                completed_difficulties,
                &sprite_list))
            return false;
        if (((snapshot.menu >= MENU_FILE_SELECT
                        && snapshot.menu <= MENU_MISSION_COMPLETE)
                    || snapshot.menu == MENU_007_OPTIONS)
                && !(snapshot.menu == MENU_FILE_SELECT
                    && snapshot.erase_pending)
                && !ge_3ds_original_frontend_append_cursor_sprite(
                    &sprite_list,snapshot.cursor.x,snapshot.cursor.y,
                    snapshot.menu==MENU_FILE_SELECT
                        ?snapshot.file_action:0U))
            return false;
        sprite_vertex_count = append_original_frontend_sprite_vertices(
            &draw_list, &sprite_list);
        if (snapshot.presentation.renderer
                    == GE_ORIGINAL_FRONTEND_RENDERER_PITEM_MODEL
                && snapshot.presentation.model_prop >= 0) {
            if (!prepare_original_frontend_pitem_scene(
                    runtime, snapshot.presentation.model_prop,
                    &snapshot.presentation, texture_cache,
                    (Vertex *)vertex_buffer
                        + ORIGINAL_FRONTEND_MODEL_VERTEX_OFFSET))
                return false;
        } else if (!snapshot.presentation.startup_active
                && snapshot.menu != MENU_GOLDENEYE_LOGO
                && snapshot.menu != MENU_DISPLAY_CAST
                && snapshot.menu != MENU_SWITCH_SCREENS
                && !prepare_original_frontend_wallet(
                    runtime, &snapshot, texture_cache,
                    (Vertex *)vertex_buffer
                        + ORIGINAL_FRONTEND_MODEL_VERTEX_OFFSET))
            return false;
        if (snapshot.presentation.renderer
                    == GE_ORIGINAL_FRONTEND_RENDERER_RAREWARE)
            (void)prepare_original_frontend_rareware(
                runtime, &snapshot.presentation);
        draw_original_frontend_list(top_target, runtime, snapshot.menu,
            &snapshot.presentation,
            original_frontend_page(snapshot.menu), &draw_list,
            &sprite_list, sprite_vertex_count);
        if (snapshot.menu == MENU_RAREWARE_LOGO
                && snapshot.presentation.duration_frames != 0U
                && snapshot.presentation.frame
                    >= snapshot.presentation.duration_frames)
            (void)ge_original_frontend_start_sequence_complete(frontend);
        if (snapshot.menu == MENU_EYE_INTRO
                && gunbarrel_result
                    == GE_ORIGINAL_GUNBARREL_TICK_COMPLETE)
            (void)ge_original_frontend_start_sequence_complete(frontend);
        runtime->previous_menu = snapshot.menu;
        gspWaitForVBlank();
    }
    return false;
}

static bool run_original_mission_complete_report(
    C3D_RenderTarget *top_target, bool cstick_available,
    GeTextureCache *texture_cache,
    RuntimeOriginalFrontend *runtime,
    GeOriginalFrontendStart *frontend,
    RuntimeStageOrdinaryObjects *stage_objects,
    const GeOriginalMissionResultSnapshot *result_snapshot,
    GeOriginalMusicRuntime **music_runtime,
    GeAudioOutput *audio_output,
    bool audio_active,
    int32_t *selected_level_id)
{
    GeOriginalFrontendMissionResult result = {0};
    GeOriginalPlayerCombatSnapshot combat = {0};
    GeOriginalFrontendStatistics statistics = {0};
    GeOriginalMissionOutcomeInput outcome_input = {0};
    GeOriginalMissionOutcome outcome = {0};
    GeOriginalFrontendHeldWeapon
        held[GE_ORIGINAL_FRONTEND_HELD_WEAPON_COUNT];
    int32_t shot_register[GE_ORIGINAL_FRONTEND_SHOT_REGISTER_COUNT];
    int32_t kill_count;
    size_t objective;
    if (top_target == NULL || texture_cache == NULL
            || runtime == NULL || frontend == NULL
            || stage_objects == NULL || result_snapshot == NULL
            || selected_level_id == NULL) return false;
    ge_original_dam_guard_player_combat_snapshot(&combat);
    if (!ge_original_bond_live_statistics_state(
            shot_register, &kill_count, held)
            || !ge_original_frontend_statistics_snapshot(
                shot_register, kill_count, held, &statistics))
        return false;
    outcome_input.difficulty = result_snapshot->difficulty;
    outcome_input.mission_failed_or_aborted = mission_failed_or_aborted;
    outcome_input.bond_kia = combat.dead;
    result.mission_time_ticks = getMissiontimer();
    result.shots_fired = statistics.shot_register[0];
    result.head_hits = statistics.shot_register[1];
    result.body_hits = statistics.shot_register[2];
    result.limb_hits = statistics.shot_register[3];
    result.gun_hits = statistics.shot_register[4];
    result.hat_hits = statistics.shot_register[5];
    result.object_hits = statistics.shot_register[6];
    result.kill_count = statistics.kill_count;
    result.favorite_weapon_right = statistics.favorite_weapon_right;
    result.favorite_weapon_left = statistics.favorite_weapon_left;
    result.favorite_weapon_dual = statistics.favorite_weapon_dual;
    result.new_cheat_unlocked =
        (uint8_t)(result_snapshot->new_cheat_unlocked != 0);
    result.target_time_seconds = result_snapshot->cheat_target_seconds;
    result.best_time_seconds = runtime->save_provider != NULL
        ? ge_3ds_save_provider_stage_time(runtime->save_provider,
            result_snapshot->mission, result_snapshot->difficulty) : -1;
    if (result.best_time_seconds <= 0
            || result.best_time_seconds >= 0x3ff)
        result.best_time_seconds = -1;
    for (objective = 0U;
            objective < GE_ORIGINAL_FRONTEND_MAX_OBJECTIVES; ++objective) {
        GeOriginalStageObjectiveEvaluation evaluation;
        const GeOriginalStageObjectiveRegistry *registry =
            stage_objects->objective_runtime.registry;
        int32_t objective_index;
        if (registry == NULL) return false;
        objective_index = registry->objective_by_menu[objective];
        if (objective_index < 0) continue;
        if ((size_t)objective_index >= registry->objective_entry_count
                || ge_original_stage_objective_runtime_evaluate(
                    &stage_objects->objective_runtime, (uint8_t)objective,
                    &evaluation) != GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK)
            return false;
        outcome_input.objectives[objective].text_id =
            registry->objectives[objective_index].text_id;
        outcome_input.objectives[objective].enabled_difficulty =
            (uint16_t)(uint8_t)
                registry->objectives[objective_index].difficulty;
        outcome_input.objectives[objective].status = evaluation.value;
        result.objective_status[objective] = evaluation.value;
    }
    if (!ge_original_mission_outcome_evaluate_exact(
            &outcome_input, &outcome)) return false;
    result.bond_kia = (uint8_t)(outcome.status
        == GE_ORIGINAL_MISSION_OUTCOME_KIA);
    result.all_objectives_complete_alive =
        outcome.all_objectives_complete_alive;
    result.mission_failed_or_aborted = (uint8_t)(outcome.status
        == GE_ORIGINAL_MISSION_OUTCOME_ABORTED);
    if (!ge_original_frontend_start_stage_ended(frontend, &result))
        return false;
    /* Continue the same source-pinned frontend state through Report,
     * Statistics, mission select and the authentic retry path. */
    return run_original_frontend(top_target, cstick_available, texture_cache,
        runtime, frontend, music_runtime, audio_output, audio_active,
        false, selected_level_id);
}

static bool renderer_init(GeAssetPack *asset_pack,
                          GeTextureCache *texture_cache,
                          const GeStageAssetDescriptor *stage_assets,
                          const GeOriginalStageSpawn *stage_spawn,
                          RuntimeGbiMesh *rareware_mesh,
                          RuntimeBlotterPreview *blotter_preview,
                          RuntimeDamPreview *dam_preview)
{
    float stage_fog_values[GE_ORIGINAL_STAGE_ENVIRONMENT_LUT_SIZE];
    C3D_AttrInfo *attributes;
    C3D_BufInfo *buffers;
    C3D_TexEnv *texture_environment;

    shader_dvlb = DVLB_ParseFile((u32 *)vshader_shbin, vshader_shbin_size);
    if (shader_dvlb == NULL) {
        return false;
    }

    shaderProgramInit(&shader_program);
    shaderProgramSetVsh(&shader_program, &shader_dvlb->DVLE[0]);
    C3D_BindProgram(&shader_program);
    C3D_CullFace(GPU_CULL_NONE);
    projection_uniform = shaderInstanceGetUniformLocation(shader_program.vertexShader, "projection");

    attributes = C3D_GetAttrInfo();
    AttrInfo_Init(attributes);
    AttrInfo_AddLoader(attributes, 0, GPU_FLOAT, 3);
    AttrInfo_AddLoader(attributes, 1, GPU_FLOAT, 2);
    AttrInfo_AddLoader(attributes, 2, GPU_FLOAT, 4);

    Mtx_OrthoTilt(&projection, 0.0f, 400.0f, 0.0f, 240.0f, 0.0f, 1.0f, true);

    vertex_buffer = linearAlloc(TOTAL_VERTEX_COUNT * sizeof(Vertex));
    if (vertex_buffer == NULL) {
        shaderProgramFree(&shader_program);
        DVLB_Free(shader_dvlb);
        return false;
    }
    memset(vertex_buffer, 0, CROSSHAIR_VERTEX_COUNT * sizeof(Vertex));
    memset((Vertex *)vertex_buffer + CROSSHAIR_VERTEX_COUNT, 0, ICON_VERTEX_COUNT * sizeof(Vertex));
    memcpy((Vertex *)vertex_buffer + DAM_ENVIRONMENT_VERTEX_OFFSET,
           dam_environment_vertices, sizeof(dam_environment_vertices));
    if (dam_preview != NULL && dam_preview->environment_ready) {
        Vertex *environment_vertices =
            (Vertex *)vertex_buffer + DAM_ENVIRONMENT_VERTEX_OFFSET;
        size_t environment_vertex;
        for (environment_vertex = 0U;
                environment_vertex < DAM_ENVIRONMENT_VERTEX_COUNT;
                ++environment_vertex) {
            environment_vertices[environment_vertex].r =
                (float)dam_preview->environment.red / 255.0f;
            environment_vertices[environment_vertex].g =
                (float)dam_preview->environment.green / 255.0f;
            environment_vertices[environment_vertex].b =
                (float)dam_preview->environment.blue / 255.0f;
        }
    }
    memset((Vertex *)vertex_buffer + DAM_CLOUD_VERTEX_OFFSET, 0,
           DAM_CLOUD_VERTEX_CAPACITY * sizeof(Vertex));
    load_copy_icon(texture_cache, vertex_buffer);
    if (asset_pack != NULL) {
        Tex3DS_SubTexture cloud_subtexture;

        dam_cloud_texture_loaded = import_packed_texture(
            asset_pack,
            "converted/textures/t3x/CLOUDS_GRAYSCALE-0.t3x",
            &dam_cloud_texture, &cloud_subtexture);
        if (dam_cloud_texture_loaded) {
            C3D_TexSetWrap(&dam_cloud_texture, GPU_REPEAT, GPU_REPEAT);
            C3D_TexSetFilter(&dam_cloud_texture, GPU_LINEAR, GPU_LINEAR);
        }
    }
    autogun_beam_texture_loaded = import_cached_texture(
        texture_cache, GE_3DS_ORIGINAL_AUTOGUN_BEAM_TEXTURE_SOURCE,
        &autogun_beam_texture, &autogun_beam_subtexture);
    if (autogun_beam_texture_loaded) {
        C3D_TexSetWrap(&autogun_beam_texture, GPU_CLAMP_TO_EDGE,
                       GPU_CLAMP_TO_EDGE);
        C3D_TexSetFilter(&autogun_beam_texture, GPU_LINEAR, GPU_LINEAR);
    }
    if (rareware_mesh != NULL) {
        rareware_mesh->textured = load_rareware_textures(asset_pack,
                                                         rareware_mesh);
    }
    if (rareware_mesh != NULL && rareware_mesh->loaded) {
        memcpy((Vertex *)vertex_buffer + RAREWARE_VERTEX_OFFSET,
               rareware_mesh->vertices,
               rareware_mesh->vertex_count * sizeof(Vertex));
    }
    memset((Vertex *)vertex_buffer + RAREWARE_FRONT_VERTEX_OFFSET, 0,
           RAREWARE_FRONT_VERTEX_CAPACITY * sizeof(Vertex));
    memset((Vertex *)vertex_buffer + RAREWARE_BODY_VERTEX_OFFSET, 0,
           RAREWARE_BODY_VERTEX_CAPACITY * sizeof(Vertex));
    memset((Vertex *)vertex_buffer + BLOTTER_VERTEX_OFFSET, 0,
           BLOTTER_VERTEX_COUNT * sizeof(Vertex));
    memset((Vertex *)vertex_buffer + DAM_ROOM_VERTEX_OFFSET, 0,
           DAM_ROOM_VERTEX_COUNT * sizeof(Vertex));
    memset((Vertex *)vertex_buffer + FIRST_PERSON_VERTEX_OFFSET, 0,
           FIRST_PERSON_VERTEX_CAPACITY * sizeof(Vertex));
    memset((Vertex *)vertex_buffer + AUTOGUN_BEAM_VERTEX_OFFSET, 0,
           AUTOGUN_BEAM_VERTEX_CAPACITY * sizeof(Vertex));
    memset((Vertex *)vertex_buffer + GUARD_MUZZLE_FLASH_VERTEX_OFFSET, 0,
           GUARD_MUZZLE_FLASH_VERTEX_CAPACITY * sizeof(Vertex));
    memset((Vertex *)vertex_buffer + FADE_OVERLAY_VERTEX_OFFSET, 0,
           FADE_OVERLAY_VERTEX_COUNT * sizeof(Vertex));
    memset((Vertex *)vertex_buffer + ORIGINAL_HUD_VERTEX_OFFSET, 0,
           ORIGINAL_HUD_VERTEX_CAPACITY * sizeof(Vertex));
    memset((Vertex *)vertex_buffer + ORIGINAL_GAMEPLAY_HUD_SOLID_VERTEX_OFFSET,
           0, GE_3DS_ORIGINAL_GAMEPLAY_HUD_SOLID_VERTEX_CAPACITY
                * sizeof(Vertex));
    memset((Vertex *)vertex_buffer + ORIGINAL_GAMEPLAY_HUD_FONT_VERTEX_OFFSET,
           0, GE_3DS_ORIGINAL_GAMEPLAY_HUD_FONT_VERTEX_CAPACITY
                * sizeof(Vertex));
    memset((Vertex *)vertex_buffer + ORIGINAL_BOTTOM_HUD_VERTEX_OFFSET,
           0, ORIGINAL_HUD_VERTEX_CAPACITY * sizeof(Vertex));
    memset((Vertex *)vertex_buffer + ORIGINAL_AMMO_ICON_VERTEX_OFFSET,
           0, ORIGINAL_AMMO_ICON_VERTEX_CAPACITY * sizeof(Vertex));
    memset((Vertex *)vertex_buffer + ORIGINAL_FRONTEND_MODEL_VERTEX_OFFSET,
           0, ORIGINAL_FRONTEND_MODEL_VERTEX_CAPACITY * sizeof(Vertex));
    if (ge_3ds_original_hud_build_atlas(&original_hud_atlas)
            && import_original_font_texture(
                &original_hud_atlas, &original_hud_font_texture)) {
        original_hud_font_texture_loaded = true;
    }
    if (ge_3ds_original_hud_build_bank_gothic_atlas(
            &original_gameplay_hud_atlas)
            && import_original_font_texture(
                &original_gameplay_hud_atlas,
                &original_gameplay_hud_font_texture)) {
        original_gameplay_hud_font_texture_loaded = true;
    }
    load_original_ammo_icon_textures(texture_cache);
    gun_sight_texture_loaded = import_cached_texture(texture_cache,
        ge_original_gun_sight_texture_source(),
        &gun_sight_texture, &gun_sight_subtexture);
    if (gun_sight_texture_loaded) {
        C3D_TexSetFilter(&gun_sight_texture, GPU_LINEAR, GPU_LINEAR);
        C3D_TexSetWrap(&gun_sight_texture, GPU_REPEAT, GPU_REPEAT);
    }
    gun_sight_frames = gun_sight_visible_frames = gun_sight_failures = 0U;
    load_original_frontend_sprite_textures(texture_cache);
    if (blotter_preview != NULL) {
        *blotter_preview = load_blotter_preview(asset_pack, texture_cache);
        if (blotter_preview->loaded) {
            memcpy((Vertex *)vertex_buffer + BLOTTER_VERTEX_OFFSET,
                   blotter_preview->vertices,
                   BLOTTER_VERTEX_COUNT * sizeof(Vertex));
        }
    }
    if (dam_preview != NULL) {
        load_dam_room_preview(asset_pack, texture_cache, stage_assets,
                              stage_spawn,
                              dam_preview,
                              (Vertex *)vertex_buffer + DAM_ROOM_VERTEX_OFFSET);
        initialize_original_dam_camera(
            dam_preview, stage_spawn,
            (Vertex *)vertex_buffer + DAM_ROOM_VERTEX_OFFSET);
    }
    GSPGPU_FlushDataCache(vertex_buffer, TOTAL_VERTEX_COUNT * sizeof(Vertex));

    buffers = C3D_GetBufInfo();
    BufInfo_Init(buffers);
    BufInfo_Add(buffers, vertex_buffer, sizeof(Vertex), 3, 0x210);

    texture_environment = C3D_GetTexEnv(0);
    C3D_TexEnvInit(texture_environment);
    C3D_TexEnvSrc(texture_environment, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
    C3D_TexEnvFunc(texture_environment, C3D_Both, GPU_REPLACE);
    if (dam_preview != NULL && dam_preview->environment_ready
            && ge_original_stage_environment_build_fog_lut(
                &dam_preview->environment, stage_fog_values))
        FogLut_FromArray(&dam_environment_fog_lut, stage_fog_values);
    return true;
}

static void renderer_draw(const RuntimeGbiMesh *rareware_mesh,
                          const RuntimeBlotterPreview *blotter_preview,
                          const RuntimeDamPreview *dam_preview,
                          const RuntimeStageOrdinaryObjects *stage_objects,
                          const RuntimeFirstPersonScene *first_person,
                          const GeOriginalDamMissionExitSnapshot *fade_snapshot)
{
    GePicaTextureRectangleDraw gun_sight_draw = {0};
    size_t gun_sight_vertex_count = 0U;
    Vertex *vertices = vertex_buffer;
    C3D_TexEnv *texture_environment = C3D_GetTexEnv(0);
    GeDamSkyScene sky_scene;
    Ge3dsFadeOverlay fade_overlay = {0};
    GeOriginalDamMissionHudRenderSnapshot mission_hud = {0};
    static Ge3dsOriginalHudDrawList mission_hud_draw;
    GeOriginalBottomHudRenderSnapshot bottom_hud = {0};
    static Ge3dsOriginalHudDrawList bottom_hud_draw;
    GeOriginalGameplayHudRenderSnapshot gameplay_hud = {0};
    Ge3dsOriginalGameplayHudDrawList gameplay_hud_draw = {0};
    GeOriginalBondMotionSnapshot bond_motion = {0};
    Ge3dsOriginalWatchObjectiveLine watch_objective_lines[
        GE_ORIGINAL_STAGE_OBJECTIVE_MAX];
    size_t watch_objective_line_count = 0U;
    bool watch_open = false;
    bool watch_objectives_visible = false;
    bool mission_hud_uses_bank_gothic = false;
    RuntimeAmmoIconTexture *right_ammo_icon = NULL;
    RuntimeAmmoIconTexture *left_ammo_icon = NULL;
    RuntimeRendererMaterialCache world_material_cache = {0};
    RuntimeRendererMaterialCache first_person_material_cache = {0};
    static RuntimeRendererPreparedMaterialCache world_prepared_materials;
    static RuntimeRendererPreparedMaterialCache first_person_prepared_materials;
    static uint8_t
        world_batch_visibility_cache[DAM_SCENE_PROJECTED_VERTEX_CAPACITY];
    size_t sky_vertex_count = 0U;
    size_t mission_hud_vertex_count = 0U;
    size_t bottom_hud_vertex_count = 0U;
    size_t ammo_icon_vertex_count = 0U;
    size_t right_ammo_icon_vertex_count = 0U;
    size_t left_ammo_icon_vertex_count = 0U;
    size_t autogun_beam_vertex_count = 0U;
    size_t guard_muzzle_flash_vertex_count = 0U;
    GeOriginalCreditsRenderSnapshot credits_snapshot = {0};
    Ge3dsOriginalCreditsLine credits_lines[
        GE_ORIGINAL_CREDITS_VISIBLE_LINE_CAPACITY];
    bool credits_active = false;
    size_t i;

    (void)rareware_mesh;
    (void)blotter_preview;
    autogun_beam_vertex_count = update_stage_autogun_beam_vertices(
        stage_objects, dam_preview,
        vertices + AUTOGUN_BEAM_VERTEX_OFFSET);
    guard_muzzle_flash_vertex_count=update_stage_guard_muzzle_flash_vertices(
        stage_objects,dam_preview,
        vertices+GUARD_MUZZLE_FLASH_VERTEX_OFFSET);
    watch_open = ge_original_bond_live_motion_snapshot(&bond_motion)
        && (bond_motion.watch_animation_state == WATCH_ANIMATION_0x5
            || bond_motion.watch_animation_state == WATCH_ANIMATION_0xc);
    watch_objectives_visible = watch_open
        && ge_original_bond_live_watch_objectives_visible();
    if (g_CurrentPlayer != NULL) {
        /* Canonical maybe_mp_interface ordering advances the lower queue
         * before the upper queue, once per displayed frame. */
        if (watch_objectives_visible) {
            const int objective_count =
                ge_original_stage_objective_live_count();
            const int selected_difficulty = lvlGetSelectedDifficulty();
            int menu;
            for (menu = 0; menu < objective_count
                    && watch_objective_line_count
                        < GE_ORIGINAL_STAGE_OBJECTIVE_MAX; ++menu) {
                GeOriginalStageObjectiveEvaluation evaluation;
                int8_t difficulty;
                uint16_t text_id;
                const char *objective_text;
                const char *status_text;
                if (!ge_original_stage_objective_live_difficulty(
                        (uint8_t)menu, &difficulty)
                        || difficulty > selected_difficulty
                        || !ge_original_stage_objective_live_text_id(
                            (uint8_t)menu, &text_id)
                        || ge_original_stage_objective_live_evaluate(
                            (uint8_t)menu, &evaluation)
                            != GE_ORIGINAL_STAGE_OBJECTIVE_RUNTIME_OK)
                    continue;
                objective_text = ge_original_language_text(text_id);
                if (objective_text == NULL) continue;
                switch (evaluation.value) {
                case GE_ORIGINAL_STAGE_OBJECTIVE_COMPLETE:
                    status_text = ge_original_language_text_by_bank(
                        LMISC, 0x2dU);
                    break;
                case GE_ORIGINAL_STAGE_OBJECTIVE_FAILED:
                    status_text = ge_original_language_text_by_bank(
                        LMISC, 0x2fU);
                    break;
                default:
                    status_text = ge_original_language_text_by_bank(
                        LMISC, 0x2eU);
                    break;
                }
                watch_objective_lines[watch_objective_line_count++] =
                    (Ge3dsOriginalWatchObjectiveLine){
                        objective_text, status_text, (uint8_t)menu,
                        evaluation.value,
                    };
            }
            if (original_gameplay_hud_font_texture_loaded
                    && ge_3ds_original_watch_objectives_build_draw_list(
                        &original_gameplay_hud_atlas,
                        watch_objective_lines, watch_objective_line_count,
                        &mission_hud_draw)
                    && mission_hud_draw.visible) {
                mission_hud_uses_bank_gothic = true;
                mission_hud_vertex_count = mission_hud_draw.box_vertex_count
                    + mission_hud_draw.glyph_vertex_count;
                memcpy(vertices + ORIGINAL_HUD_VERTEX_OFFSET,
                       mission_hud_draw.vertices,
                       mission_hud_vertex_count * sizeof(Vertex));
                GSPGPU_FlushDataCache(
                    vertices + ORIGINAL_HUD_VERTEX_OFFSET,
                    renderer_vertex_flush_bytes(
                        mission_hud_vertex_count,
                        ORIGINAL_HUD_VERTEX_CAPACITY));
            }
        } else if (!watch_open) {
            ge_original_bottom_hud_tick();
            (void)ge_original_bottom_hud_render_snapshot(&bottom_hud);
            if (original_gameplay_hud_font_texture_loaded
                    && ge_3ds_original_bottom_hud_build_draw_list(
                        &original_gameplay_hud_atlas, &bottom_hud,
                        &bottom_hud_draw)
                    && bottom_hud_draw.visible) {
                bottom_hud_vertex_count = bottom_hud_draw.box_vertex_count
                    + bottom_hud_draw.glyph_vertex_count;
                memcpy(vertices + ORIGINAL_BOTTOM_HUD_VERTEX_OFFSET,
                       bottom_hud_draw.vertices,
                       bottom_hud_vertex_count * sizeof(Vertex));
                GSPGPU_FlushDataCache(
                    vertices + ORIGINAL_BOTTOM_HUD_VERTEX_OFFSET,
                    renderer_vertex_flush_bytes(
                        bottom_hud_vertex_count,
                        ORIGINAL_HUD_VERTEX_CAPACITY));
            }
            ge_original_dam_mission_hud_tick();
            (void)ge_original_dam_mission_hud_render_snapshot(&mission_hud);
            if (original_hud_font_texture_loaded
                    && ge_3ds_original_hud_build_draw_list(
                        &original_hud_atlas, &mission_hud, &mission_hud_draw)
                    && mission_hud_draw.visible) {
            mission_hud_vertex_count = mission_hud_draw.box_vertex_count
                + mission_hud_draw.glyph_vertex_count;
            memcpy(vertices + ORIGINAL_HUD_VERTEX_OFFSET,
                   mission_hud_draw.vertices,
                   mission_hud_vertex_count * sizeof(Vertex));
            GSPGPU_FlushDataCache(
                vertices + ORIGINAL_HUD_VERTEX_OFFSET,
                renderer_vertex_flush_bytes(
                    mission_hud_vertex_count,
                    ORIGINAL_HUD_VERTEX_CAPACITY));
        }
        }
        if (!watch_open)
            (void)ge_original_gameplay_hud_render_snapshot(&gameplay_hud);
        if (gameplay_hud.ammo_visible) {
            right_ammo_icon = find_original_ammo_icon_texture(
                gameplay_hud.icon_image);
        }
        if (right_ammo_icon != NULL) {
            set_original_ammo_icon_vertices(
                vertices + ORIGINAL_AMMO_ICON_VERTEX_OFFSET,
                &right_ammo_icon->subtexture,
                (float)gameplay_hud.icon_x,
                (float)gameplay_hud.icon_y,
                (float)right_ammo_icon->asset->width,
                (float)right_ammo_icon->asset->height);
            right_ammo_icon_vertex_count = 6U;
            ammo_icon_vertex_count += right_ammo_icon_vertex_count;
        }
        if (gameplay_hud.left_ammo_visible) {
            left_ammo_icon = find_original_ammo_icon_texture(
                gameplay_hud.left_icon_image);
        }
        if (left_ammo_icon != NULL) {
            set_original_ammo_icon_vertices(
                vertices + ORIGINAL_AMMO_ICON_VERTEX_OFFSET
                    + ammo_icon_vertex_count,
                &left_ammo_icon->subtexture,
                (float)gameplay_hud.left_icon_x,
                (float)gameplay_hud.left_icon_y,
                (float)left_ammo_icon->asset->width,
                (float)left_ammo_icon->asset->height);
            left_ammo_icon_vertex_count = 6U;
            ammo_icon_vertex_count += left_ammo_icon_vertex_count;
        }
        if (ammo_icon_vertex_count != 0U) {
            GSPGPU_FlushDataCache(
                vertices + ORIGINAL_AMMO_ICON_VERTEX_OFFSET,
                renderer_vertex_flush_bytes(
                    ammo_icon_vertex_count,
                    ORIGINAL_AMMO_ICON_VERTEX_CAPACITY));
        }
        if (original_gameplay_hud_font_texture_loaded
                && ge_3ds_original_gameplay_hud_build_draw_list(
                    &original_gameplay_hud_atlas, &gameplay_hud,
                    &gameplay_hud_draw)) {
            if (gameplay_hud_draw.solid_vertex_count != 0U) {
                memcpy(vertices + ORIGINAL_GAMEPLAY_HUD_SOLID_VERTEX_OFFSET,
                       gameplay_hud_draw.solid_vertices,
                       gameplay_hud_draw.solid_vertex_count * sizeof(Vertex));
                GSPGPU_FlushDataCache(
                    vertices + ORIGINAL_GAMEPLAY_HUD_SOLID_VERTEX_OFFSET,
                    renderer_vertex_flush_bytes(
                        gameplay_hud_draw.solid_vertex_count,
                        GE_3DS_ORIGINAL_GAMEPLAY_HUD_SOLID_VERTEX_CAPACITY));
            }
            if (gameplay_hud_draw.font_vertex_count != 0U) {
                memcpy(vertices + ORIGINAL_GAMEPLAY_HUD_FONT_VERTEX_OFFSET,
                       gameplay_hud_draw.font_vertices,
                       gameplay_hud_draw.font_vertex_count * sizeof(Vertex));
                GSPGPU_FlushDataCache(
                    vertices + ORIGINAL_GAMEPLAY_HUD_FONT_VERTEX_OFFSET,
                    renderer_vertex_flush_bytes(
                        gameplay_hud_draw.font_vertex_count,
                        GE_3DS_ORIGINAL_GAMEPLAY_HUD_FONT_VERTEX_CAPACITY));
                }
        }
        if (original_hud_font_texture_loaded
                && ge_original_campaign_credits_render_tick_exact(
                    0, 240, &credits_snapshot)
                && credits_snapshot.visible) {
            size_t credits_line_count = 0U;
            while (credits_line_count < credits_snapshot.line_count) {
                const GeOriginalCreditsRenderLine *source =
                    &credits_snapshot.lines[credits_line_count];
                const char *text = original_campaign_text(source->text_id);
                if (text == NULL) break;
                credits_lines[credits_line_count] =
                    (Ge3dsOriginalCreditsLine){
                        text, source->position, source->y,
                        source->alignment,
                    };
                ++credits_line_count;
            }
            if (credits_line_count == credits_snapshot.line_count
                    && ge_3ds_original_credits_build_draw_list(
                        &original_hud_atlas, credits_lines,
                        credits_line_count, &mission_hud_draw)
                    && mission_hud_draw.visible) {
                credits_active = true;
                mission_hud_uses_bank_gothic = false;
                mission_hud_vertex_count =
                    mission_hud_draw.glyph_vertex_count;
                memcpy(vertices + ORIGINAL_HUD_VERTEX_OFFSET,
                       mission_hud_draw.vertices,
                       mission_hud_vertex_count * sizeof(Vertex));
                GSPGPU_FlushDataCache(
                    vertices + ORIGINAL_HUD_VERTEX_OFFSET,
                    renderer_vertex_flush_bytes(
                        mission_hud_vertex_count,
                        ORIGINAL_HUD_VERTEX_CAPACITY));
            }
        }
    }
    gun_sight_vertex_count = build_original_sight_vertices(vertices, &gun_sight_draw);
    if (gun_sight_vertex_count != 0U)
        GSPGPU_FlushDataCache(
            vertex_buffer,
            renderer_vertex_flush_bytes(CROSSHAIR_VERTEX_COUNT,
                                        CROSSHAIR_VERTEX_COUNT));
    if (ge_3ds_fade_overlay_from_snapshot(
            fade_snapshot, &fade_overlay) && fade_overlay.visible) {
        static const float positions[FADE_OVERLAY_VERTEX_COUNT][2] = {
            {0.0f, 0.0f}, {400.0f, 0.0f}, {400.0f, 240.0f},
            {0.0f, 0.0f}, {400.0f, 240.0f}, {0.0f, 240.0f},
        };
        for (i = 0U; i < FADE_OVERLAY_VERTEX_COUNT; ++i) {
            vertices[FADE_OVERLAY_VERTEX_OFFSET + i] = (Vertex){
                positions[i][0], positions[i][1], 0.5f, 0.0f, 0.0f,
                fade_overlay.red, fade_overlay.green, fade_overlay.blue,
                fade_overlay.alpha,
            };
        }
        GSPGPU_FlushDataCache(
            vertices + FADE_OVERLAY_VERTEX_OFFSET,
            renderer_vertex_flush_bytes(
                FADE_OVERLAY_VERTEX_COUNT, FADE_OVERLAY_VERTEX_COUNT));
    }
    if (dam_cloud_texture_loaded && dam_preview != NULL
            && dam_preview->original_camera_ready
            && dam_preview->environment_ready
            && dam_preview->environment.clouds != 0U) {
        const GeDamSkyCamera sky_camera = {
            {
                dam_preview->original_camera_position[0],
                dam_preview->original_camera_position[1],
                dam_preview->original_camera_position[2],
            },
            {
                dam_preview->original_camera_look[0],
                dam_preview->original_camera_look[1],
                dam_preview->original_camera_look[2],
            },
            {
                dam_preview->original_camera_up[0],
                dam_preview->original_camera_up[1],
                dam_preview->original_camera_up[2],
            },
            40.0f, 0.0f, 320.0f, 240.0f, 60.0f, 4.0f / 3.0f,
        };

        if (ge_dam_sky_build_environment(
                &sky_camera, &dam_preview->environment,
                dam_cloud_offset, &sky_scene)) {
            sky_vertex_count = sky_scene.vertex_count;
            for (i = 0U; i < sky_vertex_count; ++i) {
                const GeDamSkyVertex *source = &sky_scene.vertices[i];

                vertices[DAM_CLOUD_VERTEX_OFFSET + i] = (Vertex){
                    source->screen_x, source->screen_y, 0.5f,
                    source->texture_u, source->texture_v,
                    source->red, source->green, source->blue, source->alpha,
                };
            }
            if (sky_vertex_count != 0U)
                GSPGPU_FlushDataCache(
                    vertices + DAM_CLOUD_VERTEX_OFFSET,
                    renderer_vertex_flush_bytes(
                        sky_vertex_count, DAM_CLOUD_VERTEX_CAPACITY));
        }
    }

    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, projection_uniform, &projection);
    /* The original 320-wide viewport is centered in the 400-wide top screen.
     * The 3DS render target is rotated, so its 40-pixel screen pillars are the
     * framebuffer's Y=40..360 scissor interval. */
    C3D_SetScissor(GPU_SCISSOR_NORMAL, 0U, 40U, 240U, 360U);
    C3D_FogGasMode(GPU_NO_FOG, GPU_PLAIN_DENSITY, false);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
    C3D_TexEnvInit(texture_environment);
    C3D_TexEnvSrc(texture_environment, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
    C3D_TexEnvFunc(texture_environment, C3D_Both, GPU_REPLACE);
    C3D_DrawArrays(GPU_TRIANGLES, DAM_ENVIRONMENT_VERTEX_OFFSET,
                   DAM_ENVIRONMENT_VERTEX_COUNT);
    if (sky_vertex_count != 0U) {
        C3D_TexBind(0, &dam_cloud_texture);
        C3D_TexEnvInit(texture_environment);
        /* Original skyRender combine:
         *   ENVIRONMENT + (SHADE - ENVIRONMENT) * TEXEL0. */
        C3D_TexEnvSrc(texture_environment, C3D_RGB,
                      GPU_PRIMARY_COLOR, GPU_CONSTANT, GPU_TEXTURE0);
        C3D_TexEnvFunc(texture_environment, C3D_RGB, GPU_INTERPOLATE);
        C3D_TexEnvSrc(texture_environment, C3D_Alpha,
                      GPU_PRIMARY_COLOR, 0, 0);
        C3D_TexEnvFunc(texture_environment, C3D_Alpha, GPU_REPLACE);
        C3D_TexEnvColor(texture_environment,
            UINT32_C(0xff000000)
                | ge_original_stage_environment_pica_fog_color(
                    &dam_preview->environment));
        C3D_DrawArrays(GPU_TRIANGLES, DAM_CLOUD_VERTEX_OFFSET,
                       sky_vertex_count);
    }
    if (dam_preview != NULL && dam_preview->loaded) {
        const bool gpu_world_render = dam_preview->original_camera_ready
            && dam_preview->gpu_world_ready;
        const bool camera_render = !gpu_world_render
            && dam_preview->original_camera_ready
            && dam_preview->render_batches != NULL;
        const size_t draw_batch_count = camera_render
            ? dam_preview->render_batch_count : dam_preview->batch_count;
        const size_t visibility_cache_count = gpu_world_render
                && dam_preview->batch_count
                    <= DAM_SCENE_PROJECTED_VERTEX_CAPACITY
            ? dam_preview->batch_count : 0U;
        int coordinate_projection = -1;

        if (visibility_cache_count != 0U)
            memset(world_batch_visibility_cache, 0,
                   visibility_cache_count);

        if (dam_preview->environment_ready
                && dam_preview->environment.fog_enabled != 0U) {
            C3D_FogGasMode(GPU_FOG, GPU_PLAIN_DENSITY, false);
            C3D_FogColor(
                ge_original_stage_environment_pica_fog_color(
                    &dam_preview->environment));
            C3D_FogLutBind(&dam_environment_fog_lut);
        } else {
            C3D_FogGasMode(GPU_NO_FOG, GPU_PLAIN_DENSITY, false);
        }
        for (i = 0U; i < draw_batch_count;) {
            const size_t source_index = camera_render
                ? dam_preview->render_batches[i].source_batch : i;
            const GeDamRoomDrawBatch *batch =
                &dam_preview->batches[source_index];
            const Ge3dsSceneTextureSlot *slot;
            C3D_Tex *texture;
            Ge3dsMaterialBinding binding;
            Ge3dsMaterialResult material_result;
            const size_t first_vertex = camera_render
                ? dam_preview->render_batches[i].first_vertex
                : batch->first_vertex;
            size_t vertex_count = camera_render
                ? dam_preview->render_batches[i].vertex_count
                : batch->vertex_count;
            size_t next = i + 1U;
            size_t merged_authored_batches = 1U;
            size_t scanned_vertex_end = first_vertex + vertex_count;

            if (gpu_world_render && dam_preview->visibility_ready
                    && !dam_visibility_contains_room(
                        dam_preview, batch->room_id)) {
                i = next;
                continue;
            }
            if (gpu_world_render
                    && !renderer_world_batch_may_draw(
                        dam_preview, source_index,
                        world_batch_visibility_cache,
                        visibility_cache_count)) {
                i = next;
                continue;
            }

            while (next < draw_batch_count) {
                const size_t next_source = camera_render
                    ? dam_preview->render_batches[next].source_batch : next;
                const size_t next_first = camera_render
                    ? dam_preview->render_batches[next].first_vertex
                    : dam_preview->batches[next_source].first_vertex;
                const GeDamRoomDrawBatch *next_batch =
                    &dam_preview->batches[next_source];
                const bool next_room_visible = !(gpu_world_render
                    && dam_preview->visibility_ready
                    && !dam_visibility_contains_room(
                        dam_preview, next_batch->room_id));
                if (!next_room_visible || next_first != scanned_vertex_end
                        || batch->coordinate_space
                            != next_batch->coordinate_space) {
                    break;
                }
                if (gpu_world_render && !renderer_world_batch_may_draw(
                        dam_preview, next_source,
                        world_batch_visibility_cache,
                        visibility_cache_count)) {
                    /* This exact authored range has a unanimous homogeneous
                     * clip outcode. It can sit inside a later merged range
                     * only under the SAME projection: it cannot produce a
                     * fragment under any material state in this draw. */
                    scanned_vertex_end += next_batch->vertex_count;
                    next++;
                    continue;
                }
                if (!dam_batch_materials_compatible(
                        batch, next_batch)) break;
                scanned_vertex_end += camera_render
                    ? dam_preview->render_batches[next].vertex_count
                    : next_batch->vertex_count;
                vertex_count = scanned_vertex_end - first_vertex;
                merged_authored_batches++;
                next++;
            }

            if (gpu_world_render) {
                const int batch_projection = (int)batch->coordinate_space;
                if (batch_projection != coordinate_projection) {
                    const C3D_Mtx *batch_matrix =
                        &dam_preview->gpu_world_projection;
                    if (batch->coordinate_space
                            == GE_DAM_ROOM_COORDINATE_RUNTIME)
                        batch_matrix = &dam_preview->gpu_runtime_projection;
                    else if (batch->coordinate_space
                            == GE_DAM_ROOM_COORDINATE_EYE)
                        batch_matrix =
                            &dam_preview->gpu_first_person_projection;
                    C3D_FVUnifMtx4x4(
                        GPU_VERTEX_SHADER, projection_uniform,
                        batch_matrix);
                    coordinate_projection = batch_projection;
                }
            }
            if (world_material_cache.valid
                    && memcmp(&world_material_cache.material,
                              &batch->material,
                              sizeof(batch->material)) == 0) {
                texture = world_material_cache.texture;
            } else {
                slot = ge_3ds_scene_textures_find(
                    &dam_scene_textures, batch->texture.texture_id);
                texture = slot != NULL ? (C3D_Tex *)&slot->texture : NULL;
                fine_profile.world_texture_lookups++;
            }
            binding = (Ge3dsMaterialBinding){
                texture,
                GE_3DS_MATERIAL_TEXTURE_FALLBACK_SHADE,
            };
            if (renderer_apply_material_cached(
                    &world_material_cache, &world_prepared_materials,
                    &batch->material, &binding,
                    &material_result,
                    &fine_profile.world_material_apply_calls,
                    &fine_profile.world_material_apply_reuses,
                    &fine_profile.world_material_prepare_hits,
                    &fine_profile.world_material_prepare_misses)
                    == GE_3DS_MATERIAL_OK
                    && material_result.state.draw_enabled != 0U) {
                C3D_DrawArrays(GPU_TRIANGLES,
                               DAM_ROOM_VERTEX_OFFSET + first_vertex,
                               vertex_count);
                fine_profile.world_draw_calls++;
                fine_profile.world_authored_batches +=
                    merged_authored_batches;
            }
            i = next;
        }
    }
    if(guard_muzzle_flash_vertex_count!=0U&&dam_preview!=NULL
            &&dam_preview->gpu_world_ready){
        size_t flash;
        /* dogfnegx selects the cloud-surface combiner, which does not
         * inherit the world's fog input. */
        C3D_FogGasMode(GPU_NO_FOG, GPU_PLAIN_DENSITY, false);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER,projection_uniform,
                         &dam_preview->gpu_first_person_projection);
        C3D_CullFace(GPU_CULL_BACK_CCW);
        /* texSelect(..., mode 4, zbuffer 1) is the canonical cloud-surface
         * pass: depth-tested translucent color, without a depth write. */
        C3D_DepthTest(true,GPU_GREATER,GPU_WRITE_COLOR);
        C3D_AlphaTest(false,GPU_ALWAYS,0U);
        C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_SRC_ALPHA,
            GPU_ONE_MINUS_SRC_ALPHA,GPU_ONE,GPU_ZERO);
        C3D_TexEnvInit(texture_environment);
        C3D_TexEnvSrc(texture_environment,C3D_Both,
                      GPU_TEXTURE0,GPU_PRIMARY_COLOR,0);
        C3D_TexEnvFunc(texture_environment,C3D_Both,GPU_MODULATE);
        for(flash=0U;flash<guard_muzzle_flash_vertex_count
                /GUARD_MUZZLE_FLASH_VERTICES;++flash){
            const Ge3dsSceneTextureSlot *slot=ge_3ds_scene_textures_find(
                &dam_scene_textures,guard_muzzle_flash_images[flash]);
            if(slot==NULL)continue;
            /* Pchrkalash sets bilerp immediately before GUNFIRE, and the
             * authored flagsS/flagsT are both clamp. */
            C3D_TexSetFilter((C3D_Tex *)&slot->texture,
                             GPU_LINEAR, GPU_LINEAR);
            C3D_TexSetWrap((C3D_Tex *)&slot->texture,
                           GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
            C3D_TexBind(0,(C3D_Tex *)&slot->texture);
            C3D_DrawArrays(GPU_TRIANGLES,GUARD_MUZZLE_FLASH_VERTEX_OFFSET
                +flash*GUARD_MUZZLE_FLASH_VERTICES,
                GUARD_MUZZLE_FLASH_VERTICES);
        }
    }
    if (autogun_beam_vertex_count != 0U && dam_preview != NULL
            && dam_preview->gpu_world_ready
            && autogun_beam_texture_loaded) {
        /* sub_GAME_7F061E18 is an object-world translucent pass.  The
         * canonical beam has already been advanced by the sole propsTick;
         * this block only submits its read-only published vertices. */
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, projection_uniform,
                         &dam_preview->gpu_world_projection);
        C3D_CullFace(GPU_CULL_NONE);
        C3D_DepthTest(true, GPU_GREATER, GPU_WRITE_COLOR);
        C3D_AlphaTest(false, GPU_ALWAYS, 0U);
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
            GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ZERO);
        C3D_TexBind(0, &autogun_beam_texture);
        C3D_TexEnvInit(texture_environment);
        C3D_TexEnvSrc(texture_environment, C3D_RGB,
                      GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
        C3D_TexEnvFunc(texture_environment, C3D_RGB, GPU_MODULATE);
        C3D_TexEnvSrc(texture_environment, C3D_Alpha,
                      GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
        C3D_TexEnvFunc(texture_environment, C3D_Alpha, GPU_MODULATE);
        C3D_DrawArrays(GPU_TRIANGLES, AUTOGUN_BEAM_VERTEX_OFFSET,
                       autogun_beam_vertex_count);
    }
    /* The original first-person and 2D passes clear G_FOG. */
    C3D_FogGasMode(GPU_NO_FOG, GPU_PLAIN_DENSITY, false);
    if (!watch_open && first_person != NULL && first_person->ready
            && dam_preview != NULL && dam_preview->gpu_world_ready) {
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, projection_uniform,
                          &dam_preview->gpu_first_person_projection);
        for (i = 0U; i < first_person->batch_count;) {
            const GeDamRoomDrawBatch *batch;
            const Ge3dsSceneTextureSlot *slot;
            C3D_Tex *texture;
            Ge3dsMaterialBinding binding;
            Ge3dsMaterialResult material_result;
            size_t vertex_count;
            size_t next;
            batch = &first_person->batches[i];
            vertex_count = batch->vertex_count;
            next = i + 1U;
            while (next < first_person->batch_count
                    && dam_batches_compatible(
                        &first_person->batches[next - 1U],
                        &first_person->batches[next])) {
                vertex_count += first_person->batches[next].vertex_count;
                ++next;
            }
            if (first_person_material_cache.valid
                    && memcmp(&first_person_material_cache.material,
                              &batch->material,
                              sizeof(batch->material)) == 0) {
                texture = first_person_material_cache.texture;
            } else {
                slot = ge_3ds_scene_textures_find(
                    &first_person_scene_textures,
                    batch->texture.texture_id);
                texture = slot != NULL ? (C3D_Tex *)&slot->texture : NULL;
                fine_profile.first_person_texture_lookups++;
            }
            binding = (Ge3dsMaterialBinding){
                texture,
                GE_3DS_MATERIAL_TEXTURE_FALLBACK_SHADE,
            };
            if (renderer_apply_material_cached(
                    &first_person_material_cache,
                    &first_person_prepared_materials,
                    &batch->material, &binding,
                    &material_result,
                    &fine_profile.first_person_material_apply_calls,
                    &fine_profile.first_person_material_apply_reuses,
                    &fine_profile.first_person_material_prepare_hits,
                    &fine_profile.first_person_material_prepare_misses)
                    == GE_3DS_MATERIAL_OK
                    && material_result.state.draw_enabled != 0U) {
                /* Material application restores the display-list depth mode,
                 * so apply the original first-person ModelRenderData override
                 * immediately before every hand draw. */
                C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
                C3D_DrawArrays(GPU_TRIANGLES,
                    FIRST_PERSON_VERTEX_OFFSET + batch->first_vertex,
                    vertex_count);
                fine_profile.first_person_draw_calls++;
                fine_profile.first_person_authored_batches += next - i;
            }
            i = next;
        }
    }
    C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, projection_uniform, &projection);
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0U, 0U, 0U, 0U);
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
    C3D_CullFace(GPU_CULL_NONE);
    if (gameplay_hud_draw.solid_vertex_count != 0U
            || gameplay_hud_draw.font_vertex_count != 0U) {
        C3D_AlphaTest(false, GPU_ALWAYS, 0U);
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
            GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ZERO);
        C3D_TexBind(0, &original_gameplay_hud_font_texture);
        configure_original_font_texture_environment(texture_environment);
        if (gameplay_hud_draw.solid_vertex_count != 0U)
            C3D_DrawArrays(GPU_TRIANGLES,
                           ORIGINAL_GAMEPLAY_HUD_SOLID_VERTEX_OFFSET,
                           gameplay_hud_draw.solid_vertex_count);
        if (gameplay_hud_draw.font_vertex_count != 0U)
            C3D_DrawArrays(GPU_TRIANGLES,
                           ORIGINAL_GAMEPLAY_HUD_FONT_VERTEX_OFFSET,
                           gameplay_hud_draw.font_vertex_count);
    }
    if (bottom_hud_vertex_count != 0U) {
        C3D_AlphaTest(false, GPU_ALWAYS, 0U);
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
            GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ZERO);
        C3D_TexBind(0, &original_gameplay_hud_font_texture);
        configure_original_font_texture_environment(texture_environment);
        C3D_DrawArrays(GPU_TRIANGLES,
                       ORIGINAL_BOTTOM_HUD_VERTEX_OFFSET
                           + bottom_hud_draw.box_vertex_count,
                       bottom_hud_draw.glyph_vertex_count);
    }
    if (mission_hud_vertex_count != 0U) {
        C3D_AlphaTest(false, GPU_ALWAYS, 0U);
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
            GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ZERO);
        C3D_TexBind(0, mission_hud_uses_bank_gothic
            ? &original_gameplay_hud_font_texture
            : &original_hud_font_texture);
        configure_original_font_texture_environment(texture_environment);
        C3D_DrawArrays(GPU_TRIANGLES, ORIGINAL_HUD_VERTEX_OFFSET,
                       mission_hud_draw.box_vertex_count);
        C3D_DrawArrays(GPU_TRIANGLES,
                       ORIGINAL_HUD_VERTEX_OFFSET
                           + mission_hud_draw.box_vertex_count,
                       mission_hud_draw.glyph_vertex_count);
    }
    C3D_TexEnvInit(texture_environment);
    C3D_TexEnvSrc(texture_environment, C3D_Both, GPU_PRIMARY_COLOR, 0, 0);
    C3D_TexEnvFunc(texture_environment, C3D_Both, GPU_REPLACE);
    if (!watch_open && !credits_active && gun_sight_vertex_count != 0U) {
        const Ge3dsMaterialBinding binding = {
            &gun_sight_texture, GE_3DS_MATERIAL_TEXTURE_FALLBACK_SHADE
        };
        Ge3dsMaterialResult material;
        if (ge_3ds_material_apply(&gun_sight_draw.material, &binding, &material)
                == GE_3DS_MATERIAL_OK && material.state.draw_enabled)
            C3D_DrawArrays(GPU_TRIANGLES, 0, gun_sight_vertex_count);
        else
            ++gun_sight_failures;
    }
    if (right_ammo_icon_vertex_count != 0U) {
        C3D_AlphaTest(false, GPU_ALWAYS, 0U);
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
            GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ZERO);
        C3D_TexBind(0, &right_ammo_icon->texture);
        C3D_TexEnvInit(texture_environment);
        C3D_TexEnvSrc(texture_environment, C3D_RGB,
                      GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
        C3D_TexEnvFunc(texture_environment, C3D_RGB, GPU_MODULATE);
        C3D_TexEnvSrc(texture_environment, C3D_Alpha,
                      GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
        C3D_TexEnvFunc(texture_environment, C3D_Alpha, GPU_MODULATE);
        C3D_DrawArrays(GPU_TRIANGLES,
                       ORIGINAL_AMMO_ICON_VERTEX_OFFSET,
                       right_ammo_icon_vertex_count);
    }
    if (left_ammo_icon_vertex_count != 0U) {
        C3D_AlphaTest(false, GPU_ALWAYS, 0U);
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
            GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ZERO);
        C3D_TexBind(0, &left_ammo_icon->texture);
        C3D_TexEnvInit(texture_environment);
        C3D_TexEnvSrc(texture_environment, C3D_RGB,
                      GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
        C3D_TexEnvFunc(texture_environment, C3D_RGB, GPU_MODULATE);
        C3D_TexEnvSrc(texture_environment, C3D_Alpha,
                      GPU_TEXTURE0, GPU_PRIMARY_COLOR, 0);
        C3D_TexEnvFunc(texture_environment, C3D_Alpha, GPU_MODULATE);
        C3D_DrawArrays(GPU_TRIANGLES,
                       ORIGINAL_AMMO_ICON_VERTEX_OFFSET
                           + right_ammo_icon_vertex_count,
                       left_ammo_icon_vertex_count);
    }
    if (fade_overlay.visible) {
        /* Original colour-screen state is a final full-frame RGBA pass. */
        C3D_AlphaTest(false, GPU_ALWAYS, 0U);
        C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
            GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA, GPU_ONE, GPU_ZERO);
        /* This pass follows textured HUD draws.  Restore the original
         * G_CC_PRIMITIVE semantics explicitly so their bound font/ammo
         * texture cannot modulate a damage flash to black. */
        C3D_TexEnvInit(texture_environment);
        C3D_TexEnvSrc(texture_environment, C3D_Both,
                      GPU_PRIMARY_COLOR, 0, 0);
        C3D_TexEnvFunc(texture_environment, C3D_Both, GPU_REPLACE);
        C3D_DrawArrays(GPU_TRIANGLES, FADE_OVERLAY_VERTEX_OFFSET,
                       FADE_OVERLAY_VERTEX_COUNT);
    }
}

static void renderer_exit(void)
{
    ge_3ds_scene_textures_close(&dam_scene_textures);
    if (copy_icon_loaded) {
        C3D_TexDelete(&copy_icon_texture);
        copy_icon_loaded = false;
    }
    if (blotter_texture_loaded) {
        C3D_TexDelete(&blotter_texture);
        blotter_texture_loaded = false;
    }
    if (dam_cloud_texture_loaded) {
        C3D_TexDelete(&dam_cloud_texture);
        dam_cloud_texture_loaded = false;
    }
    if (autogun_beam_texture_loaded) {
        C3D_TexDelete(&autogun_beam_texture);
        memset(&autogun_beam_texture, 0, sizeof(autogun_beam_texture));
        memset(&autogun_beam_subtexture, 0,
               sizeof(autogun_beam_subtexture));
        autogun_beam_texture_loaded = false;
    }
    if (original_hud_font_texture_loaded) {
        C3D_TexDelete(&original_hud_font_texture);
        original_hud_font_texture_loaded = false;
    }
    if (gun_sight_texture_loaded) {
        C3D_TexDelete(&gun_sight_texture);
        gun_sight_texture_loaded = false;
    }
    if (original_gameplay_hud_font_texture_loaded) {
        C3D_TexDelete(&original_gameplay_hud_font_texture);
        original_gameplay_hud_font_texture_loaded = false;
    }
    {
        size_t index;
        for (index = 0U; index < GE_ORIGINAL_AMMO_ICON_ASSET_COUNT;
                ++index) {
            RuntimeAmmoIconTexture *runtime =
                &original_ammo_icon_textures[index];
            if (runtime->loaded) C3D_TexDelete(&runtime->texture);
            memset(runtime, 0, sizeof(*runtime));
        }
    }
    {
        size_t index;
        for (index = 0U; index < GE_3DS_ORIGINAL_FRONTEND_MAX_SPRITES;
                ++index) {
            RuntimeFrontendSpriteTexture *runtime =
                &original_frontend_sprite_textures[index];
            if (runtime->loaded) C3D_TexDelete(&runtime->texture);
            memset(runtime, 0, sizeof(*runtime));
        }
    }
    {
        size_t index;
        for (index = 0U; index < 4U; index++) {
            if (rareware_textures_loaded[index]) {
                C3D_TexDelete(&rareware_textures[index]);
                rareware_textures_loaded[index] = false;
            }
        }
        if (rareware_body_texture_loaded) {
            C3D_TexDelete(&rareware_body_texture);
            memset(&rareware_body_texture, 0, sizeof(rareware_body_texture));
            memset(&rareware_body_subtexture, 0,
                   sizeof(rareware_body_subtexture));
            rareware_body_texture_loaded = false;
        }
        if (rareware_front_texture_loaded) {
            C3D_TexDelete(&rareware_front_texture);
            memset(&rareware_front_texture, 0,
                   sizeof(rareware_front_texture));
            memset(&rareware_front_subtexture, 0,
                   sizeof(rareware_front_subtexture));
            rareware_front_texture_loaded = false;
        }
    }
    linearFree(vertex_buffer);
    vertex_buffer = NULL;
    shaderProgramFree(&shader_program);
    DVLB_Free(shader_dvlb);
    shader_dvlb = NULL;
}

static bool load_texture_catalog(GeAssetPack *asset_pack,
                                 GeTextureCatalog *catalog,
                                 void **catalog_data)
{
    const char *path = "converted/textures/catalog.gecat";
    const GeAssetPackEntry *entry;
    void *data;

    if (asset_pack == NULL || catalog == NULL || catalog_data == NULL
            || (entry = ge_asset_pack_find(asset_pack, path)) == NULL
            || entry->data_size > SIZE_MAX) {
        return false;
    }
    data = malloc((size_t)entry->data_size);
    if (data == NULL
            || ge_asset_pack_read(asset_pack, path, data,
                                  (size_t)entry->data_size, NULL) != GE_ASSET_PACK_OK
            || ge_texture_catalog_open_memory(catalog, data,
                                              (size_t)entry->data_size)
                != GE_TEXTURE_CATALOG_OK) {
        free(data);
        return false;
    }
    *catalog_data = data;
    return true;
}

typedef struct RuntimeGbiMeshBuild {
    RuntimeGbiMesh *mesh;
    int16_t minimum_x;
    int16_t maximum_x;
    int16_t minimum_y;
    int16_t maximum_y;
} RuntimeGbiMeshBuild;

static int collect_rareware_triangles(const GeGbiPipelineEvent *event,
                                      void *user_data)
{
    RuntimeGbiMeshBuild *build = user_data;
    uint8_t triangle_index;

    if (event->action.kind != GE_GBI_STATE_ACTION_DRAW_TRIANGLES) {
        return 1;
    }
    for (triangle_index = 0U;
            triangle_index < event->action.data.draw.count;
            triangle_index++) {
        const GeGbiTriangle *triangle =
            &event->action.data.draw.triangles[triangle_index];
        uint8_t vertex_index;

        for (vertex_index = 0U; vertex_index < 3U; vertex_index++) {
            const GeGbiVertex *source =
                &event->vertex_cache[triangle->vertex[vertex_index]];
            Vertex *destination;

            if (build->mesh->vertex_count >= RAREWARE_VERTEX_CAPACITY) {
                return 0;
            }
            destination = &build->mesh->vertices[build->mesh->vertex_count++];
            *destination = (Vertex){
                (float)source->x, (float)source->y, 0.5f,
                (float)source->texture_s / 32.0f,
                (float)source->texture_t / 32.0f,
                0.88f, 0.64f, 0.16f, 1.0f,
            };
            if (source->x < build->minimum_x) build->minimum_x = source->x;
            if (source->x > build->maximum_x) build->maximum_x = source->x;
            if (source->y < build->minimum_y) build->minimum_y = source->y;
            if (source->y > build->maximum_y) build->maximum_y = source->y;
        }
    }
    return 1;
}

static RuntimeGbiMesh load_rareware_display_list(GeAssetPack *asset_pack)
{
    const char *path = "converted/runtime/segments/rarewarelogo.bin";
    RuntimeGbiMesh mesh = {0};
    RuntimeGbiMeshBuild build = {&mesh, INT16_MAX, INT16_MIN,
                                 INT16_MAX, INT16_MIN};
    const GeAssetPackEntry *entry;
    GeGbiMemoryMap memory;
    GeGbiTraversalConfig config = {32U, 20000U};
    GeGbiPipelineResult result;
    uint8_t *segment;

    if (asset_pack == NULL || (entry = ge_asset_pack_find(asset_pack, path)) == NULL
            || entry->data_size != UINT64_C(0x67f0)) {
        return mesh;
    }
    segment = malloc((size_t)entry->data_size);
    if (segment == NULL
            || ge_asset_pack_read(asset_pack, path, segment,
                                  (size_t)entry->data_size, NULL)
                != GE_ASSET_PACK_OK) {
        free(segment);
        return mesh;
    }

    ge_gbi_memory_map_init(&memory);
    if (ge_gbi_memory_map_set_segment(&memory, 2U, segment,
                                      (size_t)entry->data_size)
            == GE_GBI_RESOLVE_OK) {
        result = ge_gbi_pipeline_execute(
            &memory, (GeGbiAddress){UINT32_C(0x020044b0), UINT32_C(0x44b0), 2U},
            GE_GBI_BYTE_ORDER_BIG_ENDIAN, &config,
            collect_rareware_triangles, &build);
        if (result.status == GE_GBI_PIPELINE_OK
                && result.unsupported_commands == 0U
                && result.triangles == RAREWARE_TRIANGLE_COUNT
                && mesh.vertex_count == RAREWARE_VERTEX_CAPACITY
                && build.maximum_x > build.minimum_x
                && build.maximum_y > build.minimum_y) {
            mesh.loaded = true;
            mesh.commands = result.traversal.commands_visited;
            mesh.draws = result.draw_calls;
            mesh.triangles = result.triangles;
        }
    }
    free(segment);
    return mesh;
}

static int collect_rareware_body(const GeGbiPipelineEvent *event,
                                 void *user_data)
{
    RuntimeGbiModel *model = user_data;
    uint8_t triangle_index;

    if (event->action.kind != GE_GBI_STATE_ACTION_DRAW_TRIANGLES) {
        return 1;
    }
    model->material_ready = ge_pica_material_translate(
        event->state, &model->material) == GE_PICA_MATERIAL_OK;
    for (triangle_index = 0U;
            triangle_index < event->action.data.draw.count;
            triangle_index++) {
        const GeGbiTriangle *triangle =
            &event->action.data.draw.triangles[triangle_index];
        uint8_t vertex_index;

        for (vertex_index = 0U; vertex_index < 3U; vertex_index++) {
            const GeGbiVertex *source =
                &event->vertex_cache[triangle->vertex[vertex_index]];
            RuntimeModelVertex *destination;

            if (model->vertex_count >= RAREWARE_BODY_VERTEX_CAPACITY) {
                return 0;
            }
            destination = &model->vertices[model->vertex_count++];
            *destination = (RuntimeModelVertex){
                (float)source->x, (float)source->y, (float)source->z,
                (float)(int8_t)source->red / 127.0f,
                (float)(int8_t)source->green / 127.0f,
                (float)(int8_t)source->blue / 127.0f,
            };
        }
    }
    return 1;
}

static bool load_rareware_body_model(GeAssetPack *asset_pack,
                                     RuntimeGbiModel *model)
{
    const char *path = "converted/runtime/segments/rarewarelogo.bin";
    const GeAssetPackEntry *entry;
    GeGbiMemoryMap memory;
    GeGbiTraversalConfig config = {32U, 20000U};
    GeGbiPipelineResult pipeline;
    uint8_t *segment;

    if (model == NULL) {
        return false;
    }
    memset(model, 0, sizeof(*model));
    if (asset_pack == NULL || (entry = ge_asset_pack_find(asset_pack, path)) == NULL
            || entry->data_size != UINT64_C(0x67f0)) {
        return false;
    }
    segment = malloc((size_t)entry->data_size);
    if (segment == NULL
            || ge_asset_pack_read(asset_pack, path, segment,
                                  (size_t)entry->data_size, NULL)
                != GE_ASSET_PACK_OK) {
        free(segment);
        return false;
    }
    ge_gbi_memory_map_init(&memory);
    if (ge_gbi_memory_map_set_segment(&memory, 2U, segment,
                                      (size_t)entry->data_size)
            != GE_GBI_RESOLVE_OK) {
        free(segment);
        return false;
    }
    pipeline = ge_gbi_pipeline_execute(
        &memory, (GeGbiAddress){UINT32_C(0x02004758), UINT32_C(0x4758), 2U},
        GE_GBI_BYTE_ORDER_BIG_ENDIAN, &config,
        collect_rareware_body, model);
    free(segment);
    if (pipeline.status != GE_GBI_PIPELINE_OK
            || pipeline.unsupported_commands != 0U
            || pipeline.traversal.commands_visited != 273U
            || pipeline.draw_calls != RAREWARE_BODY_TRIANGLE_COUNT
            || pipeline.triangles != RAREWARE_BODY_TRIANGLE_COUNT
            || model->vertex_count != RAREWARE_BODY_VERTEX_CAPACITY) {
        memset(model, 0, sizeof(*model));
        return false;
    }
    model->loaded = true;
    model->commands = pipeline.traversal.commands_visited;
    model->draws = pipeline.draw_calls;
    model->triangles = pipeline.triangles;
    return true;
}

static bool load_rareware_front_model(GeAssetPack *asset_pack,
                                      RuntimeGbiModel *model)
{
    static const char path[] =
        "converted/runtime/segments/rarewarelogo.bin";
    const GeAssetPackEntry *entry;
    GeOriginalRarewareMesh query;
    GeOriginalRarewareMesh built;
    GeOriginalRarewareVertex *vertices = NULL;
    uint8_t *segment = NULL;
    size_t index;
    bool ready = false;
    if (asset_pack == NULL || model == NULL
            || (entry = ge_asset_pack_find(asset_pack, path)) == NULL
            || entry->data_size != GE_ORIGINAL_RAREWARE_SEGMENT_BYTES)
        return false;
    memset(model, 0, sizeof(*model));
    segment = malloc(GE_ORIGINAL_RAREWARE_SEGMENT_BYTES);
    if (segment == NULL || ge_asset_pack_read(
            asset_pack, path, segment, GE_ORIGINAL_RAREWARE_SEGMENT_BYTES,
            NULL) != GE_ASSET_PACK_OK)
        goto done;
    if (ge_original_rareware_mesh_build(
            segment, GE_ORIGINAL_RAREWARE_SEGMENT_BYTES,
            GE_ORIGINAL_RAREWARE_PASS_FRONT, NULL, 0U, &query)
            != GE_ORIGINAL_RAREWARE_CAPACITY_EXCEEDED
            || query.required_vertex_count > RAREWARE_BODY_VERTEX_CAPACITY)
        goto done;
    vertices = calloc(query.required_vertex_count, sizeof(*vertices));
    if (vertices == NULL || ge_original_rareware_mesh_build(
            segment, GE_ORIGINAL_RAREWARE_SEGMENT_BYTES,
            GE_ORIGINAL_RAREWARE_PASS_FRONT, vertices,
            query.required_vertex_count, &built) != GE_ORIGINAL_RAREWARE_OK)
        goto done;
    for (index = 0U; index < built.vertex_count; ++index) {
        const GeGbiVertex *source = &vertices[index].source;
        model->vertices[index] = (RuntimeModelVertex){
            (float)source->x, (float)source->y, (float)source->z,
            (float)(int8_t)source->red / 127.0f,
            (float)(int8_t)source->green / 127.0f,
            (float)(int8_t)source->blue / 127.0f,
        };
    }
    model->loaded = true;
    model->commands = built.commands_visited;
    model->draws = built.triangle_count;
    model->triangles = built.triangle_count;
    model->vertex_count = built.vertex_count;
    ready = true;
done:
    free(vertices);
    free(segment);
    if (!ready) memset(model, 0, sizeof(*model));
    return ready;
}

static void print_status(const GePortState *port,
                         const GeStageAssetDescriptor *stage_assets,
                         bool is_new_3ds,
                         bool cstick_available, bool assets_mounted, uint32_t asset_count,
                         bool texture_catalog_mounted, uint32_t texture_count,
                         const GeTextureCacheStats *texture_cache_stats,
                         const RuntimeGbiMesh *rareware_mesh,
                         const RuntimeGbiModel *body_model,
                         const RuntimeBlotterPreview *blotter_preview,
                         const RuntimeDamPreview *dam_preview,
                         const RuntimeDamCollision *dam_collision,
                         const RuntimeDamIntro *dam_intro,
                         const RuntimeDamWorldObjects *dam_objects,
                         const RuntimeBondAnimations *bond_animations,
                         const RuntimeFirstPersonModels *first_person_models,
                         const RuntimeFirstPersonScene *first_person_scene,
                         uint64_t scheduler_ticks, unsigned ticks,
                         bool audio_active, bool clip_stage_ready,
                         bool audio_abi_ready)
{
    float view_x;
    float view_y;
    float view_z;
    uint8_t next_preload_room = 0U;
    GeOriginalGameplayServiceStats gameplay_services;
    GeOriginalCovertModemFireStats modem_fire;
    GeOriginalPp7FireStats pp7_fire;
    GeOriginalDamGuardStats guard_stats;
    GeOriginalDamMissionHudRenderSnapshot mission_hud = {0};
    GeOriginalDamMissionExitSnapshot mission_exit = {0};
    GeOriginalMissionResultSnapshot mission_result = {0};
    GeOriginalGunLiveHand right_hand = {0};
    const bool right_hand_published =
        ge_original_gun_live_hand_snapshot(0U, &right_hand) != 0;
    const bool preload_pending = ge_dam_preload_queue_peek(
        &dam_preview->preload_queue, &next_preload_room) == GE_DAM_PRELOAD_OK;

    ge_port_view_vector(port, &view_x, &view_y, &view_z);
    ge_original_gameplay_services_snapshot(&gameplay_services);
    ge_original_covert_modem_fire_snapshot(&modem_fire);
    ge_original_pp7_fire_snapshot(&pp7_fire);
    ge_original_dam_guards_snapshot(&guard_stats);
    (void)ge_original_dam_mission_hud_render_snapshot(&mission_hud);
    ge_original_dam_mission_exit_services_snapshot(&mission_exit);
    ge_original_mission_result_snapshot(&mission_result);
    (void)rareware_mesh;
    (void)body_model;
    (void)blotter_preview;
    printf("\x1b[2J\x1b[1;1H");
    printf("GoldenEye 007 - %s\n",
           stage_assets != NULL ? stage_assets->key : "stage");
    printf("Gold marker: %s spawn pad %ld\n\n",
           dam_preview->setup_loaded ? "setup" : "fallback",
           (long)dam_preview->spawn_pad);
    printf("Target:       %s\n", is_new_3ds ? "New Nintendo 3DS" : "Nintendo 3DS");
    printf("C-stick:      %s\n", cstick_available ? "ready" : "unavailable");
    printf("Asset pack:   %s (%lu files)\n", assets_mounted ? "mounted" : "not found",
           (unsigned long)asset_count);
    printf("Textures:     %s (%lu LODs)\n",
           texture_catalog_mounted ? "cataloged" : "legacy",
           (unsigned long)texture_count);
    printf("Tex cache:    %lu/%lu slots, %lu B, %llu hit/%llu miss\n",
           (unsigned long)texture_cache_stats->occupied_slots,
           (unsigned long)texture_cache_stats->slot_count,
           (unsigned long)texture_cache_stats->loaded_bytes,
           (unsigned long long)texture_cache_stats->hits,
           (unsigned long long)texture_cache_stats->misses);
    printf("Scene tex:    %lu/%lu loaded, %lu missing\n",
           (unsigned long)dam_scene_textures.loaded_count,
           (unsigned long)dam_scene_textures.texture_count,
           (unsigned long)dam_scene_textures.missing_count);
    printf("Stage world: %s (%lu rooms, %lu lists)\n",
           dam_preview->loaded ? "spawn+portals" : "missing",
           (unsigned long)dam_preview->rooms,
           (unsigned long)dam_preview->lists);
    printf("World graph: %s %lu/%lu rooms via %lu portals\n",
           ge_dam_world_status_name(dam_preview->world_status),
           (unsigned long)dam_preview->world_room_count,
           (unsigned long)dam_preview->world.room_count,
           (unsigned long)dam_preview->world.portal_count);
    printf("Portal vis:  %s %lu room / %lu descent\n",
           ge_dam_visibility_runtime_status_name(
               dam_preview->visibility_status),
           (unsigned long)dam_preview->visibility_result.room_count,
           (unsigned long)dam_preview->visibility_result.portal_descents);
    printf("Room preload: %s %lu pending (next %ld) / %lu req / %lu blocked\n",
           ge_dam_preload_status_name(dam_preview->preload_status),
           (unsigned long)dam_preview->preload_queue.pending_count,
           preload_pending ? (long)next_preload_room : -1L,
           (unsigned long)dam_preview->preload_queue.accepted_count,
           (unsigned long)dam_preview->preload_queue.overflow_count);
    printf("Room install: %s gen %llu / %llu ok / %llu fail\n",
           ge_dam_dynamic_scene_status_name(
               dam_preview->dynamic_scene_status),
           (unsigned long long)dam_preview->dynamic_scene.generation,
           (unsigned long long)dam_preview->dynamic_scene.install_successes,
           (unsigned long long)dam_preview->dynamic_scene.install_failures);
    printf("Stage setup: %s pad %ld %s @ %.0f/%.0f/%.0f\n",
           dam_preview->setup_loaded ? "original" : "missing",
           (long)dam_preview->spawn_pad,
           dam_preview->spawn_plink != NULL ? dam_preview->spawn_plink : "-",
           (double)dam_preview->spawn_position[0],
           (double)dam_preview->spawn_position[1],
           (double)dam_preview->spawn_position[2]);
    if (mission_hud.count != 0U) {
        size_t hud_index;
        printf("Mission HUD:\n");
        for (hud_index = 0U; hud_index < mission_hud.count; ++hud_index)
            printf("  %s\n", mission_hud.messages[hud_index]);
    }
    printf("Bond camera: %s (%lu mtx/%lu light, room %u)\n",
           dam_preview->original_camera_ready ? "original" :
               ge_original_bond_camera_status_name(
                   dam_preview->original_camera_status),
           (unsigned long)dam_preview->original_camera_matrices,
           (unsigned long)dam_preview->original_camera_lights,
           (unsigned int)dam_preview->original_camera_room);
    printf("Camera clip: %s %lu/%lu in -> %lu tri (%lu batch)\n",
           ge_dam_camera_status_name(dam_preview->camera_handoff_status),
           (unsigned long)dam_preview->camera_visible_triangles,
           (unsigned long)dam_preview->camera_input_triangles,
           (unsigned long)dam_preview->camera_output_triangles,
           (unsigned long)dam_preview->render_batch_count);
    printf("Dam geometry:%lu cmd / %lu src / %lu gpu / %lu tri / %lu vtx\n",
           (unsigned long)dam_preview->commands,
           (unsigned long)dam_preview->draws,
           (unsigned long)dam_preview->material_groups,
           (unsigned long)dam_preview->triangles,
           (unsigned long)dam_preview->vertex_count);
    printf("Dam STAN:     %s (%lu tile, %lu pt, spawn %lu/r%u)\n",
           dam_collision->loaded ? "native" : "missing",
           (unsigned long)dam_collision->surface.tile_count,
           (unsigned long)dam_collision->surface.point_count,
           (unsigned long)dam_collision->surface.spawn_tile,
           (unsigned int)dam_collision->surface.spawn_room);
    printf("Original STAN:%s %s %s/rad-%s floor %.1f\n",
           dam_collision->original_bound ? "bound" : "unbound",
           dam_preview->spawn_plink != NULL ? dam_preview->spawn_plink : "-",
           dam_collision->original_spawn_matched
               && dam_collision->original_spawn_in_bounds ? "ok" : "fail",
           dam_collision->original_spawn_radius_clear ? "ok" : "fail",
           dam_collision->original_spawn_floor_y);
    printf("Decomp intro: %s pad %ld/%s @ %.1f %.1f %.1f yaw %.1f\n",
           dam_intro->spawn.loaded ? "loaded" : "missing",
           (long)dam_intro->spawn.pad_index,
           dam_intro->spawn.stan_name != NULL ? dam_intro->spawn.stan_name : "-",
           (double)dam_intro->spawn.position[0],
           (double)dam_intro->spawn.position[1],
           (double)dam_intro->spawn.position[2],
           (double)dam_intro->spawn.look_angle_degrees);
    printf("Bond player:  %s r%d rad %.0f @ %.1f %.1f %.1f\n",
           dam_intro->player.initialized ? "original" : "missing",
           (int)dam_intro->player.room,
           (double)dam_intro->player.collision_radius,
           (double)dam_intro->player.collision_position[0],
           (double)dam_intro->player.collision_position[1],
           (double)dam_intro->player.collision_position[2]);
    printf("Dam objects:  ABI %lu/4, original prop %lu active\n",
           (unsigned long)dam_objects->state.definitions_materialized,
           (unsigned long)ge_original_prop_state_active_count());
    printf("Dam guards:   %lu/%lu authored, mtx %lu, status %u\n",
           (unsigned long)guard_stats.constructed_guards,
           (unsigned long)guard_stats.authored_guards,
           (unsigned long)guard_stats.matrix_updates,
           (unsigned int)dam_objects->guard_status);
#if defined(GE_DAM_FULL_PROPS_LIVE)
    printf("Props tick:   original %llu tick/%llu reject, status %u\n",
           (unsigned long long)dam_objects->guard_runtime.ticks,
           (unsigned long long)dam_objects->guard_runtime.rejected_ticks,
           (unsigned int)dam_objects->guard_runtime.last_status);
#endif
    printf("Guard scene:  %s %lu guard/%lu list, %lu tri\n",
           dam_objects->guard_model_loaded
                   && dam_objects->guard_weapon_model_loaded
               ? ge_original_dam_guard_scene_status_name(
                   dam_objects->guard_scene.status)
               : "asset missing",
           (unsigned long)dam_objects->guard_scene.guard_count,
           (unsigned long)dam_objects->guard_scene.input_count,
           (unsigned long)dam_objects->guard_scene.triangle_count);
    printf("Mission AI:   %s list %04lx offset %u, %lu tick/%lu transition\n",
           dam_objects->mission_flow_live ? "original" : "frontier",
           (unsigned long)dam_objects->mission_flow.stage_list_id,
           (unsigned int)dam_objects->mission_flow.ai_offset,
           (unsigned long)dam_objects->mission_flow.ticks,
           (unsigned long)dam_objects->mission_flow.yield_transitions);
    printf("Mission tags: %lu/2 authored, objective %08lx, HUD %u\n",
           (unsigned long)dam_objects->mission_tags.tags_registered,
           (unsigned long)dam_objects->mission_flow.objective_registers,
           (unsigned int)dam_objects->mission_flow.hud_message_count);
    printf("Mission exit: stop %ld fade %.2f/%ld cam %ld title %lu\n",
           (long)mission_exit.stop_time,
           (double)mission_exit.fade_fraction,
           (long)mission_exit.fade_time_max,
           (long)mission_exit.camera_mode,
           (unsigned long)mission_exit.title_stage_requests);
    printf("Mission result: %lu apply, %lu save, %lu cheat, %lu frontier\n",
           (unsigned long)mission_result.apply_calls,
           (unsigned long)mission_result.completion_mutations,
           (unsigned long)mission_result.cheat_mutations,
           (unsigned long)mission_result.persistence_frontiers);
    printf("Prop scene:   %s %lu tri / %lu batch / %lu vtx\n",
           dam_objects->model_scene_ready ? "live" :
               ge_original_model_scene_status_name(
                   dam_objects->model_scene_status),
           (unsigned long)dam_objects->model_scene_triangles,
           (unsigned long)dam_objects->model_scene_batches,
           (unsigned long)dam_objects->model_scene_vertices);
#if defined(GE_DAM_FULL_PROPS_LIVE)
    printf("Scene refresh:%llu guard/%llu door/%llu full, %llu/%llu in-place\n",
           (unsigned long long)dam_objects->guard_overlay_updates,
           (unsigned long long)dam_objects->door_overlay_updates,
           (unsigned long long)dam_objects->overlay_full_rebuilds,
           (unsigned long long)dam_preview->dynamic_scene
               .overlay_update_successes,
           (unsigned long long)dam_preview->dynamic_scene
               .overlay_update_failures);
    printf("Guard cache:  %llu one-pass/%llu call, %llu topology\n",
           (unsigned long long)dam_objects->guard_scene_cache
               .single_pass_builds,
           (unsigned long long)dam_objects->guard_scene_cache.build_attempts,
           (unsigned long long)dam_objects->guard_scene_cache
               .topology_rebuilds);
#endif
    printf("Setup cmds:   glass %ld, prop %ld, doors %ld/%ld\n",
           (long)dam_objects->state.first_glass.command_index,
           (long)dam_objects->state.first_object.command_index,
           (long)dam_objects->state.first_door.command_index,
           (long)dam_objects->state.second_door.command_index);
    printf("Dam gates:    %s/%s linked %s portals %ld/%ld\n",
           ge_original_door_status_name(dam_objects->door_status[0]),
           ge_original_door_status_name(dam_objects->door_status[1]),
           dam_objects->doors_linked ? "yes" : "no",
           (long)dam_objects->doors[0].portal_number,
           (long)dam_objects->doors[1].portal_number);
    printf("Door tick:    %s %lu tick/%lu collision (%s)\n",
           ge_original_door_runtime_status_name(
               dam_objects->door_runtime.status),
           (unsigned long)dam_objects->door_runtime.ticks,
           (unsigned long)dam_objects->door_runtime.collision_tests,
           ge_original_door_collision_status_name(
               dam_objects->door_collision.status));
    printf("Gate scene:   gen %lu/%lu clip %u/%u\n",
           (unsigned long)dam_objects->installed_door_scene_generation[0],
           (unsigned long)dam_objects->installed_door_scene_generation[1],
           (unsigned int)dam_objects->door_scenes[0].uses_clipped_vertices,
           (unsigned int)dam_objects->door_scenes[1].uses_clipped_vertices);
    printf("Gate use:     %s %lu/%lu hit, %lu activate, %lu reload\n",
           ge_original_door_interaction_result_name(
               dam_objects->door_interaction.result),
           (unsigned long)dam_objects->door_interaction.interaction_hits,
           (unsigned long)dam_objects->door_interaction.interaction_tests,
           (unsigned long)dam_objects->door_interaction.activations,
           (unsigned long)dam_objects->door_interaction.reload_requests);
    printf("Model 62:     %s, obj %s/%s move %s/s%lu\n",
           ge_original_model62_status_name(dam_objects->model62_status),
           ge_original_default_object_status_name(
               dam_objects->default_object_status),
           dam_objects->default_object.object_initialized
               ? "pre-placement" : "not initialized",
           ge_original_default_object_status_name(
               dam_objects->placement_status),
           (unsigned long)dam_objects->default_object.placement_stage);
    printf("Bond anim:    %s gait %s walk %u / sprint %u\n",
           bond_animations->decoder_verified ? "ROM frames" : "missing",
           bond_animations->gait_verified ? "m0 exact" :
               ge_original_player_gait_status_name(
                   bond_animations->gait_status),
           (unsigned int)ge_original_animation_root_frame_count(
               bond_animations->walking),
           (unsigned int)ge_original_animation_root_frame_count(
               bond_animations->sprinting));
    printf("Live chain:   exact move %lu input-only %lu pos %.2f/%.2f\n",
           (unsigned long)first_person_models->bond_live.move_tick_count,
           (unsigned long)first_person_models->bond_live.input_tick_count,
           dam_intro->player.collision_position[0],
           dam_intro->player.collision_position[2]);
    printf("Game svc:     sfx %lu/%lu evt %lu dec %lu/%lu pcm %llu\n",
           (unsigned long)gameplay_services.sound_play_calls,
           (unsigned long)gameplay_services.active_sounds,
           (unsigned long)gameplay_services.sound_parameter_events,
           (unsigned long)gameplay_services.decoded_sound_starts,
           (unsigned long)gameplay_services.sound_decode_failures,
           (unsigned long long)gameplay_services.mixed_audio_frames);
    printf("Interact:     %lu/%lu unsupported %lu\n",
           (unsigned long)gameplay_services.interaction_hits,
           (unsigned long)gameplay_services.interaction_tests,
           (unsigned long)gameplay_services.unsupported_object_calls);
    printf("Modem fire:   %lu tick, %lu/%lu throw, %lu pose reject\n",
           (unsigned long)modem_fire.both_hands_ticks,
           (unsigned long)modem_fire.successful_throws,
           (unsigned long)modem_fire.throw_attempts,
           (unsigned long)modem_fire.pose_rejections);
    printf("PP7 fire:     %lu shot, hit/clear %lu/%lu, sfx %u, beam %u\n",
           (unsigned long)pp7_fire.pp7_shots,
           (unsigned long)pp7_fire.stan_hits,
           (unsigned long)pp7_fire.clear_stan_paths,
           (unsigned int)pp7_fire.last_shot_sound,
           (unsigned int)pp7_fire.last_beam_pose_ready);
    printf("Guard hit:    %lu registered, %lu damage, %lu frontier\n",
           (unsigned long)pp7_fire.guard_hits_registered,
           (unsigned long)pp7_fire.guard_damage_applied,
           (unsigned long)pp7_fire.guard_damage_frontiers);
    printf("Guard fire:   %llu dispatch, %llu sfx, %llu Bond hit %.3f/%.3f\n",
           (unsigned long long)dam_objects->guard_runtime.weapon_fire_dispatches,
           (unsigned long long)dam_objects->guard_runtime.weapon_sound_starts,
           (unsigned long long)dam_objects->guard_runtime.player_damage_events,
           dam_objects->guard_runtime.player_health_damage,
           dam_objects->guard_runtime.player_armour_damage);
    printf("Bond hands:   PP7 %s/%lu, sil %s/%lu, init %s\n",
           first_person_models->status[0]
                   == GE_ORIGINAL_FIRST_PERSON_ASSET_OK ? "native" : "fail",
           (unsigned long)ge_original_first_person_assets_native_node_count(
               &first_person_models->assets, 0U),
           first_person_models->status[1]
                   == GE_ORIGINAL_FIRST_PERSON_ASSET_OK ? "native" : "fail",
           (unsigned long)ge_original_first_person_assets_native_node_count(
               &first_person_models->assets, 1U),
           first_person_models->bond_live.initialized ? "canonical" : "missing");
    printf("Gun scene:    right %s gen %llu, %lu/%lu src, %lu/%lu gpu\n",
           right_hand_published && right_hand.visible
               && right_hand.model != NULL && right_hand.matrices != NULL
                   ? "exact" : "not published",
           (unsigned long long)right_hand.generation,
           (unsigned long)first_person_scene->vertex_count,
           (unsigned long)first_person_scene->batch_count,
           (unsigned long)first_person_scene->render_vertex_count,
           (unsigned long)first_person_scene->render_batch_count);
    printf("Dam loadout:  guns %ld/%ld, %lu item + %lu ammo, %lu model\n",
           (long)first_person_models->bond_live.loadout.starting_weapon[0],
           (long)first_person_models->bond_live.loadout.starting_weapon[1],
           (unsigned long)first_person_models->bond_live.loadout.item_records,
           (unsigned long)first_person_models->bond_live.loadout.ammo_records,
           (unsigned long)first_person_models->bond_live.loadout.projectile_model_requests);
    printf("Gun loader:   %lu call / %lu ok / status %d / %lu tex handoff\n",
           (unsigned long)first_person_models->loader.load_calls,
           (unsigned long)first_person_models->loader.successful_loads,
           (int)first_person_models->loader.last_status,
           (unsigned long)first_person_models->loader.texture_pool_handoffs);
    printf("CPU RSP:      clip %s, audio %s\n",
           clip_stage_ready ? "ready" : "fail",
           audio_abi_ready ? "ready" : "fail");
    printf("Sched ticks:  %llu\n", (unsigned long long)scheduler_ticks);
    if (audio_active) {
        const GeAudioRefillState *audio_stats = ge_3ds_audio_refill_stats();

        printf("NDSP audio:   active (%llu refill, %llu silent)\n",
               (unsigned long long)audio_stats->blocks_prepared,
               (unsigned long long)audio_stats->silent_frames);
    } else {
        printf("NDSP audio:   unavailable (%08lx)\n",
               (unsigned long)(uint32_t)ge_3ds_audio_last_error());
    }
    printf("Engine ticks: %llu (+%u), %llu overload dropped\n",
           (unsigned long long)port->simulation_ticks, ticks,
           (unsigned long long)port->dropped_simulation_ticks);
    printf("GE frame/stg: %ld %ld/%ld\n", (long)currentFrameCounter,
           (long)port->original_stage,
           (long)port->original_requested_stage);
    printf("GE stage time:%ld / %.2fs idle %ld/%ld\n",
           (long)port->original_stage_frames,
           port->original_stage_seconds,
           (long)port->original_idle_frames,
           (long)port->original_idle_latched);
    printf("GE RNG:       %08lx\n", (unsigned long)port->random_sample);
    printf("Frame alpha:  %.2f\n", ge_port_frame_alpha(port));
    printf("GE move:      %+.2f %+.2f\n",
           port->original_move_x, port->original_move_y);
    printf("GE look/btn:  %+.2f %+.2f %04x/%04x\n",
           port->original_look_x, port->original_look_y,
           (unsigned int)port->original_buttons,
           (unsigned int)port->original_buttons_pressed);
    printf("View yaw/pit: %+.2f %+.2f\n", port->view_yaw, port->view_pitch);
    printf("View quat W:  %+.3f\n", port->view_orientation[0]);
    printf("View vector:  %+.2f %+.2f %+.2f\n", view_x, view_y, view_z);
    /* Keep the complete authored bootstrap chain visible at the bottom of
     * the console.  Earlier detailed rows scroll above the 3DS viewport, and
     * a single failed link (notably STAN -> setup -> intro -> player) leaves
     * an otherwise healthy renderer showing only its clear colour. */
    printf("Boot:p%u stan%u/%u[%u:%d/%lu] setup%u(%u/%u/%u) i%u l%u\n",
           assets_mounted ? 1U : 0U,
           dam_collision->loaded ? 1U : 0U,
           dam_collision->original_bound ? 1U : 0U,
           stage_collision_boot_step, stage_collision_boot_status,
           (unsigned long)stage_collision_boot_native_size,
           dam_preview->setup_loaded ? 1U : 0U,
           (unsigned)stage_setup_load_audit,
           (unsigned)stage_setup_stan_audit,
           stage_setup_spawn_audit ? 1U : 0U,
           dam_intro->spawn.player_committed ? 1U : 0U,
           first_person_models->bond_live.initialized ? 1U : 0U);
    printf("Free RAM/LIN: %lu/%lu KiB\n",
           (unsigned long)(osGetMemRegionFree(MEMREGION_APPLICATION) / 1024),
           (unsigned long)(linearSpaceFree() / 1024));
    if (frame_profile.samples != 0U) {
        const uint64_t samples = frame_profile.samples;
        printf("CPU ms avg:   frm %llu sim %llu ovl %llu cam %llu fp %llu gpu %llu\n",
               (unsigned long long)(frame_profile.frame_ms / samples),
               (unsigned long long)(frame_profile.simulation_ms / samples),
               (unsigned long long)(frame_profile.overlay_ms / samples),
               (unsigned long long)(frame_profile.camera_ms / samples),
               (unsigned long long)(frame_profile.first_person_ms / samples),
               (unsigned long long)(frame_profile.gpu_ms / samples));
        frame_profile_total.frame_ms += frame_profile.frame_ms;
        frame_profile_total.simulation_ms += frame_profile.simulation_ms;
        frame_profile_total.overlay_ms += frame_profile.overlay_ms;
        frame_profile_total.camera_ms += frame_profile.camera_ms;
        frame_profile_total.first_person_ms += frame_profile.first_person_ms;
        frame_profile_total.gpu_ms += frame_profile.gpu_ms;
        frame_profile_total.samples += frame_profile.samples;
        memset(&frame_profile, 0, sizeof(frame_profile));
    }
    if (visual_probe_tour.enabled
            && visual_probe_tour.current_view
                < visual_probe_tour.tour.count) {
        const GeVisualProbeView *tour_view =
            &visual_probe_tour.tour.views[visual_probe_tour.current_view];
        const GeDamPreloadRoomState tour_room_state =
            ge_dam_preload_queue_room_state(
                &dam_preview->preload_queue, tour_view->room);

        printf("Tour %lu/%lu r%u s%u %s, %lu p/%llu f\n",
               (unsigned long)visual_probe_tour.current_view + 1UL,
               (unsigned long)visual_probe_tour.tour.count,
               (unsigned int)tour_view->room,
               (unsigned int)tour_room_state,
               ge_dam_dynamic_scene_status_name(
                   dam_preview->dynamic_scene_status),
               (unsigned long)dam_preview->preload_queue.pending_count,
               (unsigned long long)dam_preview->dynamic_scene.install_failures);
        printf("stream t%u/c%u/a%u tex%lu/%lu; %s (%lu/%lu)\n",
               (unsigned int)dam_preview->stream_texture_status,
               (unsigned int)dam_preview->stream_camera_status,
               (unsigned int)dam_preview->stream_allocation_failed,
               (unsigned long)dam_scene_textures.loaded_count,
               (unsigned long)dam_scene_textures.texture_count,
               tour_view->label,
               (unsigned long)visual_probe_tour.view_elapsed_frames,
               (unsigned long)tour_view->hold_frames);
    }
}

static void print_runtime_console_header(
    const GeStageAssetDescriptor *stage_assets, int32_t selected_level_id)
{
    if (stage_assets == NULL) return;
    printf("GoldenEye 007 - native 3DS port\n");
    printf("Stage: %s (LEVELID %ld)\n\n", stage_assets->key,
           (long)selected_level_id);
    printf("Circle Pad   Move\n");
    printf("C-stick      Look\n");
    printf("R / B / Y    Fire / Use / Reload\n");
    printf("A / X        Next / previous weapon\n");
    printf("L / ZR       Aim / crouch\n");
    printf("START        Bond watch / pause\n");
}

int main(void)
{
    C3D_RenderTarget *top_target;
    PrintConsole bottom_console;
    GeAssetPack asset_pack;
    GeTextureCatalog texture_catalog;
    GeTextureCache texture_cache;
    GeTextureCacheEntry texture_cache_entries[16];
    GePortState port;
    GeRetraceScheduler scheduler;
    GeAudioOutput audio_output;
    GeOriginalMusicRuntime *original_music = NULL;
    RuntimeOriginalMusicSync original_music_sync;
    Ge3dsSaveProvider mission_save_provider = {0};
    RuntimeGbiMesh rareware_mesh = {0};
    RuntimeBlotterPreview blotter_preview = {0};
    RuntimeDamPreview dam_preview = {0};
    RuntimeDamCollision dam_collision = {0};
    RuntimeDamIntro dam_intro = {0};
    GeOriginalStageSetupRuntime packaged_stage_setup = {0};
    GeOriginalStageSpawn stage_spawn = {0};
    stagesetup *original_stage_setup = NULL;
    RuntimeOriginalSfxBank original_sfx = {0};
    RuntimeDamWorldObjects dam_objects = {0};
    RuntimeStageOrdinaryObjects stage_ordinary_objects = {0};
    GeOriginalDoorCharacterCollisionProviders door_collision_providers;
    GeOriginalDoorRuntimeProviders door_runtime_providers;
    RuntimeBondAnimations bond_animations = {0};
    RuntimeFirstPersonModels first_person_models = {0};
    RuntimeFirstPersonScene first_person_scene = {0};
    RuntimeInputProbe input_probe = {0};
    RuntimeOriginalFrontend original_frontend_runtime = {0};
    GeOriginalFrontendStart original_frontend = {0};
    RuntimeWatchMissionAbort watch_abort_runtime = {0};
    GeOriginalWatchMissionAbortServiceAdapter watch_abort_services = {0};
    GeOriginalWatchMissionAbort watch_abort = {0};
    const GeStageAssetDescriptor *stage_assets;
    int32_t selected_level_id;
    bool stage_setup_loaded = false;
    bool dam_stage;
    bool is_new_3ds = false;
    bool cstick_available = false;
    bool assets_mounted = false;
    bool texture_catalog_mounted = false;
    bool texture_cache_ready = false;
    bool scheduler_active = false;
    bool audio_output_ready = false;
    bool audio_active = false;
    bool clip_stage_ready = false;
    bool audio_abi_ready = false;
    bool stage_transition_pending = false;
    bool gameplay_stage_ended = false;
    bool run_start_frontend = true;
    bool next_stage_requested = false;
    void *texture_catalog_data = NULL;
    u64 previous_time;
    uint64_t scheduler_ticks = 0U;
    uint64_t rendered_player_generation = 0U;
    unsigned frame_counter = 0;
    int result = 0;

    gfxInitDefault();
    consoleInit(GFX_BOTTOM, &bottom_console);
    stage_assets = load_stage_selection();
    selected_level_id = stage_level_id(stage_assets);
    dam_stage = stage_assets->stage == GE_STAGE_DAM;

    if (R_SUCCEEDED(APT_CheckNew3DS(&is_new_3ds)) && is_new_3ds) {
        osSetSpeedupEnable(true);
        cstick_available = R_SUCCEEDED(irrstInit());
    }

    print_runtime_console_header(stage_assets, selected_level_id);

    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
        printf("\nCould not initialize citro3d.\n");
        result = 1;
        goto exit_graphics;
    }

    top_target = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    if (top_target == NULL) {
        printf("\nCould not create the top-screen target.\n");
        result = 1;
        goto exit_citro3d;
    }
    C3D_RenderTargetSetOutput(top_target, GFX_TOP, GFX_LEFT, DISPLAY_TRANSFER_FLAGS);

    assets_mounted = ge_asset_pack_open(
                         &asset_pack, "sdmc:/3ds/goldeneye-3ds/goldeneye.u.gepack") ==
                     GE_ASSET_PACK_OK;
    texture_catalog_mounted = assets_mounted
        && load_texture_catalog(&asset_pack, &texture_catalog,
                                &texture_catalog_data);
    texture_cache_ready = texture_catalog_mounted
        && ge_texture_cache_init(&texture_cache, &texture_catalog, &asset_pack,
                                 texture_cache_entries,
                                 sizeof(texture_cache_entries)
                                     / sizeof(texture_cache_entries[0]),
                                 2U * 1024U * 1024U,
                                 GE_TEXTURE_FORMAT_RGBA5551,
                                 NULL, NULL, NULL) == GE_TEXTURE_CACHE_OK;

start_stage_runtime:
    memset(&original_music_sync, 0, sizeof(original_music_sync));
    original_music_sync.track[0] = INT32_MIN;
    original_music_sync.track[1] = INT32_MIN;
    original_music_sync.track[2] = INT32_MIN;
    if (assets_mounted) {
        rareware_mesh = load_rareware_display_list(&asset_pack);
        (void)load_rareware_front_model(&asset_pack, &rareware_front_model);
        (void)load_rareware_body_model(&asset_pack, &rareware_body_model);
        (void)load_stage_collision(
            &asset_pack, stage_assets, &dam_collision);
        if (dam_collision.loaded && dam_collision.original_bound) {
            stage_setup_loaded = load_original_stage_setup(
                &asset_pack, stage_assets, &dam_collision,
                &packaged_stage_setup, &stage_spawn,
                &original_stage_setup);
        }
        (void)load_original_sfx_bank(&asset_pack, &original_sfx);
        load_bond_animations(&asset_pack, &bond_animations);
        initialize_first_person_models(&first_person_models, &asset_pack);
    }

    if (!renderer_init(assets_mounted ? &asset_pack : NULL,
                       texture_cache_ready ? &texture_cache : NULL,
                       stage_assets,
                       stage_setup_loaded ? &stage_spawn : NULL,
                       &rareware_mesh, &blotter_preview, &dam_preview)) {
        printf("\nCould not initialize the PICA200 renderer.\n");
        result = 1;
        goto exit_citro3d;
    }
    ge_port_init(&port);
    if (ge_audio_output_init(&audio_output, audio_ring_storage,
                             AUDIO_RING_FRAMES,
                             GE_ORIGINAL_MUSIC_SAMPLE_RATE) == 0) {
        const char *music_path = run_start_frontend
            ? NULL : stage_music_asset_path(selected_level_id);
        audio_output_ready = true;
        ge_libultra_audio_bind(&audio_output);
        if (assets_mounted && music_path != NULL)
            original_music = ge_original_music_runtime_open_asset_pack(
                &asset_pack, music_path, INT16_MAX, &audio_output);
        if (assets_mounted && music_path != NULL
                && original_music == NULL) {
            printf("Could not initialize original music: %s\n",
                   music_path != NULL ? music_path : "unsupported stage");
        }
        audio_active = ge_3ds_audio_init(&audio_output) == 0;
        if (audio_active && original_music != NULL
                && ge_3ds_audio_bind_secondary(
                    ge_original_music_runtime_output(original_music)) != 0) {
            printf("Could not bind original music output.\n");
            goto cleanup_runtime;
        }
    }
    if (ge_3ds_save_provider_init(
            &mission_save_provider, SAVE_SLOT_PATH, FOLDER1)
            == GE_3DS_SAVE_PROVIDER_OK) {
        original_frontend_runtime.save_provider = &mission_save_provider;
    }
    original_frontend_runtime.rareware_mesh = &rareware_mesh;
    original_frontend_runtime.rareware_front = &rareware_front_model;
    original_frontend_runtime.rareware_body = &rareware_body_model;
    if (run_start_frontend && !runtime_diagnostics_requested(stage_assets)) {
        if (!assets_mounted || !texture_cache_ready
                || !initialize_original_frontend_model(
                    &original_frontend_runtime, &asset_pack, &texture_cache,
                    (Vertex *)vertex_buffer
                        + ORIGINAL_FRONTEND_MODEL_VERTEX_OFFSET)) {
            printf("Could not initialize original frontend models.\n");
            goto cleanup_runtime;
        }
        /* The bottom console is a port-development aid, not part of the
         * original startup presentation. Keep it for diagnostic/probe runs,
         * but blank it while the normal legal/logo/gunbarrel/cast and folder
         * frontend owns the application. */
        consoleClear();
        if (!run_original_frontend(
                top_target, cstick_available, &texture_cache,
                &original_frontend_runtime, &original_frontend,
                &original_music, &audio_output, audio_active, true,
                &selected_level_id))
            goto cleanup_runtime;
        (void)ge_3ds_audio_bind_secondary(NULL);
        ge_original_music_runtime_close(original_music);
        {
            const char *music_path =
                stage_music_asset_path(selected_level_id);
            original_music = music_path != NULL
                ? ge_original_music_runtime_open_asset_pack(
                    &asset_pack, music_path, INT16_MAX, &audio_output)
                : NULL;
        }
        if (original_music == NULL) {
            printf("Could not initialize original stage music.\n");
        }
        if (audio_active && original_music != NULL
                && ge_3ds_audio_bind_secondary(
                ge_original_music_runtime_output(original_music)) != 0) {
            printf("Could not bind original Dam music output.\n");
            goto cleanup_runtime;
        }
        run_start_frontend = false;
        if (selected_level_id != stage_level_id(stage_assets)) {
            /* The menu owns stage selection. If a developer stage.cfg chose a
             * different resident scene, discard that bootstrap cleanly and
             * load the frontend's requested authored stage before gameplay. */
            next_stage_requested = true;
            goto cleanup_runtime;
        }
        consoleClear();
        print_runtime_console_header(stage_assets, selected_level_id);
    }
    run_start_frontend = false;
    if (original_frontend_runtime.ramrom_active) {
        const GeOriginalRamromHeader *header =
            &original_frontend_runtime.ramrom_replay.header;
        /* copy_recorded_ramrom_registers_to_proper_place_ingame's
         * retained RNG state, before setup/guard constructors consume it.
         * The US attract recordings all use the live port's existing solo,
         * 1.1-controller and default-aim configuration; front.c's menu-only
         * backing globals are intentionally not part of the 3DS link. */
        g_randomSeed = header->random_seed;
        g_chrObjRandomSeed = header->character_random_seed;
    }
    (void)initialize_dam_prop_state(&dam_objects);
    ge_original_effect_buffers_reset_single_player();
    initialize_original_stage_intro(&dam_collision, &dam_intro,
                                    &dam_objects.props,
                                    original_stage_setup,
                                    selected_level_id,
                                    original_frontend_runtime.ramrom_active
                                        != 0U);
    if (load_visual_probe_tour(&visual_probe_tour, stage_assets)) {
        const GeVisualProbeView *view = ge_visual_probe_tour_view_at(
            &visual_probe_tour.tour, 0U, &visual_probe_tour.current_view);
        if (view != NULL) {
            const bool camera_updated = update_original_dam_camera(
                &dam_preview, view->position, view->look, view->up,
                view->room,
                (Vertex *)vertex_buffer + DAM_ROOM_VERTEX_OFFSET, true);
            visual_probe_record_camera(
                &visual_probe_tour, &dam_preview, camera_updated);
            visual_probe_tour.current_view_ready = camera_updated
                && ge_dam_preload_queue_room_state(
                    &dam_preview.preload_queue, view->room)
                    == GE_DAM_PRELOAD_ROOM_RESIDENT
                && dam_preview.preload_queue.pending_count == 0U;
            printf("\nVisual tour: %lu views / %llu frames\n",
                (unsigned long)visual_probe_tour.tour.count,
                (unsigned long long)visual_probe_tour.tour.total_frames);
            printf("View 1/%lu: %s\n",
                (unsigned long)visual_probe_tour.tour.count, view->label);
        }
    } else if (dam_intro.spawn.player_committed) {
        (void)update_original_dam_camera(
            &dam_preview, dam_intro.player.camera_position,
            dam_intro.player.camera_look, dam_intro.player.camera_up,
            (uint8_t)dam_intro.player.room,
            (Vertex *)vertex_buffer + DAM_ROOM_VERTEX_OFFSET, true);
    }
    if (bond_animations.loaded)
        (void)ge_original_guard_animation_table_bind(
            bond_animations.segment, bond_animations.segment_size);
    ge_original_gameplay_services_reset();
    ge_original_dam_mission_exit_services_reset();
    {
        size_t credits_count = 0U;
        const CreditsEntry *credits = stage_setup_loaded
            ? ge_original_stage_setup_credits(
                &packaged_stage_setup, &credits_count)
            : NULL;
        ge_original_campaign_credits_bind(
            credits, (uint32_t)credits_count);
    }
    {
        GeOriginalWatchMissionAbortOwners watch_abort_owners = {
            .mission_context = &watch_abort_runtime,
            .set_mission_state_zero =
                original_watch_abort_set_mission_state_zero,
            .frontend_context = &watch_abort_runtime,
            .request_title_stage = original_watch_abort_request_title,
            .mark_mission_failed_or_aborted =
                original_watch_abort_mark_aborted,
            .save_context = &watch_abort_runtime,
            .persist_current_folder_settings =
                mission_save_provider.ready
                    ? original_watch_abort_persist_settings : NULL,
            .audio_context = &watch_abort_runtime,
            .play_watch_beep = original_watch_abort_play_beep,
        };
        memset(&watch_abort_runtime, 0, sizeof(watch_abort_runtime));
        watch_abort_runtime.save_provider = mission_save_provider.ready
            ? &mission_save_provider : NULL;
        mission_failed_or_aborted = FALSE;
        watch_item_is_actively_selected = 0;
        D_800409A4 = 0;
        if (!ge_original_watch_mission_abort_services_bind(
                    &watch_abort_services, &watch_abort_owners)
                || !ge_original_watch_mission_abort_reset(
                    &watch_abort,
                    ge_original_watch_mission_abort_services(
                        &watch_abort_services)))
            printf("Watch abort services unavailable.\n");
    }
    /* The mission reset clears the retained file.c provider boundary. Bind
     * the freshly opened slot afterwards so the live end-of-stage body can
     * persist this stage before the report/next-mission frontend runs. */
    ge_original_mission_result_bind(NULL);
    if (mission_save_provider.ready) {
        GeOriginalMissionResultProviders mission_result_providers;
        ge_3ds_save_provider_make_mission_result_providers(
            &mission_save_provider, &mission_result_providers);
        ge_original_mission_result_bind(&mission_result_providers);
    }
    if (!ge_original_mission_result_set_current_mission(
            original_frontend.mission)) {
        printf("Unsupported mission result index %ld.\n",
               (long)original_frontend.mission);
        goto cleanup_runtime;
    }
    ge_original_gameplay_services_bind_settings_persistence(
        mission_save_provider.ready ? &mission_save_provider : NULL,
        mission_save_provider.ready
            ? original_watch_persist_settings_fields : NULL);
    if (first_person_models.ready && dam_intro.spawn.player_committed)
        (void)ge_original_bond_live_initialize(
            first_person_models.buffers[0], first_person_models.buffers[1],
            &first_person_models.bond_live);
    if (first_person_models.bond_live.initialized) {
        /* playerInit establishes g_playerPointers before any dynamic player
         * collision query.  The bounded port binds that canonical table in
         * ge_original_bond_live_initialize, so perform the spawn validation
         * here rather than while the intro record is still being decoded. */
        (void)ge_original_bond_collision_validate_position();
        ge_original_first_person_pose_bind(&first_person_models.pose);
        bind_first_person_loadout(&first_person_models);
        ge_original_gun_live_reset();
        ge_original_gameplay_services_set_exact_gun_dispatch(1);
    }
    initialize_current_player_gait(&bond_animations);
    if (dam_stage) {
#if defined(GE_DAM_STAGE_RUNTIME_LIVE)
        /* The campaign runtime below now owns every authored Dam guard,
         * object, door, room registration and renderer publication. Keep the
         * legacy holder only for the shared prop-pool lifetime and fallback
         * diagnostics; constructing its smaller graph here created duplicate
         * room-list objects and ghost collision before active-list compose. */
        dam_objects.collision = &dam_collision;
        dam_objects.preview = &dam_preview;
        dam_objects.setup = &packaged_stage_setup;
#else
        ge_original_dam_guards_reset();
        dam_objects.guard_status = ge_original_dam_guards_construct_initial();
        if (dam_objects.guard_status == GE_ORIGINAL_DAM_GUARD_OK
                && dam_preview.original_camera_ready)
            dam_objects.guard_status = ge_original_dam_guards_update_matrices(
                dam_preview.original_camera_view);
        (void)ge_original_dam_guard_scene_cache_init(
            &dam_objects.guard_scene_cache);
        materialize_dam_world_objects(
            &dam_objects, assets_mounted ? &asset_pack : NULL,
            &packaged_stage_setup, &dam_collision, &dam_preview);
#if defined(GE_DAM_FULL_PROPS_LIVE)
        ge_original_dam_guard_runtime_reset();
#endif
        dam_objects.mission_flow_live =
            ge_original_dam_mission_flow_begin(&dam_objects.mission_flow) != 0;
        ge_original_door_interaction_bind(NULL, &dam_objects.door_interaction);
        {
        void *visible_props[DAM_NATIVE_OBJECT_CAPACITY
            + GE_ORIGINAL_DAM_INITIAL_GUARD_CAPACITY] = {
            dam_objects.glass_object.prop,
            dam_objects.default_object.prop,
            dam_objects.doors[0].prop,
            dam_objects.doors[1].prop,
        };
        size_t window_index;
        size_t guard_index;
        for (window_index = 0U;
                window_index < GE_ORIGINAL_DAM_SPAWN_WINDOW_COUNT;
                ++window_index)
            visible_props[DAM_BASE_OBJECT_COUNT + window_index] =
                dam_objects.spawn_windows[window_index].prop;
        visible_props[DAM_MATERIALIZED_OBJECT_CAPACITY] =
            dam_objects.mission_tags.tag5_object.prop;
        visible_props[DAM_MATERIALIZED_OBJECT_CAPACITY + 1U] =
            dam_objects.mission_tags.tag4_object.prop;
        for (window_index = 0U; window_index < DAM_ALARM_OBJECT_COUNT;
                ++window_index)
            visible_props[DAM_MATERIALIZED_OBJECT_CAPACITY
                + DAM_MISSION_TAG_OBJECT_COUNT + window_index] =
                    dam_objects.alarms[window_index].prop;
        for (guard_index = 0U;
             guard_index < ge_original_dam_guards_count(); ++guard_index)
            visible_props[DAM_NATIVE_OBJECT_CAPACITY + guard_index] =
                ge_original_dam_guard_prop(guard_index);
        ge_original_gameplay_services_bind_visible_props(
            visible_props, DAM_NATIVE_OBJECT_CAPACITY
                + ge_original_dam_guards_count());
        }
        {
        void *visible_doors[2] = {
            dam_objects.doors[0].prop,
            dam_objects.doors[1].prop,
        };
        (void)ge_original_door_interaction_bind_visible_doors(
            visible_doors, 2U);
        }
        (void)install_dam_world_object_scenes(&dam_preview, &dam_objects);
#endif
    }
#if defined(GE_DAM_STAGE_RUNTIME_LIVE)
    if (stage_setup_loaded) {
#else
    if (!dam_stage && stage_setup_loaded) {
#endif
        (void)initialize_stage_ordinary_objects(
            &stage_ordinary_objects, &asset_pack,
            &packaged_stage_setup, &dam_objects.props,
            &dam_collision, &dam_preview);
    }

    (void)ge_port_start_stage(&port, selected_level_id);
    /* This is the unchanged mp_music.c stage initializer. It chooses the
     * authored main/background state and owns subsequent watch, death and
     * mission transitions; the native runtime only mirrors its three CSeq
     * players into the shared libaudio synth. */
    sub_GAME_7F0C11FC(selected_level_id);
    if (original_music != NULL
            && !sync_original_gameplay_music(
                original_music, &asset_pack, &original_music_sync)) {
        printf("Could not bind original stage music layers.\n");
    }
    if (original_frontend_runtime.ramrom_active
            && !ge_original_input_ramrom_bind(
                original_frontend_runtime.ramrom_replay
                    .header.controller_count)) {
        goto cleanup_runtime;
    }
    if (stage_ordinary_objects.initialized) {
        /* The canonical stage RNG is initialized above before bodyChooseHead
         * consumes it. Guard/provider state remains owned by this persistent
         * runtime while resident-room scene publications are rebuilt. */
        ge_original_stage_mission_runtime_reset_globals(
            &stage_ordinary_objects.mission_runtime);
        if (initialize_stage_guard_objects(
                &stage_ordinary_objects, &asset_pack)) {
            stage_ordinary_objects.init_mask |= UINT32_C(1);
            if (initialize_stage_interactive_objects(
                    &stage_ordinary_objects)) {
                stage_ordinary_objects.init_mask |= UINT32_C(2);
                if (initialize_stage_safe_relations(
                        &stage_ordinary_objects)) {
                    stage_ordinary_objects.init_mask |= UINT32_C(4);
                    if (initialize_stage_objectives(
                            &stage_ordinary_objects))
                        stage_ordinary_objects.init_mask |= UINT32_C(8);
                }
            }
        }
        if (install_stage_ordinary_object_scenes(&stage_ordinary_objects)) {
            (void)upload_dam_gpu_world_scene(
                &dam_preview,
                (Vertex *)vertex_buffer + DAM_ROOM_VERTEX_OFFSET);
            publish_stage_ordinary_visibility(
                &stage_ordinary_objects, true);
        }
        /* Exclude one-time asset relocation/topology bootstrap from the live
         * cumulative combat profile. The callback is observational only. */
        ge_original_model_scene_cache_bind_profile_clock(
            &stage_ordinary_objects.guard_scene_cache,
            runtime_profile_clock, NULL);
        if (initialize_stage_active_props(
                &stage_ordinary_objects, &dam_intro)) {
            ge_original_door_interaction_bind(
                NULL, &stage_ordinary_objects.door_interaction);
            publish_stage_ordinary_visibility(
                &stage_ordinary_objects, true);
        }
        if (dam_stage
                && stage_ordinary_objects.actor_tick_status
                    == RUNTIME_STAGE_ACTOR_TICK_READY) {
            /* initialize_stage_active_props has already allocated all eight
             * authored background actors. The unchanged chrpropTick body is
             * their sole scheduler; reallocating ai_20 here discarded the
             * other seven actors and dispatching it below double-ticked it. */
            dam_objects.mission_flow_live = false;
            dam_objects.full_props_activated = true;
        }
    }
    memset(&door_collision_providers, 0, sizeof(door_collision_providers));
    if (stage_ordinary_objects.actor_tick_status
            == RUNTIME_STAGE_ACTOR_TICK_READY
            && stage_ordinary_objects.live_door_count != 0U) {
        ge_original_door_collision_bind(
            &door_collision_providers,
            &stage_ordinary_objects.door_collision);
    } else if (dam_stage) {
        ge_original_door_collision_bind(
            &door_collision_providers, &dam_objects.door_collision);
    }
    memset(&door_runtime_providers, 0, sizeof(door_runtime_providers));
    door_runtime_providers.context = &port;
    door_runtime_providers.global_timer = dam_door_global_timer;
    door_runtime_providers.clock_timer = dam_door_clock_timer;
    door_runtime_providers.test_collision = ge_original_door_collision_test;
    if (stage_ordinary_objects.actor_tick_status
            == RUNTIME_STAGE_ACTOR_TICK_READY
            && stage_ordinary_objects.live_door_count != 0U) {
        ge_original_door_runtime_bind(
            &door_runtime_providers,
            &stage_ordinary_objects.door_runtime);
    } else if (dam_stage) {
        ge_original_door_runtime_bind(
            &door_runtime_providers, &dam_objects.door_runtime);
    }
    if (!visual_probe_tour.enabled
            && load_input_probe(&input_probe)) {
        /* Stick/button traces are stage-independent and use the same exact
         * live input path. Existing coordinate routes name Dam-authored pads
         * and guards, so never silently run those routes on another stage. */
        if (!dam_stage && input_probe.target_count != 0U) {
            input_probe.enabled = false;
        } else {
        input_probe.move_tick_start =
            first_person_models.bond_live.move_tick_count;
        input_probe_capture_player(&input_probe, &dam_intro);
        input_probe_capture_gates(
            &input_probe, &stage_ordinary_objects, false);
        printf("Input probe:  %lu exact-input frames\n",
               (unsigned long)input_probe.target_frames);
        }
    }
    clip_stage_ready = verify_clip_stage();
    audio_abi_ready = verify_audio_abi_stage();
    ge_original_gameplay_services_bind_audio(
        original_sfx.loaded ? &original_sfx.bank : NULL,
        audio_output_ready ? &audio_output : NULL);
    scheduler_active = ge_retrace_scheduler_init(&scheduler, NULL, NULL) == 0
        && ge_retrace_scheduler_start(&scheduler, 16667U) == 0;
    previous_time = osGetTime();

    while (aptMainLoop()) {
        GePortInput input = read_input(cstick_available);
        GeOriginalRamromStatus ramrom_status = GE_ORIGINAL_RAMROM_OK;
        const u64 current_time = osGetTime();
        const u64 elapsed_milliseconds = current_time - previous_time;
        u64 simulation_start;
        u64 simulation_elapsed_milliseconds;
        u64 gpu_elapsed_milliseconds = 0U;
        const double elapsed = (double)elapsed_milliseconds / 1000.0;
        unsigned ticks;
        GeRetracePumpReport retrace_report;
#if defined(GE_DAM_FULL_PROPS_LIVE)
        bool guard_runtime_updated = false;
#endif
        bool stage_actor_runtime_updated = false;
        const RuntimeFrameProfile frame_profile_before = frame_profile;
        const uint64_t frame_topology_rebuilds_before =
            stage_ordinary_objects.guard_scene_cache.topology_rebuilds;
        const uint64_t frame_topology_component_misses_before =
            stage_ordinary_objects.guard_scene_cache
                .topology_component_misses;
        const uint64_t frame_scene_generation_before =
            dam_preview.dynamic_scene.generation;
        const uint64_t frame_overlay_full_rebuilds_before =
            stage_ordinary_objects.overlay_full_rebuilds;

        if (original_frontend_runtime.ramrom_active) {
            if (input.pressed != 0U) {
                /* ramrom_replay_handler checks the regular controller and
                 * calls ramromFadeToTitle on any rising button edge. */
                original_frontend_runtime.ramrom_return_to_title = 1U;
                ge_original_input_ramrom_unbind();
                run_start_frontend = true;
                next_stage_requested = true;
                break;
            }
            ramrom_status = ge_original_ramrom_replay_next(
                &original_frontend_runtime.ramrom_replay,
                &original_frontend_runtime.ramrom_block);
            if (ramrom_status == GE_ORIGINAL_RAMROM_COMPLETE
                    || (ramrom_status == GE_ORIGINAL_RAMROM_OK
                        && original_frontend_runtime.ramrom_block
                            .random_seed_check != (uint8_t)g_randomSeed)) {
                /* The unchanged callback fades to title on either terminal
                 * block or RNG divergence. Preserve that exact integrity
                 * boundary instead of continuing a desynchronised demo. */
                original_frontend_runtime.ramrom_return_to_title = 1U;
                ge_original_input_ramrom_unbind();
                run_start_frontend = true;
                next_stage_requested = true;
                break;
            }
            if (ramrom_status != GE_ORIGINAL_RAMROM_OK
                    || !ge_original_input_ramrom_queue(
                        &original_frontend_runtime.ramrom_replay,
                        &original_frontend_runtime.ramrom_block)) {
                goto cleanup_runtime;
            }
            original_frontend_runtime.ramrom_block_ready = 1U;
            memset(&input, 0, sizeof(input));
        }

        simulation_start = osGetTime();

        if (input_probe.enabled) {
            float aim_position[3];
            bool aim_guard_complete = false;
            const size_t aim_target_index = input_probe.target_index;
            const float *aim = input_probe_live_guard_aim_position(
                    &input_probe, &stage_ordinary_objects,
                    dam_preview.original_camera_ready
                        ? dam_preview.original_camera_view_to_world : NULL,
                    aim_position,
                    &aim_guard_complete)
                ? aim_position : NULL;
            input = input_probe_sample(
                &input_probe, &dam_intro.player, aim, aim_guard_complete);
            if (aim_target_index < input_probe.target_count
                    && input_probe.targets[aim_target_index].aim_chr >= 0
                    && dam_intro.player.initialized) {
                size_t axis;
                ++input_probe.aim_command_samples;
                input_probe.last_aim_target_index =
                    (uint32_t)aim_target_index;
                input_probe.last_aim_route_frame =
                    input_probe_route_frame(&input_probe);
                input_probe.last_aim_dwell_remaining =
                    input_probe.target_dwell_remaining;
                input_probe.last_aim_held = input.held;
                input_probe.last_aim_command_look[0] = input.look_x;
                input_probe.last_aim_command_look[1] = input.look_y;
                for (axis = 0U; axis < 3U; ++axis) {
                    input_probe.last_aim_camera_position[axis] =
                        dam_intro.player.camera_position[axis];
                    input_probe.last_aim_camera_look[axis] =
                        dam_intro.player.camera_look[axis];
                }
            }
        }

        if (original_frontend_runtime.ramrom_active) {
            ticks = ge_port_advance_retraces(
                &port,
                original_frontend_runtime.ramrom_block.speed_frames,
                &input);
            original_frontend_runtime.ramrom_block_ready = 0U;
        } else if (scheduler_active
                && ge_retrace_scheduler_pump(
                    &scheduler, elapsed_milliseconds * 1000U,
                    SIZE_MAX, &retrace_report) == 0) {
            const unsigned retrace_frames = retrace_report.generated_ticks
                    > UINT_MAX
                ? UINT_MAX
                : (unsigned)retrace_report.generated_ticks;
            scheduler_ticks += retrace_report.generated_ticks;
            ticks = ge_port_advance_retraces(
                &port, retrace_frames, &input);
        } else {
            /* The elapsed-time accumulator remains a safe fallback if the
             * native retrace service could not be started. */
            ticks = ge_port_advance_bounded(&port, elapsed, &input, 1U);
        }
        if (audio_active) {
            ge_3ds_audio_pump();
        }
        previous_time = current_time;

        if (ticks == 0U && !original_frontend_runtime.ramrom_active) {
            /* No original retrace is ready. Input edges are already latched
             * by ge_port_advance_* and audio has been serviced above. Do not
             * rebuild/submit the same scene or retick render-owned HUD state.
             * This short idle yield never delays a ready gameplay pass and
             * does not serialize a completed frame against display VBlank. */
            ++fine_profile.idle_present_skips;
            svcSleepThread(1000000LL);
            continue;
        }
        {
            const uint64_t frame_begin_start = svcGetSystemTick();
            C3D_FrameBegin(0);
            fine_profile.frame_begin_ticks += svcGetSystemTick() - frame_begin_start;
        }

        {
            unsigned original_tick;
            for (original_tick = 0U; original_tick < ticks; original_tick++) {
                GeOriginalDynFrameAudit dyn_frame_audit;
                bool gun_tick_complete = false;
                if (stage_ordinary_objects.actor_tick_status
                        == RUNTIME_STAGE_ACTOR_TICK_READY) {
                    /* Exact bossMainloop/lvlManageMpGame boundary: background
                     * mission AI advances before the player-id shuffle and
                     * before lvlViewMoveTick/MoveBond. The paired propsTick
                     * remains at lvlRender below. */
                    stage_ordinary_objects.active_prop_status =
                        ge_original_stage_active_props_pre_tick_exact(
                            &stage_ordinary_objects.active_props);
                }
                /* bossMainloop calls the unchanged four-slot player shuffle
                 * immediately after lvlManageMpGame and before every
                 * lvlViewMoveTick. It remains required in solo play: besides
                 * ordering player props it advances the recorded gameplay
                 * RNG exactly three times per presented simulation pass. */
                shuffle_player_ids();
                ge_original_music_port_tick();
                if (original_music != NULL
                        && !sync_original_gameplay_music(
                            original_music, &asset_pack,
                            &original_music_sync)) {
                    printf("Original music layer sync failed.\n");
                }
                if (audio_active && original_music != NULL
                        && ge_original_music_runtime_tick_60hz(
                            original_music) != GE_AUDIO_ABI_OK) {
                    printf("Original music render failed.\n");
                    (void)ge_3ds_audio_bind_secondary(NULL);
                    ge_original_music_runtime_close(original_music);
                    original_music = NULL;
                }
                const bool dyn_frame_active =
                    ge_original_gun_live_frame_begin() != 0;
                /* Canonical viewport order updates colour-screen state before
                 * MoveBond and evaluates its exit tail immediately after. */
                ge_original_dam_mission_exit_services_tick();
                if (dam_preview.environment_ready
                        && dam_preview.environment.clouds != 0U)
                    ge_dam_sky_tick(&dam_cloud_offset,
                                    port.original_clock_timer);
                if (first_person_models.bond_live.initialized) {
                    /* Canonical bondviewMovePlayerUpdateViewport publishes
                     * these three persisted options immediately before its
                     * MoveBond call.  The native renderer owns the VI-only
                     * portion of that function, but gameplay must retain the
                     * same decompiled option order so this frame's input can
                     * consume the auto-aim target selected after the prior
                     * propsTick. */
                    currentPlayerSetYAutoAimEnabled(
                        cur_player_get_autoaim());
                    currentPlayerSetXAutoAimEnabled(
                        cur_player_get_autoaim());
                    currentPlayerSetLookAheadSetting(
                        cur_player_get_lookahead());
                    gunSetGunAmmoVisible(
                        GUNAMMOREASON_OPTION,
                        cur_player_get_ammo_onscreen_setting());
                    /* The native port is currently single-player, making
                     * this the exact live branch of the original
                     * getPlayerCount/g_playerPerm expression. */
                    gunSetSightVisible(
                        GUNSIGHTREASON_1,
                        cur_player_get_sight_onscreen_control());
                    (void)ge_original_bond_move_live_tick(
                        &first_person_models.bond_live,
                        port.original_clock_timer,
                        port.original_global_timer,
                        port.original_global_timer_delta);
                    {
                        GeOriginalBondMotionSnapshot watch_motion;
                        if (watch_abort.bound
                                && ge_original_bond_live_motion_snapshot(
                                    &watch_motion)
                                && watch_motion.watch_animation_state
                                    == WATCH_ANIMATION_0x5
                                && watch_screen_index
                                    == GE_ORIGINAL_WATCH_ABORT_MISSION_STATUS_PAGE) {
                            GeOriginalBondInputFrame watch_input;
                            ge_original_input_read_bond_frame(
                                0U, &watch_input);
                            /* draw_watch_current_page owns this presentation
                             * mutation in the original. Publish its selected
                             * bit back to the retained navigation body, while
                             * the adapter keeps the confirm bit private until
                             * the missing GBI page renderer is linked. This
                             * prevents the retained navigation body from
                             * issuing the same abort a second time. */
                            watch_abort.item_selected =
                                watch_item_is_actively_selected != 0;
                            (void)ge_original_watch_mission_abort_frame_tick(
                                &watch_abort,
                                port.original_buttons_pressed,
                                port.original_buttons,
                                watch_input.stick_x);
                            watch_item_is_actively_selected =
                                watch_abort.item_selected != 0;
                        }
                    }
                    ge_original_dam_mission_exit_process_input_exact(
                        port.original_buttons);
                    if (!visual_probe_tour.enabled
                            && dam_intro.player.initialized
                            && dam_intro.player.publication_generation
                                != rendered_player_generation
                            ) {
                        const u64 camera_start = osGetTime();
                        const bool camera_updated = update_original_dam_camera(
                                &dam_preview,
                                dam_intro.player.camera_position,
                                dam_intro.player.camera_look,
                                dam_intro.player.camera_up,
                                (uint8_t)dam_intro.player.room,
                                (Vertex *)vertex_buffer
                                    + DAM_ROOM_VERTEX_OFFSET,
                                false);
                        frame_profile.camera_ms += osGetTime() - camera_start;
                        if (camera_updated) {
                            if (dam_stage && dam_objects.guard_status
                                    == GE_ORIGINAL_DAM_GUARD_OK)
                                dam_objects.guard_status =
                                    ge_original_dam_guards_update_matrices(
                                        dam_preview.original_camera_view);
                            if (stage_ordinary_objects.actor_tick_status
                                    == RUNTIME_STAGE_ACTOR_TICK_READY) {
                                (void)refresh_stage_ordinary_object_scenes(
                                    &stage_ordinary_objects,
                                    (Vertex *)vertex_buffer
                                        + DAM_ROOM_VERTEX_OFFSET);
                                publish_stage_ordinary_visibility(
                                    &stage_ordinary_objects, true);
                            }
                        }
                    }
                    sync_first_person_pose_model(&first_person_models);
                    gun_tick_complete = ge_original_gun_live_tick() != 0;
                    if (gun_tick_complete)
                        first_person_models.pose_status =
                            GE_ORIGINAL_FIRST_PERSON_POSE_OK;
                }
                if (stage_ordinary_objects.actor_tick_status
                        == RUNTIME_STAGE_ACTOR_TICK_READY) {
                    tick_stage_interaction(&stage_ordinary_objects);
                    /* Attribute the exact interaction edge before any later
                     * catch-up tick can replace last_prop. Door travel and
                     * overlap are sampled again after active-prop ticking. */
                    if (input_probe.enabled)
                        input_probe_capture_gates(
                            &input_probe, &stage_ordinary_objects, false);
                } else if (dam_stage) {
                    tick_dam_interaction(&dam_objects);
                }
                if (dam_objects.mission_flow_live
                        && stage_ordinary_objects.actor_tick_status
                            != RUNTIME_STAGE_ACTOR_TICK_READY)
                    (void)ge_original_dam_mission_flow_tick(
                        &dam_objects.mission_flow);
                if (dam_stage
                        && stage_ordinary_objects.actor_tick_status
                            != RUNTIME_STAGE_ACTOR_TICK_READY
                        && dam_objects.objective_runtime.bound)
                    (void)tick_dam_objectives(&dam_objects);
                ge_original_gameplay_services_tick();
                if (audio_output_ready && !audio_active)
                    (void)ge_audio_output_discard(
                        &audio_output,
                        ge_audio_output_queued(&audio_output));
                if (stage_ordinary_objects.actor_tick_status
                            == RUNTIME_STAGE_ACTOR_TICK_READY
                        && dyn_frame_active && gun_tick_complete
                        && dam_preview.original_camera_ready) {
                    stage_ordinary_objects.active_prop_status =
                        GE_ORIGINAL_STAGE_ACTIVE_PROP_INVALID_ARGUMENT;
                    if (input_probe.enabled || GE_3DS_LIVE_DIAGNOSTICS) {
                        RuntimeStageGuardCombatAudit combat_before;
                        RuntimeStageGuardCombatAudit combat_after;
                        stage_guard_combat_audit(
                            &stage_ordinary_objects, &combat_before);
                        stage_ordinary_objects.active_prop_status =
                            ge_original_stage_active_props_tick_exact(
                                &stage_ordinary_objects.active_props);
                        if (stage_ordinary_objects.active_prop_status
                                == GE_ORIGINAL_STAGE_ACTIVE_PROP_OK) {
                            /* Exact lv.c ordering: target selection consumes
                             * the ONSCREEN flags produced for this canonical
                             * prop state before interaction/pickup ticking. */
                            chraiUpdateOnscreenPropCount();
                            chrpropUpdateAutoaimTarget();
                            ge_original_stage_props_tick_player_exact();
                        }
                        if (input_probe.enabled)
                            input_probe_capture_gates(
                                &input_probe, &stage_ordinary_objects, true);
                        stage_guard_combat_audit(
                            &stage_ordinary_objects, &combat_after);
                        stage_guard_combat_record(&stage_ordinary_objects,
                            &combat_before, &combat_after);
                    } else {
                        stage_ordinary_objects.active_prop_status =
                            ge_original_stage_active_props_tick_exact(
                                &stage_ordinary_objects.active_props);
                        if (stage_ordinary_objects.active_prop_status
                                == GE_ORIGINAL_STAGE_ACTIVE_PROP_OK) {
                            chraiUpdateOnscreenPropCount();
                            chrpropUpdateAutoaimTarget();
                            ge_original_stage_props_tick_player_exact();
                        }
                    }
                    stage_ordinary_objects.last_mission_tick_status =
                        UINT32_MAX;
                    stage_ordinary_objects.last_monitor_tick_ok = UINT32_MAX;
                    stage_ordinary_objects.last_objective_tick_ok = UINT32_MAX;
                    stage_ordinary_objects.last_guard_lighting_status =
                        UINT32_MAX;
                    stage_ordinary_objects.last_guard_matrix_status =
                        UINT32_MAX;
                    if (stage_ordinary_objects.active_prop_status
                            == GE_ORIGINAL_STAGE_ACTIVE_PROP_OK) {
                        stage_ordinary_objects.last_mission_tick_status =
                            ge_original_stage_mission_runtime_observe_tick(
                                &stage_ordinary_objects.mission_runtime);
                    }
                    if (stage_ordinary_objects.last_mission_tick_status
                            == GE_ORIGINAL_STAGE_MISSION_RUNTIME_OK) {
                        stage_ordinary_objects.last_monitor_tick_ok =
                            tick_stage_monitors(&stage_ordinary_objects);
                    }
                    if (stage_ordinary_objects.last_monitor_tick_ok == 1U) {
                        stage_ordinary_objects.last_objective_tick_ok =
                            tick_stage_objectives(&stage_ordinary_objects);
                    }
                    if (stage_ordinary_objects.last_objective_tick_ok == 1U) {
                        /* The unchanged chrTick in propsTick has already
                         * sampled set_color_shading_from_tile and advanced
                         * shadecol toward nextcol in its exact visible branch
                         * (or snapped it in the culled branch).  Calling the
                         * stage adapter here a second time both walked every
                         * guard STAN again and made visible lighting converge
                         * twice as fast as the original game. */
                        stage_ordinary_objects.last_guard_lighting_status =
                            GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK;
                    }
                    if (stage_ordinary_objects.last_guard_lighting_status
                            == GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK) {
                        const uint64_t matrix_start = svcGetSystemTick();
                        stage_ordinary_objects.last_guard_matrix_status =
                            ge_original_stage_guard_runtime_update_matrices(
                                stage_ordinary_objects.guards,
                                dam_preview.original_camera_view);
                        fine_profile.guard_matrix_ticks +=
                            svcGetSystemTick() - matrix_start;
                        fine_profile.guard_matrix_calls++;
                    }
                    if (stage_ordinary_objects.last_guard_matrix_status
                            == GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK) {
                        stage_ordinary_objects.actor_tick_count++;
                        stage_actor_runtime_updated = true;
                    } else {
                        stage_ordinary_objects.actor_tick_status =
                            RUNTIME_STAGE_ACTOR_TICK_RUNTIME_FAILED;
                    }
                }
                if (dam_stage && dam_objects.doors_linked
                        && stage_ordinary_objects.actor_tick_status
                            != RUNTIME_STAGE_ACTOR_TICK_READY) {
#if defined(GE_DAM_FULL_PROPS_LIVE)
                    if (dyn_frame_active && gun_tick_complete
                            && dam_preview.original_camera_ready
                            && (dam_objects.full_props_activated
                                || dam_full_props_live_ready(&dam_objects))) {
                        const GeOriginalDamGuardRuntimeStatus runtime_status =
                            ge_original_dam_guard_runtime_tick(
                                dam_preview.original_camera_view);
                        ge_original_dam_guard_runtime_snapshot(
                            &dam_objects.guard_runtime);
                        if (runtime_status
                                == GE_ORIGINAL_DAM_GUARD_RUNTIME_OK) {
                            dam_objects.full_props_activated = true;
                            guard_runtime_updated = true;
                        }
                    }
#else
                    (void)ge_original_door_runtime_tick(
                        dam_objects.state.first_door.definition);
                    if (dam_world_object_scenes_need_refresh(&dam_objects)
                            && install_dam_world_object_scenes(
                                &dam_preview, &dam_objects))
                        rendered_player_generation = UINT64_MAX;
#endif
                }
                {
                    GeOriginalPosendCameraSnapshot orbit_camera;
                    if (ge_original_campaign_posend_camera_tick_exact(
                            &orbit_camera)
                            && orbit_camera.valid) {
                        const u64 camera_start = osGetTime();
                        (void)update_original_dam_camera(
                            &dam_preview,
                            orbit_camera.position,
                            orbit_camera.look_direction,
                            orbit_camera.up,
                            orbit_camera.room,
                            (Vertex *)vertex_buffer
                                + DAM_ROOM_VERTEX_OFFSET,
                            false);
                        frame_profile.camera_ms +=
                            osGetTime() - camera_start;
                    }
                }
                if (dyn_frame_active)
                    (void)ge_original_gun_live_frame_finalize(
                        &dyn_frame_audit);
                if (input_probe.enabled
                        && input_probe.simulation_frames != UINT32_MAX) {
                    input_probe_capture_armour(&input_probe);
                    ++input_probe.simulation_frames;
                }
            }
        }
        {
            GeOriginalBossState boss_state;
            ge_original_boss_snapshot(&boss_state);
            stage_transition_pending =
                boss_state.requested_stage != LEVELID_NONE;
        }
#if defined(GE_DAM_FULL_PROPS_LIVE)
        if (dam_stage && (dam_objects.objective_runtime.bound
                || stage_ordinary_objects.objective_runtime.bound))
            display_objective_status_text_on_status_change();
#endif
#if defined(GE_DAM_FULL_PROPS_LIVE)
        if (guard_runtime_updated
                && stage_ordinary_objects.actor_tick_status
                    != RUNTIME_STAGE_ACTOR_TICK_READY) {
            const u64 overlay_start = osGetTime();

            /* A slow display frame can consume several original retraces.
             * Keep every canonical simulation tick above, then publish only
             * the final state once for this displayed frame.  Rebuilding the
             * same guard/door GBI overlay after every catch-up tick caused a
             * positive feedback loop at low frame rates and did not expose
             * any of the intermediate states to the GPU. */
            /* Door/guard publication uploads its exact changed range and
             * advances gpu_uploaded_scene_generation itself.  It does not
             * invalidate Bond's camera publication: coupling these
             * generations made every animated guard force a redundant
             * camera/portal pass on the next simulation tick. */
            (void)refresh_dam_live_overlays(
                &dam_preview, &dam_objects,
                (Vertex *)vertex_buffer + DAM_ROOM_VERTEX_OFFSET);
            frame_profile.overlay_ms += osGetTime() - overlay_start;
            /* Exact renderer publication owns this list. Run it after the
             * final guard/object state has supplied ONSCREEN flags and
             * zDepth, so stale PropRecords cannot reach the next frame's
             * gun/interaction queries. */
            chraiUpdateOnscreenPropCount();
            chrpropUpdateAutoaimTarget();
        }
#endif
        if (stage_actor_runtime_updated) {
            const u64 overlay_start = osGetTime();

            if (refresh_stage_live_overlays(&stage_ordinary_objects,
                    (Vertex *)vertex_buffer + DAM_ROOM_VERTEX_OFFSET)) {
                publish_stage_ordinary_visibility(
                    &stage_ordinary_objects, false);
                /* refresh_stage_live_overlays has already committed and
                 * uploaded the exact door/guard ranges. Keep the independent
                 * player generation intact so stationary combat does not
                 * rerun camera visibility—and so a slow displayed frame
                 * cannot repeat that work for each catch-up tick. */
                (void)0;
            }
            frame_profile.overlay_ms += osGetTime() - overlay_start;
            /* Publish the final display-frame visibility list for the next
             * exact per-tick propsTick -> autoaim -> propsTickPlayer pass.
             * Target selection itself remains inside that canonical order. */
            chraiUpdateOnscreenPropCount();
        }
        simulation_elapsed_milliseconds = osGetTime() - simulation_start;
        frame_profile.simulation_ms += simulation_elapsed_milliseconds;

        visual_probe_tour.native_actor_tick_status =
            (uint32_t)stage_ordinary_objects.actor_tick_status;
        visual_probe_tour.native_actor_service_status =
            (uint32_t)stage_ordinary_objects.active_prop_status;
        visual_probe_tour.native_actor_prop_count =
            stage_ordinary_objects.active_prop_count;
        visual_probe_tour.native_actor_materializer_ready_count =
            stage_ordinary_objects.report.ready;
        visual_probe_tour.native_actor_materializer_constructed_count =
            stage_ordinary_objects.report.constructed;
        visual_probe_tour.native_actor_materializer_failed_count =
            stage_ordinary_objects.report.failed;
        visual_probe_tour.native_actor_materialized_live_count =
            stage_ordinary_objects.live_count;
        visual_probe_tour.native_owned_ordinary_embedded_count =
            stage_ordinary_objects.owned_ordinary_embedded_count;
        visual_probe_tour.native_owned_ordinary_assigned_count =
            stage_ordinary_objects.owned_ordinary_assigned_count;
        visual_probe_tour.native_owned_ordinary_pending_count =
            stage_ordinary_objects.owned_ordinary_pending_count;
        visual_probe_tour.native_actor_first_failed_command = SIZE_MAX;
        {
            size_t failed_entry;
            for (failed_entry = 0U;
                    failed_entry < stage_ordinary_objects.entry_count;
                    ++failed_entry) {
                const RuntimeStageOrdinaryEntry *entry =
                    &stage_ordinary_objects.entries[failed_entry];
                if (entry->live) continue;
                visual_probe_tour.native_actor_first_failed_command =
                    entry->command_index;
                visual_probe_tour.native_actor_first_failed_type = entry->type;
                visual_probe_tour.native_actor_first_failed_construct_status =
                    (uint32_t)entry->construct_status;
                visual_probe_tour.native_actor_first_failed_placement_status =
                    (uint32_t)entry->placement_status;
                break;
            }
        }
        visual_probe_tour.native_actor_authored_weapon_count =
            stage_ordinary_objects.guard_weapon_report
                .authored_assigned_collectables;
        visual_probe_tour.native_actor_attached_weapon_count =
            stage_ordinary_objects.guard_weapon_count;
        visual_probe_tour.native_actor_attached_hat_count =
            stage_ordinary_objects.guard_hat_count;
        visual_probe_tour.native_actor_guard_overlay_updates =
            stage_ordinary_objects.guard_overlay_updates;
        visual_probe_tour.native_actor_door_overlay_updates =
            stage_ordinary_objects.door_overlay_updates;
        visual_probe_tour.native_actor_overlay_full_rebuilds =
            stage_ordinary_objects.overlay_full_rebuilds;
        visual_probe_tour.native_actor_guard_cache_builds =
            stage_ordinary_objects.guard_scene_cache.cached_builds;
        visual_probe_tour.native_actor_guard_cache_topology_rebuilds =
            stage_ordinary_objects.guard_scene_cache.topology_rebuilds;
        visual_probe_tour.native_actor_door_cache_builds =
            stage_ordinary_objects.door_scene_cache.cached_builds;
        visual_probe_tour.native_actor_door_cache_topology_rebuilds =
            stage_ordinary_objects.door_scene_cache.topology_rebuilds;
        visual_probe_tour.native_actor_tick_count =
            stage_ordinary_objects.actor_tick_count;
        visual_probe_tour.native_mission_actor_count =
            stage_ordinary_objects.mission_runtime
                .live_background_actor_count;
        visual_probe_tour.native_mission_tick_count =
            stage_ordinary_objects.mission_runtime.observed_ticks;
        visual_probe_tour.native_mission_ai_offset_hash =
            stage_ordinary_objects.mission_runtime.ai_offset_hash;
        visual_probe_tour.native_monitor_count =
            stage_ordinary_objects.monitor_count;
        visual_probe_tour.native_monitor_screen_count =
            stage_ordinary_objects.monitor_screen_count;
        visual_probe_tour.native_monitor_tick_count =
            stage_ordinary_objects.monitor_tick_count;
        visual_probe_tour.native_monitor_noop_tick_count =
            stage_ordinary_objects.monitor_noop_tick_count;
        visual_probe_tour.native_monitor_surface_update_count =
            stage_ordinary_objects.monitor_surface_update_count;
        visual_probe_tour.native_monitor_surface_unchanged_count =
            stage_ordinary_objects.monitor_surface_unchanged_count;
        visual_probe_tour.native_monitor_surface_failure_count =
            stage_ordinary_objects.monitor_surface_failure_count;
        visual_probe_tour.native_articulated_scene_update_count =
            stage_ordinary_objects.articulated_scene_update_count;
        visual_probe_tour.native_articulated_scene_unchanged_count =
            stage_ordinary_objects.articulated_scene_unchanged_count;
        visual_probe_tour.native_articulated_scene_topology_change_count =
            stage_ordinary_objects.articulated_scene_topology_change_count;
        visual_probe_tour.native_articulated_scene_failure_count =
            stage_ordinary_objects.articulated_scene_failure_count;
        visual_probe_tour.native_supply_count =
            stage_ordinary_objects.supply_count;
        visual_probe_tour.native_supply_slot_model_load_count =
            stage_ordinary_objects.supply_slot_model_load_count;
        visual_probe_tour.native_tinted_glass_count =
            stage_ordinary_objects.tinted_glass_count;
        visual_probe_tour.native_cctv_count =
            stage_ordinary_objects.cctv_count;
        visual_probe_tour.native_autogun_count =
            stage_ordinary_objects.autogun_count;
        visual_probe_tour.native_gas_releasing_count =
            stage_ordinary_objects.gas_releasing_count;
        visual_probe_tour.native_safe_count =
            stage_ordinary_objects.safe_count;
        visual_probe_tour.native_safe_relation_count =
            stage_ordinary_objects.safe_relation_count;
        visual_probe_tour.native_safe_status =
            (uint32_t)stage_ordinary_objects.safe_runtime.status;
        visual_probe_tour.native_safe_relation_status =
            (uint32_t)stage_ordinary_objects.safe_relation_status;
        visual_probe_tour.native_stage_init_mask =
            stage_ordinary_objects.init_mask;
        visual_probe_tour.native_door_interaction_tick_count =
            stage_ordinary_objects.door_interaction.ticks;
        visual_probe_tour.native_door_interaction_activation_count =
            stage_ordinary_objects.door_interaction.activations;
        visual_probe_tour.native_objective_count =
            stage_ordinary_objects.objectives.objective_entry_count;
        visual_probe_tour.native_objective_criterion_count =
            stage_ordinary_objects.objectives.criterion_count;
        visual_probe_tour.native_objective_blocked_tag_count =
            stage_ordinary_objects.objectives.blocked_tag_count;
        visual_probe_tour.native_objective_evaluation_ready_count =
            stage_ordinary_objects.objective_evaluation_ready_count;
        visual_probe_tour.native_objective_evaluation_blocked_count =
            stage_ordinary_objects.objective_evaluation_blocked_count;
        visual_probe_tour.native_objective_evaluation_ticks =
            stage_ordinary_objects.objective_evaluation_ticks;
        if (dam_stage) {
            /* Dam owns the earlier dedicated exact propsTick/mission slice,
             * whereas the campaign-wide actor telemetry above intentionally
             * describes RuntimeStageOrdinaryObjects. Publish both explicitly
             * so a completed Dam route proves that the live canonical path
             * advanced instead of appearing as an all-zero actor runtime. */
            const bool stage_runtime_live =
                stage_ordinary_objects.actor_tick_status
                    == RUNTIME_STAGE_ACTOR_TICK_READY;
            visual_probe_tour.native_dam_guard_tick_count =
                stage_runtime_live
                    ? stage_ordinary_objects.active_props.ticks
                    : dam_objects.guard_runtime.ticks;
            visual_probe_tour.native_dam_guard_rejected_tick_count =
                stage_runtime_live ? 0U
                    : dam_objects.guard_runtime.rejected_ticks;
            visual_probe_tour.native_dam_guard_last_status =
                stage_runtime_live
                    ? (uint32_t)stage_ordinary_objects.active_prop_status
                    : dam_objects.guard_runtime.last_status;
            visual_probe_tour.native_dam_guard_weapon_fire_count =
                dam_objects.guard_runtime.weapon_fire_dispatches;
            visual_probe_tour.native_dam_guard_player_damage_count =
                dam_objects.guard_runtime.player_damage_events;
            visual_probe_tour.native_dam_door_interaction_tick_count =
                stage_runtime_live
                    ? stage_ordinary_objects.door_interaction.ticks
                    : dam_objects.door_interaction.ticks;
            visual_probe_tour.native_dam_door_interaction_activation_count =
                stage_runtime_live
                    ? stage_ordinary_objects.door_interaction.activations
                    : dam_objects.door_interaction.activations;
            visual_probe_tour.native_dam_alarm_count =
                stage_runtime_live
                    ? stage_ordinary_objects.alarm_count
                    : dam_objects.alarm_count;
            visual_probe_tour.native_dam_alarm_model_status =
                (uint32_t)ge_original_pitem_model_last_status(
                    dam_objects.pitem_models);
            visual_probe_tour.native_dam_alarm_misc_status =
                (uint32_t)dam_objects.alarms[0].status;
            visual_probe_tour.native_dam_alarm_construct_status =
                (uint32_t)dam_objects.alarms[0].construct_status;
            visual_probe_tour.native_dam_alarm_placement_status =
                (uint32_t)dam_objects.alarms[0].placement_status;
            visual_probe_tour.native_dam_alarm_instance_bits =
                (dam_objects.alarms[0].prop != NULL ? 1U : 0U)
                | (dam_objects.alarms[0].instance.prop
                        == dam_objects.alarms[0].prop ? 2U : 0U)
                | (dam_objects.alarms[0].instance.model != NULL ? 4U : 0U)
                | (dam_objects.alarms[0].instance.constructed ? 8U : 0U)
                | (dam_objects.alarms[0].instance.activated ? 16U : 0U)
                | (dam_objects.alarms[0].live ? 32U : 0U);
            visual_probe_tour.native_dam_alarm_scan_count =
                dam_objects.alarm_scan_count;
            visual_probe_tour.native_dam_alarm_materialize_failure =
                dam_objects.alarm_materialize_failure;
            visual_probe_tour.native_dam_alarm_scene_part_count =
                ge_original_pitem_model_scene_part_count(
                    dam_objects.pitem_models, 1);
            visual_probe_tour.native_dam_model_scene_status =
                (uint32_t)dam_objects.model_scene_status;
            visual_probe_tour.native_dam_model_scene_ready =
                dam_objects.model_scene_ready ? 1U : 0U;
            visual_probe_tour.native_dam_scene_prerequisite_bits =
                (dam_preview.dynamic_scene.initialized ? 1U : 0U)
                | (dam_preview.texture_cache != NULL ? 2U : 0U)
                | (dam_objects.model62 != NULL ? 4U : 0U)
                | (dam_objects.model104[0] != NULL
                    && dam_objects.model104[
                        DAM_WINDOW_MODEL_INSTANCE_COUNT - 1U] != NULL
                        ? 8U : 0U)
                | (dam_objects.model178[0] != NULL
                    && dam_objects.model178[1] != NULL ? 16U : 0U)
                | (dam_objects.objective_models != NULL ? 32U : 0U)
                | (dam_objects.guard_model_loaded
                    && dam_objects.guard_weapon_model_loaded ? 64U : 0U)
                | (dam_preview.original_camera_ready ? 128U : 0U)
                | (dam_objects.glass_object.placement_completed
                    ? 256U : 0U)
                | (dam_objects.default_object.placement_completed
                    ? 512U : 0U)
                | (dam_objects.mission_objects[0].placement_completed
                    ? 1024U : 0U)
                | (dam_objects.mission_objects[1].placement_completed
                    ? 2048U : 0U)
                | (dam_objects.alarm_count == DAM_ALARM_OBJECT_COUNT
                    ? 4096U : 0U)
                | (dam_objects.doors[0].constructed ? 8192U : 0U)
                | (dam_objects.doors[1].constructed ? 16384U : 0U);
            visual_probe_tour.native_dam_scene_install_failure =
                dam_objects.model_scene_install_failure;
            visual_probe_tour.native_dam_alarm_status =
                stage_runtime_live
                    ? (uint32_t)stage_ordinary_objects
                        .alarm_interaction.status
                    : (uint32_t)dam_objects.alarm_interaction.status;
            visual_probe_tour.native_dam_alarm_interaction_tick_count =
                stage_runtime_live
                    ? stage_ordinary_objects.alarm_interaction_ticks
                    : dam_objects.alarm_interaction_ticks;
            visual_probe_tour.native_dam_alarm_interaction_activation_count =
                stage_runtime_live
                    ? stage_ordinary_objects.alarm_interaction_activations
                    : dam_objects.alarm_interaction_activations;
            visual_probe_tour.native_dam_objective_count = stage_runtime_live
                ? stage_ordinary_objects.objectives.objective_entry_count
                : dam_objects.objectives.objective_entry_count;
            visual_probe_tour.native_dam_objective_blocked_tag_count =
                stage_runtime_live
                    ? stage_ordinary_objects.objectives.blocked_tag_count
                    : dam_objects.objectives.blocked_tag_count;
            visual_probe_tour.native_dam_objective_evaluation_ready_count =
                stage_runtime_live
                    ? stage_ordinary_objects
                        .objective_evaluation_ready_count
                    : dam_objects.objective_evaluation_ready_count;
            visual_probe_tour.native_dam_objective_evaluation_blocked_count =
                stage_runtime_live
                    ? stage_ordinary_objects
                        .objective_evaluation_blocked_count
                    : dam_objects.objective_evaluation_blocked_count;
            visual_probe_tour.native_dam_objective_evaluation_ticks =
                stage_runtime_live
                    ? stage_ordinary_objects.objective_evaluation_ticks
                    : dam_objects.objective_evaluation_ticks;
            {
                GeOriginalDamObjectiveStatusSnapshot snapshot;
                (void)ge_original_dam_objective_status_last(&snapshot);
                visual_probe_tour.native_dam_objective_hud_message_count =
                    snapshot.message_count;
            }
            visual_probe_tour.native_dam_guard_overlay_updates =
                stage_runtime_live
                    ? stage_ordinary_objects.guard_overlay_updates
                    : dam_objects.guard_overlay_updates;
            visual_probe_tour.native_dam_door_overlay_updates =
                stage_runtime_live
                    ? stage_ordinary_objects.door_overlay_updates
                    : dam_objects.door_overlay_updates;
            visual_probe_tour.native_dam_overlay_full_rebuilds =
                stage_runtime_live
                    ? stage_ordinary_objects.overlay_full_rebuilds
                    : dam_objects.overlay_full_rebuilds;
            visual_probe_tour.native_dam_mission_tick_count =
                stage_runtime_live
                    ? stage_ordinary_objects.mission_runtime.observed_ticks
                    : dam_objects.mission_flow.ticks;
            if (stage_runtime_live) {
                uint16_t primary_offset = 0U;
                uint16_t exit_offset = 0U;
                (void)ge_original_stage_mission_runtime_actor_offset(
                    &stage_ordinary_objects.mission_runtime,
                    0x1000, &primary_offset);
                (void)ge_original_stage_mission_runtime_actor_offset(
                    &stage_ordinary_objects.mission_runtime,
                    0x1004, &exit_offset);
                visual_probe_tour.native_dam_mission_ai_offset =
                    primary_offset;
                visual_probe_tour.native_dam_mission_exit_ai_offset =
                    exit_offset;
            } else {
                visual_probe_tour.native_dam_mission_ai_offset =
                    dam_objects.mission_flow.ai_offset;
            }
            visual_probe_tour.native_dam_mission_objective_registers =
                stage_runtime_live
                    ? stage_ordinary_objects.mission_runtime
                        .objective_registers
                    : dam_objects.mission_flow.objective_registers;
            visual_probe_tour.native_dam_mission_hud_message_count =
                dam_objects.mission_flow.hud_message_count;
            visual_probe_tour.native_dam_full_props_activated =
                stage_runtime_live || dam_objects.full_props_activated
                    ? 1U : 0U;
        }

        if (visual_probe_tour.enabled) {
            const GeVisualProbeView *view;
            GeDamPreloadRoomState room_state;

            if (visual_probe_tour.current_view
                    >= visual_probe_tour.tour.count) {
                (void)write_visual_probe_tour_result(
                    &visual_probe_tour, &dam_preview);
                printf("Visual tour complete.\n");
                break;
            }
            if (dam_preview.dynamic_scene.room_count
                    > visual_probe_tour.peak_resident_rooms) {
                visual_probe_tour.peak_resident_rooms =
                    dam_preview.dynamic_scene.room_count;
            }
            if (dam_scene_textures.texture_count
                    > visual_probe_tour.peak_scene_textures) {
                visual_probe_tour.peak_scene_textures =
                    dam_scene_textures.texture_count;
            }
            view = &visual_probe_tour.tour.views[
                visual_probe_tour.current_view];
            room_state = ge_dam_preload_queue_room_state(
                &dam_preview.preload_queue, view->room);
            if (room_state == GE_DAM_PRELOAD_ROOM_UNLOADED) {
                /* Diagnostic camera jumps do not traverse intervening
                 * portals, so explicitly enter the same canonical room
                 * preload queue that gameplay portal visibility uses. */
                (void)ge_dam_preload_queue_request(
                    &dam_preview.preload_queue, view->room);
            }
            if (!visual_probe_tour.current_view_ready
                    || dam_preview.preload_queue.pending_count != 0U) {
                const bool camera_updated = update_original_dam_camera(
                    &dam_preview, view->position, view->look, view->up,
                    view->room,
                    (Vertex *)vertex_buffer + DAM_ROOM_VERTEX_OFFSET, true);
                bool ready_update = camera_updated;
                bool ordinary_refresh_attempted = false;
                bool ordinary_refresh_succeeded = false;
                if (camera_updated
                        && stage_ordinary_objects.actor_tick_status
                            == RUNTIME_STAGE_ACTOR_TICK_READY) {
                    ordinary_refresh_attempted = true;
                    ordinary_refresh_succeeded =
                        refresh_stage_ordinary_object_scenes(
                        &stage_ordinary_objects,
                        (Vertex *)vertex_buffer + DAM_ROOM_VERTEX_OFFSET);
                    ready_update = ordinary_refresh_succeeded;
                }
                ++visual_probe_tour.diagnostic_attempts;
                visual_probe_tour.diagnostic_camera_updated =
                    camera_updated ? 1U : 0U;
                visual_probe_tour.diagnostic_ordinary_refresh_attempted =
                    ordinary_refresh_attempted ? 1U : 0U;
                visual_probe_tour.diagnostic_ordinary_refresh_succeeded =
                    ordinary_refresh_succeeded ? 1U : 0U;
                visual_probe_tour.diagnostic_ordinary_scene_ready =
                    stage_ordinary_objects.scene_ready ? 1U : 0U;
                visual_probe_tour.diagnostic_ordinary_overlay_status =
                    (uint32_t)stage_ordinary_objects.overlay_status;
                visual_probe_tour
                    .diagnostic_ordinary_resident_install_successes =
                    stage_ordinary_objects.resident_install_successes;
                visual_probe_tour
                    .diagnostic_ordinary_resident_eviction_successes =
                    stage_ordinary_objects.resident_eviction_successes;
                visual_probe_tour.diagnostic_overlay_full_rebuilds =
                    stage_ordinary_objects.overlay_full_rebuilds;
                visual_probe_tour.diagnostic_door_overlay_failures =
                    stage_ordinary_objects.door_overlay_refresh_failures;
                visual_probe_tour.diagnostic_guard_overlay_failures =
                    stage_ordinary_objects.guard_overlay_refresh_failures;
                visual_probe_tour.diagnostic_monitor_overlay_failures =
                    stage_ordinary_objects.monitor_overlay_refresh_failures;
                visual_probe_tour.diagnostic_articulated_failures =
                    stage_ordinary_objects.articulated_scene_failure_count;
                visual_probe_record_camera(
                    &visual_probe_tour, &dam_preview, ready_update);
                rendered_player_generation = UINT64_MAX;
                visual_probe_tour.current_view_ready = ready_update
                    && ge_dam_preload_queue_room_state(
                        &dam_preview.preload_queue, view->room)
                        == GE_DAM_PRELOAD_ROOM_RESIDENT
                    && dam_preview.preload_queue.pending_count == 0U;
                (void)write_visual_probe_tour_diagnostic(
                    &visual_probe_tour, &dam_preview);
            }
            /* Hold time starts only after this exact authored room has been
             * installed and successfully projected. This makes captures
             * deterministic even when emulator streaming is much slower
             * than hardware. */
            if (visual_probe_tour.current_view_ready
                    && ++visual_probe_tour.view_elapsed_frames
                        >= view->hold_frames) {
                ++visual_probe_tour.current_view;
                visual_probe_tour.view_elapsed_frames = 0U;
                visual_probe_tour.current_view_camera_failed = false;
                visual_probe_tour.current_view_visibility_failed = false;
                visual_probe_tour.current_view_ready = false;
                if (visual_probe_tour.current_view
                        < visual_probe_tour.tour.count) {
                    view = &visual_probe_tour.tour.views[
                        visual_probe_tour.current_view];
                    printf("View %lu/%lu: %s\n",
                        (unsigned long)visual_probe_tour.current_view + 1UL,
                        (unsigned long)visual_probe_tour.tour.count,
                        view->label);
                }
            }
        }

        if (dam_intro.player.initialized
                && dam_intro.player.publication_generation
                    != rendered_player_generation) {
            const u64 camera_start = osGetTime();
            const bool camera_updated = upload_dam_gpu_world_scene(
                &dam_preview,
                (Vertex *)vertex_buffer + DAM_ROOM_VERTEX_OFFSET);
            frame_profile.camera_ms += osGetTime() - camera_start;
            if (camera_updated) {
                rendered_player_generation =
                    dam_intro.player.publication_generation;
                if (dam_preview.gpu_dirty_vertex_count != 0U) {
                    const uint64_t flush_start = svcGetSystemTick();
                    const size_t flushed_vertices =
                        dam_preview.gpu_dirty_vertex_count;
                    GSPGPU_FlushDataCache(
                        (Vertex *)vertex_buffer + DAM_ROOM_VERTEX_OFFSET
                            + dam_preview.gpu_dirty_vertex_offset,
                        renderer_vertex_flush_bytes(
                            dam_preview.gpu_dirty_vertex_count,
                            DAM_ROOM_VERTEX_COUNT));
                    fine_profile.world_gpu_flush_ticks +=
                        svcGetSystemTick() - flush_start;
                    fine_profile.world_gpu_flush_calls++;
                    fine_profile.world_gpu_flush_vertices +=
                        flushed_vertices;
                    dam_preview.gpu_dirty_vertex_count = 0U;
                }
            }
        }

        if (texture_cache_ready) {
            const u64 first_person_start = osGetTime();
            const bool first_person_updated = update_first_person_scene(
                    &first_person_models, &first_person_scene,
                    &dam_preview, &texture_cache,
                    (Vertex *)vertex_buffer + FIRST_PERSON_VERTEX_OFFSET);
            frame_profile.first_person_ms +=
                osGetTime() - first_person_start;
            if (first_person_updated) {
            GSPGPU_FlushDataCache(
                (Vertex *)vertex_buffer + FIRST_PERSON_VERTEX_OFFSET,
                first_person_scene.render_vertex_count * sizeof(Vertex));
            }
        }

        {
            const u64 gpu_start = osGetTime();
            uint64_t fine_start;
            GeOriginalDamMissionExitSnapshot fade_snapshot = {0};
            ge_original_dam_mission_exit_services_snapshot(&fade_snapshot);
            C3D_RenderTargetClear(top_target, C3D_CLEAR_ALL, CLEAR_COLOR, 0);
            C3D_FrameDrawOn(top_target);
            fine_start = svcGetSystemTick();
            renderer_draw(&rareware_mesh, &blotter_preview,
                          &dam_preview, &stage_ordinary_objects,
                          &first_person_scene, &fade_snapshot);
            fine_profile.renderer_draw_ticks +=
                svcGetSystemTick() - fine_start;
            fine_start = svcGetSystemTick();
            C3D_FrameEnd(0);
            fine_profile.frame_end_ticks +=
                svcGetSystemTick() - fine_start;
            fine_profile.rendered_frames++;
            gpu_elapsed_milliseconds = osGetTime() - gpu_start;
            frame_profile.gpu_ms += gpu_elapsed_milliseconds;
        }
        if (stage_transition_pending) {
            /* bossMainloop waits for the final graphics work before consuming
             * g_MainStageNum. Present the completed canonical fade once, then
             * commit at the same outer-stage boundary. */
            if (input_probe.enabled) {
                /* The successful ai_24 input edge requests the title stage
                 * before the ordinary displayed-frame completion check. Flush
                 * the final canonical mission/save snapshot at that boundary
                 * so the controller-only route can prove the transition. */
                input_probe_capture_player(&input_probe, &dam_intro);
                input_probe_capture_gates(
                    &input_probe, &stage_ordinary_objects, false);
                (void)write_input_probe_result(
                    &input_probe, &stage_ordinary_objects,
                    &first_person_models, &first_person_scene.cache, &dam_intro);
            }
            (void)ge_original_boss_commit_requested_stage();
            gameplay_stage_ended = true;
            break;
        }
        {
            const u64 displayed_frame_ms = osGetTime() - current_time;
            frame_profile.frame_ms += displayed_frame_ms;
            if (visual_probe_tour.enabled) {
                ++visual_probe_tour.displayed_frame_count;
                visual_probe_tour.displayed_frame_total_ms +=
                    displayed_frame_ms;
                if (displayed_frame_ms
                        > visual_probe_tour.displayed_frame_peak_ms)
                    visual_probe_tour.displayed_frame_peak_ms =
                        displayed_frame_ms;
                visual_probe_tour.simulation_total_ms +=
                    simulation_elapsed_milliseconds;
                visual_probe_tour.gpu_total_ms += gpu_elapsed_milliseconds;
            }
            if (input_probe.enabled) {
                ++input_probe.displayed_frames;
                input_probe.displayed_total_ms += displayed_frame_ms;
                if (displayed_frame_ms > input_probe.displayed_peak_ms)
                    input_probe.displayed_peak_ms = displayed_frame_ms;
                if (input_probe.displayed_frames > 120U) {
                    input_probe.displayed_samples_after_warmup++;
                    if (displayed_frame_ms
                            > input_probe.displayed_peak_after_warmup_ms)
                        input_probe.displayed_peak_after_warmup_ms =
                            displayed_frame_ms;
                    if (displayed_frame_ms > 16U)
                        input_probe.displayed_over_16_ms++;
                    if (displayed_frame_ms > 25U)
                        input_probe.displayed_over_25_ms++;
                    if (displayed_frame_ms > 33U)
                        input_probe.displayed_over_33_ms++;
                    if (displayed_frame_ms > 50U)
                        input_probe.displayed_over_50_ms++;
                }
                if (input_probe.displayed_frames > 120U) {
                    RuntimeInputProbeSlowFrame sample = {
                        input_probe.displayed_frames,
                        dam_intro.player.room,
                        displayed_frame_ms,
                        frame_profile.simulation_ms
                            - frame_profile_before.simulation_ms,
                        frame_profile.overlay_ms
                            - frame_profile_before.overlay_ms,
                        frame_profile.camera_ms
                            - frame_profile_before.camera_ms,
                        frame_profile.first_person_ms
                            - frame_profile_before.first_person_ms,
                        gpu_elapsed_milliseconds,
                        stage_ordinary_objects.guard_scene_cache
                            .topology_rebuilds
                            - frame_topology_rebuilds_before,
                        stage_ordinary_objects.guard_scene_cache
                            .topology_component_misses
                            - frame_topology_component_misses_before,
                        dam_preview.dynamic_scene.generation
                            - frame_scene_generation_before,
                        stage_ordinary_objects.overlay_full_rebuilds
                            - frame_overlay_full_rebuilds_before,
                    };
                    size_t slow_slot;
                    if (input_probe.slow_frame_count
                            < sizeof(input_probe.slow_frames)
                                / sizeof(input_probe.slow_frames[0])) {
                        slow_slot = input_probe.slow_frame_count++;
                    } else {
                        size_t slow_index;
                        slow_slot = 0U;
                        for (slow_index = 1U;
                                slow_index < input_probe.slow_frame_count;
                                ++slow_index)
                            if (input_probe.slow_frames[slow_index].total_ms
                                    < input_probe.slow_frames[slow_slot]
                                        .total_ms)
                                slow_slot = slow_index;
                        if (sample.total_ms
                                <= input_probe.slow_frames[slow_slot]
                                    .total_ms)
                            slow_slot = SIZE_MAX;
                    }
                    if (slow_slot != SIZE_MAX)
                        input_probe.slow_frames[slow_slot] = sample;
                }
                input_probe_capture_player(&input_probe, &dam_intro);
                input_probe_capture_gates(
                    &input_probe, &stage_ordinary_objects, false);
            }
        }
        frame_profile.samples++;

        if (input_probe.enabled
                && input_probe_route_frame(&input_probe)
                    >= input_probe.target_frames) {
            (void)write_input_probe_result(
                &input_probe, &stage_ordinary_objects,
                &first_person_models, &first_person_scene.cache, &dam_intro);
            printf("Input probe complete.\n");
            break;
        }

        /* Formatting several kilobytes of adapter telemetry on the emulated
         * 3DS CPU produces visible multi-frame stalls.  Automated probes
         * persist the same evidence outside the live frame, so interactive
         * builds keep the console static unless diagnostics are explicitly
         * enabled at compile time. */
        if (GE_3DS_LIVE_DIAGNOSTICS
                && !input_probe.enabled && !visual_probe_tour.enabled
                && (frame_counter++ % 120u) == 0u) {
            GeTextureCacheStats texture_cache_stats;

            ge_texture_cache_get_stats(texture_cache_ready ? &texture_cache : NULL,
                                       &texture_cache_stats);
            print_status(&port, stage_assets, is_new_3ds, cstick_available,
                         assets_mounted,
                         assets_mounted ? asset_pack.entry_count : 0,
                         texture_catalog_mounted,
                         texture_catalog_mounted ? texture_catalog.entry_count : 0,
                         &texture_cache_stats, &rareware_mesh,
                         &rareware_body_model,
                         &blotter_preview,
                         &dam_preview,
                         &dam_collision,
                         &dam_intro,
                         &dam_objects,
                         &bond_animations,
                         &first_person_models,
                         &first_person_scene,
                         scheduler_ticks, ticks, audio_active,
                         clip_stage_ready, audio_abi_ready);
        }
    }

    if (gameplay_stage_ended
            && !input_probe.enabled && !visual_probe_tour.enabled) {
        GeOriginalMissionResultSnapshot completed_mission = {0};
        ge_original_mission_result_snapshot(&completed_mission);
        (void)ge_3ds_audio_bind_secondary(NULL);
        ge_original_music_runtime_close(original_music);
        original_music = NULL;
        next_stage_requested = run_original_mission_complete_report(
            top_target, cstick_available,
            texture_cache_ready ? &texture_cache : NULL,
            &original_frontend_runtime, &original_frontend,
            &stage_ordinary_objects,
            &completed_mission, &original_music, &audio_output,
            audio_active,
            &selected_level_id);
    }

cleanup_runtime:
    if (scheduler_active) {
        (void)ge_retrace_scheduler_stop(&scheduler);
    }
    ge_original_gameplay_services_bind_audio(NULL, NULL);
    ge_original_gameplay_services_bind_settings_persistence(NULL, NULL);
    (void)ge_3ds_audio_bind_secondary(NULL);
    ge_original_music_runtime_close(original_music);
    original_music = NULL;
    if (audio_active)
        ge_3ds_audio_exit();
    if (audio_output_ready)
        ge_libultra_audio_bind(NULL);
    if (dam_preview.dynamic_scene.initialized != 0U) {
        ge_dam_dynamic_scene_close(&dam_preview.dynamic_scene);
    } else {
        free(dam_preview.source_vertices);
        free(dam_preview.batches);
    }
    free(dam_preview.render_batches);
    free(dam_preview.gpu_batch_bounds);
    ge_dam_visibility_runtime_close(&dam_preview.visibility_runtime);
    close_dam_world_objects(&dam_objects);
    close_stage_ordinary_objects(&stage_ordinary_objects);
    close_first_person_scene(&first_person_scene);
    close_first_person_models(&first_person_models);
    close_bond_animations(&bond_animations);
    close_original_sfx_bank(&original_sfx);
    close_original_frontend_model(&original_frontend_runtime);
    ge_3ds_save_provider_close(&mission_save_provider);
    ge_original_stage_setup_close(&packaged_stage_setup);
    close_dam_collision(&dam_collision);
    renderer_exit();

    if (next_stage_requested) {
        const GeStageAssetDescriptor *next_stage =
            ge_stage_asset_descriptor_by_level_id(selected_level_id);
        if (next_stage == NULL) {
            printf("Unsupported requested LEVELID %ld.\n",
                   (long)selected_level_id);
            result = 1;
            goto exit_citro3d;
        }
        stage_assets = next_stage;
        dam_stage = stage_assets->stage == GE_STAGE_DAM;
        stage_setup_loaded = false;
        scheduler_active = false;
        audio_output_ready = false;
        audio_active = false;
        clip_stage_ready = false;
        audio_abi_ready = false;
        stage_transition_pending = false;
        gameplay_stage_ended = false;
        next_stage_requested = false;
        previous_time = 0U;
        scheduler_ticks = 0U;
        rendered_player_generation = 0U;
        frame_counter = 0U;
        original_stage_setup = NULL;
        memset(&rareware_mesh, 0, sizeof(rareware_mesh));
        memset(&rareware_front_model, 0, sizeof(rareware_front_model));
        memset(&rareware_body_model, 0, sizeof(rareware_body_model));
        memset(&blotter_preview, 0, sizeof(blotter_preview));
        memset(&dam_preview, 0, sizeof(dam_preview));
        memset(&dam_collision, 0, sizeof(dam_collision));
        memset(&dam_intro, 0, sizeof(dam_intro));
        memset(&packaged_stage_setup, 0, sizeof(packaged_stage_setup));
        memset(&stage_spawn, 0, sizeof(stage_spawn));
        memset(&original_sfx, 0, sizeof(original_sfx));
        memset(&dam_objects, 0, sizeof(dam_objects));
        memset(&stage_ordinary_objects, 0, sizeof(stage_ordinary_objects));
        memset(&bond_animations, 0, sizeof(bond_animations));
        memset(&first_person_models, 0, sizeof(first_person_models));
        memset(&first_person_scene, 0, sizeof(first_person_scene));
        memset(&input_probe, 0, sizeof(input_probe));
        memset(&visual_probe_tour, 0, sizeof(visual_probe_tour));
        memset(&frame_profile, 0, sizeof(frame_profile));
        memset(&frame_profile_total, 0, sizeof(frame_profile_total));
        memset(&fine_profile, 0, sizeof(fine_profile));
        printf("\nLoading next stage: %s (LEVELID %ld)\n",
               stage_assets->key, (long)selected_level_id);
        goto start_stage_runtime;
    }

exit_citro3d:
    close_original_frontend_ramrom(&original_frontend_runtime);
    if (texture_cache_ready) {
        ge_texture_cache_close(&texture_cache);
    }
    if (texture_catalog_mounted) {
        ge_texture_catalog_close(&texture_catalog);
        free(texture_catalog_data);
    }
    if (assets_mounted) {
        ge_asset_pack_close(&asset_pack);
    }
    C3D_Fini();
exit_graphics:
    if (cstick_available) {
        irrstExit();
    }
    gfxExit();
    return result;
}
