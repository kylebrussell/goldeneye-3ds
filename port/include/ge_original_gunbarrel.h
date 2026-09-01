#ifndef GE_ORIGINAL_GUNBARREL_H
#define GE_ORIGINAL_GUNBARREL_H

#include <stdint.h>

#define GE_ORIGINAL_GUNBARREL_HOLE_VERTEX_COUNT 30U
#define GE_ORIGINAL_GUNBARREL_MAX_LAYER_HOLE_VERTICES \
    (GE_ORIGINAL_GUNBARREL_HOLE_VERTEX_COUNT * 2U)

typedef enum GeOriginalGunbarrelLayer {
    GE_ORIGINAL_GUNBARREL_LAYER_CLEAR_BLACK = 1U << 0,
    GE_ORIGINAL_GUNBARREL_LAYER_MOVING_HOLE = 1U << 1,
    GE_ORIGINAL_GUNBARREL_LAYER_SNIPER_SIGHT = 1U << 2,
    GE_ORIGINAL_GUNBARREL_LAYER_SIGHT_BACKDROP = 1U << 3,
    GE_ORIGINAL_GUNBARREL_LAYER_BOND = 1U << 4,
    GE_ORIGINAL_GUNBARREL_LAYER_BLOOD_IMAGE = 1U << 5,
    GE_ORIGINAL_GUNBARREL_LAYER_BLOOD_COLOUR = 1U << 6,
    GE_ORIGINAL_GUNBARREL_LAYER_FADE_BLACK = 1U << 7
} GeOriginalGunbarrelLayer;

typedef enum GeOriginalGunbarrelTickResult {
    GE_ORIGINAL_GUNBARREL_TICK_INVALID = 0,
    GE_ORIGINAL_GUNBARREL_TICK_RUNNING,
    GE_ORIGINAL_GUNBARREL_TICK_COMPLETE,
    GE_ORIGINAL_GUNBARREL_TICK_NEEDS_BLOOD_DECODER
} GeOriginalGunbarrelTickResult;

/* Resource inputs copied from initializeGunBarrelIntro. The model provider is
 * responsible for resolving these authored IDs; this module never installs a
 * substitute silhouette or weapon. */
typedef struct GeOriginalGunbarrelAssets {
    int32_t body_model;
    int32_t head_model;
    int32_t gun_model;
    float model_scale;
    float animation_play_speed;
    float animation_translation_scale;
    int32_t walk_animation_frame_backstep;
    float camera_position[3];
    float camera_direction[3];
    float camera_up[3];
    float field_of_view_degrees;
    float perspective_aspect;
    float perspective_near;
    float perspective_far;
    uint16_t logical_width;
    uint16_t logical_height;
    uint16_t native_width;
    uint16_t native_height;
    uint16_t sight_width;
    uint16_t sight_height;
    uint16_t sight_y;
    float backdrop_offset_x;
    float backdrop_offset_y;
    float backdrop_scale_x;
    float backdrop_scale_y;
    uint16_t blood_width;
    uint16_t blood_height;
    uint8_t blood_red;
    uint8_t blood_green;
    uint8_t blood_blue;
    uint8_t blood_alpha;
} GeOriginalGunbarrelAssets;

/* Exact 16-byte layout filled by unchanged createGunbarrelRenderHole. */
typedef struct GeOriginalGunbarrelHoleVertex {
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t flag;
    int16_t s;
    int16_t t;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
} GeOriginalGunbarrelHoleVertex;

/* Canonical title-space output of manipulateGunbarrelAndLogoMatrices and
 * insert_sight_backdrop_eye_intro.  Coordinates remain in initialize's
 * 1280x960 ortho space so a platform can map them to its 4:3 viewport once. */
typedef struct GeOriginalGunbarrelLayerHoleVertex {
    float x;
    float y;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
} GeOriginalGunbarrelLayerHoleVertex;

/* Exact visible rectangle/source window emitted by
 * titleRenderFolderMenuBackgroundLines in the native 440x330 title mode.
 * Bounds are half-open. Source X may extend beyond the 440-pixel row because
 * the original texture tile clamps at its edge. */
typedef struct GeOriginalGunbarrelSightRect {
    int16_t destination_left;
    int16_t destination_top;
    int16_t destination_right;
    int16_t destination_bottom;
    int16_t source_left;
    int16_t source_top;
    int16_t source_right;
    int16_t source_bottom;
} GeOriginalGunbarrelSightRect;

typedef struct GeOriginalGunbarrelState {
    uint8_t mode;
    uint8_t complete;
    uint16_t sway_angle;
    int32_t intro_counter;
    int32_t animation_tick;
    int16_t transition_delay;
    float title_x;
    float title_y;
    float transition_x;
    float transition_y;
    uint32_t rendered_frames;
} GeOriginalGunbarrelState;

typedef struct GeOriginalGunbarrelFrame {
    uint8_t mode;
    uint8_t mode_after;
    uint8_t animation_start;
    uint8_t animation_speedup;
    uint8_t fire_shot;
    uint8_t blood_reset;
    uint8_t blood_advance;
    uint8_t sequence_complete;
    uint8_t fade_alpha;
    uint8_t bond_animation_ticks;
    uint32_t layers;
    float title_x;
    float title_y;
    float transition_x;
    float transition_y;
} GeOriginalGunbarrelFrame;

/* mode is the unchanged die_blood_image_routine argument: zero resets and
 * decodes the first authored frame; one advances the compressed stream. */
typedef int (*GeOriginalGunbarrelBloodTick)(void *context, int mode);

const char *ge_original_gunbarrel_contract_sha256(void);
void ge_original_gunbarrel_assets(GeOriginalGunbarrelAssets *assets);
uint32_t ge_original_gunbarrel_build_hole(
    GeOriginalGunbarrelHoleVertex *vertices, uint32_t capacity);
uint32_t ge_original_gunbarrel_build_frame_holes(
    const GeOriginalGunbarrelFrame *frame,
    GeOriginalGunbarrelLayerHoleVertex *vertices, uint32_t capacity);
int ge_original_gunbarrel_sight_rect(
    const GeOriginalGunbarrelFrame *frame,
    GeOriginalGunbarrelSightRect *rect);
void ge_original_gunbarrel_reset(GeOriginalGunbarrelState *state);
GeOriginalGunbarrelTickResult ge_original_gunbarrel_tick(
    GeOriginalGunbarrelState *state,
    GeOriginalGunbarrelBloodTick blood_tick,
    void *blood_context,
    GeOriginalGunbarrelFrame *frame);

#endif
