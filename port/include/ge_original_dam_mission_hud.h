#ifndef GE_ORIGINAL_DAM_MISSION_HUD_H
#define GE_ORIGINAL_DAM_MISSION_HUD_H

#include <stddef.h>
#include <stdint.h>

#define GE_ORIGINAL_DAM_MISSION_HUD_CAPACITY 2U
#define GE_ORIGINAL_DAM_MISSION_HUD_MESSAGE_CAPACITY 0x97U

typedef struct GeOriginalDamMissionHudRenderSnapshot {
    size_t count;
    int32_t timer;
    uint8_t visible;
    char messages[GE_ORIGINAL_DAM_MISSION_HUD_CAPACITY]
                 [GE_ORIGINAL_DAM_MISSION_HUD_MESSAGE_CAPACITY];
} GeOriginalDamMissionHudRenderSnapshot;

#define GE_ORIGINAL_GAMEPLAY_HUD_GAUGE_VERTEX_COUNT 46U
#define GE_ORIGINAL_BOTTOM_HUD_CAPACITY 5U
#define GE_ORIGINAL_BOTTOM_HUD_MESSAGE_CAPACITY 101U
#define GE_ORIGINAL_AMMO_ICON_ASSET_COUNT 13U
#define GE_ORIGINAL_AMMO_ICON_9MM_SEGMENTED_ADDRESS UINT32_C(0x02000C84)

typedef struct GeOriginalAmmoIconAsset {
    int32_t ammo_type;
    uint32_t segmented_address;
    uint16_t image_id;
    uint8_t width;
    uint8_t height;
    float y_offset;
    const char *source;
} GeOriginalAmmoIconAsset;

typedef struct GeOriginalBottomHudRenderSnapshot {
    size_t count;
    int32_t timer;
    int16_t x;
    int16_t y;
    uint8_t visible;
    char message[GE_ORIGINAL_BOTTOM_HUD_MESSAGE_CAPACITY];
} GeOriginalBottomHudRenderSnapshot;

typedef struct GeOriginalGameplayHudGaugeVertex {
    int16_t x;
    int16_t z;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
} GeOriginalGameplayHudGaugeVertex;

typedef struct GeOriginalGameplayHudRenderSnapshot {
    GeOriginalGameplayHudGaugeVertex armour
        [GE_ORIGINAL_GAMEPLAY_HUD_GAUGE_VERTEX_COUNT];
    GeOriginalGameplayHudGaugeVertex health
        [GE_ORIGINAL_GAMEPLAY_HUD_GAUGE_VERTEX_COUNT];
    int32_t magazine_ammo;
    int32_t reserve_ammo;
    int32_t left_magazine_ammo;
    int32_t left_reserve_ammo;
    int16_t magazine_x;
    int16_t reserve_x;
    int16_t left_magazine_x;
    int16_t left_reserve_x;
    int16_t ammo_y;
    int16_t icon_x;
    int16_t icon_y;
    int16_t left_icon_x;
    int16_t left_icon_y;
    uint32_t icon_image;
    uint32_t left_icon_image;
    uint32_t ammo_suppression_reasons;
    int32_t ammo_type;
    int32_t left_ammo_type;
    uint8_t gauges_visible;
    uint8_t ammo_visible;
    uint8_t reserve_visible;
    uint8_t left_ammo_visible;
    uint8_t left_reserve_visible;
} GeOriginalGameplayHudRenderSnapshot;

void ge_original_dam_mission_hud_reset(void);
/* Exact bondviewUpperTextWindowTimerTick presentation cadence. It is called
 * once from the displayed-frame HUD pass, where the original renderer called
 * it immediately before drawing the upper message window. */
void ge_original_dam_mission_hud_tick(void);
void ge_original_bottom_hud_reset(void);
void ge_original_bottom_hud_tick(void);
void ge_original_hud_bottom_show_exact(char *message);
size_t ge_original_dam_mission_hud_count(void);
const char *ge_original_dam_mission_hud_message(size_t index);

/* Read-only platform boundary for drawing the exact canonical upper-HUD
 * queue. The snapshot owns its strings, so a renderer cannot mutate or hold
 * stale pointers into the game queue. */
int ge_original_dam_mission_hud_render_snapshot(
    GeOriginalDamMissionHudRenderSnapshot *snapshot);

/* Read-only presentation boundary over canonical player/hand/ammo state. */
int ge_original_gameplay_hud_render_snapshot(
    GeOriginalGameplayHudRenderSnapshot *snapshot);
int ge_original_bottom_hud_render_snapshot(
    GeOriginalBottomHudRenderSnapshot *snapshot);

/* Exact join of ammo_related's segment-2 image addresses to the ROM-authored
 * oddtextures image records already present in the converted texture catalog. */
size_t ge_original_ammo_icon_asset_count(void);
const GeOriginalAmmoIconAsset *ge_original_ammo_icon_asset_at(size_t index);
const GeOriginalAmmoIconAsset *ge_original_ammo_icon_asset_for_ammo_type(
    int32_t ammo_type);
const GeOriginalAmmoIconAsset *
ge_original_ammo_icon_asset_for_segmented_address(uint32_t address);

#endif
