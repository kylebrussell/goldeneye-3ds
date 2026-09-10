#include "ge_mp_state_inventory.h"
#include "ge_original_bond_input_provider.h"
#include "ge_original_input.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/bondinv.h"
#include "game/chrObjRandom.h"
#include <random.h>
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

extern s32 scenario;
extern InvItem *bondinvGetInvItem(ITEM_IDS);
extern void bondinvRemoveItem(InvItem *);
extern int bondinvAddPropToInv(PropRecord *);
extern void increment_num_kills_display_text_in_MP(void);
extern void increment_num_times_killed_MwtGC(void);
extern void increment_num_deaths(void);
extern void increment_num_suicides_display_MP(void);
static int mission_timer=600;
s32 getMissiontimer(void){return mission_timer;}
u8 *langGet(s32 id){(void)id;return (u8 *)"fixture";}
void hudmsgBottomShow(char *message){assert(message);}
static GeMpStateInventorySnapshot exported;
static GeMpStateInventoryOwner owner;
static GeMpRegistry registry;
static GeMpRegistryEntry entries[128];
static PropRecord props[2];
static uint64_t prop_ids[2];
static GeMpStateStatus prop_actor(void *context,const void *prop,uint64_t *id)
{
    (void)context;
    for(int i=0;i<2;++i)if(prop==&props[i]){*id=prop_ids[i];return GE_MP_STATE_OK;}
    return GE_MP_STATE_INCOMPLETE;
}
static const GeMpStateInventoryProviders providers={NULL,prop_actor};
static void export_ok(void)
{
    struct player *current=g_CurrentPlayer;
    struct player_data *perm=g_playerPerm;
    const u64 before_rng=g_randomSeed,before_chr=g_chrObjRandomSeed;
    assert(ge_mp_state_inventory_export(&owner,&providers,&exported)==GE_MP_STATE_OK);
    assert(exported.record_count==124 && exported.coverage.enumerated_systems==GE_MP_SYSTEM_INVENTORY);
    assert(!exported.coverage.unclassified_fields && !exported.coverage.unresolved_references);
    size_t schema_count;
    const GeMpRecordSchema *schema=ge_mp_state_inventory_schemas(&schema_count);
    assert(schema_count==2);
    for(size_t i=0;i<exported.record_count;++i) {
        const GeMpStateRecord *record=&exported.records[i];
        const GeMpRecordSchema *shape=&schema[i<120?0:1];
        assert(record->kind==shape->kind && record->field_count==shape->field_count);
        if(i)assert(exported.records[i-1].actor_id<record->actor_id);
        for(size_t j=0;j<record->field_count;++j) {
            const GeMpStateField *field=&record->fields[j];
            assert(field->id==shape->fields[j].id && field->type==shape->fields[j].type);
            assert(field->count>=shape->fields[j].min_count && field->count<=shape->fields[j].max_count);
            if(j)assert(record->fields[j-1].id<field->id);
        }
    }
    assert(g_CurrentPlayer==current && g_playerPerm==perm && g_randomSeed==before_rng && g_chrObjRandomSeed==before_chr);
}
/* Fixture-only typed payload fingerprint. It deliberately is NOT a whole
 * state checkpoint: aggregate native coverage remains incomplete below. */
static void fingerprint(uint8_t digest[32])
{
    static unsigned char bytes[65536];
    static const unsigned width[]={0,1,2,4,8,1,2,4,8,4,1,8,4};
    size_t used=0;
    export_ok();
    for(size_t r=0;r<exported.record_count;++r) {
        const GeMpStateRecord *record=&exported.records[r];
        memcpy(bytes+used,&record->actor_id,8);used+=8;
        for(size_t f=0;f<record->field_count;++f) {
            const GeMpStateField *field=&record->fields[f];
            size_t n=width[field->type]*field->count;
            assert(used+7+n<sizeof(bytes));
            memcpy(bytes+used,&field->id,2);used+=2;bytes[used++]=field->type;
            memcpy(bytes+used,&field->count,4);used+=4;
            memcpy(bytes+used,field->values,n);used+=n;
        }
    }
    assert(ge_mp_sha256(bytes,used,digest)==GE_MP_STATE_OK);
}
static void setup(unsigned count,int fresh_registry)
{
    assert(ge_original_multiplayer_players_create(count,count-1,UINT64_C(0x915612342356abcd),UINT64_C(0x879812344321abcd)));
    scenario=SCENARIO_NORMAL;
    for(unsigned p=0;p<count;++p) {
        assert(ge_original_multiplayer_player_bind_inventory(p));
        assert(ge_original_multiplayer_select_player(p));bondinvReinitInv();
        assert(bondinvAddInvItem(ITEM_FIST));assert(bondinvAddInvItem(ITEM_WPPK));
        g_playerPlayerData[p].autoaim=(uint8_t)(p&1);
        g_playerPlayerData[p].sight=(uint8_t)((p+1)&1);
        g_playerPlayerData[p].handicap=0.5f+(float)p/4;
    }
    if(fresh_registry) {
        assert(ge_mp_registry_init(&registry,entries,128)==GE_MP_STATE_OK);
        for(int i=0;i<2;++i) {
            uint32_t k=ge_mp_registry_key(0x20,0,(uint16_t)i);
            assert(ge_mp_registry_define(&registry,k)==GE_MP_STATE_OK);
            assert(ge_mp_registry_acquire(&registry,k,&prop_ids[i])==GE_MP_STATE_OK);
        }
    }
    assert(ge_mp_state_inventory_begin(&owner,&registry)==GE_MP_STATE_OK);
    assert(ge_mp_state_inventory_begin(&owner,&registry)==GE_MP_STATE_STALE);
    export_ok();
    for(unsigned p=0;p<4;++p) {
        assert(exported.permission[p].active==(p<count));
        assert(exported.permission[p].reserved_capacity==30);
        assert(exported.permission_fields[p][0].count==(p<count?2:0));
        assert(exported.permission_fields[p][1].count==(p<count?28:0));
        for(unsigned s=0;s<30;++s) {
            unsigned n=p*30+s;
            assert(exported.records[n].actor_id==owner.ids[n]);
            assert(exported.inventory[n].type==(p>=count?INV_ITEM_NONE:s<2?INV_ITEM_WEAPON:-1));
        }
    }
}
static void teardown(void)
{
    assert(ge_mp_state_inventory_end(&owner)==GE_MP_STATE_OK);
    ge_original_multiplayer_players_destroy();
    assert(!ge_original_multiplayer_inventory_storage(0,&(GeOriginalMultiplayerInventoryStorage){0}));
}
static void operations(unsigned count)
{
    uint8_t before[32],after[32];
    setup(count,1);fingerprint(before);
    for(unsigned p=0;p<count;++p) {
        assert(ge_original_multiplayer_select_player(p));
        assert(bondinvAddDoublesInvItem(ITEM_WPPK,ITEM_WPPK));
        assert(bondinvAddPropToInv(&props[p%2]));
        increment_num_kills_display_text_in_MP();
        increment_num_times_killed_MwtGC();increment_num_deaths();increment_num_deaths();
        increment_num_suicides_display_MP();
        g_playerPlayerData[p].kill_counts[p]=3;
        export_ok();
        assert(exported.permission[p].kill_count==1 && exported.permission[p].gg_kills==1);
        assert(exported.permission[p].deathcount==2 && exported.permission[p].suicides==1);
        assert(exported.permission[p].kills_this_life==1 && exported.permission[p].last_kill_time[0]==600);
        assert(exported.permission[p].score==-3+(int)count-2);
        assert(exported.inventory[p*30+2].right==ITEM_WPPK && exported.inventory[p*30+2].left==ITEM_WPPK);
        assert(exported.inventory[p*30+3].prop==prop_ids[p%2]);
        InvItem *item=bondinvGetInvItem(ITEM_WPPK);
        uint64_t slot_id=owner.ids[p*30+1];
        bondinvRemoveItem(item);export_ok();
        assert(exported.inventory[p*30+1].type==-1 && owner.ids[p*30+1]==slot_id);
        assert(exported.inventory[p*30+1].next && exported.inventory[p*30+1].prev);
        assert(exported.inventory_fields[p*30+1][3].count==0);
        /* Inactive union payload is not future state: original add writes the
         * selected member before reading it. Stale links remain exported. */
        fingerprint(before);item->type_inv_item.type_prop.prop=(void *)(uintptr_t)0x12345678;
        fingerprint(after);assert(!memcmp(before,after,32));
        assert(bondinvAddInvItem(ITEM_KNIFE));export_ok();
        assert(exported.inventory[p*30+1].weapon==ITEM_KNIFE && owner.ids[p*30+1]==slot_id);
    }
    fingerprint(after);assert(memcmp(before,after,32));teardown();
}
static void mutation_sensitivity(void)
{
    uint8_t before[32],after[32];
    setup(4,1);
    struct player *p=ge_original_multiplayer_player(3);
    struct player_data *d=&g_playerPlayerData[3];
#define CHANGE(expr) do { __typeof__(expr) saved=(expr);fingerprint(before);(expr)=saved+1;fingerprint(after);assert(memcmp(before,after,32));(expr)=saved; } while(0)
    for(int i=0;i<7;++i)CHANGE(d->shot_count[i]);
    for(int i=0;i<4;++i){CHANGE(d->kill_counts[i]);CHANGE(p->last_kill_time[i]);}
    CHANGE(d->kill_count);CHANGE(d->killed_gg_owner_count);CHANGE(d->time_other_players_on_screen);
    CHANGE(d->distance_traveled);CHANGE(d->damage_to_backside);CHANGE(d->body_armor_pickups);
    CHANGE(d->min_time_between_kills);CHANGE(d->max_time_between_kills);CHANGE(d->longest_inning);
    CHANGE(d->shortest_inning);CHANGE(d->most_killed_one_life);CHANGE(d->most_killed_one_time);
    CHANGE(d->handicap);CHANGE(d->flag_counter);CHANGE(d->player_perspective_height);
    CHANGE(d->order_out_in_yolt);CHANGE(d->have_token_or_goldengun);CHANGE(d->autoaim);CHANGE(d->sight);CHANGE(d->killed_civilians);
    CHANGE(p->equipallguns);CHANGE(p->equipcuritem);CHANGE(p->deathcount);CHANGE(p->num_suicides);
    CHANGE(p->kills_this_life);CHANGE(p->lifestarttime60);
    for(int i=0;i<30;++i)CHANGE(p->ammoheldarr[i]);
    for(int i=0;i<10;++i){CHANGE(p->gunheldarr[i].weapon1);CHANGE(p->gunheldarr[i].weapon2);CHANGE(p->gunheldarr[i].totaltime);}
    assert(ge_original_multiplayer_select_player(3));
    CHANGE(p->p_itemcur[1].type_inv_item.type_weap.weapon);
    assert(bondinvAddDoublesInvItem(ITEM_WPPK,ITEM_WPPK));
    CHANGE(p->p_itemcur[2].type_inv_item.type_dual.weapon_right);
    CHANGE(p->p_itemcur[2].type_inv_item.type_dual.weapon_left);
    assert(bondinvAddPropToInv(&props[0]));fingerprint(before);
    p->p_itemcur[3].type_inv_item.type_prop.prop=&props[1];fingerprint(after);assert(memcmp(before,after,32));
    fingerprint(before);p->ptr_inventory_first_in_cycle=p->ptr_inventory_first_in_cycle->next;
    fingerprint(after);assert(memcmp(before,after,32));
    /* Free-slot links also participate, independent of active ring order. */
    p->p_itemcur[29].next=&p->p_itemcur[0];fingerprint(before);
    p->p_itemcur[29].next=&p->p_itemcur[1];fingerprint(after);assert(memcmp(before,after,32));
    p->p_itemcur[29].prev=&p->p_itemcur[0];fingerprint(before);
    p->p_itemcur[29].prev=&p->p_itemcur[1];fingerprint(after);assert(memcmp(before,after,32));
    fingerprint(before);d->distance_traveled=-0.0f;fingerprint(after);assert(memcmp(before,after,32));
    uint32_t bits;memcpy(&bits,&exported.permission[3].distance,4);assert(bits==UINT32_C(0x80000000));
    d->kill_counts[3]=INT_MAX;export_ok();assert(exported.permission[3].score==-INT_MAX);
    memcpy(&bits,&exported.permission[3].score,4);assert(bits==UINT32_C(0x80000001));
#undef CHANGE
    teardown();
}
static void failures_and_lifetime(void)
{
    setup(2,1);
    {
        GeMpRegistry limited;GeMpRegistryEntry small[123];GeMpStateInventoryOwner pending={0};
        assert(ge_mp_registry_init(&limited,small,123)==GE_MP_STATE_OK);
        assert(ge_mp_state_inventory_begin(&pending,&limited)==GE_MP_STATE_CAPACITY);
        assert(limited.count==0 && !pending.live);
        assert(ge_mp_registry_define(&limited,ge_mp_registry_key(GE_MP_INVENTORY_KIND,0,0))==GE_MP_STATE_OK);
        limited.entries[0].generation=UINT32_MAX;
        assert(ge_mp_state_inventory_begin(&pending,&limited)==GE_MP_STATE_EXHAUSTED);
        assert(limited.count==1 && !pending.live);
    }
    struct player *p=ge_original_multiplayer_player(0);
    assert(ge_mp_state_inventory_export(&owner,NULL,&exported)==GE_MP_STATE_INCOMPLETE);
    assert(exported.coverage.unresolved_references && !exported.record_count);
    s32 known_weapon=p->p_itemcur[0].type_inv_item.type_weap.weapon;
    p->p_itemcur[0].type_inv_item.type_weap.weapon=ITEM_IDS_MAX;
    assert(ge_mp_state_inventory_export(&owner,&providers,&exported)==GE_MP_STATE_INCOMPLETE);
    p->p_itemcur[0].type_inv_item.type_weap.weapon=known_weapon;
    p->p_itemcur[29].type=INV_ITEM_PICKUP;
    assert(ge_mp_state_inventory_export(&owner,&providers,&exported)==GE_MP_STATE_INCOMPLETE);
    assert(exported.coverage.unclassified_fields && !exported.record_count);p->p_itemcur[29].type=-1;
    p->p_itemcur[29].next=(void *)(uintptr_t)1;
    assert(ge_mp_state_inventory_export(&owner,&providers,&exported)==GE_MP_STATE_INCOMPLETE);
    p->p_itemcur[29].next=NULL;p->textoverrides=(void *)(uintptr_t)1;
    assert(ge_mp_state_inventory_export(&owner,&providers,&exported)==GE_MP_STATE_INCOMPLETE);
    p->textoverrides=NULL;assert(ge_original_multiplayer_select_player(0));
    assert(bondinvAddPropToInv((void *)(uintptr_t)1));
    assert(ge_mp_state_inventory_export(&owner,&providers,&exported)==GE_MP_STATE_INCOMPLETE);
    assert(exported.coverage.unresolved_references);p->p_itemcur[2].type_inv_item.type_prop.prop=&props[0];
    uint64_t saved_id=prop_ids[0];prop_ids[0]++;
    assert(ge_mp_state_inventory_export(&owner,&providers,&exported)==GE_MP_STATE_INCOMPLETE);
    prop_ids[0]=saved_id;export_ok();
    uint64_t old_slot=owner.ids[0];
    assert(ge_mp_state_inventory_end(&owner)==GE_MP_STATE_OK);
    assert(ge_mp_state_inventory_begin(&owner,&registry)==GE_MP_STATE_OK);
    assert(owner.ids[0]==old_slot+1 && ge_mp_registry_resolve(&registry,old_slot,NULL)==GE_MP_STATE_STALE);
    /* Deliberately miss native teardown hook. Same count and any same-address
     * allocator reuse still cannot validate the previous physical lifetime. */
    ge_original_multiplayer_players_destroy();
    assert(ge_original_multiplayer_players_create(2,0,1,2));
    assert(ge_mp_state_inventory_export(&owner,&providers,&exported)==GE_MP_STATE_STALE);
    assert(!exported.record_count);teardown();
}
static void repeated_history(void)
{
    uint8_t first[32],last[32];
    setup(2,1);fingerprint(first);
    struct player *p=ge_original_multiplayer_player(0);
    p->mpmenuon=1;p->watch_pause_time=77;p->equipcuritem=9;p->ammoheldarr[29]=123;
    g_playerPlayerData[3].kill_count=79;teardown();
    setup(4,1);
    p=ge_original_multiplayer_player(3);p->gunheldarr[9].totaltime=900;
    p->num_suicides=4;g_playerPlayerData[0].have_token_or_goldengun=1;teardown();
    setup(2,1);fingerprint(last);assert(!memcmp(first,last,32));
    /* Agreed user permission changes remain meaningful, never normalized. */
    g_playerPlayerData[0].autoaim^=1;fingerprint(last);assert(memcmp(first,last,32));teardown();
}
static void incomplete_checkpoint(void)
{
    setup(2,1);export_ok();
    GeMpStateSchema schema={0};GeMpCanonicalSnapshot snapshot={0};
    uint8_t digest[32],old[32];GeMpCoverageReport report;
    GeMpRecordSchema records[3];size_t count;
    const GeMpRecordSchema *domain=ge_mp_state_inventory_schemas(&count);assert(count==2);
    records[0]=(GeMpRecordSchema){0,GE_MP_SYSTEM_SESSION,NULL,0};records[1]=domain[0];records[2]=domain[1];
    schema.version=1;schema.registered_systems=GE_MP_SYSTEM_SESSION|GE_MP_SYSTEM_INVENTORY|GE_MP_SYSTEM_PLAYERS|GE_MP_SYSTEM_RULES;
    schema.records=records;schema.record_count=3; /* complete intentionally stays zero */
    snapshot.session=1;snapshot.enabled_systems=schema.registered_systems;
    memset(snapshot.build_sha256,1,32);memset(snapshot.content_sha256,2,32);memset(snapshot.rules_sha256,3,32);
    snapshot.schema=&schema;snapshot.registry=&registry;snapshot.records=exported.records;
    snapshot.record_count=exported.record_count;snapshot.coverage=exported.coverage;
    memset(digest,0x5a,sizeof(digest));memcpy(old,digest,sizeof(old));
    assert(ge_mp_state_sha256(&snapshot,digest,&report)==GE_MP_STATE_INCOMPLETE);
    assert(!memcmp(digest,old,32));teardown();
}
int main(void)
{
    operations(2);operations(4);mutation_sensitivity();failures_and_lifetime();repeated_history();incomplete_checkpoint();
    puts("native inventory/permissions: all124 physical records,2/4 original inventory operations/death counters,typed mutation/negative bits,registry lifetime,incomplete references,2->4->2 convergence passed");
    return 0;
}
