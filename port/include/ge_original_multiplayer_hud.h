#ifndef GE_ORIGINAL_MULTIPLAYER_HUD_H
#define GE_ORIGINAL_MULTIPLAYER_HUD_H

#include "ge_original_dam_mission_hud.h"
#include "ge_original_gun_sight.h"

#define GE_ORIGINAL_MULTIPLAYER_HUD_PLAYERS 4U

typedef struct GeOriginalMultiplayerHudFade {
    int32_t red, green, blue;
    float fraction, time, time_max;
} GeOriginalMultiplayerHudFade;

/* No borrowed game/model/queue pointers. The renderer may build text and
 * icon vertices from this value after any number of later shooter passes. */
typedef struct GeOriginalMultiplayerHudSnapshot {
    GeOriginalBottomHudRenderSnapshot bottom;
    GeOriginalDamMissionHudRenderSnapshot upper;
    GeOriginalGameplayHudRenderSnapshot gameplay;
    GeOriginalGunSightSnapshot sight;
    GePicaTextureRectangleDraw sight_draw;
    GeOriginalMultiplayerHudFade fade;
    int32_t watch_animation_state;
    int32_t menu_on;
    uint8_t watch_open;
    uint8_t watch_objectives_visible;
    uint8_t sight_visible;
} GeOriginalMultiplayerHudSnapshot;

typedef struct GeOriginalMultiplayerHud {
    GeOriginalMultiplayerHudSnapshot players[GE_ORIGINAL_MULTIPLAYER_HUD_PLAYERS];
    uint64_t tick;
    uint32_t player_count;
    uint8_t initialized;
    uint8_t prepared;
    uint8_t captured;
    uint8_t failed;
    uint8_t intro_camera;
} GeOriginalMultiplayerHud;

/* Zero-initialize state once. Tick is a monotonically increasing simulation
 * identifier, independent of eye/display count. No active unfinished frame
 * may be replaced. These functions never select a different player. */
int ge_original_multiplayer_hud_begin_frame(GeOriginalMultiplayerHud *state,
    uint64_t tick, uint32_t player_count);
/* Selected canonical shooter: after gun/first-person, before death/respawn.
 * Copies gauge geometry and watch flags, then performs the original gauge
 * healthdisplaytime decrement. Intro camera skips the original gauge path. */
int ge_original_multiplayer_hud_prepare_shooter(GeOriginalMultiplayerHud *state,
    uint32_t slot);
/* Same shooter, after death/respawn: lower tick/render selection/snapshot,
 * upper tick/snapshot, sight/ammo/fade. Shared upper queue advances once for
 * EVERY canonical shooter, including watch-open players. */
int ge_original_multiplayer_hud_capture_shooter(GeOriginalMultiplayerHud *state,
    uint32_t slot);
/* Read-only, deep value copy. A stale tick or unfinished slot fails. A final
 * draw must use this function, and must not call the HUD tick producers. */
int ge_original_multiplayer_hud_get(const GeOriginalMultiplayerHud *state,
    uint64_t tick, uint32_t slot, GeOriginalMultiplayerHudSnapshot *snapshot);

/* Source-extracted native boundaries; called by the adapter in exact order. */
void ge_original_gameplay_hud_prepare_exact(void);
void ge_original_bottom_hud_prepare_exact(void);
void ge_original_multiplayer_hud_fade_snapshot_exact(
    GeOriginalMultiplayerHudFade *snapshot);

#endif
