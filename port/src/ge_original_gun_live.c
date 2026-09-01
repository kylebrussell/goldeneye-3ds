#include "ge_original_gun_live.h"

#include "ge_original_gun_frame_arena.h"

#include <string.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/bondview.h"
#include "game/model.h"
#include "ge_original_bond_input_provider.h"

/* The native renderer originally activated only the first-person gun bodies,
 * so this adapter started with the N64 one-player 0x10000 vtx-pool size. The
 * unchanged maybe_mp_interface death path now shares the same canonical
 * dynAllocate stream (two 80x96 blood buffers plus the hand/model matrices).
 * Use the original engine's largest authored per-frame vtx-pool size; this
 * changes no allocation address/order within a frame and prevents an
 * unchecked N64 dynAllocate from escaping the backing store on 3DS. */
#define GE_ORIGINAL_GUN_FRAME_ARENA_SIZE 0x28000U

static unsigned char ge_original_gun_frame_storage[
    GE_ORIGINAL_GUN_FRAME_ARENA_SIZE] __attribute__((aligned(16)));
static GeOriginalGunLiveStats ge_original_gun_live_stats;
static GeOriginalGunLiveHand ge_original_gun_live_hands[2];

extern void ge_original_gun_update_and_fire_both_hands_exact(void);

void ge_original_gun_live_reset(void)
{
    memset(&ge_original_gun_live_stats, 0,
           sizeof(ge_original_gun_live_stats));
    memset(ge_original_gun_live_hands, 0,
           sizeof(ge_original_gun_live_hands));
}

int ge_original_gun_live_frame_begin(void)
{
    if (ge_original_gun_frame_arena_active()) return 0;
    return ge_original_gun_frame_arena_begin(
        ge_original_gun_frame_storage,
        sizeof(ge_original_gun_frame_storage));
}

int ge_original_gun_live_frame_finalize(GeOriginalDynFrameAudit *audit)
{
    if (!ge_original_gun_frame_arena_active()) return 0;
    return ge_original_gun_frame_arena_finalize(audit);
}

int ge_original_gun_live_tick(void)
{
    GeOriginalDynFrameAudit audit;
    int owns_frame = !ge_original_gun_frame_arena_active();
    size_t start_used;
    size_t used;
    if (owns_frame && !ge_original_gun_frame_arena_begin(
            ge_original_gun_frame_storage,
            sizeof(ge_original_gun_frame_storage))) return 0;
    start_used = ge_original_gun_frame_arena_used();
    ge_original_gun_update_and_fire_both_hands_exact();
    if (!ge_original_gun_frame_arena_audit(&audit)
            || audit.used < start_used) {
        if (owns_frame) (void)ge_original_gun_frame_arena_finalize(NULL);
        return 0;
    }
    used = audit.used - start_used;
    if (owns_frame
            && !ge_original_gun_frame_arena_finalize(&audit)) return 0;
    ge_original_gun_live_stats.ticks++;
    ge_original_gun_live_stats.last_frame_generation = audit.generation;
    ge_original_gun_live_stats.last_frame_bytes = used;
    if (used > ge_original_gun_live_stats.peak_frame_bytes)
        ge_original_gun_live_stats.peak_frame_bytes = used;
    {
        GeOriginalBondInputProvider *provider =
            ge_original_bond_input_provider();
        unsigned hand;
        if (provider == NULL || provider->current_player == NULL) return 0;
        for (hand = 0U; hand < 2U; ++hand) {
            struct hand *source = &provider->current_player->hands[hand];
            Model *model;
#if defined(GE_PORT_GUN_HOST_MODEL_ABI) || UINTPTR_MAX > UINT32_MAX
            model = ge_original_gun_host_model_peek(hand);
#else
            model = (Model *)(void *)&source->field_B68;
#endif
            ge_original_gun_live_hands[hand].model =
                source->field_87F != 0 ? model : NULL;
            ge_original_gun_live_hands[hand].matrices =
                source->field_87F != 0
                    ? (const float (*)[4][4])(const void *)source->mtxlist
                    : NULL;
            ge_original_gun_live_hands[hand].matrix_count =
                source->field_87F != 0 && model != NULL && model->obj != NULL
                    ? (size_t)model->obj->numMatrices : 0U;
            ge_original_gun_live_hands[hand].visible =
                source->field_87F != 0;
            ge_original_gun_live_hands[hand].generation =
                ge_original_gun_live_stats.ticks;
        }
    }
    return 1;
}

void ge_original_gun_live_snapshot(GeOriginalGunLiveStats *stats)
{
    if (stats != NULL) *stats = ge_original_gun_live_stats;
}

int ge_original_gun_live_hand_snapshot(
    unsigned hand, GeOriginalGunLiveHand *publication)
{
    if (hand >= 2U || publication == NULL
            || ge_original_gun_live_hands[hand].generation == 0U) return 0;
    *publication = ge_original_gun_live_hands[hand];
    return 1;
}
