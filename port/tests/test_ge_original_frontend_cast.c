#include "ge_original_frontend_cast.h"

#include <bondconstants.h>
#include "assets/obseg/text/LtitleE.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct Harness {
    uint32_t random_values[32];
    size_t random_count;
    size_t random_index;
    int cradle;
    int aztec;
    int egypt;
    int music_calls;
    int head_calls;
} Harness;

static uint32_t next_random(void *context)
{
    Harness *h=context;
    assert(h->random_count!=0U);
    return h->random_values[(h->random_index++)%h->random_count];
}

static int choose_head(void *context,int32_t body,int32_t *head)
{
    Harness *h=context;
    assert(body>=0&&head!=NULL);
    ++h->head_calls;
    *head=HEAD_Male_Jim;
    return 1;
}

static int cradle(void *context){return ((Harness *)context)->cradle;}
static int aztec(void *context){return ((Harness *)context)->aztec;}
static int egypt(void *context){return ((Harness *)context)->egypt;}
static void music(void *context){++((Harness *)context)->music_calls;}

static GeOriginalFrontendCastServices services(Harness *h)
{
    GeOriginalFrontendCastServices value={0};
    value.context=h;
    value.random_next=next_random;
    value.choose_random_head=choose_head;
    value.cradle_complete=cradle;
    value.aztec_secret_or_00_complete=aztec;
    value.egypt_00_complete=egypt;
    value.play_intro_music=music;
    return value;
}

static int closef(float a,float b){return fabsf(a-b)<0.0002f;}

int main(void)
{
    Harness h={0};
    GeOriginalFrontendCast cast;
    GeOriginalFrontendCastFrame frame;
    GeOriginalFrontendCastEvent event;
    GeOriginalFrontendCastServices svc;
    uint32_t tick;
    const float root[3]={10.0f,20.0f,30.0f};
    const float transformed[3]={15.0f,60.0f,55.0f};
    assert(ge_original_frontend_cast_reset(NULL,NULL,0)
        ==GE_ORIGINAL_FRONTEND_CAST_INVALID_ARGUMENT);
    h.random_values[0]=1U;
    h.random_values[1]=0U;
    h.random_values[2]=1U;
    h.random_values[3]=0U;
    h.random_values[4]=UINT32_MAX;
    h.random_values[5]=0U;
    h.random_values[6]=UINT32_MAX;
    h.random_values[7]=0U;
    h.random_values[8]=UINT32_MAX;
    h.random_count=9U;
    svc=services(&h);
    assert(ge_original_frontend_cast_reset(&cast,&svc,0)
        ==GE_ORIGINAL_FRONTEND_CAST_OK);
    assert(ge_original_frontend_cast_snapshot(&cast,&frame)
        ==GE_ORIGINAL_FRONTEND_CAST_OK);
    assert(frame.selection.character_index==1);
    assert(frame.selection.body==BODY_Formal_Wear);
    assert(frame.selection.head==HEAD_Male_Brosnan_Default);
    assert(frame.selection.weapon_prop==-1);
    assert(frame.selection.animation_id==ANIM_spotting_bond);
    assert(frame.selection.animation_record_offset==0x5d10U);
    assert(frame.selection.animation_start_frame==98.0f);
    assert(frame.selection.animation_playback_speed==1.0f);
    assert(frame.selection.animation_flip==1U);
    assert(frame.selection.text_id[0]
        ==getStringID(LTITLE,TITLE_STR_229_STARRING));
    assert(frame.selection.text_id[1]
        ==getStringID(LTITLE,TITLE_STR_232_007));
    assert(frame.selection.text_id[2]
        ==getStringID(LTITLE,TITLE_STR_233_JAMESBOND));
    assert(frame.timer==0U&&frame.duration_frames==180U);
    assert(frame.logical_width==440U&&frame.logical_height==330U);
    assert(frame.projection_fov_y_degrees==46.0f);
    assert(closef(frame.projection_aspect,4.0f/3.0f));
    assert(frame.projection_near==10.0f&&frame.projection_far==2000.0f);
    assert(frame.model_scale==0.1f);
    assert(frame.animation_translation_scale==0.1f);
    assert(frame.animation_tick_speed==0.5f);
    assert(frame.zbuffer_enabled&&frame.model_cull_both);
    assert(frame.model_lighting_enabled&&frame.model_texture_gen_enabled);
    assert(frame.reflection_camera_eye_z==4000.0f);
    assert(frame.fade==0.0f&&!frame.pose_applied);
    assert(closef(frame.camera_eye[0],-14.0f));
    assert(closef(frame.camera_eye[1],-100.0f));
    assert(closef(frame.camera_eye[2],-70.0f));
    assert(closef(frame.camera_target[0],-14.0f));
    assert(closef(frame.camera_target[2],0.0f));
    assert(ge_original_frontend_cast_apply_pose(
        &cast,root,transformed,1U,1.0f)==GE_ORIGINAL_FRONTEND_CAST_OK);
    assert(ge_original_frontend_cast_snapshot(&cast,&frame)
        ==GE_ORIGINAL_FRONTEND_CAST_OK&&frame.pose_applied);
    assert(closef(frame.camera_eye[0],-4.0f));
    assert(closef(frame.camera_eye[1],-27.5f));
    assert(closef(frame.camera_eye[2],-40.0f));
    assert(closef(frame.camera_target[0],1.0f));
    assert(closef(frame.camera_target[1],50.0f));
    assert(closef(frame.camera_target[2],55.0f));
    for(tick=0U;tick<30U;++tick){
        assert(ge_original_frontend_cast_tick(&cast,0,&event)
            ==GE_ORIGINAL_FRONTEND_CAST_OK);
        assert(event==GE_ORIGINAL_FRONTEND_CAST_EVENT_NONE);
    }
    assert(ge_original_frontend_cast_snapshot(&cast,&frame)
        ==GE_ORIGINAL_FRONTEND_CAST_OK&&frame.fade==1.0f);
    for(;tick<151U;++tick)
        assert(ge_original_frontend_cast_tick(&cast,0,&event)
            ==GE_ORIGINAL_FRONTEND_CAST_OK);
    assert(ge_original_frontend_cast_snapshot(&cast,&frame)
        ==GE_ORIGINAL_FRONTEND_CAST_OK
        &&closef(frame.fade,29.0f/30.0f));
    for(;tick<180U;++tick)
        assert(ge_original_frontend_cast_tick(&cast,0,&event)
            ==GE_ORIGINAL_FRONTEND_CAST_OK);
    assert(ge_original_frontend_cast_snapshot(&cast,&frame)
        ==GE_ORIGINAL_FRONTEND_CAST_OK&&frame.fade==0.0f);
    assert(ge_original_frontend_cast_tick(&cast,0,&event)
        ==GE_ORIGINAL_FRONTEND_CAST_OK
        &&event==GE_ORIGINAL_FRONTEND_CAST_EVENT_RELOAD
        &&cast.selection.character_index==2&&!cast.initialized);
    assert(ge_original_frontend_cast_begin_current(&cast)
        ==GE_ORIGINAL_FRONTEND_CAST_OK);
    assert(ge_original_frontend_cast_tick(&cast,1,&event)
        ==GE_ORIGINAL_FRONTEND_CAST_OK
        &&event==GE_ORIGINAL_FRONTEND_CAST_EVENT_FILE_SELECT);

    memset(&h,0,sizeof(h));
    h.random_values[0]=0U;
    h.random_values[1]=1U;
    h.random_values[2]=0U;
    h.random_values[3]=7U;
    h.random_count=4U;
    svc=services(&h);
    assert(ge_original_frontend_cast_reset(&cast,&svc,0)
        ==GE_ORIGINAL_FRONTEND_CAST_OK);
    assert(cast.selection.animation_camera_preset==INTRO_WEAPON_TYPE_PISTOL);
    assert(cast.selection.weapon_prop==PROP_CHRWPPK);

    memset(&h,0,sizeof(h));
    h.random_values[0]=0U;
    h.random_count=1U;
    svc=services(&h);
    assert(ge_original_frontend_cast_reset(&cast,&svc,1)
        ==GE_ORIGINAL_FRONTEND_CAST_OK);
    assert(h.music_calls==1&&cast.selection.character_index==0
        &&cast.full_actor_intro);
    for(tick=0U;tick<181U;++tick)
        assert(ge_original_frontend_cast_tick(&cast,1,&event)
            ==GE_ORIGINAL_FRONTEND_CAST_OK);
    assert(event==GE_ORIGINAL_FRONTEND_CAST_EVENT_RELOAD
        &&cast.selection.character_index==1);
    assert(strlen(ge_original_frontend_cast_contract_sha256())==64U);
    puts("canonical frontend cast scheduler passed");
    return 0;
}
