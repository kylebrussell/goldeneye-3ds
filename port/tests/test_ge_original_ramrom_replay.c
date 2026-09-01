#include "ge_original_ramrom_replay.h"
#include "ge_original_input.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *read_file(const char *path, size_t *size)
{
    FILE *stream = fopen(path, "rb");
    uint8_t *bytes;
    long length;
    assert(stream != NULL && fseek(stream, 0, SEEK_END) == 0);
    length = ftell(stream);
    assert(length > 0 && fseek(stream, 0, SEEK_SET) == 0);
    bytes = malloc((size_t)length);
    assert(bytes != NULL
        && fread(bytes, 1U, (size_t)length, stream) == (size_t)length);
    fclose(stream);
    *size = (size_t)length;
    return bytes;
}

static void verify_demo(const char *path, int first)
{
    GeOriginalRamromReplay replay;
    GeOriginalRamromBlock block;
    GeOriginalRamromStatus status;
    uint8_t *bytes;
    size_t size;
    size_t blocks = 0U;
    size_t samples = 0U;
    bytes = read_file(path, &size);
    assert(ge_original_ramrom_replay_begin(&replay, bytes, size)
        == GE_ORIGINAL_RAMROM_OK);
    assert(replay.header.file_size <= size
        && replay.header.controller_count >= 1U
        && replay.header.controller_count <= 4U);
    if (first) {
        assert(replay.header.random_seed
            == UINT64_C(0x00000000ce035af9));
        assert(replay.header.character_random_seed
            == UINT64_C(0x000000002545d9a3));
        assert(replay.header.stage_id == 0x21);
        assert(replay.header.difficulty == 0);
        assert(replay.header.controller_count == 3U);
        assert(replay.header.total_time_60 == 0x5baU);
        assert(replay.header.file_size == 0x51fcU);
    }
    while ((status = ge_original_ramrom_replay_next(&replay, &block))
            == GE_ORIGINAL_RAMROM_OK) {
        GeOriginalRamromPad pad;
        assert(block.sample_count != 0U || block.speed_frames != 0U);
        if (block.sample_count != 0U) {
            assert(ge_original_ramrom_block_pad(
                &replay, &block, 0U, 0U, &pad));
            assert(!ge_original_ramrom_block_pad(
                &replay, &block, block.sample_count, 0U, &pad));
        }
        samples += block.sample_count;
        ++blocks;
    }
    if (status != GE_ORIGINAL_RAMROM_COMPLETE)
        fprintf(stderr, "%s: block %zu at 0x%zx: %s\n", path,
            blocks, replay.cursor, ge_original_ramrom_status_name(status));
    assert(status == GE_ORIGINAL_RAMROM_COMPLETE);
    assert(blocks != 0U && samples != 0U
        && replay.cursor <= replay.header.file_size);
    free(bytes);
}

int main(int argc, char **argv)
{
    static const char *names[] = {
        "ramrom_Dam_1.bin", "ramrom_Dam_2.bin",
        "ramrom_Facility_1.bin", "ramrom_Facility_2.bin",
        "ramrom_Facility_3.bin", "ramrom_Runway_1.bin",
        "ramrom_Runway_2.bin", "ramrom_BunkerI_1.bin",
        "ramrom_BunkerI_2.bin", "ramrom_Silo_1.bin",
        "ramrom_Silo_2.bin", "ramrom_Frigate_1.bin",
        "ramrom_Frigate_2.bin", "ramrom_Train.bin",
    };
    size_t index;
    assert(argc == 2);
    assert(ge_original_ramrom_replay_begin(NULL, NULL, 0U)
        == GE_ORIGINAL_RAMROM_INVALID_ARGUMENT);
    for (index = 0U; index < sizeof(names) / sizeof(names[0]); ++index) {
        char path[1024];
        int length = snprintf(path, sizeof(path), "%s/%s", argv[1], names[index]);
        assert(length > 0 && (size_t)length < sizeof(path));
        verify_demo(path, index == 0U);
    }
    {
        uint8_t *bytes;
        size_t size;
        char path[1024];
        GeOriginalRamromReplay replay;
        GeOriginalRamromBlock block;
        int length = snprintf(path, sizeof(path), "%s/%s", argv[1], names[0]);
        assert(length > 0 && (size_t)length < sizeof(path));
        bytes = read_file(path, &size);
        {
            GeOriginalInputSample input = {0};
            GeOriginalRamromPad expected;
            GeOriginalBondInputFrame actual;
            assert(ge_original_ramrom_replay_begin(
                &replay, bytes, size) == GE_ORIGINAL_RAMROM_OK);
            assert(ge_original_ramrom_replay_next(&replay, &block)
                == GE_ORIGINAL_RAMROM_OK && block.sample_count != 0U);
            assert(ge_original_ramrom_block_pad(
                &replay, &block, block.sample_count - 1U, 0U,
                &expected));
            ge_original_input_init();
            assert(ge_original_input_ramrom_bind(
                replay.header.controller_count));
            assert(ge_original_input_ramrom_queue(&replay, &block));
            ge_original_input_tick(&input);
            ge_original_input_read_bond_frame(0U, &actual);
            assert(actual.stick_x == expected.stick_x
                && actual.stick_y == expected.stick_y
                && actual.buttons == expected.buttons);
            ge_original_input_ramrom_unbind();
        }
        assert(ge_original_ramrom_replay_begin(&replay, bytes, size)
            == GE_ORIGINAL_RAMROM_OK);
        bytes[GE_ORIGINAL_RAMROM_HEADER_SIZE + 3U] ^= UINT8_C(1);
        assert(ge_original_ramrom_replay_next(&replay, &block)
            == GE_ORIGINAL_RAMROM_CHECKSUM_MISMATCH);
        free(bytes);
    }
    puts("all 14 authored RAMROM attract demos parse exactly");
    return 0;
}
