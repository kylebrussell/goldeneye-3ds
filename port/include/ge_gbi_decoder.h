#ifndef GE_GBI_DECODER_H
#define GE_GBI_DECODER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * GoldenEye uses the original Fast3D layout with Rare's TRI4 and SETTEX
 * extensions. This decoder deliberately does not accept the incompatible
 * Fast3DEX2 layouts which reuse several of the same opcodes.
 */
typedef enum GeGbiByteOrder {
    GE_GBI_BYTE_ORDER_BIG_ENDIAN,
    GE_GBI_BYTE_ORDER_LITTLE_ENDIAN
} GeGbiByteOrder;

typedef enum GeGbiStatus {
    GE_GBI_STATUS_OK,
    GE_GBI_STATUS_INVALID_ARGUMENT,
    GE_GBI_STATUS_TRUNCATED,
    GE_GBI_STATUS_OUTPUT_FULL,
    GE_GBI_STATUS_MALFORMED
} GeGbiStatus;

typedef enum GeGbiCommandKind {
    GE_GBI_COMMAND_UNKNOWN,
    GE_GBI_COMMAND_NOOP,
    GE_GBI_COMMAND_MATRIX,
    GE_GBI_COMMAND_MOVE_MEMORY,
    GE_GBI_COMMAND_VERTEX,
    GE_GBI_COMMAND_DISPLAY_LIST,
    GE_GBI_COMMAND_TRIANGLE,
    GE_GBI_COMMAND_TRIANGLE4,
    GE_GBI_COMMAND_CLEAR_GEOMETRY_MODE,
    GE_GBI_COMMAND_SET_GEOMETRY_MODE,
    GE_GBI_COMMAND_END_DISPLAY_LIST,
    GE_GBI_COMMAND_POP_MATRIX,
    GE_GBI_COMMAND_MOVE_WORD,
    GE_GBI_COMMAND_TEXTURE,
    GE_GBI_COMMAND_SET_OTHER_MODE_LOW,
    GE_GBI_COMMAND_SET_OTHER_MODE_HIGH,
    GE_GBI_COMMAND_RARE_SET_TEXTURE,
    GE_GBI_COMMAND_SET_TEXTURE_IMAGE,
    GE_GBI_COMMAND_SET_COMBINE,
    GE_GBI_COMMAND_SET_ENV_COLOR,
    GE_GBI_COMMAND_SET_PRIM_COLOR,
    GE_GBI_COMMAND_SET_BLEND_COLOR,
    GE_GBI_COMMAND_SET_FOG_COLOR,
    GE_GBI_COMMAND_SET_FILL_COLOR,
    GE_GBI_COMMAND_FILL_RECTANGLE,
    GE_GBI_COMMAND_TEXTURE_RECTANGLE,
    GE_GBI_COMMAND_RDP_HALF_1,
    GE_GBI_COMMAND_RDP_HALF_2,
    GE_GBI_COMMAND_SET_TILE,
    GE_GBI_COMMAND_LOAD_TILE,
    GE_GBI_COMMAND_LOAD_BLOCK,
    GE_GBI_COMMAND_SET_TILE_SIZE,
    GE_GBI_COMMAND_LOAD_TLUT,
    GE_GBI_COMMAND_PIPE_SYNC,
    GE_GBI_COMMAND_TILE_SYNC,
    GE_GBI_COMMAND_LOAD_SYNC,
    GE_GBI_COMMAND_FULL_SYNC
} GeGbiCommandKind;

typedef struct GeGbiAddress {
    uint32_t raw;
    uint32_t offset;
    uint8_t segment;
} GeGbiAddress;

typedef struct GeGbiTriangle {
    uint8_t vertex[3];
} GeGbiTriangle;

typedef struct GeGbiTileRect {
    uint16_t upper_s;
    uint16_t upper_t;
    uint16_t lower_s;
    uint16_t lower_t;
    uint8_t tile;
} GeGbiTileRect;

typedef struct GeGbiScreenRect {
    uint16_t upper_x;
    uint16_t upper_y;
    uint16_t lower_x;
    uint16_t lower_y;
} GeGbiScreenRect;

/* Fast3D emits a textured rectangle as three consecutive commands. The first
 * carries its 10.2 screen bounds and render tile; RDP_HALF_1 carries the
 * signed 10.5 texture origin; RDP_HALF_2 carries signed 5.10 derivatives. */
typedef struct GeGbiTextureRectCommand {
    GeGbiScreenRect screen;
    uint8_t tile;
    uint8_t flipped;
} GeGbiTextureRectCommand;

typedef struct GeGbiRdpHalf {
    uint32_t word;
    int16_t high;
    int16_t low;
} GeGbiRdpHalf;

typedef enum GeGbiMoveMemoryKind {
    GE_GBI_MOVE_MEMORY_UNKNOWN,
    GE_GBI_MOVE_MEMORY_VIEWPORT,
    GE_GBI_MOVE_MEMORY_LOOK_AT_Y,
    GE_GBI_MOVE_MEMORY_LOOK_AT_X,
    GE_GBI_MOVE_MEMORY_LIGHT,
    GE_GBI_MOVE_MEMORY_FORCE_MATRIX_PART
} GeGbiMoveMemoryKind;

typedef enum GeGbiMoveWordIndex {
    GE_GBI_MOVE_WORD_MATRIX = 0x00,
    GE_GBI_MOVE_WORD_NUM_LIGHTS = 0x02,
    GE_GBI_MOVE_WORD_CLIP = 0x04,
    GE_GBI_MOVE_WORD_SEGMENT = 0x06,
    GE_GBI_MOVE_WORD_FOG = 0x08,
    GE_GBI_MOVE_WORD_LIGHT_COLOR = 0x0a,
    GE_GBI_MOVE_WORD_POINTS = 0x0c,
    GE_GBI_MOVE_WORD_PERSP_NORM = 0x0e
} GeGbiMoveWordIndex;

typedef struct GeGbiCommand {
    GeGbiCommandKind kind;
    size_t byte_offset;
    uint32_t raw_w0;
    uint32_t raw_w1;
    uint8_t opcode;
    union {
        struct {
            GeGbiAddress address;
            uint16_t length;
            uint8_t parameters;
        } dma;
        struct {
            GeGbiAddress address;
            uint16_t length;
            GeGbiMoveMemoryKind kind;
            uint8_t index;
            uint8_t light_slot;
            uint8_t matrix_part;
        } move_memory;
        struct {
            GeGbiAddress address;
            uint16_t count;
            uint8_t first;
        } vertex;
        struct {
            GeGbiAddress address;
            uint8_t push;
        } display_list;
        struct {
            uint8_t projection;
        } pop_matrix;
        struct {
            GeGbiTriangle triangles[4];
            uint8_t count;
        } geometry;
        struct {
            uint32_t mask;
        } geometry_mode;
        struct {
            uint16_t scale_s;
            uint16_t scale_t;
            uint8_t level;
            uint8_t tile;
            uint8_t enabled;
            uint8_t reserved;
        } texture;
        struct {
            uint16_t shift;
            uint8_t length;
            uint32_t data;
        } other_mode;
        struct {
            uint16_t offset;
            uint8_t index;
            uint32_t data;
        } move_word;
        struct {
            uint16_t texture_id;
            uint16_t detail_texture_id;
            uint8_t min_level;
            uint8_t type;
            uint8_t tile;
            uint8_t clamp_mirror_s;
            uint8_t clamp_mirror_t;
            uint8_t shift_s;
            uint8_t shift_t;
        } rare_texture;
        struct {
            GeGbiAddress address;
            uint16_t width;
            uint8_t format;
            uint8_t size;
        } image;
        struct {
            uint32_t mux0;
            uint32_t mux1;
        } combine;
        struct {
            uint8_t red;
            uint8_t green;
            uint8_t blue;
            uint8_t alpha;
            uint8_t min_level;
            uint8_t lod_fraction;
        } color;
        GeGbiScreenRect screen_rect;
        GeGbiTextureRectCommand texture_rect;
        GeGbiRdpHalf rdp_half;
        struct {
            uint16_t line;
            uint16_t tmem;
            uint8_t format;
            uint8_t size;
            uint8_t tile;
            uint8_t palette;
            uint8_t clamp_mirror_s;
            uint8_t clamp_mirror_t;
            uint8_t mask_s;
            uint8_t mask_t;
            uint8_t shift_s;
            uint8_t shift_t;
        } tile;
        GeGbiTileRect tile_rect;
        struct {
            uint16_t upper_s;
            uint16_t upper_t;
            uint16_t lower_s;
            uint16_t dxt;
            uint8_t tile;
        } load_block;
        struct {
            uint16_t count;
            uint8_t tile;
        } load_tlut;
    } data;
} GeGbiCommand;

typedef struct GeGbiDecodeResult {
    GeGbiStatus status;
    size_t bytes_consumed;
    size_t commands_decoded;
    uint8_t reached_end_display_list;
} GeGbiDecodeResult;

GeGbiStatus ge_gbi_decode_command(const uint8_t *bytes,
                                  size_t byte_count,
                                  GeGbiByteOrder byte_order,
                                  size_t byte_offset,
                                  GeGbiCommand *command);

/* Stops after the first end-display-list command. Unknown opcodes are kept. */
GeGbiDecodeResult ge_gbi_decode_list(const uint8_t *bytes,
                                     size_t byte_count,
                                     GeGbiByteOrder byte_order,
                                     GeGbiCommand *commands,
                                     size_t command_capacity);

const char *ge_gbi_command_kind_name(GeGbiCommandKind kind);

#ifdef __cplusplus
}
#endif

#endif
