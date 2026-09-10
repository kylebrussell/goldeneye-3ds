#include "ge_audio_abi.h"
#include "src/libultra/audio/synthInternals.h"

#include <assert.h>
#include <stdio.h>

extern void init_lpfilter(ALLowPass *lp);
#if defined(GE_PORT_LIBAUDIO_NATIVE)
extern Acmd *_filterBuffer(ALLowPass *lp, s32 buff, s32 count, Acmd *p);

static ALLowPass *current_filter;
static uint8_t filter_state[8];

static void clear_bytes(void *pointer, size_t size)
{
    uint8_t *bytes = pointer;
    size_t i;
    for (i = 0; i < size; ++i) bytes[i] = 0;
}

u32 osVirtualToPhysical(void *pointer)
{
    if (pointer == current_filter->fcvec.fccoef) return 0x1000;
    assert(pointer == current_filter->fstate);
    return 0x2000;
}

static void *resolve(void *context, uint32_t address, size_t size)
{
    (void)context;
    if (address == 0x1000 && size <= 32) return current_filter->fcvec.fccoef;
    if (address == 0x2000 && size <= sizeof(filter_state)) return filter_state;
    return NULL;
}

static int16_t read_be16(const uint8_t *p)
{
    return (int16_t)((uint16_t)p[0] << 8 | p[1]);
}

static void write_be16(uint8_t *p, int16_t value)
{
    p[0] = (uint8_t)((uint16_t)value >> 8);
    p[1] = (uint8_t)value;
}

static void impulse(ALLowPass *filter, int polarity, FILE *dump, int check_decay)
{
    GeAudioAbiState abi;
    unsigned block, sample;
    int max_tail = 0;
    ge_audio_abi_init(&abi);
    clear_bytes(filter_state, sizeof(filter_state));
    current_filter = filter;
    filter->fstate = (POLEF_STATE *)filter_state;
    filter->first = 1;
    for (block = 0; block < 2757; ++block) {
        Acmd commands[3];
        Acmd *end;
        clear_bytes(abi.dmem + 0x100, 16);
        if (block == 0) write_be16(abi.dmem + 0x100, (int16_t)(16000 * polarity));
        end = _filterBuffer(filter, 0x100, 8, commands);
        assert(end - commands == 3);
        assert(ge_audio_abi_execute(&abi, (const GeAudioAbiCommand *)commands,
            3, resolve, NULL) == GE_AUDIO_ABI_OK);
        for (sample = 0; sample < 8; ++sample) {
            int16_t value = read_be16(abi.dmem + 0x100 + sample * 2);
            int magnitude = value < 0 ? -(int)value : value;
            if (check_decay) {
                assert(magnitude <= 16000);
                if (block > 32 && magnitude > max_tail) max_tail = magnitude;
            }
            if (dump) {
                uint8_t bytes[2] = {(uint8_t)value, (uint8_t)((uint16_t)value >> 8)};
                assert(fwrite(bytes, 1, sizeof(bytes), dump) == sizeof(bytes));
            }
        }
    }
    /* A positive impulse disappears; negative fixed-point rounding may leave
     * a one-unit DC remainder, but must never produce a sustained tone. */
    if (check_decay) assert(max_tail <= 1);
}
#endif

int main(int argc, char **argv)
{
    static const int16_t cutoffs[] = {0x2500, 0x3000, 0x3500, 0x4000, 0x4500, 0x5000};
    /* Independent expected Q14 powers of fc/32768, truncated per coefficient. */
    static const int16_t expected[][8] = {
        {4736, 1369, 395, 114, 33, 9, 2, 0},
        {6144, 2304, 864, 324, 121, 45, 17, 6},
        {6784, 2809, 1163, 481, 199, 82, 34, 14},
        {8192, 4096, 2048, 1024, 512, 256, 128, 64},
        {8832, 4761, 2566, 1383, 745, 402, 216, 116},
        {10240, 6400, 4000, 2500, 1562, 976, 610, 381},
    };
    size_t index;
    for (index = 0; index < sizeof(cutoffs) / sizeof(cutoffs[0]); ++index) {
        ALLowPass filter = {0};
        unsigned coefficient;
        filter.fc = cutoffs[index];
        init_lpfilter(&filter);
        assert(filter.fgain == 16384 - cutoffs[index] / 2);
#if defined(GE_PORT_LIBAUDIO_NATIVE)
        for (coefficient = 0; coefficient < 16; ++coefficient)
            assert(read_be16((const uint8_t *)filter.fcvec.fccoef + coefficient * 2)
                == (coefficient < 8 ? 0 : expected[index][coefficient - 8]));
        impulse(&filter, 1, NULL, 1);
        impulse(&filter, -1, NULL, 1);
#else
        /* The original N64 branch still produces numeric coefficients. On
         * big-endian MIPS these already have the RSP's required encoding. */
        for (coefficient = 0; coefficient < 16; ++coefficient)
            assert(filter.fcvec.fccoef[coefficient]
                == (coefficient < 8 ? 0 : expected[index][coefficient - 8]));
#endif
    }
#if defined(GE_PORT_LIBAUDIO_NATIVE)
    if (argc == 3) {
        ALLowPass filter = {0};
        FILE *correct = fopen(argv[1], "wb");
        FILE *swapped = fopen(argv[2], "wb");
        unsigned i;
        assert(correct && swapped);
        filter.fc = 0x4500; /* Final custom GoldenEye reverb section. */
        init_lpfilter(&filter);
        impulse(&filter, 1, correct, 1);
        /* Reconstruct the previous little-endian producer defect explicitly
         * so this diagnostic works on either host byte order. */
        for (i = 0; i < 16; ++i) {
            uint8_t *p = (uint8_t *)filter.fcvec.fccoef + i * 2;
            uint8_t first = p[0]; p[0] = p[1]; p[1] = first;
        }
        impulse(&filter, 1, swapped, 0);
        fclose(correct);
        fclose(swapped);
    }
    puts("original reverb coefficient producer and ABI impulse decay passed");
#else
    (void)argc;
    (void)argv;
    puts("original N64 reverb coefficient producer branch unchanged");
#endif
    return 0;
}
