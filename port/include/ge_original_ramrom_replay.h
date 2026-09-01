#ifndef GE_ORIGINAL_RAMROM_REPLAY_H
#define GE_ORIGINAL_RAMROM_REPLAY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    /* sizeof(ramromfilestructure) in the original 32-bit ABI. The recorder
     * writes 0xf0 bytes, but advances by this 0xe8-byte sizeof boundary; the
     * first block therefore begins at 0xe8 in every authored demo. */
    GE_ORIGINAL_RAMROM_HEADER_SIZE = 0xe8,
    GE_ORIGINAL_RAMROM_MAX_CONTROLLERS = 4,
};

typedef enum GeOriginalRamromStatus {
    GE_ORIGINAL_RAMROM_OK = 0,
    GE_ORIGINAL_RAMROM_COMPLETE,
    GE_ORIGINAL_RAMROM_INVALID_ARGUMENT,
    GE_ORIGINAL_RAMROM_TRUNCATED,
    GE_ORIGINAL_RAMROM_INVALID_HEADER,
    GE_ORIGINAL_RAMROM_CHECKSUM_MISMATCH,
} GeOriginalRamromStatus;

typedef struct GeOriginalRamromHeader {
    uint64_t random_seed;
    uint64_t character_random_seed;
    int32_t stage_id;
    int32_t difficulty;
    uint32_t controller_count;
    uint32_t total_time_60;
    uint32_t file_size;
    int32_t game_mode;
    uint32_t slot_number;
    uint32_t player_count;
    uint32_t control_style[GE_ORIGINAL_RAMROM_MAX_CONTROLLERS];
    uint32_t aim_option;
} GeOriginalRamromHeader;

typedef struct GeOriginalRamromPad {
    int8_t stick_x;
    int8_t stick_y;
    uint16_t buttons;
} GeOriginalRamromPad;

typedef struct GeOriginalRamromBlock {
    uint8_t speed_frames;
    uint8_t sample_count;
    uint8_t random_seed_check;
    uint8_t checksum;
    uint32_t block_index;
    const uint8_t *sample_bytes;
    size_t sample_bytes_size;
} GeOriginalRamromBlock;

typedef struct GeOriginalRamromReplay {
    const uint8_t *data;
    size_t data_size;
    size_t cursor;
    uint32_t block_index;
    GeOriginalRamromHeader header;
    uint8_t complete;
} GeOriginalRamromReplay;

GeOriginalRamromStatus ge_original_ramrom_replay_begin(
    GeOriginalRamromReplay *replay, const void *data, size_t data_size);

/* This is the bounded native equivalent of the unchanged
 * iterate_ramrom_entries_handle_camera_out block walk. Each returned block
 * retains the original speedframes/count/seed/check header and the exact
 * controller samples that ramrom_replay_handler copies into joy.c. */
GeOriginalRamromStatus ge_original_ramrom_replay_next(
    GeOriginalRamromReplay *replay, GeOriginalRamromBlock *block);

int ge_original_ramrom_block_pad(const GeOriginalRamromReplay *replay,
    const GeOriginalRamromBlock *block, size_t sample_index,
    size_t controller_index, GeOriginalRamromPad *pad);

const char *ge_original_ramrom_status_name(GeOriginalRamromStatus status);

#ifdef __cplusplus
}
#endif

#endif
