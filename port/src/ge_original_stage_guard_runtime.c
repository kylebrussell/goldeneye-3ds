#include "ge_original_stage_guard_runtime.h"

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/chrai.h"
#include "game/chraction.h"
#include "game/model.h"
#include "game/matrixmath.h"
#ifndef PLAYERFLAG
typedef int PLAYERFLAG;
#endif
#include "game/bondview.h"
#include "ge_original_player_gait_internal.h"
#include "ge_original_default_object_internal.h"
#include "ge_original_stage_guard_actor.h"
#include "ge_original_global_ai.h"
#include "random.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

extern PropRecord *g_ActivePropsHead;
extern struct player *g_CurrentPlayer;
extern void chrpropActivateThisFrame(PropRecord *prop);
extern void chrpropDelist(PropRecord *prop);
extern void chrpropEnable(PropRecord *prop);
extern signed short sins(unsigned short angle);

typedef struct GeOriginalStageGuardSlot {
    GeOriginalStageGuardSnapshot state;
    GeOriginalCharacterModelPair pair;
    RenderPosView *render_positions;
} GeOriginalStageGuardSlot;

typedef struct GeOriginalStageGuardWeaponSlot {
    GeOriginalStageGuardWeaponSnapshot state;
    WeaponObjRecord weapon;
    PropRecord *prop;
    PropRecord fallback_prop;
    ModelFileHeader *header;
    Model *model;
    RenderPosView *render_positions;
    GeOriginalStageGuardSlot *owner;
} GeOriginalStageGuardWeaponSlot;

typedef struct GeOriginalStageGuardHatSlot {
    GeOriginalStageGuardHatSnapshot state;
    HatRecord hat;
    PropRecord *prop;
    PropRecord fallback_prop;
    ModelFileHeader *header;
    Model *model;
    RenderPosView *render_positions;
    float pitem_scale;
    GeOriginalStageGuardSlot *owner;
} GeOriginalStageGuardHatSlot;

struct GeOriginalStageGuardRuntime {
    GeOriginalCharacterModelProvider *models;
    GeOriginalStageGuardRuntimeServices services;
    GeOriginalStageGuardSlot *slots;
    ChrRecord *chrs;
    PropRecord **props;
    PropRecord *fallback_props;
    GeOriginalPitemModelProvider *weapon_models;
    GeOriginalStageGuardWeaponSlot *weapons;
    size_t weapon_count;
    GeOriginalStageGuardHatSlot *hats;
    size_t hat_count;
    size_t hat_capacity;
    GeOriginalModelSceneInput *scene_inputs;
    size_t scene_input_capacity;
    GeOriginalCharacterModelScenePart *scene_character_parts;
    size_t scene_character_part_capacity;
    uint64_t scene_collect_calls;
    uint64_t scene_scratch_allocation_events;
    uint64_t scene_allocation_free_collect_calls;
    float draw_world_to_view[4][4];
    uint8_t draw_camera_ready;
    size_t capacity;
    size_t count;
    GeOriginalStageGuardRuntimeStatus last_status;
    GeOriginalCharacterModelPair player_pair;
    RenderPosView *player_render_positions;
    int32_t player_body_id;
    int32_t player_head_id;
    uint8_t player_pair_ready;
    uint8_t player_matrices_ready;
    GeOriginalStageGuardWeaponSlot player_weapon;
    uint8_t player_weapon_ready;
    struct player *player_owner;
    PropRecord *player_prop;
};

static GeOriginalStageGuardRuntime *ge_active_stage_guard_runtime;

int ge_original_stage_guard_snapshot_death_complete(
    const GeOriginalStageGuardSnapshot *snapshot)
{
    return snapshot != NULL
        && (snapshot->action_type == ACT_DIE
            || snapshot->action_type == ACT_DEAD
            || (!snapshot->active_linked
                && snapshot->room_id == UINT8_MAX));
}

static PropRecord *runtime_allocate_prop(GeOriginalStageGuardRuntime *runtime,
                                         PropRecord *fallback)
{
    PropRecord *prop = runtime->services.allocate_prop != NULL
        ? runtime->services.allocate_prop(runtime->services.context)
        : fallback;
    if (prop != NULL) memset(prop, 0, sizeof(*prop));
    return prop;
}

extern void subcalcmatrices(ModelRenderData *renderdata, Model *model);
extern Mtxf *modelFindNodeMtx(Model *model,ModelNode *node,s32 arg2);

static int matrix_valid(const float matrix[4][4])
{
    size_t row,column;
    if(matrix==NULL)return 0;
    for(row=0;row<4U;++row)for(column=0;column<4U;++column)
        if(!isfinite(matrix[row][column]))return 0;
    return 1;
}

/* chrTick publishes exact animated matrices in the shared transient frame
 * arena.  Character instances also own durable renderer slots; retain the
 * canonical result there before the arena is reused.  A zero return means no
 * transient publication occurred (bootstrap or canonical offscreen branch),
 * so the caller may run its renderer-only fallback calculation. */
static int runtime_retain_matrices(Model *model,
                                   RenderPosView *durable,
                                   size_t matrix_count)
{
    if(model==NULL||durable==NULL||matrix_count==0U
            ||model->obj==NULL
            ||matrix_count>(size_t)model->obj->numMatrices
            ||model->render_pos==NULL)return -1;
    if(model->render_pos==durable)return 0;
    memcpy(durable,model->render_pos,
        matrix_count*sizeof(*durable));
    model->render_pos=durable;
    return 1;
}

static ChrRecord *runtime_live_chr(
    const GeOriginalStageGuardRuntime *runtime,size_t guard_index);

int ge_original_stage_guard_draw_sphere_visible(
    const float world_to_view[4][4],float vertical_fov_degrees,
    float aspect,float near_distance,const float center[3],float radius)
{
    float view[3],depth,tan_vertical,tan_horizontal;
    float vertical_normal,horizontal_normal;
    size_t axis;
    if(!matrix_valid(world_to_view)||center==NULL
            ||!isfinite(vertical_fov_degrees)
            ||!isfinite(aspect)||!isfinite(near_distance)
            ||!isfinite(radius)||vertical_fov_degrees<=0.0f
            ||vertical_fov_degrees>=180.0f||aspect<=0.0f
            ||near_distance<=0.0f||radius<0.0f)return 1;
    for(axis=0U;axis<3U;++axis)if(!isfinite(center[axis]))return 1;
    for(axis=0U;axis<3U;++axis)
        view[axis]=center[0]*world_to_view[0][axis]
            +center[1]*world_to_view[1][axis]
            +center[2]*world_to_view[2][axis]
            +world_to_view[3][axis];
    if(!isfinite(view[0])||!isfinite(view[1])||!isfinite(view[2]))return 1;
    depth=-view[2];
    /* Strict separation keeps spheres tangent to the near plane. */
    if(depth+radius<near_distance)return 0;
    tan_vertical=tanf(vertical_fov_degrees*(M_PI_F/360.0f));
    tan_horizontal=tan_vertical*aspect;
    if(!isfinite(tan_vertical)||!isfinite(tan_horizontal)
            ||tan_vertical<=0.0f||tan_horizontal<=0.0f)return 1;
    vertical_normal=sqrtf(1.0f+tan_vertical*tan_vertical);
    horizontal_normal=sqrtf(1.0f+tan_horizontal*tan_horizontal);
    if(view[0]-depth*tan_horizontal>radius*horizontal_normal
            ||-view[0]-depth*tan_horizontal>radius*horizontal_normal
            ||view[1]-depth*tan_vertical>radius*vertical_normal
            ||-view[1]-depth*tan_vertical>radius*vertical_normal)return 0;
    return 1;
}

static int runtime_guard_draw_visible(
    const GeOriginalStageGuardRuntime *runtime,size_t guard_index)
{
    ChrRecord *chr;
    Model *model;
    float center[3],radius;
    if(runtime==NULL||!runtime->draw_camera_ready
            ||g_CurrentPlayer==NULL)return 1;
    chr=runtime_live_chr(runtime,guard_index);
    if(chr==NULL||chr->prop==NULL||(model=chr->model)==NULL
            ||model->obj==NULL)return 1;
    center[0]=chr->prop->pos.x;center[1]=chr->prop->pos.y;
    center[2]=chr->prop->pos.z;
    radius=model->obj->BoundingVolumeRadius*model->scale;
    return ge_original_stage_guard_draw_sphere_visible(
        runtime->draw_world_to_view,g_CurrentPlayer->c_perspfovy,
        g_CurrentPlayer->c_perspaspect,g_CurrentPlayer->c_perspnear,
        center,radius);
}

static int runtime_room_resident(const GeOriginalStageGuardRuntime *runtime,
                                 uint8_t room)
{
    return runtime->services.room_resident==NULL
        || runtime->services.room_resident(runtime->services.context,room);
}

/* solo_char_load owns the lifetime decision: the body is present only in the
 * third-person/cinematic camera modes which asked it to be constructed, and
 * bondviewRemovePlayerBody clears these same original ownership relations on
 * return to first person.  Keep renderer publication subordinate to those
 * relations instead of duplicating camera-mode policy here. */
static Model *runtime_live_player_body(
    const GeOriginalStageGuardRuntime *runtime,uint8_t *room_id)
{
    PropRecord *prop;ChrRecord *chr;Model *model;int32_t room;
    if(runtime==NULL||!runtime->player_pair_ready
            ||runtime->player_owner==NULL||runtime->player_prop==NULL)
        return NULL;
    prop=runtime->player_prop;chr=prop->chr;
    model=runtime->player_pair.model_instance;
    if(model==NULL||runtime->player_owner->prop!=prop
            ||runtime->player_owner->bodyModel!=model
            ||prop->type!=PROP_TYPE_VIEWER||chr==NULL
            ||chr->prop!=prop||chr->model!=model)return NULL;
    room=(int32_t)prop->rooms[0];
    if((room<0||room>=UINT8_MAX)&&prop->stan!=NULL)room=prop->stan->room;
    if(room<0||room>=UINT8_MAX)return NULL;
    if(room_id!=NULL)*room_id=(uint8_t)room;
    return model;
}

static GeOriginalStageGuardWeaponSlot *runtime_live_player_weapon(
    GeOriginalStageGuardRuntime *runtime)
{
    GeOriginalStageGuardWeaponSlot *slot;ChrRecord *chr;int hand;
    if(runtime_live_player_body(runtime,NULL)==NULL
            ||!runtime->player_weapon_ready)return NULL;
    slot=&runtime->player_weapon;chr=runtime->player_prop->chr;
    hand=slot->state.hand;
    if(hand<GUNRIGHT||hand>GUNLEFT||slot->prop==NULL||slot->model==NULL
            ||slot->weapon.prop!=slot->prop||slot->weapon.model!=slot->model
            ||slot->prop->type!=PROP_TYPE_WEAPON
            ||slot->prop->obj!=(ObjectRecord *)(void *)&slot->weapon
            ||slot->prop->parent!=runtime->player_prop
            ||chr->weapons_held[hand]!=slot->prop)return NULL;
    return slot;
}

static void runtime_release_player_weapon(
    GeOriginalStageGuardRuntime *runtime)
{
    if(runtime==NULL)return;
    if(runtime->weapon_models!=NULL&&runtime->player_weapon.model!=NULL)
        (void)ge_original_pitem_model_release_instance(
            runtime->weapon_models,runtime->player_weapon.model);
    memset(&runtime->player_weapon,0,sizeof(runtime->player_weapon));
    runtime->player_weapon_ready=0U;
}

/* ChrRecords are stable for the stage lifetime, but their PropRecords are
 * canonical shared-pool entries. chrpropCleanupForRemoval clears model/chrnum
 * before propsTick returns the prop to that pool, where its union may
 * immediately become an object, weapon, or smoke record. Never recover a
 * character by reading that reused union unless every original ownership
 * relation still identifies this authored slot. */
static ChrRecord *runtime_live_chr(
    const GeOriginalStageGuardRuntime *runtime, size_t guard_index)
{
    PropRecord *prop;
    ChrRecord *chr;
    const GeOriginalStageGuardSlot *slot;

    if (runtime == NULL || guard_index >= runtime->count) return NULL;
    prop = runtime->props[guard_index];
    slot = &runtime->slots[guard_index];
    chr = (ChrRecord *)slot->state.chr_record;
    if (prop == NULL || chr == NULL || prop->type != PROP_TYPE_CHR
            || prop->chr != chr
            || chr->prop != prop || chr->model != slot->pair.model_instance
            || chr->chrnum != (s16)slot->state.chr_id) return NULL;
    return chr;
}

static int runtime_model_available(void *context,int32_t model_id)
{
    GeOriginalStageGuardRuntime *runtime=context;
    return runtime!=NULL&&ge_original_character_model_available(
        runtime->models,model_id);
}

static int metadata_for_id(int32_t model_id,
                           GeOriginalCharacterModelMetadata *metadata)
{
    size_t index;
    for(index=0;index<ge_original_character_model_dependency_count();++index){
        GeOriginalCharacterModelMetadata candidate;
        if(ge_original_character_model_dependency_metadata(index,&candidate)
                &&candidate.model_id==model_id){
            if(metadata!=NULL)*metadata=candidate;
            return 1;
        }
    }
    return 0;
}

GeOriginalStageGuardRuntime *ge_original_stage_guard_runtime_create(
    GeOriginalCharacterModelProvider *models,size_t guard_capacity,
    const GeOriginalStageGuardRuntimeServices *services,
    GeOriginalStageGuardRuntimeStatus *status)
{
    GeOriginalStageGuardRuntime *runtime=NULL;
    GeOriginalStageGuardRuntimeStatus local=
        GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK;
    if(models==NULL||guard_capacity==0U){
        local=GE_ORIGINAL_STAGE_GUARD_RUNTIME_INVALID_ARGUMENT;
    }else{
        runtime=calloc(1,sizeof(*runtime));
        if(runtime!=NULL){
            runtime->slots=calloc(guard_capacity,sizeof(*runtime->slots));
            runtime->chrs=calloc(guard_capacity,sizeof(*runtime->chrs));
            runtime->props=calloc(guard_capacity,sizeof(*runtime->props));
            runtime->fallback_props=calloc(
                guard_capacity,sizeof(*runtime->fallback_props));
        }
        if(runtime==NULL||runtime->slots==NULL||runtime->chrs==NULL
                ||runtime->props==NULL||runtime->fallback_props==NULL){
            if(runtime!=NULL)free(runtime->slots);
            if(runtime!=NULL)free(runtime->chrs);
            if(runtime!=NULL)free(runtime->props);
            if(runtime!=NULL)free(runtime->fallback_props);
            free(runtime);runtime=NULL;
            local=GE_ORIGINAL_STAGE_GUARD_RUNTIME_CAPACITY_EXHAUSTED;
        }else{
            runtime->models=models;runtime->capacity=guard_capacity;
            if(!ge_original_stage_guard_actor_pool_begin(runtime->chrs,
                                                         guard_capacity)){
                free(runtime->fallback_props);free(runtime->props);
                free(runtime->chrs);free(runtime->slots);
                free(runtime);runtime=NULL;
                local=GE_ORIGINAL_STAGE_GUARD_RUNTIME_ACTOR_UNAVAILABLE;
                if(status!=NULL)*status=local;
                return NULL;
            }
            if(services!=NULL)runtime->services=*services;
            runtime->last_status=local;
            ge_active_stage_guard_runtime=runtime;
        }
    }
    if(status!=NULL)*status=local;
    return runtime;
}

void ge_original_stage_guard_runtime_destroy(
    GeOriginalStageGuardRuntime *runtime)
{
    size_t index;
    if(runtime==NULL)return;
    /* AI-created guards use the canonical immediate active-list insertion.
     * Remove every still-linked root before releasing runtime-owned records;
     * authored roots may also be linked by the stage composer, and the exact
     * delist operation is safe for either ownership path. */
    for(index=0U;index<runtime->count;++index){
        PropRecord *prop=runtime->props[index];
        PropRecord *cursor=g_ActivePropsHead;
        size_t visited=0U;
        while(cursor!=NULL&&visited++<MAX_PROPS){
            if(cursor==prop){chrpropDelist(prop);break;}
            cursor=cursor->next;
        }
    }
    if(runtime->player_owner!=NULL&&runtime->player_prop!=NULL){
        if(runtime->player_prop->chr!=NULL
                &&runtime->player_prop->chr->model
                    ==runtime->player_pair.model_instance)
            runtime->player_prop->chr=NULL;
        if(runtime->player_owner->bodyModel
                ==runtime->player_pair.model_instance)
            runtime->player_owner->bodyModel=NULL;
    }
    if(ge_active_stage_guard_runtime==runtime)
        ge_active_stage_guard_runtime=NULL;
    if(runtime->weapon_models!=NULL)for(index=0U;index<runtime->weapon_count;++index)
        if(runtime->weapons[index].model!=NULL)
            (void)ge_original_pitem_model_release_instance(
                runtime->weapon_models,runtime->weapons[index].model);
    runtime_release_player_weapon(runtime);
    if(runtime->weapon_models!=NULL)for(index=0U;index<runtime->hat_count;++index)
        if(runtime->hats[index].model!=NULL)
            (void)ge_original_pitem_model_release_instance(
                runtime->weapon_models,runtime->hats[index].model);
    ge_original_stage_guard_actor_pool_end(runtime->chrs);
    free(runtime->scene_character_parts);free(runtime->scene_inputs);
    free(runtime->hats);free(runtime->weapons);free(runtime->fallback_props);
    free(runtime->props);free(runtime->chrs);
    free(runtime->slots);free(runtime);
}

int ge_original_stage_guard_runtime_load_player_body(
    void *context,struct player *player,int32_t body_id,
    int32_t head_id,float yaw)
{
    GeOriginalStageGuardRuntime *runtime=context;
    ChrRecord *self;Model *model;int new_pair=0;
    if(runtime==NULL||player==NULL||player->prop==NULL
            ||player->prop->stan==NULL||!isfinite(yaw))return 0;
    if(runtime->player_weapon_ready
            &&runtime_live_player_weapon(runtime)==NULL)
        runtime_release_player_weapon(runtime);
    runtime->player_matrices_ready=0U;
    if(player->prop->chr!=NULL){
        runtime->player_owner=player;runtime->player_prop=player->prop;
        self=player->prop->chr;
        if(self->model==NULL)return 0;
        if(self->model->anim!=NULL)return 1;
        self->chrflags|=CHRFLAG_INIT;
        chrlvMergeKneelToStand(self,0.0f);
        ge_original_stage_guard_model_set_root_offset_exact(
            player->bodyModel,&player->prop->pos);
        ge_original_stage_guard_model_set_root_angle_exact(
            player->bodyModel,yaw);
        return 1;
    }
    if(!runtime->player_pair_ready){
        if(!ge_original_character_model_resolve_pair(runtime->models,
                body_id,head_id,0,&runtime->player_pair))return 0;
        runtime->player_body_id=body_id;runtime->player_head_id=head_id;
        runtime->player_render_positions=
            ((Model *)runtime->player_pair.model_instance)->render_pos;
        runtime->player_pair_ready=1U;new_pair=1;
    }else if(runtime->player_body_id!=body_id
            ||runtime->player_head_id!=head_id)return 0;
    model=runtime->player_pair.model_instance;
    if(model==NULL||!ge_original_stage_guard_actor_pool_begin(
            runtime->chrs,runtime->capacity))return 0;
    if(new_pair)ge_original_stage_guard_model_set_scale_exact(
        model,model->scale*0.97f);
    if(ge_original_stage_guard_actor_construct_exact(player->prop,model,
            &player->prop->pos,yaw,player->prop->stan,NULL)==NULL)return 0;
    player->bodyModel=model;player->prop->type=PROP_TYPE_VIEWER;
    runtime->player_owner=player;runtime->player_prop=player->prop;
    self=player->prop->chr;self->chrflags|=CHRFLAG_INIT;
    ge_original_stage_guard_model_set_root_offset_exact(
        model,&player->prop->pos);
    ge_original_stage_guard_model_set_root_angle_exact(model,yaw);
    chrlvMergeKneelToStand(self,0.0f);
    return 1;
}

int ge_original_stage_guard_runtime_attach_player_held_item(
    void *context,struct player *player,int32_t prop_id,
    int32_t item_id,uint32_t flags)
{
    GeOriginalStageGuardRuntime *runtime=context;
    GeOriginalStageGuardWeaponSlot *slot;ChrRecord *chr;
    ObjectRecord *object;void *header=NULL,*model=NULL;float pitem_scale=0.0f;
    int hand=(flags&PROPFLAG_WEAPON_LEFTHANDED)!=0U?GUNLEFT:GUNRIGHT;
    size_t axis;
    if(runtime==NULL||player==NULL||runtime->weapon_models==NULL
            ||player!=runtime->player_owner
            ||runtime_live_player_body(runtime,NULL)==NULL)return 0;
    slot=&runtime->player_weapon;chr=runtime->player_prop->chr;
    if(runtime->player_weapon_ready){
        GeOriginalStageGuardWeaponSlot *live=
            runtime_live_player_weapon(runtime);
        return live!=NULL&&live->state.model_id==prop_id
            &&live->state.weapon_id==item_id
            &&live->state.hand==(uint8_t)hand;
    }
    if(chr==NULL||chr->model==NULL||chr->model->obj==NULL
            ||chr->model->obj->numSwitches<=(hand==GUNLEFT?5:3)
            ||chr->model->obj->Switches[hand==GUNLEFT?5:3]==NULL)
        return 0;
    if(!ge_original_pitem_model_resolve_instance(runtime->weapon_models,
            prop_id,&header,&model,&pitem_scale)
            ||header==NULL||model==NULL)return 0;
    memset(slot,0,sizeof(*slot));slot->header=header;slot->model=model;
    slot->render_positions=((Model *)model)->render_pos;
    slot->prop=runtime_allocate_prop(runtime,&slot->fallback_prop);
    if(slot->prop==NULL)goto fail;
    /* Exact blank_08_object_preset_4001 state from propobj.c. The native
     * provider supplies only its resource/allocation boundary; object and
     * character relation semantics remain the original ones below. */
    slot->weapon.extrascale=0x0100;slot->weapon.type=PROPDEF_COLLECTABLE;
    slot->weapon.pad=chr->chrnum;slot->weapon.flags=flags|0x4000U;
    slot->weapon.damage=1000.0f;slot->weapon.weaponnum=(ITEM_IDS)item_id;
    slot->weapon.obj=(PROP)prop_id;slot->weapon.LinkedWeaponType=-1;
    slot->weapon.timer=-1;
    slot->weapon.shadecol.r=slot->weapon.shadecol.g=
        slot->weapon.shadecol.b=0xffU;
    slot->weapon.nextcol.r=slot->weapon.nextcol.g=
        slot->weapon.nextcol.b=0xffU;
    for(axis=0U;axis<4U;++axis)slot->weapon.mtx.m[axis][axis]=1.0f;
    object=(ObjectRecord *)(void *)&slot->weapon;
    if(ge_original_objInitPreallocatedSlice(object,header,slot->prop,
            model,pitem_scale,NULL)==NULL)goto fail;
    slot->prop->type=PROP_TYPE_WEAPON;
    ge_original_stage_guard_actor_set_gunfire_visible(slot->prop,0);
    if(!ge_original_stage_guard_actor_equip_weapon(&slot->weapon,chr))goto fail;
    slot->state.command_index=SIZE_MAX;
    slot->state.owner_chr_id=chr->chrnum;slot->state.model_id=prop_id;
    slot->state.weapon_id=item_id;slot->state.hand=(uint8_t)hand;
    slot->state.model_instance=model;slot->state.prop_record=slot->prop;
    slot->state.weapon_record=&slot->weapon;
    runtime->player_weapon_ready=1U;
    runtime->last_status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK;return 1;
fail:
    if(slot->prop!=NULL&&slot->prop!=&slot->fallback_prop)
        chrpropFree(slot->prop);
    (void)ge_original_pitem_model_release_instance(
        runtime->weapon_models,model);
    memset(slot,0,sizeof(*slot));
    runtime->last_status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_WEAPON_UNAVAILABLE;
    return 0;
}

/* Native allocation owner for the unchanged chrSpawnAtPad/chrSpawnAtChr
 * bodies. init_guards reserves authored_count + 10 slots on N64; the 3DS
 * stage bootstrap now gives this runtime the same spare-slot capacity. */
PropRecord *chrSpawnAtCoord(s32 bodynum, s32 headnum, coord3d *pos,
    StandTile *stan, f32 angle, AIListRecord *ailist, s32 spawnflags)
{
    GeOriginalStageGuardRuntime *runtime=ge_active_stage_guard_runtime;
    GeOriginalStageGuardSlot *slot;
    GeOriginalCharacterModelMetadata metadata;
    PropRecord *prop;
    ChrRecord *chr;
    s32 resolved_head=headnum;
    int32_t selected_head;
    int sunglasses=0;
    if(runtime==NULL||pos==NULL||stan==NULL||ailist==NULL
            ||runtime->count>=runtime->capacity
            ||!metadata_for_id(bodynum,&metadata)
            ||!metadata.is_body_dependency)return NULL;
    if(metadata.has_integrated_head){
        resolved_head=-1;
    }else if(resolved_head<0){
        selected_head=(int32_t)resolved_head;
        if(runtime->services.choose_head==NULL
                ||!runtime->services.choose_head(runtime->services.context,
                    bodynum,&selected_head))return NULL;
        resolved_head=(s32)selected_head;
    }
    if(!metadata.has_integrated_head){
        if((spawnflags&1)!=0)sunglasses=1;
        else if((spawnflags&2)!=0){
            if(runtime->services.choose_sunglasses==NULL
                    ||!runtime->services.choose_sunglasses(
                        runtime->services.context,2U,&sunglasses))return NULL;
        }
    }
    slot=&runtime->slots[runtime->count];memset(slot,0,sizeof(*slot));
    if(!ge_original_character_model_resolve_pair(runtime->models,bodynum,
            resolved_head,sunglasses,&slot->pair))return NULL;
    slot->render_positions=
        ((Model *)slot->pair.model_instance)->render_pos;
    prop=runtime_allocate_prop(runtime,
        &runtime->fallback_props[runtime->count]);
    if(prop==NULL)return NULL;
    if(!ge_original_stage_guard_actor_pool_begin(runtime->chrs,
                                                 runtime->capacity))
        return NULL;
    if(ge_original_stage_guard_actor_construct_exact(prop,
            slot->pair.model_instance,pos,angle,stan,(AIRecord *)ailist)==NULL)
        return NULL;
    chr=prop->chr;
    chrpropActivateThisFrame(prop);
    chrpropEnable(prop);
    chr->headnum=(s8)resolved_head;chr->bodynum=(s8)bodynum;
    slot->state.command_index=SIZE_MAX;
    slot->state.chr_id=chr->chrnum;
    slot->state.body_id=bodynum;
    slot->state.authored_head_id=headnum;
    slot->state.resolved_head_id=resolved_head;
    slot->state.ai_list_id=-1;
    slot->state.pad_id=-1;
    slot->state.room_id=(uint8_t)stan->room;
    slot->state.sunglasses=(uint8_t)sunglasses;
    slot->state.visible=1U;
    slot->state.ai_list_resolved=1U;
    memcpy(slot->state.position,pos->f,sizeof(slot->state.position));
    slot->state.angle=angle;
    slot->state.model_instance=slot->pair.model_instance;
    slot->state.prop_record=prop;
    slot->state.chr_record=chr;
    runtime->props[runtime->count]=prop;
    ++runtime->count;
    runtime->last_status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK;
    return prop;
}

int ge_original_stage_guard_runtime_materializer(
    GeOriginalStageGuardRuntime *runtime,
    GeOriginalStagePropMaterializerProviders *providers)
{
    if(runtime==NULL||providers==NULL)return 0;
    memset(providers,0,sizeof(*providers));
    providers->context=runtime;
    providers->capabilities=GE_ORIGINAL_STAGE_PROP_CAP_GUARD;
    providers->model_available=runtime_model_available;
    providers->construct_guard=ge_original_stage_guard_runtime_construct;
    return 1;
}

static GeOriginalStageGuardRuntimeStatus runtime_construct(
    GeOriginalStageGuardRuntime *runtime,
    const GeOriginalStagePropConstructionRequest *request)
{
    const GeOriginalStagePropRecord *record;
    GeOriginalCharacterModelMetadata body_metadata;
    GeOriginalStageGuardSlot *slot;
    int32_t authored_head,resolved_head;
    uint16_t appearance;
    int sunglasses=0;AIRecord *ailist=NULL;size_t ai_index;
    if(runtime==NULL||request==NULL||request->record==NULL
            ||request->service!=GE_ORIGINAL_STAGE_PROP_SERVICE_GUARD
            ||request->record->type!=PROPDEF_GUARD)
        return GE_ORIGINAL_STAGE_GUARD_RUNTIME_INVALID_ARGUMENT;
    if(runtime->count>=runtime->capacity)
        return GE_ORIGINAL_STAGE_GUARD_RUNTIME_CAPACITY_EXHAUSTED;
    if(!request->placement_resolved||!request->placement.has_stan
            ||request->placement.room<0||request->placement.room>=UINT8_MAX)
        return GE_ORIGINAL_STAGE_GUARD_RUNTIME_PLACEMENT_UNAVAILABLE;
    record=request->record;
    if(record->word_count!=7U||!metadata_for_id(record->model_id,
                                                &body_metadata)
            ||!body_metadata.is_body_dependency)
        return GE_ORIGINAL_STAGE_GUARD_RUNTIME_MODEL_UNAVAILABLE;
    authored_head=(int16_t)record->words[5];
    resolved_head=authored_head;
    appearance=(uint16_t)(record->words[5]>>16);
    if(body_metadata.has_integrated_head){
        resolved_head=-1;
    }else if(resolved_head<0){
        if(runtime->services.choose_head==NULL
                ||!runtime->services.choose_head(runtime->services.context,
                                                  record->model_id,
                                                  &resolved_head))
            return GE_ORIGINAL_STAGE_GUARD_RUNTIME_HEAD_SELECTION_UNAVAILABLE;
    }
    if(!body_metadata.has_integrated_head){
        if((appearance&1U)!=0U){
            sunglasses=1;
        }else if((appearance&2U)!=0U){
            if(runtime->services.choose_sunglasses==NULL
                    ||!runtime->services.choose_sunglasses(
                        runtime->services.context,appearance,&sunglasses)
                    ||(sunglasses!=0&&sunglasses!=1))
                return GE_ORIGINAL_STAGE_GUARD_RUNTIME_SUNGLASSES_SELECTION_UNAVAILABLE;
        }
    }
    slot=&runtime->slots[runtime->count];
    memset(slot,0,sizeof(*slot));
    if(!ge_original_character_model_resolve_pair(runtime->models,
            record->model_id,resolved_head,sunglasses,&slot->pair))
        return GE_ORIGINAL_STAGE_GUARD_RUNTIME_MODEL_UNAVAILABLE;
    slot->render_positions=
        ((Model *)slot->pair.model_instance)->render_pos;
    slot->state.command_index=request->command_index;
    slot->state.chr_id=record->chr_id;
    slot->state.body_id=record->model_id;
    slot->state.authored_head_id=authored_head;
    slot->state.resolved_head_id=resolved_head;
    slot->state.ai_list_id=record->ai_list_id;
    slot->state.pad_id=record->pad_id;
    slot->state.preset=(uint16_t)(record->words[3]>>16);
    slot->state.chrpreset=(uint16_t)record->words[3];
    slot->state.health=(uint16_t)(record->words[4]>>16);
    slot->state.reaction=(uint16_t)record->words[4];
    slot->state.appearance_flags=appearance;
    slot->state.room_id=(uint8_t)request->placement.room;
    slot->state.sunglasses=(uint8_t)sunglasses;
    slot->state.visible=1U;
    memcpy(slot->state.position,request->placement.position,
           sizeof(slot->state.position));
    slot->state.angle=atan2f(request->placement.look[0],
                            request->placement.look[2]);
    slot->state.model_instance=slot->pair.model_instance;
    if(request->runtime==NULL||request->runtime->setup==NULL
            ||request->runtime->setup->ailists==NULL)
        return GE_ORIGINAL_STAGE_GUARD_RUNTIME_AI_LIST_UNAVAILABLE;
    if(isGlobalAIListID(record->ai_list_id)){
        ailist=ge_original_global_ai_find(record->ai_list_id);
    }else{
        for(ai_index=0U;ai_index<request->runtime->ailist_count;++ai_index){
            AIListRecord *entry=&request->runtime->setup->ailists[ai_index];
            if(entry->ID==record->ai_list_id){ailist=entry->ailist;break;}
        }
    }
    if(ailist==NULL)return GE_ORIGINAL_STAGE_GUARD_RUNTIME_AI_LIST_UNAVAILABLE;
    /* The extracted constructor retains its canonical global slot search.
     * Rebind immediately before each call so multiple staged runtimes cannot
     * redirect one another's ChrRecord ownership. Authored chrnum replaces
     * the constructor's temporary 5000-series ID below. */
    runtime->props[runtime->count]=runtime_allocate_prop(runtime,
        &runtime->fallback_props[runtime->count]);
    if(runtime->props[runtime->count]==NULL)
        return GE_ORIGINAL_STAGE_GUARD_RUNTIME_ACTOR_UNAVAILABLE;
    if(!ge_original_stage_guard_actor_pool_begin(runtime->chrs,
                                                 runtime->capacity))
        return GE_ORIGINAL_STAGE_GUARD_RUNTIME_ACTOR_UNAVAILABLE;
    if(ge_original_stage_guard_actor_construct_exact(
            runtime->props[runtime->count],slot->pair.model_instance,
            (coord3d *)(void *)slot->state.position,slot->state.angle,
            (StandTile *)request->placement.stan,ailist)==NULL
            ||!ge_original_stage_guard_actor_apply_setup(
                runtime->props[runtime->count],record->chr_id,
                record->model_id,resolved_head,slot->state.health,
                slot->state.reaction,slot->state.preset,
                slot->state.chrpreset,appearance))
        return GE_ORIGINAL_STAGE_GUARD_RUNTIME_ACTOR_UNAVAILABLE;
    slot->state.prop_record=runtime->props[runtime->count];
    slot->state.chr_record=runtime->props[runtime->count]->chr;
    slot->state.ai_list_resolved=ailist!=NULL;
    ++runtime->count;
    return GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK;
}

int ge_original_stage_guard_runtime_construct(
    void *context,const GeOriginalStagePropConstructionRequest *request)
{
    GeOriginalStageGuardRuntime *runtime=context;
    GeOriginalStageGuardRuntimeStatus status=runtime_construct(runtime,request);
    if(runtime!=NULL)runtime->last_status=status;
    return status==GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK;
}

size_t ge_original_stage_guard_runtime_count(
    const GeOriginalStageGuardRuntime *runtime)
{return runtime!=NULL?runtime->count:0U;}

int ge_original_stage_guard_runtime_snapshot(
    const GeOriginalStageGuardRuntime *runtime,size_t guard_index,
    GeOriginalStageGuardSnapshot *snapshot)
{
    ChrRecord *chr;
    PropRecord *prop;
    if(runtime==NULL||snapshot==NULL||guard_index>=runtime->count)return 0;
    *snapshot=runtime->slots[guard_index].state;
    prop=runtime->props[guard_index];
    snapshot->prop_flags=(uint32_t)prop->flags;
    snapshot->prop_zdepth=prop->zDepth;
    chr=runtime_live_chr(runtime,guard_index);
    if(chr==NULL){
        snapshot->visible=0U;
        snapshot->matrices_ready=0U;
        snapshot->active_linked=0U;
        snapshot->animation_active=0U;
        snapshot->room_id=UINT8_MAX;
        return 1;
    }
    {
        PropRecord *cursor=g_ActivePropsHead;
        size_t visited=0U;
        while(cursor!=NULL&&visited++<MAX_PROPS){
            if(cursor==prop){
                snapshot->active_linked=1U;
                break;
            }
            cursor=cursor->next;
        }
    }
    memcpy(snapshot->position,prop->pos.f,
        sizeof(snapshot->position));
    if(prop->rooms[0]!=UINT8_MAX)
        snapshot->room_id=prop->rooms[0];
    snapshot->animation_active=(uint8_t)(chr->model->anim!=NULL);
    {
        snapshot->action_type=(uint8_t)chr->actiontype;
        snapshot->ai_offset=chr->aioffset;
        snapshot->ai_opcode=chr->ailist!=NULL
            ?((const u8 *)chr->ailist)[chr->aioffset]:UINT8_MAX;
        snapshot->sleep=chr->sleep;
        snapshot->chr_flags=(uint32_t)chr->chrflags;
        snapshot->hidden=chr->hidden;
        snapshot->alertness=chr->alertness;
        snapshot->morale=chr->morale;
        snapshot->firecount[0]=chr->firecount[0];
        snapshot->firecount[1]=chr->firecount[1];
        if(chr->actiontype==ACT_STAND){
            snapshot->stand_prestand=(uint8_t)(chr->act_stand.prestand!=0);
            snapshot->stand_reaim=(uint8_t)(chr->act_stand.reaim!=0);
        }
        snapshot->last_seen_target_60=chr->lastseetarget60;
        snapshot->last_heard_target_60=chr->lastheartarget60;
        snapshot->vision_range=chr->visionrange;
        snapshot->damage=chr->damage;
        snapshot->max_damage=chr->maxdamage;
        snapshot->shotbondsum=chr->shotbondsum;
        if(chr->model!=NULL){
            snapshot->model_size=getinstsize(chr->model);
            snapshot->animation_frame=
                chr->model->framea;
            snapshot->model_angle=
                ge_original_stage_guard_model_get_root_angle_exact(chr->model);
        }
    }
    return 1;
}

int ge_original_stage_guard_runtime_autoaim_world_position(
    const GeOriginalStageGuardRuntime *runtime,size_t guard_index,
    const float view_to_world[4][4],float world_position[3])
{
    PropRecord *prop;
    ChrRecord *chr;
    Model *model;
    float view_offset[3];
    size_t axis;
    if(runtime==NULL||guard_index>=runtime->count||world_position==NULL
            ||!matrix_valid(view_to_world))return 0;
    chr=runtime_live_chr(runtime,guard_index);
    if(chr==NULL||(prop=chr->prop)==NULL||(model=chr->model)==NULL)return 0;
    /* chrGetOnscreenRenderBounds selects matrix 1 plus one quarter of the
     * vector back to matrix 0.  Retain that exact animated offset, but anchor
     * it to the character's current canonical PropRecord position.  The
     * render matrices are view-space publications and can be from an older
     * camera frame while a probe is turning; transforming their absolute
     * translation through the current camera inverse makes a moving target
     * drift away from its live collision prop. */
    if(runtime->slots[guard_index].state.matrices_ready
            &&model->obj!=NULL&&model->render_pos!=NULL
            &&model->obj->numMatrices>=2){
        for(axis=0U;axis<3U;++axis)
            view_offset[axis]=(model->render_pos[1].pos.m[3][axis]
                -model->render_pos[0].pos.m[3][axis])*0.75f;
        if(!isfinite(view_offset[0])||!isfinite(view_offset[1])
                ||!isfinite(view_offset[2]))return 0;
        for(axis=0U;axis<3U;++axis)
            world_position[axis]=prop->pos.f[axis]
                +view_offset[0]*view_to_world[0][axis]
                +view_offset[1]*view_to_world[1][axis]
                +view_offset[2]*view_to_world[2][axis];
    }else{
        /* Offscreen actors intentionally have no renderer matrices.  Use the
         * same canonical character height and 0.75 aim fraction to turn the
         * diagnostic controller back toward the live prop; once visible, the
         * animated matrix offset above takes ownership again. */
        world_position[0]=prop->pos.x;
        world_position[1]=prop->pos.y+chr->chrheight*0.75f;
        world_position[2]=prop->pos.z;
    }
    return isfinite(world_position[0])&&isfinite(world_position[1])
        &&isfinite(world_position[2]);
}

int ge_original_stage_guard_runtime_actor(
    GeOriginalStageGuardRuntime *runtime,size_t guard_index,
    void **prop_record,void **chr_record)
{
    ChrRecord *chr;
    if(runtime==NULL||guard_index>=runtime->count||prop_record==NULL
            ||chr_record==NULL)return 0;
    chr=runtime_live_chr(runtime,guard_index);
    if(chr==NULL){*prop_record=NULL;*chr_record=NULL;return 0;}
    *prop_record=runtime->props[guard_index];
    *chr_record=chr;
    return 1;
}

int ge_original_stage_guard_runtime_firecount(
    const GeOriginalStageGuardRuntime *runtime,size_t guard_index,
    int32_t firecount[2])
{
    ChrRecord *chr;
    if(runtime==NULL||guard_index>=runtime->count||firecount==NULL)return 0;
    chr=runtime_live_chr(runtime,guard_index);
    if(chr==NULL)return 0;
    firecount[0]=chr->firecount[0];
    firecount[1]=chr->firecount[1];
    return 1;
}

void *ge_original_stage_guard_runtime_stan(
    const GeOriginalStageGuardRuntime *runtime,size_t guard_index)
{
    return runtime_live_chr(runtime,guard_index)!=NULL
        ?runtime->props[guard_index]->stan:NULL;
}

static GeOriginalStageGuardSlot *runtime_guard_for_chr(
    GeOriginalStageGuardRuntime *runtime,int32_t chr_id)
{
    size_t index;
    for(index=0U;index<runtime->count;++index)
        if(runtime->slots[index].state.chr_id==chr_id)
            return &runtime->slots[index];
    return NULL;
}

static void runtime_release_weapon_candidates(
    GeOriginalPitemModelProvider *models,GeOriginalStageGuardWeaponSlot *slots,
    size_t count)
{
    size_t index;
    if(slots==NULL)return;
    for(index=0U;index<count;++index)
        if(slots[index].model!=NULL)
            (void)ge_original_pitem_model_release_instance(
                models,slots[index].model);
    free(slots);
}

GeOriginalStageGuardRuntimeStatus
ge_original_stage_guard_runtime_bind_authored_weapons(
    GeOriginalStageGuardRuntime *runtime,
    const GeOriginalStageSetupRuntime *setup,
    GeOriginalPitemModelProvider *models,
    int (*load_projectile_models)(void *context,int32_t weapon_id),
    void *projectile_context,GeOriginalStageGuardWeaponBindReport *report)
{
    GeOriginalStageGuardWeaponSlot *candidates=NULL;
    GeOriginalStageGuardWeaponBindReport local={0};
    size_t index,candidate_count=0U,candidate_capacity=0U;
    GeOriginalStageGuardRuntimeStatus status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK;
    local.failed_command_index=SIZE_MAX;
    if(runtime==NULL||setup==NULL||!setup->loaded||models==NULL
            ||load_projectile_models==NULL)
        return GE_ORIGINAL_STAGE_GUARD_RUNTIME_INVALID_ARGUMENT;
    if(runtime->weapon_models!=NULL&&runtime->weapon_models!=models)
        return GE_ORIGINAL_STAGE_GUARD_RUNTIME_WEAPON_ABI_UNAVAILABLE;
    if(runtime->weapons!=NULL)
        return GE_ORIGINAL_STAGE_GUARD_RUNTIME_WEAPON_ABI_UNAVAILABLE;
    for(index=0U;index<setup->prop_record_count;++index){
        const GeOriginalStagePropRecord *record=&setup->prop_records[index];
        if(record->type==PROPDEF_COLLECTABLE&&record->word_count==34U
                &&(record->words[2]&PROPFLAG_ASSIGNEDTOCHR)!=0U){
            ++local.authored_assigned_collectables;
            if(runtime_guard_for_chr(runtime,record->pad_id)!=NULL)
                ++candidate_capacity;
            else ++local.owner_not_present;
        }
    }
    if(candidate_capacity!=0U){
        candidates=calloc(candidate_capacity,sizeof(*candidates));
        if(candidates==NULL){status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_CAPACITY_EXHAUSTED;goto fail;}
    }
    for(index=0U;index<setup->prop_record_count;++index){
        GeOriginalStagePropConstructionRequest request;
        const GeOriginalStagePropRecord *record=&setup->prop_records[index];
        GeOriginalStageGuardSlot *owner;GeOriginalStageGuardWeaponSlot *slot;
        ObjectRecord *object;uint16_t extrascale;int hand,switch_index;
        void *header=NULL,*model=NULL;float pitem_scale=0.0f;
        size_t previous;
        if(record->type!=PROPDEF_COLLECTABLE||record->word_count!=34U
                ||(record->words[2]&PROPFLAG_ASSIGNEDTOCHR)==0U)continue;
        owner=runtime_guard_for_chr(runtime,record->pad_id);
        if(owner==NULL)continue;
        local.failed_command_index=index;local.failed_model_id=record->model_id;
        local.failed_owner_chr_id=record->pad_id;
        hand=(record->words[2]&PROPFLAG_WEAPON_LEFTHANDED)!=0U
            ?GUNLEFT:GUNRIGHT;
        switch_index=hand==GUNLEFT?5:3;
        if(owner->state.chr_record==NULL||owner->pair.model_instance==NULL
                ||((Model *)owner->pair.model_instance)->obj==NULL
                ||((Model *)owner->pair.model_instance)->obj->numSwitches<=switch_index
                ||((Model *)owner->pair.model_instance)->obj->Switches[switch_index]==NULL){
            local.failed_branch=1U;
            status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_WEAPON_ABI_UNAVAILABLE;goto fail;
        }
        for(previous=0U;previous<candidate_count;++previous)
            if(candidates[previous].owner==owner
                    &&candidates[previous].state.hand==(uint8_t)hand){
                local.failed_branch=2U;
                status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_WEAPON_ABI_UNAVAILABLE;goto fail;
            }
        if(!ge_original_stage_prop_construction_request(setup,index,&request)
                ||request.service!=GE_ORIGINAL_STAGE_PROP_SERVICE_ITEM
                ||!ge_original_stage_prop_native_definition_init(
                    &request,&candidates[candidate_count].weapon,
                    sizeof(candidates[candidate_count].weapon))){
            local.failed_branch=3U;
            status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_WEAPON_ABI_UNAVAILABLE;goto fail;
        }
        slot=&candidates[candidate_count];object=(ObjectRecord *)(void *)&slot->weapon;
        if(!load_projectile_models(projectile_context,slot->weapon.weaponnum)
                ||!ge_original_pitem_model_resolve_instance(models,
                    record->model_id,&header,&model,&pitem_scale)
                ||header==NULL||model==NULL){
            local.failed_branch=4U;
            status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_WEAPON_UNAVAILABLE;goto fail;
        }
        slot->header=header;slot->model=model;slot->owner=owner;
        slot->render_positions=((Model *)model)->render_pos;
        slot->prop=runtime_allocate_prop(runtime,&slot->fallback_prop);
        if(slot->prop==NULL){
            local.failed_branch=5U;
            status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_WEAPON_UNAVAILABLE;goto fail;
        }
        /* From this point the candidate owns the provider instance, including
         * an exact objInit failure path. */
        ++candidate_count;
        object->prop=slot->prop;
        if(ge_original_objInitPreallocatedSlice(object,slot->header,slot->prop,
                slot->model,pitem_scale,NULL)==NULL){
            local.failed_branch=6U;
            status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_WEAPON_UNAVAILABLE;goto fail;
        }
        slot->prop->type=PROP_TYPE_WEAPON;
        ge_original_stage_guard_actor_set_gunfire_visible(slot->prop,0);
        extrascale=(uint16_t)(record->words[0]>>16U);
        /* Exact modelSetScale body is a single scale-field assignment. */
        slot->model->scale=slot->model->scale*((f32)extrascale/256.0f);
        slot->state.command_index=index;slot->state.owner_chr_id=record->pad_id;
        slot->state.model_id=record->model_id;
        slot->state.weapon_id=slot->weapon.weaponnum;
        slot->state.hand=(uint8_t)hand;slot->state.model_instance=slot->model;
        slot->state.prop_record=slot->prop;slot->state.weapon_record=&slot->weapon;
    }
    local.failed_command_index=SIZE_MAX;local.failed_branch=0U;
    /* Every failure-prone provider/allocation/ABI check completed above.
     * These exact equip calls now publish only the validated graph. */
    for(index=0U;index<candidate_count;++index){
        GeOriginalStageGuardWeaponSlot *slot=&candidates[index];
        if(!ge_original_stage_guard_actor_equip_weapon(
                &slot->weapon,slot->owner->state.chr_record)){
            local.failed_command_index=slot->state.command_index;
            local.failed_model_id=slot->state.model_id;
            local.failed_owner_chr_id=slot->state.owner_chr_id;
            local.failed_branch=7U;
            status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_WEAPON_ABI_UNAVAILABLE;goto fail;
        }
    }
    runtime->weapon_models=models;runtime->weapons=candidates;
    runtime->weapon_count=candidate_count;local.attached=candidate_count;
    if(report!=NULL)*report=local;
    return runtime->last_status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK;
fail:
    runtime_release_weapon_candidates(models,candidates,candidate_count);
    if(report!=NULL)*report=local;
    return runtime->last_status=status;
}

size_t ge_original_stage_guard_runtime_weapon_count(
    const GeOriginalStageGuardRuntime *runtime)
{return runtime!=NULL?runtime->weapon_count:0U;}

int ge_original_stage_guard_runtime_weapon_snapshot(
    const GeOriginalStageGuardRuntime *runtime,size_t weapon_index,
    GeOriginalStageGuardWeaponSnapshot *snapshot)
{
    if(runtime==NULL||snapshot==NULL||weapon_index>=runtime->weapon_count)return 0;
    *snapshot=runtime->weapons[weapon_index].state;return 1;
}

size_t ge_original_stage_guard_runtime_muzzle_flash_count(
    const GeOriginalStageGuardRuntime *runtime)
{
    return runtime != NULL ? runtime->weapon_count : 0U;
}

int ge_original_stage_guard_runtime_muzzle_flash(
    const GeOriginalStageGuardRuntime *runtime, size_t weapon_index,
    GeOriginalGuardMuzzleFlashPublication *publication)
{
    const GeOriginalStageGuardWeaponSlot *weapon;
    GeOriginalPitemModelGunfire gunfire;
    uint32_t scale_random, angle_random;
    uint16_t phase;
    if(runtime==NULL||publication==NULL||runtime->weapon_models==NULL
            ||weapon_index>=runtime->weapon_count)return 0;
    weapon=&runtime->weapons[weapon_index];
    if(!weapon->state.matrices_ready||weapon->model==NULL
            ||weapon->model->obj==NULL||weapon->model->render_pos==NULL
            ||weapon->prop==NULL||weapon->owner==NULL
            ||weapon->prop->parent!=weapon->owner->state.prop_record
            ||!weapon->owner->state.visible
            ||!ge_original_pitem_model_instance_gunfire(
                runtime->weapon_models,weapon->model,0U,&gunfire)
            ||!gunfire.visible
            ||gunfire.matrix_index>=(uint16_t)weapon->model->obj->numMatrices)
        return 0;
    /* These are the two exact renderer-time random branches in dogfnegx. */
    scale_random=randomGetNext();angle_random=randomGetNext();
    phase=(uint16_t)((angle_random*UINT32_C(1024))&UINT32_C(0xffff));
    return ge_original_guard_muzzle_flash_build(&gunfire,
        weapon->model->render_pos[gunfire.matrix_index].pos.m,
        weapon->model->scale,scale_random,
        sins((uint16_t)(phase+UINT16_C(0x4000))),sins(phase),publication);
}

static void runtime_release_hat_candidates(
    GeOriginalPitemModelProvider *models,GeOriginalStageGuardHatSlot *slots,
    size_t count)
{
    size_t index;if(slots==NULL)return;
    for(index=0U;index<count;++index)if(slots[index].model!=NULL)
        (void)ge_original_pitem_model_release_instance(models,slots[index].model);
    free(slots);
}

GeOriginalStageGuardRuntimeStatus
ge_original_stage_guard_runtime_bind_authored_hats(
    GeOriginalStageGuardRuntime *runtime,
    const GeOriginalStageSetupRuntime *setup,
    GeOriginalPitemModelProvider *models,
    GeOriginalStageGuardHatBindReport *report)
{
    GeOriginalStageGuardHatSlot *candidates=NULL;
    GeOriginalStageGuardHatBindReport local={0};
    GeOriginalStageGuardRuntimeStatus status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK;
    size_t index,candidate_count=0U,candidate_capacity=0U;
    size_t dynamic_capacity=0U,allocation_capacity=0U;
    local.failed_command_index=SIZE_MAX;
    if(runtime==NULL||setup==NULL||!setup->loaded||models==NULL)
        return GE_ORIGINAL_STAGE_GUARD_RUNTIME_INVALID_ARGUMENT;
    if(runtime->hats!=NULL||(runtime->weapon_models!=NULL
            &&runtime->weapon_models!=models))
        return GE_ORIGINAL_STAGE_GUARD_RUNTIME_HAT_ABI_UNAVAILABLE;
    for(index=0U;index<setup->prop_record_count;++index){
        const GeOriginalStagePropRecord *record=&setup->prop_records[index];
        if(record->type==PROPDEF_HAT&&record->word_count==32U
                &&(record->words[2]&PROPFLAG_ASSIGNEDTOCHR)!=0U){
            ++local.authored_assigned_hats;
            if(runtime_guard_for_chr(runtime,record->pad_id)!=NULL)
                ++candidate_capacity;
            else ++local.owner_not_present;
        }
    }
    /* init_guards reserves authored_count + 10 character slots. Reserve the
     * corresponding remaining hat slots up front as well: HatRecord and its
     * PropRecord ownership pointers must stay stable for the stage lifetime,
     * so a later AI spawn must never realloc already attached records. */
    dynamic_capacity=runtime->capacity-runtime->count;
    if(candidate_capacity>SIZE_MAX-dynamic_capacity){
        status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_CAPACITY_EXHAUSTED;goto fail;
    }
    allocation_capacity=candidate_capacity+dynamic_capacity;
    if(allocation_capacity!=0U){
        candidates=calloc(allocation_capacity,sizeof(*candidates));
        if(candidates==NULL){
            status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_CAPACITY_EXHAUSTED;goto fail;
        }
    }
    for(index=0U;index<setup->prop_record_count;++index){
        const GeOriginalStagePropRecord *record=&setup->prop_records[index];
        GeOriginalStagePropConstructionRequest request;
        GeOriginalStageGuardSlot *owner;GeOriginalStageGuardHatSlot *slot;
        void *header=NULL,*model=NULL;float pitem_scale=0.0f;
        if(record->type!=PROPDEF_HAT||record->word_count!=32U
                ||(record->words[2]&PROPFLAG_ASSIGNEDTOCHR)==0U)continue;
        owner=runtime_guard_for_chr(runtime,record->pad_id);if(owner==NULL)continue;
        local.failed_command_index=index;local.failed_model_id=record->model_id;
        local.failed_owner_chr_id=record->pad_id;
        if(owner->state.chr_record==NULL||owner->pair.model_instance==NULL
                ||((Model *)owner->pair.model_instance)->obj==NULL
                ||((Model *)owner->pair.model_instance)->obj->numSwitches<=6
                ||((Model *)owner->pair.model_instance)->obj->Switches[6]==NULL
                ||((ChrRecord *)owner->state.chr_record)->handle_positiondata_hat
                    !=NULL){
            local.failed_branch=1U;
            status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_HAT_ABI_UNAVAILABLE;goto fail;
        }
        if(!ge_original_stage_prop_construction_request(setup,index,&request)
                ||request.service!=GE_ORIGINAL_STAGE_PROP_SERVICE_ITEM
                ||!ge_original_stage_prop_native_definition_init(
                    &request,&candidates[candidate_count].hat,
                    sizeof(candidates[candidate_count].hat))){
            local.failed_branch=2U;
            status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_HAT_ABI_UNAVAILABLE;goto fail;
        }
        if(!ge_original_pitem_model_resolve_instance(models,record->model_id,
                &header,&model,&pitem_scale)||header==NULL||model==NULL){
            local.failed_branch=3U;
            status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_HAT_UNAVAILABLE;goto fail;
        }
        slot=&candidates[candidate_count];slot->header=header;slot->model=model;
        slot->render_positions=((Model *)model)->render_pos;
        slot->owner=owner;
        if((((ObjectRecord *)(void *)&slot->hat)->flags&0x100U)!=0U){
            local.failed_branch=4U;
            status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_HAT_ABI_UNAVAILABLE;goto fail;
        }
        slot->state.command_index=index;slot->state.owner_chr_id=record->pad_id;
        slot->state.model_id=record->model_id;
        slot->prop=runtime_allocate_prop(runtime,&slot->fallback_prop);
        if(slot->prop==NULL){
            local.failed_branch=5U;
            status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_HAT_UNAVAILABLE;goto fail;
        }
        slot->state.model_instance=slot->model;slot->state.prop_record=slot->prop;
        slot->state.hat_record=&slot->hat;
        slot->pitem_scale=pitem_scale;
        ++candidate_count;
    }
    for(index=0U;index<candidate_count;++index){
        GeOriginalStageGuardHatSlot *slot=&candidates[index];
        if(ge_original_stage_guard_actor_apply_hat(
                &slot->hat,(ChrRecord *)slot->owner->state.chr_record,
                slot->header,slot->prop,slot->model,slot->pitem_scale)==NULL){
            local.failed_command_index=slot->state.command_index;
            local.failed_model_id=slot->state.model_id;
            local.failed_owner_chr_id=slot->state.owner_chr_id;
            local.failed_branch=5U;
            status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_HAT_UNAVAILABLE;goto fail;
        }
    }
    local.failed_command_index=SIZE_MAX;local.failed_branch=0U;
    runtime->weapon_models=models;runtime->hats=candidates;
    runtime->hat_count=candidate_count;runtime->hat_capacity=allocation_capacity;
    local.attached=candidate_count;
    if(report!=NULL)*report=local;
    return runtime->last_status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK;
fail:
    runtime_release_hat_candidates(models,candidates,candidate_count);
    if(report!=NULL)*report=local;
    return runtime->last_status=status;
}

size_t ge_original_stage_guard_runtime_hat_count(
    const GeOriginalStageGuardRuntime *runtime)
{return runtime!=NULL?runtime->hat_count:0U;}

int ge_original_stage_guard_runtime_hat_snapshot(
    const GeOriginalStageGuardRuntime *runtime,size_t hat_index,
    GeOriginalStageGuardHatSnapshot *snapshot)
{
    if(runtime==NULL||snapshot==NULL||hat_index>=runtime->hat_count)return 0;
    *snapshot=runtime->hats[hat_index].state;return 1;
}

PropRecord *hatCreateForChr(ChrRecord *chr, s32 modelnum, u32 flags)
{
    GeOriginalStageGuardRuntime *runtime=ge_active_stage_guard_runtime;
    GeOriginalStageGuardSlot *owner;
    GeOriginalStageGuardHatSlot *slot;
    void *header=NULL,*model=NULL;
    float pitem_scale=0.0f;
    HatRecord tmp;
    if(runtime==NULL||chr==NULL||runtime->weapon_models==NULL
            ||(owner=runtime_guard_for_chr(runtime,chr->chrnum))==NULL)
        return NULL;
    memset(&tmp,0,sizeof(tmp));
    tmp.extrascale=0x0100;tmp.type=PROPDEF_HAT;
    tmp.flags=PROPFLAG_ASSIGNEDTOCHR;tmp.damage=1000.0f;
    tmp.mtx.m[0][0]=tmp.mtx.m[1][1]=1.0f;
    tmp.mtx.m[2][2]=tmp.mtx.m[3][3]=1.0f;
    tmp.shadecol.r=tmp.shadecol.g=tmp.shadecol.b=0xff;
    tmp.nextcol.r=tmp.nextcol.g=tmp.nextcol.b=0xff;
    if(!ge_original_pitem_model_resolve_instance(runtime->weapon_models,
            modelnum,&header,&model,&pitem_scale)
            ||header==NULL||model==NULL)return NULL;
    if(runtime->hat_count>=runtime->hat_capacity){
        (void)ge_original_pitem_model_release_instance(
            runtime->weapon_models,model);
        runtime->last_status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_CAPACITY_EXHAUSTED;
        return NULL;
    }
    slot=&runtime->hats[runtime->hat_count];memset(slot,0,sizeof(*slot));
    slot->header=header;slot->model=model;slot->owner=owner;
    slot->render_positions=((Model *)model)->render_pos;
    slot->prop=runtime_allocate_prop(runtime,&slot->fallback_prop);
    if(slot->prop==NULL)goto fail;
    slot->hat=tmp;slot->hat.obj=modelnum;
    slot->hat.flags=flags|PROPFLAG_ASSIGNEDTOCHR;
    slot->hat.pad=chr->chrnum;
    slot->state.command_index=SIZE_MAX;
    slot->state.owner_chr_id=chr->chrnum;
    slot->state.model_id=modelnum;
    slot->state.model_instance=model;
    slot->state.prop_record=slot->prop;
    slot->state.hat_record=&slot->hat;
    slot->pitem_scale=pitem_scale;
    if(ge_original_stage_guard_actor_apply_hat(&slot->hat,chr,header,
            slot->prop,model,pitem_scale)==NULL)goto fail;
    ++runtime->hat_count;
    runtime->last_status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK;
    return slot->prop;
fail:
    if(slot->prop!=NULL)chrpropFree(slot->prop);
    (void)ge_original_pitem_model_release_instance(
        runtime->weapon_models,model);
    memset(slot,0,sizeof(*slot));
    runtime->last_status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_HAT_UNAVAILABLE;
    return NULL;
}

size_t ge_original_stage_guard_runtime_active_prop_count(
    const GeOriginalStageGuardRuntime *runtime)
{return runtime!=NULL?runtime->count+runtime->weapon_count+runtime->hat_count:0U;}

int ge_original_stage_guard_runtime_active_prop(
    GeOriginalStageGuardRuntime *runtime,size_t active_index,
    size_t *command_index,void **prop_record)
{
    size_t rank,index;size_t selected_command=SIZE_MAX;void *selected=NULL;
    if(runtime==NULL||command_index==NULL||prop_record==NULL
            ||active_index>=runtime->count+runtime->weapon_count
                +runtime->hat_count)return 0;
    for(rank=0U;rank<=active_index;++rank){
        size_t best_command=SIZE_MAX;void *best=NULL;
        for(index=0U;index<runtime->count;++index){
            size_t command=runtime->slots[index].state.command_index;
            if((rank==0U||command>selected_command)&&command<best_command){
                best_command=command;best=runtime->props[index];
            }
        }
        for(index=0U;index<runtime->weapon_count;++index){
            size_t command=runtime->weapons[index].state.command_index;
            if((rank==0U||command>selected_command)&&command<best_command){
                best_command=command;best=runtime->weapons[index].prop;
            }
        }
        for(index=0U;index<runtime->hat_count;++index){
            size_t command=runtime->hats[index].state.command_index;
            if((rank==0U||command>selected_command)&&command<best_command){
                best_command=command;best=runtime->hats[index].prop;
            }
        }
        if(best==NULL)return 0;
        selected_command=best_command;selected=best;
    }
    *command_index=selected_command;*prop_record=selected;return 1;
}

size_t ge_original_stage_guard_runtime_root_prop_count(
    const GeOriginalStageGuardRuntime *runtime)
{return runtime!=NULL?runtime->count:0U;}

int ge_original_stage_guard_runtime_root_prop(
    GeOriginalStageGuardRuntime *runtime,size_t root_index,
    size_t *command_index,void **prop_record)
{
    size_t rank,index;size_t selected_command=SIZE_MAX;void *selected=NULL;
    if(runtime==NULL||command_index==NULL||prop_record==NULL
            ||root_index>=runtime->count)return 0;
    for(rank=0U;rank<=root_index;++rank){
        size_t best_command=SIZE_MAX;void *best=NULL;
        for(index=0U;index<runtime->count;++index){
            size_t command=runtime->slots[index].state.command_index;
            if((rank==0U||command>selected_command)&&command<best_command){
                best_command=command;best=runtime->props[index];
            }
        }
        if(best==NULL)return 0;
        selected_command=best_command;selected=best;
    }
    *command_index=selected_command;*prop_record=selected;return 1;
}

int ge_original_stage_guard_runtime_set_visibility(
    GeOriginalStageGuardRuntime *runtime,size_t guard_index,
    int visible,uint8_t room_id)
{
    if(runtime==NULL||guard_index>=runtime->count)return 0;
    if(runtime_live_chr(runtime,guard_index)==NULL)visible=0;
    runtime->slots[guard_index].state.visible=visible!=0;
    runtime->slots[guard_index].state.room_id=room_id;return 1;
}

static void runtime_clear_attachment_matrices(
    GeOriginalStageGuardRuntime *runtime, GeOriginalStageGuardSlot *slot)
{
    size_t index;
    for(index=0U;index<runtime->weapon_count;++index)
        if(runtime->weapons[index].owner==slot)
            runtime->weapons[index].state.matrices_ready=0U;
    for(index=0U;index<runtime->hat_count;++index)
        if(runtime->hats[index].owner==slot)
            runtime->hats[index].state.matrices_ready=0U;
}

GeOriginalStageGuardRuntimeStatus
ge_original_stage_guard_runtime_update_matrices(
    GeOriginalStageGuardRuntime *runtime,const float world_to_view[4][4])
{
    size_t index;
    if(runtime==NULL||!matrix_valid(world_to_view))
        return GE_ORIGINAL_STAGE_GUARD_RUNTIME_INVALID_ARGUMENT;
    memcpy(runtime->draw_world_to_view,world_to_view,
        sizeof(runtime->draw_world_to_view));
    runtime->draw_camera_ready=1U;
    for(index=0;index<runtime->count;++index){
        GeOriginalStageGuardSlot *slot=&runtime->slots[index];
        Model *model=slot->pair.model_instance;
        ModelRenderData renderdata;Mtxf base;size_t matrix;int retained;
        ChrRecord *chr=runtime_live_chr(runtime,index);
        if(chr==NULL||chr->model!=model||model==NULL||model->obj==NULL){
            slot->state.visible=0U;
            slot->state.matrices_ready=0U;
            runtime_clear_attachment_matrices(runtime,slot);
            continue;
        }
        if(!slot->state.visible
                ||!runtime_room_resident(runtime,slot->state.room_id)){
            slot->state.matrices_ready=0U;
            runtime_clear_attachment_matrices(runtime,slot);
            continue;
        }
        /* Matrix publication is renderer-only.  The unchanged chr/prop tick
         * has already advanced every actor before this boundary, while the
         * scene builder later rejects this same conservative bounding sphere.
         * Reject it here as well so guards wholly outside the live camera do
         * not run subcalcmatrices for their body, weapon and hat only to be
         * discarded immediately afterwards.  Invalid camera parameters make
         * runtime_guard_draw_visible return visible, preserving the former
         * conservative behaviour during bootstrap and host fixtures. */
        if(!runtime_guard_draw_visible(runtime,index)){
            slot->state.matrices_ready=0U;
            runtime_clear_attachment_matrices(runtime,slot);
            continue;
        }
        if(model->render_pos==NULL||slot->render_positions==NULL
                ||slot->pair.matrix_count==0U)
            return runtime->last_status=
                GE_ORIGINAL_STAGE_GUARD_RUNTIME_MATRIX_UNAVAILABLE;
        if(!ge_original_character_model_prepare_instance_relations(
                runtime->models,model))
            return runtime->last_status=
                GE_ORIGINAL_STAGE_GUARD_RUNTIME_MATRIX_UNAVAILABLE;
        retained=runtime_retain_matrices(model,slot->render_positions,
                                         slot->pair.matrix_count);
        if(retained<0)return runtime->last_status=
            GE_ORIGINAL_STAGE_GUARD_RUNTIME_MATRIX_UNAVAILABLE;
        if(retained==0){
            memcpy(base.m,world_to_view,sizeof(base.m));
            memset(&renderdata,0,sizeof(renderdata));renderdata.basemtx=&base;
            renderdata.mtxlist=&model->render_pos[0].pos;
            subcalcmatrices(&renderdata,model);
        }
        for(matrix=0;matrix<slot->pair.matrix_count;++matrix)
            if(!matrix_valid(model->render_pos[matrix].pos.m))
                return runtime->last_status=
                    GE_ORIGINAL_STAGE_GUARD_RUNTIME_MATRIX_UNAVAILABLE;
        slot->state.matrices_ready=1U;
        for(size_t weapon_index=0U;weapon_index<runtime->weapon_count;
                ++weapon_index){
            GeOriginalStageGuardWeaponSlot *weapon=&runtime->weapons[weapon_index];
            Mtxf *attachment;Mtxf left_rotation;size_t weapon_matrix;
            int weapon_retained;
            if(weapon->owner!=slot)continue;
            /* chrDropItem/chrpropDetach make a dead guard's weapon a root
             * prop.  It no longer has a character attachment matrix and must
             * leave this attached-model publication path immediately. */
            if(weapon->prop==NULL
                    ||weapon->prop->parent!=runtime->props[index]){
                weapon->state.matrices_ready=0U;
                continue;
            }
            if(weapon->model==NULL||weapon->render_positions==NULL
                    ||weapon->model->obj==NULL
                    ||weapon->model->render_pos==NULL
                    ||weapon->model->obj->numMatrices<=0)
                return runtime->last_status=
                    GE_ORIGINAL_STAGE_GUARD_RUNTIME_MATRIX_UNAVAILABLE;
            weapon_retained=runtime_retain_matrices(weapon->model,
                weapon->render_positions,
                (size_t)weapon->model->obj->numMatrices);
            if(weapon_retained<0)return runtime->last_status=
                GE_ORIGINAL_STAGE_GUARD_RUNTIME_MATRIX_UNAVAILABLE;
            if(weapon_retained==0){
                memset(&renderdata,0,sizeof(renderdata));
                attachment=modelFindNodeMtx(model,
                    weapon->model->attachedto_objinst,0);
                if(attachment==NULL)return runtime->last_status=
                    GE_ORIGINAL_STAGE_GUARD_RUNTIME_MATRIX_UNAVAILABLE;
                renderdata.basemtx=attachment;
                if(weapon->state.hand==GUNLEFT){
                    matrix_4x4_set_rotation_around_z(M_PI_F,&left_rotation);
                    matrix_4x4_multiply_in_place(attachment,&left_rotation);
                    renderdata.basemtx=&left_rotation;
                }
                renderdata.mtxlist=&weapon->model->render_pos[0].pos;
                subcalcmatrices(&renderdata,weapon->model);
            }
            for(weapon_matrix=0U;
                    weapon_matrix<(size_t)weapon->model->obj->numMatrices;
                    ++weapon_matrix)
                if(!matrix_valid(weapon->model->render_pos[weapon_matrix].pos.m))
                    return runtime->last_status=
                        GE_ORIGINAL_STAGE_GUARD_RUNTIME_MATRIX_UNAVAILABLE;
            weapon->state.matrices_ready=1U;
        }
        for(size_t hat_index=0U;hat_index<runtime->hat_count;++hat_index){
            GeOriginalStageGuardHatSlot *hat=&runtime->hats[hat_index];
            Mtxf *attachment;size_t hat_matrix;int hat_retained;
            if(hat->owner!=slot)continue;
            if(hat->prop==NULL||hat->prop->parent!=runtime->props[index]){
                hat->state.matrices_ready=0U;
                continue;
            }
            if(hat->model==NULL||hat->render_positions==NULL
                    ||hat->model->obj==NULL
                    ||hat->model->render_pos==NULL
                    ||hat->model->obj->numMatrices<=0)
                return runtime->last_status=
                    GE_ORIGINAL_STAGE_GUARD_RUNTIME_MATRIX_UNAVAILABLE;
            hat_retained=runtime_retain_matrices(hat->model,
                hat->render_positions,(size_t)hat->model->obj->numMatrices);
            if(hat_retained<0)return runtime->last_status=
                GE_ORIGINAL_STAGE_GUARD_RUNTIME_MATRIX_UNAVAILABLE;
            if(hat_retained==0){
                attachment=modelFindNodeMtx(model,
                    hat->model->attachedto_objinst,0);
                if(attachment==NULL)return runtime->last_status=
                    GE_ORIGINAL_STAGE_GUARD_RUNTIME_MATRIX_UNAVAILABLE;
                memset(&renderdata,0,sizeof(renderdata));
                renderdata.basemtx=attachment;
                renderdata.mtxlist=&hat->model->render_pos[0].pos;
                subcalcmatrices(&renderdata,hat->model);
            }
            for(hat_matrix=0U;hat_matrix<(size_t)hat->model->obj->numMatrices;
                    ++hat_matrix)
                if(!matrix_valid(hat->model->render_pos[hat_matrix].pos.m))
                    return runtime->last_status=
                        GE_ORIGINAL_STAGE_GUARD_RUNTIME_MATRIX_UNAVAILABLE;
            hat->state.matrices_ready=1U;
        }
    }
    runtime->player_matrices_ready=0U;
    {
        Model *model;
        ModelRenderData renderdata;Mtxf base;size_t matrix;int retained;
        uint8_t room_id;
        model=runtime_live_player_body(runtime,&room_id);
        if(model!=NULL&&runtime_room_resident(runtime,room_id)){
            if(model->obj==NULL||model->render_pos==NULL
                    ||runtime->player_render_positions==NULL
                    ||runtime->player_pair.matrix_count==0U
                    ||!ge_original_character_model_prepare_instance_relations(
                        runtime->models,model))
                return runtime->last_status=
                    GE_ORIGINAL_STAGE_GUARD_RUNTIME_MATRIX_UNAVAILABLE;
            retained=runtime_retain_matrices(model,
                runtime->player_render_positions,
                runtime->player_pair.matrix_count);
            if(retained<0)return runtime->last_status=
                GE_ORIGINAL_STAGE_GUARD_RUNTIME_MATRIX_UNAVAILABLE;
            if(retained==0){
                memcpy(base.m,world_to_view,sizeof(base.m));
                memset(&renderdata,0,sizeof(renderdata));
                renderdata.basemtx=&base;
                renderdata.mtxlist=&model->render_pos[0].pos;
                subcalcmatrices(&renderdata,model);
            }
            for(matrix=0U;matrix<runtime->player_pair.matrix_count;++matrix)
                if(!matrix_valid(model->render_pos[matrix].pos.m))
                    return runtime->last_status=
                        GE_ORIGINAL_STAGE_GUARD_RUNTIME_MATRIX_UNAVAILABLE;
            runtime->player_matrices_ready=1U;
        }
    }
    runtime->player_weapon.state.matrices_ready=0U;
    if(runtime->player_matrices_ready){
        GeOriginalStageGuardWeaponSlot *weapon=
            runtime_live_player_weapon(runtime);
        if(weapon!=NULL){
            Model *body=runtime->player_pair.model_instance;
            ModelRenderData renderdata;Mtxf *attachment;Mtxf left_rotation;
            size_t weapon_matrix;int retained;
            if(weapon->model->obj==NULL||weapon->model->render_pos==NULL
                    ||weapon->render_positions==NULL
                    ||weapon->model->obj->numMatrices<=0)
                return runtime->last_status=
                    GE_ORIGINAL_STAGE_GUARD_RUNTIME_MATRIX_UNAVAILABLE;
            retained=runtime_retain_matrices(weapon->model,
                weapon->render_positions,
                (size_t)weapon->model->obj->numMatrices);
            if(retained<0)return runtime->last_status=
                GE_ORIGINAL_STAGE_GUARD_RUNTIME_MATRIX_UNAVAILABLE;
            if(retained==0){
                attachment=modelFindNodeMtx(body,
                    weapon->model->attachedto_objinst,0);
                if(attachment==NULL)return runtime->last_status=
                    GE_ORIGINAL_STAGE_GUARD_RUNTIME_MATRIX_UNAVAILABLE;
                memset(&renderdata,0,sizeof(renderdata));
                renderdata.basemtx=attachment;
                if(weapon->state.hand==GUNLEFT){
                    matrix_4x4_set_rotation_around_z(M_PI_F,&left_rotation);
                    matrix_4x4_multiply_in_place(attachment,&left_rotation);
                    renderdata.basemtx=&left_rotation;
                }
                renderdata.mtxlist=&weapon->model->render_pos[0].pos;
                subcalcmatrices(&renderdata,weapon->model);
            }
            for(weapon_matrix=0U;
                    weapon_matrix<(size_t)weapon->model->obj->numMatrices;
                    ++weapon_matrix)
                if(!matrix_valid(
                        weapon->model->render_pos[weapon_matrix].pos.m))
                    return runtime->last_status=
                        GE_ORIGINAL_STAGE_GUARD_RUNTIME_MATRIX_UNAVAILABLE;
            weapon->state.matrices_ready=1U;
        }
    }
    return runtime->last_status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK;
}

GeOriginalStageGuardRuntimeStatus
ge_original_stage_guard_runtime_update_lighting(
    GeOriginalStageGuardRuntime *runtime)
{
    size_t index;
    if(runtime==NULL)return GE_ORIGINAL_STAGE_GUARD_RUNTIME_INVALID_ARGUMENT;
    if(runtime->services.tile_rgb==NULL)
        return runtime->last_status=
            GE_ORIGINAL_STAGE_GUARD_RUNTIME_LIGHTING_UNAVAILABLE;
    ge_original_stage_guard_actor_set_lighting_service(
        runtime->services.tile_rgb,runtime->services.context);
    for(index=0U;index<runtime->count;++index){
        PropRecord *prop=runtime->props[index];
        ChrRecord *chr=runtime_live_chr(runtime,index);
        /* Exact death/removal may deregister a corpse's final STAN before its
         * stable authored slot is reclaimed. It no longer has tile lighting
         * to sample and retains its last shade while the canonical lifecycle
         * finishes. */
        if(chr==NULL||chr->model==NULL||prop->stan==NULL
                ||(prop->flags&PROPFLAG_ENABLED)==0U)continue;
        if(!ge_original_stage_guard_actor_sample_lighting(
                prop,chr->nextcol.rgba))
            return runtime->last_status=
                GE_ORIGINAL_STAGE_GUARD_RUNTIME_LIGHTING_UNAVAILABLE;
        ge_original_stage_guard_actor_step_lighting(
            chr->shadecol.rgba,chr->nextcol.rgba);
    }
    return runtime->last_status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK;
}

int ge_original_stage_guard_runtime_lighting_snapshot(
    const GeOriginalStageGuardRuntime *runtime,size_t guard_index,
    GeOriginalStageGuardLightingSnapshot *snapshot)
{
    ChrRecord *chr;
    if(runtime==NULL||snapshot==NULL||guard_index>=runtime->count)return 0;
    chr=runtime_live_chr(runtime,guard_index);if(chr==NULL)return 0;
    memcpy(snapshot->current_rgba,chr->shadecol.rgba,4U);
    memcpy(snapshot->target_rgba,chr->nextcol.rgba,4U);return 1;
}

size_t ge_original_stage_guard_runtime_shadow_count(
    const GeOriginalStageGuardRuntime *runtime,size_t guard_index)
{
    if(runtime==NULL||guard_index>=runtime->count
            ||runtime_live_chr(runtime,guard_index)==NULL
            ||!runtime->slots[guard_index].state.matrices_ready)return 0U;
    return ge_original_character_model_instance_shadow_count(
        runtime->models,runtime->slots[guard_index].pair.model_instance);
}

int ge_original_stage_guard_runtime_shadow(
    const GeOriginalStageGuardRuntime *runtime,size_t guard_index,
    size_t shadow_index,GeOriginalStageGuardShadowPublication *shadow)
{
    GeOriginalCharacterModelShadow authored;Model *model;ChrRecord *chr;
    float sizex,sizey,height,y;size_t vertex;
    static const int signs[4][2]={{-1,-1},{-1,1},{1,1},{1,-1}};
    if(runtime==NULL||shadow==NULL||guard_index>=runtime->count
            ||runtime_live_chr(runtime,guard_index)==NULL
            ||!runtime->slots[guard_index].state.matrices_ready)return 0;
    model=runtime->slots[guard_index].pair.model_instance;
    chr=runtime_live_chr(runtime,guard_index);
    if(model==NULL||chr==NULL||model->scale==0.0f
            ||!ge_original_character_model_instance_shadow(
                runtime->models,model,shadow_index,&authored))return 0;
    height=authored.height_above_ground;sizex=authored.size[0];
    sizey=authored.size[1];y=(2.0f-height)/model->scale;
    if(height<50.0f){sizex*=1.25f;sizey*=1.25f;}
    else if(height>300.0f){sizex=0.0f;sizey=0.0f;}
    else{sizex*=(300.0f-height)/200.0f;
        sizey*=(300.0f-height)/200.0f;}
    memset(shadow,0,sizeof(*shadow));
    for(vertex=0U;vertex<4U;++vertex){
        shadow->vertices[vertex][0]=(float)(s16)(authored.position[0]
            +(float)signs[vertex][0]*sizex);
        shadow->vertices[vertex][1]=(float)(s16)y;
        shadow->vertices[vertex][2]=(float)(s16)(authored.position[1]
            +(float)signs[vertex][1]*sizey);
    }
    shadow->matrix_index=authored.matrix_index;
    shadow->image_id=authored.image_id;
    shadow->image_width=authored.image_width;
    shadow->image_height=authored.image_height;
    shadow->opacity=(chr->chrflags&CHRFLAG_NO_SHADOW)!=0U?0U:0x50U;
    return 1;
}

static GeOriginalStageGuardRuntimeStatus build_scene_internal(
    GeOriginalStageGuardRuntime *runtime,const float view_to_world[4][4],
    const GeDamRoomSceneStorage *storage,GeOriginalStageGuardScene *scene)
{
    GeOriginalModelSceneInput *inputs=NULL;GeOriginalModelScene *queries=NULL;
    size_t input_count=0,guard_index,input_index=0,vertices=0,batches=0;
    size_t triangles=0,commands=0,resident=0,published=0;
    GeOriginalStageGuardRuntimeStatus status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK;
    if(runtime==NULL||scene==NULL||!matrix_valid(view_to_world))
        return GE_ORIGINAL_STAGE_GUARD_RUNTIME_INVALID_ARGUMENT;
    memset(scene,0,sizeof(*scene));scene->guard_count=runtime->count;
    for(guard_index=0;guard_index<runtime->count;++guard_index){
        GeOriginalStageGuardSlot *slot=&runtime->slots[guard_index];
        if(runtime_live_chr(runtime,guard_index)==NULL
                ||!slot->state.visible
                ||!runtime_room_resident(runtime,slot->state.room_id))continue;
        ++resident;
        if(!slot->state.matrices_ready)continue;
        ++published;
        input_count+=ge_original_character_model_instance_scene_part_count(
            runtime->models,slot->pair.model_instance);
        for(size_t weapon_index=0U;weapon_index<runtime->weapon_count;
                ++weapon_index){
            GeOriginalStageGuardWeaponSlot *weapon=&runtime->weapons[weapon_index];
            if(weapon->owner==slot&&weapon->state.matrices_ready)
                input_count+=ge_original_pitem_model_scene_part_count(
                    runtime->weapon_models,weapon->state.model_id);
        }
        for(size_t hat_index=0U;hat_index<runtime->hat_count;++hat_index){
            GeOriginalStageGuardHatSlot *hat=&runtime->hats[hat_index];
            if(hat->owner==slot&&hat->state.matrices_ready)
                input_count+=ge_original_pitem_model_scene_part_count(
                    runtime->weapon_models,hat->state.model_id);
        }
    }
    scene->resident_guard_count=resident;
    scene->published_guard_count=published;
    scene->culled_guard_count=resident-published;
    scene->input_count=input_count;
    if(input_count!=0U){
        inputs=calloc(input_count,sizeof(*inputs));
        queries=calloc(input_count,sizeof(*queries));
        if(inputs==NULL||queries==NULL){status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_CAPACITY_EXHAUSTED;goto done;}
    }
    for(guard_index=0;guard_index<runtime->count;++guard_index){
        GeOriginalStageGuardSlot *slot=&runtime->slots[guard_index];Model *model;
        size_t parts,part_index;
        if(runtime_live_chr(runtime,guard_index)==NULL
                ||!slot->state.visible||!slot->state.matrices_ready
                ||!runtime_room_resident(runtime,slot->state.room_id))continue;
        model=slot->pair.model_instance;
        parts=ge_original_character_model_instance_scene_part_count(
            runtime->models,model);
        for(part_index=0;part_index<parts;++part_index){
            GeOriginalCharacterModelScenePart part;GeOriginalModelSceneInput *input;
            GeOriginalModelSceneStatus scene_status;size_t row,column;
            if(input_index>=input_count||!ge_original_character_model_instance_scene_part(
                    runtime->models,model,part_index,&part)){
                status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_SCENE_ERROR;goto done;
            }
            input=&inputs[input_index];input->blob=part.blob;
            input->blob_size=part.blob_size;input->primary_offset=part.primary_offset;
            input->secondary_offset=part.secondary_offset;
            input->segment4_offset=part.segment4_offset;
            input->room_id=slot->state.room_id;
            input->world_zbuffer_enabled=1U;
            input->segment3_matrices=(const float (*)[4][4])(const void *)model->render_pos;
            input->segment3_matrix_count=slot->pair.matrix_count;
            for(row=0;row<4U;++row)for(column=0;column<4U;++column)
                input->matrix[row][column]=view_to_world[row][column];
            scene_status=ge_original_model_scene_build(input,NULL,&queries[input_index]);
            if(scene_status!=GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED
                    ||queries[input_index].required_vertex_count>SIZE_MAX-vertices
                    ||queries[input_index].required_batch_count>SIZE_MAX-batches
                    ||queries[input_index].triangle_count>SIZE_MAX-triangles
                    ||queries[input_index].commands_visited>SIZE_MAX-commands){
                status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_SCENE_ERROR;goto done;
            }
            vertices+=queries[input_index].required_vertex_count;
            batches+=queries[input_index].required_batch_count;
            triangles+=queries[input_index].triangle_count;
            commands+=queries[input_index].commands_visited;++input_index;
        }
        for(size_t weapon_index=0U;weapon_index<runtime->weapon_count;
                ++weapon_index){
            GeOriginalStageGuardWeaponSlot *weapon=&runtime->weapons[weapon_index];
            size_t weapon_parts,weapon_part_index;
            if(weapon->owner!=slot||!weapon->state.matrices_ready)continue;
            weapon_parts=ge_original_pitem_model_scene_part_count(
                runtime->weapon_models,weapon->state.model_id);
            for(weapon_part_index=0U;weapon_part_index<weapon_parts;
                    ++weapon_part_index){
                GeOriginalPitemModelScenePart part;
                GeOriginalModelSceneInput *input;
                GeOriginalModelSceneStatus scene_status;size_t row,column;
                if(input_index>=input_count
                        ||!ge_original_pitem_model_scene_part(
                            runtime->weapon_models,weapon->state.model_id,
                            weapon_part_index,&part)){
                    status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_SCENE_ERROR;goto done;
                }
                input=&inputs[input_index];input->blob=part.blob;
                input->blob_size=part.blob_size;
                input->primary_offset=part.primary_offset;
                input->secondary_offset=part.secondary_offset;
                input->segment4_offset=part.segment4_offset;
                input->room_id=slot->state.room_id;
                input->world_zbuffer_enabled=1U;
                input->segment3_matrices=(const float (*)[4][4])(const void *)
                    weapon->model->render_pos;
                input->segment3_matrix_count=(size_t)weapon->model->obj->numMatrices;
                for(row=0U;row<4U;++row)for(column=0U;column<4U;++column)
                    input->matrix[row][column]=view_to_world[row][column];
                scene_status=ge_original_model_scene_build(
                    input,NULL,&queries[input_index]);
                if(scene_status!=GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED
                        ||queries[input_index].required_vertex_count>SIZE_MAX-vertices
                        ||queries[input_index].required_batch_count>SIZE_MAX-batches
                        ||queries[input_index].triangle_count>SIZE_MAX-triangles
                        ||queries[input_index].commands_visited>SIZE_MAX-commands){
                    status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_SCENE_ERROR;goto done;
                }
                vertices+=queries[input_index].required_vertex_count;
                batches+=queries[input_index].required_batch_count;
                triangles+=queries[input_index].triangle_count;
                commands+=queries[input_index].commands_visited;++input_index;
            }
        }
        for(size_t hat_index=0U;hat_index<runtime->hat_count;++hat_index){
            GeOriginalStageGuardHatSlot *hat=&runtime->hats[hat_index];
            size_t hat_parts,hat_part_index;
            if(hat->owner!=slot||!hat->state.matrices_ready)continue;
            hat_parts=ge_original_pitem_model_scene_part_count(
                runtime->weapon_models,hat->state.model_id);
            for(hat_part_index=0U;hat_part_index<hat_parts;++hat_part_index){
                GeOriginalPitemModelScenePart part;
                GeOriginalModelSceneInput *input;
                GeOriginalModelSceneStatus scene_status;size_t row,column;
                if(input_index>=input_count
                        ||!ge_original_pitem_model_scene_part(
                            runtime->weapon_models,hat->state.model_id,
                            hat_part_index,&part)){
                    status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_SCENE_ERROR;goto done;
                }
                input=&inputs[input_index];input->blob=part.blob;
                input->blob_size=part.blob_size;
                input->primary_offset=part.primary_offset;
                input->secondary_offset=part.secondary_offset;
                input->segment4_offset=part.segment4_offset;
                input->room_id=slot->state.room_id;
                input->world_zbuffer_enabled=1U;
                input->segment3_matrices=(const float (*)[4][4])(const void *)
                    hat->model->render_pos;
                input->segment3_matrix_count=(size_t)hat->model->obj->numMatrices;
                for(row=0U;row<4U;++row)for(column=0U;column<4U;++column)
                    input->matrix[row][column]=view_to_world[row][column];
                scene_status=ge_original_model_scene_build(
                    input,NULL,&queries[input_index]);
                if(scene_status!=GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED
                        ||queries[input_index].required_vertex_count>SIZE_MAX-vertices
                        ||queries[input_index].required_batch_count>SIZE_MAX-batches
                        ||queries[input_index].triangle_count>SIZE_MAX-triangles
                        ||queries[input_index].commands_visited>SIZE_MAX-commands){
                    status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_SCENE_ERROR;goto done;
                }
                vertices+=queries[input_index].required_vertex_count;
                batches+=queries[input_index].required_batch_count;
                triangles+=queries[input_index].triangle_count;
                commands+=queries[input_index].commands_visited;++input_index;
            }
        }
    }
    if(input_index!=input_count){status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_SCENE_ERROR;goto done;}
    scene->required_vertex_count=vertices;scene->required_batch_count=batches;
    scene->triangle_count=triangles;scene->commands_visited=commands;
    if(storage==NULL||vertices>storage->vertex_capacity
            ||batches>storage->batch_capacity
            ||(vertices!=0U&&storage->vertices==NULL)
            ||(batches!=0U&&storage->batches==NULL)){
        status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_SCENE_CAPACITY_EXCEEDED;goto done;
    }
    vertices=0U;batches=0U;
    for(input_index=0;input_index<input_count;++input_index){
        GeDamRoomSceneStorage local={storage->vertices+vertices,
            queries[input_index].required_vertex_count,storage->batches+batches,
            queries[input_index].required_batch_count};
        GeOriginalModelScene built;size_t local_batch;
        if(ge_original_model_scene_build(&inputs[input_index],&local,&built)
                !=GE_ORIGINAL_MODEL_SCENE_OK){status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_SCENE_ERROR;goto done;}
        for(local_batch=0;local_batch<built.batch_count;++local_batch)
            storage->batches[batches+local_batch].first_vertex+=vertices;
        vertices+=built.vertex_count;batches+=built.batch_count;
    }
    scene->vertex_count=vertices;scene->batch_count=batches;
done:
    free(queries);free(inputs);scene->status=status;
    runtime->last_status=status;return status;
}

static GeOriginalStageGuardRuntimeStatus collect_scene_inputs(
    GeOriginalStageGuardRuntime *runtime,const float view_to_world[4][4],
    GeOriginalModelSceneInput **result,size_t *result_count,
    size_t *resident_count,size_t *published_count)
{
    GeOriginalModelSceneInput *inputs;
    GeOriginalCharacterModelScenePart *character_parts;
    size_t input_count=0U,input_index=0U,guard_index;
    size_t maximum_character_parts=0U;
    uint64_t allocation_events_before;
    if(runtime==NULL||result==NULL||result_count==NULL
            ||resident_count==NULL||published_count==NULL
            ||!matrix_valid(view_to_world))
        return GE_ORIGINAL_STAGE_GUARD_RUNTIME_INVALID_ARGUMENT;
    *result=NULL;*result_count=0U;*resident_count=0U;*published_count=0U;
    allocation_events_before=runtime->scene_scratch_allocation_events;
    runtime->scene_collect_calls++;
    for(guard_index=0U;guard_index<runtime->count;++guard_index){
        GeOriginalStageGuardSlot *slot=&runtime->slots[guard_index];
        size_t attachment;
        if(runtime_live_chr(runtime,guard_index)==NULL
                ||!slot->state.visible
                ||!runtime_room_resident(runtime,slot->state.room_id))continue;
        ++*resident_count;
        if(!slot->state.matrices_ready)continue;
        ++*published_count;
        {
            const size_t parts =
                ge_original_character_model_instance_scene_part_count(
                    runtime->models,slot->pair.model_instance);
            input_count += parts;
            if (parts > maximum_character_parts)
                maximum_character_parts = parts;
        }
        for(attachment=0U;attachment<runtime->weapon_count;++attachment){
            GeOriginalStageGuardWeaponSlot *weapon=&runtime->weapons[attachment];
            if(weapon->owner==slot&&weapon->state.matrices_ready)
                input_count+=ge_original_pitem_model_scene_part_count(
                    runtime->weapon_models,weapon->state.model_id);
        }
        for(attachment=0U;attachment<runtime->hat_count;++attachment){
            GeOriginalStageGuardHatSlot *hat=&runtime->hats[attachment];
            if(hat->owner==slot&&hat->state.matrices_ready)
                input_count+=ge_original_pitem_model_scene_part_count(
                    runtime->weapon_models,hat->state.model_id);
        }
    }
    {
        uint8_t room_id;Model *model=runtime_live_player_body(runtime,&room_id);
        if(model!=NULL&&runtime->player_matrices_ready
                &&runtime_room_resident(runtime,room_id)){
            const size_t parts=
                ge_original_character_model_instance_scene_part_count(
                    runtime->models,model);
            input_count+=parts;
            if(parts>maximum_character_parts)maximum_character_parts=parts;
            {
                GeOriginalStageGuardWeaponSlot *weapon=
                    runtime_live_player_weapon(runtime);
                if(weapon!=NULL&&weapon->state.matrices_ready)
                    input_count+=
                        ge_original_pitem_model_instance_scene_part_count(
                            runtime->weapon_models,weapon->model);
            }
        }
    }
    if(input_count>runtime->scene_input_capacity){
        GeOriginalModelSceneInput *candidate;
        if(input_count>SIZE_MAX/sizeof(*candidate))
            return GE_ORIGINAL_STAGE_GUARD_RUNTIME_CAPACITY_EXHAUSTED;
        candidate=realloc(runtime->scene_inputs,input_count*sizeof(*candidate));
        if(candidate==NULL)
            return GE_ORIGINAL_STAGE_GUARD_RUNTIME_CAPACITY_EXHAUSTED;
        runtime->scene_inputs=candidate;
        runtime->scene_input_capacity=input_count;
        runtime->scene_scratch_allocation_events++;
    }
    if(maximum_character_parts>runtime->scene_character_part_capacity){
        GeOriginalCharacterModelScenePart *candidate;
        if(maximum_character_parts>SIZE_MAX/sizeof(*candidate))
            return GE_ORIGINAL_STAGE_GUARD_RUNTIME_CAPACITY_EXHAUSTED;
        candidate=realloc(runtime->scene_character_parts,
            maximum_character_parts*sizeof(*candidate));
        if(candidate==NULL)
            return GE_ORIGINAL_STAGE_GUARD_RUNTIME_CAPACITY_EXHAUSTED;
        runtime->scene_character_parts=candidate;
        runtime->scene_character_part_capacity=maximum_character_parts;
        runtime->scene_scratch_allocation_events++;
    }
    inputs=runtime->scene_inputs;
    character_parts=runtime->scene_character_parts;
    if(input_count!=0U)memset(inputs,0,input_count*sizeof(*inputs));
    for(guard_index=0U;guard_index<runtime->count;++guard_index){
        GeOriginalStageGuardSlot *slot=&runtime->slots[guard_index];Model *model;
        size_t parts,part_index,attachment;
        if(runtime_live_chr(runtime,guard_index)==NULL
                ||!slot->state.visible||!slot->state.matrices_ready
                ||!runtime_room_resident(runtime,slot->state.room_id))continue;
        model=slot->pair.model_instance;
        if(!ge_original_character_model_instance_scene_parts(
                runtime->models,model,character_parts,
                maximum_character_parts,&parts))goto fail;
        for(part_index=0U;part_index<parts;++part_index){
            const GeOriginalCharacterModelScenePart *part=
                &character_parts[part_index];
            GeOriginalModelSceneInput *input;
            size_t row,column;
            if(input_index>=input_count)goto fail;
            input=&inputs[input_index++];input->blob=part->blob;
            input->blob_size=part->blob_size;
            input->primary_offset=part->primary_offset;
            input->secondary_offset=part->secondary_offset;
            input->segment4_offset=part->segment4_offset;
            input->room_id=slot->state.room_id;
            input->world_zbuffer_enabled=1U;
            input->segment3_matrices=(const float (*)[4][4])(const void *)
                model->render_pos;
            input->segment3_matrix_count=slot->pair.matrix_count;
            for(row=0U;row<4U;++row)for(column=0U;column<4U;++column)
                input->matrix[row][column]=view_to_world[row][column];
        }
        for(attachment=0U;attachment<runtime->weapon_count;++attachment){
            GeOriginalStageGuardWeaponSlot *weapon=&runtime->weapons[attachment];
            size_t weapon_parts,weapon_part;
            if(weapon->owner!=slot||!weapon->state.matrices_ready)continue;
            weapon_parts=ge_original_pitem_model_scene_part_count(
                runtime->weapon_models,weapon->state.model_id);
            for(weapon_part=0U;weapon_part<weapon_parts;++weapon_part){
                GeOriginalPitemModelScenePart part;GeOriginalModelSceneInput *input;
                size_t row,column;
                if(input_index>=input_count||!ge_original_pitem_model_scene_part(
                        runtime->weapon_models,weapon->state.model_id,
                        weapon_part,&part))goto fail;
                input=&inputs[input_index++];input->blob=part.blob;
                input->blob_size=part.blob_size;
                input->primary_offset=part.primary_offset;
                input->secondary_offset=part.secondary_offset;
                input->segment4_offset=part.segment4_offset;
                input->room_id=slot->state.room_id;
                input->world_zbuffer_enabled=1U;
                input->segment3_matrices=(const float (*)[4][4])(const void *)
                    weapon->model->render_pos;
                input->segment3_matrix_count=(size_t)weapon->model->obj->numMatrices;
                for(row=0U;row<4U;++row)for(column=0U;column<4U;++column)
                    input->matrix[row][column]=view_to_world[row][column];
            }
        }
        for(attachment=0U;attachment<runtime->hat_count;++attachment){
            GeOriginalStageGuardHatSlot *hat=&runtime->hats[attachment];
            size_t hat_parts,hat_part;
            if(hat->owner!=slot||!hat->state.matrices_ready)continue;
            hat_parts=ge_original_pitem_model_scene_part_count(
                runtime->weapon_models,hat->state.model_id);
            for(hat_part=0U;hat_part<hat_parts;++hat_part){
                GeOriginalPitemModelScenePart part;GeOriginalModelSceneInput *input;
                size_t row,column;
                if(input_index>=input_count||!ge_original_pitem_model_scene_part(
                        runtime->weapon_models,hat->state.model_id,
                        hat_part,&part))goto fail;
                input=&inputs[input_index++];input->blob=part.blob;
                input->blob_size=part.blob_size;
                input->primary_offset=part.primary_offset;
                input->secondary_offset=part.secondary_offset;
                input->segment4_offset=part.segment4_offset;
                input->room_id=slot->state.room_id;
                input->world_zbuffer_enabled=1U;
                input->segment3_matrices=(const float (*)[4][4])(const void *)
                    hat->model->render_pos;
                input->segment3_matrix_count=(size_t)hat->model->obj->numMatrices;
                for(row=0U;row<4U;++row)for(column=0U;column<4U;++column)
                    input->matrix[row][column]=view_to_world[row][column];
            }
        }
    }
    {
        uint8_t room_id;Model *model=runtime_live_player_body(runtime,&room_id);
        size_t parts,part_index;
        if(model!=NULL&&runtime->player_matrices_ready
                &&runtime_room_resident(runtime,room_id)){
            if(!ge_original_character_model_instance_scene_parts(
                    runtime->models,model,character_parts,
                    maximum_character_parts,&parts))goto fail;
            for(part_index=0U;part_index<parts;++part_index){
                const GeOriginalCharacterModelScenePart *part=
                    &character_parts[part_index];
                GeOriginalModelSceneInput *input;size_t row,column;
                if(input_index>=input_count)goto fail;
                input=&inputs[input_index++];input->blob=part->blob;
                input->blob_size=part->blob_size;
                input->primary_offset=part->primary_offset;
                input->secondary_offset=part->secondary_offset;
                input->segment4_offset=part->segment4_offset;
                input->room_id=room_id;input->world_zbuffer_enabled=1U;
                input->segment3_matrices=
                    (const float (*)[4][4])(const void *)model->render_pos;
                input->segment3_matrix_count=runtime->player_pair.matrix_count;
                for(row=0U;row<4U;++row)for(column=0U;column<4U;++column)
                    input->matrix[row][column]=view_to_world[row][column];
            }
            {
                GeOriginalStageGuardWeaponSlot *weapon=
                    runtime_live_player_weapon(runtime);
                size_t weapon_parts,weapon_part;
                if(weapon!=NULL&&weapon->state.matrices_ready){
                    weapon_parts=
                        ge_original_pitem_model_instance_scene_part_count(
                            runtime->weapon_models,weapon->model);
                    for(weapon_part=0U;weapon_part<weapon_parts;
                            ++weapon_part){
                        GeOriginalPitemModelScenePart part;
                        GeOriginalModelSceneInput *input;size_t row,column;
                        if(input_index>=input_count
                                ||!ge_original_pitem_model_instance_scene_part(
                                    runtime->weapon_models,weapon->model,
                                    weapon_part,&part))goto fail;
                        input=&inputs[input_index++];input->blob=part.blob;
                        input->blob_size=part.blob_size;
                        input->primary_offset=part.primary_offset;
                        input->secondary_offset=part.secondary_offset;
                        input->segment4_offset=part.segment4_offset;
                        input->room_id=room_id;
                        input->world_zbuffer_enabled=1U;
                        input->segment3_matrices=
                            (const float (*)[4][4])(const void *)
                                weapon->model->render_pos;
                        input->segment3_matrix_count=
                            (size_t)weapon->model->obj->numMatrices;
                        for(row=0U;row<4U;++row)
                            for(column=0U;column<4U;++column)
                                input->matrix[row][column]=
                                    view_to_world[row][column];
                    }
                }
            }
        }
    }
    if(input_index!=input_count)goto fail;
    if(runtime->scene_scratch_allocation_events==allocation_events_before)
        runtime->scene_allocation_free_collect_calls++;
    *result=inputs;*result_count=input_count;
    return GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK;
fail:
    return GE_ORIGINAL_STAGE_GUARD_RUNTIME_SCENE_ERROR;
}

GeOriginalStageGuardRuntimeStatus
ge_original_stage_guard_runtime_build_scene_cached(
    GeOriginalStageGuardRuntime *runtime,GeOriginalModelSceneCache *cache,
    const float view_to_world[4][4],const GeDamRoomSceneStorage *storage,
    GeOriginalStageGuardScene *scene)
{
    GeOriginalModelSceneInput *inputs=NULL;GeOriginalModelScene built={0};
    GeOriginalStageGuardRuntimeStatus status;GeOriginalModelSceneStatus model_status;
    size_t input_count=0U,resident=0U,published=0U;
    if(runtime==NULL||cache==NULL||scene==NULL){
        if(scene!=NULL){memset(scene,0,sizeof(*scene));
            scene->status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_INVALID_ARGUMENT;}
        return GE_ORIGINAL_STAGE_GUARD_RUNTIME_INVALID_ARGUMENT;
    }
    memset(scene,0,sizeof(*scene));scene->guard_count=runtime->count;
    status=collect_scene_inputs(runtime,view_to_world,&inputs,&input_count,
        &resident,&published);
    if(status!=GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK)goto done;
    model_status=ge_original_model_scene_cache_build(
        cache,inputs,input_count,storage,&built);
    scene->resident_guard_count=resident;
    scene->published_guard_count=published;
    scene->culled_guard_count=resident-published;
    scene->input_count=input_count;
    scene->vertex_count=built.vertex_count;scene->batch_count=built.batch_count;
    scene->triangle_count=built.triangle_count;
    scene->commands_visited=built.commands_visited;
    scene->required_vertex_count=built.required_vertex_count;
    scene->required_batch_count=built.required_batch_count;
    if(model_status==GE_ORIGINAL_MODEL_SCENE_OK)
        status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK;
    else if(model_status==GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED)
        status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_SCENE_CAPACITY_EXCEEDED;
    else status=GE_ORIGINAL_STAGE_GUARD_RUNTIME_SCENE_ERROR;
done:
    scene->status=status;runtime->last_status=status;return status;
}

void ge_original_stage_guard_runtime_scene_scratch_stats(
    const GeOriginalStageGuardRuntime *runtime,
    GeOriginalStageGuardSceneScratchStats *stats)
{
    if(stats==NULL)return;
    memset(stats,0,sizeof(*stats));
    if(runtime==NULL)return;
    stats->input_capacity=runtime->scene_input_capacity;
    stats->character_part_capacity=runtime->scene_character_part_capacity;
    stats->collect_calls=runtime->scene_collect_calls;
    stats->allocation_events=runtime->scene_scratch_allocation_events;
    stats->allocation_free_collect_calls=
        runtime->scene_allocation_free_collect_calls;
}

GeOriginalStageGuardRuntimeStatus ge_original_stage_guard_runtime_build_scene(
    GeOriginalStageGuardRuntime *runtime,const float view_to_world[4][4],
    const GeDamRoomSceneStorage *storage,GeOriginalStageGuardScene *scene)
{return build_scene_internal(runtime,view_to_world,storage,scene);}

GeOriginalStageGuardRuntimeStatus ge_original_stage_guard_runtime_append_scene(
    GeOriginalStageGuardRuntime *runtime,const float view_to_world[4][4],
    const GeDamRoomSceneStorage *storage,size_t *vertex_cursor,
    size_t *batch_cursor,GeOriginalStageGuardScene *scene)
{
    GeDamRoomSceneStorage local;GeOriginalStageGuardScene built={0};
    size_t vertex_base,batch_base,index;
    GeOriginalStageGuardRuntimeStatus status;
    if(runtime==NULL||storage==NULL||vertex_cursor==NULL||batch_cursor==NULL
            ||scene==NULL||*vertex_cursor>storage->vertex_capacity
            ||*batch_cursor>storage->batch_capacity)
        return GE_ORIGINAL_STAGE_GUARD_RUNTIME_INVALID_ARGUMENT;
    vertex_base=*vertex_cursor;batch_base=*batch_cursor;
    local.vertices=storage->vertices!=NULL
        ?storage->vertices+vertex_base:NULL;
    local.vertex_capacity=storage->vertex_capacity-vertex_base;
    local.batches=storage->batches!=NULL
        ?storage->batches+batch_base:NULL;
    local.batch_capacity=storage->batch_capacity-batch_base;
    status=build_scene_internal(runtime,view_to_world,&local,&built);
    if(status!=GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK){
        *scene=built;return status;
    }
    for(index=0U;index<built.batch_count;++index)
        storage->batches[batch_base+index].first_vertex+=vertex_base;
    *vertex_cursor=vertex_base+built.vertex_count;
    *batch_cursor=batch_base+built.batch_count;
    *scene=built;return GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK;
}

GeOriginalStageGuardRuntimeStatus ge_original_stage_guard_runtime_last_status(
    const GeOriginalStageGuardRuntime *runtime)
{return runtime!=NULL?runtime->last_status:GE_ORIGINAL_STAGE_GUARD_RUNTIME_INVALID_ARGUMENT;}

const char *ge_original_stage_guard_runtime_status_name(
    GeOriginalStageGuardRuntimeStatus status)
{
    switch(status){
    case GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK:return "ok";
    case GE_ORIGINAL_STAGE_GUARD_RUNTIME_INVALID_ARGUMENT:return "invalid argument";
    case GE_ORIGINAL_STAGE_GUARD_RUNTIME_CAPACITY_EXHAUSTED:return "capacity exhausted";
    case GE_ORIGINAL_STAGE_GUARD_RUNTIME_PLACEMENT_UNAVAILABLE:return "authored placement unavailable";
    case GE_ORIGINAL_STAGE_GUARD_RUNTIME_HEAD_SELECTION_UNAVAILABLE:return "canonical head selection unavailable";
    case GE_ORIGINAL_STAGE_GUARD_RUNTIME_SUNGLASSES_SELECTION_UNAVAILABLE:return "canonical sunglasses selection unavailable";
    case GE_ORIGINAL_STAGE_GUARD_RUNTIME_MODEL_UNAVAILABLE:return "character model unavailable";
    case GE_ORIGINAL_STAGE_GUARD_RUNTIME_AI_LIST_UNAVAILABLE:return "authored AI list unavailable";
    case GE_ORIGINAL_STAGE_GUARD_RUNTIME_ACTOR_UNAVAILABLE:return "canonical actor construction unavailable";
    case GE_ORIGINAL_STAGE_GUARD_RUNTIME_WEAPON_UNAVAILABLE:return "authored weapon model unavailable";
    case GE_ORIGINAL_STAGE_GUARD_RUNTIME_WEAPON_ABI_UNAVAILABLE:return "canonical weapon attachment ABI unavailable";
    case GE_ORIGINAL_STAGE_GUARD_RUNTIME_HAT_UNAVAILABLE:return "authored hat model unavailable";
    case GE_ORIGINAL_STAGE_GUARD_RUNTIME_HAT_ABI_UNAVAILABLE:return "canonical hat attachment ABI unavailable";
    case GE_ORIGINAL_STAGE_GUARD_RUNTIME_LIGHTING_UNAVAILABLE:return "canonical character lighting unavailable";
    case GE_ORIGINAL_STAGE_GUARD_RUNTIME_MATRIX_UNAVAILABLE:return "canonical model matrices unavailable";
    case GE_ORIGINAL_STAGE_GUARD_RUNTIME_SCENE_ERROR:return "character scene error";
    case GE_ORIGINAL_STAGE_GUARD_RUNTIME_SCENE_CAPACITY_EXCEEDED:return "scene capacity exceeded";
    default:return "unknown";
    }
}
