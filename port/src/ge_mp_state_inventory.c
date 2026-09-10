#include "ge_mp_state_inventory.h"
#include "ge_original_multiplayer_rules.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/front.h"
#include <limits.h>
#include <math.h>
#include <string.h>

_Static_assert(sizeof(struct player_data)==0x70,"review player_data coverage after native layout changes");
_Static_assert(sizeof(invitem_weap)==4 && sizeof(invitem_dual)==8
    && sizeof(invitem_prop)==sizeof(void *),"review inventory union member coverage");

#define PERMISSION_FIELDS(X) \
 X(GE_MP_FIELD_ACTIVE_ORDER,SLOT_REF,active_order,0,30,GE_MP_FIELD_UNIQUE) \
 X(GE_MP_FIELD_FREE_ORDER,SLOT_REF,free_order,0,30,GE_MP_FIELD_UNIQUE) \
 X(GE_MP_PERM_ACTIVE,BOOL,active,1,1,0) \
 X(GE_MP_PERM_BASE,SLOT_REF,base,1,1,GE_MP_FIELD_NULLABLE) \
 X(GE_MP_PERM_HEAD,SLOT_REF,head,1,1,GE_MP_FIELD_NULLABLE) \
 X(GE_MP_PERM_CAPACITY,I32,capacity,1,1,0) \
 X(GE_MP_PERM_ALL_GUNS,I32,all_guns,1,1,0) \
 X(GE_MP_PERM_CURRENT_ITEM,I32,current_item,1,1,0) \
 X(GE_MP_PERM_AMMO,I32,ammo,30,30,0) \
 X(GE_MP_PERM_HELD_RIGHT,I32,held_right,10,10,0) \
 X(GE_MP_PERM_HELD_LEFT,I32,held_left,10,10,0) \
 X(GE_MP_PERM_HELD_TIME,I32,held_time,10,10,0) \
 X(GE_MP_PERM_DEATHCOUNT,I32,deathcount,1,1,0) \
 X(GE_MP_PERM_SUICIDES,I32,suicides,1,1,0) \
 X(GE_MP_PERM_KILLS_THIS_LIFE,I32,kills_this_life,1,1,0) \
 X(GE_MP_PERM_LIFE_START,I32,life_start,1,1,0) \
 X(GE_MP_PERM_LAST_KILL_TIME,I32,last_kill_time,4,4,0) \
 X(GE_MP_PERM_SHOT_COUNT,I32,shot_count,7,7,0) \
 X(GE_MP_PERM_KILL_COUNT,I32,kill_count,1,1,0) \
 X(GE_MP_PERM_GG_KILLS,I32,gg_kills,1,1,0) \
 X(GE_MP_PERM_KILL_COUNTS,I32,kill_counts,4,4,0) \
 X(GE_MP_PERM_ON_SCREEN_TIME,I32,on_screen_time,1,1,0) \
 X(GE_MP_PERM_DISTANCE,F32,distance,1,1,0) \
 X(GE_MP_PERM_BACKSIDE_DAMAGE,I32,backside_damage,1,1,0) \
 X(GE_MP_PERM_ARMOUR_PICKUPS,F32,armour_pickups,1,1,0) \
 X(GE_MP_PERM_MIN_KILL_TIME,I32,min_kill_time,1,1,0) \
 X(GE_MP_PERM_MAX_KILL_TIME,I32,max_kill_time,1,1,0) \
 X(GE_MP_PERM_LONGEST_INNING,I32,longest_inning,1,1,0) \
 X(GE_MP_PERM_SHORTEST_INNING,I32,shortest_inning,1,1,0) \
 X(GE_MP_PERM_MOST_KILLED_LIFE,I32,most_killed_life,1,1,0) \
 X(GE_MP_PERM_MOST_KILLED_TIME,I32,most_killed_time,1,1,0) \
 X(GE_MP_PERM_HANDICAP,F32,handicap,1,1,0) \
 X(GE_MP_PERM_FLAG_COUNTER,I32,flag_counter,1,1,0) \
 X(GE_MP_PERM_PERSPECTIVE_HEIGHT,F32,perspective_height,1,1,0) \
 X(GE_MP_PERM_YOLT_ORDER,U8,yolt_order,1,1,0) \
 X(GE_MP_PERM_TEAM_OR_TOKEN,U8,team_or_token,1,1,0) \
 X(GE_MP_PERM_AUTOAIM,U8,autoaim,1,1,0) \
 X(GE_MP_PERM_SIGHT,U8,sight,1,1,0) \
 X(GE_MP_PERM_CIVILIANS,I32,civilians,1,1,0) \
 X(GE_MP_PERM_SCORE,I32,score,0,1,0) \
 X(GE_MP_PERM_TEXT_OVERRIDE_PRESENT,BOOL,text_override_present,1,1,0) \
 X(GE_MP_PERM_RESERVED_CAPACITY,U32,reserved_capacity,1,1,0)

static const GeMpFieldSchema inventory_schema[] = {
    {GE_MP_INV_TYPE,GE_MP_STATE_I32,0,1,1},
    {GE_MP_INV_NEXT,GE_MP_STATE_SLOT_REF,GE_MP_FIELD_NULLABLE,1,1},
    {GE_MP_INV_PREV,GE_MP_STATE_SLOT_REF,GE_MP_FIELD_NULLABLE,1,1},
    {GE_MP_INV_WEAPON,GE_MP_STATE_I32,0,0,1},
    {GE_MP_INV_DUAL_RIGHT,GE_MP_STATE_I32,0,0,1},
    {GE_MP_INV_DUAL_LEFT,GE_MP_STATE_I32,0,0,1},
    {GE_MP_INV_PROP,GE_MP_STATE_ACTOR_REF,0,0,1}
};
#define SCHEMA(id,type,member,min,max,flags) {id,GE_MP_STATE_##type,flags,min,max},
static const GeMpFieldSchema permission_schema[] = {PERMISSION_FIELDS(SCHEMA)};
#undef SCHEMA
_Static_assert(sizeof(permission_schema)/sizeof(permission_schema[0])==GE_MP_PERMISSION_FIELDS,"permission schema count");
static const GeMpRecordSchema schemas[] = {
    {GE_MP_INVENTORY_KIND,GE_MP_SYSTEM_INVENTORY,inventory_schema,GE_MP_INVENTORY_FIELDS},
    {GE_MP_PERMISSION_KIND,GE_MP_SYSTEM_INVENTORY|GE_MP_SYSTEM_PLAYERS|GE_MP_SYSTEM_RULES,permission_schema,GE_MP_PERMISSION_FIELDS}
};
const GeMpRecordSchema *ge_mp_state_inventory_schemas(size_t *count)
{if(count)*count=2;return schemas;}

static uint32_t key(unsigned index)
{
    return index<120?ge_mp_registry_key(GE_MP_INVENTORY_KIND,(uint8_t)(index/30),(uint16_t)(index%30))
        :ge_mp_registry_key(GE_MP_PERMISSION_KIND,(uint8_t)(index-120),0);
}
static GeMpStateStatus registry_ready(const GeMpRegistry *registry)
{
    size_t i;
    if(!registry || !registry->entries || registry->count>registry->capacity
            || registry->capacity>GE_MP_STATE_MAX_RECORDS)return GE_MP_STATE_INVALID;
    for(i=0;i<registry->count;++i) {
        const GeMpRegistryEntry *e=&registry->entries[i];
        if(!(e->key>>24) || e->live>1 || (e->live&&!e->generation)
                || (i && registry->entries[i-1].key>=e->key))return GE_MP_STATE_INVALID;
    }
    return GE_MP_STATE_OK;
}
static const GeMpRegistryEntry *find_key(const GeMpRegistry *registry,uint32_t k)
{
    size_t i;for(i=0;i<registry->count;++i)if(registry->entries[i].key==k)return &registry->entries[i];
    return NULL;
}
GeMpStateStatus ge_mp_state_inventory_begin(GeMpStateInventoryOwner *owner,GeMpRegistry *registry)
{
    GeMpStateInventoryOwner next={0};
    size_t i,missing=0;
    GeMpStateStatus status;
    if(!owner)return GE_MP_STATE_INVALID;
    if(owner->live)return GE_MP_STATE_STALE;
    status=registry_ready(registry);if(status!=GE_MP_STATE_OK)return status;
    next.player_count=ge_original_multiplayer_player_count();
    if(next.player_count<2 || next.player_count>4)return GE_MP_STATE_INCOMPLETE;
    for(i=0;i<4;++i)if(!ge_original_multiplayer_inventory_storage((uint32_t)i,&next.storage[i])
            || next.storage[i].capacity!=30)return GE_MP_STATE_INCOMPLETE;
    /* Preflight all definitions/acquires so capacity/stale/overflow errors
     * cannot strand a partly acquired physical domain. */
    for(i=0;i<GE_MP_INVENTORY_RECORDS;++i) {
        const GeMpRegistryEntry *e=find_key(registry,key((unsigned)i));
        if(!e)++missing;
        else if(e->live)return GE_MP_STATE_STALE;
        else if(e->generation==UINT32_MAX)return GE_MP_STATE_EXHAUSTED;
    }
    if(missing>registry->capacity-registry->count)return GE_MP_STATE_CAPACITY;
    for(i=0;i<GE_MP_INVENTORY_RECORDS;++i) {
        uint32_t k=key((unsigned)i);
        if(!find_key(registry,k)) {
            status=ge_mp_registry_define(registry,k);
            if(status!=GE_MP_STATE_OK)return status;
        }
        status=ge_mp_registry_acquire(registry,k,&next.ids[i]);
        if(status!=GE_MP_STATE_OK)return status;
    }
    next.registry=registry;next.live=1;*owner=next;
    return GE_MP_STATE_OK;
}
GeMpStateStatus ge_mp_state_inventory_end(GeMpStateInventoryOwner *owner)
{
    size_t i;GeMpStateStatus status;
    if(!owner || !owner->live)return GE_MP_STATE_STALE;
    for(i=0;i<GE_MP_INVENTORY_RECORDS;++i) {
        status=ge_mp_registry_resolve(owner->registry,owner->ids[i],NULL);
        if(status!=GE_MP_STATE_OK)return status;
    }
    for(i=0;i<GE_MP_INVENTORY_RECORDS;++i) {
        status=ge_mp_registry_release(owner->registry,owner->ids[i]);
        if(status!=GE_MP_STATE_OK)return status;
    }
    memset(owner,0,sizeof(*owner));return GE_MP_STATE_OK;
}
static int slot_ref(const GeOriginalMultiplayerInventoryStorage *storage,unsigned player,
    const InvItem *item,uint32_t *result)
{
    uintptr_t start=(uintptr_t)storage->items,address=(uintptr_t)item;
    if(!item){*result=0;return 1;}
    if(address<start || address-start>=30*sizeof(InvItem) || (address-start)%sizeof(InvItem))return 0;
    *result=key(player*30+(unsigned)((address-start)/sizeof(InvItem)));return 1;
}
static GeMpStateStatus gap(GeMpStateInventorySnapshot *out,int ref)
{
    if(ref)++out->coverage.unresolved_references;else ++out->coverage.unclassified_fields;
    out->record_count=0;return GE_MP_STATE_INCOMPLETE;
}
static int score_safe(unsigned count)
{
    int64_t total=0;
    unsigned i,j;
    const int mode=get_scenario();
    if(mode<SCENARIO_NORMAL || mode>SCENARIO_2v1)return 0;
    for(i=0;i<count;++i)for(j=0;j<count;++j) {
        int64_t x=g_playerPlayerData[i].kill_counts[j];total+=x<0?-x:x;
    }
    if(total>INT32_MAX)return 0;
    if(mode==SCENARIO_NORMAL || mode==SCENARIO_MWTGG || mode==SCENARIO_LTK)
        for(i=0;i<count;++i) {
            int64_t x=g_playerPlayerData[i].killed_gg_owner_count;
            if(total+(x<0?-x:x)*(count-2)>INT32_MAX)return 0;
        }
    return 1;
}

GeMpStateStatus ge_mp_state_inventory_export(const GeMpStateInventoryOwner *owner,
    const GeMpStateInventoryProviders *providers,GeMpStateInventorySnapshot *out)
{
    unsigned p,s,i;GeMpStateStatus status;
    if(!out)return GE_MP_STATE_INVALID;
    memset(out,0,sizeof(*out));
    if(!owner || !owner->live)return GE_MP_STATE_STALE;
    if(!providers || !providers->prop_actor)return gap(out,1);
    if(ge_original_multiplayer_player_count()!=owner->player_count)return GE_MP_STATE_STALE;
    if(getPlayerCount()!=(s32)owner->player_count)return gap(out,0);
    for(i=0;i<GE_MP_INVENTORY_RECORDS;++i) {
        status=ge_mp_registry_resolve(owner->registry,owner->ids[i],NULL);
        if(status!=GE_MP_STATE_OK)return status;
    }
    if(!score_safe(owner->player_count))return gap(out,0);
    for(p=0;p<4;++p) {
        GeOriginalMultiplayerInventoryStorage storage;
        const struct player *player;
        const struct player_data *data=&g_playerPlayerData[p];
        GeMpPermissionValues *v=&out->permission[p];
        GeMpStateField *fields=out->permission_fields[p];
        uint32_t active_count=0,free_count=0,head;
        uint8_t seen[30]={0};
        const InvItem *cursor;
        if(!ge_original_multiplayer_inventory_storage(p,&storage)
                || storage.items!=owner->storage[p].items || storage.player!=owner->storage[p].player
                || storage.lifetime!=owner->storage[p].lifetime || storage.capacity!=30
                || storage.active_player!=(p<owner->player_count))return GE_MP_STATE_STALE;
        player=storage.player;
        if(storage.active_player && (g_playerPointers[p]!=player
                || player->p_itemcur!=storage.items || player->equipmaxitems!=30))return gap(out,1);
        if(!storage.active_player && (g_playerPointers[p] || player->p_itemcur || player->equipmaxitems))return gap(out,0);
        if(!slot_ref(&storage,p,player->p_itemcur,&v->base)
                || !slot_ref(&storage,p,player->ptr_inventory_first_in_cycle,&v->head))return gap(out,1);
        v->active=storage.active_player;v->reserved_capacity=storage.capacity;
        cursor=player->ptr_inventory_first_in_cycle;
        head=v->head;
        while(cursor) {
            uint32_t next,prev,k;
            if(!slot_ref(&storage,p,cursor,&k) || !k || active_count>=30)return gap(out,1);
            s=k&0xffffU;if(seen[s])return gap(out,0);seen[s]=1;
            if(cursor->type!=INV_ITEM_WEAPON && cursor->type!=INV_ITEM_DUAL && cursor->type!=INV_ITEM_PROP)return gap(out,0);
            if(!slot_ref(&storage,p,cursor->next,&next) || !next
                    || !slot_ref(&storage,p,cursor->prev,&prev) || !prev)return gap(out,1);
            if(cursor->next->prev!=cursor || cursor->prev->next!=cursor)return gap(out,0);
            v->active_order[active_count++]=k;
            if(next==head)break;
            cursor=cursor->next;
        }
        for(s=0;s<30;++s) {
            const InvItem *item=&storage.items[s];
            unsigned n=p*30+s;
            GeMpInventoryValues *x=&out->inventory[n];
            GeMpStateField *f=out->inventory_fields[n];
            x->type=item->type;
            if(!slot_ref(&storage,p,item->next,&x->next) || !slot_ref(&storage,p,item->prev,&x->prev))return gap(out,1);
            if(item->type==-1) {
                if(s<(unsigned)player->equipmaxitems)v->free_order[free_count++]=key(n);
            } else if(item->type!=INV_ITEM_NONE) {
                if(!seen[s])return gap(out,0);
                if(item->type==INV_ITEM_WEAPON) {
                    x->weapon=item->type_inv_item.type_weap.weapon;
                    if(x->weapon<ITEM_UNARMED || x->weapon>=ITEM_IDS_MAX)return gap(out,0);
                }
                else if(item->type==INV_ITEM_DUAL) {
                    x->right=item->type_inv_item.type_dual.weapon_right;x->left=item->type_inv_item.type_dual.weapon_left;
                    if(x->right<ITEM_UNARMED || x->right>=ITEM_IDS_MAX
                            || x->left<ITEM_UNARMED || x->left>=ITEM_IDS_MAX)return gap(out,0);
                } else if(item->type==INV_ITEM_PROP) {
                    const void *prop=item->type_inv_item.type_prop.prop;
                    if(!prop || providers->prop_actor(providers->context,prop,&x->prop)!=GE_MP_STATE_OK
                            || !x->prop || ge_mp_registry_resolve(owner->registry,x->prop,NULL)!=GE_MP_STATE_OK)return gap(out,1);
                } else return gap(out,0);
            }
            f[0]=(GeMpStateField){GE_MP_INV_TYPE,GE_MP_STATE_I32,1,&x->type};
            f[1]=(GeMpStateField){GE_MP_INV_NEXT,GE_MP_STATE_SLOT_REF,1,&x->next};
            f[2]=(GeMpStateField){GE_MP_INV_PREV,GE_MP_STATE_SLOT_REF,1,&x->prev};
            f[3]=(GeMpStateField){GE_MP_INV_WEAPON,GE_MP_STATE_I32,item->type==INV_ITEM_WEAPON,&x->weapon};
            f[4]=(GeMpStateField){GE_MP_INV_DUAL_RIGHT,GE_MP_STATE_I32,item->type==INV_ITEM_DUAL,&x->right};
            f[5]=(GeMpStateField){GE_MP_INV_DUAL_LEFT,GE_MP_STATE_I32,item->type==INV_ITEM_DUAL,&x->left};
            f[6]=(GeMpStateField){GE_MP_INV_PROP,GE_MP_STATE_ACTOR_REF,item->type==INV_ITEM_PROP,&x->prop};
            out->records[n]=(GeMpStateRecord){owner->ids[n],GE_MP_INVENTORY_KIND,f,GE_MP_INVENTORY_FIELDS};
        }
        v->capacity=player->equipmaxitems;v->all_guns=player->equipallguns;v->current_item=player->equipcuritem;
        for(i=0;i<30;++i)v->ammo[i]=player->ammoheldarr[i];
        for(i=0;i<10;++i) {v->held_right[i]=player->gunheldarr[i].weapon1;v->held_left[i]=player->gunheldarr[i].weapon2;v->held_time[i]=player->gunheldarr[i].totaltime;}
        v->deathcount=player->deathcount;v->suicides=player->num_suicides;v->kills_this_life=player->kills_this_life;v->life_start=player->lifestarttime60;
        for(i=0;i<4;++i){v->last_kill_time[i]=player->last_kill_time[i];v->kill_counts[i]=data->kill_counts[i];}
        for(i=0;i<7;++i)v->shot_count[i]=data->shot_count[i];
#define COPY(dst,src) v->dst=data->src
        COPY(kill_count,kill_count);COPY(gg_kills,killed_gg_owner_count);COPY(on_screen_time,time_other_players_on_screen);
        COPY(distance,distance_traveled);COPY(backside_damage,damage_to_backside);COPY(armour_pickups,body_armor_pickups);
        COPY(min_kill_time,min_time_between_kills);COPY(max_kill_time,max_time_between_kills);
        COPY(longest_inning,longest_inning);COPY(shortest_inning,shortest_inning);
        COPY(most_killed_life,most_killed_one_life);COPY(most_killed_time,most_killed_one_time);
        COPY(handicap,handicap);COPY(flag_counter,flag_counter);COPY(perspective_height,player_perspective_height);
        COPY(yolt_order,order_out_in_yolt);COPY(team_or_token,have_token_or_goldengun);
        COPY(autoaim,autoaim);COPY(sight,sight);COPY(civilians,killed_civilians);
#undef COPY
        if(!isfinite(v->distance) || !isfinite(v->armour_pickups) || !isfinite(v->handicap)
                || !isfinite(v->perspective_height))return gap(out,0);
        v->text_override_present=player->textoverrides!=NULL;
        if(v->text_override_present)return gap(out,0);
        if(storage.active_player && ge_original_multiplayer_score((int32_t)p,&v->score)!=GE_ORIGINAL_MP_OK)return gap(out,0);
        i=0;
#define FIELD(id,type,member,min,max,flags) fields[i++]=(GeMpStateField){id,GE_MP_STATE_##type,max,&v->member};
        PERMISSION_FIELDS(FIELD)
#undef FIELD
        fields[0].count=active_count;fields[1].count=free_count;
        fields[GE_MP_PERMISSION_FIELDS-3].count=storage.active_player;
        out->records[120+p]=(GeMpStateRecord){owner->ids[120+p],GE_MP_PERMISSION_KIND,fields,GE_MP_PERMISSION_FIELDS};
    }
    out->record_count=GE_MP_INVENTORY_RECORDS;
    out->coverage.enumerated_systems=GE_MP_SYSTEM_INVENTORY;
    return GE_MP_STATE_OK;
}
