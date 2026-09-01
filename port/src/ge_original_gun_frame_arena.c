#include "ge_original_gun_frame_arena.h"

#include <stdint.h>
#include <string.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#ifndef PLAYERFLAG
typedef int PLAYERFLAG;
#endif
#include "game/bondview.h"
#include "game/model.h"
#include "ge_original_bond_input_provider.h"

u8 *g_GfxMemPos;

static u8 *ge_gun_frame_start;
static u8 *ge_gun_frame_end;
static uint64_t ge_gun_frame_generation;
static int ge_gun_frame_active;

static int ge_original_gun_frame_arena_measure(size_t *used,
                                                size_t *capacity)
{
    uintptr_t start = (uintptr_t)ge_gun_frame_start;
    uintptr_t end = (uintptr_t)ge_gun_frame_end;
    uintptr_t current = (uintptr_t)g_GfxMemPos;

    if (used != NULL) *used = 0U;
    if (capacity != NULL) *capacity = 0U;
    if (start == 0U || end < start || current < start) return 0;
    if (capacity != NULL) *capacity = (size_t)(end - start);
    if (used != NULL) *used = (size_t)(current - start);
    return current <= end;
}

/* The original hand embeds a 32-bit Model and 32 runtime-data words.  Host
 * sanitizers use 64-bit pointers, so their exact semantic equivalents need
 * naturally aligned, pointer-width storage.  ARM continues to use the
 * canonical embedded fields. */
static Model ge_host_gun_models[2] __attribute__((aligned(16)));
static union ModelRwData ge_host_gun_rwdata[2][32]
    __attribute__((aligned(16)));

int ge_original_gun_frame_arena_begin(void *storage, size_t storage_size)
{
    uintptr_t address;
    uintptr_t aligned;
    size_t skipped;

    if (storage == NULL || storage_size < 16U) {
        return 0;
    }
    address = (uintptr_t)storage;
    aligned = (address + 15U) & ~(uintptr_t)15U;
    skipped = (size_t)(aligned - address);
    if (skipped >= storage_size) {
        return 0;
    }
    ge_gun_frame_start = (u8 *)aligned;
    ge_gun_frame_end = (u8 *)storage + storage_size;
    g_GfxMemPos = ge_gun_frame_start;
    ge_gun_frame_generation++;
    ge_gun_frame_active = 1;
    return 1;
}

size_t ge_original_gun_frame_arena_used(void)
{
    size_t used;
    return ge_original_gun_frame_arena_measure(&used, NULL) ? used : 0U;
}

int ge_original_gun_frame_arena_active(void)
{
    return ge_gun_frame_active;
}

int ge_original_gun_frame_arena_audit(GeOriginalDynFrameAudit *audit)
{
    size_t used;
    size_t capacity;
    int within_bounds = ge_original_gun_frame_arena_measure(
        &used, &capacity);

    if (audit != NULL) {
        audit->generation = ge_gun_frame_generation;
        audit->capacity = capacity;
        audit->used = used;
        audit->active = ge_gun_frame_active;
        audit->within_bounds = within_bounds;
    }
    return within_bounds;
}

int ge_original_gun_frame_arena_finalize(GeOriginalDynFrameAudit *audit)
{
    int within_bounds = ge_original_gun_frame_arena_audit(audit);
    ge_gun_frame_active = 0;
    if (audit != NULL) audit->active = 0;
    return within_bounds;
}

void *ge_original_gun_current_player_view_to_world(void)
{
    GeOriginalBondInputProvider *provider = ge_original_bond_input_provider();
    if (provider == NULL || provider->current_player == NULL) return NULL;
    return provider->current_player->viewtoworldmtxf;
}

void *ge_original_gun_host_model(unsigned hand)
{
    if (hand >= 2U) return NULL;
    memset(&ge_host_gun_models[hand], 0, sizeof(ge_host_gun_models[hand]));
    return &ge_host_gun_models[hand];
}

int32_t *ge_original_gun_host_model_rwdata(unsigned hand)
{
    if (hand >= 2U) return NULL;
    memset(ge_host_gun_rwdata[hand], 0, sizeof(ge_host_gun_rwdata[hand]));
    return (int32_t *)ge_host_gun_rwdata[hand];
}

void ge_original_gun_host_model_set_render_pos(unsigned hand, void *matrices)
{
    if (hand >= 2U) return;
    ge_host_gun_models[hand].render_pos = (RenderPosView *)matrices;
}

void *ge_original_gun_host_model_peek(unsigned hand)
{
    return hand < 2U ? &ge_host_gun_models[hand] : NULL;
}
