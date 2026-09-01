#include "ge_original_music_bank.h"

#include <stdlib.h>
#include <string.h>

typedef enum GeMusicObjectType {
    GE_MUSIC_OBJECT_BANK,
    GE_MUSIC_OBJECT_INSTRUMENT,
    GE_MUSIC_OBJECT_SOUND,
    GE_MUSIC_OBJECT_ENVELOPE,
    GE_MUSIC_OBJECT_KEYMAP,
    GE_MUSIC_OBJECT_WAVETABLE,
    GE_MUSIC_OBJECT_ADPCM_BOOK,
    GE_MUSIC_OBJECT_ADPCM_LOOP,
    GE_MUSIC_OBJECT_RAW_LOOP
} GeMusicObjectType;

typedef struct GeMusicAllocation {
    void *pointer;
    struct GeMusicAllocation *next;
} GeMusicAllocation;

typedef struct GeMusicRelocation {
    GeMusicObjectType type;
    uint32_t offset;
    void *pointer;
    struct GeMusicRelocation *next;
} GeMusicRelocation;

struct GeOriginalMusicBank {
    const uint8_t *ctl;
    size_t ctl_size;
    const uint8_t *tbl;
    size_t tbl_size;
    ALBank *native;
    GeMusicAllocation *allocations;
    GeMusicRelocation *relocations;
    GeOriginalMusicBankStats stats;
    int failed;
};

static uint16_t read_be16(const uint8_t *bytes)
{
    return (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}

static uint32_t read_be32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16)
        | ((uint32_t)bytes[2] << 8) | bytes[3];
}

static int range_ok(const GeOriginalMusicBank *bank, uint32_t offset,
        size_t length)
{
    return offset <= bank->ctl_size && length <= bank->ctl_size - offset;
}

static void *bank_allocate(GeOriginalMusicBank *bank, size_t size)
{
    GeMusicAllocation *allocation;
    void *pointer;
    if (size == 0U || bank->failed) return NULL;
    pointer = calloc(1U, size);
    allocation = malloc(sizeof(*allocation));
    if (pointer == NULL || allocation == NULL) {
        free(pointer);
        free(allocation);
        bank->failed = 1;
        return NULL;
    }
    allocation->pointer = pointer;
    allocation->next = bank->allocations;
    bank->allocations = allocation;
    return pointer;
}

static void *find_relocation(GeOriginalMusicBank *bank,
        GeMusicObjectType type, uint32_t offset)
{
    GeMusicRelocation *entry;
    for (entry = bank->relocations; entry != NULL; entry = entry->next) {
        if (entry->type == type && entry->offset == offset) {
            return entry->pointer;
        }
    }
    return NULL;
}

static int add_relocation(GeOriginalMusicBank *bank,
        GeMusicObjectType type, uint32_t offset, void *pointer)
{
    GeMusicRelocation *entry = malloc(sizeof(*entry));
    if (entry == NULL) {
        bank->failed = 1;
        return 0;
    }
    entry->type = type;
    entry->offset = offset;
    entry->pointer = pointer;
    entry->next = bank->relocations;
    bank->relocations = entry;
    return 1;
}

static ALEnvelope *load_envelope(GeOriginalMusicBank *bank, uint32_t offset)
{
    ALEnvelope *out;
    if (offset == 0U) return NULL;
    out = find_relocation(bank, GE_MUSIC_OBJECT_ENVELOPE, offset);
    if (out != NULL) return out;
    if (!range_ok(bank, offset, 14U)) { bank->failed = 1; return NULL; }
    out = bank_allocate(bank, sizeof(*out));
    if (out == NULL || !add_relocation(bank, GE_MUSIC_OBJECT_ENVELOPE,
            offset, out)) return NULL;
    out->attackTime = (s32)read_be32(bank->ctl + offset);
    out->decayTime = (s32)read_be32(bank->ctl + offset + 4U);
    out->releaseTime = (s32)read_be32(bank->ctl + offset + 8U);
    out->attackVolume = bank->ctl[offset + 12U];
    out->decayVolume = bank->ctl[offset + 13U];
    return out;
}

static ALKeyMap *load_keymap(GeOriginalMusicBank *bank, uint32_t offset)
{
    ALKeyMap *out;
    if (offset == 0U) return NULL;
    out = find_relocation(bank, GE_MUSIC_OBJECT_KEYMAP, offset);
    if (out != NULL) return out;
    if (!range_ok(bank, offset, 6U)) { bank->failed = 1; return NULL; }
    out = bank_allocate(bank, sizeof(*out));
    if (out == NULL || !add_relocation(bank, GE_MUSIC_OBJECT_KEYMAP,
            offset, out)) return NULL;
    memcpy(out, bank->ctl + offset, 6U);
    return out;
}

static ALADPCMBook *load_book(GeOriginalMusicBank *bank, uint32_t offset)
{
    ALADPCMBook *out;
    uint32_t order, predictors, coefficients;
    size_t size;
    if (offset == 0U) return NULL;
    out = find_relocation(bank, GE_MUSIC_OBJECT_ADPCM_BOOK, offset);
    if (out != NULL) return out;
    if (!range_ok(bank, offset, 8U)) { bank->failed = 1; return NULL; }
    order = read_be32(bank->ctl + offset);
    predictors = read_be32(bank->ctl + offset + 4U);
    if (order == 0U || predictors == 0U || order > 16U
            || predictors > 256U
            || order > UINT32_MAX / predictors / 8U) {
        bank->failed = 1;
        return NULL;
    }
    coefficients = order * predictors * 8U;
    size = offsetof(ALADPCMBook, book) + coefficients * sizeof(s16);
    if (!range_ok(bank, offset, 8U + coefficients * 2U)) {
        bank->failed = 1;
        return NULL;
    }
    out = bank_allocate(bank, size);
    if (out == NULL || !add_relocation(bank, GE_MUSIC_OBJECT_ADPCM_BOOK,
            offset, out)) return NULL;
    out->order = (s32)order;
    out->npredictors = (s32)predictors;
    /* The CPU only consumes order/count.  The RSP command consumes the
     * coefficient byte stream, so retain its authored big-endian encoding
     * for the native ABI interpreter instead of host-endianizing it. */
    memcpy(out->book, bank->ctl + offset + 8U, coefficients * 2U);
    ++bank->stats.adpcm_books;
    return out;
}

static ALADPCMloop *load_adpcm_loop(GeOriginalMusicBank *bank,
        uint32_t offset)
{
    ALADPCMloop *out;
    if (offset == 0U) return NULL;
    out = find_relocation(bank, GE_MUSIC_OBJECT_ADPCM_LOOP, offset);
    if (out != NULL) return out;
    if (!range_ok(bank, offset, 44U)) { bank->failed = 1; return NULL; }
    out = bank_allocate(bank, sizeof(*out));
    if (out == NULL || !add_relocation(bank, GE_MUSIC_OBJECT_ADPCM_LOOP,
            offset, out)) return NULL;
    out->start = read_be32(bank->ctl + offset);
    out->end = read_be32(bank->ctl + offset + 4U);
    out->count = read_be32(bank->ctl + offset + 8U);
    /* Like the book, loop history is copied verbatim into the microcode
     * state buffer and must stay in authored byte order. */
    memcpy(out->state, bank->ctl + offset + 12U, 32U);
    ++bank->stats.adpcm_loops;
    return out;
}

static ALRawLoop *load_raw_loop(GeOriginalMusicBank *bank, uint32_t offset)
{
    ALRawLoop *out;
    if (offset == 0U) return NULL;
    out = find_relocation(bank, GE_MUSIC_OBJECT_RAW_LOOP, offset);
    if (out != NULL) return out;
    if (!range_ok(bank, offset, 12U)) { bank->failed = 1; return NULL; }
    out = bank_allocate(bank, sizeof(*out));
    if (out == NULL || !add_relocation(bank, GE_MUSIC_OBJECT_RAW_LOOP,
            offset, out)) return NULL;
    out->start = read_be32(bank->ctl + offset);
    out->end = read_be32(bank->ctl + offset + 4U);
    out->count = read_be32(bank->ctl + offset + 8U);
    ++bank->stats.raw_loops;
    return out;
}

static ALWaveTable *load_wavetable(GeOriginalMusicBank *bank,
        uint32_t offset)
{
    ALWaveTable *out;
    uint32_t base, length, loop, book;
    if (offset == 0U) return NULL;
    out = find_relocation(bank, GE_MUSIC_OBJECT_WAVETABLE, offset);
    if (out != NULL) return out;
    if (!range_ok(bank, offset, 20U)) { bank->failed = 1; return NULL; }
    base = read_be32(bank->ctl + offset);
    length = read_be32(bank->ctl + offset + 4U);
    if (base > bank->tbl_size || length > bank->tbl_size - base) {
        bank->failed = 1;
        return NULL;
    }
    out = bank_allocate(bank, sizeof(*out));
    if (out == NULL || !add_relocation(bank, GE_MUSIC_OBJECT_WAVETABLE,
            offset, out)) return NULL;
    out->base = (u8 *)(bank->tbl + base);
    out->len = (s32)length;
    out->type = bank->ctl[offset + 8U];
    out->flags = 1U;
    loop = read_be32(bank->ctl + offset + 12U);
    book = read_be32(bank->ctl + offset + 16U);
    if (out->type == AL_ADPCM_WAVE) {
        out->waveInfo.adpcmWave.loop = load_adpcm_loop(bank, loop);
        out->waveInfo.adpcmWave.book = load_book(bank, book);
        if (book != 0U && out->waveInfo.adpcmWave.book == NULL) return NULL;
    } else if (out->type == AL_RAW16_WAVE) {
        out->waveInfo.rawWave.loop = load_raw_loop(bank, loop);
    } else {
        bank->failed = 1;
        return NULL;
    }
    ++bank->stats.wavetables;
    return out;
}

static ALSound *load_sound(GeOriginalMusicBank *bank, uint32_t offset)
{
    ALSound *out;
    uint32_t envelope, keymap, wavetable;
    if (offset == 0U) return NULL;
    out = find_relocation(bank, GE_MUSIC_OBJECT_SOUND, offset);
    if (out != NULL) return out;
    if (!range_ok(bank, offset, 15U)) { bank->failed = 1; return NULL; }
    out = bank_allocate(bank, sizeof(*out));
    if (out == NULL || !add_relocation(bank, GE_MUSIC_OBJECT_SOUND,
            offset, out)) return NULL;
    envelope = read_be32(bank->ctl + offset);
    keymap = read_be32(bank->ctl + offset + 4U);
    wavetable = read_be32(bank->ctl + offset + 8U);
    out->envelope = load_envelope(bank, envelope);
    out->keyMap = load_keymap(bank, keymap);
    out->wavetable = load_wavetable(bank, wavetable);
    out->samplePan = bank->ctl[offset + 12U];
    out->sampleVolume = bank->ctl[offset + 13U];
    out->flags = 1U;
    if (out->envelope == NULL || out->keyMap == NULL || out->wavetable == NULL)
        return NULL;
    ++bank->stats.sounds;
    return out;
}

static ALInstrument *load_instrument(GeOriginalMusicBank *bank,
        uint32_t offset)
{
    ALInstrument *out;
    uint16_t sound_count;
    uint32_t index;
    size_t size;
    if (offset == 0U) return NULL;
    out = find_relocation(bank, GE_MUSIC_OBJECT_INSTRUMENT, offset);
    if (out != NULL) return out;
    if (!range_ok(bank, offset, 16U)) { bank->failed = 1; return NULL; }
    sound_count = read_be16(bank->ctl + offset + 14U);
    if (!range_ok(bank, offset, 16U + (size_t)sound_count * 4U)) {
        bank->failed = 1;
        return NULL;
    }
    size = offsetof(ALInstrument, soundArray)
        + (size_t)sound_count * sizeof(ALSound *);
    out = bank_allocate(bank, size);
    if (out == NULL || !add_relocation(bank, GE_MUSIC_OBJECT_INSTRUMENT,
            offset, out)) return NULL;
    memcpy(out, bank->ctl + offset, 12U);
    out->bendRange = (s16)read_be16(bank->ctl + offset + 12U);
    out->soundCount = (s16)sound_count;
    out->flags = 1U;
    for (index = 0U; index < sound_count; ++index) {
        uint32_t sound = read_be32(bank->ctl + offset + 16U + index * 4U);
        out->soundArray[index] = load_sound(bank, sound);
        if (sound != 0U && out->soundArray[index] == NULL) return NULL;
    }
    ++bank->stats.instruments;
    return out;
}

static ALBank *load_bank(GeOriginalMusicBank *bank, uint32_t offset)
{
    ALBank *out;
    uint16_t instrument_count;
    uint32_t percussion, index;
    size_t size;
    if (offset == 0U) return NULL;
    out = find_relocation(bank, GE_MUSIC_OBJECT_BANK, offset);
    if (out != NULL) return out;
    if (!range_ok(bank, offset, 12U)) { bank->failed = 1; return NULL; }
    instrument_count = read_be16(bank->ctl + offset);
    if (!range_ok(bank, offset, 12U + (size_t)instrument_count * 4U)) {
        bank->failed = 1;
        return NULL;
    }
    size = offsetof(ALBank, instArray)
        + (size_t)instrument_count * sizeof(ALInstrument *);
    out = bank_allocate(bank, size);
    if (out == NULL || !add_relocation(bank, GE_MUSIC_OBJECT_BANK,
            offset, out)) return NULL;
    out->instCount = (s16)instrument_count;
    out->flags = 1U;
    out->sampleRate = (s32)read_be32(bank->ctl + offset + 4U);
    percussion = read_be32(bank->ctl + offset + 8U);
    out->percussion = load_instrument(bank, percussion);
    if (percussion != 0U && out->percussion == NULL) return NULL;
    for (index = 0U; index < instrument_count; ++index) {
        uint32_t instrument = read_be32(bank->ctl + offset + 12U
                + index * 4U);
        out->instArray[index] = load_instrument(bank, instrument);
        if (instrument != 0U && out->instArray[index] == NULL) return NULL;
    }
    ++bank->stats.banks;
    return out;
}

GeOriginalMusicBank *ge_original_music_bank_open(
        const uint8_t *ctl, size_t ctl_size,
        const uint8_t *tbl, size_t tbl_size,
        uint32_t bank_index)
{
    GeOriginalMusicBank *bank;
    uint16_t bank_count;
    uint32_t bank_offset;
    if (ctl == NULL || tbl == NULL || ctl_size < 8U
            || read_be16(ctl) != AL_BANK_VERSION) return NULL;
    bank_count = read_be16(ctl + 2U);
    if (bank_index >= bank_count
            || 4U + ((size_t)bank_index + 1U) * 4U > ctl_size) return NULL;
    bank = calloc(1U, sizeof(*bank));
    if (bank == NULL) return NULL;
    bank->ctl = ctl;
    bank->ctl_size = ctl_size;
    bank->tbl = tbl;
    bank->tbl_size = tbl_size;
    bank_offset = read_be32(ctl + 4U + bank_index * 4U);
    bank->native = load_bank(bank, bank_offset);
    if (bank->failed || bank->native == NULL) {
        ge_original_music_bank_close(bank);
        return NULL;
    }
    return bank;
}

void ge_original_music_bank_close(GeOriginalMusicBank *bank)
{
    GeMusicAllocation *allocation;
    GeMusicRelocation *relocation;
    if (bank == NULL) return;
    while ((allocation = bank->allocations) != NULL) {
        bank->allocations = allocation->next;
        free(allocation->pointer);
        free(allocation);
    }
    while ((relocation = bank->relocations) != NULL) {
        bank->relocations = relocation->next;
        free(relocation);
    }
    free(bank);
}

ALBank *ge_original_music_bank_native(GeOriginalMusicBank *bank)
{
    return bank == NULL ? NULL : bank->native;
}

void ge_original_music_bank_stats(const GeOriginalMusicBank *bank,
        GeOriginalMusicBankStats *stats)
{
    if (stats != NULL) {
        if (bank == NULL) memset(stats, 0, sizeof(*stats));
        else *stats = bank->stats;
    }
}
