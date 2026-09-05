#ifndef GE_3DS_SHADE_CACHE_H
#define GE_3DS_SHADE_CACHE_H

#include "ge_gbi_vertex.h"
#include <string.h>

/* Scratch for one immutable shading state. Reset at every batch and whenever
 * its matrix/lights/look-at state changes. No results survive a frame. */
typedef struct Ge3dsShadeCacheEntry {
    uint32_t rgba;
    int16_t texture_s, texture_t;
    float normal[3], texture[2];
    uint8_t shaded_rgba[4];
    uint8_t texture_generated;
} Ge3dsShadeCacheEntry;

typedef struct Ge3dsShadeCache {
    const GeGbiRenderState *state;
    Ge3dsShadeCacheEntry entries[8];
    unsigned count, next;
} Ge3dsShadeCache;

static inline void ge_3ds_shade_cache_reset(
    Ge3dsShadeCache *cache, const GeGbiRenderState *state)
{
    cache->state = state;
    cache->count = cache->next = 0U;
}

static inline GeGbiVertexProcessStatus ge_3ds_shade_cached(
    Ge3dsShadeCache *cache, const GeGbiVertex *vertex,
    GeGbiProcessedVertex *processed)
{
    if (cache == NULL || cache->state == NULL || vertex == NULL || processed == NULL)
        return GE_GBI_VERTEX_PROCESS_INVALID_ARGUMENT;
    const uint32_t rgba = (uint32_t)vertex->red | (uint32_t)vertex->green << 8U
        | (uint32_t)vertex->blue << 16U | (uint32_t)vertex->alpha << 24U;
    for (unsigned i = 0U; i < cache->count; ++i) {
        const Ge3dsShadeCacheEntry *entry = &cache->entries[i];
        if (entry->rgba != rgba || (!entry->texture_generated
                && (entry->texture_s != vertex->texture_s
                    || entry->texture_t != vertex->texture_t))) continue;
        /* Shade reads RGBA (normals under lighting) and authored ST only.
         * Generated ST replaces authored ST. Preserve all position, clip,
         * padding and unlit-normal bytes from this individual vertex. */
        if (cache->state->geometry_mode & GE_GBI_GEOMETRY_LIGHTING)
            memcpy(processed->normal, entry->normal, sizeof(entry->normal));
        memcpy(processed->texture, entry->texture, sizeof(entry->texture));
        memcpy(processed->rgba, entry->shaded_rgba, sizeof(entry->shaded_rgba));
        processed->texture_generated = entry->texture_generated;
        return GE_GBI_VERTEX_PROCESS_OK;
    }
    const GeGbiVertexProcessStatus status =
        ge_gbi_vertex_shade(cache->state, vertex, processed);
    if (status != GE_GBI_VERTEX_PROCESS_OK) return status;
    Ge3dsShadeCacheEntry *entry = &cache->entries[cache->next];
    cache->next = (cache->next + 1U) & 7U;
    if (cache->count < 8U) ++cache->count;
    entry->rgba = rgba;
    entry->texture_s = vertex->texture_s;
    entry->texture_t = vertex->texture_t;
    memcpy(entry->normal, processed->normal, sizeof(entry->normal));
    memcpy(entry->texture, processed->texture, sizeof(entry->texture));
    memcpy(entry->shaded_rgba, processed->rgba, sizeof(entry->shaded_rgba));
    entry->texture_generated = processed->texture_generated;
    return GE_GBI_VERTEX_PROCESS_OK;
}

#endif
