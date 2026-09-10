#include "ge_mp_state_models.h"
#include "ge_original_pitem_models.h"
#include "ge_original_character_models.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/model.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

void clear_model_obj(Model *);
bool modelmgrCanSlotFitRwdata(Model *,ModelFileHeader *);
static GeMpRegistry registry;
static GeMpRegistryEntry entries[1800];
static GeOriginalPropState pool;
static GeMpStatePropsOwner props;
static GeMpStateModelsOwner owner;
static GeMpStateModelsSnapshot output;
static Model models[4];
static Model *prefix;
static ModelFileHeader header;
static ModelNode node;
static ModelAnimation animations[2];
static uint32_t rw[4][16];
static uint64_t ref_ids[12];
static GeMpModelDefinition definitions[5];
static unsigned model_count,flip_calls;
static int unknown_roles,unknown_frame;
static void flip_a(void){++flip_calls;}
static void flip_b(void){flip_calls+=10;}
static s32 root_a(Model *m,coord3d *a,coord3d *b,f32 *c)
{(void)m;(void)a;(void)b;(void)c;return 1;}
static s32 root_b(Model *m,coord3d *a,coord3d *b,f32 *c)
{(void)m;(void)a;(void)b;(void)c;return 0;}
static GeMpStateStatus ref(void *ctx,GeMpModelReferenceKind kind,const void *p,uint64_t *id)
{
    (void)ctx;
    if(kind==GE_MP_MODEL_REF_HEADER && p==&header) {*id=ref_ids[kind];return GE_MP_STATE_OK;}
    if(kind==GE_MP_MODEL_REF_ANIMATION && p==&animations[0]) {*id=ref_ids[kind];return GE_MP_STATE_OK;}
    if(kind==GE_MP_MODEL_REF_ANIMATION2 && p==&animations[1]) {*id=ref_ids[kind];return GE_MP_STATE_OK;}
    if(kind==GE_MP_MODEL_REF_RW)for(unsigned i=0;i<4;++i)if(p==rw[i]) {*id=ref_ids[kind];return GE_MP_STATE_OK;}
    /* Explicit boundary resources: distinct per reference kind; this fixture
     * does not claim ownership of their content or complete animation state. */
    if(p==&ref_ids[kind]) {*id=ref_ids[kind];return GE_MP_STATE_OK;}
    return GE_MP_STATE_INCOMPLETE;
}
static GeMpStateStatus frame_ref(void *ctx,GeMpModelReferenceKind kind,const void *m,int32_t word,uint64_t *id)
{
    (void)ctx;assert(m==&models[0]);
    if(unknown_frame || word!=-(int32_t)(100+kind))return GE_MP_STATE_INCOMPLETE;
    *id=ref_ids[kind];return GE_MP_STATE_OK;
}
static GeMpStateStatus root_role(void *ctx,GeOriginalModelRootCallback fn,uint32_t *role)
{
    (void)ctx;if(unknown_roles)return GE_MP_STATE_INCOMPLETE;
    if(fn==root_a){*role=11;return GE_MP_STATE_OK;}
    if(fn==root_b){*role=22;return GE_MP_STATE_OK;}
    return GE_MP_STATE_INCOMPLETE;
}
static GeMpStateStatus flip_role(void *ctx,GeOriginalModelFlipCallback fn,uint32_t *role)
{
    (void)ctx;if(unknown_roles)return GE_MP_STATE_INCOMPLETE;
    if(fn==flip_a){*role=33;return GE_MP_STATE_OK;}
    if(fn==flip_b){*role=44;return GE_MP_STATE_OK;}
    return GE_MP_STATE_INCOMPLETE;
}
static const GeMpStateModelsProviders providers={NULL,ref,frame_ref,root_role,flip_role};
static void export_ok(void)
{
    assert(ge_mp_state_models_export(&owner,&providers,&output)==GE_MP_STATE_OK);
    assert(output.record_count==model_count+1 && !output.coverage.unresolved_references);
    assert(!output.coverage.unclassified_fields && output.coverage.enumerated_systems==GE_MP_SYSTEM_ANIMATION);
    const GeMpRecordSchema *schema=ge_mp_state_models_schema();
    for(size_t i=0;i<output.record_count;++i)for(size_t j=0;j<GE_MP_MODELS_FIELDS;++j) {
        assert(output.fields[i][j].type==schema->fields[j].type && output.fields[i][j].id==schema->fields[j].id);
        assert(output.fields[i][j].count>=schema->fields[j].min_count && output.fields[i][j].count<=schema->fields[j].max_count);
    }
}
static void begin(unsigned count)
{
    assert(!owner.live && !props.live);model_count=count;unknown_roles=unknown_frame=0;
    memset(models,0,sizeof(models));memset(&header,0,sizeof(header));memset(&node,0,sizeof(node));
    memset(animations,0,sizeof(animations));memset(rw,0,sizeof(rw));
    /* An actual empty non-root model node skips geometry/root-motion resource
     * decoding while all original animation clock/merge code remains linked. */
    node.Opcode=MODELNODE_OPCODE_NULL;header.RootNode=&node;
    animations[0].unk04=20;animations[1].unk04=30;
    assert(ge_original_prop_state_reset(&pool,4));
    assert(ge_mp_registry_init(&registry,entries,1800)==GE_MP_STATE_OK);
    assert(ge_mp_state_props_begin(&props,&registry)==GE_MP_STATE_OK);
    for(unsigned i=0;i<12;++i) {
        uint32_t key=ge_mp_registry_key(0x30,255,i);
        assert(ge_mp_registry_define(&registry,key)==GE_MP_STATE_OK);
        assert(ge_mp_registry_acquire(&registry,key,&ref_ids[i])==GE_MP_STATE_OK);
    }
    for(unsigned i=0;i<count;++i) {
        animInit(&models[i],&header,rw[i]);
        definitions[i]=(GeMpModelDefinition){&models[i],sizeof(Model),0,GE_MP_MODEL_FULL,i};
    }
    prefix=calloc(1,offsetof(Model,anim));assert(prefix);
    modelInit(prefix,&header,rw[0]);
    definitions[count]=(GeMpModelDefinition){prefix,offsetof(Model,anim),1,GE_MP_MODEL_PREFIX,0};
    assert(ge_mp_state_models_begin(&owner,&props,definitions,count+1)==GE_MP_STATE_OK);export_ok();
    assert(output.fields[count][2].count==1 && output.fields[count][3].count==6);
    assert(output.fields[count][4].count==0 && output.fields[count][7].count==0);
}
static void end(void)
{
    assert(ge_mp_state_models_end(&owner)==GE_MP_STATE_OK);
    free(prefix);prefix=NULL;
    assert(ge_original_prop_state_end(&pool));assert(ge_mp_state_props_end(&props)==GE_MP_STATE_OK);
}
static void native_transitions(unsigned count)
{
    begin(count);flip_calls=0;
    for(unsigned i=0;i<count;++i) {
        Model *m=&models[i];uint64_t id=owner.ids[i];
        modelSetAnimation(m,&animations[0],i&1,0,1,0);
        modelSetAnimSpeed(m,3,8);modelSetAnimPlaySpeed(m,2,4);
        export_ok();assert(output.values[i].floats[5]==3 && output.values[i].floats[7]==8);
        modelTickAnim(m,1,0);export_ok();
        assert(output.values[i].floats[22]==1.25f && output.values[i].floats[26]==1);
        assert(output.values[i].floats[8]==1.25f && output.values[i].floats[4]==1.3125f);
        assert(output.values[i].floats[1]==1.640625f && output.values[i].frames[0]==1);
        modelSetAnimSpeed(m,1,0);modelSetAnimPlaySpeed(m,1,0);
        modelSetAnimFrame(m,18);modelSetAnimLooping(m,2,0);
        modelSetAnimFlipFunction(m,flip_a);ge_original_model_root_callback_set(m,root_a);
        modelTickAnim(m,1,0);export_ok();
        assert(m->animframe1==2 && output.values[i].floats[1]==2);
        assert(output.values[i].callbacks[0]==11 && output.values[i].callbacks[1]==33);
        assert(flip_calls==i+1 && owner.ids[i]==id);
        /* Original free only clears obj. Retained RW capacity still controls
         * reuse eligibility; physical identity survives this native free. */
        m->rwdatalen=16;header.numRecords=8;assert(modelmgrCanSlotFitRwdata(m,&header));
        clear_model_obj(m);export_ok();assert(!output.values[i].refs[1]);
        assert(output.values[i].refs[3] && output.values[i].prefix[1]==16 && owner.ids[i]==id);
        header.numRecords=17;assert(!modelmgrCanSlotFitRwdata(m,&header));header.numRecords=0;
        /* animInit clears its authored subset, not residual time/target words. */
        m->newspeed=17;m->animframe2=9;
        animInit(m,&header,rw[i]);export_ok();
        assert(output.values[i].floats[5]==17 && output.values[i].floats[9]==9);
        assert(!output.values[i].callbacks[0] && !output.values[i].callbacks[1]);
        assert(owner.ids[i]==id);
    }
    end();
}
static void references_and_floats(void)
{
    begin(2);Model *m=&models[0];uint64_t id;
    m->chr=(void *)&ref_ids[0];m->render_pos=(void *)&ref_ids[2];
    m->attachedto=&models[1];m->attachedto_objinst=(void *)&ref_ids[5];
    m->anim=&animations[0];m->anim2=&animations[1];
    m->unk34=-(100+GE_MP_MODEL_REF_FRAME_A);m->unk38=-(100+GE_MP_MODEL_REF_FRAME_B);
    m->unk64=-(100+GE_MP_MODEL_REF_FRAME2_A);m->unk68=-(100+GE_MP_MODEL_REF_FRAME2_B);
    export_ok();assert(output.values[0].refs[4]==owner.ids[1]);
    assert(ge_mp_state_models_actor(&owner,m,&id)==GE_MP_STATE_OK && id==owner.ids[0]);
    unknown_frame=1;assert(ge_mp_state_models_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);
    assert(output.record_count==0 && output.coverage.unresolved_references==4);unknown_frame=0;
    assert(ge_mp_registry_release(&registry,ref_ids[9])==GE_MP_STATE_OK);
    assert(ge_mp_state_models_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);
    assert(ge_mp_registry_acquire(&registry,(uint32_t)(ref_ids[9]>>32),&ref_ids[9])==GE_MP_STATE_OK);export_ok();
    /* Snapshot scalar sequence covers every named f32 with finite validation,
     * including inactive interpolation targets that animInit leaves intact. */
    float *floats[]={&m->scale,&m->animframe1,&m->unk2c,&m->endframe,&m->speed,&m->newspeed,&m->oldspeed,
        &m->timespeed,&m->elapsespeed,&m->animframe2,&m->unk5c,&m->unk6c,&m->speed2,&m->unk74,&m->unk78,
        &m->unk7c,&m->unk80,&m->unk84,&m->unk88,&m->unk8c,&m->animloopframe,&m->animloopmerge,
        &m->playspeed,&m->animrate,&m->unkac,&m->unkb0,&m->unkb4,&m->anim_translation_scale};
    for(unsigned i=0;i<28;++i) {
        float saved=*floats[i];*floats[i]=-(float)(i+1);export_ok();assert(output.values[0].floats[i]==*floats[i]);
        *floats[i]=-0.0f;export_ok();assert(signbit(output.values[0].floats[i]));
        *floats[i]=NAN;assert(ge_mp_state_models_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE && !output.record_count);
        *floats[i]=INFINITY;assert(ge_mp_state_models_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);
        *floats[i]=saved;
    }
    ge_original_model_root_callback_set(m,root_b);modelSetAnimFlipFunction(m,flip_b);export_ok();
    assert(output.values[0].callbacks[0]==22 && output.values[0].callbacks[1]==44);
    unknown_roles=1;assert(ge_mp_state_models_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);
    assert(output.coverage.unresolved_references==2);unknown_roles=0;
    m->unka0=INT32_MAX;assert(ge_mp_state_models_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);
    ge_original_model_root_callback_set(m,root_b);m->animflipfunc=INT32_MAX;
    assert(ge_mp_state_models_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);
    modelSetAnimFlipFunction(m,flip_b);export_ok();
    /* A bounded exporter never upgrades the aggregate completeness attestation. */
    GeMpStateSchema schema={1,GE_MP_SYSTEM_ANIMATION,0,ge_mp_state_models_schema(),1};
    GeMpCanonicalSnapshot snapshot={.session=1,.schema=&schema,.registry=&registry,.records=output.records,.record_count=output.record_count};
    uint8_t digest[32],saved_digest[32];memset(digest,0x7a,32);memcpy(saved_digest,digest,32);
    assert(ge_mp_state_sha256(&snapshot,digest,NULL)!=GE_MP_STATE_OK && !memcmp(digest,saved_digest,32));
    end();
}
static void lifetime(void)
{
    begin(4);GeMpStateModelsOwner saved=owner;GeMpRegistryEntry before[1800];
    for(unsigned test=0;test<8;++test) {
        owner=saved;memcpy(before,entries,sizeof(entries));GeMpRegistry r=registry;
        if(test==0)owner.ids[0]=(UINT64_C(0x30000000)<<32)|(uint32_t)owner.ids[0];
        if(test==1)owner.ids[0]^=UINT64_C(1)<<48;
        if(test==2)owner.ids[0]^=UINT64_C(1)<<32;
        if(test==3)owner.count=GE_MP_MODELS_CAPACITY+1;
        if(test==4)owner.ids[owner.count]=owner.ids[0];
        if(test==5)owner.definitions[0].bytes--;
        if(test==6)owner.definitions[0].extent=255;
        if(test==7)owner.pool_id=owner.ids[0];
        assert(ge_mp_state_models_end(&owner)==GE_MP_STATE_INVALID);
        assert(!memcmp(entries,before,sizeof(entries)) && !memcmp(&registry,&r,sizeof(r)));
    }
    owner=saved;uint64_t old=owner.ids[0],replacement;
    assert(ge_mp_registry_release(&registry,old)==GE_MP_STATE_OK);
    assert(ge_mp_registry_acquire(&registry,(uint32_t)(old>>32),&replacement)==GE_MP_STATE_OK);
    assert(ge_mp_state_models_end(&owner)==GE_MP_STATE_STALE);
    assert(ge_mp_registry_resolve(&registry,replacement,NULL)==GE_MP_STATE_OK);
    assert(ge_mp_registry_release(&registry,replacement)==GE_MP_STATE_OK);free(prefix);
    assert(ge_original_prop_state_end(&pool));assert(ge_mp_state_props_end(&props)==GE_MP_STATE_OK);
    begin(2);old=owner.ids[0];
    assert(ge_original_prop_state_end(&pool));
    assert(ge_mp_state_models_export(&owner,&providers,&output)==GE_MP_STATE_STALE);
    assert(ge_mp_state_models_end(&owner)==GE_MP_STATE_STALE);
    assert(ge_mp_registry_resolve(&registry,old,NULL)==GE_MP_STATE_STALE);free(prefix);
    assert(ge_mp_state_props_end(&props)==GE_MP_STATE_OK);
    begin(2);old=owner.ids[0];
    assert(ge_original_prop_state_end(&pool));assert(ge_mp_state_props_end(&props)==GE_MP_STATE_OK);
    assert(ge_original_prop_state_reset(&pool,4));
    assert(ge_mp_state_props_begin(&props,&registry)==GE_MP_STATE_OK);
    assert(ge_mp_state_models_actor(&owner,&models[0],&old)==GE_MP_STATE_STALE);
    assert(ge_mp_state_models_end(&owner)==GE_MP_STATE_STALE);free(prefix);
    assert(ge_original_prop_state_end(&pool));assert(ge_mp_state_props_end(&props)==GE_MP_STATE_OK);
    begin(2);old=owner.ids[0];
    assert(ge_original_prop_state_reset(&pool,4));
    assert(ge_mp_state_models_actor(&owner,&models[0],&old)==GE_MP_STATE_INCOMPLETE);
    assert(ge_mp_state_models_end(&owner)==GE_MP_STATE_INCOMPLETE);free(prefix);
    assert(ge_mp_state_props_end(&props)!=GE_MP_STATE_OK);
    assert(ge_original_prop_state_end(&pool));
}
static void descriptor_validation(void)
{
    begin(2);static GeMpStateModelsOwner candidate;
    GeMpModelDefinition defs[2]={definitions[0],definitions[1]};
    defs[0].creation_namespace=10;defs[1].creation_namespace=10;
    GeMpRegistryEntry before[1800];GeMpRegistry r=registry;
    for(unsigned test=0;test<7;++test) {
        GeMpModelDefinition bad[2]={defs[0],defs[1]};memcpy(before,entries,sizeof(entries));
        if(test==0)bad[0].bytes=offsetof(Model,anim);
        if(test==1)bad[0].extent=255;
        if(test==2)bad[1].model=bad[0].model;
        if(test==3)bad[1].slot=bad[0].slot;
        if(test==4)bad[0].model=NULL;
        if(test==5)bad[0].model=(const char *)bad[0].model+1;
        if(test==6)bad[0].slot=2;
        assert(ge_mp_state_models_begin(&candidate,&props,bad,2)==GE_MP_STATE_INVALID);
        assert(!memcmp(entries,before,sizeof(entries)) && !memcmp(&registry,&r,sizeof(r)) && !candidate.live);
    }
    assert(ge_mp_state_models_begin(&candidate,&props,defs,GE_MP_MODELS_CAPACITY+1)==GE_MP_STATE_INVALID);
    size_t capacity=registry.capacity;registry.capacity=registry.count+1;
    memcpy(before,entries,sizeof(entries));
    assert(ge_mp_state_models_begin(&candidate,&props,defs,2)==GE_MP_STATE_CAPACITY);
    assert(!memcmp(entries,before,sizeof(entries)));registry.capacity=capacity;
    /* Descriptor construction may use the owner's own descriptor buffer. */
    memcpy(candidate.definitions,defs,sizeof(defs));
    assert(ge_mp_state_models_begin(&candidate,&props,candidate.definitions,2)==GE_MP_STATE_OK);
    assert(candidate.definitions[1].model==&models[1]);
    assert(ge_mp_state_models_end(&candidate)==GE_MP_STATE_OK);end();
}
/* Run before registering any callback in this process: equivalent native
 * function roles must export identically when process token order differs. */
static void registration_order(void)
{
    uint32_t reports[2][4];
    for(unsigned order=0;order<2;++order) {
        int pipefd[2];assert(pipe(pipefd)==0);pid_t child=fork();assert(child>=0);
        if(!child) {
            close(pipefd[0]);begin(2);
            for(unsigned j=0;j<2;++j) {
                unsigned i=j^order;ge_original_model_root_callback_set(&models[i],i?root_b:root_a);
                modelSetAnimFlipFunction(&models[i],i?flip_b:flip_a);
            }
            export_ok();uint32_t report[4]={models[0].unka0,models[0].animflipfunc,output.values[0].callbacks[0],output.values[0].callbacks[1]};
            assert(write(pipefd[1],report,sizeof(report))==sizeof(report));end();_exit(0);
        }
        close(pipefd[1]);assert(read(pipefd[0],reports[order],sizeof(reports[order]))==sizeof(reports[order]));close(pipefd[0]);
        int status;assert(waitpid(child,&status,0)==child && WIFEXITED(status) && !WEXITSTATUS(status));
    }
    assert(reports[0][0]!=reports[1][0] && reports[0][1]!=reports[1][1]);
    assert(reports[0][2]==reports[1][2] && reports[0][3]==reports[1][3]);
}
static void physical_providers(const char *path,unsigned count)
{
    GeAssetPack pack;GeOriginalPitemModelStatus ps;GeOriginalCharacterModelStatus cs;
    assert(ge_asset_pack_open(&pack,path)==GE_ASSET_PACK_OK);
    GeOriginalPitemModelProvider *p=ge_original_pitem_model_provider_create(&pack,2,count,&ps);
    GeOriginalCharacterModelProvider *c=ge_original_character_model_provider_create(&pack,2,count,&cs);
    assert(p && c && !ps && !cs);
    assert(ge_original_pitem_model_physical_capacity(p)==count);
    assert(ge_original_character_model_physical_capacity(c)==count);
    GeMpModelDefinition defs[8];const void *native;size_t bytes;int active;
    for(unsigned i=0;i<count;++i) {
        assert(ge_original_pitem_model_physical_instance(p,i,&native,&bytes,&active) && !active);
        assert(bytes==sizeof(Model));defs[i]=(GeMpModelDefinition){native,bytes,2,GE_MP_MODEL_FULL,i};
        assert(ge_original_character_model_physical_instance(c,i,&native,&bytes,&active) && !active);
        assert(bytes==sizeof(Model));defs[count+i]=(GeMpModelDefinition){native,bytes,3,GE_MP_MODEL_FULL,i};
    }
    assert(!ge_original_pitem_model_physical_instance(p,count,&native,&bytes,&active));
    assert(!ge_original_character_model_physical_instance(c,count,&native,&bytes,&active));
    assert(ge_original_prop_state_reset(&pool,4));
    assert(ge_mp_registry_init(&registry,entries,1800)==GE_MP_STATE_OK);
    assert(ge_mp_state_props_begin(&props,&registry)==GE_MP_STATE_OK);
    assert(ge_mp_state_models_begin(&owner,&props,defs,2*count)==GE_MP_STATE_OK);
    assert(ge_mp_state_models_export(&owner,NULL,&output)==GE_MP_STATE_OK && output.record_count==2*count);
    for(unsigned i=0;i<count;++i) {
        void *h,*instance;float scale,pov;uint64_t id;
        assert(ge_original_pitem_model_resolve_instance(p,184,&h,&instance,&scale));
        assert(instance==defs[i].model);
        assert(ge_original_pitem_model_physical_instance(p,i,&native,&bytes,&active) && active && native==instance);
        assert(ge_mp_state_models_actor(&owner,instance,&id)==GE_MP_STATE_OK && id==owner.ids[i]);
        /* Actual ROM constructor introduces unowned header/RW/matrix resource
         * refs. The Model reader must report those rather than pretend closure. */
        assert(ge_mp_state_models_export(&owner,NULL,&output)==GE_MP_STATE_INCOMPLETE && !output.record_count);
    }
    for(unsigned i=0;i<count;++i)assert(ge_original_pitem_model_release_instance(p,(void *)defs[i].model));
    assert(ge_mp_state_models_export(&owner,NULL,&output)==GE_MP_STATE_OK && output.record_count==2*count);
    for(unsigned i=0;i<count;++i) {
        assert(ge_original_pitem_model_physical_instance(p,i,&native,&bytes,&active) && !active && native==defs[i].model);
        assert(!output.values[i].refs[1] && !output.values[i].refs[3]);
    }
    void *h,*instance;float scale,pov;uint64_t old=owner.ids[0];
    assert(ge_original_pitem_model_resolve_instance(p,184,&h,&instance,&scale) && instance==defs[0].model);
    assert(owner.ids[0]==old);
    for(unsigned i=0;i<count;++i) {
        assert(ge_original_character_model_resolve_instance(c,0,&h,&instance,&scale,&pov));
        assert(instance==defs[count+i].model);
        assert(ge_original_character_model_physical_instance(c,i,&native,&bytes,&active) && active && native==instance);
    }
    GeOriginalPitemModelStats pb={0},pa={0};GeOriginalCharacterModelStats cb={0},ca={0};
    ge_original_pitem_model_get_stats(p,&pb);ge_original_character_model_get_stats(c,&cb);
    for(unsigned i=0;i<count;++i) {
        assert(ge_original_pitem_model_physical_instance(p,i,&native,&bytes,&active));
        assert(ge_original_character_model_physical_instance(c,i,&native,&bytes,&active));
    }
    ge_original_pitem_model_get_stats(p,&pa);ge_original_character_model_get_stats(c,&ca);
    assert(!memcmp(&pa,&pb,sizeof(pa)) && !memcmp(&ca,&cb,sizeof(ca)));
    assert(ge_mp_state_models_export(&owner,NULL,&output)==GE_MP_STATE_INCOMPLETE);
    assert(ge_mp_state_models_end(&owner)==GE_MP_STATE_OK);
    ge_original_pitem_model_provider_destroy(p);ge_original_character_model_provider_destroy(c);
    assert(ge_original_prop_state_end(&pool));assert(ge_mp_state_props_end(&props)==GE_MP_STATE_OK);
    ge_asset_pack_close(&pack);
}
int main(int argc,char **argv)
{
    assert(ge_mp_state_models_native_size(GE_MP_MODEL_PREFIX)==offsetof(Model,anim));
    assert(ge_mp_state_models_native_size(GE_MP_MODEL_FULL)==sizeof(Model));
    assert(!ge_mp_state_models_native_size(255));
    registration_order();native_transitions(2);native_transitions(4);references_and_floats();lifetime();descriptor_validation();
    assert(argc==2);physical_providers(argv[1],2);physical_providers(argv[1],4);
    puts("native Model EC: 2/4 clock/loop/reset/free/reuse, prefix extent, references, callbacks, floats and stage lifetimes passed");
    return 0;
}
