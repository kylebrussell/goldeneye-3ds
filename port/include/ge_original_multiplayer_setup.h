#ifndef GE_ORIGINAL_MULTIPLAYER_SETUP_H
#define GE_ORIGINAL_MULTIPLAYER_SETUP_H
#include <stdint.h>
#include "ge_original_multiplayer_frontend.h"

typedef enum GeOriginalMultiplayerSetupStatus {
    GE_ORIGINAL_MP_SETUP_OK,
    GE_ORIGINAL_MP_SETUP_INVALID,
    GE_ORIGINAL_MP_SETUP_UNBOUND,
    GE_ORIGINAL_MP_SETUP_RESOURCE_FAILED,
    GE_ORIGINAL_MP_SETUP_LAUNCH_PENDING,
    GE_ORIGINAL_MP_SETUP_RETURN_TO_MODE,
    GE_ORIGINAL_MP_SETUP_STALE
} GeOriginalMultiplayerSetupStatus;
typedef enum GeOriginalMultiplayerSetupAudio {
    GE_ORIGINAL_MP_SETUP_SFX_VOLUME,
    GE_ORIGINAL_MP_SETUP_MUSIC_VOLUME,
    GE_ORIGINAL_MP_SETUP_MUSIC_PLAY,
    GE_ORIGINAL_MP_SETUP_SFX_PLAY
} GeOriginalMultiplayerSetupAudio;
/* Explicit resource boundaries for unchanged native setup interface bodies.
 * Wallet pointers must be real instances from the provider, borrowed until end.
 * Missing/failed resources stop publication; successful no-op substitutes are
 * not supplied by this adapter. stage_status returns native STAGESTATUS_*. */
typedef struct GeOriginalMultiplayerSetupServices {
    void *context;
    int (*wallet_load)(void *,void **model);
    int (*wallet_clear_switches)(void *,const void *model);
    int (*wallet_set_switch)(void *,const void *model,int32_t index,int32_t value);
    int (*texture_load)(void *,uint32_t texture);
    int (*stage_status)(void *,int32_t folder,int32_t mission,int32_t difficulty);
    int (*audio)(void *,GeOriginalMultiplayerSetupAudio,int32_t value);
} GeOriginalMultiplayerSetupServices;
typedef struct GeOriginalMultiplayerSetupSnapshot {
    int32_t menu,next_menu,reload_menu,players,stage_choice,scenario,weapon_set;
    int32_t length,aim_setting,character_count,stage_highlight,scenario_highlight,team_choice;
    int32_t characters[4],handicaps[4],control_styles[4],teams[4];
    int32_t ready[4],current_character[4],previous_character[4],scroll[4],size[4],stick_centered[4];
    int32_t option_highlights[9],enabled[7],tab_selected[3],tab_highlights[3];
    float cursor[2],fov_y,aspect,znear,zfar;
    uint32_t team_blink;
    uint8_t use_zbuffer,launch_taken;
} GeOriginalMultiplayerSetupSnapshot;
typedef struct GeOriginalMultiplayerSetupLaunch {
    GeOriginalMultiplayerFrontendStart start;
    int32_t handicap_indices[4],control_styles[4],aim_setting;
    /* All four physical native rows, including inactive settings. */
    int32_t characters[4];
    uint8_t teams[4],sight,autoaim;
} GeOriginalMultiplayerSetupLaunch;
/* Enter from the existing mode-selection owner. The provider supplies the
 * source cursor/folder and actual virtual seat count (2..4), not joy's extra
 * secondary gameplay pad count. Native options persist across begin/end. */
GeOriginalMultiplayerSetupStatus ge_original_multiplayer_setup_begin(
    const GeOriginalMultiplayerSetupServices *,int32_t folder,
    float cursor_x,float cursor_y,uint32_t connected_seats,int returning_from_stage);
/* Caller publishes one whole raw-pad frame first. Exact native cursor/input
 * bodies run once with source 440x330 menu view and this delta. */
GeOriginalMultiplayerSetupStatus ge_original_multiplayer_setup_tick(float delta);
GeOriginalMultiplayerSetupStatus ge_original_multiplayer_setup_snapshot(
    GeOriginalMultiplayerSetupSnapshot *);
/* Stops at actual frontChangeMenu(MENU_RUN_STAGE,1). The caller owns the
 * framebuffer-dependent screen switch/loading boundary; no fake VI cadence.
 * Copied launch is one-shot, includes random-arena selection's native result. */
GeOriginalMultiplayerSetupStatus ge_original_multiplayer_setup_take_launch(
    uint64_t request_id,GeOriginalMultiplayerSetupLaunch *);
/* After players_create/configure, before intro/spawn: publish actual lv.c
 * handicap/height/aim/sight, teams, character and control owners. No resets,
 * inventory loads, RNG consumption or player-selection changes. */
GeOriginalMultiplayerSetupStatus ge_original_multiplayer_setup_apply_launch(
    const GeOriginalMultiplayerSetupLaunch *);
void ge_original_multiplayer_setup_end(void);
#endif
