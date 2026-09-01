#ifndef GE_GBI_STATE_H
#define GE_GBI_STATE_H

#include <stdint.h>

#include "ge_gbi_decoder.h"
#include "ge_gbi_matrix.h"
#include "ge_gbi_rsp.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum GeGbiStateStatus {
    GE_GBI_STATE_OK = 0,
    GE_GBI_STATE_INVALID_ARGUMENT,
    GE_GBI_STATE_MISSING_VERTEX,
    GE_GBI_STATE_MATRIX_STACK_OVERFLOW,
    GE_GBI_STATE_MATRIX_STACK_UNDERFLOW,
    GE_GBI_STATE_INVALID_MATRIX_PARAMETERS,
    GE_GBI_STATE_INVALID_MOVE_WORD,
    GE_GBI_STATE_MALFORMED_SEQUENCE,
    GE_GBI_STATE_MISSING_RSP_PAYLOAD,
    GE_GBI_STATE_UNSUPPORTED
} GeGbiStateStatus;

typedef enum GeGbiStateActionKind {
    GE_GBI_STATE_ACTION_NONE = 0,
    GE_GBI_STATE_ACTION_LOAD_MATRIX,
    GE_GBI_STATE_ACTION_LOAD_VIEWPORT,
    GE_GBI_STATE_ACTION_LOAD_LIGHT,
    GE_GBI_STATE_ACTION_LOAD_LOOK_AT,
    GE_GBI_STATE_ACTION_LOAD_VERTICES,
    GE_GBI_STATE_ACTION_DRAW_TRIANGLES,
    GE_GBI_STATE_ACTION_DRAW_FILL_RECTANGLE,
    GE_GBI_STATE_ACTION_DRAW_TEXTURE_RECTANGLE,
    GE_GBI_STATE_ACTION_CALL_DISPLAY_LIST,
    GE_GBI_STATE_ACTION_BRANCH_DISPLAY_LIST,
    GE_GBI_STATE_ACTION_END_DISPLAY_LIST
} GeGbiStateActionKind;

typedef enum GeGbiTextureBindingKind {
    GE_GBI_TEXTURE_BINDING_NONE = 0,
    GE_GBI_TEXTURE_BINDING_RARE_ID,
    GE_GBI_TEXTURE_BINDING_IMAGE
} GeGbiTextureBindingKind;

typedef struct GeGbiTextureState {
    uint16_t scale_s;
    uint16_t scale_t;
    uint8_t level;
    uint8_t tile;
    uint8_t enabled;
} GeGbiTextureState;

typedef struct GeGbiRareTextureState {
    uint16_t texture_id;
    uint16_t detail_texture_id;
    uint8_t min_level;
    uint8_t type;
    uint8_t tile;
    uint8_t clamp_mirror_s;
    uint8_t clamp_mirror_t;
    uint8_t shift_s;
    uint8_t shift_t;
} GeGbiRareTextureState;

typedef struct GeGbiImageState {
    GeGbiAddress address;
    uint16_t width;
    uint8_t format;
    uint8_t size;
} GeGbiImageState;

typedef struct GeGbiTileState {
    uint16_t line;
    uint16_t tmem;
    uint16_t upper_s;
    uint16_t upper_t;
    uint16_t lower_s;
    uint16_t lower_t;
    uint8_t format;
    uint8_t size;
    uint8_t palette;
    uint8_t clamp_mirror_s;
    uint8_t clamp_mirror_t;
    uint8_t mask_s;
    uint8_t mask_t;
    uint8_t shift_s;
    uint8_t shift_t;
} GeGbiTileState;

typedef struct GeGbiTextureRectangle {
    GeGbiScreenRect screen;
    int16_t s;
    int16_t t;
    int16_t dsdx;
    int16_t dtdy;
    uint8_t tile;
    uint8_t flipped;
} GeGbiTextureRectangle;

typedef struct GeGbiRenderState {
    uint32_t geometry_mode;
    uint32_t other_mode_low;
    uint32_t other_mode_high;
    uint32_t combine_mux0;
    uint32_t combine_mux1;
    uint32_t environment_color;
    uint32_t primitive_color;
    uint32_t blend_color;
    uint32_t fog_color;
    uint32_t fill_color;
    uint16_t valid_vertices;
    GeGbiTextureState texture;
    GeGbiRareTextureState rare_texture;
    GeGbiImageState texture_image;
    GeGbiTileState tiles[8];
    GeGbiMatrixStack modelview_stack;
    GeGbiMatrixStack projection_stack;
    GeGbiViewport viewport;
    GeGbiLight lights[GE_GBI_LIGHT_COUNT];
    GeGbiLight look_at[2];
    uint32_t segment_bases[16];
    uint16_t valid_segment_bases;
    uint16_t perspective_normalization;
    int16_t fog_multiplier;
    int16_t fog_offset;
    uint8_t directional_light_count;
    uint8_t valid_lights;
    uint8_t valid_look_at;
    uint8_t viewport_valid;
    uint8_t rare_texture_valid;
    uint8_t texture_image_valid;
    uint8_t active_texture_binding;
    GeGbiTextureRectangle pending_texture_rectangle;
    uint8_t texture_rectangle_phase;
} GeGbiRenderState;

typedef struct GeGbiStateAction {
    GeGbiStateActionKind kind;
    union {
        struct {
            GeGbiAddress address;
            uint8_t parameters;
            uint8_t has_value;
            GeGbiMatrix value;
        } matrix;
        struct {
            GeGbiAddress address;
            uint8_t has_value;
            GeGbiViewport value;
        } viewport;
        struct {
            GeGbiAddress address;
            uint8_t slot;
            uint8_t has_value;
            GeGbiLight value;
        } light;
        struct {
            GeGbiAddress address;
            uint8_t axis;
            uint8_t has_value;
            GeGbiLight value;
        } look_at;
        struct {
            GeGbiAddress address;
            uint16_t count;
            uint8_t first;
        } vertices;
        struct {
            GeGbiTriangle triangles[4];
            uint8_t count;
        } draw;
        GeGbiScreenRect fill_rectangle;
        GeGbiTextureRectangle texture_rectangle;
        struct {
            GeGbiAddress address;
        } display_list;
    } data;
} GeGbiStateAction;

void ge_gbi_state_init(GeGbiRenderState *state);

/*
 * Applies one decoded command to the portable RSP/RDP shadow state. Commands
 * which require the platform renderer or traversal layer return a typed action.
 */
GeGbiStateStatus ge_gbi_state_apply(GeGbiRenderState *state,
                                    const GeGbiCommand *command,
                                    GeGbiStateAction *action);

GeGbiStateStatus ge_gbi_state_apply_matrix(GeGbiRenderState *state,
                                           const GeGbiMatrix *matrix,
                                           uint8_t parameters);

#ifdef __cplusplus
}
#endif

#endif
