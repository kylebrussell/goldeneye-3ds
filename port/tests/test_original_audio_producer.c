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

static uint64_t test_profile_clock(void *context)
{
    uint64_t *ticks = context;
    return ++*ticks;
}

static void test_original_bus_save_chain_to_pcm_ring(int profile)
{
    static const int16_t left_samples[TEST_FRAMES] = {
        1000, -2000, 30000, -30000
    };
    static const int16_t right_samples[TEST_FRAMES] = {
        -1000, 2000, 10000, -10000
    };
    uint64_t profile_clock = 0U;
    ge_audio_abi_profile_bind(profile ? test_profile_clock : NULL, &profile_clock);
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
    uint64_t ticks[16], calls[16], hash, bytes, expected_hash = UINT64_C(14695981039346656037);
    ge_audio_abi_profile_totals(ticks, calls);
    ge_audio_abi_profile_pcm(&hash, &bytes);
    uint64_t total_calls = 0U;
    for (size_t opcode = 0; opcode < 16; ++opcode) {
        assert(ticks[opcode] == calls[opcode]);
        total_calls += calls[opcode];
    }
    assert(total_calls == (profile ? 15U : 0U));
    assert(bytes == (profile ? sizeof(memory.output) : 0U));
    if (profile) for (size_t byte = 0; byte < sizeof(memory.output); ++byte)
        expected_hash = (expected_hash ^ memory.output[byte]) * UINT64_C(1099511628211);
    assert(hash == expected_hash);
    ge_audio_abi_profile_bind(NULL, NULL);

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

static int resample_reference_range(uint16_t input, int position)
{
    if (position < -4) return 0;
    return position < 0 || (uint32_t)input + (uint32_t)position * 2U + 2U
        <= GE_AUDIO_ABI_DMEM_BYTES;
}

static void test_resample_pitch_sweep(void)
{
    uint32_t pitch;
    uint32_t digest = UINT32_C(2166136261);
    size_t accepted = 0U, rejected = 0U;
    for (pitch = 0U; pitch <= UINT16_MAX; ++pitch) {
        TestMemory memory = {0};
        GeAudioAbiState abi;
        uint8_t before[GE_AUDIO_ABI_DMEM_BYTES];
        uint8_t history_before[32];
        const uint16_t count = (uint16_t)(pitch % 257U);
        const uint16_t initial_phase = (uint16_t)(pitch * 40503U);
        const int initialize = (pitch & 1U) != 0U;
        const uint16_t inputs[] = {0U, 1U, 0x200U, 3839U, 4094U,
            4095U, 4096U, UINT16_MAX};
        const GeAudioAbiCommand command = {
            A_RESAMPLE << 24U | (initialize ? A_INIT << 16U : 0U) | pitch,
            RESAMPLE_STATE_ADDRESS
        };
        const size_t samples = ((size_t)count + 15U) / 16U * 8U;
        uint32_t phase = initialize ? 0U : initial_phase;
        int position = -4;
        int valid = 1;
        size_t sample, tap, byte;

        ge_audio_abi_init(&abi);
        abi.dmem_input = inputs[(pitch / 257U) % 8U];
        abi.dmem_output = 0U;
        abi.count_bytes = count;
        for (byte = 0U; byte < sizeof(abi.dmem); ++byte)
            abi.dmem[byte] = (uint8_t)(byte * 13U + pitch);
        fill_bytes(memory.resample_state, 0x5a, sizeof(memory.resample_state));
        store_be16(memory.resample_state + 8U, (int16_t)initial_phase);
        copy_bytes(before, abi.dmem, sizeof(before));
        copy_bytes(history_before, memory.resample_state, sizeof(history_before));

        /* Independent scalar walk of every tap, followed by persisted
         * history. This deliberately does not use the optimized endpoint. */
        for (sample = 0U; sample < samples; ++sample) {
            for (tap = 0U; tap < 4U; ++tap)
                if (!resample_reference_range(abi.dmem_input,
                        position + (int)tap)) valid = 0;
            phase += pitch * 2U;
            position += (int)(phase >> 16U);
            phase &= 0xffffU;
        }
        for (tap = 0U; tap < 4U; ++tap)
            if (!resample_reference_range(abi.dmem_input,
                    position + (int)tap)) valid = 0;
        assert(ge_audio_abi_execute(&abi, &command, 1U,
            resolve_test_address, &memory)
            == (valid ? GE_AUDIO_ABI_OK : GE_AUDIO_ABI_DMEM_RANGE));
        if (!valid) {
            ++rejected;
            for (byte = 0U; byte < sizeof(before); ++byte)
                assert(before[byte] == abi.dmem[byte]);
            for (byte = 0U; byte < sizeof(history_before); ++byte)
                assert(history_before[byte] == memory.resample_state[byte]);
        } else {
            ++accepted;
            assert((uint16_t)load_be16(memory.resample_state + 8U) == phase);
        }
        for (byte = 0U; byte < sizeof(abi.dmem); ++byte)
            digest = (digest ^ abi.dmem[byte]) * UINT32_C(16777619);
        for (byte = 0U; byte < sizeof(memory.resample_state); ++byte)
            digest = (digest ^ memory.resample_state[byte]) * UINT32_C(16777619);
    }
    assert(accepted == 31898U && rejected == 33638U);
    /* Recorded from the prior scalar-preflight implementation. Covers PCM,
     * overlapping DMEM, continued history/phase and unchanged rejected data. */
    assert(digest == UINT32_C(0xabfbe11a));
}

/* Every coefficient row and every sign combination at full scale. Exercise
 * both history/DMEM crossing and the contiguous path, odd and overlapping
 * buffers, and extreme pitches. The digest comes from the int64 scalar
 * implementation, including persisted history and untouched DMEM bytes. */
static void test_resample_full_scale(void)
{
    uint32_t digest = UINT32_C(2166136261);
    const uint16_t pitches[] = {0U, 1U, 0x4000U, 0x7fffU, 0x8000U, 0xffffU};
    unsigned row, signs, variant;
    for (row = 0U; row < 64U; ++row) {
        for (signs = 0U; signs < 16U; ++signs) {
            for (variant = 0U; variant < 6U; ++variant) {
                TestMemory memory = {0};
                GeAudioAbiState abi;
                GeAudioAbiCommand command = {
                    A_RESAMPLE << 24U | pitches[variant],
                    RESAMPLE_STATE_ADDRESS
                };
                size_t sample, byte;
                ge_audio_abi_init(&abi);
                abi.dmem_input = 0x101U;
                abi.dmem_output = variant & 1U ? 0x101U : 0x800U;
                abi.count_bytes = 256U;
                for (sample = 0U; sample < 512U; ++sample) {
                    const int16_t value = signs & (1U << (sample % 4U))
                        ? INT16_MIN : INT16_MAX;
                    store_be16(abi.dmem + abi.dmem_input + sample * 2U, value);
                    if (sample < 4U)
                        store_be16(memory.resample_state + sample * 2U, value);
                }
                store_be16(memory.resample_state + 8U, (int16_t)(row << 10U));
                assert(ge_audio_abi_execute(&abi, &command, 1U,
                    resolve_test_address, &memory) == GE_AUDIO_ABI_OK);
                for (byte = 0U; byte < sizeof(abi.dmem); ++byte)
                    digest = (digest ^ abi.dmem[byte]) * UINT32_C(16777619);
                for (byte = 0U; byte < sizeof(memory.resample_state); ++byte)
                    digest = (digest ^ memory.resample_state[byte]) * UINT32_C(16777619);
            }
        }
    }
    assert(digest == UINT32_C(0x41fa0db5));
}

static uint32_t mixer_test_random(uint32_t *seed)
{
    *seed = *seed * UINT32_C(1664525) + UINT32_C(1013904223);
    return *seed;
}

static void test_envmixer_ramps_and_overlap(void)
{
    uint32_t digest = UINT32_C(2166136261), seed = 719U;
    const uint16_t offsets[] = {0U, 1U, 0x200U, 0x201U, 0x400U, 0x600U};
    unsigned trial;
    for (trial = 0U; trial < 4096U; ++trial) {
        GeAudioAbiState abi;
        TestMemory memory = {0};
        const GeAudioAbiCommand command = {
            A_ENVMIXER << 24U | ((trial & 1U ? A_INIT : 0U)
                | (trial & 2U ? A_AUX : 0U)) << 16U,
            ENVMIX_STATE_ADDRESS
        };
        size_t byte, channel;
        ge_audio_abi_init(&abi);
        for (byte = 0U; byte < sizeof(abi.dmem); ++byte)
            abi.dmem[byte] = (uint8_t)(mixer_test_random(&seed) >> 24U);
        for (byte = 0U; byte < sizeof(memory.envmix_state); ++byte)
            memory.envmix_state[byte] = (uint8_t)(mixer_test_random(&seed) >> 24U);
        abi.count_bytes = trial % 513U;
        abi.dmem_input = offsets[trial % 6U];
        abi.dmem_output = offsets[(trial / 6U) % 6U];
        abi.dmem_dry_right = offsets[(trial / 36U) % 6U];
        abi.dmem_wet_left = offsets[(trial / 216U) % 6U];
        abi.dmem_wet_right = offsets[(trial / 1296U) % 6U];
        abi.envelope_dry = (int16_t)(mixer_test_random(&seed) >> 16U);
        abi.envelope_wet = (int16_t)(mixer_test_random(&seed) >> 16U);
        for (channel = 0U; channel < 2U; ++channel) {
            abi.envelope_volume[channel] = (int16_t)(mixer_test_random(&seed) >> 16U);
            abi.envelope_target[channel] = (int16_t)(mixer_test_random(&seed) >> 16U);
            abi.envelope_rate[channel] = (int32_t)mixer_test_random(&seed);
            if (trial % 3U == 0U) {
                abi.envelope_rate[channel] = 0;
                fill_bytes(memory.envmix_state + 16U + channel * 4U, 0U, 4U);
            }
            if (trial % 5U == 0U)
                abi.envelope_target[channel] = abi.envelope_volume[channel];
        }
        assert(ge_audio_abi_execute(&abi, &command, 1U,
                resolve_test_address, &memory) == GE_AUDIO_ABI_OK);
        for (byte = 0U; byte < sizeof(abi.dmem); ++byte)
            digest = (digest ^ abi.dmem[byte]) * UINT32_C(16777619);
        for (byte = 0U; byte < sizeof(memory.envmix_state); ++byte)
            digest = (digest ^ memory.envmix_state[byte]) * UINT32_C(16777619);
    }
    /* Recorded with the previous per-sample scalar mixer at -O2. */
    assert(digest == UINT32_C(0x1f5e37d0));
}

static void test_adpcm_extreme_predictors(int bounded)
{
    uint32_t digest = UINT32_C(2166136261), seed = 819U;
    unsigned trial;
    for (trial = 0U; trial < 4096U; ++trial) {
        GeAudioAbiState abi;
        TestMemory memory = {0};
        const GeAudioAbiCommand command = {
            A_ADPCM << 24U | (trial & 1U ? A_INIT << 16U : 0U),
            ADPCM_STATE_ADDRESS
        };
        size_t sample, byte;
        ge_audio_abi_init(&abi);
        abi.dmem_input = 0x101U;
        abi.dmem_output = 0x501U;
        abi.count_bytes = 256U;
        abi.adpcm_codebook_bytes = sizeof(abi.adpcm_codebook);
        for (sample = 0U; sample < GE_AUDIO_ABI_ADPCM_BOOK_SAMPLES; ++sample)
            abi.adpcm_codebook[sample] = trial % 3U
                ? (int16_t)(mixer_test_random(&seed) >> 16U)
                : (sample & 1U ? INT16_MIN : INT16_MAX);
        if (bounded) {
            for (sample = 0U; sample < GE_AUDIO_ABI_ADPCM_BOOK_SAMPLES; ++sample)
                abi.adpcm_codebook[sample] = trial % 5U == 0U
                    ? (int16_t)((int32_t)(mixer_test_random(&seed) >> 19U) - 4096)
                    : 0;
            if (trial % 5U != 0U) {
                for (sample = 0U; sample < 8U; ++sample) {
                    /* Both signs, immediately on each side of the proven
                     * 63487 coefficient-sum boundary. Alternate predictor
                     * sizes within one command to exercise both paths. */
                    const int negative = (trial & 2U) != 0U;
                    abi.adpcm_codebook[sample * 16U + 7U] =
                        negative ? INT16_MIN : INT16_MAX;
                    abi.adpcm_codebook[sample * 16U + 15U] = negative
                        ? (int16_t)(-30719 - (int)(sample & 1U))
                        : (int16_t)(30720 + (sample & 1U));
                }
            }
        }
        for (byte = 0U; byte < sizeof(memory.adpcm_state); ++byte)
            memory.adpcm_state[byte] = (uint8_t)(mixer_test_random(&seed) >> 24U);
        for (byte = 0U; byte < 72U; ++byte)
            abi.dmem[abi.dmem_input + byte] = (uint8_t)(mixer_test_random(&seed) >> 24U);
        for (sample = 0U; sample < 8U; ++sample)
            abi.dmem[abi.dmem_input + sample * 9U] =
                (uint8_t)((trial % 16U) << 4U | sample);
        assert(ge_audio_abi_execute(&abi, &command, 1U,
                resolve_test_address, &memory) == GE_AUDIO_ABI_OK);
        for (byte = 0U; byte < sizeof(abi.dmem); ++byte)
            digest = (digest ^ abi.dmem[byte]) * UINT32_C(16777619);
        for (byte = 0U; byte < sizeof(memory.adpcm_state); ++byte)
            digest = (digest ^ memory.adpcm_state[byte]) * UINT32_C(16777619);
    }
    /* Recorded with the previous int64 floor/saturation implementation. */
    if (bounded) assert(digest == UINT32_C(0xb961c332));
    else assert(digest == UINT32_C(0xd2ddec30));
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
    test_original_bus_save_chain_to_pcm_ring(0);
    test_original_bus_save_chain_to_pcm_ring(1);
    test_adpcm_loop_golden_vector();
    test_adpcm_init_scale_and_sign();
    test_adpcm_bounds_and_atomic_errors();
    test_resample_init_golden_vector();
    test_resample_continue_golden_vector();
    test_resample_bounds_and_atomic_errors();
    test_resample_pitch_sweep();
    test_resample_full_scale();
    test_envmixer_ramps_and_overlap();
    test_adpcm_extreme_predictors(0);
    test_adpcm_extreme_predictors(1);
    test_goldeneye_envmixer_init_and_continue();
    test_polef_identity_and_saved_history();
    test_explicit_rsp_frontier();
    puts("original libaudio producer, ADPCM/resample/envmixer, and PCM ring pass");
    return 0;
}
