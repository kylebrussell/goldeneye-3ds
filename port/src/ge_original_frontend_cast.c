#include "ge_original_frontend_cast.h"

#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>
#include "assets/obseg/text/LtitleE.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define GE_CAST_DURATION 180U
#define GE_CAST_RELOAD_TIMER 181U
#define GE_CAST_FADE 30U
#define GE_CAST_FADEOUT 151U
#define GE_CAST_DAMP 0.94999999f
#define GE_CAST_DAMP_COMP 0.050000012f

typedef struct GeOriginalFrontendCastCharacter {
    int32_t body;
    int32_t head;
    uint16_t text[3];
    int16_t reserved;
    int32_t flag;
} GeOriginalFrontendCastCharacter;

typedef struct GeOriginalFrontendCastAnimation {
    int32_t animation_id;
    float start_frame;
    float playback_speed;
    int32_t camera_preset;
    uint32_t record_offset;
} GeOriginalFrontendCastAnimation;

#include "ge_original_frontend_cast_contract.inc"

static size_t ge_cast_character_count(void)
{
    return sizeof(ge_cast_characters)/sizeof(ge_cast_characters[0]);
}

static size_t ge_cast_animation_count(void)
{
    return sizeof(ge_cast_animations)/sizeof(ge_cast_animations[0]);
}

static uint32_t ge_cast_random(GeOriginalFrontendCast *cast)
{
    return cast->services.random_next(cast->services.context);
}

static float ge_cast_random_unit(GeOriginalFrontendCast *cast)
{
    return (float)ge_cast_random(cast)*(1.0f/4294967295.0f);
}

static int ge_cast_character_locked(GeOriginalFrontendCast *cast,
                                    int32_t body)
{
    if(body==BODY_Moonraker_Elite_1_Male
            ||body==BODY_Moonraker_Elite_2_Female)
        return !cast->services.aztec_secret_or_00_complete(
            cast->services.context);
    if(body==BODY_Mayday||body==BODY_Jaws){
        if(cast->services.aztec_secret_or_00_complete(
                cast->services.context))return 0;
        return ge_cast_random(cast)%0x2710U!=0U;
    }
    if(body==BODY_Oddjob||body==BODY_Baron_Samedi){
        if(cast->services.egypt_00_complete(cast->services.context))return 0;
        return ge_cast_random(cast)%0x2710U!=0U;
    }
    return 0;
}

static GeOriginalFrontendCastEvent ge_cast_advance_character(
    GeOriginalFrontendCast *cast)
{
    size_t index=(size_t)(cast->selection.character_index+1);
    while(index<ge_cast_character_count()){
        const GeOriginalFrontendCastCharacter *character=
            &ge_cast_characters[index];
        if((character->flag!=0&&!cast->full_actor_intro)
                ||ge_cast_character_locked(cast,character->body)){
            ++index;
            continue;
        }
        cast->selection.character_index=(int32_t)index;
        cast->initialized=0U;
        return GE_ORIGINAL_FRONTEND_CAST_EVENT_RELOAD;
    }
    cast->selection.character_index=0;
    cast->initialized=0U;
    if(cast->full_actor_intro){
        cast->full_actor_intro=0U;
        return GE_ORIGINAL_FRONTEND_CAST_EVENT_MISSION_SELECT;
    }
    return GE_ORIGINAL_FRONTEND_CAST_EVENT_RAMROM;
}

GeOriginalFrontendCastStatus ge_original_frontend_cast_reset(
    GeOriginalFrontendCast *cast,
    const GeOriginalFrontendCastServices *services,
    int full_actor_intro)
{
    if(cast==NULL||services==NULL||services->random_next==NULL
            ||services->choose_random_head==NULL
            ||services->cradle_complete==NULL
            ||services->aztec_secret_or_00_complete==NULL
            ||services->egypt_00_complete==NULL)
        return GE_ORIGINAL_FRONTEND_CAST_INVALID_ARGUMENT;
    memset(cast,0,sizeof(*cast));
    cast->services=*services;
    cast->full_actor_intro=(uint8_t)(full_actor_intro!=0);
    cast->selection.character_index=cast->full_actor_intro?0:1;
    if(cast->full_actor_intro&&cast->services.play_intro_music!=NULL)
        cast->services.play_intro_music(cast->services.context);
    return ge_original_frontend_cast_begin_current(cast);
}

GeOriginalFrontendCastStatus ge_original_frontend_cast_begin_current(
    GeOriginalFrontendCast *cast)
{
    const GeOriginalFrontendCastCharacter *character;
    const GeOriginalFrontendCastAnimation *animation;
    int32_t body;
    int32_t head;
    int32_t weapon=-1;
    uint32_t animation_index;
    if(cast==NULL||cast->services.random_next==NULL
            ||cast->selection.character_index<0
            ||(size_t)cast->selection.character_index
                >=ge_cast_character_count())
        return GE_ORIGINAL_FRONTEND_CAST_INVALID_ARGUMENT;
    character=&ge_cast_characters[cast->selection.character_index];
    cast->selection.animation_flip=(uint8_t)(ge_cast_random(cast)&1U);
    animation_index=ge_cast_random(cast)%(uint32_t)ge_cast_animation_count();
    animation=&ge_cast_animations[animation_index];
    body=character->body;
    head=character->head;
    if(body==BODY_Special_Operations_Uniform){
        switch(ge_cast_random(cast)%5U){
        case 1:body=BODY_Formal_Wear;head=HEAD_Male_Brosnan_Default;break;
        case 2:body=BODY_Jungle_Fatigues;head=HEAD_Male_Brosnan_Default;break;
        case 3:body=BODY_Parka;head=HEAD_Male_Brosnan_Default;break;
        case 4:body=BODY_Brosnan_Tuxedo;head=HEAD_Male_Brosnan_Tuxedo;break;
        default:break;
        }
    }else if(body==BODY_Natalya_Skirt){
        if(ge_cast_random(cast)&1U)body=BODY_Natalya_Jungle_Fatigues;
    }else if(body==BODY_Trevelyan_006&&(ge_cast_random(cast)&1U))
        body=BODY_Trevelyan_Janus;
    if(head==(int32_t)HEAD_RANDOM&&!cast->services.choose_random_head(
            cast->services.context,body,&head))
        return GE_ORIGINAL_FRONTEND_CAST_HEAD_UNAVAILABLE;
    if(animation->camera_preset!=INTRO_WEAPON_TYPE_NONE){
        if(animation->camera_preset==INTRO_WEAPON_TYPE_RIFLE)
            weapon=ge_cast_rifles[ge_cast_random(cast)%6U];
        else weapon=ge_cast_pistols[ge_cast_random(cast)%10U];
        if(weapon==PROP_CHRRUGER
                &&!cast->services.cradle_complete(cast->services.context))
            weapon=PROP_CHRWPPK;
        if(weapon==PROP_CHRLASER
                &&!cast->services.aztec_secret_or_00_complete(
                    cast->services.context))weapon=PROP_CHRWPPK;
        if(weapon==PROP_CHRGOLDEN
                &&!cast->services.egypt_00_complete(cast->services.context))
            weapon=PROP_CHRWPPK;
    }
    cast->selection.body=body;
    cast->selection.head=head;
    cast->selection.weapon_prop=weapon;
    cast->selection.animation_id=animation->animation_id;
    cast->selection.animation_record_offset=animation->record_offset;
    cast->selection.animation_start_frame=animation->start_frame;
    cast->selection.animation_playback_speed=animation->playback_speed;
    cast->selection.animation_camera_preset=(uint8_t)animation->camera_preset;
    memcpy(cast->selection.text_id,character->text,sizeof(character->text));
    cast->camera_dist_start=ge_cast_random_unit(cast)*80.0f+70.0f;
    cast->camera_dist_end=ge_cast_random_unit(cast)*80.0f+70.0f;
    cast->camera_angle_start=(ge_cast_random_unit(cast)-0.5f)*M_TAU_F;
    cast->camera_angle_end=(ge_cast_random_unit(cast)-0.5f)*2.5132742f;
    cast->camera_height_start=ge_cast_random_unit(cast)*200.0f-100.0f;
    cast->camera_height_end=ge_cast_random_unit(cast)*200.0f-100.0f;
    cast->timer=0U;
    cast->camera_reset=1U;
    cast->pose_applied=0U;
    memset(cast->root_position_smoothed,0,
           sizeof(cast->root_position_smoothed));
    cast->initialized=1U;
    return GE_ORIGINAL_FRONTEND_CAST_OK;
}

GeOriginalFrontendCastStatus ge_original_frontend_cast_tick(
    GeOriginalFrontendCast *cast,int any_input,
    GeOriginalFrontendCastEvent *event)
{
    if(cast==NULL||event==NULL||!cast->initialized)
        return GE_ORIGINAL_FRONTEND_CAST_INVALID_ARGUMENT;
    *event=GE_ORIGINAL_FRONTEND_CAST_EVENT_NONE;
    if(cast->timer<UINT32_MAX)++cast->timer;
    if(cast->timer>=GE_CAST_RELOAD_TIMER){
        *event=ge_cast_advance_character(cast);
    }else if(any_input&&!cast->full_actor_intro){
        *event=GE_ORIGINAL_FRONTEND_CAST_EVENT_FILE_SELECT;
    }
    return GE_ORIGINAL_FRONTEND_CAST_OK;
}

GeOriginalFrontendCastStatus ge_original_frontend_cast_apply_pose(
    GeOriginalFrontendCast *cast,const float root_position[3],
    const float transformed_target[3],uint32_t clock_ticks,
    float timer_delta)
{
    float velocity[3];
    float target[3];
    uint32_t tick;
    size_t axis;
    if(cast==NULL||root_position==NULL||transformed_target==NULL
            ||!cast->initialized||clock_ticks==0U||timer_delta<=0.0f)
        return GE_ORIGINAL_FRONTEND_CAST_INVALID_ARGUMENT;
    if(cast->camera_reset)cast->root_position_smoothed[1]=root_position[1];
    for(axis=0U;axis<3U;++axis){
        velocity[axis]=(root_position[axis]
            -cast->root_position_smoothed[axis])/timer_delta;
        if(cast->camera_reset)
            cast->root_velocity_accumulator[axis]=
                velocity[axis]/GE_CAST_DAMP_COMP;
    }
    for(tick=0U;tick<clock_ticks;++tick)
        for(axis=0U;axis<3U;++axis)
            cast->root_velocity_accumulator[axis]=velocity[axis]
                +GE_CAST_DAMP*cast->root_velocity_accumulator[axis];
    for(axis=0U;axis<3U;++axis){
        cast->root_position_smoothed[axis]+=
            cast->root_velocity_accumulator[axis]
                *GE_CAST_DAMP_COMP*timer_delta;
        target[axis]=transformed_target[axis]
            -cast->root_position_smoothed[axis];
        if(cast->camera_reset)
            cast->target_accumulator[axis]=target[axis]/GE_CAST_DAMP_COMP;
    }
    for(tick=0U;tick<clock_ticks;++tick)
        for(axis=0U;axis<3U;++axis)
            cast->target_accumulator[axis]=target[axis]
                +GE_CAST_DAMP*cast->target_accumulator[axis];
    for(axis=0U;axis<3U;++axis)
        cast->target_smoothed[axis]=
            cast->target_accumulator[axis]*GE_CAST_DAMP_COMP;
    cast->camera_reset=0U;
    cast->pose_applied=1U;
    return GE_ORIGINAL_FRONTEND_CAST_OK;
}

GeOriginalFrontendCastStatus ge_original_frontend_cast_snapshot(
    const GeOriginalFrontendCast *cast,GeOriginalFrontendCastFrame *frame)
{
    float fraction;
    float distance;
    float angle;
    float height;
    float side_x;
    float side_z;
    size_t axis;
    if(cast==NULL||frame==NULL||!cast->initialized)
        return GE_ORIGINAL_FRONTEND_CAST_INVALID_ARGUMENT;
    memset(frame,0,sizeof(*frame));
    frame->selection=cast->selection;
    frame->timer=cast->timer;
    frame->duration_frames=GE_CAST_DURATION;
    frame->logical_width=440U;
    frame->logical_height=330U;
    frame->projection_fov_y_degrees=46.0f;
    frame->projection_aspect=320.0f/240.0f;
    frame->projection_near=10.0f;
    frame->projection_far=2000.0f;
    frame->model_scale=0.1f;
    frame->animation_translation_scale=0.1f;
    frame->animation_tick_speed=0.5f;
    frame->zbuffer_enabled=1U;
    frame->model_cull_both=1U;
    frame->model_lighting_enabled=1U;
    frame->model_texture_gen_enabled=1U;
    frame->reflection_camera_eye_z=4000.0f;
    frame->full_actor_intro=cast->full_actor_intro;
    frame->pose_applied=cast->pose_applied;
    if(cast->timer>=GE_CAST_DURATION)frame->fade=0.0f;
    else if(cast->timer<GE_CAST_FADE)
        frame->fade=(float)cast->timer/(float)GE_CAST_FADE;
    else if(cast->timer>=GE_CAST_FADEOUT)
        frame->fade=(float)(GE_CAST_DURATION-cast->timer)
            /(float)GE_CAST_FADE;
    else frame->fade=1.0f;
    fraction=(float)cast->timer/(float)GE_CAST_DURATION;
    distance=(cast->camera_dist_end-cast->camera_dist_start)*fraction
        +cast->camera_dist_start;
    angle=(cast->camera_angle_end-cast->camera_angle_start)*fraction
        +cast->camera_angle_start;
    height=(cast->camera_height_end-cast->camera_height_start)*fraction
        +cast->camera_height_start;
    if(angle<0.0f)angle+=M_TAU_F;
    side_x=cosf(angle)*0.2f*distance;
    side_z=-sinf(angle)*0.2f*distance;
    frame->camera_eye[0]=distance*sinf(angle)+side_x;
    frame->camera_eye[1]=height;
    frame->camera_eye[2]=distance*cosf(angle)+side_z;
    frame->camera_target[0]=side_x;
    frame->camera_target[2]=side_z;
    frame->camera_up[1]=1.0f;
    if(cast->pose_applied){
        for(axis=0U;axis<3U;++axis){
            frame->camera_eye[axis]+=cast->root_position_smoothed[axis];
            frame->camera_target[axis]+=cast->root_position_smoothed[axis]
                +cast->target_smoothed[axis];
        }
        frame->camera_eye[1]+=52.5f;
        frame->camera_target[1]-=10.0f;
    }
    return GE_ORIGINAL_FRONTEND_CAST_OK;
}

const char *ge_original_frontend_cast_contract_sha256(void)
{
    return GE_FRONTEND_CAST_CONTRACT_SHA256;
}
