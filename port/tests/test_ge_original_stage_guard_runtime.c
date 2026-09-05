#include "ge_asset_pack.h"
#include "ge_original_stage_guard_runtime.h"
#include "ge_original_global_ai.h"
#include "ge_original_stage_active_props.h"
#include "ge_original_stage_interactive_objects.h"
#include "ge_original_stage_setup.h"
#include "ge_stage_assets.h"
#include "ge_stan_collision.h"
#include "ge_stan_native.h"

#include <bondconstants.h>
#include <bondtypes.h>
#ifndef PLAYERFLAG
typedef int PLAYERFLAG;
#endif
#include "game/player.h"
#include "game/chraction.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Harness {
    uint8_t resident_room;
    uint8_t all_rooms_resident;
    size_t head_choices;
    size_t sunglasses_choices;
    size_t matrix_updates;
    size_t projectile_loads;
} Harness;

typedef struct SurfaceClosedObjectAudit {
    size_t constructed;
    unsigned fallback_command_mask;
} SurfaceClosedObjectAudit;

typedef struct AssignedItemResolver {
    GeOriginalStageGuardRuntime *runtime;
} AssignedItemResolver;

static int resolve_assigned_item(
    void *context,const GeOriginalStagePropConstructionRequest *request,
    void **prop,void **model)
{
    AssignedItemResolver *resolver=context;size_t index,count;
    if(resolver==NULL||resolver->runtime==NULL||request==NULL
            ||prop==NULL||model==NULL)return 0;
    count=ge_original_stage_guard_runtime_active_prop_count(resolver->runtime);
    for(index=0U;index<count;++index){
        size_t command;void *candidate;
        PropRecord *candidate_prop;
        if(!ge_original_stage_guard_runtime_active_prop(
                resolver->runtime,index,&command,&candidate)
                ||command!=request->command_index)continue;
        candidate_prop=candidate;
        if(candidate_prop==NULL||candidate_prop->obj==NULL
                ||candidate_prop->obj->model==NULL)return 0;
        *prop=candidate_prop;*model=candidate_prop->obj->model;return 1;
    }
    return 0;
}

static int surface_model_available(void *context,int32_t model_id)
{(void)context;return model_id>=0;}

static int surface_construct_ready(
    void *context,const GeOriginalStagePropConstructionRequest *request)
{
    SurfaceClosedObjectAudit *audit=context;
    assert(audit!=NULL&&request!=NULL);
    if(request->command_index>=76U&&request->command_index<=78U)
        audit->fallback_command_mask|=
            1U<<(unsigned)(request->command_index-76U);
    ++audit->constructed;
    return 1;
}

s32 g_GlobalTimer=120;
s32 g_ClockTimer=1;
f32 g_GlobalTimerDelta=1.0f;
f32 slider_007_mode_health=1.0f;
PropRecord *g_ActivePropsHead;
PropRecord *g_ActivePropsTail;
ChrRecord *g_ChrSlots;
s32 g_NumChrSlots;
s32 g_OnScreenPropCount;
PropRecord *g_OnScreenPropList[MAX_PROPS+1];
PropRecord **g_LastOnScreenProp=g_OnScreenPropList;
stagesetup g_CurrentSetup;
static struct player stage_player;
static size_t player_body_merge_calls;
static size_t campaign_attached_hats;
static uint8_t campaign_hat_models[340];
struct player *g_CurrentPlayer=&stage_player;
struct player *g_playerPointers[4];

extern PropRecord *chrSpawnAtCoord(s32 bodynum,s32 headnum,coord3d *pos,
    StandTile *stan,f32 angle,AIListRecord *ailist,s32 spawnflags);
extern PropRecord *hatCreateForChr(ChrRecord *chr,s32 modelnum,u32 flags);

/* Exact chraiUpdateOnscreenPropCount traversal, kept local so this focused
 * real-asset fixture does not pull in the unrelated full chrprop scheduler. */
static void test_update_onscreen_prop_count_exact(void)
{
    PropRecord *prop= g_ActivePropsTail;
    s32 count=0;
    for(;prop!=NULL;prop=prop->prev){
        if((prop->flags&(PROPFLAG_ENABLED|PROPFLAG_ONSCREEN))
                ==(PROPFLAG_ENABLED|PROPFLAG_ONSCREEN)){
            assert(count<MAX_PROPS);
            g_OnScreenPropList[count++]=prop;
        }
    }
    g_OnScreenPropCount=count;
    g_OnScreenPropList[count]=NULL;
    g_LastOnScreenProp=&g_OnScreenPropList[count];
}

/* Exact chrGetEquippedWeaponProp body. */
static PropRecord *test_chr_get_equipped_weapon_prop_exact(
    ChrRecord *chr,GUNHAND hand)
{
    return chr->weapons_held[hand];
}

int ge_dam_setup_world_definition_header(
    const void *definition,uint16_t *extrascale,uint8_t *state,uint8_t *type)
{
    (void)definition;(void)extrascale;(void)state;(void)type;return 0;
}

int ge_dam_setup_world_definition_set_state(void *definition,uint8_t state)
{(void)definition;(void)state;return 0;}

void ge_original_dam_guard_props_tick_exact(void)
{assert(!"structural all-stage composer must not tick unbound services");}

void chrlvMergeKneelToStand(ChrRecord *self,f32 mergetime)
{
    /* Animation semantics have dedicated exact-body coverage. This fixture
     * deliberately keeps that service bounded while exercising the real
     * body/head resource, ChrRecord, matrix and scene ownership graph. */
    assert(self!=NULL&&isfinite(mergetime));
    ++player_body_merge_calls;
}

DIFFICULTY lvlGetSelectedDifficulty(void)
{return DIFFICULTY_AGENT;}

s32 sub_GAME_7F01FC10(Model *model,coord3d *src,coord3d *dst,f32 *ground)
{
    assert(model!=NULL&&src!=NULL&&dst!=NULL&&ground!=NULL);
    *dst=*src;*ground=src->y;return 0;
}

void set_color_shading_from_tile(PropRecord *prop,u8 col[4])
{
    assert(prop!=NULL&&prop->stan!=NULL&&col!=NULL);
    col[0]=prop->stan->mid.headerMid.r*17U;
    col[1]=prop->stan->mid.headerMid.g*17U;
    col[2]=prop->stan->mid.headerMid.b*17U;
    col[3]=0U;
}

static int tile_rgb(void *context,void *stan,float x,float z,uint8_t rgb[3])
{
    StandTile *tile=stan;(void)context;(void)x;(void)z;
    assert(tile!=NULL&&rgb!=NULL);
    rgb[0]=tile->mid.headerMid.r*17U;
    rgb[1]=tile->mid.headerMid.g*17U;
    rgb[2]=tile->mid.headerMid.b*17U;
    return 1;
}

void ge_original_dam_guard_chr_detect_rooms_exact(ChrRecord *chr)
{
    assert(chr!=NULL&&chr->prop!=NULL&&chr->prop->stan!=NULL);
    chr->prop->rooms[0]=chr->prop->stan->room;
    chr->prop->rooms[1]=UINT8_MAX;
}

static union ModelRwData *test_node_rw(Model *model,ModelNode *node)
{
    union ModelRwData **data=model->datas;ModelNode *parent=node;
    int32_t index=0;
    switch(node->Opcode&0xffU){
    case MODELNODE_OPCODE_HEADER:index=node->Data->Header.RwDataIndex;break;
    case MODELNODE_OPCODE_LOD:index=node->Data->LOD.RwDataIndex;break;
    case MODELNODE_OPCODE_BSP:index=node->Data->BSP.RwDataIndex;break;
    case MODELNODE_OPCODE_SWITCH:index=node->Data->Switch.RwDataIndex;break;
    case MODELNODE_OPCODE_HEAD:index=node->Data->HeadPlaceholder.RwDataIndex;break;
    case MODELNODE_OPCODE_DLCOLLISION:
        index=node->Data->DisplayListCollisions.RwDataIndex;break;
    default:break;
    }
    while(parent->Parent!=NULL){
        parent=parent->Parent;
        if((parent->Opcode&0xffU)==MODELNODE_OPCODE_HEAD){
            ModelRwData_HeadPlaceholderRecord *head=
                &test_node_rw(model,parent)->HeadPlaceholder;
            data=head->RwDatas;break;
        }
    }
    return (union ModelRwData *)(void *)&data[index];
}

Mtxf *modelFindNodeMtx(Model *model,ModelNode *node,s32 arg2)
{
    while(model!=NULL&&node!=NULL){
        switch(node->Opcode&0xffU){
        case MODELNODE_OPCODE_HEADER:
            return &model->render_pos[node->Data->Header.MatrixIndex].pos;
        case MODELNODE_OPCODE_GROUP:
            return &model->render_pos[node->Data->Group.MatrixIDs[
                arg2==0x200?2:(arg2==0x100?1:0)]].pos;
        case MODELNODE_OPCODE_GROUPSIMPLE:
            return &model->render_pos[node->Data->GroupSimple.Group1].pos;
        default:node=node->Parent;break;
        }
    }
    return NULL;
}

/* Reproduce chrGetOnscreenRenderBounds' exact BBOX consumption without
 * linking the unrelated full chrprop scheduler graph into this focused
 * real-asset runtime test.  Enumerating all eight corners is algebraically
 * identical to the original chrpropSumMatrix{Neg,Pos}{X,Y} selection. */
static int test_model_axis_extents(
    Model *model,int axis,float *maximum,float *minimum)
{
    ModelNode *node;
    int found=0;
    assert(model!=NULL&&model->obj!=NULL&&maximum!=NULL&&minimum!=NULL);
    node=model->obj->RootNode;
    while(node!=NULL){
        if((node->Opcode&0xffU)==MODELNODE_OPCODE_BBOX){
            const ModelRoData_BoundingBoxRecord *bbox=&node->Data->BoundingBox;
            Mtxf *matrix=modelFindNodeMtx(model,node,0);
            const float bounds[3][2]={
                {bbox->Bounds.xmin,bbox->Bounds.xmax},
                {bbox->Bounds.ymin,bbox->Bounds.ymax},
                {bbox->Bounds.zmin,bbox->Bounds.zmax}
            };
            unsigned corner;
            assert(matrix!=NULL);
            for(corner=0U;corner<8U;++corner){
                float value=matrix->m[3][axis];
                value+=bounds[0][corner&1U]*matrix->m[0][axis];
                value+=bounds[1][(corner>>1)&1U]*matrix->m[1][axis];
                value+=bounds[2][(corner>>2)&1U]*matrix->m[2][axis];
                if(!found||value>*maximum)*maximum=value;
                if(!found||value<*minimum)*minimum=value;
                found=1;
            }
        }
        if(node->Child!=NULL)node=node->Child;
        else{
            while(node!=NULL&&node->Next==NULL)node=node->Parent;
            if(node!=NULL)node=node->Next;
        }
    }
    return found;
}

static int test_chr_get_onscreen_render_bounds_exact(
    PropRecord *prop,coord3d *aim,coord2d *xbounds,coord2d *ybounds)
{
    ChrRecord *chr;
    Model *model;
    RenderPosView *root,*second;
    assert(prop!=NULL&&aim!=NULL&&xbounds!=NULL&&ybounds!=NULL);
    chr=prop->chr;
    if((prop->flags&PROPFLAG_ONSCREEN)==0U||chr==NULL
            ||chr->actiontype==ACT_DIE||chr->actiontype==ACT_DEAD
            ||(chr->chrflags&CHRFLAG_NO_AUTOAIM)!=0U)return 0;
    model=chr->model;
    assert(model!=NULL&&model->render_pos!=NULL&&model->obj!=NULL
           &&model->obj->numMatrices>=2);
    root=&model->render_pos[0];second=&model->render_pos[1];
    aim->z=second->pos.m[3][2]
        +(root->pos.m[3][2]-second->pos.m[3][2])*0.25f;
    if(aim->z>=0.0f)return 0;
    aim->x=second->pos.m[3][0]
        +(root->pos.m[3][0]-second->pos.m[3][0])*0.25f;
    aim->y=second->pos.m[3][1]
        +(root->pos.m[3][1]-second->pos.m[3][1])*0.25f;
    xbounds->x=xbounds->y=ybounds->x=ybounds->y=0.0f;
    assert(test_model_axis_extents(model,0,&xbounds->y,&xbounds->x));
    assert(test_model_axis_extents(model,1,&ybounds->y,&ybounds->x));
    return 1;
}

/* Focused host boundary for the unchanged subcalcmatrices call. The complete
 * canonical function is linked by the ARM target; this test double makes its
 * invocation and matrix-bank ownership observable without importing the
 * unrelated animation decoder graph. */
void subcalcmatrices(ModelRenderData *renderdata, Model *model)
{
    ModelNode *root=model->obj->RootNode;
    union ModelRwData **words=model->datas;
    ModelRwData_HeaderRecord *header=NULL;
    size_t index,row,column,visited=0U;ModelNode *node;
    assert(renderdata!=NULL&&renderdata->basemtx!=NULL&&model!=NULL);
    if((root->Opcode&0xffU)==MODELNODE_OPCODE_HEADER)
        header=(ModelRwData_HeaderRecord *)(void *)
            &words[root->Data->Header.RwDataIndex];
    model->render_pos=(RenderPosView *)(void *)renderdata->mtxlist;
    /* Publish the canonical distance-relation outcome needed by this host
     * scene test. Production runs the unchanged modelUpdateMatrices body. */
    node=model->obj->RootNode;
    while(node!=NULL&&visited++<512U){
        if((node->Opcode&0xffU)==MODELNODE_OPCODE_LOD){
            test_node_rw(model,node)->LOD.visible=TRUE;
            node->Child=node->Data->LOD.Affects;
        }
        if(node->Child!=NULL)node=node->Child;
        else{
            while(node!=NULL&&node->Next==NULL)node=node->Parent;
            if(node!=NULL)node=node->Next;
        }
    }
    assert(node==NULL);
    for(index=0;index<(size_t)model->obj->numMatrices;++index){
        Mtxf *matrix=&model->render_pos[index].pos;
        for(row=0;row<4U;++row)for(column=0;column<4U;++column)
            matrix->m[row][column]=renderdata->basemtx->m[row][column];
        if(header!=NULL){
            matrix->m[3][0]+=header->pos.x;
            matrix->m[3][1]+=header->pos.y;
            matrix->m[3][2]+=header->pos.z;
        }else if((root->Opcode&0xffU)==MODELNODE_OPCODE_GROUPSIMPLE){
            matrix->m[3][0]+=root->Data->GroupSimple.Origin.x;
            matrix->m[3][1]+=root->Data->GroupSimple.Origin.y;
            matrix->m[3][2]+=root->Data->GroupSimple.Origin.z;
        }
    }
}

void modelCalculateRwDataLen(ModelFileHeader *header)
{(void)header;}

/* Exact model.c getinstsize body.  The ARM target links the canonical model
 * service; this focused runtime fixture supplies the same calculation so the
 * diagnostic snapshot can expose chrTestHit's depth/radius gate. */
f32 getinstsize(Model *model)
{
    assert(model!=NULL&&model->obj!=NULL);
    return model->obj->BoundingVolumeRadius*model->scale;
}

static int choose_head(void *context,int32_t body_id,int32_t *head_id)
{
    Harness *harness=context;
    assert(body_id>=0&&head_id!=NULL);*head_id=42;
    ++harness->head_choices;return 1;
}

static int choose_sunglasses(void *context,uint16_t flags,int *sunglasses)
{
    Harness *harness=context;
    assert((flags&2U)!=0U&&sunglasses!=NULL);*sunglasses=0;
    ++harness->sunglasses_choices;return 1;
}

static int room_resident(void *context,uint8_t room)
{
    Harness *harness=context;
    return harness->all_rooms_resident
        ||room==harness->resident_room;
}

static int load_projectile_models(void *context,int32_t weapon_id)
{
    Harness *harness=context;
    assert(weapon_id>ITEM_UNARMED);
    ++harness->projectile_loads;return 1;
}

static void identity(float matrix[4][4])
{
    size_t row,column;
    for(row=0;row<4U;++row)for(column=0;column<4U;++column)
        matrix[row][column]=row==column?1.0f:0.0f;
}

static void assert_conservative_guard_draw_frustum(void)
{
    float view[4][4];
    float center[3]={0.0f,0.0f,-100.0f};
    const float fov=60.0f,aspect=5.0f/3.0f,near_distance=10.0f;
    const float tan_vertical=tanf(fov*(M_PI_F/360.0f));
    const float tan_horizontal=tan_vertical*aspect;
    const float horizontal_normal=sqrtf(1.0f+tan_horizontal*tan_horizontal);
    const float vertical_normal=sqrtf(1.0f+tan_vertical*tan_vertical);
    identity(view);
    assert(ge_original_stage_guard_draw_sphere_visible(
        view,fov,aspect,near_distance,center,10.0f));
    center[2]=20.0f;
    assert(!ge_original_stage_guard_draw_sphere_visible(
        view,fov,aspect,near_distance,center,5.0f));
    /* Tangency at the near plane and every side plane remains published. */
    center[0]=0.0f;center[1]=0.0f;center[2]=-5.0f;
    assert(ge_original_stage_guard_draw_sphere_visible(
        view,fov,aspect,near_distance,center,5.0f));
    center[2]=-100.0f;center[0]=100.0f*tan_horizontal
        +10.0f*horizontal_normal;
    assert(ge_original_stage_guard_draw_sphere_visible(
        view,fov,aspect,near_distance,center,10.0f));
    center[0]+=0.01f;
    assert(!ge_original_stage_guard_draw_sphere_visible(
        view,fov,aspect,near_distance,center,10.0f));
    center[0]=0.0f;center[1]=100.0f*tan_vertical
        +10.0f*vertical_normal;
    assert(ge_original_stage_guard_draw_sphere_visible(
        view,fov,aspect,near_distance,center,10.0f));
    center[1]+=0.01f;
    assert(!ge_original_stage_guard_draw_sphere_visible(
        view,fov,aspect,near_distance,center,10.0f));
    center[0]=center[1]=0.0f;center[2]=20.0f;
    assert(ge_original_stage_guard_draw_sphere_visible(
        view,0.0f,aspect,near_distance,center,0.0f));
}

static void assert_cached_scene_matches(
    const GeDamRoomSceneStorage *canonical,
    const GeDamRoomSceneStorage *cached,size_t vertices,size_t batches)
{
    size_t index,axis;
    for(index=0U;index<vertices;++index){
        assert(memcmp(&canonical->vertices[index].source,
            &cached->vertices[index].source,
            sizeof(canonical->vertices[index].source))==0);
        assert(memcmp(canonical->vertices[index].processed.rgba,
            cached->vertices[index].processed.rgba,4U)==0);
        assert(canonical->vertices[index].processed.texture[0]
                ==cached->vertices[index].processed.texture[0]
            &&canonical->vertices[index].processed.texture[1]
                ==cached->vertices[index].processed.texture[1]);
        for(axis=0U;axis<3U;++axis)
            assert(fabsf(canonical->vertices[index].world[axis]
                -cached->vertices[index].world[axis])<0.0001f);
    }
    assert(memcmp(canonical->batches,cached->batches,
        batches*sizeof(*canonical->batches))==0);
}

static uint8_t *read_asset(GeAssetPack *pack,const char *path,size_t *size)
{
    const GeAssetPackEntry *entry=ge_asset_pack_find(pack,path);uint8_t *data;
    assert(entry!=NULL&&entry->data_size>0U&&entry->data_size<=SIZE_MAX);
    *size=(size_t)entry->data_size;data=malloc(*size);assert(data!=NULL);
    assert(ge_asset_pack_read(pack,path,data,*size,NULL)==GE_ASSET_PACK_OK);
    return data;
}

static void assert_authored_stan_binding(
    const GeOriginalStageSetupRuntime *setup)
{
    size_t index,resolvable_pads=0U,resolvable_boundpads=0U;
    for(index=0U;index<setup->pad_count;++index){
        const PadRecord *pad=&((const PadRecord *)setup->pads_storage)[index];
        if(pad->stan!=NULL) ++resolvable_pads;
    }
    for(index=0U;index<setup->boundpad_count;++index){
        const BoundPadRecord *pad=
            &((const BoundPadRecord *)setup->boundpads_storage)[index];
        if(pad->stan!=NULL) ++resolvable_boundpads;
    }
    assert(setup->pad_stan_count==resolvable_pads
           &&setup->bound_pad_stan_count==resolvable_boundpads);
}

typedef struct AttachmentTextureAudit { const ModelFileHeader *header; size_t next; } AttachmentTextureAudit;
static int audit_attachment_texture(void *context, uint16_t image_id)
{
    AttachmentTextureAudit *audit = context;
    while (audit->next < (size_t)audit->header->numtextures
            && audit->header->Textures[audit->next].TextureID > UINT16_MAX) ++audit->next;
    assert(audit->next < (size_t)audit->header->numtextures);
    assert(audit->header->Textures[audit->next++].TextureID == image_id);
    return 1;
}
static void audit_attachment_dependencies(GeOriginalPitemModelProvider *models,
    int32_t model_id, const Model *model)
{
    AttachmentTextureAudit audit = {model->obj, 0U};
    assert(ge_original_pitem_model_visit_texture_ids(models, model_id, &audit, audit_attachment_texture));
    while (audit.next < (size_t)audit.header->numtextures
            && audit.header->Textures[audit.next].TextureID > UINT16_MAX) ++audit.next;
    assert(audit.next == (size_t)audit.header->numtextures);
}

static size_t audit_stage_guard_construction(
    GeAssetPack *pack,const GeStageAssetDescriptor *stage,
    const GeOriginalStageGuardRuntimeServices *services)
{
    static const size_t expected_pad_stan_count[GE_STAGE_COUNT]={
        0U,311U,176U,299U,105U,232U,177U,290U,121U,227U,210U,
        285U,354U,206U,591U,231U,152U,367U,178U,191U,48U
    };
    static const size_t expected_bound_pad_stan_count[GE_STAGE_COUNT]={
        0U,135U,12U,135U,54U,83U,97U,125U,110U,7U,320U,
        66U,52U,175U,5U,197U,6U,114U,147U,57U,0U
    };
    uint8_t *collision;size_t collision_size,stan_size,guard_count;
    void *stan_storage;GeStanCollisionSurface surface;GeStanNativeMap stan;
    GeOriginalStageSetupRuntime setup;GeOriginalCharacterModelProvider *models;
    GeOriginalCharacterModelStatus model_status;
    GeOriginalStageGuardRuntime *runtime;
    GeOriginalStageGuardRuntimeStatus runtime_status;
    GeOriginalPitemModelProvider *weapon_models=NULL;
    GeOriginalPitemModelStatus weapon_model_status;
    GeOriginalStageGuardWeaponBindReport weapon_report={0};
    GeOriginalStageGuardHatBindReport hat_report={0};
    GeOriginalStagePropMaterializerProviders materializer;
    GeOriginalStagePropMaterializerReport report;size_t index,active_count;
    size_t weapon_model_capacity,assigned_weapon_count=0U;
    GeOriginalStageActiveProps active={0};
    GeOriginalStageActivePropInput *active_inputs;
    PropRecord viewer={0},*owned_attachment=NULL;ChrRecord *chr_pool=NULL;
    size_t owned_attachment_command=SIZE_MAX;
    float world_to_view[4][4];
    collision=read_asset(pack,stage->collision_path,&collision_size);
    assert(ge_stan_collision_open(collision,collision_size,&surface)
           ==GE_STAN_COLLISION_OK);
    assert(ge_stan_native_required_size(&surface,&stan_size)
           ==GE_STAN_COLLISION_OK);
    stan_storage=malloc(stan_size);assert(stan_storage!=NULL);
    assert(ge_stan_native_materialize(&surface,stage->level_scale,
        stan_storage,stan_size,&stan)==GE_STAN_COLLISION_OK);
    assert(ge_original_stage_setup_load(pack,stage,&setup)
           ==GE_ORIGINAL_STAGE_SETUP_OK);
    {
        GeOriginalStageSetupStatus bind_status =
            ge_original_stage_setup_bind_stan(&setup,&stan);
        if(bind_status!=GE_ORIGINAL_STAGE_SETUP_OK)
            fprintf(stderr,"%s STAN bind failed: %d\n",stage->key,
                (int)bind_status);
        if(bind_status!=GE_ORIGINAL_STAGE_SETUP_OK){
            for(index=0U;index<setup.pad_count;++index){
                PadRecord *pad=&((PadRecord *)setup.pads_storage)[index];
                GeStanNativeTile *tile=NULL;
                if(pad->plink!=NULL&&pad->plink[0]!='\0'
                   &&ge_original_stan_resolve_pad(&stan,pad->plink,pad->pos.x,
                        pad->pos.y,pad->pos.z,&tile)==0)
                    fprintf(stderr,"  ordinary %zu '%s'\n",index,pad->plink);
            }
            for(index=0U;index<setup.boundpad_count;++index){
                BoundPadRecord *pad=&
                    ((BoundPadRecord *)setup.boundpads_storage)[index];
                GeStanNativeTile *tile=NULL;
                if(pad->plink!=NULL&&pad->plink[0]!='\0'
                   &&ge_original_stan_resolve_pad(&stan,pad->plink,pad->pos.x,
                        pad->pos.y,pad->pos.z,&tile)==0)
                    fprintf(stderr,"  bound %zu '%s'\n",index,pad->plink);
            }
        }
        assert(bind_status==GE_ORIGINAL_STAGE_SETUP_OK);
    }
    assert((size_t)stage->stage<GE_STAGE_COUNT);
    if(stage->stage!=GE_STAGE_DAM){
        if(setup.pad_stan_count!=expected_pad_stan_count[stage->stage]
                ||setup.bound_pad_stan_count
                    !=expected_bound_pad_stan_count[stage->stage])
            fprintf(stderr,"%s STAN count: %zu/%zu, expected %zu/%zu\n",
                stage->key,setup.pad_stan_count,setup.bound_pad_stan_count,
                expected_pad_stan_count[stage->stage],
                expected_bound_pad_stan_count[stage->stage]);
        assert(setup.pad_stan_count==expected_pad_stan_count[stage->stage]);
        assert(setup.bound_pad_stan_count
               ==expected_bound_pad_stan_count[stage->stage]);
    }
    assert_authored_stan_binding(&setup);
    if(stage->stage==GE_STAGE_SURFACE){
        GeOriginalStagePropMaterializerProviders closed_providers={0};
        GeOriginalStagePropMaterializerReport closed_report;
        SurfaceClosedObjectAudit closed_audit={0};
        for(index=76U;index<=78U;++index){
            GeOriginalStagePropConstructionRequest request;
            BoundPadRecord *pad;
            GeStanNativeTile *resolved=NULL;
            assert(ge_original_stage_prop_construction_request(
                &setup,index,&request));
            assert(request.service
                ==GE_ORIGINAL_STAGE_PROP_SERVICE_DEFAULT_OBJECT);
            assert(request.model_id==62&&request.pad_id==(int32_t)(9949U+index));
            assert(request.placement_resolved!=0U
                   &&request.placement.has_stan!=0U);
            pad=&((BoundPadRecord *)setup.boundpads_storage)[index-51U];
            assert(pad->plink!=NULL&&pad->plink[0]=='\0');
            assert(ge_original_stan_resolve_pad(&stan,pad->plink,pad->pos.x,
                pad->pos.y,pad->pos.z,&resolved)==2);
            assert(resolved==(GeStanNativeTile *)pad->stan);
        }
        {
            BoundPadRecord *unresolved=
                &((BoundPadRecord *)setup.boundpads_storage)[41];
            assert(unresolved->plink!=NULL&&unresolved->plink[0]=='\0'
                   &&unresolved->stan==NULL);
            for(index=0U;index<setup.prop_record_count;++index)
                assert(setup.prop_records[index].pad_id!=10041);
        }
        closed_providers.context=&closed_audit;
        closed_providers.capabilities=GE_ORIGINAL_STAGE_PROP_CAP_DEFAULT_OBJECT;
        closed_providers.model_available=surface_model_available;
        closed_providers.construct_default_object=surface_construct_ready;
        assert(ge_original_stage_prop_materialize_ready(
            &setup,&closed_providers,&closed_report));
        assert(closed_report.failed==0U
               &&closed_report.constructed==closed_report.ready);
        assert(closed_audit.constructed==closed_report.ready
               &&closed_audit.fallback_command_mask==7U);
    }
    guard_count=ge_original_stage_setup_prop_type_count(&setup,PROPDEF_GUARD);
    assert(guard_count>0U);
    models=ge_original_character_model_provider_create(pack,
        ge_original_character_model_dependency_count(),guard_count,
        &model_status);
    assert(models!=NULL&&model_status==GE_ORIGINAL_CHARACTER_MODEL_OK);
    runtime=ge_original_stage_guard_runtime_create(models,guard_count,
        services,&runtime_status);
    assert(runtime!=NULL&&runtime_status==GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
    assert(ge_original_stage_guard_runtime_materializer(runtime,&materializer));
    if(!ge_original_stage_prop_materialize_ready(&setup,&materializer,&report)){
        fprintf(stderr,"guard construct stopped at %zu: %s\n",
            ge_original_stage_guard_runtime_count(runtime),
            ge_original_stage_guard_runtime_status_name(
                ge_original_stage_guard_runtime_last_status(runtime)));
        assert(0);
    }
    assert(report.ready==guard_count&&report.constructed==guard_count
           &&report.failed==0U
           &&ge_original_stage_guard_runtime_count(runtime)==guard_count);
    for(index=0U;index<setup.prop_record_count;++index){
        const GeOriginalStagePropRecord *record=&setup.prop_records[index];
        if(record->type==PROPDEF_COLLECTABLE&&record->word_count==34U
                &&(record->words[2]&PROPFLAG_ASSIGNEDTOCHR)!=0U)
            ++assigned_weapon_count;
    }
    weapon_model_capacity=ge_original_stage_prop_model_dependencies(
        &setup,GE_ORIGINAL_STAGE_PROP_SERVICE_ITEM,NULL,0U);
    if(assigned_weapon_count!=0U){
        Harness *harness=services->context;
        weapon_models=ge_original_pitem_model_provider_create(
            pack,weapon_model_capacity,guard_count*3U,&weapon_model_status);
        assert(weapon_models!=NULL&&weapon_model_status==GE_ORIGINAL_PITEM_MODEL_OK);
        runtime_status=ge_original_stage_guard_runtime_bind_authored_weapons(
            runtime,&setup,weapon_models,load_projectile_models,harness,
            &weapon_report);
        if(runtime_status!=GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK)
            fprintf(stderr,"%s assigned weapons: %s (%zu authored, %zu owner miss, %zu attached; command %zu model %d owner %d branch %u; provider %s)\n",
                stage->key,ge_original_stage_guard_runtime_status_name(runtime_status),
                weapon_report.authored_assigned_collectables,
                weapon_report.owner_not_present,weapon_report.attached,
                weapon_report.failed_command_index,weapon_report.failed_model_id,
                weapon_report.failed_owner_chr_id,weapon_report.failed_branch,
                ge_original_pitem_model_status_name(
                    ge_original_pitem_model_last_status(weapon_models)));
        assert(runtime_status==GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
        assert(weapon_report.authored_assigned_collectables>0U
               &&weapon_report.attached>0U
               &&weapon_report.attached
                    ==ge_original_stage_guard_runtime_weapon_count(runtime));
        assert(ge_original_stage_guard_runtime_bind_authored_hats(
            runtime,&setup,weapon_models,&hat_report)
            ==GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
        assert(hat_report.attached
                    ==ge_original_stage_guard_runtime_hat_count(runtime));
        if(hat_report.authored_assigned_hats>0U)assert(hat_report.attached>0U);
        else assert(hat_report.attached==0U);
        campaign_attached_hats+=hat_report.attached;
        {
            AssignedItemResolver resolver={runtime};
            GeOriginalStageInteractiveProviders item_providers={0};
            GeOriginalStageInteractiveRuntime items={0};
            size_t live,active,prior=0U;
            item_providers.context=&resolver;
            item_providers.difficulty=0U;item_providers.player_count=1U;
            item_providers.resolve_assigned_item=resolve_assigned_item;
            assert(ge_original_stage_interactive_materialize(
                &setup,&item_providers,&items));
            live=weapon_report.attached+hat_report.attached;
            assert(ge_original_stage_interactive_live_item_count(&items)==live
                   &&items.report.constructed_items==live
                   &&ge_original_stage_interactive_expected_item_count(&items)
                        >=live);
            assert(ge_original_stage_interactive_root_item_count(&items)==0U);
            for(active=0U;active<live;++active){
                size_t command;void *prop;
                assert(ge_original_stage_interactive_active_item(
                    &items,active,&command,&prop)&&prop!=NULL);
                if(active!=0U)assert(command>prior);
                assert(setup.prop_records[command].type==PROPDEF_COLLECTABLE
                    ||setup.prop_records[command].type==PROPDEF_HAT);
                prior=command;
            }
            {
                size_t command=0U;void *prop=NULL;
                assert(!ge_original_stage_interactive_root_item(
                    &items,0U,&command,&prop));
            }
            ge_original_stage_interactive_close(&items);
            /* Assigned guard objects are externally owned by the exact guard
             * runtime and must survive generic item-runtime teardown. */
            assert(ge_original_stage_guard_runtime_weapon_count(runtime)
                    ==weapon_report.attached
                   &&ge_original_stage_guard_runtime_hat_count(runtime)
                    ==hat_report.attached);
        }
        for(index=0U;index<weapon_report.attached;++index){
            GeOriginalStageGuardWeaponSnapshot weapon;
            WeaponObjRecord *record;PropRecord *prop;ChrRecord *owner=NULL;
            size_t guard_index;
            assert(ge_original_stage_guard_runtime_weapon_snapshot(
                runtime,index,&weapon));
            record=weapon.weapon_record;prop=weapon.prop_record;
            for(guard_index=0U;guard_index<guard_count;++guard_index){
                GeOriginalStageGuardSnapshot guard;
                assert(ge_original_stage_guard_runtime_snapshot(
                    runtime,guard_index,&guard));
                if(guard.chr_id==weapon.owner_chr_id){owner=guard.chr_record;break;}
            }
            assert(owner!=NULL&&record!=NULL&&prop!=NULL
                   &&prop->type==PROP_TYPE_WEAPON&&prop->obj==(ObjectRecord *)record
                   &&prop->parent==owner->prop
                   &&owner->weapons_held[weapon.hand]==prop
                   &&record->model==weapon.model_instance
                   &&record->model->attachedto==owner->model
                   &&record->model->attachedto_objinst!=NULL);
        }
        for(index=0U;index<hat_report.attached;++index){
            GeOriginalStageGuardHatSnapshot hat;
            HatRecord *record;PropRecord *prop;ChrRecord *owner=NULL;
            size_t guard_index;
            assert(ge_original_stage_guard_runtime_hat_snapshot(
                runtime,index,&hat));
            assert(hat.model_id>=0&&hat.model_id<340);
            audit_attachment_dependencies(weapon_models, hat.model_id, hat.model_instance);
            campaign_hat_models[hat.model_id]=1U;
            record=hat.hat_record;prop=hat.prop_record;
            for(guard_index=0U;guard_index<guard_count;++guard_index){
                GeOriginalStageGuardSnapshot guard;
                assert(ge_original_stage_guard_runtime_snapshot(
                    runtime,guard_index,&guard));
                if(guard.chr_id==hat.owner_chr_id){owner=guard.chr_record;break;}
            }
            assert(owner!=NULL&&record!=NULL&&prop!=NULL
                   &&prop->obj==(ObjectRecord *)record&&prop->parent==owner->prop
                   &&owner->handle_positiondata_hat==prop
                   &&record->model==hat.model_instance
                   &&record->model->attachedto==owner->model
                   &&record->model->attachedto_objinst
                        ==owner->model->obj->Switches[6]);
        }
        assert(harness->projectile_loads>=weapon_report.attached);
    }else if(ge_original_stage_setup_prop_type_count(&setup,PROPDEF_HAT)>0U){
        weapon_models=ge_original_pitem_model_provider_create(
            pack,weapon_model_capacity,guard_count*3U,&weapon_model_status);
        assert(weapon_models!=NULL&&weapon_model_status==GE_ORIGINAL_PITEM_MODEL_OK);
        assert(ge_original_stage_guard_runtime_bind_authored_hats(
            runtime,&setup,weapon_models,&hat_report)
            ==GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
        assert(hat_report.authored_assigned_hats>0U
               &&hat_report.attached>0U
               &&hat_report.attached
                    ==ge_original_stage_guard_runtime_hat_count(runtime));
        campaign_attached_hats+=hat_report.attached;
        for(index=0U;index<hat_report.attached;++index){
            GeOriginalStageGuardHatSnapshot hat;
            assert(ge_original_stage_guard_runtime_hat_snapshot(
                runtime,index,&hat)&&hat.model_id>=0&&hat.model_id<340);
            campaign_hat_models[hat.model_id]=1U;
        }
    }
    {
        size_t all_count=ge_original_stage_guard_runtime_active_prop_count(runtime);
        size_t all,prior=0U,roots=0U,attachments=0U;
        assert(all_count==guard_count+weapon_report.attached+hat_report.attached);
        for(all=0U;all<all_count;++all){
            size_t command;void *opaque_prop;PropRecord *prop;
            assert(ge_original_stage_guard_runtime_active_prop(
                runtime,all,&command,&opaque_prop)&&opaque_prop!=NULL);
            prop=opaque_prop;
            if(all!=0U)assert(command>prior);
            if(prop->parent==NULL){
                assert(prop->type==PROP_TYPE_CHR);++roots;
            }else{
                assert(prop->type!=PROP_TYPE_CHR);++attachments;
                if(owned_attachment==NULL){
                    owned_attachment=prop;owned_attachment_command=command;
                }
            }
            prior=command;
        }
        assert(roots==guard_count
               &&attachments==weapon_report.attached+hat_report.attached);
    }
    active_count=ge_original_stage_guard_runtime_root_prop_count(runtime);
    assert(active_count==guard_count);
    active_inputs=calloc(active_count,sizeof(*active_inputs));
    assert(active_inputs!=NULL);
    viewer.type=PROP_TYPE_VIEWER;viewer.flags=PROPFLAG_ENABLED;
    memset(&stage_player,0,sizeof(stage_player));stage_player.prop=&viewer;
    g_playerPointers[0]=&stage_player;
    for(index=0U;index<guard_count;++index){
        GeOriginalStageGuardSnapshot snapshot;
        PropRecord *prop;ChrRecord *chr;void *opaque_prop,*opaque_chr;
        int32_t firecount[2]={INT32_MIN,INT32_MIN};
        uint8_t room, visible;
        assert(ge_original_stage_guard_runtime_snapshot(runtime,index,&snapshot));
        assert(ge_original_stage_guard_runtime_room_visibility(runtime,index,&room,&visible));
        assert(room == snapshot.room_id && visible == snapshot.visible);
        assert(snapshot.body_id>=0&&snapshot.model_instance!=NULL
               &&snapshot.room_id<UINT8_MAX&&isfinite(snapshot.angle)
               &&snapshot.prop_record!=NULL&&snapshot.chr_record!=NULL);
        assert(ge_original_stage_guard_runtime_actor(runtime,index,
            &opaque_prop,&opaque_chr));
        prop=opaque_prop;chr=opaque_chr;
        assert(ge_original_stage_guard_runtime_firecount(
            runtime,index,firecount));
        assert(firecount[0]==chr->firecount[0]
               &&firecount[1]==chr->firecount[1]);
        if(index==0U)chr_pool=chr;
        assert(prop==snapshot.prop_record&&chr==snapshot.chr_record
               &&prop->type==PROP_TYPE_CHR&&prop->chr==chr
               &&chr->prop==prop&&chr->model==snapshot.model_instance
               &&chr->chrnum==(s16)snapshot.chr_id
               &&chr->bodynum==(s8)snapshot.body_id
               &&chr->headnum==(s8)snapshot.resolved_head_id
               &&snapshot.ai_list_resolved&&chr->ailist!=NULL
               &&chr->actiontype==ACT_INIT
               &&chr->padpreset1==(s16)snapshot.preset
               &&chr->chrpreset1==(s16)snapshot.chrpreset
               &&fabsf(chr->hearingscale-(float)snapshot.health/1000.0f)<0.0001f
               &&chr->visionrange==(float)snapshot.reaction
               &&chr->chrwidth==20.0f&&chr->chrheight==185.0f
               &&(prop->flags&PROPFLAG_ENABLED)!=0U
               &&prop->stan!=NULL&&prop->rooms[0]==snapshot.room_id
               &&prop->rooms[1]==UINT8_MAX);
    }
    for(index=0U;index<active_count;++index){
        assert(ge_original_stage_guard_runtime_root_prop(runtime,index,
            &active_inputs[index].command_index,&active_inputs[index].prop));
        assert(((PropRecord *)active_inputs[index].prop)->type==PROP_TYPE_CHR
               &&((PropRecord *)active_inputs[index].prop)->parent==NULL);
        active_inputs[index].kind=GE_ORIGINAL_STAGE_ACTIVE_PROP_AUTHORED;
    }
    assert(chr_pool!=NULL);
    if(owned_attachment!=NULL){
        GeOriginalStageActiveProps rejected={0};
        GeOriginalStageActivePropInput child_input={
            owned_attachment_command,owned_attachment,
            GE_ORIGINAL_STAGE_ACTIVE_PROP_AUTHORED};
        PropRecord *parent=owned_attachment->parent;
        PropRecord *prev=owned_attachment->prev,*next=owned_attachment->next;
        assert(owned_attachment_command<setup.prop_record_count);
        assert(ge_original_stage_active_props_compose(&rejected,&setup,&viewer,
            chr_pool,guard_count,&child_input,1U)
            ==GE_ORIGINAL_STAGE_ACTIVE_PROP_INVALID_ACTOR);
        assert(owned_attachment->parent==parent&&owned_attachment->prev==prev
               &&owned_attachment->next==next&&!rejected.bound);
    }
    assert(ge_original_stage_active_props_compose(&active,&setup,&viewer,
        chr_pool,guard_count,active_inputs,active_count)
        ==GE_ORIGINAL_STAGE_ACTIVE_PROP_OK);
    {
        PropRecord *prop=viewer.next;size_t visited=0U,last_command=0U;
        while(prop!=NULL){
            size_t input;
            for(input=0U;input<active_count;++input)
                if(active_inputs[input].prop==prop)break;
            assert(input<active_count);
            if(visited!=0U)
                assert(active_inputs[input].command_index>last_command);
            last_command=active_inputs[input].command_index;
            prop=prop->next;++visited;
        }
        assert(visited==active_count&&g_ActivePropsHead==&viewer
               &&g_ActivePropsTail!=NULL&&g_ChrSlots==chr_pool
               &&g_NumChrSlots==(s32)guard_count);
    }
    /* Every authored room becomes resident in turn after all instances have
     * been constructed. This deliberately exercises reused head resources
     * after later bodies have changed the shared HEAD root's parent, matching
     * the Depot route-001 failure that a first-instance-only test missed. */
    identity(world_to_view);
    assert(ge_original_stage_guard_runtime_update_lighting(runtime)
        ==GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
    if(stage->stage==GE_STAGE_DAM){
        /* Authored first-road route position, aimed at its nearest authored
         * guard. All rooms are made resident only to isolate the renderer's
         * camera rejection from the separate portal visibility filter. */
        const float route_x=19799.692681f,route_z=16910.632465f;
        GeOriginalStageGuardSnapshot target={0};
        GeOriginalStageGuardScene route_scene;
        float view_to_world[4][4];float nearest=INFINITY;
        size_t matrix_ready_count=0U,target_index=SIZE_MAX;
        Harness *harness=services->context;
        for(index=0U;index<guard_count;++index){
            GeOriginalStageGuardSnapshot candidate;float dx,dz,distance;
            assert(ge_original_stage_guard_runtime_snapshot(
                runtime,index,&candidate));
            dx=candidate.position[0]-route_x;
            dz=candidate.position[2]-route_z;
            distance=dx*dx+dz*dz;
            if(distance<nearest){
                nearest=distance;target=candidate;target_index=index;
            }
        }
        matrix_4x4_set_lookat_target((Mtxf *)(void *)world_to_view,
            target.position[0],target.position[1]+50.0f,
            target.position[2]+500.0f,
            target.position[0],target.position[1]+50.0f,target.position[2],
            0.0f,1.0f,0.0f);
        matrix_4x4_set_inverse_rotation_and_translation(
            (Mtxf *)(void *)world_to_view,
            (Mtxf *)(void *)view_to_world);
        stage_player.c_perspfovy=60.0f;
        stage_player.c_perspaspect=5.0f/3.0f;
        stage_player.c_perspnear=10.0f;
        harness->all_rooms_resident=1U;
        assert(ge_original_stage_guard_runtime_update_matrices(
            runtime,world_to_view)==GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
        for(index=0U;index<guard_count;++index){
            GeOriginalStageGuardSnapshot candidate;
            assert(ge_original_stage_guard_runtime_snapshot(
                runtime,index,&candidate));
            if(candidate.matrices_ready!=0U)++matrix_ready_count;
        }
        assert(ge_original_stage_guard_runtime_build_scene(
            runtime,view_to_world,NULL,&route_scene)
            ==GE_ORIGINAL_STAGE_GUARD_RUNTIME_SCENE_CAPACITY_EXCEEDED);
        assert(route_scene.resident_guard_count==guard_count
            &&route_scene.published_guard_count>0U
            &&route_scene.published_guard_count<route_scene.resident_guard_count
            &&matrix_ready_count==route_scene.published_guard_count
            &&route_scene.culled_guard_count
                ==route_scene.resident_guard_count
                    -route_scene.published_guard_count);
        {
            size_t culled_index;
            int checked_culled_aim=0;
            for(culled_index=0U;culled_index<guard_count;++culled_index){
                GeOriginalStageGuardSnapshot culled;
                void *opaque_prop,*opaque_chr;
                float fallback[3];
                assert(ge_original_stage_guard_runtime_snapshot(
                    runtime,culled_index,&culled));
                if(culled.matrices_ready!=0U)continue;
                assert(ge_original_stage_guard_runtime_actor(runtime,
                    culled_index,&opaque_prop,&opaque_chr));
                assert(ge_original_stage_guard_runtime_autoaim_world_position(
                    runtime,culled_index,view_to_world,fallback));
                assert(fabsf(fallback[0]-((PropRecord *)opaque_prop)->pos.x)
                            <0.001f
                    &&fabsf(fallback[1]-(((PropRecord *)opaque_prop)->pos.y
                        +((ChrRecord *)opaque_chr)->chrheight*0.75f))<0.001f
                    &&fabsf(fallback[2]-((PropRecord *)opaque_prop)->pos.z)
                            <0.001f);
                checked_culled_aim=1;
                break;
            }
            assert(checked_culled_aim);
        }
        {
            GeOriginalStageGuardSnapshot published;
            PropRecord *prop;ChrRecord *chr;void *opaque_prop,*opaque_chr;
            Model *model;coord3d aim={0};coord2d xbounds={0},ybounds={0};
            float root_z,second_z,expected_z;
            float probe_world[3],expected_world[3],view_offset[3];
            size_t axis;
            s32 action;u32 chrflags;u8 propflags;
            assert(target_index<SIZE_MAX
                   &&ge_original_stage_guard_runtime_snapshot(
                       runtime,target_index,&published)
                   &&published.matrices_ready!=0U
                   &&ge_original_stage_guard_runtime_actor(
                       runtime,target_index,&opaque_prop,&opaque_chr));
            prop=opaque_prop;chr=opaque_chr;model=chr->model;
            assert(prop!=NULL&&chr!=NULL&&model!=NULL
                   &&model->obj->numMatrices>=2);
            root_z=model->render_pos[0].pos.m[3][2];
            second_z=model->render_pos[1].pos.m[3][2];
            expected_z=second_z+(root_z-second_z)*0.25f;
            propflags=prop->flags;action=chr->actiontype;
            chrflags=chr->chrflags;
            prop->flags|=PROPFLAG_ONSCREEN;
            chr->actiontype=ACT_STAND;
            chr->chrflags&=~CHRFLAG_NO_AUTOAIM;
            {
                PropRecord *right,*left,*held;
                size_t onscreen_index,weapon_index,matched=0U;
                test_update_onscreen_prop_count_exact();
                for(onscreen_index=0U;
                        onscreen_index<(size_t)g_OnScreenPropCount;
                        ++onscreen_index)
                    if(g_OnScreenPropList[onscreen_index]==prop)break;
                assert(onscreen_index<(size_t)g_OnScreenPropCount
                       &&g_LastOnScreenProp
                            ==&g_OnScreenPropList[g_OnScreenPropCount]);
                right=test_chr_get_equipped_weapon_prop_exact(chr,GUNRIGHT);
                left=test_chr_get_equipped_weapon_prop_exact(chr,GUNLEFT);
                assert((right!=NULL)!=(left!=NULL));
                held=right!=NULL?right:left;
                assert(held->type==PROP_TYPE_WEAPON&&held->parent==prop
                       &&held->weapon!=NULL&&held->weapon->model!=NULL);
                for(weapon_index=0U;
                        weapon_index<ge_original_stage_guard_runtime_weapon_count(
                            runtime);++weapon_index){
                    GeOriginalStageGuardWeaponSnapshot weapon;
                    assert(ge_original_stage_guard_runtime_weapon_snapshot(
                        runtime,weapon_index,&weapon));
                    if(weapon.owner_chr_id==published.chr_id){
                        assert(weapon.prop_record==held
                               &&weapon.weapon_record==held->weapon);
                        ++matched;
                    }
                }
                assert(matched==1U);
                for(weapon_index=0U;weapon_index<active_count;++weapon_index)
                    assert(active_inputs[weapon_index].prop!=held);
                printf("Dam first-road autoaim candidate: guard %ld list "
                       "%lu/%ld, weapon %s child published\n",
                    (long)published.chr_id,
                    (unsigned long)onscreen_index,
                    (long)g_OnScreenPropCount,
                    right!=NULL?"right":"left");
            }
            assert(test_chr_get_onscreen_render_bounds_exact(
                prop,&aim,&xbounds,&ybounds));
            assert(ge_original_stage_guard_runtime_autoaim_world_position(
                runtime,target_index,view_to_world,probe_world));
            for(axis=0U;axis<3U;++axis){
                view_offset[axis]=(model->render_pos[1].pos.m[3][axis]
                    -model->render_pos[0].pos.m[3][axis])*0.75f;
                expected_world[axis]=prop->pos.f[axis]
                    +view_offset[0]*view_to_world[0][axis]
                    +view_offset[1]*view_to_world[1][axis]
                    +view_offset[2]*view_to_world[2][axis];
            }
            assert(root_z<0.0f&&second_z<0.0f&&aim.z<0.0f
                   &&fabsf(aim.z-expected_z)<0.0001f
                   &&fabsf(probe_world[0]-expected_world[0])<0.001f
                   &&fabsf(probe_world[1]-expected_world[1])<0.001f
                   &&fabsf(probe_world[2]-expected_world[2])<0.001f
                   &&isfinite(aim.x)&&isfinite(aim.y)
                   &&isfinite(xbounds.x)&&isfinite(xbounds.y)
                   &&isfinite(ybounds.x)&&isfinite(ybounds.y)
                   &&xbounds.x<xbounds.y&&ybounds.x<ybounds.y);
            prop->flags&=(u8)~PROPFLAG_ONSCREEN;
            assert(!test_chr_get_onscreen_render_bounds_exact(
                prop,&aim,&xbounds,&ybounds));
            prop->flags|=PROPFLAG_ONSCREEN;chr->actiontype=ACT_DIE;
            assert(!test_chr_get_onscreen_render_bounds_exact(
                prop,&aim,&xbounds,&ybounds));
            chr->actiontype=ACT_STAND;chr->chrflags|=CHRFLAG_NO_AUTOAIM;
            assert(!test_chr_get_onscreen_render_bounds_exact(
                prop,&aim,&xbounds,&ybounds));
            prop->flags=propflags;chr->actiontype=action;
            chr->chrflags=chrflags;
            printf("Dam first-road autoaim bounds: guard %zu rp z %.2f/%.2f "
                   "-> aim z %.2f, x [%.2f, %.2f], y [%.2f, %.2f]\n",
                target_index,root_z,second_z,expected_z,
                xbounds.x,xbounds.y,ybounds.x,ybounds.y);
        }
        printf("Dam first-road draw cull: %zu resident -> %zu published "
               "(%zu renderer-only culled)\n",
            route_scene.resident_guard_count,
            route_scene.published_guard_count,
            route_scene.culled_guard_count);
        harness->all_rooms_resident=0U;
        stage_player.c_perspfovy=0.0f;
        identity(world_to_view);
    }
    for(index=0U;index<guard_count;++index){
        GeOriginalStageGuardSnapshot snapshot;
        Harness *harness=services->context;
        assert(ge_original_stage_guard_runtime_snapshot(runtime,index,&snapshot));
        harness->resident_room=snapshot.room_id;
        assert(ge_original_stage_guard_runtime_update_matrices(
            runtime,world_to_view)==GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
        assert(ge_original_stage_guard_runtime_snapshot(runtime,index,&snapshot)
               &&snapshot.matrices_ready!=0U);
        assert(ge_original_stage_guard_runtime_shadow_count(runtime,index)>0U);
    }
    {
        const size_t retired_index=guard_count-1U;
        GeOriginalStageGuardSnapshot before,after;
        GeOriginalStageGuardLightingSnapshot lighting;
        GeOriginalStageGuardScene scene;
        ObjectRecord reused_object={0};
        void *opaque_prop=NULL,*opaque_chr=NULL;
        PropRecord *retired_prop;
        ChrRecord *retired_chr;
        size_t attachment;

        assert(ge_original_stage_guard_runtime_snapshot(
            runtime,retired_index,&before));
        assert(ge_original_stage_guard_runtime_actor(
            runtime,retired_index,&opaque_prop,&opaque_chr));
        retired_prop=opaque_prop;retired_chr=opaque_chr;

        /* Exact chrpropCleanupForRemoval leaves the authored ChrRecord slot
         * stable, then propExecuteTickOperation returns its PropRecord to the
         * shared pool. Reuse that union as an object and prove no guard API
         * can interpret the ObjectRecord pointer as a ChrRecord. */
        retired_chr->model=NULL;
        retired_chr->chrnum=-1;
        if(retired_prop->prev!=NULL)retired_prop->prev->next=retired_prop->next;
        else g_ActivePropsHead=retired_prop->next;
        if(retired_prop->next!=NULL)retired_prop->next->prev=retired_prop->prev;
        else g_ActivePropsTail=retired_prop->prev;
        retired_prop->prev=retired_prop->next=NULL;
        retired_prop->type=PROP_TYPE_OBJ;
        retired_prop->obj=&reused_object;
        retired_prop->rooms[0]=before.room_id;
        retired_prop->rooms[1]=UINT8_MAX;

        opaque_prop=(void *)(uintptr_t)1U;
        opaque_chr=(void *)(uintptr_t)1U;
        assert(!ge_original_stage_guard_runtime_actor(
            runtime,retired_index,&opaque_prop,&opaque_chr));
        assert(opaque_prop==NULL&&opaque_chr==NULL);
        assert(!ge_original_stage_guard_runtime_firecount(
            runtime,retired_index,(int32_t[2]){0,0}));
        assert(ge_original_stage_guard_runtime_stan(runtime,retired_index)
            ==NULL);
        assert(!ge_original_stage_guard_runtime_lighting_snapshot(
            runtime,retired_index,&lighting));
        assert(ge_original_stage_guard_runtime_shadow_count(
            runtime,retired_index)==0U);
        assert(ge_original_stage_guard_runtime_snapshot(
            runtime,retired_index,&after));
        assert(after.visible==0U&&after.matrices_ready==0U
               &&after.active_linked==0U&&after.animation_active==0U
               &&after.room_id==UINT8_MAX);
        {
            uint8_t room = 0U, visible = 1U;
            assert(ge_original_stage_guard_runtime_room_visibility(
                runtime,retired_index,&room,&visible));
            assert(room == UINT8_MAX && visible == 0U);
            assert(!ge_original_stage_guard_runtime_room_visibility(
                runtime,SIZE_MAX,&room,&visible));
        }
        assert(ge_original_stage_guard_runtime_set_visibility(
            runtime,retired_index,1,before.room_id));
        assert(ge_original_stage_guard_runtime_update_lighting(runtime)
            ==GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
        assert(ge_original_stage_guard_runtime_update_matrices(
            runtime,world_to_view)==GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
        assert(ge_original_stage_guard_runtime_build_scene(
            runtime,world_to_view,NULL,&scene)
            ==GE_ORIGINAL_STAGE_GUARD_RUNTIME_SCENE_CAPACITY_EXCEEDED);
        assert(scene.resident_guard_count<guard_count);
        for(attachment=0U;attachment<weapon_report.attached;++attachment){
            GeOriginalStageGuardWeaponSnapshot weapon;
            assert(ge_original_stage_guard_runtime_weapon_snapshot(
                runtime,attachment,&weapon));
            if(weapon.owner_chr_id==before.chr_id)
                assert(weapon.matrices_ready==0U);
        }
        for(attachment=0U;attachment<hat_report.attached;++attachment){
            GeOriginalStageGuardHatSnapshot hat;
            assert(ge_original_stage_guard_runtime_hat_snapshot(
                runtime,attachment,&hat));
            if(hat.owner_chr_id==before.chr_id)
                assert(hat.matrices_ready==0U);
        }
    }
    ge_original_stage_active_props_close(&active);free(active_inputs);
    ge_original_stage_guard_runtime_destroy(runtime);
    ge_original_pitem_model_provider_destroy(weapon_models);
    ge_original_character_model_provider_destroy(models);
    ge_original_stage_setup_close(&setup);free(stan_storage);free(collision);
    return guard_count;
}

int main(int argc,char **argv)
{
    GeAssetPack pack;const GeStageAssetDescriptor *stage;
    GeOriginalStageSetupRuntime setup;GeStanCollisionSurface surface;
    GeStanNativeMap stan;void *stan_storage;size_t stan_size,collision_size;
    uint8_t *collision;GeOriginalCharacterModelProvider *models;
    GeOriginalCharacterModelStatus model_status;
    GeOriginalStageGuardRuntime *runtime,*blocked_runtime;
    GeOriginalStageGuardRuntimeStatus runtime_status;
    GeOriginalPitemModelProvider *weapon_models;
    GeOriginalPitemModelStatus weapon_model_status;
    GeOriginalStageGuardWeaponBindReport weapon_report;
    GeOriginalStageGuardHatBindReport hat_report;
    GeOriginalStageGuardRuntimeServices services;Harness harness={0};
    GeOriginalStagePropMaterializerProviders materializer;
    GeOriginalStagePropMaterializerReport report;
    GeOriginalStageGuardSnapshot first,lifecycle;float world_to_view[4][4];
    float view_to_world[4][4];GeOriginalStageGuardScene query,built;
    GeDamRoomSceneStorage storage;size_t index;size_t guard_count;
    size_t cached_baseline_inputs=0U;
    PropRecord viewer;
    memset(&lifecycle,0,sizeof(lifecycle));lifecycle.active_linked=1U;
    lifecycle.action_type=ACT_STAND;
    assert(!ge_original_stage_guard_snapshot_death_complete(&lifecycle));
    lifecycle.action_type=ACT_DIE;
    assert(ge_original_stage_guard_snapshot_death_complete(&lifecycle));
    lifecycle.action_type=ACT_DEAD;
    assert(ge_original_stage_guard_snapshot_death_complete(&lifecycle));
    lifecycle.action_type=ACT_STAND;lifecycle.active_linked=0U;
    assert(!ge_original_stage_guard_snapshot_death_complete(&lifecycle));
    lifecycle.room_id=UINT8_MAX;
    assert(ge_original_stage_guard_snapshot_death_complete(&lifecycle));
    assert(argc==2&&ge_asset_pack_open(&pack,argv[1])==GE_ASSET_PACK_OK);
    assert_conservative_guard_draw_frustum();
    assert(ge_original_global_ai_count()==18U);
    assert(ge_original_global_ai_find(0)!=NULL
           &&ge_original_global_ai_find(17)!=NULL
           &&ge_original_global_ai_find(18)==NULL);
    stage=ge_stage_asset_facility();assert(stage!=NULL);
    collision=read_asset(&pack,stage->collision_path,&collision_size);
    assert(ge_stan_collision_open(collision,collision_size,&surface)
           ==GE_STAN_COLLISION_OK);
    assert(ge_stan_native_required_size(&surface,&stan_size)
           ==GE_STAN_COLLISION_OK);
    stan_storage=malloc(stan_size);assert(stan_storage!=NULL);
    assert(ge_stan_native_materialize(&surface,stage->level_scale,
        stan_storage,stan_size,&stan)==GE_STAN_COLLISION_OK);
    assert(ge_original_stage_setup_load(&pack,stage,&setup)
           ==GE_ORIGINAL_STAGE_SETUP_OK);
    assert(ge_original_stage_setup_bind_stan(&setup,&stan)
           ==GE_ORIGINAL_STAGE_SETUP_OK);
    assert_authored_stan_binding(&setup);
    {
        PadRecord *first_pad=&((PadRecord *)setup.pads_storage)[0];
        char *authored_plink=first_pad->plink;
        coord3d authored_pos=first_pad->pos;
        first_pad->plink="not_an_authored_stan";
        first_pad->pos.x=1000000000.0f;
        first_pad->pos.y=1000000000.0f;
        first_pad->pos.z=1000000000.0f;
        assert(ge_original_stage_setup_bind_stan(&setup,&stan)
               ==GE_ORIGINAL_STAGE_SETUP_STAN_UNRESOLVED);
        first_pad->plink=authored_plink;
        first_pad->pos=authored_pos;
        assert(ge_original_stage_setup_bind_stan(&setup,&stan)
               ==GE_ORIGINAL_STAGE_SETUP_OK);
        assert_authored_stan_binding(&setup);
    }
    guard_count=ge_original_stage_setup_prop_type_count(&setup,PROPDEF_GUARD);
    assert(guard_count==65U);
    models=ge_original_character_model_provider_create(&pack,
        ge_original_character_model_dependency_count(),guard_count+2U,
        &model_status);
    assert(models!=NULL&&model_status==GE_ORIGINAL_CHARACTER_MODEL_OK);
    memset(&services,0,sizeof(services));services.context=&harness;
    services.choose_head=choose_head;
    services.choose_sunglasses=choose_sunglasses;
    services.room_resident=room_resident;
    services.tile_rgb=tile_rgb;
    runtime=ge_original_stage_guard_runtime_create(models,guard_count+2U,
        &services,&runtime_status);
    assert(runtime!=NULL&&runtime_status==GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
    assert(ge_original_stage_guard_runtime_materializer(runtime,&materializer));
    if(!ge_original_stage_prop_materialize_ready(&setup,&materializer,&report)){
        fprintf(stderr,"facility guard construct stopped at %zu: %s\n",
            ge_original_stage_guard_runtime_count(runtime),
            ge_original_stage_guard_runtime_status_name(
                ge_original_stage_guard_runtime_last_status(runtime)));
        assert(0);
    }
    assert(report.ready==guard_count&&report.constructed==guard_count
           &&report.failed==0U);
    assert(ge_original_stage_guard_runtime_count(runtime)==guard_count);
    weapon_models=ge_original_pitem_model_provider_create(
        &pack,24U,guard_count*2U,&weapon_model_status);
    assert(weapon_models!=NULL&&weapon_model_status==GE_ORIGINAL_PITEM_MODEL_OK);
    memset(&weapon_report,0,sizeof(weapon_report));
    runtime_status=ge_original_stage_guard_runtime_bind_authored_weapons(
        runtime,&setup,weapon_models,load_projectile_models,&harness,
        &weapon_report);
    if(runtime_status!=GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK)
        fprintf(stderr,"Facility assigned weapons: %s/%d (%zu authored, %zu owner miss, %zu attached; command %zu model %d owner %d branch %u)\n",
            ge_original_stage_guard_runtime_status_name(runtime_status),
            (int)runtime_status,
            weapon_report.authored_assigned_collectables,
            weapon_report.owner_not_present,weapon_report.attached,
            weapon_report.failed_command_index,weapon_report.failed_model_id,
            weapon_report.failed_owner_chr_id,weapon_report.failed_branch);
    assert(runtime_status==GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
    assert(weapon_report.authored_assigned_collectables>0U
           &&weapon_report.attached>0U
           &&weapon_report.attached
                ==ge_original_stage_guard_runtime_weapon_count(runtime));
    memset(&hat_report,0,sizeof(hat_report));
    runtime_status=ge_original_stage_guard_runtime_bind_authored_hats(
        runtime,&setup,weapon_models,&hat_report);
    if(runtime_status!=GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK)
        fprintf(stderr,"Facility assigned hats: %s/%d (%zu authored, %zu owner miss, %zu attached; command %zu model %d owner %d branch %u)\n",
            ge_original_stage_guard_runtime_status_name(runtime_status),
            (int)runtime_status,hat_report.authored_assigned_hats,
            hat_report.owner_not_present,hat_report.attached,
            hat_report.failed_command_index,hat_report.failed_model_id,
            hat_report.failed_owner_chr_id,hat_report.failed_branch);
    assert(runtime_status==GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK
           &&hat_report.authored_assigned_hats>0U&&hat_report.attached>0U
           &&hat_report.attached
                ==ge_original_stage_guard_runtime_hat_count(runtime));
    campaign_attached_hats+=hat_report.attached;
    for(index=0U;index<hat_report.attached;++index){
        GeOriginalStageGuardHatSnapshot hat;
        assert(ge_original_stage_guard_runtime_hat_snapshot(runtime,index,&hat)
               &&hat.model_id>=0&&hat.model_id<340);
        campaign_hat_models[hat.model_id]=1U;
    }
    assert(ge_original_stage_guard_runtime_snapshot(runtime,0U,&first));
    assert(first.command_index==315U&&first.body_id==2
           &&first.authored_head_id==-1&&first.resolved_head_id==42
           &&first.ai_list_id==2&&first.pad_id==254
           &&first.model_instance!=NULL&&isfinite(first.angle));
    harness.resident_room=first.room_id;
    identity(world_to_view);identity(view_to_world);
    assert(ge_original_stage_guard_runtime_update_matrices(runtime,world_to_view)
           ==GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
    assert(ge_original_stage_guard_runtime_snapshot(runtime,0U,&first)
           &&first.matrices_ready!=0U);
    /* The live stage path receives exact chrTick/chrRenderHeldWeapon matrices
     * from the transient dyn arena. Preserve them byte-for-byte in the
     * provider-owned durable slots; a second subcalcmatrices pass here would
     * run after chrHandleJointPositioned was cleared and lose ACT_ATTACK's
     * torso/arm/head aiming. */
    {
        Model *body=(Model *)first.model_instance;
        RenderPosView *durable_body;
        RenderPosView *transient_body;
        RenderPosView *expected_body;
        GeOriginalStageGuardWeaponSnapshot weapon_snapshot;
        Model *weapon_model;
        RenderPosView *durable_weapon;
        RenderPosView *transient_weapon;
        RenderPosView *expected_weapon;
        size_t body_count,weapon_count,matrix,row,column;
        assert(body!=NULL&&body->obj!=NULL&&body->render_pos!=NULL);
        body_count=(size_t)body->obj->numMatrices;
        assert(body_count>0U);
        durable_body=body->render_pos;
        transient_body=calloc(body_count,sizeof(*transient_body));
        expected_body=calloc(body_count,sizeof(*expected_body));
        assert(transient_body!=NULL&&expected_body!=NULL);
        for(matrix=0U;matrix<body_count;++matrix){
            for(row=0U;row<4U;++row)
                transient_body[matrix].pos.m[row][row]=1.0f;
            for(column=0U;column<3U;++column)
                transient_body[matrix].pos.m[3][column]=
                    3000.0f+(float)(matrix*10U+column);
        }
        memcpy(expected_body,transient_body,
            body_count*sizeof(*expected_body));
        body->render_pos=transient_body;

        {
            size_t failure_line = 0U, failure_matrix = SIZE_MAX;
            int32_t failure_chr = -1;
            int retained = 0;
            float failed_values[16] = {0};
            float failed_camera[32], failed_model[32];
            Mtxf failure_camera = {0};
            Mtxf *saved_camera = stage_player.field_10CC;
            const float saved_scale = body->scale;
            failure_camera.m[0][0] = 7.0f;
            stage_player.field_10CC = &failure_camera;
            transient_body[0].pos.m[0][0] = NAN;
            assert(ge_original_stage_guard_runtime_update_matrices(
                runtime, world_to_view)
                == GE_ORIGINAL_STAGE_GUARD_RUNTIME_MATRIX_UNAVAILABLE);
            ge_original_stage_guard_runtime_matrix_failure(
                runtime, &failure_line, &failure_chr);
            ge_original_stage_guard_runtime_matrix_failure_values(
                runtime, &failure_matrix, &retained, failed_values);
            assert(failure_line != 0U && failure_chr == first.chr_id
                   && failure_matrix == 0U && retained == 1
                   && isnan(failed_values[0]));
            /* The report can be serialized hundreds of ticks later during
             * death. It must retain the inputs from the failure itself. */
            failure_camera.m[0][0] = 9.0f;
            body->scale = saved_scale + 1.0f;
            ge_original_stage_guard_runtime_matrix_failure_state(
                runtime, failed_camera, failed_model);
            assert(failed_camera[0] == 7.0f && failed_model[0] == saved_scale);
            assert(failed_model[15] == (float)body->obj->RootNode->Opcode);
            body->scale = saved_scale;
            stage_player.field_10CC = saved_camera;
            memcpy(transient_body, expected_body,
                   body_count * sizeof(*expected_body));
            body->render_pos = transient_body;
        }

        assert(ge_original_stage_guard_runtime_weapon_snapshot(
            runtime,0U,&weapon_snapshot));
        weapon_model=(Model *)weapon_snapshot.model_instance;
        assert(weapon_model!=NULL&&weapon_model->obj!=NULL
               &&weapon_model->render_pos!=NULL);
        weapon_count=(size_t)weapon_model->obj->numMatrices;
        assert(weapon_count>0U);
        durable_weapon=weapon_model->render_pos;
        transient_weapon=calloc(weapon_count,sizeof(*transient_weapon));
        expected_weapon=calloc(weapon_count,sizeof(*expected_weapon));
        assert(transient_weapon!=NULL&&expected_weapon!=NULL);
        for(matrix=0U;matrix<weapon_count;++matrix){
            for(row=0U;row<4U;++row)
                transient_weapon[matrix].pos.m[row][row]=1.0f;
            transient_weapon[matrix].pos.m[3][0]=4001.0f+(float)matrix;
            transient_weapon[matrix].pos.m[3][1]=4002.0f+(float)matrix;
            transient_weapon[matrix].pos.m[3][2]=4003.0f+(float)matrix;
        }
        memcpy(expected_weapon,transient_weapon,
            weapon_count*sizeof(*expected_weapon));
        weapon_model->render_pos=transient_weapon;

        assert(ge_original_stage_guard_runtime_update_matrices(
            runtime,world_to_view)==GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
        assert(body->render_pos==durable_body
               &&memcmp(durable_body,expected_body,
                   body_count*sizeof(*expected_body))==0);
        assert(weapon_model->render_pos==durable_weapon
               &&memcmp(durable_weapon,expected_weapon,
                   weapon_count*sizeof(*expected_weapon))==0);

        /* Culling cannot leave models pointing into the frame arena. The
         * canonical prop tick may publish a matrix before the renderer's
         * narrower visibility test rejects it. Reuse that arena, then bring
         * the guard back on screen without a new canonical publication. */
        for (size_t cull = 0U; cull < 3U; ++cull) {
            float culled_view[4][4];
            GeOriginalStageGuardSnapshot culled;
            memcpy(culled_view, world_to_view, sizeof(culled_view));
            memcpy(transient_body, expected_body, body_count * sizeof(*expected_body));
            memcpy(transient_weapon, expected_weapon, weapon_count * sizeof(*expected_weapon));
            body->render_pos = transient_body;
            weapon_model->render_pos = transient_weapon;
            assert(ge_original_stage_guard_runtime_set_visibility(
                runtime, 0U, cull != 0U, first.room_id));
            if (cull == 1U) harness.resident_room = UINT8_MAX;
            if (cull == 2U) {
                stage_player.c_perspfovy = 60.0f;
                stage_player.c_perspaspect = 4.0f / 3.0f;
                stage_player.c_perspnear = 10.0f;
                culled_view[3][2] = 10000000.0f;
            }
            assert(ge_original_stage_guard_runtime_update_matrices(
                runtime, culled_view) == GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
            assert(ge_original_stage_guard_runtime_snapshot(runtime, 0U, &culled)
                   && !culled.matrices_ready);
            assert(body->render_pos == durable_body);
            assert(weapon_model->render_pos == durable_weapon);
            assert(memcmp(durable_body, expected_body,
                          body_count * sizeof(*expected_body)) == 0);
            assert(memcmp(durable_weapon, expected_weapon,
                          weapon_count * sizeof(*expected_weapon)) == 0);
            memset(transient_body, 0xff, body_count * sizeof(*transient_body));
            memset(transient_weapon, 0xff, weapon_count * sizeof(*transient_weapon));
            harness.resident_room = first.room_id;
            stage_player.c_perspfovy = 0.0f;
            assert(ge_original_stage_guard_runtime_set_visibility(
                runtime, 0U, 1, first.room_id));
            /* No new transient publication: calculate the original fallback
             * pose, never retain the now-overwritten previous frame. */
            assert(ge_original_stage_guard_runtime_update_matrices(
                runtime, world_to_view) == GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
        }
        puts("Guard body/weapon matrices survive visibility, residency and sphere culls plus arena reuse");
        free(expected_weapon);free(transient_weapon);
        free(expected_body);free(transient_body);
    }
    {
        GeOriginalStageGuardHatSnapshot hat;
        Model *model;
        RenderPosView *durable, *transient;
        size_t matrix_count;
        assert(ge_original_stage_guard_runtime_hat_count(runtime) > 0U);
        assert(ge_original_stage_guard_runtime_hat_snapshot(runtime, 0U, &hat));
        model = hat.model_instance;
        assert(model != NULL && model->obj != NULL && model->render_pos != NULL);
        durable = model->render_pos;
        matrix_count = (size_t)model->obj->numMatrices;
        transient = malloc(matrix_count * sizeof(*transient));
        assert(transient != NULL);
        memcpy(transient, durable, matrix_count * sizeof(*transient));
        model->render_pos = transient;
        harness.resident_room = UINT8_MAX;
        assert(ge_original_stage_guard_runtime_update_matrices(
            runtime, world_to_view) == GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
        assert(model->render_pos == durable);
        assert(memcmp(durable, transient, matrix_count * sizeof(*transient)) == 0);
        memset(transient, 0xff, matrix_count * sizeof(*transient));
        harness.resident_room = first.room_id;
        assert(ge_original_stage_guard_runtime_update_matrices(
            runtime, world_to_view) == GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
        free(transient);
        puts("Authored hat matrices survive nonresident arena reuse");
    }
    {
        GeOriginalStageGuardLightingSnapshot before,after;
        size_t channel,shadow_count;
        assert(ge_original_stage_guard_runtime_lighting_snapshot(
            runtime,0U,&before));
        assert(ge_original_stage_guard_runtime_update_lighting(runtime)
            ==GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
        assert(ge_original_stage_guard_runtime_lighting_snapshot(
            runtime,0U,&after));
        for(channel=0U;channel<4U;++channel)
            assert(after.current_rgba[channel]
                ==(uint8_t)(before.current_rgba[channel]
                    +(((int)after.target_rgba[channel]
                        -(int)before.current_rgba[channel]+7)>>3)));
        shadow_count=ge_original_stage_guard_runtime_shadow_count(runtime,0U);
        assert(shadow_count>0U);
        for(index=0U;index<shadow_count;++index){
            GeOriginalStageGuardShadowPublication shadow;
            size_t vertex,axis;
            assert(ge_original_stage_guard_runtime_shadow(
                runtime,0U,index,&shadow));
            assert(shadow.matrix_index>=0&&shadow.opacity==0x50U);
            for(vertex=0U;vertex<4U;++vertex)
                for(axis=0U;axis<3U;++axis)
                    assert(isfinite(shadow.vertices[vertex][axis]));
        }
    }
    assert(ge_original_stage_guard_runtime_build_scene(runtime,view_to_world,
        NULL,&query)==GE_ORIGINAL_STAGE_GUARD_RUNTIME_SCENE_CAPACITY_EXCEEDED);
    assert(query.resident_guard_count>0U&&query.input_count>0U
           &&query.required_vertex_count>0U&&query.required_batch_count>0U
           &&query.triangle_count>0U);
    storage.vertex_capacity=query.required_vertex_count;
    storage.batch_capacity=query.required_batch_count;
    storage.vertices=calloc(storage.vertex_capacity,sizeof(*storage.vertices));
    storage.batches=calloc(storage.batch_capacity,sizeof(*storage.batches));
    assert(storage.vertices!=NULL&&storage.batches!=NULL);
    assert(ge_original_stage_guard_runtime_build_scene(runtime,view_to_world,
        &storage,&built)==GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
    assert(built.vertex_count==query.required_vertex_count
           &&built.batch_count==query.required_batch_count
           &&built.triangle_count==query.triangle_count);
    {
        GeOriginalModelSceneCache cache={0};
        GeOriginalStageGuardScene cached_scene;
        GeDamRoomSceneStorage cached_storage={0};
        GeOriginalStageGuardSceneScratchStats scratch_first;
        GeOriginalStageGuardSceneScratchStats scratch_final;
        assert(ge_original_stage_guard_runtime_build_scene_cached(
            runtime,&cache,view_to_world,NULL,&cached_scene)
            ==GE_ORIGINAL_STAGE_GUARD_RUNTIME_SCENE_CAPACITY_EXCEEDED);
        cached_baseline_inputs=cached_scene.input_count;
        ge_original_stage_guard_runtime_scene_scratch_stats(
            runtime,&scratch_first);
        assert(scratch_first.collect_calls==1U
            &&scratch_first.allocation_events==2U
            &&scratch_first.allocation_free_collect_calls==0U
            &&scratch_first.input_capacity==cached_scene.input_count
            &&scratch_first.character_part_capacity>0U);
        assert(cached_scene.required_vertex_count==built.vertex_count
            &&cached_scene.required_batch_count==built.batch_count);
        cached_storage.vertex_capacity=built.vertex_count;
        cached_storage.batch_capacity=built.batch_count;
        cached_storage.vertices=calloc(cached_storage.vertex_capacity,
            sizeof(*cached_storage.vertices));
        cached_storage.batches=calloc(cached_storage.batch_capacity,
            sizeof(*cached_storage.batches));
        assert(cached_storage.vertices!=NULL&&cached_storage.batches!=NULL);
        assert(ge_original_stage_guard_runtime_build_scene_cached(
            runtime,&cache,view_to_world,&cached_storage,&cached_scene)
            ==GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
        assert_cached_scene_matches(&storage,&cached_storage,
            built.vertex_count,built.batch_count);
        assert(cache.topology_rebuilds==1U&&cache.cached_builds==1U
            &&cache.identity_outer_vertices_published==built.vertex_count
            &&cache.topology_transform_maps_built
                ==cache.topology_component_misses
            &&cache.topology_transform_map_vertices_reused
                ==built.vertex_count
            &&cache.shared_matrix_banks_reused>0U
            &&cache.duplicate_vertex_transforms_avoided>0U);
        for(index=0U;index<cached_scene.vertex_count;++index){
            size_t axis;
            for(axis=0U;axis<3U;++axis)
                assert(cached_storage.vertices[index].world[axis]
                    ==cached_storage.vertices[index].processed.eye[axis]);
        }
        assert(ge_original_stage_guard_runtime_build_scene_cached(
            runtime,&cache,view_to_world,&cached_storage,&cached_scene)
            ==GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK
            &&cache.topology_rebuilds==1U&&cache.cached_builds==1U
            &&cache.unchanged_builds==1U);
        ge_original_stage_guard_runtime_scene_scratch_stats(
            runtime,&scratch_final);
        assert(scratch_final.collect_calls==3U
            &&scratch_final.allocation_events
                ==scratch_first.allocation_events
            &&scratch_final.allocation_free_collect_calls==2U
            &&scratch_final.input_capacity==scratch_first.input_capacity
            &&scratch_final.character_part_capacity
                ==scratch_first.character_part_capacity);
        free(cached_storage.batches);free(cached_storage.vertices);
        ge_original_model_scene_cache_close(&cache);
    }
    for(index=0U;index<built.batch_count;++index){
        assert(storage.batches[index].room_id==harness.resident_room);
        assert(storage.batches[index].coordinate_space
               ==GE_DAM_ROOM_COORDINATE_RUNTIME);
        /* modelRender owns world-character depth setup outside each child
         * display list. The portable flattener must retain that inheritance
         * or guards draw over opaque Dam walls. */
        assert(storage.batches[index].material.depth_test_enabled==1U);
        assert(storage.batches[index].material.depth_write_enabled
               ==(storage.batches[index].list_kind
                    ==GE_DAM_ROOM_LIST_PRIMARY));
    }
    {
        GeDamRoomSceneStorage combined;
        GeOriginalStageGuardScene appended;
        size_t vertex_cursor=3U,batch_cursor=1U;
        combined.vertex_capacity=query.required_vertex_count+3U;
        combined.batch_capacity=query.required_batch_count+1U;
        combined.vertices=calloc(combined.vertex_capacity,
                                 sizeof(*combined.vertices));
        combined.batches=calloc(combined.batch_capacity,
                                sizeof(*combined.batches));
        assert(combined.vertices!=NULL&&combined.batches!=NULL);
        combined.batches[0].room_id=UINT32_C(0x12345678);
        assert(ge_original_stage_guard_runtime_append_scene(
            runtime,view_to_world,&combined,&vertex_cursor,&batch_cursor,
            &appended)==GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
        assert(vertex_cursor==query.required_vertex_count+3U
               &&batch_cursor==query.required_batch_count+1U
               &&combined.batches[0].room_id==UINT32_C(0x12345678));
        for(index=1U;index<batch_cursor;++index)
            assert(combined.batches[index].first_vertex>=3U);
        vertex_cursor=3U;batch_cursor=1U;
        combined.vertex_capacity=3U;combined.batch_capacity=1U;
        assert(ge_original_stage_guard_runtime_append_scene(
            runtime,view_to_world,&combined,&vertex_cursor,&batch_cursor,
            &appended)
            ==GE_ORIGINAL_STAGE_GUARD_RUNTIME_SCENE_CAPACITY_EXCEEDED);
        assert(vertex_cursor==3U&&batch_cursor==1U
               &&combined.batches[0].room_id==UINT32_C(0x12345678));
        free(combined.batches);free(combined.vertices);
    }
    /* The unchanged solo_char_load constructor now feeds the same relocated
     * character-model matrices and cached scene path as authored guards. Its
     * original bodyModel/prop ownership relation is also the visibility
     * switch used by bondviewRemovePlayerBody when first-person resumes. */
    {
        PropRecord *source_prop=(PropRecord *)first.prop_record;
        GeOriginalModelSceneCache body_cache={0},removed_cache={0};
        GeOriginalStageGuardScene body_query,body_built,removed_query;
        GeDamRoomSceneStorage body_storage={0};
        size_t baseline_inputs=cached_baseline_inputs;
        size_t body_parts,weapon_parts;
        assert(source_prop!=NULL&&source_prop->stan!=NULL);
        memset(&viewer,0,sizeof(viewer));viewer.pos=source_prop->pos;
        viewer.stan=source_prop->stan;
        viewer.rooms[0]=(s8)harness.resident_room;viewer.rooms[1]=(s8)-1;
        stage_player.prop=&viewer;stage_player.bodyModel=NULL;
        assert(ge_original_stage_guard_runtime_load_player_body(
            runtime,&stage_player,BODY_Formal_Wear,
            HEAD_Male_Brosnan_Default,0.0f));
        assert(player_body_merge_calls==1U&&stage_player.bodyModel!=NULL
               &&viewer.type==PROP_TYPE_VIEWER&&viewer.chr!=NULL
               &&viewer.chr->model==stage_player.bodyModel);
        assert(ge_original_stage_guard_runtime_attach_player_held_item(
            runtime,&stage_player,PROP_CHRWPPK,ITEM_WPPK,0U));
        assert(viewer.chr->weapons_held[GUNRIGHT]!=NULL
               &&viewer.chr->weapons_held[GUNRIGHT]->parent==&viewer
               &&viewer.chr->weapons_held[GUNRIGHT]->type==PROP_TYPE_WEAPON
               &&viewer.chr->weapons_held[GUNRIGHT]->obj!=NULL
               &&viewer.chr->weapons_held[GUNRIGHT]->obj->model!=NULL);
        assert(ge_original_stage_guard_runtime_update_matrices(
            runtime,world_to_view)==GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
        body_parts=ge_original_character_model_instance_scene_part_count(
            models,stage_player.bodyModel);
        {
            Model *body = stage_player.bodyModel;
            Model *weapon = viewer.chr->weapons_held[GUNRIGHT]->obj->model;
            RenderPosView *body_durable = body->render_pos;
            RenderPosView *weapon_durable = weapon->render_pos;
            const size_t body_size = (size_t)body->obj->numMatrices * sizeof(*body_durable);
            const size_t weapon_size = (size_t)weapon->obj->numMatrices * sizeof(*weapon_durable);
            RenderPosView *body_transient = malloc(body_size);
            RenderPosView *weapon_transient = malloc(weapon_size);
            assert(body_transient != NULL && weapon_transient != NULL);
            memcpy(body_transient, body_durable, body_size);
            memcpy(weapon_transient, weapon_durable, weapon_size);
            body->render_pos = body_transient;
            weapon->render_pos = weapon_transient;
            harness.resident_room = UINT8_MAX;
            assert(ge_original_stage_guard_runtime_update_matrices(
                runtime, world_to_view) == GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
            assert(body->render_pos == body_durable && weapon->render_pos == weapon_durable);
            assert(memcmp(body_durable, body_transient, body_size) == 0);
            assert(memcmp(weapon_durable, weapon_transient, weapon_size) == 0);
            memset(body_transient, 0xff, body_size);
            memset(weapon_transient, 0xff, weapon_size);
            harness.resident_room = first.room_id;
            assert(ge_original_stage_guard_runtime_update_matrices(
                runtime, world_to_view) == GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
            free(weapon_transient);
            free(body_transient);
            puts("Canonical player body/weapon matrices survive nonresident arena reuse");
        }
        weapon_parts=ge_original_pitem_model_instance_scene_part_count(
            weapon_models,viewer.chr->weapons_held[GUNRIGHT]->obj->model);
        assert(body_parts>0U&&weapon_parts>0U);
        assert(ge_original_stage_guard_runtime_build_scene_cached(
            runtime,&body_cache,view_to_world,NULL,&body_query)
            ==GE_ORIGINAL_STAGE_GUARD_RUNTIME_SCENE_CAPACITY_EXCEEDED);
        assert(body_query.resident_guard_count==query.resident_guard_count
               &&body_query.published_guard_count==query.published_guard_count
               &&body_query.input_count
                    ==baseline_inputs+body_parts+weapon_parts
               &&body_query.required_vertex_count>query.required_vertex_count
               &&body_query.required_batch_count>query.required_batch_count);
        body_storage.vertex_capacity=body_query.required_vertex_count;
        body_storage.batch_capacity=body_query.required_batch_count;
        body_storage.vertices=calloc(body_storage.vertex_capacity,
            sizeof(*body_storage.vertices));
        body_storage.batches=calloc(body_storage.batch_capacity,
            sizeof(*body_storage.batches));
        assert(body_storage.vertices!=NULL&&body_storage.batches!=NULL);
        assert(ge_original_stage_guard_runtime_build_scene_cached(
            runtime,&body_cache,view_to_world,&body_storage,&body_built)
            ==GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
        assert(body_built.input_count==body_query.input_count
               &&body_built.vertex_count==body_query.required_vertex_count
               &&body_built.batch_count==body_query.required_batch_count);
        for(index=0U;index<body_built.batch_count;++index)
            assert(body_storage.batches[index].room_id==harness.resident_room);
        stage_player.bodyModel=NULL;
        assert(ge_original_stage_guard_runtime_update_matrices(
            runtime,world_to_view)==GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
        assert(ge_original_stage_guard_runtime_build_scene_cached(
            runtime,&removed_cache,view_to_world,NULL,&removed_query)
            ==GE_ORIGINAL_STAGE_GUARD_RUNTIME_SCENE_CAPACITY_EXCEEDED);
        assert(removed_query.input_count==baseline_inputs
               &&removed_query.required_vertex_count==query.required_vertex_count
               &&removed_query.required_batch_count==query.required_batch_count);
        ge_original_model_scene_cache_close(&removed_cache);
        ge_original_model_scene_cache_close(&body_cache);
        free(body_storage.batches);free(body_storage.vertices);
    }
    /* Exercise the actual AI pad-spawn wrapper into the shared character,
     * model and prop ownership graph, then attach a runtime-created hat. */
    {
        ChrRecord *source_chr=(ChrRecord *)first.chr_record;
        PropRecord *spawned,*dynamic_hat;
        GeOriginalStageGuardSnapshot dynamic_guard;
        GeOriginalStageGuardHatSnapshot source_hat,hat;
        size_t prior_hat_count=ge_original_stage_guard_runtime_hat_count(runtime);
        size_t attachment;
        assert(source_chr!=NULL&&source_chr->ailist!=NULL
               &&first.pad_id>=0&&prior_hat_count>0U);
        assert(ge_original_stage_setup_publish(&setup));
        assert(ge_original_stage_guard_runtime_hat_snapshot(
            runtime,0U,&source_hat));
        spawned=chrSpawnAtPad(source_chr,first.body_id,
            first.resolved_head_id,first.pad_id,
            (AIListRecord *)(void *)source_chr->ailist,0);
        if(spawned==NULL){
            GeOriginalCharacterModelStats stats;
            ge_original_character_model_get_stats(models,&stats);
            fprintf(stderr,
            "dynamic pad spawn failed: count %zu, status %s, body %d, head %d, pad %d, model %s %zu/%zu\n",
            ge_original_stage_guard_runtime_count(runtime),
            ge_original_stage_guard_runtime_status_name(
                ge_original_stage_guard_runtime_last_status(runtime)),
            first.body_id,first.resolved_head_id,first.pad_id,
            ge_original_character_model_status_name(
                ge_original_character_model_last_status(models)),
            stats.instantiated_models,stats.instance_capacity);
        }
        assert(spawned!=NULL&&spawned==g_ActivePropsHead
               &&(spawned->flags&PROPFLAG_ENABLED)!=0U
               &&spawned->type==PROP_TYPE_CHR&&spawned->chr!=NULL);
        assert(ge_original_stage_guard_runtime_count(runtime)==guard_count+1U);
        assert(ge_original_stage_guard_runtime_snapshot(
            runtime,guard_count,&dynamic_guard));
        assert(dynamic_guard.command_index==SIZE_MAX
               &&dynamic_guard.pad_id==-1
               &&dynamic_guard.body_id==first.body_id
               &&dynamic_guard.resolved_head_id==first.resolved_head_id
               &&dynamic_guard.prop_record==spawned
               &&dynamic_guard.chr_record==spawned->chr
               &&dynamic_guard.active_linked!=0U);
        dynamic_hat=hatCreateForChr(spawned->chr,source_hat.model_id,0U);
        assert(dynamic_hat!=NULL&&dynamic_hat->parent==spawned
               &&spawned->chr->handle_positiondata_hat==dynamic_hat);
        assert(ge_original_stage_guard_runtime_hat_count(runtime)
               ==prior_hat_count+1U);
        assert(ge_original_stage_guard_runtime_hat_snapshot(
            runtime,prior_hat_count,&hat));
        assert(hat.command_index==SIZE_MAX
               &&hat.owner_chr_id==spawned->chr->chrnum
               &&hat.model_id==source_hat.model_id
               &&hat.prop_record==dynamic_hat
               &&dynamic_hat->obj==(ObjectRecord *)hat.hat_record);
        /* Adding the dynamic record must not relocate any already-published
         * HatRecord pointers owned by PropRecords. */
        for(attachment=0U;attachment<prior_hat_count;++attachment){
            assert(ge_original_stage_guard_runtime_hat_snapshot(
                runtime,attachment,&hat));
            assert(hat.prop_record!=NULL&&hat.hat_record!=NULL
                   &&((PropRecord *)hat.prop_record)->obj
                        ==(ObjectRecord *)hat.hat_record);
        }
    }
    /* A default-headed guard is blocked without the canonical chooser. */
    memset(&services,0,sizeof(services));
    blocked_runtime=ge_original_stage_guard_runtime_create(models,1U,
        &services,&runtime_status);assert(blocked_runtime!=NULL);
    for(index=0U;index<setup.prop_record_count;++index){
        GeOriginalStagePropConstructionRequest request;
        if(setup.prop_records[index].type!=PROPDEF_GUARD)continue;
        assert(ge_original_stage_prop_construction_request(&setup,index,&request));
        assert(!ge_original_stage_guard_runtime_construct(blocked_runtime,&request));
        assert(ge_original_stage_guard_runtime_last_status(blocked_runtime)
               ==GE_ORIGINAL_STAGE_GUARD_RUNTIME_HEAD_SELECTION_UNAVAILABLE);
        break;
    }
    printf("Facility guard runtime: %zu exact authored pairs, %zu resident, "
           "%zu part/%zu vertex/%zu batch/%zu triangle\n",guard_count,
           built.resident_guard_count,built.input_count,built.vertex_count,
           built.batch_count,built.triangle_count);
    ge_original_stage_guard_runtime_destroy(blocked_runtime);
    free(storage.batches);free(storage.vertices);
    ge_original_stage_guard_runtime_destroy(runtime);
    stage_player.prop=NULL;
    ge_original_pitem_model_provider_destroy(weapon_models);
    ge_original_character_model_provider_destroy(models);
    ge_original_stage_setup_close(&setup);free(stan_storage);free(collision);
    {
        size_t all_solo_guards=guard_count;
        GeStageId stage_id;
        memset(&services,0,sizeof(services));services.context=&harness;
        services.choose_head=choose_head;
        services.choose_sunglasses=choose_sunglasses;
        services.room_resident=room_resident;
        services.tile_rgb=tile_rgb;
        all_solo_guards+=audit_stage_guard_construction(
            &pack,ge_stage_asset_dam(),&services);
        for(stage_id=(GeStageId)(GE_STAGE_FACILITY+1);
                stage_id<GE_STAGE_COUNT;
                stage_id=(GeStageId)(stage_id+1))
            all_solo_guards+=audit_stage_guard_construction(
                &pack,ge_stage_asset_descriptor(stage_id),&services);
        printf("all solo-stage guard construction: %zu exact authored pairs "
               "across %u stages, including all 36 Dam guards\n",
               all_solo_guards,(unsigned)GE_STAGE_COUNT);
        {
            size_t model_id,unique_hats=0U;
            for(model_id=0U;model_id<sizeof(campaign_hat_models);++model_id)
                unique_hats+=campaign_hat_models[model_id]!=0U;
            printf("all non-Dam authored hats: %zu attached, %zu exact models; "
                   "lighting/shadow publication across every guard\n",
                   campaign_attached_hats,unique_hats);
            assert(campaign_attached_hats>0U&&unique_hats>0U);
        }
        fflush(stdout);
        assert(all_solo_guards==665U);
    }
    ge_asset_pack_close(&pack);return 0;
}
