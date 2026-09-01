#include "ge_original_character_appearance.h"

#include <ultra64.h>
#include <bondconstants.h>
#include "game/chrobjdata.h"
#include <random.h>

/* Canonical src/game/chr.c pools. Keep the sentinels: begin_stage derives the
 * exact reset_counter_rand_body_head counts instead of baking their sizes. */
static const s32 ge_random_male_heads[] = {
    HEAD_Male_Jim,HEAD_Male_Chris,HEAD_Male_Lee,HEAD_Male_Graeme,
    HEAD_Male_Steve_H,HEAD_Male_Neil,HEAD_Male_Robin,HEAD_Male_Des,
    HEAD_Male_Grant,HEAD_Male_Dave_Dr_Doak,HEAD_Male_Karl,HEAD_Male_Alan,
    HEAD_Male_Pete,HEAD_Male_Martin,HEAD_Male_Mark,HEAD_Male_Duncan,
    HEAD_Male_Shaun,HEAD_Male_Dwayne,HEAD_Male_B,HEAD_Male_Steve_Ellis,
    HEAD_Male_Joel,HEAD_Male_Scott,HEAD_Male_Joe_Altered,HEAD_Male_Ken,
    HEAD_Male_Joe,-1
};
static const s32 ge_random_female_heads[] = {
    HEAD_Female_Sally,HEAD_Female_Marion_Rosika,HEAD_Female_Mandy,
    HEAD_Female_Vivien,-1
};
static const s32 ge_random_bodies[] = {
    BODY_Jungle_Commando,BODY_St_Petersburg_Guard,BODY_Russian_Soldier,
    BODY_Russian_Infantry,BODY_Janus_Special_Forces,BODY_Brosnan_Tuxedo,
    BODY_Boris,BODY_Ourumov,BODY_Trevelyan_Janus,BODY_Valentin_,BODY_Xenia,
    BODY_Baron_Samedi,BODY_Jaws,BODY_Mayday,BODY_Oddjob,
    BODY_Natalya_Skirt,BODY_Janus_Marine,BODY_Russian_Commandant,
    BODY_Siberian_Guard_1_Mishkin,BODY_Naval_Officer,
    BODY_Siberian_Special_Forces,BODY_Special_Operations_Uniform,
    BODY_Formal_Wear,BODY_Jungle_Fatigues,BODY_Unused_Female,BODY_Rosika,
    BODY_Scientist_2_Female,BODY_Civilian_1_Female,BODY_Unused_Male_1,
    BODY_Unused_Male_2,BODY_Civilian_4,BODY_Civilian_2,BODY_Civilian_3,
    BODY_Scientist_1_Male,BODY_Brosnan_Tuxedo,BODY_Brosnan_Tuxedo,
    BODY_Brosnan_Tuxedo,BODY_Helicopter_Pilot,BODY_Siberian_Guard_2,
    BODY_Arctic_Commando,BODY_Moonraker_Elite_1_Male,
    BODY_Moonraker_Elite_2_Female,-1
};

static u32 ge_num_male_heads;
static u32 ge_num_female_heads;
static u32 ge_num_bodies;
static u32 ge_current_random_male_head;
static u32 ge_current_random_female_head;
static u32 ge_current_random_body;

void ge_original_character_appearance_begin_stage(void)
{
    ge_num_male_heads=0U;
    while(ge_random_male_heads[ge_num_male_heads]>=0)++ge_num_male_heads;
    ge_num_female_heads=0U;
    while(ge_random_female_heads[ge_num_female_heads]>=0)
        ++ge_num_female_heads;
    ge_num_bodies=0U;
    while(ge_random_bodies[ge_num_bodies]>=0)++ge_num_bodies;
    ge_current_random_male_head=randomGetNext()%ge_num_male_heads;
    ge_current_random_female_head=randomGetNext()%ge_num_female_heads;
    ge_current_random_body=randomGetNext()%ge_num_bodies;
}

int ge_original_character_appearance_choose_head(
    void *context,int32_t body_id,int32_t *head_id)
{
    s32 selected;
    (void)context;
    if(head_id==NULL||body_id<0||body_id>79
            ||c_item_entries[body_id].header==NULL
            ||ge_num_male_heads==0U||ge_num_female_heads==0U)return 0;
    if(c_item_entries[body_id].isMale){
        selected=(s32)(randomGetNext()&3U);
        selected=((s32)ge_current_random_male_head+selected)
            %(s32)ge_num_male_heads;
        *head_id=ge_random_male_heads[selected];
    }else{
        *head_id=ge_random_female_heads[ge_current_random_female_head];
    }
    return 1;
}

int ge_original_character_appearance_choose_sunglasses(
    void *context,uint16_t appearance_flags,int *sunglasses)
{
    (void)context;
    if(sunglasses==NULL||(appearance_flags&2U)==0U)return 0;
    *sunglasses=(randomGetNext()&1U)==0U;
    return 1;
}
