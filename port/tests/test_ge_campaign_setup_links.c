#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>
#include <bondtypes.h>
#include "ge_original_stage_switch_runtime.h"
#include "ge_original_stage_interactive_objects.h"
#include "ge_original_stage_padlock_runtime.h"
#include "ge_original_stage_safe_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ObjectRecord *g_LevelLoadPropSwitch;
LockDoorRecord *g_LevelLoadPropLockDoor;
s32 g_ClockTimer=1;
#define RUNTIMEBITFLAG_ACTIVATED (1U<<14U)
#define RUNTIMEBITFLAG_REMOVE (1U<<2U)
#define RUNTIMEBITFLAG_00008000 (1U<<15U)
#define RUNTIMEBITFLAG_00010000 (1U<<16U)
static struct {s16 TypeID;} object_explosion_details[512];
static void doorStartOpen(DoorRecord *door) {(void)door;}
static void doorStartClose(DoorRecord *door) {(void)door;}
static void objDeform(ObjectRecord *obj,int level) {(void)obj;(void)level;}
static void objDestroySupportedObjects(PropRecord *prop,int player) {(void)prop;(void)player;}
static void objFall(ObjectRecord *obj,int player) {(void)obj;(void)player;}
static u32 randomGetNext(void) {return 1;}
static int walkTilesBetweenPoints_NoCallback(StandTile **stan,float x,float z,float xx,float zz) {assert(0);return 0;}
static void explosionCreate(PropRecord *prop,coord3d *pos,StandTile *stan,int type,int arg,int player,u8 *rooms,int arg2) {assert(0);}
void sub_GAME_7F03E6A0(PropRecord *prop);
static DoorRecord *find_native_door(void *definition) { return definition; }
#include "campaign_link_exact.inc"

static int link_doors(void *context,void *first,void *second)
{ (void)context;return ge_original_door_runtime_link_pair(first,second); }

typedef struct Fixture { GeOriginalStageSetupRuntime setup; DoorRecord *objects; PropRecord *props; } Fixture;
static void *resolve(void *context,size_t index)
{
    Fixture *f=context;
    assert(index<f->setup.prop_record_count);
    const GeOriginalStagePropRecord *r=&f->setup.prop_records[index];
    return r->word_count>=32U && r->type!=PROPDEF_GUARD ? &f->objects[index] : NULL;
}
static size_t target(Fixture *f,size_t index,size_t slot)
{
    const GeOriginalStagePropRecord *r=&f->setup.prop_records[index];
    assert(slot<r->relation_count);
    ptrdiff_t dst=r->relations[slot]-f->setup.prop_records;
    assert(dst>=0 && (size_t)dst<f->setup.prop_record_count);
    return (size_t)dst;
}
int main(int argc,char **argv)
{
    GeAssetPack pack;
    size_t total_switch=0,total_lock=0,total_safe=0,total_door=0,total_tag=0,total_objective=0,total_path=0,total_vehicle=0;
    assert(argc==2 && ge_asset_pack_open(&pack,argv[1])==GE_ASSET_PACK_OK);
    puts("stage,switches,padlocks,safes,door_links,backward_nonreciprocal,tags,objective_refs,patrol_paths,vehicles");
    for (size_t stage=0;stage<GE_STAGE_COUNT;++stage) {
        Fixture f={0};
        GeOriginalStageSwitchRuntime switches={0};
        GeOriginalStagePadlockRuntime padlocks={0};
        GeOriginalStageSafeRuntime safes={0};
        size_t ns=0,nl=0,nf=0,nd=0,nb=0,nt=0,no=0,nv=0;
        GeOriginalStageInteractiveRuntime interactive={0};
        const GeStageAssetDescriptor *desc=ge_stage_asset_descriptor((GeStageId)stage);
        assert(ge_original_stage_setup_load(&pack,desc,&f.setup)==GE_ORIGINAL_STAGE_SETUP_OK);
        f.objects=calloc(f.setup.prop_record_count,sizeof(*f.objects));
        f.props=calloc(f.setup.prop_record_count,sizeof(*f.props));
        SafeObjectRecord *safe_links=calloc(f.setup.prop_record_count,sizeof(*safe_links));
        assert(f.objects && f.props && safe_links);
        interactive.entries=calloc(f.setup.prop_record_count,sizeof(*interactive.entries));
        assert(interactive.entries);interactive.providers.link_doors=link_doors;
        for(size_t i=0;i<f.setup.prop_record_count;++i) {
            GeOriginalStagePropRecord *r=&f.setup.prop_records[i];
            ObjectRecord *o=resolve(&f,i);
            if(o) {
                o->type=r->type; o->prop=&f.props[i];f.props[i].obj=o;
                o->damage=(float)(int32_t)r->words[29]/65536.0f; o->obj=0;
                if(r->type==PROPDEF_DOOR) {
                    GeOriginalStageInteractiveEntry *entry=&interactive.entries[interactive.entry_count++];
                    entry->record=r;entry->type=r->type;entry->definition=&f.objects[i];entry->constructed=1;
                    ++interactive.report.blocker_counts[GE_ORIGINAL_STAGE_INTERACTIVE_READY];
                    f.objects[i].linkedDoorOffset=(int32_t)r->words[32];
                    f.objects[i].maxFrac=(float)(int32_t)r->words[33]/65536.0f;
                    f.objects[i].accel=(float)(int32_t)r->words[35]/65536.0f;
                    f.objects[i].decel=(float)(int32_t)r->words[36]/65536.0f;
                    f.objects[i].maxSpeed=(float)(int32_t)r->words[37]/65536.0f;
                }
            }
            for(size_t slot=0;slot<r->relation_count;++slot)(void)target(&f,i,slot);
            if(r->type==PROPDEF_SWITCH)++ns;
            if(r->type==PROPDEF_LOCK_DOOR)++nl;
            if(r->type==PROPDEF_SAFE_ITEM)++nf;
            if(r->type==PROPDEF_TAG)++nt;
            if(r->objective && r->relation_count)++no;
            if(r->type==PROPDEF_VEHICHLE||r->type==PROPDEF_AIRCRAFT||r->type==PROPDEF_TANK)++nv;
            if(r->type==PROPDEF_DOOR && r->relation_count) {
                ++nd; size_t dst=target(&f,i,0);
                assert(f.setup.prop_records[dst].type==PROPDEF_DOOR);
                
                if(dst<i && (f.setup.prop_records[dst].relation_count==0 || target(&f,dst,0)!=i))++nb;
            }
        }
        ge_interactive_link_doors(&interactive);
        for(size_t i=0;i<f.setup.prop_record_count;++i)
            if(f.setup.prop_records[i].type==PROPDEF_DOOR && f.setup.prop_records[i].relation_count)
                assert(f.objects[i].linkedDoor==&f.objects[target(&f,i,0)]);
        /* Every authored linked group must remain reachable from every leaf;
         * execute unchanged doorActivate and real displacement for each. */
        for(size_t i=0;i<f.setup.prop_record_count;++i) {
            if(f.setup.prop_records[i].type!=PROPDEF_DOOR || !f.setup.prop_records[i].relation_count)continue;
            for(size_t j=0;j<f.setup.prop_record_count;++j){f.objects[j].openstate=DOORSTATE_STATIONARY;f.objects[j].openPosition=0;}
            doorActivate(&f.objects[i],DOORSTATE_OPENING);
            DoorRecord *current=&f.objects[i];size_t visited=0;
            do {
                assert(current->openstate==DOORSTATE_OPENING);
                assert(updateDoorDisplacement(current));
                if(current->maxFrac>0)assert(current->openPosition>0);
                current=current->linkedDoor;assert(++visited<=f.setup.prop_record_count);
            }while(current && current!=&f.objects[i]);
        }
        assert(ge_original_stage_switch_runtime_materialize(&switches,&f.setup,&f,resolve));
        assert(switches.relation_count==ns && switches.omitted_count==0);
        assert(ge_original_stage_padlock_runtime_materialize(&padlocks,&f.setup,&f,resolve));
        assert(padlocks.relation_count==nl && padlocks.omitted_count==0);
        for(size_t i=0;i<f.setup.prop_record_count;++i) {
            GeOriginalStagePropRecord *r=&f.setup.prop_records[i];
            if(r->type==PROPDEF_SWITCH) {
                size_t first=target(&f,i,0),second=target(&f,i,1);
                if(f.setup.prop_records[first].type==PROPDEF_DOOR)
                    fprintf(stderr,"door switch endpoint: %s command%zu ->%zu\n",desc->key,first,second);
                for(size_t j=0;j<f.setup.prop_record_count;++j) {
                    f.objects[j].openstate=DOORSTATE_STATIONARY;f.objects[j].openPosition=0;
                }
                sub_GAME_7F03E6A0(&f.props[first]);
                assert(f.objects[second].openstate==DOORSTATE_OPENING);
                assert(updateDoorDisplacement(&f.objects[second]));
                assert(f.objects[second].openPosition>0);
            }
        }
        /* Native objExplode owns the destroyed flag. Directly accumulate a
         * nonlethal/lethal damage amount; deformation/fall presentation is an
         * observing boundary, and no bullet ray/collision is simulated. */
        for(LockDoorRecord *link=g_LevelLoadPropLockDoor;link;link=link->next) {
            assert(!ge_original_stage_padlock_runtime_is_free(&padlocks,link->door));
            link->lock->maxdamage=link->lock->damage;
            objExplode(link->lock,&link->lock->runtime_pos,0);
            assert(objIsHealthy(link->lock));
        }
        for(LockDoorRecord *link=g_LevelLoadPropLockDoor;link;link=link->next) {
            link->lock->maxdamage=link->lock->damage+1;
            objExplode(link->lock,&link->lock->runtime_pos,0);
            assert(!objIsHealthy(link->lock));
            int all_destroyed=1;
            for(LockDoorRecord *other=g_LevelLoadPropLockDoor;other;other=other->next)
                if(other->door==link->door && objIsHealthy(other->lock))all_destroyed=0;
            assert(ge_original_stage_padlock_runtime_is_free(&padlocks,link->door)==all_destroyed);
        }
        for(LockDoorRecord *link=g_LevelLoadPropLockDoor;link;link=link->next)
            assert(ge_original_stage_padlock_runtime_is_free(&padlocks,link->door));
        ge_original_stage_safe_runtime_bind(&safes);
        for(size_t i=0;i<f.setup.prop_record_count;++i)if(f.setup.prop_records[i].type==PROPDEF_SAFE_ITEM){
            SafeObjectRecord *link=&safe_links[i];
            link->item=resolve(&f,target(&f,i,0));link->safe=resolve(&f,target(&f,i,1));link->door=resolve(&f,target(&f,i,2));
            assert(ge_original_stage_safe_runtime_register_relation(&safes,link));
            link->item->flags2|=PROPFLAG2_LINKEDTOSAFE;
            link->door->openPosition=0.5f;
            assert(!ge_original_stage_safe_runtime_can_pickup(&safes,link->item));
            link->door->openPosition=0.501f;
            assert(ge_original_stage_safe_runtime_can_pickup(&safes,link->item));
        }
        assert(safes.relation_count==nf);
        GeOriginalStagePathAudit path;
        if(f.setup.waypoint_count)assert(ge_original_stage_setup_path_audit(&f.setup,&path));
        printf("%s,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu,%zu\n",desc->key,ns,nl,nf,nd,nb,nt,no,f.setup.patrolpath_count,nv);
        total_switch+=ns;total_lock+=nl;total_safe+=nf;total_door+=nd;total_tag+=nt;total_objective+=no;total_path+=f.setup.patrolpath_count;total_vehicle+=nv;
        ge_original_stage_safe_runtime_close(&safes);
        ge_original_stage_padlock_runtime_close(&padlocks);
        ge_original_stage_switch_runtime_close(&switches);
        assert(!g_LevelLoadPropLockDoor && !g_LevelLoadPropSwitch);
        for(size_t i=0;i<f.setup.prop_record_count;++i){f.objects[i].state=0;f.objects[i].maxdamage=0;}
        assert(ge_original_stage_padlock_runtime_materialize(&padlocks,&f.setup,&f,resolve));
        for(LockDoorRecord *link=g_LevelLoadPropLockDoor;link;link=link->next)
            assert(!ge_original_stage_padlock_runtime_is_free(&padlocks,link->door));
        ge_original_stage_padlock_runtime_close(&padlocks);
        free(interactive.entries);free(safe_links);free(f.objects);free(f.props);ge_original_stage_setup_close(&f.setup);
    }
    printf("total,%zu,%zu,%zu,%zu,,%zu,%zu,%zu,%zu\n",total_switch,total_lock,total_safe,total_door,total_tag,total_objective,total_path,total_vehicle);
    ge_asset_pack_close(&pack);
    return 0;
}
