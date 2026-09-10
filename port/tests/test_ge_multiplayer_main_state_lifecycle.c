/* Exercises the production main.c ownership helpers against actual native
 * player/inventory and prop pools. This is lifecycle, not match acceptance. */
#include "ge_mp_state_core.h"
#include "ge_mp_state_inventory.h"
#include "ge_mp_state_props.h"
#include "ge_mp_state_guns.h"
#include "ge_mp_state_players.h"
#include "ge_mp_state_bg.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/chrai.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct { bool failed; } multiplayer;
#include "main_state_lifecycle.c"

static GeOriginalPropState props;
extern PropRecord g_Props[600];
void fixture_unobserved_free(PropRecord *);

static void begin(unsigned count)
{
    /* Main acquires E7 after geometry load, before native prop reset. The
     * subsequent stage begin must retain this same fresh-session registry. */
    assert(runtime_multiplayer_state_registry_ready());
    assert(runtime_multiplayer_state_acquire(GE_MP_BG_KIND, GE_MP_BG_NAMESPACE,
        GE_MP_BG_SLOT, &multiplayer_state.bg));
    assert(ge_original_prop_state_reset(&props, 16));
    assert(runtime_multiplayer_state_begin_stage());
    assert(ge_mp_registry_resolve(&multiplayer_state.registry, multiplayer_state.bg, NULL) == GE_MP_STATE_OK);
    if (count) {
        assert(ge_original_multiplayer_players_create(count, 0, 0x12345678, 0x9abcdef0));
        assert(runtime_multiplayer_state_begin_players());
        for (unsigned slot = 0; slot < count; ++slot) {
            struct player *player = ge_original_multiplayer_player(slot);
            player->prop = chrpropAllocate();
            assert(player->prop);
            player->prop->type = PROP_TYPE_VIEWER;
            player->prop->chr = NULL;
            chrpropActivate(player->prop);
            chrpropEnable(player->prop);
            chrpropRegisterRoom(player->prop, 2);
        }
    }
}

static void end(unsigned count)
{
    runtime_multiplayer_state_release(&multiplayer_state.bg);
    if (count) {
        runtime_multiplayer_state_end_players();
        ge_original_multiplayer_players_destroy();
    }
    /* Main's resource wrapper has already closed and been cleared here. */
    memset(&props, 0, sizeof(props));
    assert(ge_original_prop_state_end(&props));
    runtime_multiplayer_state_end_stage();
}

static void repeated_stage(unsigned count)
{
    uint64_t ids[134];
    size_t used = 0;
    begin(count);
    ids[used++] = multiplayer_state.core;
    ids[used++] = multiplayer_state.bg;
    for (unsigned slot = 0; slot < 4; ++slot) {
        ids[used++] = multiplayer_state.players.ids[slot];
        for (unsigned hand = 0; hand < 2; ++hand)
            ids[used++] = multiplayer_state.hands[slot][hand];
    }
    for (unsigned item = 0; item < GE_MP_INVENTORY_RECORDS; ++item) {
        uint64_t id = multiplayer_state.inventory.ids[item];
        assert((uint32_t)id == 1);
        assert(ge_mp_registry_resolve(&multiplayer_state.registry, id, NULL) == GE_MP_STATE_OK);
    }
    for (size_t index = 0; index < used; ++index)
        assert((uint32_t)ids[index] == 1);
    /* Partial object construction is retired by arena end, not a made-up free. */
    PropRecord *partial = chrpropAllocate();
    assert(partial);
    partial->type = PROP_TYPE_OBJ;
    partial->obj = NULL;
    uint64_t before, after;
    assert(ge_mp_state_props_actor(&multiplayer_state.props, g_Props, &before) == GE_MP_STATE_OK);
    assert(ge_original_prop_state_release(&props, g_Props));
    PropRecord *reused = chrpropAllocate();
    assert(reused == g_Props);
    assert(ge_mp_state_props_actor(&multiplayer_state.props, reused, &after) == GE_MP_STATE_OK);
    assert((uint32_t)after == (uint32_t)before + 1);
    assert(ge_mp_registry_resolve(&multiplayer_state.registry, before, NULL) == GE_MP_STATE_STALE);
    end(count);
    assert(!multiplayer_state.failed);
    for (size_t index = 0; index < used; ++index)
        assert(ge_mp_registry_resolve(&multiplayer_state.registry, ids[index], NULL) == GE_MP_STATE_STALE);
    assert(multiplayer_state.registry.count == 739);
}

int main(void)
{
    /* Separate matches start with identical keys/generations even after a
     * different roster or allocation history. Reuse inside each still advances. */
    repeated_stage(2);
    repeated_stage(4);
    repeated_stage(2);
    /* Failure before player creation still retires the live stage owners. */
    begin(0);
    assert(chrpropAllocate());
    end(0);
    assert(!multiplayer_state.failed);
    /* Deliberately bypass one real native free observer: main must latch the
     * missed event, complete teardown, and refuse another stage. */
    begin(2);
    fixture_unobserved_free(chrpropAllocate());
    end(2);
    assert(multiplayer_state.failed && multiplayer.failed);
    assert(!runtime_multiplayer_state_begin_stage());
    puts("main state lifetime: native 2/4/2 history independence, within-match reuse, partial loads and fail-closed retirement passed");
    return 0;
}
