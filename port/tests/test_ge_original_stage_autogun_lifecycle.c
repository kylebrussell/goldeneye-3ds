#include "ge_original_stage_autogun_lifecycle.h"

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

s32 g_ClockTimer=1;
f32 g_GlobalTimerDelta=1.0f;

static s32 next_tick_operation;
static unsigned tick_calls;
static unsigned free_calls;
static int last_free_prop;
static int last_can_regenerate;
static GeOriginalStageSecurityStatus next_construct_status;
static GeOriginalStageSecurityInstance constructed_instance;

typedef struct ModelOwner {
    Model *expected;
    void *allocation;
    unsigned release_calls;
    int active;
    int fail_release;
} ModelOwner;

u32 randomGetNext(void)
{return UINT32_C(0x80000000);}

s32 objTick(PropRecord *prop)
{
    assert(prop!=NULL&&prop->obj!=NULL
           &&prop->obj->type==PROPDEF_AUTOGUN);
    ++tick_calls;
    return next_tick_operation;
}

void objFree(ObjectRecord *object,s32 free_prop,s32 can_regenerate)
{
    assert(object!=NULL&&object->type==PROPDEF_AUTOGUN);
    ++free_calls;last_free_prop=free_prop;
    last_can_regenerate=can_regenerate;
    if(!can_regenerate){
        if(object->model!=NULL)object->model->obj=NULL;
        object->prop=NULL;
    }
}

static int release_model(void *context,void *opaque_model)
{
    ModelOwner *owner=context;
    assert(owner!=NULL&&owner->expected==opaque_model&&owner->active);
    /* Exact objFree clears this header marker before the Pitem provider owns
     * the native render-position/RW allocations. */
    assert(owner->expected->obj==NULL);
    ++owner->release_calls;
    if(owner->fail_release)return 0;
    free(owner->allocation);owner->allocation=NULL;owner->active=0;
    return 1;
}

int ge_original_pitem_model_release_instance(
    GeOriginalPitemModelProvider *provider,void *opaque_model)
{
    return release_model(provider,opaque_model);
}

GeOriginalStageSecurityStatus ge_original_stage_security_construct(
    const GeOriginalStagePropConstructionRequest *request,
    void *definition,size_t definition_size,
    const GeOriginalStageSecurityProviders *providers,
    GeOriginalStageSecurityInstance *instance)
{
    assert(request!=NULL&&definition!=NULL&&definition_size>0U
           &&providers!=NULL&&instance!=NULL);
    if(next_construct_status==GE_ORIGINAL_STAGE_SECURITY_OK)
        *instance=constructed_instance;
    return next_construct_status;
}

static void bind_instance(GeOriginalStageSecurityInstance *instance,
                          AutogunRecord *autogun,PropRecord *prop,
                          Model *model,BeamRecord *beam)
{
    memset(instance,0,sizeof(*instance));
    memset(autogun,0,sizeof(*autogun));memset(prop,0,sizeof(*prop));
    memset(model,0,sizeof(*model));memset(beam,0,sizeof(*beam));
    autogun->type=PROPDEF_AUTOGUN;autogun->prop=prop;
    autogun->model=model;autogun->beam=(struct beam *)beam;
    prop->type=PROP_TYPE_OBJ;prop->obj=(ObjectRecord *)autogun;
    instance->definition=autogun;instance->prop=prop;
    instance->model=model;instance->beam=beam;
    instance->type=PROPDEF_AUTOGUN;
    instance->constructed=1U;instance->runtime_ready=1U;
}

int main(void)
{
    GeOriginalStageSecurityInstance instance;
    AutogunRecord autogun;PropRecord prop;Model model;BeamRecord beam;
    GeOriginalStagePropRecord record={0};
    GeOriginalStagePropConstructionRequest request={0};
    GeOriginalStageSecurityProviders providers={0};
    GeOriginalStageAutogunBeamSnapshot beam_snapshot;
    GeOriginalStageAutogunRuntimeSnapshot runtime_snapshot;
    GeOriginalStageAutogunCleanupProviders cleanup_providers={0};
    ModelOwner model_owner={0};
    int32_t operation=INT32_MIN;
    bind_instance(&instance,&autogun,&prop,&model,&beam);
    record.type=PROPDEF_AUTOGUN;request.record=&record;
    constructed_instance=instance;
    next_construct_status=GE_ORIGINAL_STAGE_SECURITY_OK;
    memset(&instance,0,sizeof(instance));
    assert(ge_original_stage_autogun_lifecycle_construct(
        &request,&autogun,sizeof(autogun),&providers,&instance)
        ==GE_ORIGINAL_STAGE_SECURITY_OK);
    assert(ge_original_stage_autogun_lifecycle_is_live(&instance));
    assert(strcmp(ge_original_stage_autogun_lifecycle_status_name(
        GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_OK),"ok")==0);
    autogun.unk90=1.25f;autogun.unk9C=-0.5f;
    autogun.unkB0=0.75f;autogun.unkD4=0.4f;
    autogun.unkAC=12;autogun.unkB8=90;autogun.unkBC=91;
    autogun.unkC0=93;autogun.is_active=1;
    autogun.unkC4=(void *)(uintptr_t)1U;autogun.unkC8=NULL;
    beam.unk00=-1;
    assert(ge_original_stage_autogun_lifecycle_runtime_snapshot(
        &instance,&runtime_snapshot));
    assert(runtime_snapshot.yaw==1.25f&&runtime_snapshot.pitch==-0.5f
           &&runtime_snapshot.barrel_spin_speed==0.75f
           &&runtime_snapshot.pending_damage==0.4f
           &&runtime_snapshot.shot_counter==12
           &&runtime_snapshot.last_tracking_tick==90
           &&runtime_snapshot.last_line_of_sight_tick==91
           &&runtime_snapshot.next_sound_tick==93
           &&runtime_snapshot.tracking_active
           &&runtime_snapshot.sound_slot_mask==1U
           &&!runtime_snapshot.beam_active);
    autogun.unkC4=NULL;autogun.is_active=0;

    next_tick_operation=TICKOP_NONE;
    assert(ge_original_stage_autogun_lifecycle_tick_exact(
        &instance,&operation)==GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_OK);
    assert(operation==TICKOP_NONE&&tick_calls==1U&&instance.runtime_ready);

    /* objTick's canonical retick operation restructures scheduling without
     * destroying the object; the active-list owner remains responsible for
     * executing it and the lifecycle must stay live. */
    next_tick_operation=TICKOP_RETICK;
    assert(ge_original_stage_autogun_lifecycle_tick_exact(
        &instance,&operation)==GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_OK);
    assert(operation==TICKOP_RETICK&&tick_calls==2U&&instance.runtime_ready);

    /* Exact low-clock beam step and expiry ordering. */
    beam.unk00=0;beam.unk20=10.0f;beam.unk1c=100.0f;beam.unk28=1.0f;
    g_ClockTimer=1;g_GlobalTimerDelta=0.5f;
    assert(ge_original_stage_autogun_lifecycle_advance_beam_exact(&instance)
        ==GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_OK);
    assert(beam.unk00==1&&fabsf(beam.unk28-6.0f)<0.0001f);
    beam.unk1c=6.0f;
    assert(ge_original_stage_autogun_lifecycle_advance_beam_exact(&instance)
        ==GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_OK);
    assert(beam.unk00==-1&&fabsf(beam.unk28-11.0f)<0.0001f);
    assert(ge_original_stage_autogun_lifecycle_advance_beam_exact(&instance)
        ==GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_OK
        &&beam.unk00==-1&&fabsf(beam.unk28-11.0f)<0.0001f);
    memset(&beam_snapshot,0xa5,sizeof(beam_snapshot));
    assert(ge_original_stage_autogun_lifecycle_beam_snapshot(
        &instance,&beam_snapshot));
    assert(!beam_snapshot.active&&beam_snapshot.age==-1
           &&beam_snapshot.origin[0]==0.0f
           &&beam_snapshot.direction[2]==0.0f
           &&beam_snapshot.maximum_distance==0.0f);
    beam.unk00=2;beam.unk20=4.0f;beam.unk1c=100.0f;beam.unk28=0.0f;
    g_ClockTimer=3;
    assert(ge_original_stage_autogun_lifecycle_advance_beam_exact(&instance)
        ==GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_OK);
    assert(beam.unk00==3&&fabsf(beam.unk28-9.0f)<0.0001f);
    beam.pos.x=1.0f;beam.pos.y=2.0f;beam.pos.z=3.0f;
    beam.delta.x=0.0f;beam.delta.y=0.5f;beam.delta.z=1.0f;
    beam.unk1c=900.0f;beam.unk20=12.0f;beam.unk24=40.0f;
    beam.unk28=8.0f;beam.item_id=ITEM_FNP90;
    assert(ge_original_stage_autogun_lifecycle_beam_snapshot(
        &instance,&beam_snapshot));
    assert(beam_snapshot.active&&beam_snapshot.age==3
           &&beam_snapshot.weapon_id==ITEM_FNP90
           &&beam_snapshot.origin[0]==1.0f
           &&beam_snapshot.origin[1]==2.0f
           &&beam_snapshot.origin[2]==3.0f
           &&beam_snapshot.direction[1]==0.5f
           &&beam_snapshot.direction[2]==1.0f
           &&beam_snapshot.maximum_distance==900.0f
           &&beam_snapshot.speed==12.0f
           &&beam_snapshot.minimum_distance==40.0f
           &&beam_snapshot.distance==8.0f);

    /* An autogun can leave objTick through the canonical REMOVE branch.  The
     * active-list owner must execute that returned free operation and must
     * not advance the now-dead beam a second time. */
    next_tick_operation=TICKOP_FREE;
    assert(ge_original_stage_autogun_lifecycle_tick_exact(
        &instance,&operation)==GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_REMOVED);
    assert(operation==TICKOP_FREE&&tick_calls==3U
           &&!instance.runtime_ready);
    assert(ge_original_stage_autogun_lifecycle_advance_beam_exact(&instance)
        ==GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_NOT_LIVE);

    bind_instance(&instance,&autogun,&prop,&model,&beam);
    assert(ge_original_stage_autogun_lifecycle_cleanup_exact(
        &instance,1,0)==GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_OK);
    assert(free_calls==1U&&last_free_prop==1&&last_can_regenerate==0
           &&!instance.runtime_ready&&instance.prop==NULL
           &&instance.model==NULL&&instance.beam==NULL);
    assert(!ge_original_stage_autogun_lifecycle_is_live(&instance));
    assert(!ge_original_stage_autogun_lifecycle_beam_snapshot(
        &instance,&beam_snapshot));
    assert(!ge_original_stage_autogun_lifecycle_runtime_snapshot(
        &instance,&runtime_snapshot));

    bind_instance(&instance,&autogun,&prop,&model,&beam);
    model.obj=(ModelFileHeader *)(uintptr_t)1U;
    model_owner.expected=&model;model_owner.allocation=malloc(64U);
    assert(model_owner.allocation!=NULL);model_owner.active=1;
    cleanup_providers.context=&model_owner;
    cleanup_providers.release_model=release_model;
    assert(ge_original_stage_autogun_lifecycle_cleanup_owned_exact(
        &instance,&cleanup_providers)
        ==GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_OK);
    assert(free_calls==2U&&last_free_prop==TRUE
           &&last_can_regenerate==FALSE
           &&model_owner.release_calls==1U&&!model_owner.active
           &&model_owner.allocation==NULL
           &&!instance.runtime_ready&&instance.prop==NULL
           &&instance.model==NULL&&instance.beam==NULL);
    assert(ge_original_stage_autogun_lifecycle_cleanup_owned_exact(
        &instance,&cleanup_providers)
        ==GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_NOT_LIVE);
    assert(free_calls==2U&&model_owner.release_calls==1U);

    /* The typed adapter must preserve the same ordering without casting the
     * Pitem provider function to the generic callback ABI. */
    bind_instance(&instance,&autogun,&prop,&model,&beam);
    model.obj=(ModelFileHeader *)(uintptr_t)1U;
    model_owner=(ModelOwner){&model,malloc(64U),0U,1,0};
    assert(model_owner.allocation!=NULL);
    assert(ge_original_stage_autogun_lifecycle_cleanup_pitem_exact(
        &instance,(GeOriginalPitemModelProvider *)&model_owner)
        ==GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_OK);
    assert(free_calls==3U&&model_owner.release_calls==1U
           &&!model_owner.active&&model_owner.allocation==NULL
           &&instance.model==NULL&&instance.prop==NULL
           &&instance.beam==NULL);

    /* A provider failure is reported after canonical prop/sound teardown,
     * while retaining the stable model address for stage-wide fallback. */
    bind_instance(&instance,&autogun,&prop,&model,&beam);
    model.obj=(ModelFileHeader *)(uintptr_t)1U;
    model_owner=(ModelOwner){&model,malloc(64U),0U,1,1};
    assert(model_owner.allocation!=NULL);
    assert(ge_original_stage_autogun_lifecycle_cleanup_owned_exact(
        &instance,&cleanup_providers)
        ==GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_MODEL_RELEASE_FAILED);
    assert(free_calls==4U&&model_owner.release_calls==1U
           &&model_owner.active&&instance.model==&model
           &&instance.prop==NULL&&instance.beam==NULL
           &&strcmp(ge_original_stage_autogun_lifecycle_status_name(
                GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_MODEL_RELEASE_FAILED),
                "model release failed")==0);
    free(model_owner.allocation);model_owner.allocation=NULL;
    model_owner.active=0;

    bind_instance(&instance,&autogun,&prop,&model,&beam);
    autogun.beam=NULL;
    assert(!ge_original_stage_autogun_lifecycle_is_live(&instance));
    assert(ge_original_stage_autogun_lifecycle_tick_exact(
        &instance,&operation)==GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_NOT_LIVE);
    assert(ge_original_stage_autogun_lifecycle_cleanup_exact(
        &instance,1,1)==GE_ORIGINAL_STAGE_AUTOGUN_LIFECYCLE_NOT_LIVE);
    constructed_instance=instance;
    next_construct_status=GE_ORIGINAL_STAGE_SECURITY_BEAM_ALLOCATION_FAILED;
    assert(ge_original_stage_autogun_lifecycle_construct(
        &request,&autogun,sizeof(autogun),&providers,&instance)
        ==GE_ORIGINAL_STAGE_SECURITY_BEAM_ALLOCATION_FAILED);
    record.type=PROPDEF_CCTV;
    assert(ge_original_stage_autogun_lifecycle_construct(
        &request,&autogun,sizeof(autogun),&providers,&instance)
        ==GE_ORIGINAL_STAGE_SECURITY_INVALID_DEFINITION);
    return 0;
}
