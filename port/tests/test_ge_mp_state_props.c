#include "ge_mp_state_props.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/chrai.h"
#include "game/explosion.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

struct player *g_playerPointers[4];
static struct player players[4];
static ChrRecord characters[4];
static ObjectRecord objects[600];
static StandTile tile;
static DoorRecord door;
static WeaponObjRecord weapon;
static struct Explosion explosion;
static struct Smoke smoke;
static uint64_t variant_ids[4];
static GeOriginalPropState native;
static GeMpStatePropsOwner owner;
static GeMpRegistry registry;
static GeMpRegistryEntry entries[1250];
static uint64_t object_ids[600],character_ids[4],player_ids[4],tile_id;
static GeMpStatePropsSnapshot output;
extern PropRecord *g_FreeProps,*g_ActivePropsHead,*g_ActivePropsTail;
extern PropRecord g_Props[600];
extern void chrpropFree(PropRecord *);
extern void chrpropDetach(PropRecord *);
void fixture_guard_grenade_object_rollback(void *);
void fixture_covert_modem_object_rollback(void *);
static GeMpStateStatus reference(void *ctx,GeMpPropReferenceKind kind,const void *ptr,uint64_t *id)
{
    (void)ctx;
    if(kind==GE_MP_PROP_REF_STAN && ptr==&tile){*id=tile_id;return GE_MP_STATE_OK;}
    for(unsigned i=0;i<4;++i) {
        if(kind==GE_MP_PROP_REF_PLAYER && ptr==&players[i]){*id=player_ids[i];return GE_MP_STATE_OK;}
        if(kind==GE_MP_PROP_REF_CHARACTER && ptr==&characters[i]){*id=character_ids[i];return GE_MP_STATE_OK;}
    }
    if(kind==GE_MP_PROP_REF_DOOR && ptr==&door){*id=variant_ids[0];return GE_MP_STATE_OK;}
    if(kind==GE_MP_PROP_REF_WEAPON && ptr==&weapon){*id=variant_ids[1];return GE_MP_STATE_OK;}
    if(kind==GE_MP_PROP_REF_EXPLOSION && ptr==&explosion){*id=variant_ids[2];return GE_MP_STATE_OK;}
    if(kind==GE_MP_PROP_REF_SMOKE && ptr==&smoke){*id=variant_ids[3];return GE_MP_STATE_OK;}
    if(kind==GE_MP_PROP_REF_OBJECT)
        for(unsigned i=0;i<600;++i)if(ptr==&objects[i]){*id=object_ids[i];return GE_MP_STATE_OK;}
    return GE_MP_STATE_INCOMPLETE;
}
static const GeMpStatePropsProviders providers={NULL,reference};
static uint64_t resource(unsigned kind,unsigned slot)
{
    uint64_t id;uint32_t key=ge_mp_registry_key((uint8_t)kind,255,(uint16_t)slot);
    assert(ge_mp_registry_define(&registry,key)==GE_MP_STATE_OK);
    assert(ge_mp_registry_acquire(&registry,key,&id)==GE_MP_STATE_OK);return id;
}
static void start(void)
{
    assert(ge_original_prop_state_reset(&native,16));
    assert(ge_mp_registry_init(&registry,entries,1250)==GE_MP_STATE_OK);
    for(unsigned i=0;i<600;++i)object_ids[i]=resource(0x10,i);
    for(unsigned i=0;i<4;++i){character_ids[i]=resource(0x11,i);player_ids[i]=resource(0x12,i);g_playerPointers[i]=&players[i];}
    tile_id=resource(0x13,0);
    for(unsigned i=0;i<4;++i)variant_ids[i]=resource(0x14,i);
    assert(ge_mp_state_props_begin(&owner,&registry)==GE_MP_STATE_OK);
    assert(ge_mp_state_props_begin(&owner,&registry)==GE_MP_STATE_STALE);
}
static PropRecord *allocate(void)
{
    PropRecord *p=chrpropAllocate();assert(p);
    unsigned i=(unsigned)(p-g_Props);
    p->type=PROP_TYPE_OBJ;p->obj=&objects[i];objects[i].prop=p;
    p->pos.x=(float)i;p->pos.y=2;p->pos.z=3;p->zDepth=4;p->unk30=(s32)i;
    return p;
}
static void export_ok(void)
{
    assert(ge_mp_state_props_export(&owner,&providers,&output)==GE_MP_STATE_OK);
    assert(output.coverage.enumerated_systems==GE_MP_SYSTEM_PROPS);
    assert(!output.coverage.unclassified_fields && !output.coverage.unresolved_references);
    size_t count;const GeMpRecordSchema *schemas=ge_mp_state_props_schemas(&count);assert(count==2);
    for(size_t i=0;i<output.record_count;++i) {
        const GeMpStateRecord *r=&output.records[i];const GeMpRecordSchema *s=&schemas[r->kind==GE_MP_PROPS_KIND?0:1];
        assert(r->kind==s->kind && r->field_count==s->field_count);
        if(i)assert(output.records[i-1].actor_id<r->actor_id);
        for(size_t j=0;j<r->field_count;++j) {
            assert(r->fields[j].id==s->fields[j].id && r->fields[j].type==s->fields[j].type);
            assert(r->fields[j].count>=s->fields[j].min_count && r->fields[j].count<=s->fields[j].max_count);
        }
    }
}
/* Fixture-only typed payload comparison, never a complete state digest. */
static void fingerprint(uint8_t digest[32])
{
    static unsigned char bytes[400000];
    static const unsigned width[]={0,1,2,4,8,1,2,4,8,4,1,8,4};
    size_t used=0;export_ok();
    for(size_t r=0;r<output.record_count;++r) {
        const GeMpStateRecord *record=&output.records[r];
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
static void release(PropRecord *p)
{
    if(p->parent)chrpropDetach(p);
    else if(ge_original_prop_state_is_active(p))chrpropDelist(p);
    chrpropDeregisterRooms(p);chrpropDisable(p);chrpropFree(p);
}
static void end(void)
{
    for(unsigned i=0;i<600;++i)if(owner.ids[i])release(&g_Props[i]);
    assert(ge_mp_state_props_end(&owner)==GE_MP_STATE_OK);
}
static void lifetime(void)
{
    start();export_ok();assert(output.record_count==1 && output.pool_fields[1].count==600);
    PropRecord *p=allocate();uint64_t first=owner.ids[0],resolved=0;
    assert(first && ge_mp_state_props_actor(&owner,p,&resolved)==GE_MP_STATE_OK && resolved==first);
    /* Free and reallocate between snapshots: generation still must advance. */
    release(p);p=allocate();assert(p==&g_Props[0] && owner.ids[0]==first+1);
    assert(ge_mp_registry_resolve(&registry,first,NULL)==GE_MP_STATE_STALE);
    fixture_guard_grenade_object_rollback(p);assert(!owner.ids[0]);
    p=allocate();assert(owner.ids[0]==first+2);
    fixture_covert_modem_object_rollback(p);assert(!owner.ids[0]);
    p=allocate();assert(owner.ids[0]==first+3);
    chrpropActivate(p);chrpropEnable(p);p->timetoregen=1200;chrpropDisable(p);
    export_ok();assert(output.props[0].timer==1200 && !(output.props[0].flags&PROPFLAG_ENABLED));
    assert(output.pool_fields[0].count==1 && output.pool.active[0]==owner.ids[0]);
    release(p);p->voidp=(void *)(uintptr_t)1; /* A freed union is never dereferenced or exported. */
    p->pos.x=72;p->zDepth=-0.0f;p->unk30=-42;p->rooms[3]=77;
    export_ok();assert(output.pool.free_position[0]==72 && output.pool.free_unk30[0]==-42 && output.pool.free_rooms[3]==77);
    assert(ge_mp_state_props_actor(&owner,p,&resolved)==GE_MP_STATE_INCOMPLETE);
    assert(ge_mp_state_props_end(&owner)==GE_MP_STATE_OK);
    /* Late begin cannot silently erase an allocate/free history. */
    assert(ge_mp_state_props_begin(&owner,&registry)==GE_MP_STATE_INCOMPLETE);
}
static void pool_and_rooms(void)
{
    start();
    PropRecord *p[600];
    for(unsigned i=0;i<600;++i){p[i]=allocate();chrpropActivate(p[i]);}
    assert(!chrpropAllocate());export_ok();assert(output.record_count==601 && output.pool_fields[1].count==0);
    assert(output.pool.active[0]==owner.ids[0] && output.pool.active[599]==owner.ids[599]);
    for(unsigned i=0;i<16;++i){assert(ge_original_prop_state_set_primary_room(p[i],2));chrpropRegisterRoom(p[i],2);}
    export_ok();assert(output.pool.room_counts[2]==16 && output.pool.room_heads[2]==0);
    assert(output.pool.block_next[0]==1 && output.pool.block_members[15]==ge_mp_registry_key(GE_MP_PROPS_KIND,255,15));
    chrpropDeregisterRoom(p[2],2);export_ok();assert(!output.pool.block_members[2] && output.pool.room_counts[2]==15);
    chrpropRegisterRoom(p[16],2);export_ok();assert(output.pool.block_members[2]==ge_mp_registry_key(GE_MP_PROPS_KIND,255,16));
    assert(output.pool.room_members[2]==owner.ids[16]);
    /* Child list uses parent->child as newest tail, then prev. */
    chrpropDelist(p[18]);chrpropDelist(p[19]);chrpropReparent(p[18],p[17]);chrpropReparent(p[19],p[17]);
    export_ok();assert(output.prop_fields[17][0].count==2);
    const uint64_t *children=output.prop_fields[17][0].values;
    assert(children[0]==owner.ids[19] && children[1]==owner.ids[18]);
    chrpropDetach(p[19]);chrpropActivate(p[19]);export_ok();assert(output.prop_fields[17][0].count==1);
    /* Release children before their parent during the canonical teardown. */
    release(p[18]);
    /* p16 registered externally above: publish room list before deregister. */
    assert(ge_original_prop_state_set_primary_room(p[16],2));
    end();
}
static void player_and_fields(unsigned count)
{
    start();
    PropRecord *p[4];
    for(unsigned i=0;i<count;++i) {
        p[i]=allocate();p[i]->type=PROP_TYPE_VIEWER;p[i]->chr=&characters[i];players[i].prop=p[i];
        p[i]->stan=&tile;chrpropActivate(p[i]);
    }
    export_ok();
    for(unsigned i=0;i<count;++i) {
        assert(output.props[i].refs[3]==character_ids[i] && output.props[i].refs[11]==player_ids[i]);
        assert(output.props[i].refs[6]==tile_id);
        p[i]->flags=0x9f;p[i]->timetoregen=-17;p[i]->unk30=INT_MIN;
        p[i]->pos.x=-0.0f;p[i]->pos.y=19.25f;p[i]->pos.z=-4;p[i]->zDepth=77;
        p[i]->rooms[3]=23;export_ok();
        assert(output.props[i].flags==0x9f && output.props[i].timer==-17 && output.props[i].unk30==INT_MIN);
        uint32_t bits;memcpy(&bits,&output.props[i].position[0],4);assert(bits==0x80000000U);
        assert(output.props[i].position[1]==19.25f && output.props[i].position[2]==-4 && output.props[i].zdepth==77 && output.props[i].rooms[3]==23);
        p[i]->flags=0;
    }
    end();memset(players,0,sizeof(players));
}
static void typed_mutations_and_variants(void)
{
    start();PropRecord *p=allocate();chrpropActivate(p);uint8_t before[32],after[32];
#define CHANGE(expr) do { __typeof__(expr) saved=(expr);fingerprint(before);(expr)=saved+1;fingerprint(after);assert(memcmp(before,after,32));(expr)=saved; } while(0)
    CHANGE(p->flags);CHANGE(p->timetoregen);CHANGE(p->unk30);
    CHANGE(p->pos.x);CHANGE(p->pos.y);CHANGE(p->pos.z);CHANGE(p->zDepth);
    for(unsigned i=1;i<4;++i)CHANGE(p->rooms[i]);
    p->rooms[0]=1;p->rooms[1]=255;CHANGE(p->rooms[0]);p->rooms[0]=255;p->rooms[1]=0;
    fingerprint(before);p->obj=&objects[1];fingerprint(after);assert(memcmp(before,after,32));p->obj=&objects[0];
    fingerprint(before);p->stan=&tile;fingerprint(after);assert(memcmp(before,after,32));p->stan=NULL;
    p->type=PROP_TYPE_DOOR;p->door=&door;export_ok();assert(output.props[0].refs[1]==variant_ids[0]);
    p->type=PROP_TYPE_WEAPON;p->weapon=&weapon;export_ok();assert(output.props[0].refs[2]==variant_ids[1]);
    p->type=PROP_TYPE_EXPLOSION;p->explosion=&explosion;export_ok();assert(output.props[0].refs[4]==variant_ids[2]);
    p->type=PROP_TYPE_SMOKE;p->smoke=&smoke;export_ok();assert(output.props[0].refs[5]==variant_ids[3]);
    p->type=PROP_TYPE_CHR;p->chr=&characters[0];export_ok();assert(output.props[0].refs[3]==character_ids[0]);
    release(p);
    CHANGE(p->type);CHANGE(p->pos.x);CHANGE(p->pos.y);CHANGE(p->pos.z);CHANGE(p->zDepth);CHANGE(p->unk30);
    for(unsigned i=0;i<4;++i)CHANGE(p->rooms[i]);
#undef CHANGE
    fingerprint(before);p->voidp=(void *)(uintptr_t)1;fingerprint(after);assert(!memcmp(before,after,32));
    /* Complete native coverage is deliberately not attested by this domain. */
    GeMpStateSchema schema={0};GeMpCanonicalSnapshot snapshot={0};GeMpCoverageReport report;
    size_t count;const GeMpRecordSchema *domain=ge_mp_state_props_schemas(&count);
    GeMpRecordSchema schemas[3]={{0,GE_MP_SYSTEM_SESSION,NULL,0},domain[0],domain[1]};
    schema.version=1;schema.registered_systems=GE_MP_SYSTEM_SESSION|GE_MP_SYSTEM_PROPS|GE_MP_SYSTEM_OBJECTS;
    schema.records=schemas;schema.record_count=3;
    snapshot.session=1;snapshot.enabled_systems=schema.registered_systems;snapshot.schema=&schema;
    memset(snapshot.build_sha256,1,32);memset(snapshot.content_sha256,2,32);memset(snapshot.rules_sha256,3,32);
    snapshot.registry=&registry;snapshot.records=output.records;snapshot.record_count=output.record_count;
    snapshot.coverage=output.coverage;memset(before,0x5a,32);memcpy(after,before,32);
    assert(ge_mp_state_sha256(&snapshot,after,&report)==GE_MP_STATE_INCOMPLETE && !memcmp(before,after,32));
    end();
}
static void failures(void)
{
    start();PropRecord *p=allocate();chrpropActivate(p);
    assert(ge_mp_state_props_export(&owner,NULL,&output)==GE_MP_STATE_INCOMPLETE);
    p->obj=(void *)(uintptr_t)1;
    assert(ge_mp_state_props_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE && !output.record_count);
    p->obj=&objects[0];p->type=PROP_TYPE_PLAYER;
    assert(ge_mp_state_props_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);
    p->type=PROP_TYPE_OBJ;p->pos.x=NAN;
    assert(ge_mp_state_props_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);
    p->pos.x=0;p->next=p;
    assert(ge_mp_state_props_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);p->next=NULL;
    memset(p->rooms,0,sizeof(p->rooms));
    assert(ge_mp_state_props_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);p->rooms[0]=255;
    end();
    start();p=allocate();
    /* Simulate a missed free callback while preserving the native free body.
     * Reuse is detected at the next allocation, even with no snapshot gap. */
    p->prev=g_FreeProps;p->next=NULL;p->stan=NULL;g_FreeProps=p;
    (void)allocate();assert(owner.fault!=GE_MP_STATE_OK);
    assert(ge_mp_state_props_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);
    assert(ge_mp_state_props_end(&owner)==GE_MP_STATE_INCOMPLETE);
    start();p=allocate();assert(ge_original_prop_state_reset(&native,16));
    assert(ge_mp_state_props_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);
    assert(ge_mp_state_props_end(&owner)==GE_MP_STATE_INCOMPLETE);
    start();
    for(size_t i=0;i<registry.count;++i)if(registry.entries[i].key==ge_mp_registry_key(GE_MP_PROPS_KIND,255,0))registry.entries[i].generation=UINT32_MAX;
    p=allocate();assert(p==&g_Props[0] && owner.fault==GE_MP_STATE_EXHAUSTED);
    assert(ge_mp_state_props_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);
    assert(ge_mp_state_props_end(&owner)==GE_MP_STATE_INCOMPLETE);
}
int main(void)
{
    lifetime();pool_and_rooms();player_and_fields(2);player_and_fields(4);typed_mutations_and_variants();failures();
    puts("native prop domain: exact 600 pool allocation/free generations,disabled timers,room block holes/order,children,2/4 viewer refs,free scalar classification and missed lifecycle failures passed");
    return 0;
}
