#include "ge_original_guard_animation_table.h"

#include <stdlib.h>
#include <string.h>

#include <ultra64.h>
#include <bondtypes.h>

#define GE_GUARD_ANIMATION_SEGMENT_SIZE 0xe7e0U
#define GE_GUARD_ANIMATION_CACHE_CAPACITY 192U

typedef struct GeGuardNativeAnimation {
    uint32_t offset;
    ModelAnimation animation;
    ModelAnimation *published;
    ModelAnimBitField *descriptors;
    uint8_t *frames;
    size_t frame_size;
    size_t handle_base;
} GeGuardNativeAnimation;

static const uint8_t *ge_animation_segment;
static size_t ge_animation_segment_size;
static const uint8_t *ge_animation_entries;
static size_t ge_animation_entries_size;
static GeGuardNativeAnimation ge_animation_cache[
    GE_GUARD_ANIMATION_CACHE_CAPACITY];
static size_t ge_animation_cache_count;
static size_t ge_animation_handle_extent;

#ifdef GE_PLATFORM_3DS
#include "game/initanitable.h"
_Static_assert(offsetof(ModelAnimation, address) == 0U,
               "native animation address must retain the N64 ABI");
_Static_assert(offsetof(ModelAnimation, bitDescriptors) == 8U,
               "native animation descriptors must retain the N64 ABI");
_Static_assert(offsetof(ModelAnimation, bitStream) == 16U,
               "native animation stream must retain the N64 ABI");
_Static_assert(offsetof(ModelAnimation, unk14) == 20U,
               "native animation authored header must remain 20 bytes");
static uint8_t ge_animation_native_table[0xffff]
    __attribute__((aligned(4)));
#endif

#define GE_WEAK __attribute__((weak))

/* Host tests which never stream a guard frame do not need to carry the large
 * authored entry bank.  The generated ABI supplies strong definitions in the
 * game and frame-stream tests; these weak fallbacks keep the adapter linkable
 * for the narrower model/gun tests. */
GE_WEAK size_t ge_port_embedded_animation_entries_size(void)
{
    return 0U;
}

GE_WEAK int ge_port_embedded_animation_entries_read(size_t offset, void *dst,
                                                     size_t size)
{
    (void)offset;
    (void)dst;
    (void)size;
    return 0;
}

static uint16_t read_be16(const uint8_t *bytes)
{
    return (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}

static uint32_t read_be32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16)
        | ((uint32_t)bytes[2] << 8) | bytes[3];
}

void ge_original_guard_animation_table_reset(void)
{
    size_t index;
    for (index = 0U; index < ge_animation_cache_count; index++) {
        free(ge_animation_cache[index].descriptors);
        free(ge_animation_cache[index].frames);
    }
    memset(ge_animation_cache, 0, sizeof(ge_animation_cache));
    ge_animation_cache_count = 0U;
    ge_animation_handle_extent = 0U;
    ge_animation_segment = NULL;
    ge_animation_segment_size = 0U;
    ge_animation_entries = NULL;
    ge_animation_entries_size = 0U;
#ifdef GE_PLATFORM_3DS
    ptr_animation_table = NULL;
#endif
}

int ge_original_guard_animation_table_bind(const uint8_t *segment,
                                            size_t segment_size)
{
    ge_original_guard_animation_table_reset();
    if (segment == NULL || segment_size != GE_GUARD_ANIMATION_SEGMENT_SIZE)
        return 0;
    ge_animation_segment = segment;
    ge_animation_segment_size = segment_size;
#ifdef GE_PLATFORM_3DS
    memset(ge_animation_native_table, 0, sizeof(ge_animation_native_table));
    ptr_animation_table = (struct animation_table_data *)(void *)
        ge_animation_native_table;
#endif
    return 1;
}

int ge_original_guard_animation_entries_bind(const uint8_t *entries,
                                               size_t entries_size)
{
    if (entries == NULL || entries_size == 0U)
        return 0;
    ge_animation_entries = entries;
    ge_animation_entries_size = entries_size;
    return 1;
}

static int read_entry_bytes(size_t offset, void *destination, size_t size)
{
    if (destination == NULL || offset > SIZE_MAX - size)
        return 0;
    if (ge_animation_entries != NULL) {
        if (offset + size > ge_animation_entries_size)
            return 0;
        memcpy(destination, ge_animation_entries + offset, size);
        return 1;
    }
    if (offset + size > ge_port_embedded_animation_entries_size())
        return 0;
    return ge_port_embedded_animation_entries_read(offset, destination, size);
}

static GeGuardNativeAnimation *find_native(const void *animation)
{
    size_t index;
    for (index = 0U; index < ge_animation_cache_count; index++) {
        if (animation == ge_animation_cache[index].published)
            return &ge_animation_cache[index];
    }
    return NULL;
}

void *ge_port_guard_animation_resolve(uint32_t record_offset)
{
    GeGuardNativeAnimation *native;
    const uint8_t *record;
    uint32_t descriptors_offset;
    uint32_t stream_offset;
    size_t descriptor_count;
    size_t index;

    for (index = 0U; index < ge_animation_cache_count; index++) {
        if (ge_animation_cache[index].offset == record_offset)
            return ge_animation_cache[index].published;
    }
    if (ge_animation_segment == NULL
            || record_offset + 20U > ge_animation_segment_size
            || ge_animation_cache_count >= GE_GUARD_ANIMATION_CACHE_CAPACITY)
        return NULL;
    record = ge_animation_segment + record_offset;
    descriptors_offset = read_be32(record + 8U);
    stream_offset = read_be32(record + 16U);
    if (stream_offset < descriptors_offset
            || stream_offset > ge_animation_segment_size
            || descriptors_offset > ge_animation_segment_size)
        return NULL;
    descriptor_count = (stream_offset - descriptors_offset) / 6U;
    if (descriptor_count == 0U)
        return NULL;
    native = &ge_animation_cache[ge_animation_cache_count];
    native->descriptors = calloc(descriptor_count,
                                 sizeof(*native->descriptors));
    if (native->descriptors == NULL)
        return NULL;
    for (index = 0U; index < descriptor_count; index++) {
        const uint8_t *source = ge_animation_segment + descriptors_offset
            + index * 6U;
        native->descriptors[index].bitOffset = read_be16(source);
        native->descriptors[index].bitCount = source[2];
        native->descriptors[index].padding = source[3];
        native->descriptors[index].valueOffset = read_be16(source + 4U);
    }
    native->offset = record_offset;
    native->animation.address = (s32)read_be32(record);
    native->animation.unk04 = read_be16(record + 4U);
    native->animation.unk06 = record[6];
    native->animation.unk07 = record[7];
    native->animation.bitDescriptors = native->descriptors;
    native->animation.unk0C = read_be16(record + 12U);
    native->animation.unk0E = read_be16(record + 14U);
    native->animation.bitStream = (u8 *)(uintptr_t)(
        ge_animation_segment + stream_offset);
#ifdef GE_PLATFORM_3DS
    /*
     * Canonical game code also forms animation pointers by adding an
     * ANIM_DATA_* offset directly to ptr_animation_table.  Publish the
     * decoded 20-byte authored header at that same offset so direct pointer
     * arithmetic, expanded tables and pointer equality all share one native
     * address.  The remaining ModelAnimation fields are decomp artefacts and
     * are not part of an authored table record.
     */
    native->published = (ModelAnimation *)(void *)(
        ge_animation_native_table + record_offset);
    native->published->address = native->animation.address;
    native->published->unk04 = native->animation.unk04;
    native->published->unk06 = native->animation.unk06;
    native->published->unk07 = native->animation.unk07;
    native->published->bitDescriptors = native->animation.bitDescriptors;
    native->published->unk0C = native->animation.unk0C;
    native->published->unk0E = native->animation.unk0E;
    native->published->bitStream = native->animation.bitStream;
#else
    native->published = &native->animation;
#endif
    native->frame_size = (size_t)(native->animation.unk0E >> 3);
    native->handle_base = ge_animation_handle_extent;
    if (native->frame_size != 0U
            && native->animation.unk04
                <= (SIZE_MAX - ge_animation_handle_extent)
                    / native->frame_size)
        ge_animation_handle_extent += native->frame_size
            * (size_t)native->animation.unk04;
    ge_animation_cache_count++;
    return native->published;
}

int ge_original_guard_animation_materialize_direct_entry(
    uintptr_t entry, uintptr_t native_table_base)
{
    uintptr_t record_offset;

    if (entry == 0U || entry == 1U)
        return 1;
    if (native_table_base != 0U && entry >= native_table_base
            && entry - native_table_base < ge_animation_segment_size) {
        record_offset = entry - native_table_base;
    } else if (entry < ge_animation_segment_size) {
        record_offset = entry;
    } else {
        return 0;
    }
    if (record_offset > UINT32_MAX)
        return 0;
    return ge_port_guard_animation_resolve((uint32_t)record_offset) != NULL;
}

int ge_port_guard_animation_owns(const void *animation)
{
    return find_native(animation) != NULL;
}

int32_t ge_port_guard_animation_load_frame(void *animation, int32_t frame)
{
    GeGuardNativeAnimation *native = find_native(animation);
    size_t frames_size;
    size_t frame_offset;

    if (native == NULL || frame < 0
            || (uint32_t)frame >= native->animation.unk04
            || native->frame_size == 0U)
        return 0;
    if (native->animation.unk04 > SIZE_MAX / native->frame_size)
        return 0;
    frames_size = (size_t)native->animation.unk04 * native->frame_size;
    if (native->frames == NULL) {
        native->frames = malloc(frames_size);
        if (native->frames == NULL
                || !read_entry_bytes((uint32_t)native->animation.address,
                                     native->frames, frames_size)) {
            free(native->frames);
            native->frames = NULL;
            return 0;
        }
    }
    frame_offset = (size_t)frame * native->frame_size;
    if (native->handle_base > (size_t)INT32_MAX - frame_offset - 1U)
        return 0;
    /* Negative handles cannot collide with the positive gait-frame handles. */
    return -(int32_t)(native->handle_base + frame_offset + 1U);
}

const uint8_t *ge_port_guard_animation_frame_data(int32_t handle)
{
    size_t index;
    size_t offset;
    if (handle >= 0)
        return NULL;
    offset = (size_t)(-(int64_t)handle - 1);
    for (index = 0U; index < ge_animation_cache_count; index++) {
        GeGuardNativeAnimation *native = &ge_animation_cache[index];
        size_t frames_size = native->frame_size
            * (size_t)native->animation.unk04;
        if (native->frames != NULL && offset >= native->handle_base
                && offset - native->handle_base < frames_size
                && (offset - native->handle_base) % native->frame_size == 0U)
            return native->frames + offset - native->handle_base;
    }
    return NULL;
}
