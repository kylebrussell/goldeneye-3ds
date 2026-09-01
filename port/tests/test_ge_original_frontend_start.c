#include "ge_original_frontend_start.h"

#include <bondconstants.h>
#include "assets/obseg/text/LtitleE.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct Harness {
    int highest;
    int folder;
    int difficulty;
    int stage;
    int difficulty_calls;
    int stage_calls;
    int progress[4];
    int copy_calls;
    int erase_calls;
    int last_sfx;
    int last_music;
    int sfx_calls;
    int music_calls;
    int music_stop_calls;
    int summary_calls;
    int slider_calls;
    float sliders[4];
} Harness;

static int highest(void *context,int32_t mission)
{assert(mission>=SP_LEVEL_DAM&&mission<SP_LEVEL_MAX);
 return ((Harness *)context)->highest;}
static int folder(void *context,int32_t value)
{Harness *h=context;h->folder=value;return 1;}
static void difficulty(void *context,int32_t value)
{Harness *h=context;h->difficulty=value;++h->difficulty_calls;}
static void stage(void *context,int32_t value)
{Harness *h=context;h->stage=value;++h->stage_calls;}
static int has_progress(void *context,int32_t value)
{Harness *h=context;assert(value>=FOLDER1&&value<MAX_FOLDER_COUNT);
 return h->progress[value];}
static int summary(void *context,int32_t value,int32_t *mission,
                   int32_t *difficulty_value)
{Harness *h=context;assert(value>=FOLDER1&&value<MAX_FOLDER_COUNT);
 ++h->summary_calls;
 *mission=h->progress[value]?SP_LEVEL_DAM:SP_LEVEL_DAM-1;
 *difficulty_value=h->progress[value]?DIFFICULTY_AGENT:DIFFICULTY_MULTI;
 return h->progress[value];}
static int copy_folder(void *context,int32_t value)
{Harness *h=context;assert(value>=FOLDER1&&value<MAX_FOLDER_COUNT);
 ++h->copy_calls;return 1;}
static int erase_folder(void *context,int32_t value)
{Harness *h=context;assert(value>=FOLDER1&&value<MAX_FOLDER_COUNT);
 ++h->erase_calls;h->progress[value]=0;return 1;}
static void play_sfx(void *context,uint32_t value)
{Harness *h=context;h->last_sfx=(int)value;++h->sfx_calls;}
static void play_music(void *context,int32_t value)
{Harness *h=context;h->last_music=value;++h->music_calls;}
static void stop_music(void *context)
{Harness *h=context;++h->music_stop_calls;}
static void set_sliders(void *context,float reaction,float health,
                        float damage,float accuracy)
{Harness *h=context;++h->slider_calls;h->sliders[0]=reaction;
 h->sliders[1]=health;h->sliders[2]=damage;h->sliders[3]=accuracy;}

static void advance_to_menu(GeOriginalFrontendStart *frontend,
                            uint32_t input,int menu)
{
    GeOriginalFrontendSnapshot snapshot;
    int tick;
    for(tick=0;tick<8;++tick){
        assert(ge_original_frontend_start_tick(
            frontend,tick==0?input:0U));
        assert(ge_original_frontend_start_snapshot(frontend,&snapshot));
        if(snapshot.menu==menu)return;
    }
    assert(!"frontend transition did not reach expected menu");
}

static void press(GeOriginalFrontendStart *frontend,uint32_t input,int menu)
{advance_to_menu(frontend,input,menu);}

static void leave_first_title(GeOriginalFrontendStart *frontend)
{
    GeOriginalFrontendSnapshot snapshot;
    int frame;
    assert(ge_original_frontend_start_tick(
        frontend,GE_ORIGINAL_FRONTEND_INPUT_START));
    assert(ge_original_frontend_start_snapshot(frontend,&snapshot)
        &&snapshot.menu==MENU_GOLDENEYE_LOGO);
    for(frame=1;frame<=90;++frame)
        assert(ge_original_frontend_start_tick(frontend,0));
    assert(ge_original_frontend_start_tick(frontend,0));
    advance_to_menu(frontend,0U,MENU_FILE_SELECT);
    assert(ge_original_frontend_start_snapshot(frontend,&snapshot)
        &&snapshot.menu==MENU_FILE_SELECT);
}

int main(void)
{
    {
        GeOriginalFrontendWalletBounds bounds[4]={{0}};
        GeOriginalFrontendCursor cursor={0};
        assert(ge_original_frontend_wallet_bounds_from_top_screen(
            40.0f,0.0f,200.0f,120.0f,&bounds[0]));
        assert(fabsf(bounds[0].left)<0.0001f
            &&fabsf(bounds[0].top)<0.0001f
            &&fabsf(bounds[0].right-220.0f)<0.0001f
            &&fabsf(bounds[0].bottom-165.0f)<0.0001f);
        bounds[1]=(GeOriginalFrontendWalletBounds){220,0,440,165};
        bounds[2]=(GeOriginalFrontendWalletBounds){0,165,220,330};
        bounds[3]=(GeOriginalFrontendWalletBounds){220,165,440,330};
        cursor.x=330.0f;cursor.y=247.5f;
        assert(ge_original_frontend_cursor_wallet(&cursor,bounds)==3);
        cursor.x=500.0f;
        assert(ge_original_frontend_cursor_wallet(&cursor,bounds)==-1);
        assert(!ge_original_frontend_wallet_bounds_from_top_screen(
            2.0f,0.0f,1.0f,1.0f,&bounds[0]));
    }
    Harness harness={0};
    GeOriginalFrontendServices services={0};
    harness.highest=DIFFICULTY_00;harness.folder=FOLDER1;
    harness.difficulty=-1;harness.stage=LEVELID_NONE;
    harness.progress[FOLDER4]=1;
    services.context=&harness;
    services.highest_unlocked_difficulty=highest;
    services.select_folder=folder;
    services.set_selected_difficulty=difficulty;
    services.request_stage=stage;
    services.folder_has_progress=has_progress;
    services.folder_summary=summary;
    services.copy_folder_to_first_free=copy_folder;
    services.erase_folder=erase_folder;
    services.play_sfx=play_sfx;
    services.play_music=play_music;
    services.stop_music=stop_music;
    services.set_007_sliders=set_sliders;
    GeOriginalFrontendStart frontend;GeOriginalFrontendSnapshot snapshot;
    {
        GeOriginalFrontendStart startup;
        int frame;
        assert(ge_original_frontend_start_reset_canonical(
            &startup,&services));
        assert(harness.music_stop_calls==1);
        assert(ge_original_frontend_start_snapshot(&startup,&snapshot)
            &&snapshot.menu==MENU_LEGAL_SCREEN
            &&snapshot.line_count==12U
            &&snapshot.lines[0].text_id
                ==getStringID(LTITLE,TITLE_STR_07_TWY)
            &&snapshot.lines[11].text_id
                ==getStringID(LTITLE,TITLE_STR_18_EMI)
            &&snapshot.lines[0].x==220&&snapshot.lines[0].y==30
            &&snapshot.lines[0].horizontal_align==CENTER_ALIGN
            &&snapshot.lines[0].vertical_align==CENTER_ALIGN
            &&snapshot.lines[0].has_authored_position
            &&snapshot.lines[1].x==34&&snapshot.lines[1].y==83
            &&snapshot.lines[1].horizontal_align==LEFT_ALIGN
            &&snapshot.presentation.startup_active
            &&snapshot.presentation.renderer
                ==GE_ORIGINAL_FRONTEND_RENDERER_PITEM_MODEL
            &&snapshot.presentation.model_prop==PROP_LEGALPAGE
            &&snapshot.presentation.model_uniform_scale==1.0f
            &&snapshot.presentation.camera_eye_z==4000.0f
            &&snapshot.presentation.camera_target_z==0.0f
            &&snapshot.presentation.projection_fov_y_degrees==60.0f
            &&fabsf(snapshot.presentation.projection_aspect
                -(320.0f/240.0f))<0.000001f
            &&snapshot.presentation.projection_near==100.0f
            &&snapshot.presentation.projection_far==10000.0f
            &&snapshot.presentation.logical_width==440U
            &&snapshot.presentation.logical_height==330U
            &&snapshot.presentation.camera_eye[0]==0.0f
            &&snapshot.presentation.camera_eye[1]==0.0f
            &&snapshot.presentation.camera_eye[2]==4000.0f
            &&snapshot.presentation.camera_target[0]==0.0f
            &&snapshot.presentation.camera_target[1]==0.0f
            &&snapshot.presentation.camera_target[2]==0.0f
            &&snapshot.presentation.camera_up[0]==0.0f
            &&snapshot.presentation.camera_up[1]==1.0f
            &&snapshot.presentation.camera_up[2]==0.0f
            &&snapshot.presentation.model_uses_authored_origin
            &&snapshot.presentation.model_cull_both
            &&!snapshot.presentation.model_zbuffer_enabled);
        assert(snapshot.presentation.duration_frames==241U);
        for(frame=0;frame<241;++frame)
            assert(ge_original_frontend_start_tick(&startup,0));
        assert(ge_original_frontend_start_snapshot(&startup,&snapshot)
            &&snapshot.menu==MENU_LEGAL_SCREEN
            &&snapshot.presentation.frame==241U);
        advance_to_menu(&startup,0U,MENU_NINTENDO_LOGO);
        assert(harness.last_music==M_INTROSWOOSH);
        assert(ge_original_frontend_start_snapshot(&startup,&snapshot)
            &&snapshot.menu==MENU_NINTENDO_LOGO
            &&snapshot.presentation.model_prop==PROP_NINTENDOLOGO
            &&snapshot.presentation.frame==1U
            &&snapshot.presentation.duration_frames==501U
            &&snapshot.presentation.nintendo_ambient==255U
            &&snapshot.presentation.camera_eye_z==4000.0f
            &&snapshot.presentation.camera_eye[2]==4000.0f
            &&snapshot.presentation.model_uses_authored_origin
            &&fabsf(snapshot.presentation.nintendo_scale
                -0.0183333326131f)<0.000001f
            &&fabsf(snapshot.presentation.nintendo_rotation_radians
                -(-1.39626348019f+0.017453292f))<0.000001f);
        {
            GeOriginalFrontendStart timed_nintendo=startup;
            for(frame=0;frame<500;++frame)
                assert(ge_original_frontend_start_tick(&timed_nintendo,0));
            assert(ge_original_frontend_start_snapshot(
                &timed_nintendo,&snapshot)
                &&snapshot.menu==MENU_NINTENDO_LOGO
                &&snapshot.presentation.frame==501U
                &&snapshot.presentation.nintendo_scale==1.1f
                &&snapshot.presentation.nintendo_ambient==0U);
            advance_to_menu(
                &timed_nintendo,0U,MENU_RAREWARE_LOGO);
            assert(ge_original_frontend_start_snapshot(
                &timed_nintendo,&snapshot)
                &&snapshot.menu==MENU_RAREWARE_LOGO);
        }
        press(&startup,GE_ORIGINAL_FRONTEND_INPUT_START,
            MENU_RAREWARE_LOGO);
        assert(ge_original_frontend_start_snapshot(&startup,&snapshot)
            &&snapshot.presentation.renderer
                ==GE_ORIGINAL_FRONTEND_RENDERER_RAREWARE
            &&snapshot.presentation.duration_frames==290U
            &&snapshot.presentation.opacity==0U
            &&snapshot.presentation.rareware_rotation_degrees==-40.0f
            &&snapshot.presentation.projection_fov_y_degrees==60.0f
            &&snapshot.presentation.projection_near==100.0f
            &&snapshot.presentation.projection_far==5000.0f
            &&snapshot.presentation.camera_eye[2]==880.0f
            &&snapshot.presentation.camera_target[2]==879.0f
            &&snapshot.presentation.camera_up[1]==1.0f
            &&snapshot.presentation.model_cull_back
            &&snapshot.presentation.model_lighting_enabled
            &&snapshot.presentation.model_texture_gen_enabled
            &&snapshot.presentation.model_smooth_shading_enabled
            &&snapshot.presentation.rareware_light_ambient==0U
            &&snapshot.presentation.rareware_light_diffuse==255U
            &&snapshot.presentation.rareware_primary_rgb[0]==0U
            &&snapshot.presentation.rareware_secondary_rgb[0]==0U);
        {
            GeOriginalFrontendStart timed_rareware=startup;
            for(frame=0;frame<70;++frame)
                assert(ge_original_frontend_start_tick(&timed_rareware,0));
            assert(ge_original_frontend_start_snapshot(
                &timed_rareware,&snapshot)
                &&snapshot.menu==MENU_RAREWARE_LOGO
                &&snapshot.presentation.frame==71U
                &&snapshot.presentation.opacity==255U
                &&snapshot.presentation.rareware_rotation_degrees==100.0f
                &&snapshot.presentation.rareware_light_ambient==255U
                &&snapshot.presentation.rareware_primary_rgb[0]==255U
                &&snapshot.presentation.rareware_primary_rgb[1]==255U
                &&snapshot.presentation.rareware_primary_rgb[2]==255U
                &&snapshot.presentation.rareware_secondary_rgb[0]==240U
                &&snapshot.presentation.rareware_secondary_rgb[1]==208U
                &&snapshot.presentation.rareware_secondary_rgb[2]==240U);
        }
        assert(ge_original_frontend_start_sequence_complete(&startup));
        advance_to_menu(&startup,0U,MENU_EYE_INTRO);
        assert(harness.last_music==M_INTRO);
        assert(ge_original_frontend_start_snapshot(&startup,&snapshot)
            &&snapshot.menu==MENU_EYE_INTRO
            &&snapshot.presentation.renderer
                ==GE_ORIGINAL_FRONTEND_RENDERER_GUNBARREL
            &&snapshot.presentation.projection_fov_y_degrees==46.0f
            &&snapshot.presentation.projection_near==10.0f
            &&snapshot.presentation.projection_far==10000.0f
            &&fabsf(snapshot.presentation.camera_eye[0]-1758.2957f)
                <0.0001f
            &&snapshot.presentation.camera_eye[1]==220.0f
            &&fabsf(snapshot.presentation.camera_eye[2]-684.28143f)
                <0.0001f
            &&fabsf(snapshot.presentation.camera_target[0]
                -(1758.2957f-0.97f))<0.0001f
            &&snapshot.presentation.camera_target[1]==220.0f
            &&fabsf(snapshot.presentation.camera_target[2]
                -(684.28143f+0.24f))<0.0001f
            &&snapshot.presentation.camera_up[1]==1.0f);
        assert(ge_original_frontend_start_sequence_complete(&startup));
        advance_to_menu(&startup,0U,MENU_GOLDENEYE_LOGO);
        assert(ge_original_frontend_start_snapshot(&startup,&snapshot)
            &&snapshot.menu==MENU_GOLDENEYE_LOGO
            &&snapshot.presentation.model_prop==PROP_GOLDENEYELOGO
            &&snapshot.presentation.duration_frames==180U
            &&snapshot.presentation.title_texture_gen
            &&snapshot.presentation.title_light_ambient==0x96U
            &&snapshot.presentation.title_light_diffuse==0xffU
            &&snapshot.presentation.title_light_direction[0]==77
            &&snapshot.presentation.title_light_direction[1]==77
            &&snapshot.presentation.title_light_direction[2]==46
            &&snapshot.presentation.model_uniform_scale==1.2f
            &&snapshot.presentation.camera_eye_z==3000.0f
            &&snapshot.presentation.camera_eye[2]==3000.0f
            &&snapshot.presentation.reflection_camera_eye_z==4000.0f
            &&snapshot.presentation.model_lighting_enabled
            &&snapshot.presentation.model_texture_gen_enabled
            &&snapshot.presentation.model_uses_authored_origin
            &&snapshot.presentation.model_cull_both
            &&!snapshot.presentation.model_zbuffer_enabled);
    assert(!ge_original_frontend_start_sequence_complete(&startup));
    }
    {
        GeOriginalFrontendStart cast_start;
        int frame;
        assert(ge_original_frontend_start_reset(&cast_start,&services));
        for(frame=0;frame<181;++frame)
            assert(ge_original_frontend_start_tick(&cast_start,0));
        assert(ge_original_frontend_start_snapshot(&cast_start,&snapshot)
            &&snapshot.menu==MENU_GOLDENEYE_LOGO);
        advance_to_menu(&cast_start,0U,MENU_DISPLAY_CAST);
        assert(ge_original_frontend_start_snapshot(&cast_start,&snapshot)
            &&snapshot.menu==MENU_DISPLAY_CAST
            &&snapshot.line_count==0U);
        assert(ge_original_frontend_start_cast_event(&cast_start,
            GE_ORIGINAL_FRONTEND_CAST_EVENT_RELOAD));
        assert(!ge_original_frontend_start_cast_event(&cast_start,
            GE_ORIGINAL_FRONTEND_CAST_EVENT_RAMROM));
        assert(!ge_original_frontend_start_ramrom(
            &cast_start,LEVELID_CUBA,DIFFICULTY_AGENT));
        assert(ge_original_frontend_start_ramrom(
            &cast_start,LEVELID_FACILITY,DIFFICULTY_SECRET));
        advance_to_menu(&cast_start,0U,MENU_RUN_STAGE);
        assert(ge_original_frontend_start_snapshot(&cast_start,&snapshot)
            &&snapshot.menu==MENU_RUN_STAGE&&snapshot.stage_requested
            &&snapshot.stage==LEVELID_FACILITY
            &&snapshot.difficulty==DIFFICULTY_SECRET
            &&harness.stage==LEVELID_FACILITY
            &&harness.difficulty==DIFFICULTY_SECRET);
        assert(ge_original_frontend_start_reset(&cast_start,&services));
        for(frame=0;frame<181;++frame)
            assert(ge_original_frontend_start_tick(&cast_start,0));
        advance_to_menu(&cast_start,0U,MENU_DISPLAY_CAST);
        assert(ge_original_frontend_start_cast_event(&cast_start,
            GE_ORIGINAL_FRONTEND_CAST_EVENT_FILE_SELECT));
        advance_to_menu(&cast_start,0U,MENU_FILE_SELECT);
        assert(ge_original_frontend_start_snapshot(&cast_start,&snapshot)
            &&snapshot.menu==MENU_FILE_SELECT);
        harness.music_calls=0;
        harness.summary_calls=0;
        harness.stage_calls=0;
        harness.difficulty_calls=0;
    }
    assert(!ge_original_frontend_start_reset(NULL,&services));
    assert(ge_original_frontend_start_reset(&frontend,&services));
    assert(ge_original_frontend_start_snapshot(&frontend,&snapshot)
        &&snapshot.menu==MENU_GOLDENEYE_LOGO&&snapshot.line_count==1U
        &&snapshot.lines[0].text_id
            ==getStringID(LTITLE,TITLE_STR_04_START));
    leave_first_title(&frontend);
    assert(harness.music_calls==1&&harness.last_music==M_FOLDERS);
    assert(harness.summary_calls==MAX_FOLDER_COUNT);
    {
        GeOriginalFrontendStart idle=frontend;
        int frame;
        idle.menu_timer=1800U;
        assert(ge_original_frontend_start_tick(&idle,0U));
        assert(ge_original_frontend_start_snapshot(&idle,&snapshot)
            &&snapshot.menu==MENU_FILE_SELECT);
        for(frame=1;frame<=4;++frame){
            assert(ge_original_frontend_start_tick(&idle,0U));
            assert(ge_original_frontend_start_snapshot(&idle,&snapshot)
                &&snapshot.menu==MENU_SWITCH_SCREENS
                &&idle.menu_timer==(uint32_t)frame);
        }
        assert(ge_original_frontend_start_tick(&idle,0U));
        assert(ge_original_frontend_start_snapshot(&idle,&snapshot)
            &&snapshot.menu==MENU_LEGAL_SCREEN
            &&snapshot.presentation.frame==1U);
        /* After the first title visit, any legal/logo input returns directly
         * to file select through the same reload transition. */
        advance_to_menu(
            &idle,GE_ORIGINAL_FRONTEND_INPUT_START,MENU_FILE_SELECT);

        idle=frontend;
        idle.menu_timer=1800U;
        assert(ge_original_frontend_start_tick(
            &idle,GE_ORIGINAL_FRONTEND_INPUT_RIGHT));
        assert(ge_original_frontend_start_snapshot(&idle,&snapshot)
            &&snapshot.menu==MENU_FILE_SELECT&&idle.menu_timer==0U
            &&idle.menu_update==MENU_INVALID);
        harness.summary_calls=MAX_FOLDER_COUNT;
        harness.music_calls=1;
    }
    {
        const GeOriginalFrontendWalletBounds bounds[4]={
            {30.0f,30.0f,190.0f,145.0f},
            {250.0f,30.0f,410.0f,145.0f},
            {30.0f,180.0f,190.0f,295.0f},
            {250.0f,180.0f,410.0f,295.0f},
        };
        frontend.cursor.x=300.0f;
        frontend.cursor.y=240.0f;
        assert(ge_original_frontend_start_set_wallet_bounds(
            &frontend,bounds));
        assert(frontend.wallet_bounds_ready
            &&frontend.wallet_hover==FOLDER4
            &&frontend.folder==FOLDER4);
        /* Once live model geometry is bound, digital direction edges no
         * longer select a bridge grid cell behind the cursor. */
        press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_LEFT,MENU_FILE_SELECT);
        assert(frontend.folder==FOLDER4);
    }
    assert(ge_original_frontend_start_snapshot(&frontend,&snapshot)
        &&snapshot.folder==FOLDER4&&snapshot.line_count==3U
        &&snapshot.lines[0].status==3U
        &&snapshot.lines[0].text_id
            ==getStringID(LTITLE,TITLE_STR_25_CONFIRM)
        &&snapshot.lines[1].text_id==getStringID(LTITLE,TITLE_STR_27_COPY)
        &&snapshot.lines[2].text_id==getStringID(LTITLE,TITLE_STR_28_ERASE)
        &&snapshot.folder_has_progress[FOLDER4]
        &&snapshot.folder_mission[FOLDER4]==SP_LEVEL_DAM
        &&snapshot.folder_difficulty[FOLDER4]==DIFFICULTY_AGENT
        &&!snapshot.folder_has_progress[FOLDER1]
        &&snapshot.folder_mission[FOLDER1]==SP_LEVEL_DAM-1
        &&snapshot.folder_difficulty[FOLDER1]==DIFFICULTY_MULTI);
    assert(harness.summary_calls==MAX_FOLDER_COUNT);
    {
        const GeOriginalFrontendWalletBounds action_bounds[2]={
            {209.0f,271.0f,280.0f,299.0f},
            {319.0f,271.0f,390.0f,299.0f},
        };
        frontend.cursor.x=220.0f;frontend.cursor.y=285.0f;
        assert(ge_original_frontend_start_set_file_action_bounds(
            &frontend,action_bounds));
        assert(frontend.file_action_hover==0);
    }
    press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,MENU_FILE_SELECT);
    assert(ge_original_frontend_start_snapshot(&frontend,&snapshot)
        &&snapshot.file_action==GE_ORIGINAL_FRONTEND_FILE_COPY
        &&snapshot.lines[1].selected
        &&harness.last_sfx==DOOR_LOCK_SFX);
    frontend.cursor.x=300.0f;frontend.cursor.y=240.0f;
    assert(ge_original_frontend_start_cursor_tick(&frontend,0,0,1.0f)
        &&frontend.wallet_hover==FOLDER4);
    press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,MENU_FILE_SELECT);
    assert(harness.copy_calls==1&&harness.last_sfx==COPY_FILE_SFX);
    assert(harness.summary_calls==MAX_FOLDER_COUNT*2);
    frontend.cursor.x=335.0f;frontend.cursor.y=285.0f;
    assert(ge_original_frontend_start_cursor_tick(&frontend,0,0,1.0f)
        &&frontend.file_action_hover==1);
    press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,MENU_FILE_SELECT);
    frontend.cursor.x=300.0f;frontend.cursor.y=240.0f;
    assert(ge_original_frontend_start_cursor_tick(&frontend,0,0,1.0f));
    press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,MENU_FILE_SELECT);
    assert(ge_original_frontend_start_snapshot(&frontend,&snapshot)
        &&snapshot.erase_pending&&!snapshot.erase_confirm_selected
        &&snapshot.line_count==3U
        &&snapshot.lines[0].text_id
            ==getStringID(LTITLE,TITLE_STR_23_ERASEFILE)
        &&snapshot.lines[1].text_id
            ==getStringID(LTITLE,TITLE_STR_24_CANCEL)
        &&snapshot.lines[1].selected
        &&snapshot.lines[2].text_id
            ==getStringID(LTITLE,TITLE_STR_25_CONFIRM));
    press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_RIGHT,MENU_FILE_SELECT);
    assert(ge_original_frontend_start_snapshot(&frontend,&snapshot)
        &&snapshot.erase_pending&&snapshot.erase_confirm_selected
        &&snapshot.lines[2].selected
        &&harness.last_sfx==OPTION_CLICK2_SFX);
    press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,MENU_FILE_SELECT);
    assert(harness.erase_calls==1&&!harness.progress[FOLDER4]
        &&harness.last_sfx==GUN_M60AMMGUN_3_SFX);
    assert(harness.summary_calls==MAX_FOLDER_COUNT*3);
    press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,MENU_MODE_SELECT);
    assert(harness.folder==FOLDER4&&harness.last_sfx==PAPER_TURN_SFX);
    assert(ge_original_frontend_start_snapshot(&frontend,&snapshot)
        &&snapshot.line_count==2U
        &&snapshot.tab_count==1U
        &&snapshot.tabs[0].status==3U
        &&snapshot.lines[0].text_id
            ==getStringID(LTITLE,TITLE_STR_29_SELECTMISSION));
    press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,MENU_MISSION_SELECT);
    press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,MENU_DIFFICULTY);
    assert(ge_original_frontend_start_snapshot(&frontend,&snapshot)
        &&snapshot.stage==LEVELID_DAM
        &&snapshot.difficulty==DIFFICULTY_00&&snapshot.line_count==4U);
    press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_UP,MENU_DIFFICULTY);
    press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,MENU_BRIEFING);
    assert(ge_original_frontend_start_snapshot(&frontend,&snapshot)
        &&snapshot.briefing_page==BRIEFING_TITLE
        &&snapshot.difficulty==DIFFICULTY_SECRET
        &&snapshot.line_count==4U
        &&strcmp(snapshot.chapter_number,"1")==0
        &&snapshot.chapter_title
            ==getStringID(LTITLE,TITLE_STR_120_ARK)
        &&strcmp(snapshot.part_number,"i")==0
        &&snapshot.part_title==getStringID(LTITLE,TITLE_STR_121_DAM)
        &&snapshot.difficulty_title
            ==getStringID(LTITLE,TITLE_STR_20_SECRETAGENT));
    /* Secret Agent exposes authored Dam objectives 0 and 3 only. */
    assert(snapshot.lines[2].text_id==getStringID(LDAM,4)
        &&snapshot.lines[3].text_id==getStringID(LDAM,7));
    press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,MENU_BRIEFING);
    assert(ge_original_frontend_start_snapshot(&frontend,&snapshot)
        &&snapshot.briefing_page==BRIEFING_OVERVIEW
        &&snapshot.lines[2].text_id==getStringID(LDAM,0));
    {
        int frame;
        assert(ge_original_frontend_start_tick(
            &frontend,GE_ORIGINAL_FRONTEND_INPUT_START));
        assert(frontend.current_menu==MENU_BRIEFING
            &&frontend.menu_update==MENU_RUN_STAGE
            &&harness.stage_calls==0&&harness.difficulty_calls==0);
        for(frame=1;frame<=4;++frame){
            assert(ge_original_frontend_start_tick(&frontend,0U));
            assert(frontend.current_menu==MENU_SWITCH_SCREENS
                &&frontend.menu_timer==(uint32_t)frame
                &&harness.stage_calls==0&&harness.difficulty_calls==0);
        }
        assert(ge_original_frontend_start_tick(&frontend,0U));
        assert(frontend.current_menu==MENU_RUN_STAGE);
    }
    assert(harness.stage_calls==1&&harness.difficulty_calls==1
        &&harness.stage==LEVELID_DAM
        &&harness.difficulty==DIFFICULTY_SECRET);
    assert(ge_original_frontend_start_tick(
        &frontend,GE_ORIGINAL_FRONTEND_INPUT_START)
        &&harness.stage_calls==1&&harness.difficulty_calls==1);

    {
        GeOriginalFrontendMissionResult result;
        memset(&result,0,sizeof(result));
        result.objective_status[0]=2U;
        result.objective_status[1]=2U;
        result.objective_status[2]=2U;
        result.objective_status[3]=1U;
        result.mission_time_ticks=3600;
        result.kill_count=4;result.shots_fired=20;result.head_hits=2;
        result.new_cheat_unlocked=1U;
        result.target_time_seconds=160;
        result.best_time_seconds=59;
        assert(ge_original_frontend_start_stage_ended(&frontend,&result));
        assert(frontend.current_menu==MENU_RUN_STAGE
            &&frontend.menu_update==MENU_MISSION_FAILED);
        assert(ge_original_frontend_start_tick(&frontend,0U));
        assert(frontend.current_menu==MENU_SWITCH_SCREENS
            &&frontend.menu_timer==1U);
        advance_to_menu(&frontend,0U,MENU_MISSION_FAILED);
        assert(ge_original_frontend_start_snapshot(&frontend,&snapshot)
            &&snapshot.menu==MENU_MISSION_FAILED
            &&snapshot.result_valid&&snapshot.line_count==6U
            &&snapshot.tab_count==2U
            &&snapshot.lines[3].text_id
                ==getStringID(LTITLE,TITLE_STR_103_FAILED)
            &&snapshot.lines[4].status==2U
            &&snapshot.result.mission_time_ticks==3600
            &&snapshot.result.new_cheat_unlocked
            &&snapshot.result.target_time_seconds==160
            &&snapshot.result.best_time_seconds==59);
        press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,
            MENU_MISSION_COMPLETE);
        assert(ge_original_frontend_start_snapshot(&frontend,&snapshot)
            &&snapshot.line_count==11U
            &&snapshot.tab_count==2U
            &&snapshot.lines[1].text_id
                ==getStringID(LTITLE,TITLE_STR_104_STATS)
            &&snapshot.lines[8].text_id
                ==getStringID(LTITLE,TITLE_STR_111_LIMBHITS)
            &&snapshot.lines[9].text_id
                ==getStringID(LTITLE,TITLE_STR_112_OTHER)
            &&snapshot.lines[10].text_id
                ==getStringID(LTITLE,TITLE_STR_113_KILLTOTAL));
        /* An incomplete mission's canonical Next action returns to the same
         * authored briefing, from which Start requests a real restart. */
        press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,MENU_BRIEFING);
        press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_START,MENU_RUN_STAGE);
        assert(harness.stage_calls==2&&harness.difficulty_calls==2);

        memset(&result,0,sizeof(result));
        result.all_objectives_complete_alive=1U;
        memset(result.objective_status,1,sizeof(result.objective_status));
        assert(ge_original_frontend_start_stage_ended(&frontend,&result));
        advance_to_menu(&frontend,0U,MENU_MISSION_FAILED);
        assert(ge_original_frontend_start_snapshot(&frontend,&snapshot)
            &&snapshot.menu==MENU_MISSION_FAILED
            &&snapshot.lines[3].text_id
                ==getStringID(LTITLE,TITLE_STR_102_COMPLETED));
        press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,
            MENU_MISSION_COMPLETE);
        press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,MENU_BRIEFING);
        assert(ge_original_frontend_start_snapshot(&frontend,&snapshot)
            &&snapshot.mission==SP_LEVEL_FACILITY
            &&snapshot.stage==LEVELID_FACILITY
            &&snapshot.briefing_page==BRIEFING_TITLE
            &&snapshot.lines[0].text_id
                ==getStringID(LTITLE,TITLE_STR_122_FAC)
            &&snapshot.line_count==7U
            &&snapshot.lines[2].text_id==getStringID(LARK,4)
            &&snapshot.lines[6].text_id==getStringID(LARK,8));
        press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_START,MENU_RUN_STAGE);
        assert(harness.stage_calls==3&&harness.difficulty_calls==3
            &&harness.stage==LEVELID_FACILITY);

        /* Retry remains attached to the currently running mission; it must
         * not fall back to the first mission after the session reload. */
        memset(&result,0,sizeof(result));
        result.mission_failed_or_aborted=1U;
        assert(ge_original_frontend_start_stage_ended(&frontend,&result));
        advance_to_menu(&frontend,0U,MENU_MISSION_FAILED);
        press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,
            MENU_MISSION_COMPLETE);
        press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,MENU_BRIEFING);
        assert(ge_original_frontend_start_snapshot(&frontend,&snapshot)
            &&snapshot.mission==SP_LEVEL_FACILITY
            &&snapshot.stage==LEVELID_FACILITY);
        press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_START,MENU_RUN_STAGE);
        assert(harness.stage_calls==4&&harness.difficulty_calls==4
            &&harness.stage==LEVELID_FACILITY);

        memset(&result,0,sizeof(result));
        result.all_objectives_complete_alive=1U;
        memset(result.objective_status,1,sizeof(result.objective_status));
        assert(ge_original_frontend_start_stage_ended(&frontend,&result));
        advance_to_menu(&frontend,0U,MENU_MISSION_FAILED);
        press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,
            MENU_MISSION_COMPLETE);
        press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,MENU_BRIEFING);
        assert(ge_original_frontend_start_snapshot(&frontend,&snapshot)
            &&snapshot.mission==SP_LEVEL_RUNWAY
            &&snapshot.stage==LEVELID_RUNWAY
            &&snapshot.lines[0].text_id
                ==getStringID(LTITLE,TITLE_STR_123_RUN)
            &&snapshot.lines[2].text_id==getStringID(LRUN,4));
        press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_START,MENU_RUN_STAGE);
        assert(harness.stage_calls==5&&harness.difficulty_calls==5
            &&harness.stage==LEVELID_RUNWAY);

        /* Successful Runway crosses the original chapter boundary into the
         * authored Severnaya Surface briefing instead of exhausting the
         * platform contract. */
        memset(&result,0,sizeof(result));
        result.all_objectives_complete_alive=1U;
        memset(result.objective_status,1,sizeof(result.objective_status));
        assert(ge_original_frontend_start_stage_ended(&frontend,&result));
        advance_to_menu(&frontend,0U,MENU_MISSION_FAILED);
        press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,
            MENU_MISSION_COMPLETE);
        press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,MENU_BRIEFING);
        assert(ge_original_frontend_start_snapshot(&frontend,&snapshot)
            &&snapshot.mission==SP_LEVEL_SURFACE1
            &&snapshot.stage==LEVELID_SURFACE
            &&snapshot.lines[0].text_id
                ==getStringID(LTITLE,TITLE_STR_125_SURF)
            &&snapshot.lines[2].text_id==getStringID(LSEVX,4)
            &&snapshot.lines[5].text_id==getStringID(LSEVX,7));
        press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_START,MENU_RUN_STAGE);
        assert(harness.stage_calls==6&&harness.difficulty_calls==6
            &&harness.stage==LEVELID_SURFACE);

        /* The next successful transition preserves the authored Severnaya
         * chapter ordering and all five Bunker 1 objective records. */
        memset(&result,0,sizeof(result));
        result.all_objectives_complete_alive=1U;
        memset(result.objective_status,1,sizeof(result.objective_status));
        assert(ge_original_frontend_start_stage_ended(&frontend,&result));
        advance_to_menu(&frontend,0U,MENU_MISSION_FAILED);
        press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,
            MENU_MISSION_COMPLETE);
        press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,MENU_BRIEFING);
        assert(ge_original_frontend_start_snapshot(&frontend,&snapshot)
            &&snapshot.mission==SP_LEVEL_BUNKER1
            &&snapshot.stage==LEVELID_BUNKER1
            &&snapshot.lines[0].text_id
                ==getStringID(LTITLE,TITLE_STR_126_BUNK)
            &&snapshot.line_count==5U
            &&snapshot.lines[2].text_id==getStringID(LSEV,4)
            &&snapshot.lines[4].text_id==getStringID(LSEV,8));
        press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_START,MENU_RUN_STAGE);
        assert(harness.stage_calls==7&&harness.difficulty_calls==7
            &&harness.stage==LEVELID_BUNKER1);

        memset(&result,0,sizeof(result));
        result.all_objectives_complete_alive=1U;
        memset(result.objective_status,1,sizeof(result.objective_status));
        assert(ge_original_frontend_start_stage_ended(&frontend,&result));
        advance_to_menu(&frontend,0U,MENU_MISSION_FAILED);
        press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,
            MENU_MISSION_COMPLETE);
        press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,MENU_BRIEFING);
        assert(ge_original_frontend_start_snapshot(&frontend,&snapshot)
            &&snapshot.mission==SP_LEVEL_SILO
            &&snapshot.stage==LEVELID_SILO
            &&snapshot.lines[0].text_id
                ==getStringID(LTITLE,TITLE_STR_128_SILO4)
            &&snapshot.line_count==6U
            &&snapshot.lines[2].text_id==getStringID(LSILO,5)
            &&snapshot.lines[5].text_id==getStringID(LSILO,8));
        press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_START,MENU_RUN_STAGE);
        assert(harness.stage_calls==8&&harness.difficulty_calls==8
            &&harness.stage==LEVELID_SILO);

        memset(&result,0,sizeof(result));
        result.all_objectives_complete_alive=1U;
        memset(result.objective_status,1,sizeof(result.objective_status));
        assert(ge_original_frontend_start_stage_ended(&frontend,&result));
        advance_to_menu(&frontend,0U,MENU_MISSION_FAILED);
        press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,
            MENU_MISSION_COMPLETE);
        press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,MENU_BRIEFING);
        assert(ge_original_frontend_start_snapshot(&frontend,&snapshot)
            &&snapshot.mission==SP_LEVEL_FRIGATE
            &&snapshot.stage==LEVELID_FRIGATE
            &&snapshot.lines[0].text_id
                ==getStringID(LTITLE,TITLE_STR_131_FRIG)
            &&snapshot.line_count==6U
            &&snapshot.lines[2].text_id==getStringID(LDEST,4)
            &&snapshot.lines[5].text_id==getStringID(LDEST,7));
        press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_START,MENU_RUN_STAGE);
        assert(harness.stage_calls==9&&harness.difficulty_calls==9
            &&harness.stage==LEVELID_FRIGATE);

        /* The native contract now retains the complete 20-entry mission
         * folder.  Frigate therefore advances to Surface 2 with that stage's
         * distinct LSEVXB authored objective order instead of terminating. */
        memset(&result,0,sizeof(result));
        result.all_objectives_complete_alive=1U;
        memset(result.objective_status,1,sizeof(result.objective_status));
        assert(ge_original_frontend_start_stage_ended(&frontend,&result));
        advance_to_menu(&frontend,0U,MENU_MISSION_FAILED);
        press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,
            MENU_MISSION_COMPLETE);
        press(&frontend,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,MENU_BRIEFING);
        assert(ge_original_frontend_start_snapshot(&frontend,&snapshot)
            &&snapshot.mission==SP_LEVEL_SURFACE2
            &&snapshot.stage==LEVELID_SURFACE2
            &&snapshot.line_count==5U
            &&snapshot.lines[2].text_id==getStringID(LSEVXB,5)
            &&snapshot.lines[4].text_id==getStringID(LSEVXB,7));
    }
    {
        GeOriginalFrontendStart selection;
        assert(ge_original_frontend_start_reset(&selection,&services));
        leave_first_title(&selection);
        press(&selection,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,MENU_MODE_SELECT);
        press(&selection,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,
            MENU_MISSION_SELECT);
        assert(ge_original_frontend_start_snapshot(&selection,&snapshot)
            &&snapshot.line_count==21U&&snapshot.mission==SP_LEVEL_DAM
            &&snapshot.lines[6].text_id
                ==getStringID(LTITLE,TITLE_STR_129_SILO)
            &&snapshot.lines[10].text_id
                ==getStringID(LTITLE,TITLE_STR_134_STAT)
            &&snapshot.lines[17].text_id
                ==getStringID(LTITLE,TITLE_STR_145_CAV)
            &&snapshot.lines[20].text_id
                ==getStringID(LTITLE,TITLE_STR_153_EGYPTIAN));
        assert(ge_original_frontend_start_tick(
            &selection,GE_ORIGINAL_FRONTEND_INPUT_RIGHT));
        assert(ge_original_frontend_start_snapshot(&selection,&snapshot)
            &&snapshot.mission==SP_LEVEL_FACILITY
            &&snapshot.lines[2].selected);
        press(&selection,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,MENU_DIFFICULTY);
        assert(ge_original_frontend_start_snapshot(&selection,&snapshot)
            &&snapshot.stage==LEVELID_FACILITY);
    }
    {
        GeOriginalFrontendStart ending;
        GeOriginalFrontendMissionResult result;
        int stage_calls=harness.stage_calls;
        assert(ge_original_frontend_start_reset(&ending,&services));
        ending.current_menu=MENU_RUN_STAGE;
        ending.maybe_prev_menu=MENU_INVALID;
        ending.stage_requested=1U;
        ending.mission=SP_LEVEL_CRADLE;
        ending.stage=LEVELID_CRADLE;
        ending.difficulty=DIFFICULTY_AGENT;
        memset(&result,0,sizeof(result));
        result.all_objectives_complete_alive=1U;
        assert(ge_original_frontend_start_stage_ended(&ending,&result));
        advance_to_menu(&ending,0U,MENU_MISSION_FAILED);
        press(&ending,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,
            MENU_MISSION_COMPLETE);
        press(&ending,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,MENU_RUN_STAGE);
        assert(harness.stage_calls==stage_calls+1
            &&harness.stage==LEVELID_CUBA);

        /* Bonus missions have no campaign successor; their exact Next action
         * returns to the complete grid without requesting another stage. */
        assert(ge_original_frontend_start_reset(&ending,&services));
        ending.current_menu=MENU_RUN_STAGE;
        ending.maybe_prev_menu=MENU_INVALID;
        ending.stage_requested=1U;
        ending.mission=SP_LEVEL_AZTEC;
        ending.stage=LEVELID_AZTEC;
        ending.difficulty=DIFFICULTY_AGENT;
        assert(ge_original_frontend_start_stage_ended(&ending,&result));
        advance_to_menu(&ending,0U,MENU_MISSION_FAILED);
        press(&ending,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,
            MENU_MISSION_COMPLETE);
        press(&ending,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,
            MENU_MISSION_SELECT);
        assert(ge_original_frontend_start_snapshot(&ending,&snapshot)
            &&snapshot.mission==SP_LEVEL_AZTEC
            &&snapshot.stage==LEVELID_NONE
            &&snapshot.line_count==21U);
    }
    {
        GeOriginalFrontendStart ending;
        GeOriginalFrontendMissionResult result;
        int stage_calls=harness.stage_calls;

        /* The live death handoff sets both the global mission-failed state
         * and Bond KIA.  The unchanged report gives KIA precedence, then its
         * Statistics Next action returns to the same authored briefing and
         * requests a real stage restart. */
        assert(ge_original_frontend_start_reset(&ending,&services));
        ending.current_menu=MENU_RUN_STAGE;
        ending.maybe_prev_menu=MENU_INVALID;
        ending.stage_requested=1U;
        ending.mission=SP_LEVEL_DAM;
        ending.stage=LEVELID_DAM;
        ending.difficulty=DIFFICULTY_AGENT;
        memset(&result,0,sizeof(result));
        result.bond_kia=1U;
        result.mission_failed_or_aborted=1U;
        assert(ge_original_frontend_start_stage_ended(&ending,&result));
        advance_to_menu(&ending,0U,MENU_MISSION_FAILED);
        assert(ge_original_frontend_start_snapshot(&ending,&snapshot)
            &&snapshot.result_valid
            &&snapshot.lines[3].text_id
                ==getStringID(LTITLE,TITLE_STR_100_KIA)
            &&snapshot.lines[3].status==OBJECTIVESTATUS_FAILED);
        press(&ending,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,
            MENU_MISSION_COMPLETE);
        press(&ending,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,MENU_BRIEFING);
        assert(ge_original_frontend_start_snapshot(&ending,&snapshot)
            &&snapshot.mission==SP_LEVEL_DAM
            &&snapshot.stage==LEVELID_DAM);
        press(&ending,GE_ORIGINAL_FRONTEND_INPUT_START,MENU_RUN_STAGE);
        assert(harness.stage_calls==stage_calls+1
            &&harness.stage==LEVELID_DAM);

        /* A watch-menu abort uses the distinct authored ABORTED status but
         * owns the same canonical retry route; it must not advance campaign
         * progress or silently fall back to another selected mission. */
        stage_calls=harness.stage_calls;
        assert(ge_original_frontend_start_reset(&ending,&services));
        ending.current_menu=MENU_RUN_STAGE;
        ending.maybe_prev_menu=MENU_INVALID;
        ending.stage_requested=1U;
        ending.mission=SP_LEVEL_DAM;
        ending.stage=LEVELID_DAM;
        ending.difficulty=DIFFICULTY_AGENT;
        memset(&result,0,sizeof(result));
        result.mission_failed_or_aborted=1U;
        assert(ge_original_frontend_start_stage_ended(&ending,&result));
        advance_to_menu(&ending,0U,MENU_MISSION_FAILED);
        assert(ge_original_frontend_start_snapshot(&ending,&snapshot)
            &&snapshot.result_valid
            &&snapshot.lines[3].text_id
                ==getStringID(LTITLE,TITLE_STR_101_ABORTED)
            &&snapshot.lines[3].status==OBJECTIVESTATUS_FAILED);
        press(&ending,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,
            MENU_MISSION_COMPLETE);
        press(&ending,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,MENU_BRIEFING);
        assert(ge_original_frontend_start_snapshot(&ending,&snapshot)
            &&snapshot.mission==SP_LEVEL_DAM
            &&snapshot.stage==LEVELID_DAM);
        press(&ending,GE_ORIGINAL_FRONTEND_INPUT_START,MENU_RUN_STAGE);
        assert(harness.stage_calls==stage_calls+1
            &&harness.stage==LEVELID_DAM);
    }
    {
        GeOriginalFrontendStart cursor_frontend;
        int frame;
        assert(ge_original_frontend_start_reset(
            &cursor_frontend,&services));
        cursor_frontend.current_menu=MENU_MISSION_SELECT;
        cursor_frontend.maybe_prev_menu=MENU_INVALID;
        cursor_frontend.mission=SP_LEVEL_DAM;
        ge_original_frontend_cursor_set_mission(
            &cursor_frontend.cursor,SP_LEVEL_DAM);
        for(frame=0;frame<7;++frame)
            assert(ge_original_frontend_start_cursor_tick(
                &cursor_frontend,70,0,1.0f));
        assert(cursor_frontend.mission==SP_LEVEL_FACILITY);
        cursor_frontend.current_menu=MENU_DIFFICULTY;
        cursor_frontend.mission=SP_LEVEL_DAM;
        ge_original_frontend_cursor_set_difficulty(
            &cursor_frontend.cursor,DIFFICULTY_00);
        assert(ge_original_frontend_start_cursor_tick(
            &cursor_frontend,0,0,1.0f));
        assert(cursor_frontend.difficulty==DIFFICULTY_00);
        cursor_frontend.current_menu=MENU_MISSION_COMPLETE;
        cursor_frontend.cursor.x=399.0f;
        cursor_frontend.cursor.y=250.0f;
        assert(ge_original_frontend_start_cursor_tick(
            &cursor_frontend,0,0,1.0f));
        assert(ge_original_frontend_start_tick(&cursor_frontend,
            GE_ORIGINAL_FRONTEND_INPUT_CONFIRM));
        assert(cursor_frontend.maybe_prev_menu==MENU_MISSION_SELECT);
    }
    {
        GeOriginalFrontendStart options;
        harness.highest=DIFFICULTY_007;
        assert(ge_original_frontend_start_reset(&options,&services));
        options.current_menu=MENU_DIFFICULTY;
        options.maybe_prev_menu=MENU_INVALID;
        options.difficulty=DIFFICULTY_007;
        options.cursor.x=106.0f;options.cursor.y=276.0f;
        press(&options,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,MENU_007_OPTIONS);
        assert(ge_original_frontend_start_snapshot(&options,&snapshot)
            &&snapshot.line_count==6U
            &&snapshot.lines[1].text_id
                ==getStringID(LTITLE,TITLE_STR_40_SPECOPS)
            &&snapshot.lines[5].text_id
                ==getStringID(LTITLE,TITLE_STR_41_REACTION));
        options.cursor.x=205.0f;options.cursor.y=170.0f;
        assert(ge_original_frontend_start_007_drag(&options,1));
        assert(harness.slider_calls==1
            &&fabsf(harness.sliders[1]-2.5f)<0.0001f);
        /* Pressing while over a slider edits it; it must not accidentally
         * activate Next until the exact tab hitbox owns the cursor. */
        assert(ge_original_frontend_start_tick(
            &options,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM)
            &&options.current_menu==MENU_007_OPTIONS);
        ge_original_frontend_cursor_set_next_tab(&options.cursor);
        assert(ge_original_frontend_start_cursor_tick(&options,0,0,1.0f));
        press(&options,GE_ORIGINAL_FRONTEND_INPUT_CONFIRM,MENU_BRIEFING);
        harness.highest=DIFFICULTY_00;
    }
    puts("canonical complete-solo frontend flow passed");return 0;
}
