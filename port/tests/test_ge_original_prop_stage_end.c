#include "ge_mp_state_props.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/chrai.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct player *g_playerPointers[4];
extern PropRecord g_Props[600];
extern PropRecord *g_FreeProps,*g_ActivePropsHead,*g_ActivePropsTail;
extern s16 *RoomPropListBlockIndices;
extern struct roomproplistblock *RoomPropListBlocks;
void fixture_unobserved_free(PropRecord *);
PropRecord *fixture_unobserved_allocate(void);
static GeOriginalPropState state;
static GeMpStatePropsOwner owner;
static GeMpRegistry registry;
static GeMpRegistryEntry entries[601];

static void begin(void)
{
    assert(ge_original_prop_state_reset(&state,16));
    assert(ge_mp_state_props_begin(&owner,&registry)==GE_MP_STATE_OK);
}
static void stage(unsigned count,unsigned prior_count)
{
    struct player *players=calloc(count,sizeof(*players));
    ObjectRecord *objects=calloc(4,sizeof(*objects));
    PropRecord saved[600];
    uint64_t ids[8],resolved;
    GeOriginalPropPoolStorage before,after;
    GeOriginalPropState wrong={0};
    begin();
    assert(ge_original_prop_state_pool_storage(&before));
    for(unsigned i=0;i<count+4;++i) {
        PropRecord *prop=chrpropAllocate();assert(prop);
        if(i<count) {
            prop->type=PROP_TYPE_VIEWER;
            players[i].prop=prop;g_playerPointers[i]=&players[i];
            prop->chr=NULL; /* guard-runtime close detaches its body owner. */
        } else {
            prop->type=PROP_TYPE_OBJ;
            prop->obj=&objects[i-count];objects[i-count].prop=prop;
        }
        if(i!=count+3) {
            chrpropActivate(prop);chrpropEnable(prop);
            assert(ge_original_prop_state_set_primary_room(prop,2));
            chrpropRegisterRoom(prop,2);
        } /* An allocated but unactivated partial constructor also survives. */
        assert(ge_mp_state_props_actor(&owner,prop,&ids[i])==GE_MP_STATE_OK);
        if(i<prior_count)assert((uint32_t)ids[i]>1);
    }
    /* One object follows real individual cleanup. Others leave with arena. */
    assert(ge_original_prop_state_release(&state,&g_Props[count]));
    assert(ge_mp_registry_resolve(&registry,ids[count],NULL)==GE_MP_STATE_STALE);
    assert(!ge_original_prop_state_end(&wrong));
    assert(ge_original_prop_state_pool_storage(&after) && after.lifetime==before.lifetime);
    /* Resource teardown can invalidate union pointers. Ending the pool must
     * inspect only its physical allocation/free links, never those unions. */
    free(objects);free(players);memset(g_playerPointers,0,sizeof(g_playerPointers));
    memset(&state,0,sizeof(state)); /* actual close_dam_world_objects behavior */
    memcpy(saved,g_Props,sizeof(saved));
    assert(ge_original_prop_state_end(&state));
    assert(!memcmp(saved,g_Props,sizeof(saved))); /* no fabricated native frees */
    assert(owner.stage_ended && owner.fault==GE_MP_STATE_OK);
    assert(!ge_original_prop_state_pool_storage(&after));
    assert(!g_FreeProps && !g_ActivePropsHead && !g_ActivePropsTail);
    assert(!RoomPropListBlockIndices && !RoomPropListBlocks);
    assert(!chrpropAllocate());
    assert(!ge_original_prop_state_is_enabled(&g_Props[0]));
    assert(!ge_original_prop_state_end(&state));
    assert(ge_mp_state_props_actor(&owner,&g_Props[0],&resolved)==GE_MP_STATE_STALE);
    for(unsigned i=0;i<count+4;++i)
        assert(ge_mp_registry_resolve(&registry,ids[i],NULL)==GE_MP_STATE_STALE);
    assert(ge_mp_state_props_end(&owner)==GE_MP_STATE_OK);
}
static void invalid_lifetimes(void)
{
    begin();
    PropRecord *prop=chrpropAllocate();assert(prop);
    fixture_unobserved_free(prop);
    assert(ge_original_prop_state_end(&state));
    assert(ge_mp_state_props_end(&owner)==GE_MP_STATE_INCOMPLETE);
    begin();assert(fixture_unobserved_allocate());
    assert(ge_original_prop_state_end(&state));
    assert(ge_mp_state_props_end(&owner)==GE_MP_STATE_INCOMPLETE);
    begin();assert(chrpropAllocate());
    assert(ge_original_prop_state_reset(&state,16));
    assert(ge_original_prop_state_end(&state));
    assert(ge_mp_state_props_end(&owner)==GE_MP_STATE_INCOMPLETE);
    /* Ending an empty partial load is legitimate and requires no frees. */
    begin();assert(ge_original_prop_state_end(&state));
    assert(ge_mp_state_props_end(&owner)==GE_MP_STATE_OK);
}
int main(void)
{
    assert(ge_mp_registry_init(&registry,entries,601)==GE_MP_STATE_OK);
    stage(2,0);stage(4,6);stage(2,6);
    invalid_lifetimes();
    puts("native stage pool: 2/4/2 arena retirement, freed consumers, partial allocations, generation reuse, and missed lifecycle negative controls passed");
    return 0;
}
