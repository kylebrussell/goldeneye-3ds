#include "ge_audio_abi.h"

#include "src/libultra/audio/synthInternals.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>

enum {
    LEFT_ADDRESS = 0x1000,
    RIGHT_ADDRESS = 0x2000,
    OUTPUT_ADDRESS = 0x3000,
    ADPCM_BOOK_ADDRESS = 0x4000,
    ADPCM_LOOP_ADDRESS = 0x5000,
    ADPCM_STATE_ADDRESS = 0x6000,
    RESAMPLE_STATE_ADDRESS = 0x7000,
    ENVMIX_STATE_ADDRESS = 0x8800,
    POLEF_STATE_ADDRESS = 0x9000,
    TEST_FRAMES = 4
};

typedef struct TestMemory {
    uint8_t left[TEST_FRAMES * 2];
    uint8_t right[TEST_FRAMES * 2];
    uint8_t output[TEST_FRAMES * 4];
    uint8_t adpcm_book[32];
    uint8_t adpcm_loop[32];
    uint8_t adpcm_state[32];
    uint8_t resample_state[32];
    uint8_t envmix_state[80];
    uint8_t polef_state[8];
} TestMemory;

typedef struct TestSource {
    ALFilter filter;
} TestSource;

/* Link target for the original debug-only libaudio assertion path. */
void osSyncPrintf(const char *format, ...)
{
    (void)format;
}

static void store_be16(uint8_t *destination, int16_t value)
{
    uint16_t bits = (uint16_t)value;

    destination[0] = (uint8_t)(bits >> 8U);
    destination[1] = (uint8_t)bits;
}

static int16_t load_be16(const uint8_t *source)
{
    return (int16_t)((uint16_t)((uint16_t)source[0] << 8U)
            | (uint16_t)source[1]);
}

static void fill_bytes(uint8_t *destination, uint8_t value, size_t count)
{
    size_t index;

    for (index = 0U; index < count; ++index) {
        destination[index] = value;
    }
}

static void copy_bytes(
        uint8_t *destination,
        const uint8_t *source,
        size_t count)
{
    size_t index;

    for (index = 0U; index < count; ++index) {
        destination[index] = source[index];
    }
}

static void *resolve_test_address(void *context, uint32_t address, size_t size)
{
    TestMemory *memory = context;

    if (address == LEFT_ADDRESS && size <= sizeof(memory->left)) {
        return memory->left;
    }
    if (address == RIGHT_ADDRESS && size <= sizeof(memory->right)) {
        return memory->right;
    }
    if (address == OUTPUT_ADDRESS && size <= sizeof(memory->output)) {
        return memory->output;
    }
    if (address == ADPCM_BOOK_ADDRESS
            && size <= sizeof(memory->adpcm_book)) {
        return memory->adpcm_book;
    }
    if (address == ADPCM_LOOP_ADDRESS
            && size <= sizeof(memory->adpcm_loop)) {
        return memory->adpcm_loop;
    }
    if (address == ADPCM_STATE_ADDRESS
            && size <= sizeof(memory->adpcm_state)) {
        return memory->adpcm_state;
    }
    if (address == RESAMPLE_STATE_ADDRESS
            && size <= sizeof(memory->resample_state)) {
        return memory->resample_state;
    }
    if (address == ENVMIX_STATE_ADDRESS
            && size <= sizeof(memory->envmix_state)) {
        return memory->envmix_state;
    }
    if (address == POLEF_STATE_ADDRESS
            && size <= sizeof(memory->polef_state)) {
        return memory->polef_state;
    }
    return NULL;
}

typedef struct DirectAddressProbe {
    uint32_t seen;
    uint8_t bytes[8];
} DirectAddressProbe;

static void *resolve_direct_address(void *context, uint32_t address, size_t size)
{
    DirectAddressProbe *probe = context;
    probe->seen = address;
    return size <= sizeof(probe->bytes) ? probe->bytes : NULL;
}

static void test_native_direct_address_mode(void)
{
    GeAudioAbiState abi;
    DirectAddressProbe probe = {0};
    const GeAudioAbiCommand commands[] = {
        {A_SETBUFF << 24U, 4U},
        {A_LOADBUFF << 24U, UINT32_C(0x08001000)},
    };
    ge_audio_abi_init(&abi);
    abi.direct_addresses = 1U;
    assert(ge_audio_abi_execute(&abi, commands, 2U,
            resolve_direct_address, &probe) == GE_AUDIO_ABI_OK);
    assert(probe.seen == UINT32_C(0x08001000));
}

static Acmd *test_source_pull(
        void *filter,
        s16 *outp,
        s32 out_count,
        s32 sample_offset,
        Acmd *commands)
{
    Acmd *next = commands;

    (void)filter;
    (void)outp;
    (void)sample_offset;
    aSetBuffer(next++, 0, AL_AUX_L_OUT, 0, out_count << 1);
    aLoadBuffer(next++, LEFT_ADDRESS);
    aSetBuffer(next++, 0, AL_AUX_R_OUT, 0, out_count << 1);
    aLoadBuffer(next++, RIGHT_ADDRESS);
    return next;
}

static int16_t expected_q15(int16_t value)
{
    int32_t product = (int32_t)value * 0x7fff;

    return (int16_t)(product >> 15);
}

static void test_original_bus_save_chain_to_pcm_ring(void)
{
    static const int16_t left_samples[TEST_FRAMES] = {
        1000, -2000, 30000, -30000
    };
    static const int16_t right_samples[TEST_FRAMES] = {
        -1000, 2000, 10000, -10000
    };
    TestMemory memory = {0};
    TestSource source = {0};
    ALAuxBus aux = {0};
    ALMainBus main_bus = {0};
    ALSave save = {0};
    ALFilter *aux_sources[] = {&source.filter};
    ALFilter *main_sources[] = {&aux.filter};
    Acmd commands[32] = {0};
    Acmd *end;
    s16 output_pointer = 0;
    GeAudioAbiState abi;
    int16_t ring_storage[TEST_FRAMES * 2] = {0};
    int16_t consumed[TEST_FRAMES * 2] = {0};
    GeAudioOutput output;
    size_t frame;

    _Static_assert(sizeof(Acmd) == sizeof(GeAudioAbiCommand),
            "portable ABI command must match Acmd");

    for (frame = 0U; frame < TEST_FRAMES; ++frame) {
        store_be16(memory.left + frame * 2U, left_samples[frame]);
        store_be16(memory.right + frame * 2U, right_samples[frame]);
    }

    source.filter.handler = test_source_pull;
    aux.filter.handler = alAuxBusPull;
    aux.sources = aux_sources;
    aux.sourceCount = 1;
    main_bus.filter.handler = alMainBusPull;
    main_bus.sources = main_sources;
    main_bus.sourceCount = 1;
    save.filter.source = &main_bus.filter;
    save.dramout = OUTPUT_ADDRESS;

    end = alSavePull(&save, &output_pointer, TEST_FRAMES, 0, commands);
    assert((size_t)(end - commands) == 15U);
    assert((commands[0].words.w0 >> 24U) == A_CLEARBUFF);
    assert((commands[14].words.w0 >> 24U) == A_SAVEBUFF);

    ge_audio_abi_init(&abi);
    assert(ge_audio_output_init(
            &output, ring_storage, TEST_FRAMES, 22050U) == 0);
    assert(ge_audio_abi_execute_and_queue(
            &abi,
            (const GeAudioAbiCommand *)(const void *)commands,
            (size_t)(end - commands),
            resolve_test_address,
            &memory,
            OUTPUT_ADDRESS,
            TEST_FRAMES,
            &output) == GE_AUDIO_ABI_OK);
    assert(abi.commands_executed == 15U);
    assert(ge_audio_output_read(&output, consumed, TEST_FRAMES)
            == TEST_FRAMES);

    for (frame = 0U; frame < TEST_FRAMES; ++frame) {
        int16_t expected_left = expected_q15(left_samples[frame]);
        int16_t expected_right = expected_q15(right_samples[frame]);

        assert(load_be16(memory.output + frame * 4U) == expected_left);
        assert(load_be16(memory.output + frame * 4U + 2U) == expected_right);
        assert(consumed[frame * 2U] == expected_left);
        assert(consumed[frame * 2U + 1U] == expected_right);
    }
}

static void test_explicit_rsp_frontier(void)
{
    GeAudioAbiState abi;
    const GeAudioAbiCommand unknown = {16U << 24U, 0U};
    const GeAudioAbiCommand bad_clear = {
        A_CLEARBUFF << 24U | (GE_AUDIO_ABI_DMEM_BYTES - 2U),
        8U
    };

    ge_audio_abi_init(&abi);
    assert(ge_audio_abi_execute(
            &abi, &unknown, 1U, NULL, NULL)
            == GE_AUDIO_ABI_UNSUPPORTED_COMMAND);
    assert(abi.unsupported_opcode == 16U);
    assert(abi.commands_executed == 0U);

    ge_audio_abi_init(&abi);
    assert(ge_audio_abi_execute(
            &abi, &bad_clear, 1U, NULL, NULL)
            == GE_AUDIO_ABI_DMEM_RANGE);
}

static int16_t mix_sample(int16_t current, int16_t source, int16_t gain)
{
    int32_t mixed = (int32_t)current
            + (((int32_t)source * gain) >> 15);

    if (mixed > INT16_MAX) return INT16_MAX;
    if (mixed < INT16_MIN) return INT16_MIN;
    return (int16_t)mixed;
}

static int16_t envelope_gain(int16_t volume, int16_t amount)
{
    int32_t gain = ((int32_t)volume * amount + 0x4000) >> 15;

    if (gain > INT16_MAX) return INT16_MAX;
    if (gain < INT16_MIN) return INT16_MIN;
    return (int16_t)gain;
}

static void store_samples(
        uint8_t *destination,
        const int16_t *samples,
        size_t count);

static void test_goldeneye_envmixer_init_and_continue(void)
{
    enum {
        INPUT = 0x100,
        DRY_LEFT = 0x200,
        DRY_RIGHT = 0x300,
        WET_LEFT = 0x400,
        WET_RIGHT = 0x500,
        SAMPLE_COUNT = 4
    };
    static const int16_t input[SAMPLE_COUNT] = {
        1000, -2000, 30000, -30000
    };
    TestMemory memory = {0};
    GeAudioAbiState abi;
    const GeAudioAbiCommand initialize[] = {
        {A_SETBUFF << 24U | A_MAIN << 16U | INPUT,
                DRY_LEFT << 16U | SAMPLE_COUNT * 2U},
        {A_SETBUFF << 24U | A_AUX << 16U | DRY_RIGHT,
                WET_LEFT << 16U | WET_RIGHT},
        {A_SETVOL << 24U | (A_LEFT | A_VOL) << 16U | 0x4000U, 0U},
        {A_SETVOL << 24U | (A_RIGHT | A_VOL) << 16U | 0x2000U, 0U},
        {A_SETVOL << 24U | (A_LEFT | A_RATE) << 16U | 0x4000U, 0U},
        {A_SETVOL << 24U | (A_RIGHT | A_RATE) << 16U | 0x2000U, 0U},
        {A_SETVOL << 24U | A_AUX << 16U | 0x7fffU, 0x4000U},
        {A_ENVMIXER << 24U | (A_INIT | A_AUX) << 16U,
                ENVMIX_STATE_ADDRESS}
    };
    const GeAudioAbiCommand continue_mix = {
        A_ENVMIXER << 24U | A_CONTINUE << 16U,
        ENVMIX_STATE_ADDRESS
    };
    const int16_t gains[4] = {
        envelope_gain(0x4000, 0x7fff),
        envelope_gain(0x2000, 0x7fff),
        envelope_gain(0x4000, 0x4000),
        envelope_gain(0x2000, 0x4000)
    };
    const uint16_t outputs[4] = {
        DRY_LEFT, DRY_RIGHT, WET_LEFT, WET_RIGHT
    };
    size_t sample;
    size_t output;

    ge_audio_abi_init(&abi);
    store_samples(abi.dmem + INPUT, input, SAMPLE_COUNT);
    assert(ge_audio_abi_execute(
            &abi, initialize, sizeof(initialize) / sizeof(initialize[0]),
            resolve_test_address, &memory) == GE_AUDIO_ABI_OK);
    assert(abi.commands_executed
            == sizeof(initialize) / sizeof(initialize[0]));
    for (sample = 0U; sample < SAMPLE_COUNT; ++sample) {
        for (output = 0U; output < 4U; ++output) {
            assert(load_be16(abi.dmem + outputs[output] + sample * 2U)
                    == mix_sample(0, input[sample], gains[output]));
        }
    }

    /* Continuation must restore its saved envelope rather than the command
     * register values, and A_NOAUX must leave wet buffers unchanged. */
    fill_bytes(abi.dmem + DRY_LEFT, 0, SAMPLE_COUNT * 2U);
    fill_bytes(abi.dmem + DRY_RIGHT, 0, SAMPLE_COUNT * 2U);
    store_samples(abi.dmem + INPUT, input, SAMPLE_COUNT);
    abi.envelope_dry = 0;
    abi.envelope_volume[0] = 0;
    abi.envelope_volume[1] = 0;
    assert(ge_audio_abi_execute(&abi, &continue_mix, 1U,
            resolve_test_address, &memory) == GE_AUDIO_ABI_OK);
    for (sample = 0U; sample < SAMPLE_COUNT; ++sample) {
        assert(load_be16(abi.dmem + DRY_LEFT + sample * 2U)
                == mix_sample(0, input[sample], gains[0]));
        assert(load_be16(abi.dmem + DRY_RIGHT + sample * 2U)
                == mix_sample(0, input[sample], gains[1]));
    }
}

static void test_polef_identity_and_saved_history(void)
{
    enum {
        INPUT = 0x600,
        OUTPUT = 0x700,
        SAMPLE_COUNT = 8
    };
    static const int16_t input[SAMPLE_COUNT] = {
        1, 2, 3, 4, 5, 6, 7, 8
    };
    static const int16_t zeros[SAMPLE_COUNT] = {0};
    TestMemory memory = {0};
    GeAudioAbiState abi;
    const GeAudioAbiCommand initialize[] = {
        {A_LOADADPCM << 24U | 32U, ADPCM_BOOK_ADDRESS},
        {A_SETBUFF << 24U | INPUT,
                OUTPUT << 16U | SAMPLE_COUNT * 2U},
        {A_POLEF << 24U | A_INIT << 16U | 0x4000U,
                POLEF_STATE_ADDRESS}
    };
    const GeAudioAbiCommand continue_filter = {
        A_POLEF << 24U, POLEF_STATE_ADDRESS
    };
    size_t sample;

    /* h1[0] feeds the prior frame's penultimate output into sample zero.
     * The first initialized frame has zero history and is otherwise unity. */
    store_be16(memory.adpcm_book + 0U, 0x4000);
    ge_audio_abi_init(&abi);
    store_samples(abi.dmem + INPUT, input, SAMPLE_COUNT);
    assert(ge_audio_abi_execute(&abi, initialize,
            sizeof(initialize) / sizeof(initialize[0]),
            resolve_test_address, &memory) == GE_AUDIO_ABI_OK);
    for (sample = 0U; sample < SAMPLE_COUNT; ++sample) {
        assert(load_be16(abi.dmem + OUTPUT + sample * 2U) == input[sample]);
    }
    assert(load_be16(memory.polef_state + 4U) == 7);
    assert(load_be16(memory.polef_state + 6U) == 8);

    store_samples(abi.dmem + INPUT, zeros, SAMPLE_COUNT);
    abi.count_bytes = SAMPLE_COUNT * 2U;
    assert(ge_audio_abi_execute(&abi, &continue_filter, 1U,
            resolve_test_address, &memory) == GE_AUDIO_ABI_OK);
    assert(load_be16(abi.dmem + OUTPUT) == 7);
    for (sample = 1U; sample < SAMPLE_COUNT; ++sample) {
        assert(load_be16(abi.dmem + OUTPUT + sample * 2U) == 0);
    }
}

static void store_samples(
        uint8_t *destination,
        const int16_t *samples,
        size_t count)
{
    size_t sample;

    for (sample = 0U; sample < count; ++sample) {
        store_be16(destination + sample * sizeof(int16_t), samples[sample]);
    }
}

static void test_resample_init_golden_vector(void)
{
    static const int16_t input[16] = {
        1000, -2000, 4000, -8000, 12000, -16000, 20000, -24000,
        28000, -30000, 31000, -32000, 15000, -7000, 3000, 0
    };
    static const int16_t expected[8] = {
        0, -2, 105, 590, -1086, 2175, -4775, 7182
    };
    enum {
        RESAMPLE_INPUT = 0x500,
        RESAMPLE_OUTPUT = 0x700
    };
    TestMemory memory = {0};
    GeAudioAbiState abi;
    const GeAudioAbiCommand commands[] = {
        {A_SEGMENT << 24U, 2U << 24U | 0x6000U},
        {A_SETBUFF << 24U | RESAMPLE_INPUT,
                RESAMPLE_OUTPUT << 16U | 16U},
        {A_RESAMPLE << 24U | A_INIT << 16U | 0x8000U,
                2U << 24U | 0x1000U}
    };
    size_t sample;

    ge_audio_abi_init(&abi);
    store_samples(abi.dmem + RESAMPLE_INPUT, input,
            sizeof(input) / sizeof(input[0]));
    fill_bytes(memory.resample_state, 0x5a,
            sizeof(memory.resample_state));

    assert(ge_audio_abi_execute(
            &abi,
            commands,
            sizeof(commands) / sizeof(commands[0]),
            resolve_test_address,
            &memory) == GE_AUDIO_ABI_OK);
    assert(abi.commands_executed == 3U);
    for (sample = 0U; sample < 8U; ++sample) {
        assert(load_be16(abi.dmem + RESAMPLE_OUTPUT
                + sample * sizeof(int16_t)) == expected[sample]);
    }
    assert(load_be16(memory.resample_state + 0U) == 12000);
    assert(load_be16(memory.resample_state + 2U) == -16000);
    assert(load_be16(memory.resample_state + 4U) == 20000);
    assert(load_be16(memory.resample_state + 6U) == -24000);
    assert((uint16_t)load_be16(memory.resample_state + 8U) == 0U);
    for (sample = 10U; sample < sizeof(memory.resample_state); ++sample) {
        assert(memory.resample_state[sample] == 0x5aU);
    }
}

static void test_resample_continue_golden_vector(void)
{
    static const int16_t input[16] = {
        1000, -2000, 4000, -8000, 12000, -16000, 20000, -24000,
        28000, -30000, 31000, -32000, 15000, -7000, 3000, 0
    };
    static const int16_t initial_history[4] = {
        1000, -2000, 3000, -4000
    };
    static const int16_t expected[8] = {
        577, 984, -2817, -397, -532, -236, 2175, -4538
    };
    enum {
        RESAMPLE_INPUT = 0x800,
        RESAMPLE_OUTPUT = 0xa00
    };
    TestMemory memory = {0};
    GeAudioAbiState abi;
    const GeAudioAbiCommand commands[] = {
        {A_SETBUFF << 24U | RESAMPLE_INPUT,
                RESAMPLE_OUTPUT << 16U | 16U},
        {A_RESAMPLE << 24U | 0x6000U, RESAMPLE_STATE_ADDRESS}
    };
    size_t sample;

    ge_audio_abi_init(&abi);
    store_samples(abi.dmem + RESAMPLE_INPUT, input,
            sizeof(input) / sizeof(input[0]));
    store_samples(memory.resample_state, initial_history,
            sizeof(initial_history) / sizeof(initial_history[0]));
    store_be16(memory.resample_state + 8U, (int16_t)0x8000U);
    fill_bytes(memory.resample_state + 10U, 0x5a,
            sizeof(memory.resample_state) - 10U);

    assert(ge_audio_abi_execute(
            &abi,
            commands,
            sizeof(commands) / sizeof(commands[0]),
            resolve_test_address,
            &memory) == GE_AUDIO_ABI_OK);
    for (sample = 0U; sample < 8U; ++sample) {
        assert(load_be16(abi.dmem + RESAMPLE_OUTPUT
                + sample * sizeof(int16_t)) == expected[sample]);
    }
    assert(load_be16(memory.resample_state + 0U) == 4000);
    assert(load_be16(memory.resample_state + 2U) == -8000);
    assert(load_be16(memory.resample_state + 4U) == 12000);
    assert(load_be16(memory.resample_state + 6U) == -16000);
    assert((uint16_t)load_be16(memory.resample_state + 8U) == 0x8000U);
    for (sample = 10U; sample < sizeof(memory.resample_state); ++sample) {
        assert(memory.resample_state[sample] == 0x5aU);
    }
}

static void test_resample_bounds_and_atomic_errors(void)
{
    enum {
        RESAMPLE_INPUT = 0x200,
        RESAMPLE_OUTPUT = 0x400
    };
    TestMemory memory = {0};
    GeAudioAbiState abi;
    GeAudioAbiCommand commands[] = {
        {A_SETBUFF << 24U | RESAMPLE_INPUT,
                RESAMPLE_OUTPUT << 16U | 16U},
        {A_RESAMPLE << 24U | 0x8000U, RESAMPLE_STATE_ADDRESS}
    };
    uint8_t saved_state[32];
    size_t byte;

    fill_bytes(memory.resample_state, 0x5a,
            sizeof(memory.resample_state));
    copy_bytes(saved_state, memory.resample_state, sizeof(saved_state));

    ge_audio_abi_init(&abi);
    fill_bytes(abi.dmem + RESAMPLE_OUTPUT, 0x5a, 16U);
    commands[0].word0 = A_SETBUFF << 24U
            | (GE_AUDIO_ABI_DMEM_BYTES - 6U);
    assert(ge_audio_abi_execute(
            &abi, commands, 2U, resolve_test_address, &memory)
            == GE_AUDIO_ABI_DMEM_RANGE);
    for (byte = 0U; byte < 16U; ++byte) {
        assert(abi.dmem[RESAMPLE_OUTPUT + byte] == 0x5aU);
    }
    for (byte = 0U; byte < sizeof(saved_state); ++byte) {
        assert(memory.resample_state[byte] == saved_state[byte]);
    }

    ge_audio_abi_init(&abi);
    commands[0].word0 = A_SETBUFF << 24U | RESAMPLE_INPUT;
    commands[0].word1 = (GE_AUDIO_ABI_DMEM_BYTES - 8U) << 16U | 16U;
    assert(ge_audio_abi_execute(
            &abi, commands, 2U, resolve_test_address, &memory)
            == GE_AUDIO_ABI_DMEM_RANGE);

    ge_audio_abi_init(&abi);
    commands[0].word1 = RESAMPLE_OUTPUT << 16U | 16U;
    commands[1].word1 = 0x8000U;
    fill_bytes(abi.dmem + RESAMPLE_OUTPUT, 0x5a, 16U);
    assert(ge_audio_abi_execute(
            &abi, commands, 2U, resolve_test_address, &memory)
            == GE_AUDIO_ABI_ADDRESS_UNMAPPED);
    for (byte = 0U; byte < 16U; ++byte) {
        assert(abi.dmem[RESAMPLE_OUTPUT + byte] == 0x5aU);
    }
}

static void test_adpcm_loop_golden_vector(void)
{
    static const int16_t expected[16] = {
        101, 2, 2, 2, 2, 2, 2, 2,
        3, 2, 2, 2, 2, 2, 2, 2
    };
    enum {
        ADPCM_INPUT = 0x100,
        ADPCM_OUTPUT = 0x200
    };
    TestMemory memory = {0};
    GeAudioAbiState abi;
    const GeAudioAbiCommand commands[] = {
        {A_LOADADPCM << 24U | 32U, ADPCM_BOOK_ADDRESS},
        {A_SETLOOP << 24U, ADPCM_LOOP_ADDRESS},
        {A_SETBUFF << 24U | ADPCM_INPUT,
                ADPCM_OUTPUT << 16U | 32U},
        {A_ADPCM << 24U | A_LOOP << 16U, ADPCM_STATE_ADDRESS}
    };
    size_t sample;

    /* predictor[0].book2[0] = 1.0 in Q11 */
    store_be16(memory.adpcm_book + 8U * sizeof(int16_t), 2048);
    store_be16(memory.adpcm_loop + 15U * sizeof(int16_t), 100);

    ge_audio_abi_init(&abi);
    abi.dmem[ADPCM_INPUT] = 0x00U;
    for (sample = 0U; sample < 8U; ++sample) {
        abi.dmem[ADPCM_INPUT + 1U + sample] = 0x11U;
    }

    assert(ge_audio_abi_execute(
            &abi,
            commands,
            sizeof(commands) / sizeof(commands[0]),
            resolve_test_address,
            &memory) == GE_AUDIO_ABI_OK);
    assert(abi.commands_executed == 4U);
    assert(abi.adpcm_codebook_bytes == 32U);
    assert(abi.adpcm_loop_address == ADPCM_LOOP_ADDRESS);

    for (sample = 0U; sample < 15U; ++sample) {
        assert(load_be16(abi.dmem + ADPCM_OUTPUT
                + sample * sizeof(int16_t)) == 0);
    }
    assert(load_be16(abi.dmem + ADPCM_OUTPUT
            + 15U * sizeof(int16_t)) == 100);
    for (sample = 0U; sample < 16U; ++sample) {
        assert(load_be16(abi.dmem + ADPCM_OUTPUT + 32U
                + sample * sizeof(int16_t)) == expected[sample]);
        assert(load_be16(memory.adpcm_state
                + sample * sizeof(int16_t)) == expected[sample]);
    }
}

static void test_adpcm_init_scale_and_sign(void)
{
    static const int16_t expected[16] = {
        4, -4, 8, -8, 12, -12, 16, -16,
        20, -20, 24, -24, 28, -28, -32, 0
    };
    static const uint8_t packed[8] = {
        0x1f, 0x2e, 0x3d, 0x4c, 0x5b, 0x6a, 0x79, 0x80
    };
    enum {
        ADPCM_INPUT = 0x300,
        ADPCM_OUTPUT = 0x400
    };
    TestMemory memory = {0};
    GeAudioAbiState abi;
    const GeAudioAbiCommand commands[] = {
        {A_LOADADPCM << 24U | 32U, ADPCM_BOOK_ADDRESS},
        {A_SETBUFF << 24U | ADPCM_INPUT,
                ADPCM_OUTPUT << 16U | 32U},
        {A_ADPCM << 24U | A_INIT << 16U, ADPCM_STATE_ADDRESS}
    };
    size_t sample;

    ge_audio_abi_init(&abi);
    abi.dmem[ADPCM_INPUT] = 0x20U;
    copy_bytes(abi.dmem + ADPCM_INPUT + 1U, packed, sizeof(packed));

    assert(ge_audio_abi_execute(
            &abi,
            commands,
            sizeof(commands) / sizeof(commands[0]),
            resolve_test_address,
            &memory) == GE_AUDIO_ABI_OK);
    for (sample = 0U; sample < 16U; ++sample) {
        assert(load_be16(abi.dmem + ADPCM_OUTPUT
                + sample * sizeof(int16_t)) == 0);
        assert(load_be16(abi.dmem + ADPCM_OUTPUT + 32U
                + sample * sizeof(int16_t)) == expected[sample]);
        assert(load_be16(memory.adpcm_state
                + sample * sizeof(int16_t)) == expected[sample]);
    }
}

static void test_adpcm_bounds_and_atomic_errors(void)
{
    enum {
        ADPCM_INPUT = 0x100,
        ADPCM_OUTPUT = 0x200
    };
    TestMemory memory = {0};
    GeAudioAbiState abi;
    GeAudioAbiCommand commands[] = {
        {A_LOADADPCM << 24U | 32U, ADPCM_BOOK_ADDRESS},
        {A_SETBUFF << 24U | ADPCM_INPUT,
                ADPCM_OUTPUT << 16U | 32U},
        {A_ADPCM << 24U | A_INIT << 16U, ADPCM_STATE_ADDRESS}
    };
    const GeAudioAbiCommand oversized_book = {
        A_LOADADPCM << 24U | 264U,
        ADPCM_BOOK_ADDRESS
    };
    size_t byte;

    ge_audio_abi_init(&abi);
    assert(ge_audio_abi_execute(
            &abi, &oversized_book, 1U, resolve_test_address, &memory)
            == GE_AUDIO_ABI_CODEBOOK_RANGE);
    assert(abi.commands_executed == 0U);

    ge_audio_abi_init(&abi);
    fill_bytes(abi.dmem + ADPCM_OUTPUT, 0x5a, 64U);
    fill_bytes(memory.adpcm_state, 0x5a, sizeof(memory.adpcm_state));
    abi.dmem[ADPCM_INPUT] = 0x01U;
    assert(ge_audio_abi_execute(
            &abi, commands, 3U, resolve_test_address, &memory)
            == GE_AUDIO_ABI_CODEBOOK_RANGE);
    assert(abi.commands_executed == 2U);
    for (byte = 0U; byte < 64U; ++byte) {
        assert(abi.dmem[ADPCM_OUTPUT + byte] == 0x5aU);
    }
    for (byte = 0U; byte < sizeof(memory.adpcm_state); ++byte) {
        assert(memory.adpcm_state[byte] == 0x5aU);
    }

    ge_audio_abi_init(&abi);
    commands[1].word0 = A_SETBUFF << 24U
            | (GE_AUDIO_ABI_DMEM_BYTES - 4U);
    assert(ge_audio_abi_execute(
            &abi, commands, 3U, resolve_test_address, &memory)
            == GE_AUDIO_ABI_DMEM_RANGE);

    ge_audio_abi_init(&abi);
    commands[1].word0 = A_SETBUFF << 24U | ADPCM_INPUT;
    commands[1].word1 = (GE_AUDIO_ABI_DMEM_BYTES - 16U) << 16U | 32U;
    assert(ge_audio_abi_execute(
            &abi, commands, 3U, resolve_test_address, &memory)
            == GE_AUDIO_ABI_DMEM_RANGE);

    ge_audio_abi_init(&abi);
    commands[1].word1 = ADPCM_OUTPUT << 16U | 32U;
    commands[2].word1 = 0x9000U;
    fill_bytes(abi.dmem + ADPCM_OUTPUT, 0x5a, 64U);
    assert(ge_audio_abi_execute(
            &abi, commands, 3U, resolve_test_address, &memory)
            == GE_AUDIO_ABI_ADDRESS_UNMAPPED);
    for (byte = 0U; byte < 64U; ++byte) {
        assert(abi.dmem[ADPCM_OUTPUT + byte] == 0x5aU);
    }
}

int main(void)
{
    test_native_direct_address_mode();
    test_original_bus_save_chain_to_pcm_ring();
    test_adpcm_loop_golden_vector();
    test_adpcm_init_scale_and_sign();
    test_adpcm_bounds_and_atomic_errors();
    test_resample_init_golden_vector();
    test_resample_continue_golden_vector();
    test_resample_bounds_and_atomic_errors();
    test_goldeneye_envmixer_init_and_continue();
    test_polef_identity_and_saved_history();
    test_explicit_rsp_frontier();
    puts("original libaudio producer, ADPCM/resample/envmixer, and PCM ring pass");
    return 0;
}
