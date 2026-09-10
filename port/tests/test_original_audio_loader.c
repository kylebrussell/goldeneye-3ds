#include "src/libultra/audio/synthInternals.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static ALADPCMBook book;
static ADPCM_STATE state, loop_state;

u32 osVirtualToPhysical(void *pointer)
{
    if (pointer == book.book) return 0xac001230U;
    if (pointer == &state) return 0xbc004560U;
    assert(pointer == &loop_state);
    return 0xcc007890U;
}

static s32 dma(s32 address, s32 length, void *context)
{
    (void)context;
    assert(length > 0 && length <= 36);
    return address;
}

static u32 expected(void *pointer)
{
#if defined(GE_PORT_LIBAUDIO_NATIVE)
    return osVirtualToPhysical(pointer);
#else
    return (u32)(uintptr_t)pointer & 0x1fffffffU;
#endif
}

int main(void)
{
    Acmd commands[32], *end;
    ALWaveTable table = {0};
    ALLoadFilter filter = {0};
    unsigned books = 0, loops = 0, decoders = 0;
    s16 output = AL_DECODER_OUT;
    table.base = (u8 *)(uintptr_t)0x08001000U;
    table.len = 36;
    table.type = AL_ADPCM_WAVE;
    table.waveInfo.adpcmWave.book = &book;
    filter.table = &table;
    filter.bookSize = 32;
    filter.state = &state;
    filter.lstate = &loop_state;
    filter.dma = dma;
    filter.memin = (s32)(uintptr_t)table.base;
    filter.first = A_INIT;
    filter.loop.start = 0;
    filter.loop.end = 16;
    filter.loop.count = -1;
    end = alAdpcmPull(&filter, &output, 48, 0, commands);
    assert(end > commands && end <= commands + 32);
    for (Acmd *command = commands; command < end; ++command) {
        switch (command->words.w0 >> 24) {
        case A_LOADADPCM:
            assert(command->words.w1 == expected(book.book));
            ++books;
            break;
        case A_SETLOOP:
            assert(command->words.w1 == expected(&loop_state));
            ++loops;
            break;
        case A_ADPCM:
            assert(command->words.w1 == expected(&state));
            ++decoders;
            break;
        }
    }
    assert(books == 1 && loops == 2 && decoders == 3);
    puts("Canonical ADPCM loader: book, decoder and repeated loop addresses pass");
    return 0;
}
