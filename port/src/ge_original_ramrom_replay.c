#include "ge_original_ramrom_replay.h"

#include <limits.h>
#include <string.h>

static uint32_t ge_ramrom_be32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24U)
        | ((uint32_t)bytes[1] << 16U)
        | ((uint32_t)bytes[2] << 8U)
        | (uint32_t)bytes[3];
}

static uint64_t ge_ramrom_be64(const uint8_t *bytes)
{
    return ((uint64_t)ge_ramrom_be32(bytes) << 32U)
        | ge_ramrom_be32(bytes + 4U);
}

GeOriginalRamromStatus ge_original_ramrom_replay_begin(
    GeOriginalRamromReplay *replay, const void *data, size_t data_size)
{
    const uint8_t *bytes = data;
    size_t controller;
    if (replay == NULL || data == NULL)
        return GE_ORIGINAL_RAMROM_INVALID_ARGUMENT;
    memset(replay, 0, sizeof(*replay));
    if (data_size < GE_ORIGINAL_RAMROM_HEADER_SIZE)
        return GE_ORIGINAL_RAMROM_TRUNCATED;
    replay->header.random_seed = ge_ramrom_be64(bytes);
    replay->header.character_random_seed = ge_ramrom_be64(bytes + 8U);
    replay->header.stage_id = (int32_t)ge_ramrom_be32(bytes + 0x10U);
    replay->header.difficulty = (int32_t)ge_ramrom_be32(bytes + 0x14U);
    replay->header.controller_count = ge_ramrom_be32(bytes + 0x18U);
    replay->header.total_time_60 = ge_ramrom_be32(bytes + 0x7cU);
    replay->header.file_size = ge_ramrom_be32(bytes + 0x80U);
    replay->header.game_mode = (int32_t)ge_ramrom_be32(bytes + 0x84U);
    replay->header.slot_number = ge_ramrom_be32(bytes + 0x88U);
    replay->header.player_count = ge_ramrom_be32(bytes + 0x8cU);
    for (controller = 0U;
            controller < GE_ORIGINAL_RAMROM_MAX_CONTROLLERS;
            ++controller) {
        replay->header.control_style[controller] =
            ge_ramrom_be32(bytes + 0xc0U + controller * 4U);
    }
    replay->header.aim_option = ge_ramrom_be32(bytes + 0xd0U);
    if (replay->header.controller_count == 0U
            || replay->header.controller_count
                > GE_ORIGINAL_RAMROM_MAX_CONTROLLERS
            || replay->header.player_count
                >= GE_ORIGINAL_RAMROM_MAX_CONTROLLERS
            || replay->header.file_size < GE_ORIGINAL_RAMROM_HEADER_SIZE
            || replay->header.file_size > data_size
            || replay->header.total_time_60 == 0U)
        return GE_ORIGINAL_RAMROM_INVALID_HEADER;
    replay->data = bytes;
    replay->data_size = replay->header.file_size;
    replay->cursor = GE_ORIGINAL_RAMROM_HEADER_SIZE;
    return GE_ORIGINAL_RAMROM_OK;
}

GeOriginalRamromStatus ge_original_ramrom_replay_next(
    GeOriginalRamromReplay *replay, GeOriginalRamromBlock *block)
{
    const uint8_t *source;
    size_t sample_bytes;
    size_t stride;
    size_t index;
    uint8_t checksum;
    if (replay == NULL || block == NULL || replay->data == NULL)
        return GE_ORIGINAL_RAMROM_INVALID_ARGUMENT;
    memset(block, 0, sizeof(*block));
    if (replay->complete != 0U) return GE_ORIGINAL_RAMROM_COMPLETE;
    if (replay->cursor > replay->data_size
            || replay->data_size - replay->cursor < 4U)
        return GE_ORIGINAL_RAMROM_TRUNCATED;
    source = replay->data + replay->cursor;
    block->speed_frames = source[0];
    block->sample_count = source[1];
    block->random_seed_check = source[2];
    block->checksum = source[3];
    block->block_index = replay->block_index;
    if (block->speed_frames == 0U && block->sample_count == 0U) {
        replay->complete = 1U;
        return GE_ORIGINAL_RAMROM_COMPLETE;
    }
    if ((size_t)block->sample_count > SIZE_MAX
            / replay->header.controller_count / 4U)
        return GE_ORIGINAL_RAMROM_INVALID_HEADER;
    sample_bytes = (size_t)block->sample_count
        * replay->header.controller_count * 4U;
    /* The decompiled address update is
     * align_addr_even(sample_bytes + 5). Its original macro clears the low
     * address bit (rounds down), so the authored four-byte-multiple payloads
     * advance by their four-byte header plus the exact sample bytes. */
    if (sample_bytes > SIZE_MAX - 5U)
        return GE_ORIGINAL_RAMROM_INVALID_HEADER;
    stride = (sample_bytes + 5U) & ~(size_t)1U;
    if (stride < 4U || stride > replay->data_size - replay->cursor)
        return GE_ORIGINAL_RAMROM_TRUNCATED;
    checksum = (uint8_t)(block->speed_frames + block->sample_count
        + block->random_seed_check);
    for (index = 0U; index < sample_bytes; ++index)
        checksum = (uint8_t)(checksum + source[4U + index]);
    if (checksum != block->checksum)
        return GE_ORIGINAL_RAMROM_CHECKSUM_MISMATCH;
    block->sample_bytes = source + 4U;
    block->sample_bytes_size = sample_bytes;
    replay->cursor += stride;
    ++replay->block_index;
    return GE_ORIGINAL_RAMROM_OK;
}

int ge_original_ramrom_block_pad(const GeOriginalRamromReplay *replay,
    const GeOriginalRamromBlock *block, size_t sample_index,
    size_t controller_index, GeOriginalRamromPad *pad)
{
    size_t offset;
    const uint8_t *bytes;
    if (replay == NULL || block == NULL || pad == NULL
            || block->sample_bytes == NULL
            || sample_index >= block->sample_count
            || controller_index >= replay->header.controller_count)
        return 0;
    offset = (sample_index * replay->header.controller_count
        + controller_index) * 4U;
    if (offset > block->sample_bytes_size
            || block->sample_bytes_size - offset < 4U)
        return 0;
    bytes = block->sample_bytes + offset;
    pad->stick_x = (int8_t)bytes[0];
    pad->stick_y = (int8_t)bytes[1];
    pad->buttons = (uint16_t)((uint16_t)bytes[3] << 8U | bytes[2]);
    return 1;
}

const char *ge_original_ramrom_status_name(GeOriginalRamromStatus status)
{
    switch (status) {
    case GE_ORIGINAL_RAMROM_OK: return "ok";
    case GE_ORIGINAL_RAMROM_COMPLETE: return "complete";
    case GE_ORIGINAL_RAMROM_INVALID_ARGUMENT: return "invalid argument";
    case GE_ORIGINAL_RAMROM_TRUNCATED: return "truncated";
    case GE_ORIGINAL_RAMROM_INVALID_HEADER: return "invalid header";
    case GE_ORIGINAL_RAMROM_CHECKSUM_MISMATCH: return "checksum mismatch";
    default: return "unknown";
    }
}
