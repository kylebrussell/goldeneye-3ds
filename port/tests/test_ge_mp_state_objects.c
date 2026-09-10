#include "ge_mp_state_objects.h"
#include "ge_original_stage_prop_materializer.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/chrai.h"
#include "game/propobj.h"
#include "game/model.h"
#include "game/explosion.h"
#include "snd.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RUNTIMEBITFLAG_EMBEDDED (1u << 6)
#define RUNTIMEBITFLAG_HASPROJECTILE (1u << 7)

extern u32 fixture_authored_words[];
size_t fixture_authored_count(void);
void fixture_armour_setup(BodyArmourRecord *);
TICKOP fixture_armour_pickup(PropRecord *,bool);
void fixture_object_regen(PropRecord *);
void objFreePermanently(ObjectRecord *,bool);
void propobjSetDropped(PropRecord *,DROPTYPE);
AmmoCrateRecord *ammocrateAllocate(void);
void chrpropFree(PropRecord *);
void chrpropDetach(PropRecord *);
extern PropRecord g_Props[600];
struct player *g_CurrentPlayer,*g_playerPointers[4];
struct player_data *g_playerPerm;
s32 g_ClockTimer=1;
void *g_musicSfxBufferPtr;
static struct player players[4];
static struct player_data perms[4];
static PropRecord player_props[4]; /* proximity inputs, not native pool actors */
static unsigned player_count,audio_calls,room_calls;
static GeOriginalPropState pool;
static GeMpRegistry registry;
static GeMpRegistryEntry entries[1400];
static GeMpStatePropsOwner props;
static GeMpStateObjectsOwner owner;
static GeMpStateObjectsSnapshot output;
static GeMpStatePropsSnapshot prop_output;
static WeaponObjRecord weapon,linked;
static MultiAmmoCrateRecord multi;
static BodyArmourRecord armour;
static Model models[24];
static ModelFileHeader model_file; /* explicit empty model resource */
static struct BulletImpact impacts[100];
static struct collision_data collision;
static Embedment embedded;
static uint64_t model_ids[24],projectile_ids[PROJECTILES_ARR_MAX],collision_id,embedded_id;
static GeMpObjectDefinition definitions[24];
static GeOriginalStagePropConstructionRequest requests[3];
static GeOriginalStagePropRecord records[3];
static uint64_t resource(unsigned kind,unsigned slot)
{
    uint64_t id;uint32_t key=ge_mp_registry_key(kind,255,slot);
    assert(ge_mp_registry_define(&registry,key)==GE_MP_STATE_OK);
    assert(ge_mp_registry_acquire(&registry,key,&id)==GE_MP_STATE_OK);return id;
}
static GeMpStateStatus reference(void *context,GeMpObjectReferenceKind kind,const void *p,uint64_t *id)
{
    (void)context;
    if(kind==GE_MP_OBJECT_REF_MODEL)for(unsigned i=0;i<24;++i)if(p==&models[i]){*id=model_ids[i];return GE_MP_STATE_OK;}
    if(kind==GE_MP_OBJECT_REF_PROJECTILE)for(unsigned i=0;i<PROJECTILES_ARR_MAX;++i)if(p==&g_Projectiles[i]){*id=projectile_ids[i];return GE_MP_STATE_OK;}
    if(kind==GE_MP_OBJECT_REF_COLLISION && p==&collision){*id=collision_id;return GE_MP_STATE_OK;}
    if(kind==GE_MP_OBJECT_REF_EMBEDMENT && p==&embedded){*id=embedded_id;return GE_MP_STATE_OK;}
    return GE_MP_STATE_INCOMPLETE;
}
static const GeMpStateObjectsProviders providers={NULL,reference};
static GeMpStateStatus prop_reference(void *context,GeMpPropReferenceKind kind,const void *p,uint64_t *id)
{
    (void)context;
    if(kind==GE_MP_PROP_REF_OBJECT || kind==GE_MP_PROP_REF_WEAPON)return ge_mp_state_objects_actor(&owner,p,id);
    return GE_MP_STATE_INCOMPLETE;
}
static const GeMpStatePropsProviders prop_providers={NULL,prop_reference};
static void export_ok(void)
{
    assert(ge_mp_state_objects_export(&owner,&providers,&output)==GE_MP_STATE_OK);
    assert(output.record_count==24 && output.coverage.enumerated_systems==GE_MP_SYSTEM_OBJECTS);
    assert(!output.coverage.unclassified_fields && !output.coverage.unresolved_references);
    const GeMpRecordSchema *schema=ge_mp_state_objects_schema();
    for(size_t i=0;i<24;++i)for(size_t f=0;f<GE_MP_OBJECTS_FIELDS;++f) {
        const GeMpStateField *v=&output.fields[i][f];
        assert(v->id==schema->fields[f].id && v->type==schema->fields[f].type);
        assert(v->count>=schema->fields[f].min_count && v->count<=schema->fields[f].max_count);
    }
}
/* Empty GPU model resource is sufficient for the unchanged free traversal.
 * Every node/door/vehicle/owned-setup path is an assertion, not a gameplay stub. */
s32 getPlayerCount(void){return player_count;}
PropRecord *getCurrentPlayerProp(void){return g_CurrentPlayer->prop;}
ALSoundState *sndPlaySfx(struct ALBankAlt_s *bank,s16 sound,ALSoundState *pending)
{assert(!bank && !pending);assert(sound==ARMOUR_COLLECT_SFX || sound==OBJ_REGEN_SFX);++audio_calls;return NULL;}
u8 sndGetPlayingState(ALSoundState *s){(void)s;abort();}
void sndDeactivate(ALSoundState *s){(void)s;abort();}
void chrobjSndCreatePostEventDefault(ALSoundState *s,coord3d *position)
{assert(!s && position);}
void bgToggleDataPortalsContrlBytes1Bit1(s32 i,s32 b){(void)i;(void)b;abort();}
void clear_aircraft_model_obj(Model *m){(void)m;abort();}
union ModelRwData *modelGetNodeRwData(Model *m,ModelNode *n){(void)m;(void)n;abort();}
bool sub_GAME_7F04B590(ModelFileHeader *m,ModelNode *n){(void)m;(void)n;abort();}
void sub_GAME_7F09C044(Vertex *v){(void)v;abort();}
void modelApplyDistanceRelations(Model *m,ModelNode *n){(void)m;(void)n;abort();}
void modelApplyToggleRelations(Model *m,ModelNode *n){(void)m;(void)n;abort();}
void modelApplyHeadRelations(Model *m,ModelNode *n){(void)m;(void)n;abort();}
void modelSetScale(Model *m,f32 scale){(void)m;(void)scale;abort();}
void chrSetFiring(ChrRecord *c,s32 hand,bool firing){(void)c;(void)hand;(void)firing;abort();}
s32 setupGetCommandIndexByProp(PropRecord *p){(void)p;abort();}
ObjectRecord *setupCommandGetObject(s32 stage,s32 i){(void)stage;(void)i;abort();}
s32 lvlGetCurrentStageToLoad(void){abort();}
/* Explicit fixed room-resource query after native regeneration enables prop.
 * Actual room registration/list mechanics remain source owned. */
void sub_GAME_7F03E134(PropRecord *p){assert(p && p->obj);++room_calls;chrpropRegisterRoom(p,1);}

static void promote_authored(void)
{
    size_t words=fixture_authored_count(),offset=0,found=0;
    memset(requests,0,sizeof(requests));memset(records,0,sizeof(records));
    for(unsigned command=0;offset<words;++command) {
        unsigned type=fixture_authored_words[offset]&255;
        if(type==PROPDEF_END)break;
        assert(type==PROPDEF_COLLECTABLE || type==PROPDEF_AMMO || type==PROPDEF_ARMOUR);
        size_t length=type==PROPDEF_AMMO?45:34;
        assert(offset+length<=words);
        if(command==0 || command==2 || command==25) {
            assert(found<3);records[found].words=&fixture_authored_words[offset];
            records[found].word_count=length;records[found].type=type;
            requests[found].record=&records[found];requests[found].command_index=command;
            requests[found].service=GE_ORIGINAL_STAGE_PROP_SERVICE_DEFAULT_OBJECT;
            void *d=found==0?(void *)&weapon:found==1?(void *)&multi:(void *)&armour;
            assert(ge_original_stage_prop_native_definition_init(&requests[found],d,ge_mp_state_objects_native_size(type)));
            ++found;
            if(found==3)break;
        }
        offset+=length;
    }
    assert(found==3 && weapon.type==PROPDEF_COLLECTABLE && multi.type==PROPDEF_AMMO && armour.type==PROPDEF_ARMOUR);
    fixture_armour_setup(&armour);assert(armour.initialamount==1.0f && armour.amount==1.0f);
}
static PropRecord *bind(ObjectRecord *obj,unsigned model_slot)
{
    PropRecord *p=chrpropAllocate();assert(p);
    /* Constructor resource boundary: these exact native objects have a model
     * and pool slot supplied, with placement at the single fixture room. */
    obj->prop=p;obj->model=&models[model_slot];models[model_slot].obj=&model_file;
    p->obj=obj;p->type=obj->type==PROPDEF_COLLECTABLE?PROP_TYPE_WEAPON:PROP_TYPE_OBJ;
    chrpropActivate(p);chrpropEnable(p);chrpropRegisterRoom(p,1);return p;
}
static void begin(unsigned count)
{
    player_count=count;audio_calls=room_calls=0;g_ClockTimer=1;
    memset(&weapon,0,sizeof(weapon));memset(&linked,0,sizeof(linked));
    memset(&multi,0,sizeof(multi));memset(&armour,0,sizeof(armour));
    memset(g_AmmoCrates,0,MAX_AMMO_CRATES*sizeof(*g_AmmoCrates));memset(models,0,sizeof(models));
    memset(players,0,sizeof(players));memset(perms,0,sizeof(perms));
    memset(player_props,0,sizeof(player_props));memset(impacts,0,sizeof(impacts));
    g_BulletImpactBuffer=impacts;
    for(unsigned i=0;i<4;++i){players[i].prop=&player_props[i];g_playerPointers[i]=&players[i];player_props[i].pos.x=1000+(float)i;}
    g_CurrentPlayer=&players[0];g_playerPerm=&perms[0];
    for(unsigned i=0;i<PROJECTILES_ARR_MAX;++i){memset(&g_Projectiles[i],0,sizeof(g_Projectiles[i]));g_Projectiles[i].flags=PROJECTILEFLAG_FREE;}
    assert(ge_original_prop_state_reset(&pool,4));
    assert(ge_mp_registry_init(&registry,entries,1400)==GE_MP_STATE_OK);
    assert(ge_mp_state_props_begin(&props,&registry)==GE_MP_STATE_OK);
    for(unsigned i=0;i<24;++i)model_ids[i]=resource(0x10,i);
    for(unsigned i=0;i<PROJECTILES_ARR_MAX;++i)projectile_ids[i]=resource(0x11,i);
    collision_id=resource(0x12,0);embedded_id=resource(0x13,0);
    promote_authored();
    definitions[0]=(GeMpObjectDefinition){&weapon,sizeof(weapon),0,PROPDEF_COLLECTABLE,0};
    definitions[1]=(GeMpObjectDefinition){&multi,sizeof(multi),0,PROPDEF_AMMO,2};
    definitions[2]=(GeMpObjectDefinition){&armour,sizeof(armour),0,PROPDEF_ARMOUR,25};
    definitions[3]=(GeMpObjectDefinition){&linked,sizeof(linked),1,PROPDEF_COLLECTABLE,0};
    assert(ge_mp_state_objects_native_ammo_definitions(2,definitions+4,20)==20);
    assert(ge_mp_state_objects_begin(&owner,&props,definitions,24)==GE_MP_STATE_OK);
    export_ok();
}
static void end(void)
{
    assert(ge_mp_state_objects_end(&owner)==GE_MP_STATE_OK);
    assert(ge_original_prop_state_end(&pool));
    assert(ge_mp_state_props_end(&props)==GE_MP_STATE_OK);
}
static void native_transitions(unsigned count)
{
    begin(count);
    PropRecord *p=bind((ObjectRecord *)&armour,2);uint64_t physical=owner.ids[2],actor;
    assert(ge_mp_state_props_actor(&props,p,&actor)==GE_MP_STATE_OK);
    armour.state|=PROPSTATE_RESPAWN;export_ok();assert(output.values[2].prop==actor);
    for(unsigned slot=0;slot<count;++slot) {
        g_CurrentPlayer=&players[slot];g_playerPerm=&perms[slot];
        uint64_t same;armour.amount=.25f+(float)slot*.125f;
        players[slot].bonddead=1;
        assert(fixture_armour_pickup(p,FALSE)==TICKOP_NONE && !p->timetoregen);
        players[slot].bonddead=0;g_ClockTimer=0;
        assert(fixture_armour_pickup(p,FALSE)==TICKOP_NONE && !p->timetoregen);
        g_ClockTimer=1;
        impacts[0].prop=p;impacts[0].room=1;impacts[0].room_clear_flag=0;
        impacts[1].prop=p;impacts[1].room=1;impacts[1].room_clear_flag=1;
        TICKOP op=fixture_armour_pickup(p,FALSE);assert(op==TICKOP_FREE);
        assert(players[slot].bondarmour==armour.amount && perms[slot].body_armor_pickups==armour.amount);
        assert(impacts[0].room==-1 && impacts[1].room==-1);
        propExecuteTickOperation(p,op);
        assert(p->timetoregen==1200 && armour.prop==p && !ge_original_prop_state_is_enabled(p));
        assert(ge_mp_state_props_actor(&props,p,&same)==GE_MP_STATE_OK && same==actor);
        export_ok();assert(owner.ids[2]==physical && output.values[2].armour[1]==armour.amount);
        for(unsigned tick=0;tick<1140;++tick)fixture_object_regen(p);
        assert(p->timetoregen==60);
        /* Each slot, including nonlocal slot3, can delay native regeneration. */
        player_props[slot].pos.x=0;
        fixture_object_regen(p);assert(p->timetoregen==119 && !ge_original_prop_state_is_enabled(p));
        player_props[slot].pos.x=1000+(float)slot;
        for(unsigned tick=0;tick<60;++tick)fixture_object_regen(p);
        assert(p->timetoregen==59 && ge_original_prop_state_is_enabled(p));
        assert(armour.amount==armour.initialamount && !(armour.runtime_bitflags&RUNTIMEBITFLAG_00000800));
        for(unsigned tick=0;tick<59;++tick)fixture_object_regen(p);
        assert(!p->timetoregen);export_ok();
    }
    assert(audio_calls==2*count && room_calls==count);
    armour.state&=~PROPSTATE_RESPAWN;ObjectRecord saved=*(ObjectRecord *)&armour;
    objFreePermanently((ObjectRecord *)&armour,TRUE);
    assert(!armour.prop && armour.model==saved.model && armour.damage==saved.damage);
    assert(ge_mp_registry_resolve(&registry,actor,NULL)==GE_MP_STATE_STALE);
    export_ok();assert(output.values[2].prop==0 && output.values[2].model==0 && owner.ids[2]==physical);
    PropRecord *reuse=bind((ObjectRecord *)&armour,2);assert(reuse==p);
    uint64_t new_actor;assert(ge_mp_state_props_actor(&props,reuse,&new_actor)==GE_MP_STATE_OK && actor!=new_actor);
    export_ok();assert(output.values[2].prop==new_actor && owner.ids[2]==physical);
    /* Native linked-weapon free clears both links; dropped state is produced
     * by source projectile allocation, source detach and source permanent free. */
    linked.type=PROPDEF_COLLECTABLE;linked.weaponnum=ITEM_WPPK;
    PropRecord *w=bind((ObjectRecord *)&weapon,0),*l=bind((ObjectRecord *)&linked,3);
    weapon.dualweapon=&linked;linked.dualweapon=&weapon;
    export_ok();assert(output.values[0].dualweapon==owner.ids[3]);
    chrpropDelist(w);chrpropReparent(w,l);weapon.runtime_bitflags|=RUNTIMEBITFLAG_HASOWNER;
    propobjSetDropped(w,DROPTYPE_DEFAULT);
    assert(weapon.projectile==&g_Projectiles[0] && weapon.projectile->droptype==DROPTYPE_DEFAULT);
    export_ok();assert(output.values[0].projectile==projectile_ids[0]);
    objDetach(w);assert(!w->parent && !(weapon.runtime_bitflags&RUNTIMEBITFLAG_HASOWNER));
    chrpropActivate(w);objFreePermanently((ObjectRecord *)&weapon,TRUE);
    assert(!weapon.prop && !weapon.dualweapon && !linked.dualweapon);
    assert(!weapon.projectile && (g_Projectiles[0].flags&PROJECTILEFLAG_FREE));
    export_ok();
    /* Native twenty-slot ammo allocator must select/free the first eligible
     * offscreen slot, then reuse its physical definition with a new E3 ID. */
    for(unsigned i=0;i<20;++i){assert(ammocrateAllocate()==&g_AmmoCrates[i]);g_AmmoCrates[i].type=PROPDEF_MAGAZINE;
        g_AmmoCrates[i].ammoType=(AMMOTYPE)(i%13+1);bind((ObjectRecord *)&g_AmmoCrates[i],4+i);}
    PropRecord *a=g_AmmoCrates[0].prop;uint64_t old;
    assert(ge_mp_state_props_actor(&props,a,&old)==GE_MP_STATE_OK);
    assert(ammocrateAllocate()==&g_AmmoCrates[0] && !g_AmmoCrates[0].prop);
    assert(ge_mp_registry_resolve(&registry,old,NULL)==GE_MP_STATE_STALE);
    export_ok();assert(output.values[4].ammo==1 && !output.values[4].prop);
    assert(bind((ObjectRecord *)&g_AmmoCrates[0],4)==a);export_ok();
    assert(output.values[4].prop!=old);
    assert(ge_mp_state_props_export(&props,&prop_providers,&prop_output)==GE_MP_STATE_OK);
    end();
}
static void field_and_failure_cases(void)
{
    begin(2);
    weapon.extrascale=257;weapon.state=PROPSTATE_2;weapon.pad=-3;weapon.obj=333;
    weapon.flags=0xabcdef01;weapon.flags2=0xfedcba98;weapon.runtime_bitflags=0;
    for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)weapon.mtx.m[i][j]=(float)(i*4+j)/8;
    weapon.runtime_pos=(coord3d){.f={-0.0f,17.5f,-31.0f}};weapon.damage=9.5f;weapon.maxdamage=2.25f;
    weapon.shadecol=(rgba_u8){1,2,3,4};weapon.nextcol=(rgba_u8){5,6,7,8};
    weapon.weaponnum=-2;weapon.LinkedWeaponType=-1;weapon.timer=-32768;
    for(unsigned i=0;i<13;++i){multi.slots[i].modelnum=65535-i;multi.slots[i].quantity=i*17;}
    g_AmmoCrates[0].ammoType=AMMO_9MM;armour.amount=-0.0f;
    export_ok();GeMpObjectValues *v=&output.values[0];
    assert(v->extrascale==257 && v->state==PROPSTATE_2 && v->model_pad[0]==333 && v->model_pad[1]==-3);
    assert(v->flags[0]==0xabcdef01 && v->flags[1]==0xfedcba98 && v->weapons[0]==-2 && v->weapons[1]==-1 && v->timer==-32768);
    assert(v->pose[15]==15.0f/8 && signbit(v->pose[16]) && v->pose[19]==2.25f && v->pose[20]==9.5f);
    for(unsigned i=0;i<8;++i)assert(v->colors[i]==i+1);
    for(unsigned i=0;i<13;++i)assert(output.values[1].slot_models[i]==65535-i && output.values[1].quantities[i]==i*17);
    assert(output.values[4].ammo==AMMO_9MM && signbit(output.values[2].armour[1]));
    /* Every floating component is validated even with no allocated prop. */
    float *floats[23];for(unsigned i=0;i<16;++i)floats[i]=&weapon.mtx.m[i/4][i%4];
    for(unsigned i=0;i<3;++i)floats[16+i]=&weapon.runtime_pos.f[i];
    floats[19]=&weapon.maxdamage;floats[20]=&weapon.damage;floats[21]=&armour.initialamount;floats[22]=&armour.amount;
    for(unsigned i=0;i<23;++i){float saved=*floats[i];*floats[i]=i%2?NAN:INFINITY;
        assert(ge_mp_state_objects_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE && !output.record_count);
        *floats[i]=saved;}export_ok();
    weapon.flags=0;bind((ObjectRecord *)&weapon,0);
    assert(ge_mp_state_objects_export(&owner,NULL,&output)==GE_MP_STATE_INCOMPLETE);
    assert(output.unresolved_kinds==(1U<<GE_MP_OBJECT_REF_MODEL));
    Model unknown={0};weapon.model=&unknown;
    assert(ge_mp_state_objects_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);weapon.model=&models[0];
    PropRecord *real_prop=weapon.prop,foreign_prop={0};weapon.prop=&foreign_prop;
    assert(ge_mp_state_objects_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);
    assert(output.unresolved_kinds==(1U<<GE_MP_OBJECT_REF_PROP));weapon.prop=real_prop;
    real_prop->type=PROP_TYPE_OBJ;
    assert(ge_mp_state_objects_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);
    real_prop->type=PROP_TYPE_WEAPON;
    weapon.ptr_allocated_collisiondata_block=&collision;weapon.state|=PROPSTATE_EXT_COLISION_BLOCK;
    weapon.projectile=&g_Projectiles[0];weapon.runtime_bitflags=RUNTIMEBITFLAG_HASPROJECTILE;
    export_ok();assert(output.values[0].collision==collision_id && output.values[0].projectile==projectile_ids[0]);
    weapon.runtime_bitflags=RUNTIMEBITFLAG_EMBEDDED;weapon.embedment=&embedded;
    export_ok();assert(output.values[0].embedment==embedded_id && !output.values[0].projectile);
    WeaponObjRecord foreign={0};weapon.dualweapon=&foreign;
    assert(ge_mp_state_objects_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);
    assert(output.unresolved_kinds==(1U<<GE_MP_OBJECT_REF_LINKED_OBJECT));weapon.dualweapon=NULL;
    assert(ge_mp_registry_release(&registry,embedded_id)==GE_MP_STATE_OK);
    assert(ge_mp_state_objects_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);
    weapon.runtime_bitflags=0;weapon.projectile=NULL;export_ok();
    uint64_t id=0;assert(ge_mp_state_objects_actor(&owner,(char *)&weapon+1,&id)==GE_MP_STATE_INCOMPLETE && id==0);
    assert(ge_mp_state_objects_actor(&owner,&weapon,&id)==GE_MP_STATE_OK && id==owner.ids[0]);
    uint8_t digest[32];memset(digest,0xa5,sizeof(digest));
    GeMpRecordSchema schemas[]={{0,GE_MP_SYSTEM_SESSION,NULL,0},*ge_mp_state_objects_schema()};
    GeMpStateSchema schema={1,GE_MP_SYSTEM_SESSION|GE_MP_SYSTEM_OBJECTS,0,schemas,2};
    GeMpCanonicalSnapshot aggregate={.session=1,.tick=1,.schema=&schema,.registry=&registry,
        .records=output.records,.record_count=output.record_count,.enabled_systems=schema.registered_systems};
    assert(ge_mp_state_sha256(&aggregate,digest,NULL)==GE_MP_STATE_INCOMPLETE);
    for(unsigned i=0;i<32;++i)assert(digest[i]==0xa5);
    end();
}
static void lifecycle_validation(void)
{
    begin(4);GeMpObjectDefinition bad[24];memcpy(bad,definitions,sizeof(bad));
    assert(ge_mp_state_objects_end(&owner)==GE_MP_STATE_OK);
    size_t count=registry.count;uint32_t generation=entries[registry.count-1].generation;
    bad[0].definition=(char *)&weapon+1;
    assert(ge_mp_state_objects_begin(&owner,&props,bad,24)==GE_MP_STATE_INVALID);
    bad[0]=definitions[0];bad[1].definition=bad[0].definition;
    assert(ge_mp_state_objects_begin(&owner,&props,bad,24)==GE_MP_STATE_INVALID);
    bad[1]=definitions[1];bad[0].bytes--;
    assert(ge_mp_state_objects_begin(&owner,&props,bad,24)==GE_MP_STATE_INVALID);
    assert(!owner.live && registry.count==count && entries[count-1].generation==generation);
    bad[0]=definitions[0];bad[0].creation_namespace=7;
    size_t capacity=registry.capacity;registry.capacity=registry.count;
    assert(ge_mp_state_objects_begin(&owner,&props,bad,1)==GE_MP_STATE_CAPACITY);
    assert(!owner.live && registry.count==count && entries[count-1].generation==generation);
    registry.capacity=capacity;
    assert(ge_mp_state_objects_begin(&owner,&props,definitions,24)==GE_MP_STATE_OK);
    uint64_t old=owner.ids[0];
    assert(ge_original_prop_state_end(&pool));
    assert(ge_mp_state_objects_export(&owner,&providers,&output)==GE_MP_STATE_STALE);
    assert(ge_mp_state_objects_end(&owner)==GE_MP_STATE_STALE);
    assert(ge_mp_registry_resolve(&registry,old,NULL)==GE_MP_STATE_STALE);
    assert(ge_mp_state_props_end(&props)==GE_MP_STATE_OK);
    /* Rebinding the same E3 owner with a new native pool cannot revive EA. */
    begin(2);old=owner.ids[0];
    assert(ge_original_prop_state_end(&pool));
    assert(ge_mp_state_props_end(&props)==GE_MP_STATE_OK);
    assert(ge_original_prop_state_reset(&pool,4));
    assert(ge_mp_state_props_begin(&props,&registry)==GE_MP_STATE_OK);
    assert(ge_mp_state_objects_actor(&owner,&weapon,&old)==GE_MP_STATE_STALE);
    assert(ge_mp_state_objects_end(&owner)==GE_MP_STATE_STALE);
    assert(ge_original_prop_state_end(&pool));
    assert(ge_mp_state_props_end(&props)==GE_MP_STATE_OK);
    begin(2);old=owner.ids[0];
    assert(ge_original_prop_state_reset(&pool,4));
    /* E3 prioritizes the explicit missed-end observation fault over epoch. */
    assert(ge_mp_state_objects_actor(&owner,&weapon,&old)==GE_MP_STATE_INCOMPLETE);
    assert(ge_mp_state_objects_end(&owner)==GE_MP_STATE_INCOMPLETE);
    assert(ge_mp_state_props_end(&props)!=GE_MP_STATE_OK);
}
static void cleanup_identity_validation(void)
{
    static GeMpRegistryEntry saved_entries[1400];
    begin(2);size_t n=registry.count;
    memcpy(saved_entries,entries,n*sizeof(*entries));
    uint64_t own=owner.ids[23];
    /* The last record is malformed: no earlier legitimate EA ID may release. */
    owner.ids[23]=model_ids[0];
    assert(ge_mp_state_objects_end(&owner)==GE_MP_STATE_INVALID && owner.live);
    assert(registry.count==n && !memcmp(saved_entries,entries,n*sizeof(*entries)));
    owner.ids[23]=owner.ids[22];
    assert(ge_mp_state_objects_end(&owner)==GE_MP_STATE_INVALID && owner.live);
    assert(!memcmp(saved_entries,entries,n*sizeof(*entries)));
    owner.ids[23]=own;owner.count=GE_MP_OBJECTS_CAPACITY+1;
    assert(ge_mp_state_objects_end(&owner)==GE_MP_STATE_INVALID);
    assert(!memcmp(saved_entries,entries,n*sizeof(*entries)));owner.count=24;
    owner.count=23;
    assert(ge_mp_state_objects_end(&owner)==GE_MP_STATE_INVALID);
    assert(!memcmp(saved_entries,entries,n*sizeof(*entries)));owner.count=24;
    ++owner.definitions[23].creation_namespace;
    assert(ge_mp_state_objects_end(&owner)==GE_MP_STATE_INVALID);
    assert(!memcmp(saved_entries,entries,n*sizeof(*entries)));--owner.definitions[23].creation_namespace;
    size_t capacity=registry.capacity;registry.capacity=n-1;
    assert(ge_mp_state_objects_end(&owner)==GE_MP_STATE_INVALID && owner.live);
    assert(!memcmp(saved_entries,entries,n*sizeof(*entries)));registry.capacity=capacity;
    /* A stale generation must not release its live successor at the same key. */
    assert(ge_mp_registry_release(&registry,own)==GE_MP_STATE_OK);
    uint64_t successor;
    assert(ge_mp_registry_acquire(&registry,(uint32_t)(own>>32),&successor)==GE_MP_STATE_OK);
    assert(successor!=own);
    assert(ge_mp_state_objects_end(&owner)==GE_MP_STATE_STALE && !owner.live);
    assert(ge_mp_registry_resolve(&registry,successor,NULL)==GE_MP_STATE_OK);
    assert(ge_mp_registry_resolve(&registry,model_ids[0],NULL)==GE_MP_STATE_OK);
    assert(ge_mp_registry_release(&registry,successor)==GE_MP_STATE_OK);
    assert(ge_original_prop_state_end(&pool));
    assert(ge_mp_state_props_end(&props)==GE_MP_STATE_OK);
}
int main(void)
{
    native_transitions(2);native_transitions(4);field_and_failure_cases();lifecycle_validation();cleanup_identity_validation();
    puts("native EA objects: authored armor pickup, 2/4 proximity regen, drop/free/reuse, typed fields and E3 lifetimes passed");return 0;
}
