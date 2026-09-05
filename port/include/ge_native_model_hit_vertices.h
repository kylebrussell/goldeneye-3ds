#ifndef GE_NATIVE_MODEL_HIT_VERTICES_H
#define GE_NATIVE_MODEL_HIT_VERTICES_H

#include <stddef.h>
#include <stdint.h>

/* Display-list addresses use 16-byte N64 vertices. Native Vertex can be
 * larger, and a relocated array starts at its authored vertex block rather
 * than at the beginning of segment 5. Keep those address spaces separate. */
typedef struct GeNativeModelHitVertices {
    size_t count;
    uint32_t blob_offset;
    int blob_offset_known;
    ptrdiff_t base_index;
    int loaded;
} GeNativeModelHitVertices;

static inline int ge_native_model_hit_vertices_load(
    GeNativeModelHitVertices *range, uint32_t word0, uint32_t word1)
{
    const unsigned first = (word0 >> 16U) & 15U;
    const unsigned count = ((word0 >> 20U) & 15U) + 1U;
    const unsigned segment = word1 >> 24U;
    uint32_t offset = word1 & UINT32_C(0x00ffffff);
    range->loaded = 0;
    if ((word0 >> 24U) != 4U || first + count > 16U
            || (word0 & UINT32_C(0xffff)) != count * 16U) return 0;
    if (segment == 5U) {
        if (!range->blob_offset_known || offset < range->blob_offset) return 0;
        offset -= range->blob_offset;
    } else if (segment != 4U) {
        return 0;
    }
    if ((offset & 15U) != 0U || offset / 16U > range->count
            || count > range->count - offset / 16U) return 0;
    range->base_index = (ptrdiff_t)(offset / 16U) - (ptrdiff_t)first;
    range->loaded = 1;
    return 1;
}

static inline int ge_native_model_hit_vertex_index(
    const GeNativeModelHitVertices *range, unsigned slot, size_t *index)
{
    if (!range->loaded || slot >= 16U) return 0;
    const ptrdiff_t value = range->base_index + (ptrdiff_t)slot;
    if (value < 0 || (size_t)value >= range->count) return 0;
    *index = (size_t)value;
    return 1;
}

#endif
