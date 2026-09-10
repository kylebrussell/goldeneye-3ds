/* Exercises the production main.c ownership helpers against actual native
 * player/inventory and prop pools. This is lifecycle, not match acceptance. */
#include "ge_mp_state_core.h"
#include "ge_mp_state_inventory.h"
#include "ge_mp_state_props.h"
#include "ge_mp_state_guns.h"
#include "ge_mp_state_players.h"
#include "ge_mp_state_bg.h"
#include "ge_mp_state_effects.h"
#include "ge_mp_state_visibility.h"
#include "ge_mp_state_fog.h"
#include "ge_mp_state_objects.h"
#include "ge_mp_state_models.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/chrai.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct { bool failed; } multiplayer;
#include "main_state_lifecycle.c"

/* Physical definition resource seams for the extracted main enumerator. The
 * E3 pool, EA registry owner and native object layouts remain real below. */
#define GE_ORIGINAL_STAGE_PLAYER_ITEM_CAPACITY 30U
typedef struct GeOriginalStageGuardRuntime {
    WeaponObjRecord weapons[2],items[GE_ORIGINAL_STAGE_PLAYER_ITEM_CAPACITY];
} GeOriginalStageGuardRuntime;
typedef struct RuntimeStageOrdinaryEntry {
    void *definition;
    size_t definition_size,command_index;
    uint8_t type;
} RuntimeStageOrdinaryEntry;
typedef struct PhysicalModelProvider {
    Model models[5];
    size_t capacity;
    unsigned active_mask;
    int fail_slot;
} PhysicalModelProvider;
typedef struct RuntimeStageOrdinaryObjects {
    GeOriginalStageGuardRuntime *guards;
    RuntimeStageOrdinaryEntry *entries;
    size_t entry_count;
    PhysicalModelProvider *models, *guard_models;
    bool initialized;
} RuntimeStageOrdinaryObjects;
AmmoCrateRecord g_AmmoCrates[MAX_AMMO_CRATES];
static size_t ge_original_stage_guard_runtime_weapon_count(const GeOriginalStageGuardRuntime *r)
{ assert(r);return 2; }
static int ge_original_stage_guard_runtime_weapon_definition(const GeOriginalStageGuardRuntime *r,
    size_t slot,const void **definition,size_t *bytes)
{ assert(r&&slot<2);*definition=&r->weapons[slot];*bytes=sizeof(r->weapons[slot]);return 1; }
static int ge_original_stage_guard_runtime_player_item_definition(const GeOriginalStageGuardRuntime *r,
    size_t slot,const void **definition,size_t *bytes)
{ assert(r&&slot<30);*definition=&r->items[slot];*bytes=sizeof(r->items[slot]);return 1; }
#include "main_object_bindings.c"

static size_t ge_original_pitem_model_physical_capacity(const PhysicalModelProvider *p)
{ return p ? p->capacity : 0; }
static size_t ge_original_character_model_physical_capacity(const PhysicalModelProvider *p)
{ return p ? p->capacity : 0; }
static int physical_model_instance(const PhysicalModelProvider *p, size_t slot,
    const void **model, size_t *bytes, int *active)
{
    assert(p && slot < p->capacity && slot < 5);
    if ((int)slot == p->fail_slot) return 0;
    *model = &p->models[slot]; *bytes = sizeof(Model);
    *active = (p->active_mask >> slot) & 1;
    return 1;
}
static int ge_original_pitem_model_physical_instance(const PhysicalModelProvider *p,
    size_t slot, const void **model, size_t *bytes, int *active)
{ return physical_model_instance(p, slot, model, bytes, active); }
static int ge_original_character_model_physical_instance(const PhysicalModelProvider *p,
    size_t slot, const void **model, size_t *bytes, int *active)
{ return physical_model_instance(p, slot, model, bytes, active); }
#include "main_model_bindings.c"


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
    assert(runtime_multiplayer_state_acquire(GE_MP_VISIBILITY_KIND,
        GE_MP_VISIBILITY_NAMESPACE, GE_MP_VISIBILITY_SLOT, &multiplayer_state.visibility));
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
    runtime_multiplayer_state_end_models();
    runtime_multiplayer_state_end_objects();
    runtime_multiplayer_state_end_visibility();
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
    ids[used++] = multiplayer_state.visibility;
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
    assert(multiplayer_state.registry.count == 740);
}

static void object_bindings(unsigned count)
{
    begin(count);
    GeOriginalStageGuardRuntime guards={0};
    WeaponObjRecord weapon={0};AmmoCrateRecord magazine={0};
    MultiAmmoCrateRecord ammo={0};BodyArmourRecord armour={0};
    /* The source enumerator must sort authored command identity, include
     * inactive physical definitions, and exclude only unsupported variants. */
    RuntimeStageOrdinaryEntry entries[]={
        {&armour,sizeof(armour),25,PROPDEF_ARMOUR},
        {&weapon,sizeof(weapon),10,PROPDEF_COLLECTABLE},
        {NULL,0,11,PROPDEF_AUTOGUN},
        {&ammo,sizeof(ammo),2,PROPDEF_AMMO},
        {&magazine,sizeof(magazine),0,PROPDEF_MAGAZINE},
    };
    RuntimeStageOrdinaryObjects objects={&guards,entries,5};
    assert(runtime_multiplayer_state_begin_objects(&objects));
    GeMpStateObjectsOwner *owner=&multiplayer_state.objects;
    assert(owner->live&&owner->count==56);
    const uint16_t commands[]={0,2,10,25};
    uint64_t ids[56];memcpy(ids,owner->ids,sizeof(ids));
    for(unsigned i=0;i<56;++i) {
        const GeMpObjectDefinition *d=&owner->definitions[i];
        unsigned ns=i<4?0:i<6?1:i<36?2:3;
        unsigned slot=i<4?commands[i]:i<6?i-4:i<36?i-6:i-36;
        assert(d->creation_namespace==ns&&d->slot==slot);
        uint64_t id=0;
        assert(ge_mp_state_objects_actor(owner,d->definition,&id)==GE_MP_STATE_OK);
        assert(id==ids[i]);
        assert((uint32_t)(id>>32)==ge_mp_registry_key(GE_MP_OBJECTS_KIND,ns,slot));
    }
    /* Main ends EA while all physical definitions and E3 are still alive. */
    end(count);
    assert(!owner->live&&!multiplayer_state.failed);
    for(unsigned i=0;i<56;++i)
        assert(ge_mp_registry_resolve(&multiplayer_state.registry,ids[i],NULL)==GE_MP_STATE_STALE);
}

static void model_bindings(unsigned count)
{
    begin(count);
    PhysicalModelProvider pitem = {.capacity = 3, .active_mask = 1, .fail_slot = -1};
    PhysicalModelProvider characters = {.capacity = 5, .active_mask = 3, .fail_slot = -1};
    RuntimeStageOrdinaryObjects objects = {.models = &pitem, .guard_models = &characters,
        .initialized = true};
    assert(runtime_multiplayer_state_begin_models(&objects));
    GeMpStateModelsOwner *owner = &multiplayer_state.models;
    assert(owner->live && owner->count == 12);
    uint64_t ids[12]; memcpy(ids, owner->ids, sizeof(ids));
    for (unsigned i = 0; i < 12; ++i) {
        const GeMpModelDefinition *d = &owner->definitions[i];
        unsigned space = i < 4 ? 0 : i < 7 ? 1 : 2;
        unsigned slot = i < 4 ? i : i < 7 ? i - 4 : i - 7;
        assert(d->creation_namespace == space && d->slot == slot);
        assert(d->extent == GE_MP_MODEL_FULL && d->bytes == sizeof(Model));
        const void *native;
        if (space == 0) {
            GeOriginalMultiplayerInventoryStorage storage;
            assert(ge_original_multiplayer_inventory_storage(slot, &storage));
            native = &storage.player->model;
            assert(storage.active_player == (slot < count));
        } else native = space == 1 ? &pitem.models[slot] : &characters.models[slot];
        assert(d->model == native);
        uint64_t id = 0;
        assert(ge_mp_state_models_actor(owner, native, &id) == GE_MP_STATE_OK && id == ids[i]);
        assert((uint32_t)(id >> 32) == ge_mp_registry_key(GE_MP_MODELS_KIND, space, slot));
        assert((uint32_t)id == 1);
    }
    /* An inactive slot becoming allocated retains its physical identity. */
    pitem.active_mask |= 4; characters.active_mask |= 16;
    uint64_t id = 0;
    assert(ge_mp_state_models_actor(owner, &pitem.models[2], &id) == GE_MP_STATE_OK && id == ids[6]);
    end(count);
    assert(!owner->live && !multiplayer_state.failed);
    for (unsigned i = 0; i < 12; ++i)
        assert(ge_mp_registry_resolve(&multiplayer_state.registry, ids[i], NULL) == GE_MP_STATE_STALE);
    const void *unavailable = NULL; size_t bytes = 0;
    assert(!ge_original_multiplayer_player_model_storage(0, &unavailable, &bytes));
}

static void incomplete_model_binding(void)
{
    begin(2);
    PhysicalModelProvider pitem = {.capacity = 3, .fail_slot = 1};
    PhysicalModelProvider characters = {.capacity = 5, .fail_slot = -1};
    RuntimeStageOrdinaryObjects objects = {.models = &pitem, .guard_models = &characters,
        .initialized = true};
    size_t registry_count = multiplayer_state.registry.count;
    assert(!runtime_multiplayer_state_begin_models(&objects));
    assert(!multiplayer_state.models.live && multiplayer_state.failed && multiplayer.failed);
    assert(multiplayer_state.registry.count == registry_count);
    end(2);
    assert(!runtime_multiplayer_state_begin_stage());
    /* Independent failure scenario: emulate a fresh application process. */
    memset(&multiplayer_state, 0, sizeof(multiplayer_state));
    multiplayer.failed = false;
}

int main(void)
{
    /* Separate matches start with identical keys/generations even after a
     * different roster or allocation history. Reuse inside each still advances. */
    repeated_stage(2);
    repeated_stage(4);
    repeated_stage(2);
    object_bindings(2);
    model_bindings(2);
    model_bindings(4);
    model_bindings(2);
    incomplete_model_binding();
    object_bindings(4);
    object_bindings(2);
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
    puts("main state lifetime: native 2/4/2 history independence, within-match reuse, 56 EA definitions, all 12 EC physical slots, partial loads and fail-closed retirement passed");
    return 0;
}
