#include "ge_mp_state_model_topology.h"
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

void disable_all_switches(Model *);
void set_item_visibility_in_objinstance(Model *,s32,s32);
static GeOriginalPropState pool;
static GeMpStatePropsOwner props;
static GeMpRegistry registry;
static GeMpRegistryEntry entries[4096];
static GeMpStateModelTopologyOwner owner;
static GeMpStateModelTopologySnapshot output;
static struct {const void *p;GeMpModelTopologyReferenceKind kind;uint64_t id;} external[4096];
static size_t external_count;
static Model models[4];
static ModelNode nodes[4][8],*switches[4][1];
static union ModelRoData rodata[4][8];
static ModelFileHeader headers[4][2];
static uintptr_t rw[4][128];
static RenderPosView matrices[4][1];
static GeMpModelTopologyDefinition definitions[40];
static void bind_external(GeMpModelTopologyReferenceKind kind,const void *p)
{
    if(!p)return;
    for(size_t i=0;i<external_count;++i)if(external[i].p==p && external[i].kind==kind)return;
    assert(external_count<4096);size_t i=external_count++;
    external[i].p=p;external[i].kind=kind;
    uint32_t key=ge_mp_registry_key(0x31,kind,i);
    assert(ge_mp_registry_define(&registry,key)==GE_MP_STATE_OK);
    assert(ge_mp_registry_acquire(&registry,key,&external[i].id)==GE_MP_STATE_OK);
}
static GeMpStateStatus reference(void *ctx,GeMpModelTopologyReferenceKind kind,const void *p,uint64_t *id)
{
    (void)ctx;
    for(size_t i=0;i<external_count;++i)if(external[i].p==p && external[i].kind==kind) {
        *id=external[i].id;return GE_MP_STATE_OK;
    }
    return GE_MP_STATE_INCOMPLETE;
}
static const GeMpStateModelTopologyProviders providers={NULL,reference};
static void bind_boundaries(void)
{
    /* Explicit typed resource seams only. This fixture does not attest that
     * RO data/skeletons/tables have complete immutable or mutable coverage. */
    for(size_t i=0;i<owner.count;++i) {
        const GeMpModelTopologyDefinition *d=&owner.definitions[i];
        if(d->component==GE_MP_TOPOLOGY_NODE) {
            const ModelNode *n=d->native;bind_external(GE_MP_TOPOLOGY_REF_RODATA,n->Data);
        } else {
            const ModelFileHeader *h=d->native;bind_external(GE_MP_TOPOLOGY_REF_SKELETON,h->Skeleton);
            bind_external(GE_MP_TOPOLOGY_REF_SWITCH_TABLE,h->Switches);
            bind_external(GE_MP_TOPOLOGY_REF_TEXTURE_TABLE,h->Textures);
#ifdef DEBUG
            bind_external(GE_MP_TOPOLOGY_REF_SWITCH_NAMES,h->SwitchNames);
#endif
        }
    }
}
static uint64_t actor(const void *p)
{uint64_t id;assert(ge_mp_state_model_topology_actor(&owner,p,&id)==GE_MP_STATE_OK);return id;}
static const GeMpModelTopologyValues *value(const void *p)
{
    uint64_t id=actor(p);
    for(size_t i=0;i<output.record_count;++i)if(output.records[i].actor_id==id)return &output.values[i];
    abort();
}
static void export_ok(void)
{
    assert(ge_mp_state_model_topology_export(&owner,&providers,&output)==GE_MP_STATE_OK);
    assert(output.record_count==owner.count && !output.coverage.unresolved_references && !output.coverage.unclassified_fields);
    assert(output.coverage.enumerated_systems==GE_MP_SYSTEM_ANIMATION);
    const GeMpRecordSchema *schema=ge_mp_state_model_topology_schema();
    for(size_t i=0;i<output.record_count;++i)for(size_t j=0;j<GE_MP_MODEL_TOPOLOGY_FIELDS;++j) {
        assert(output.fields[i][j].type==schema->fields[j].type && output.fields[i][j].id==schema->fields[j].id);
        assert(output.fields[i][j].count>=schema->fields[j].min_count && output.fields[i][j].count<=schema->fields[j].max_count);
    }
}
static void begin(void)
{
    external_count=0;assert(!owner.live && !props.live);
    assert(ge_original_prop_state_reset(&pool,4));
    assert(ge_mp_registry_init(&registry,entries,4096)==GE_MP_STATE_OK);
    assert(ge_mp_state_props_begin(&props,&registry)==GE_MP_STATE_OK);
    assert(ge_mp_state_model_topology_begin(&owner,&props,NULL,0)==GE_MP_STATE_OK);
}
static void end(void)
{
    assert(ge_mp_state_model_topology_end(&owner)==GE_MP_STATE_OK);
    assert(ge_original_prop_state_end(&pool));assert(ge_mp_state_props_end(&props)==GE_MP_STATE_OK);
}
static void native_topology(unsigned count)
{
    begin();memset(nodes,0,sizeof(nodes));memset(rodata,0,sizeof(rodata));memset(headers,0,sizeof(headers));
    memset(models,0,sizeof(models));memset(rw,0,sizeof(rw));memset(matrices,0,sizeof(matrices));
    for(unsigned i=0;i<count;++i) {
        ModelNode *n=nodes[i];union ModelRoData *r=rodata[i];ModelFileHeader *h=headers[i];
        for(unsigned j=0;j<8;++j)n[j].Data=&r[j];
        n[0].Opcode=MODELNODE_OPCODE_HEADER;n[0].Child=&n[1];
        n[1].Opcode=MODELNODE_OPCODE_LOD;n[1].Parent=&n[0];n[1].Next=&n[6];n[1].Child=&n[2];r[1].LOD.Affects=&n[2];
        n[2].Opcode=MODELNODE_OPCODE_SWITCH;n[2].Parent=&n[1];n[2].Child=&n[3];r[2].Switch.Controls=&n[3];
        n[3].Opcode=MODELNODE_OPCODE_BSP;n[3].Parent=&n[2];n[3].Child=&n[4];
        r[3].BSP.leftChild=&n[4];r[3].BSP.rightChild=&n[5];
        n[4].Parent=&n[3];n[4].Next=&n[5];n[5].Parent=&n[3];n[5].Prev=&n[4];
        n[6].Opcode=MODELNODE_OPCODE_HEAD;n[6].Parent=&n[0];n[6].Prev=&n[1];
        n[7].Opcode=MODELNODE_OPCODE_DLCOLLISION;
        switches[i][0]=&n[2];h[0].RootNode=&n[0];h[0].Switches=switches[i];h[0].numSwitches=1;
        h[0].numMatrices=1;h[0].BoundingVolumeRadius=10;h[1].RootNode=&n[7];
        for(unsigned j=0;j<8;++j)definitions[i*10+j]=(GeMpModelTopologyDefinition){&n[j],sizeof(ModelNode),i,GE_MP_TOPOLOGY_NODE,j};
        for(unsigned j=0;j<2;++j)definitions[i*10+8+j]=(GeMpModelTopologyDefinition){&h[j],sizeof(ModelFileHeader),i,GE_MP_TOPOLOGY_HEADER,8+j};
    }
    assert(ge_mp_state_model_topology_add(&owner,definitions,count*10)==GE_MP_STATE_OK);bind_boundaries();export_ok();
    for(unsigned i=0;i<count;++i) {
        ModelNode *n=nodes[i];union ModelRoData *r=rodata[i];ModelFileHeader *h=headers[i];Model *m=&models[i];
        uint64_t root=actor(&n[0]);
        assert(value(&h[0])->counts[2]==0);
        modelCalculateRwDataLen(&h[0]);modelCalculateRwDataLen(&h[1]);modelInit(m,&h[0],(u32 *)rw[i]);
        m->render_pos=matrices[i];export_ok();
        assert(value(&h[0])->counts[2]==h[0].numRecords && h[0].numRecords>0);
        assert(value(&n[3])->refs[4]==actor(&n[5]) && value(&n[5])->refs[2]==actor(&n[4]));
        assert(!value(&n[5])->refs[3] && value(&n[4])->refs[3]==actor(&n[5]));
        /* Exact source distance producer changes both visible and Child. */
        modelSetDistanceDisabled(1);r[1].LOD.MinDistance=0;r[1].LOD.MaxDistance=-1;
        modelUpdateDistanceRelations(m,&n[1]);export_ok();assert(!value(&n[1])->refs[4]);
        r[1].LOD.MaxDistance=1;modelUpdateDistanceRelations(m,&n[1]);export_ok();assert(value(&n[1])->refs[4]==actor(&n[2]));
        disable_all_switches(m);modelApplyToggleRelations(m,&n[2]);export_ok();assert(!value(&n[2])->refs[4]);
        set_item_visibility_in_objinstance(m,0,1);modelApplyToggleRelations(m,&n[2]);export_ok();
        assert(value(&n[2])->refs[4]==actor(&n[3]));
        modelApplyReorderRelationsByArg(&n[3],TRUE);export_ok();
        assert(value(&n[3])->refs[4]==actor(&n[4]) && value(&n[4])->refs[2]==actor(&n[5]));
        assert(value(&n[5])->refs[3]==actor(&n[4]) && !value(&n[5])->refs[2]);
        /* Head attachment mutates the body header, node ancestry and its RW
         * base. The actual native lookup must select the newly attached span. */
        s16 before=h[0].numRecords;modelAttachHead(m,&n[6],&h[1]);export_ok();
        assert(h[0].numRecords==before+h[1].numRecords && value(&h[0])->counts[2]==h[0].numRecords);
        assert(value(&n[6])->refs[4]==actor(&n[7]) && value(&n[7])->refs[1]==actor(&n[6]));
        ModelRwData_HeadPlaceholderRecord *head=&modelGetNodeRwData(m,&n[6])->HeadPlaceholder;
        assert(head->ModelFileHeader==&h[1] && head->RwDatas==&m->datas[before]);
        assert(modelGetNodeRwData(m,&n[7])==(union ModelRwData *)head->RwDatas);
        n[6].Child=NULL;n[7].Parent=NULL;modelApplyHeadRelations(m,&n[6]);export_ok();
        assert(value(&n[6])->refs[4]==actor(&n[7]) && value(&n[7])->refs[1]==actor(&n[6]));
        assert(actor(&n[0])==root);
        /* Other viewport/model graphs have independent physical ownership. */
        if(i+1<count)assert(headers[i+1][0].numRecords==0 && nodes[i+1][6].Child==NULL);
    }
    end();
}
static void validation(void)
{
    begin();ModelNode n[4]={0};ModelFileHeader h={0};
    GeMpModelTopologyDefinition d[]={{&n[0],sizeof(n[0]),2,GE_MP_TOPOLOGY_NODE,0},
        {&n[1],sizeof(n[1]),2,GE_MP_TOPOLOGY_NODE,2},{&h,sizeof(h),2,GE_MP_TOPOLOGY_HEADER,4}};
    assert(ge_mp_state_model_topology_add(&owner,d,3)==GE_MP_STATE_OK);export_ok();
    uint64_t old_ids[3]={actor(&n[0]),actor(&n[1]),actor(&h)};
    GeMpModelTopologyDefinition extra[]={{&n[2],sizeof(n[2]),2,GE_MP_TOPOLOGY_NODE,1},
        {&n[3],sizeof(n[3]),3,GE_MP_TOPOLOGY_NODE,0}};
    static GeMpStateModelTopologyOwner before;static GeMpRegistryEntry saved[4096];
    before=owner;memcpy(saved,entries,sizeof(entries));size_t reg_count=registry.count,cap=registry.capacity;
    registry.capacity=registry.count+1;
    assert(ge_mp_state_model_topology_add(&owner,extra,2)==GE_MP_STATE_CAPACITY);
    registry.capacity=cap;assert(!memcmp(&before,&owner,sizeof(owner)) && !memcmp(saved,entries,sizeof(entries)));
    for(unsigned bad=0;bad<5;++bad) {
        GeMpModelTopologyDefinition b[2]={extra[0],extra[1]};
        if(bad==0)b[0].bytes--;
        if(bad==1)b[0].component=255;
        if(bad==2)b[0].native=&n[0];
        if(bad==3)b[0].slot=0;
        if(bad==4)b[1].creation_namespace=1;
        assert(ge_mp_state_model_topology_add(&owner,b,2)==GE_MP_STATE_INVALID);
        assert(!memcmp(&before,&owner,sizeof(owner)) && !memcmp(saved,entries,sizeof(entries)) && registry.count==reg_count);
    }
    assert(ge_mp_state_model_topology_add(&owner,owner.definitions,1)==GE_MP_STATE_INVALID);
    assert(ge_mp_state_model_topology_add(&owner,extra,2)==GE_MP_STATE_OK);export_ok();
    assert(actor(&n[0])==old_ids[0] && actor(&n[1])==old_ids[1] && actor(&h)==old_ids[2]);
    assert(owner.definitions[1].native==&n[2] && owner.definitions[4].native==&n[3]);
    assert(ge_mp_state_model_topology_check(&owner,&n[0],GE_MP_TOPOLOGY_NODE)==GE_MP_STATE_OK);
    assert(ge_mp_state_model_topology_check(&owner,&h,GE_MP_TOPOLOGY_NODE)==GE_MP_STATE_INVALID);
    n[0].Opcode=0xAB12;h.numSwitches=-2;h.numMatrices=3;h.numRecords=12;h.numtextures=4;h.isLoaded=-1;
    h.BoundingVolumeRadius=-0.0f;export_ok();assert(signbit(value(&h)->radius));
    assert(value(&n[0])->opcode==0xAB12 && value(&h)->counts[0]==-2 && value(&h)->is_loaded==-1);
    h.BoundingVolumeRadius=NAN;assert(ge_mp_state_model_topology_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE && !output.record_count);
    h.BoundingVolumeRadius=INFINITY;assert(ge_mp_state_model_topology_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);
    h.BoundingVolumeRadius=15;export_ok();
    union ModelRoData ro={0};n[0].Data=&ro;
    assert(ge_mp_state_model_topology_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);
    assert(output.unresolved_kinds==(1U<<GE_MP_TOPOLOGY_REF_RODATA));bind_boundaries();export_ok();
    uint64_t mapped=external[0].id;assert(ge_mp_registry_release(&registry,mapped)==GE_MP_STATE_OK);
    assert(ge_mp_state_model_topology_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);
    assert(ge_mp_registry_acquire(&registry,(uint32_t)(mapped>>32),&external[0].id)==GE_MP_STATE_OK);export_ok();
    ModelNode foreign={0};n[0].Child=&foreign;
    assert(ge_mp_state_model_topology_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);
    assert(output.unresolved_kinds==(1U<<GE_MP_TOPOLOGY_REF_NODE));n[0].Child=NULL;export_ok();
    before=owner;memcpy(saved,entries,sizeof(entries));
    for(unsigned bad=0;bad<6;++bad) {
        owner=before;
        if(bad==0)owner.ids[owner.count-1]=external[0].id;
        if(bad==1)owner.ids[owner.count-1]^=UINT64_C(1)<<48;
        if(bad==2)owner.ids[owner.count-1]^=UINT64_C(1)<<32;
        if(bad==3)owner.count=GE_MP_MODEL_TOPOLOGY_CAPACITY+1;
        if(bad==4)owner.definitions[owner.count-1].component=0;
        if(bad==5)owner.ids[owner.count]=owner.ids[0];
        assert(ge_mp_state_model_topology_end(&owner)==GE_MP_STATE_INVALID);
        assert(!memcmp(saved,entries,sizeof(entries)));
    }
    owner=before;uint64_t old=owner.ids[0],new_id;
    assert(ge_mp_registry_release(&registry,old)==GE_MP_STATE_OK);
    assert(ge_mp_registry_acquire(&registry,(uint32_t)(old>>32),&new_id)==GE_MP_STATE_OK);
    before=owner;memcpy(saved,entries,sizeof(entries));
    assert(ge_mp_state_model_topology_add(&owner,NULL,0)==GE_MP_STATE_STALE);
    assert(!memcmp(&before,&owner,sizeof(owner)) && !memcmp(saved,entries,sizeof(entries)));
    assert(ge_mp_state_model_topology_end(&owner)==GE_MP_STATE_STALE);
    assert(ge_mp_registry_resolve(&registry,new_id,NULL)==GE_MP_STATE_OK);
    assert(ge_original_prop_state_end(&pool));assert(ge_mp_state_props_end(&props)==GE_MP_STATE_OK);
    begin();assert(ge_mp_state_model_topology_add(&owner,d,3)==GE_MP_STATE_OK);
    assert(ge_original_prop_state_end(&pool));
    assert(ge_mp_state_model_topology_export(&owner,&providers,&output)==GE_MP_STATE_STALE);
    assert(ge_mp_state_model_topology_end(&owner)==GE_MP_STATE_STALE);assert(ge_mp_state_props_end(&props)==GE_MP_STATE_OK);
    begin();assert(ge_mp_state_model_topology_add(&owner,d,3)==GE_MP_STATE_OK);
    assert(ge_original_prop_state_reset(&pool,4));
    assert(ge_mp_state_model_topology_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);
    assert(ge_mp_state_model_topology_add(&owner,NULL,0)==GE_MP_STATE_INCOMPLETE);
    assert(ge_mp_state_model_topology_end(&owner)==GE_MP_STATE_INCOMPLETE);
    assert(ge_mp_state_props_end(&props)!=GE_MP_STATE_OK);assert(ge_original_prop_state_end(&pool));
}
static void add_view(const GeOriginalModelTopologyView *view,uint8_t ns)
{
    assert(view->node_count<500 && view->header_bytes==sizeof(ModelFileHeader) && view->node_stride==sizeof(ModelNode));
    GeMpModelTopologyDefinition defs[501];
    defs[0]=(GeMpModelTopologyDefinition){view->header,view->header_bytes,ns,GE_MP_TOPOLOGY_HEADER,0};
    for(size_t i=0;i<view->node_count;++i) {
        assert(view->source_node_offsets[i]<0x1000000 && view->rodata_stride==sizeof(union ModelRoData));
        defs[i+1]=(GeMpModelTopologyDefinition){(const char *)view->nodes+i*view->node_stride,view->node_stride,ns,GE_MP_TOPOLOGY_NODE,i+1};
    }
    assert(ge_mp_state_model_topology_add(&owner,defs,view->node_count+1)==GE_MP_STATE_OK);
}
static void physical_providers(const char *path,unsigned count)
{
    begin();GeAssetPack pack;GeOriginalPitemModelStatus ps;GeOriginalCharacterModelStatus cs;
    assert(ge_asset_pack_open(&pack,path)==GE_ASSET_PACK_OK);
    GeOriginalPitemModelProvider *p=ge_original_pitem_model_provider_create(&pack,2,count,&ps);
    GeOriginalCharacterModelProvider *c=ge_original_character_model_provider_create(&pack,2,count,&cs);
    assert(p && c && !ps && !cs);GeOriginalModelTopologyView shared,view,body_resource;
    assert(!ge_original_pitem_model_resource_topology(p,184,&view));
    assert(!ge_original_character_model_instance_topology(c,0,0,&view));
    int body=-1;for(size_t i=0;i<ge_original_character_model_dependency_count();++i) {
        GeOriginalCharacterModelMetadata m;assert(ge_original_character_model_dependency_metadata(i,&m));
        if(m.is_body_dependency && !m.has_integrated_head && m.model_id!=BODY_Brosnan_Tuxedo){body=m.model_id;break;}
    }
    assert(body>=0);
    void *items[4];
    for(unsigned i=0;i<count;++i) {
        void *h;float scale;assert(ge_original_pitem_model_resolve_instance(p,184,&h,&items[i],&scale));
        assert(ge_original_pitem_model_resource_topology(p,184,&view) && view.header==h);
        if(!i){shared=view;add_view(&view,30);}else assert(view.nodes==shared.nodes && view.header==shared.header);
        GeOriginalCharacterModelPair pair;assert(ge_original_character_model_resolve_pair(c,body,42,0,&pair));
        assert(ge_original_character_model_resource_topology(c,body,&body_resource));
        for(unsigned part=0;part<2;++part) {
            assert(ge_original_character_model_instance_topology(c,i,part,&view));
            assert(view.header!=body_resource.header && view.nodes!=body_resource.nodes);
            if(!part)assert(pair.model_header==view.header && view.source_node_offsets==body_resource.source_node_offsets);
            add_view(&view,40+i*2+part);
        }
    }
    /* Cached resource objects are separate from each instance's copied graph. */
    add_view(&body_resource,60);assert(ge_original_character_model_resource_topology(c,42,&view));add_view(&view,61);
    size_t bytes;const void *gait_header=ge_original_player_gait_topology_header(&bytes);
    GeMpModelTopologyDefinition gait[5]={{gait_header,bytes,70,GE_MP_TOPOLOGY_HEADER,0}};
    assert(ge_original_player_gait_topology_node_count()==4);
    for(size_t i=0;i<4;++i) {
        const void *node;assert(ge_original_player_gait_topology_node(i,&node,&bytes));
        gait[i+1]=(GeMpModelTopologyDefinition){node,bytes,70,GE_MP_TOPOLOGY_NODE,i+1};
    }
    assert(!ge_original_player_gait_topology_node(4,&gait_header,&bytes));
    assert(ge_mp_state_model_topology_add(&owner,gait,5)==GE_MP_STATE_OK);
    assert(ge_mp_state_model_topology_export(&owner,NULL,&output)==GE_MP_STATE_INCOMPLETE && !output.record_count);
    bind_boundaries();export_ok();uint64_t shared_id=actor(shared.header);size_t physical_count=owner.count;
    GeMpStateSchema schema={1,GE_MP_SYSTEM_ANIMATION,0,ge_mp_state_model_topology_schema(),1};
    GeMpCanonicalSnapshot snapshot={.session=1,.schema=&schema,.registry=&registry,.records=output.records,.record_count=output.record_count};
    uint8_t digest[32],saved_digest[32];memset(digest,0x7a,32);memcpy(saved_digest,digest,32);
    assert(ge_mp_state_sha256(&snapshot,digest,NULL)!=GE_MP_STATE_OK && !memcmp(digest,saved_digest,32));
    for(unsigned i=0;i<count;++i)assert(ge_original_pitem_model_release_instance(p,items[i]));
    export_ok();assert(owner.count==physical_count && actor(shared.header)==shared_id);
    assert(ge_original_pitem_model_resource_topology(p,184,&view) && view.header==shared.header && view.nodes==shared.nodes);
    GeOriginalPitemModelStats pb={0},pa={0};GeOriginalCharacterModelStats cb={0},ca={0};
    ge_original_pitem_model_get_stats(p,&pb);ge_original_character_model_get_stats(c,&cb);
    assert(ge_original_pitem_model_resource_topology(p,184,&view));
    for(unsigned i=0;i<count;++i)assert(ge_original_character_model_instance_topology(c,i,1,&view));
    ge_original_pitem_model_get_stats(p,&pa);ge_original_character_model_get_stats(c,&ca);
    assert(!memcmp(&pa,&pb,sizeof(pa)) && !memcmp(&ca,&cb,sizeof(ca)));
    assert(ge_mp_state_model_topology_end(&owner)==GE_MP_STATE_OK);
    ge_original_pitem_model_provider_destroy(p);ge_original_character_model_provider_destroy(c);
    assert(ge_original_prop_state_end(&pool));assert(ge_mp_state_props_end(&props)==GE_MP_STATE_OK);ge_asset_pack_close(&pack);
}
int main(int argc,char **argv)
{
    assert(argc==2);native_topology(2);native_topology(4);validation();physical_providers(argv[1],2);physical_providers(argv[1],4);
    puts("native ED topology: 2/4 LOD/switch/BSP/head mutation, source RW redirection, shared/cloned/static physical owners and transactional lifetimes passed");
    return 0;
}
