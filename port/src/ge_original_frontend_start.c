#include "ge_original_frontend_start.h"

#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>
#include "assets/obseg/text/LtitleE.h"

#include <string.h>

#include "ge_original_frontend_start_contract.inc"

#define GE_FRONTEND_ANY_INPUT ((uint32_t)( \
    GE_ORIGINAL_FRONTEND_INPUT_CONFIRM \
    |GE_ORIGINAL_FRONTEND_INPUT_BACK \
    |GE_ORIGINAL_FRONTEND_INPUT_START \
    |GE_ORIGINAL_FRONTEND_INPUT_UP \
    |GE_ORIGINAL_FRONTEND_INPUT_DOWN \
    |GE_ORIGINAL_FRONTEND_INPUT_LEFT \
    |GE_ORIGINAL_FRONTEND_INPUT_RIGHT \
    |GE_ORIGINAL_FRONTEND_INPUT_FILE_COPY \
    |GE_ORIGINAL_FRONTEND_INPUT_FILE_ERASE))

#define GE_FRONTEND_LEGAL_TIMER_MAX 241U
#define GE_FRONTEND_NINTENDO_TIMER_MAX 501U

static const GeFrontendMissionContract *ge_frontend_mission(
    const GeOriginalFrontendStart *frontend)
{
    size_t index;
    if(frontend==NULL)return NULL;
    for(index=0;index<sizeof(ge_frontend_contract_missions)
            /sizeof(ge_frontend_contract_missions[0]);++index)
        if(ge_frontend_contract_missions[index].mission==frontend->mission)
            return &ge_frontend_contract_missions[index];
    return NULL;
}

static const GeFrontendMissionContract *ge_frontend_next_mission(
    const GeOriginalFrontendStart *frontend)
{
    size_t index;
    if(frontend==NULL)return NULL;
    for(index=0;index+1U<sizeof(ge_frontend_contract_missions)
            /sizeof(ge_frontend_contract_missions[0]);++index)
        if(ge_frontend_contract_missions[index].mission==frontend->mission)
            return &ge_frontend_contract_missions[index+1U];
    return NULL;
}

static void ge_frontend_select_adjacent_mission(
    GeOriginalFrontendStart *frontend,int direction)
{
    size_t index;
    ptrdiff_t candidate;
    size_t count=sizeof(ge_frontend_contract_missions)
        /sizeof(ge_frontend_contract_missions[0]);
    if(frontend==NULL||direction==0)return;
    for(index=0;index<count;++index)
        if(ge_frontend_contract_missions[index].mission==frontend->mission)
            break;
    candidate=(ptrdiff_t)index+(ptrdiff_t)direction;
    while(candidate>=0&&candidate<(ptrdiff_t)count){
        index=(size_t)candidate;
        if(frontend->services.highest_unlocked_difficulty(
                frontend->services.context,
                ge_frontend_contract_missions[index].mission)
                >=DIFFICULTY_AGENT){
            frontend->mission=ge_frontend_contract_missions[index].mission;
            return;
        }
        candidate+=direction<0?-1:1;
    }
}

static int ge_frontend_highest(const GeOriginalFrontendStart *frontend)
{
    int highest=frontend->services.highest_unlocked_difficulty(
        frontend->services.context,frontend->mission);
    if(highest<DIFFICULTY_AGENT)return -1;
    if(highest>DIFFICULTY_007)highest=DIFFICULTY_007;
    return highest;
}

static void ge_frontend_play_sfx(
    GeOriginalFrontendStart *frontend,uint32_t sfx)
{
    if(frontend->services.play_sfx!=NULL)
        frontend->services.play_sfx(frontend->services.context,sfx);
}

static void ge_frontend_refresh_folder_summaries(
    GeOriginalFrontendStart *frontend)
{
    int folder;
    for(folder=FOLDER1;folder<MAX_FOLDER_COUNT;++folder){
        int32_t mission=SP_LEVEL_DAM-1;
        int32_t difficulty=DIFFICULTY_MULTI;
        int has_progress=0;
        if(frontend->services.folder_summary!=NULL)
            has_progress=frontend->services.folder_summary(
                frontend->services.context,folder,&mission,&difficulty);
        else if(frontend->services.folder_has_progress!=NULL)
            has_progress=frontend->services.folder_has_progress(
                frontend->services.context,folder);
        frontend->folder_has_progress[folder]=(uint8_t)(has_progress!=0);
        frontend->folder_mission[folder]=mission;
        frontend->folder_difficulty[folder]=difficulty;
    }
}

static void ge_frontend_init_current(GeOriginalFrontendStart *frontend)
{
    frontend->menu_timer=0U;
    frontend->sequence_complete=0U;
    if(frontend->current_menu==MENU_FILE_SELECT){
        frontend->file_action=GE_ORIGINAL_FRONTEND_FILE_SELECT;
        frontend->wallet_bounds_ready=0U;
        frontend->wallet_hover=-1;
        frontend->file_action_hover=-1;
        frontend->erase_pending=0U;
        frontend->erase_confirm_selected=0U;
        if(frontend->services.play_music!=NULL)
            frontend->services.play_music(
                frontend->services.context,M_FOLDERS);
        ge_frontend_refresh_folder_summaries(frontend);
    }else if(frontend->current_menu==MENU_MISSION_FAILED){
        /* init_menu0C_missionfailed restores the folders track after the
         * gameplay soundscape has been stopped by the original exit path. */
        if(frontend->services.play_music!=NULL)
            frontend->services.play_music(
                frontend->services.context,M_FOLDERS);
        ge_original_frontend_cursor_set_next_tab(&frontend->cursor);
    }else if(frontend->current_menu==MENU_MISSION_SELECT){
        if(ge_frontend_mission(frontend)==NULL)
            frontend->mission=GE_FRONTEND_DAM_MISSION;
        frontend->stage=LEVELID_NONE;
    }else if(frontend->current_menu==MENU_DIFFICULTY){
        frontend->difficulty=ge_frontend_highest(frontend);
    }else if(frontend->current_menu==MENU_BRIEFING){
        frontend->briefing_page=BRIEFING_TITLE;
    }else if(frontend->current_menu==MENU_RUN_STAGE
            &&!frontend->stage_requested){
        /* Unchanged init_menu0B_runstage ordering. */
        frontend->services.request_stage(
            frontend->services.context,frontend->stage);
        frontend->services.set_selected_difficulty(
            frontend->services.context,frontend->difficulty);
        frontend->stage_requested=1U;
    }
}

/* reload=false branch of unchanged frontChangeMenu. */
static void ge_frontend_front_change_menu(
    GeOriginalFrontendStart *frontend,int32_t menu)
{frontend->maybe_prev_menu=menu;}

/* reload=true branch of unchanged frontChangeMenu. menu_init publishes the
 * four-frame black MENU_SWITCH_SCREENS transition before installing this
 * destination through maybe_prev_menu. */
static void ge_frontend_front_change_menu_reload(
    GeOriginalFrontendStart *frontend,int32_t menu)
{frontend->menu_update=menu;}

/* Relevant maybe_prev_menu branch of unchanged menu_init: init precedes the
 * newly selected menu's interface on the following frontend tick. */
static void ge_frontend_menu_init(GeOriginalFrontendStart *frontend)
{
    if(frontend->menu_update>MENU_INVALID
            &&frontend->current_menu!=MENU_SWITCH_SCREENS){
        if(frontend->current_menu==MENU_GOLDENEYE_LOGO)
            frontend->first_title_visit=0U;
        frontend->current_menu=MENU_SWITCH_SCREENS;
        frontend->menu_timer=0U;
        return;
    }
    if(frontend->maybe_prev_menu>MENU_INVALID){
        if(frontend->current_menu==MENU_GOLDENEYE_LOGO)
            frontend->first_title_visit=0U;
        frontend->current_menu=frontend->maybe_prev_menu;
        frontend->maybe_prev_menu=MENU_INVALID;
        ge_frontend_init_current(frontend);
    }
}

static int ge_frontend_reset(
    GeOriginalFrontendStart *frontend,
    const GeOriginalFrontendServices *services,int canonical_startup)
{
    if(frontend==NULL||services==NULL
            ||services->highest_unlocked_difficulty==NULL
            ||services->select_folder==NULL
            ||services->set_selected_difficulty==NULL
            ||services->request_stage==NULL)return 0;
    memset(frontend,0,sizeof(*frontend));frontend->services=*services;
    frontend->folder=FOLDER1;frontend->mission=GE_FRONTEND_DAM_MISSION;
    frontend->stage=LEVELID_NONE;frontend->difficulty=DIFFICULTY_MULTI;
    frontend->briefing_page=BRIEFING_INVALID;
    frontend->wallet_hover=-1;
    frontend->current_menu=canonical_startup
        ?MENU_LEGAL_SCREEN:MENU_GOLDENEYE_LOGO;
    frontend->maybe_prev_menu=MENU_INVALID;
    frontend->menu_update=MENU_INVALID;
    frontend->first_title_visit=1U;
    frontend->canonical_startup=(uint8_t)(canonical_startup!=0);
    ge_original_frontend_cursor_reset(&frontend->cursor);
    frontend->slider_007_reaction=0.0f;
    frontend->slider_007_health=1.0f;
    frontend->slider_007_damage=1.0f;
    frontend->slider_007_accuracy=1.0f;
    ge_frontend_init_current(frontend);return 1;
}

int ge_original_frontend_start_reset(
    GeOriginalFrontendStart *frontend,
    const GeOriginalFrontendServices *services)
{return ge_frontend_reset(frontend,services,0);}

int ge_original_frontend_start_reset_canonical(
    GeOriginalFrontendStart *frontend,
    const GeOriginalFrontendServices *services)
{return ge_frontend_reset(frontend,services,1);}

int ge_original_frontend_start_tick(
    GeOriginalFrontendStart *frontend,uint32_t input)
{
    int highest;
    if(frontend==NULL)return 0;
    ge_frontend_menu_init(frontend);
    if(frontend->menu_timer<UINT32_MAX)++frontend->menu_timer;
    switch(frontend->current_menu){
    case MENU_SWITCH_SCREENS:
        if(frontend->menu_timer>=4U
                &&frontend->menu_update>MENU_INVALID){
            frontend->maybe_prev_menu=frontend->menu_update;
            frontend->menu_update=MENU_INVALID;
        }
        break;
    case MENU_LEGAL_SCREEN:
        /* interface_menu00_legalscreen, NTSC-U. The first legal-screen
         * visit cannot be skipped because its original first-visit flag is
         * cleared only by update_menu00_legalscreen during transition. */
        if(frontend->menu_timer>=GE_FRONTEND_LEGAL_TIMER_MAX)
            ge_frontend_front_change_menu_reload(
                frontend,MENU_NINTENDO_LOGO);
        else if((input&GE_FRONTEND_ANY_INPUT)
                &&!frontend->first_title_visit)
            ge_frontend_front_change_menu_reload(frontend,MENU_FILE_SELECT);
        break;
    case MENU_NINTENDO_LOGO:
        if(frontend->menu_timer>=GE_FRONTEND_NINTENDO_TIMER_MAX)
            ge_frontend_front_change_menu_reload(
                frontend,MENU_RAREWARE_LOGO);
        else if(input&GE_FRONTEND_ANY_INPUT){
            if(!frontend->first_title_visit)
                ge_frontend_front_change_menu_reload(
                    frontend,MENU_FILE_SELECT);
            else{
                frontend->previous_keypresses=1U;
                ge_frontend_front_change_menu_reload(
                    frontend,MENU_RAREWARE_LOGO);
            }
        }
        break;
    case MENU_RAREWARE_LOGO:
        if(frontend->sequence_complete)
            ge_frontend_front_change_menu_reload(frontend,MENU_EYE_INTRO);
        else if(input&GE_FRONTEND_ANY_INPUT){
            if(!frontend->first_title_visit)
                ge_frontend_front_change_menu_reload(
                    frontend,MENU_FILE_SELECT);
            else{
                frontend->previous_keypresses=1U;
                ge_frontend_front_change_menu_reload(
                    frontend,MENU_EYE_INTRO);
            }
        }
        break;
    case MENU_EYE_INTRO:
        if(frontend->sequence_complete)
            ge_frontend_front_change_menu_reload(
                frontend,MENU_GOLDENEYE_LOGO);
        else if(input&GE_FRONTEND_ANY_INPUT){
            if(!frontend->first_title_visit)
                ge_frontend_front_change_menu_reload(
                    frontend,MENU_FILE_SELECT);
            else{
                frontend->previous_keypresses=1U;
                ge_frontend_front_change_menu_reload(
                    frontend,MENU_GOLDENEYE_LOGO);
            }
        }
        break;
    case MENU_GOLDENEYE_LOGO:
        /* Unchanged first-visit timing from interface_menu04_goldeneyelogo:
         * the first button arms the skip, but the logo remains on screen
         * until its 90-frame minimum. A keypress retained by either preceding
         * authored intro sends the 180-frame timeout to file select; without
         * an earlier input the unchanged path starts the cast roster. */
        if((frontend->logo_button_armed&&frontend->menu_timer>90U)
                ||(frontend->previous_keypresses
                    &&frontend->menu_timer>180U))
            ge_frontend_front_change_menu_reload(frontend,MENU_FILE_SELECT);
        else if(!frontend->previous_keypresses
                &&frontend->menu_timer>180U)
            ge_frontend_front_change_menu_reload(frontend,MENU_DISPLAY_CAST);
        else if(input&(GE_ORIGINAL_FRONTEND_INPUT_CONFIRM
                    |GE_ORIGINAL_FRONTEND_INPUT_START)){
            if(!frontend->first_title_visit||frontend->menu_timer>90U)
                ge_frontend_front_change_menu_reload(
                    frontend,MENU_FILE_SELECT);
            else frontend->logo_button_armed=1U;
        }
        break;
    case MENU_DISPLAY_CAST:
        /* The exact cast scheduler owns this menu's timer/input and reports
         * its frontChangeMenu event through ge_original_frontend_start_cast_event. */
        break;
    case MENU_FILE_SELECT:
        if(input&GE_FRONTEND_ANY_INPUT)frontend->menu_timer=0U;
        if(frontend->erase_pending){
            if(input&GE_ORIGINAL_FRONTEND_INPUT_BACK){
                frontend->erase_pending=0U;
                frontend->erase_confirm_selected=0U;
                if(frontend->services.play_sfx!=NULL)
                    frontend->services.play_sfx(frontend->services.context,
                        GUN_M60AMMGUN_3_SFX);
            }else if(input&(GE_ORIGINAL_FRONTEND_INPUT_LEFT
                            |GE_ORIGINAL_FRONTEND_INPUT_UP)){
                frontend->erase_confirm_selected=0U;
                if(frontend->services.play_sfx!=NULL)
                    frontend->services.play_sfx(frontend->services.context,
                        OPTION_CLICK2_SFX);
            }else if(input&(GE_ORIGINAL_FRONTEND_INPUT_RIGHT
                            |GE_ORIGINAL_FRONTEND_INPUT_DOWN)){
                frontend->erase_confirm_selected=1U;
                if(frontend->services.play_sfx!=NULL)
                    frontend->services.play_sfx(frontend->services.context,
                        OPTION_CLICK2_SFX);
            }else if(input&(GE_ORIGINAL_FRONTEND_INPUT_CONFIRM
                            |GE_ORIGINAL_FRONTEND_INPUT_START)){
                if(frontend->erase_confirm_selected
                        &&frontend->services.erase_folder!=NULL)
                    (void)frontend->services.erase_folder(
                        frontend->services.context,frontend->folder);
                if(frontend->erase_confirm_selected)
                    ge_frontend_refresh_folder_summaries(frontend);
                frontend->erase_pending=0U;
                frontend->erase_confirm_selected=0U;
                if(frontend->services.play_sfx!=NULL)
                    frontend->services.play_sfx(frontend->services.context,
                        GUN_M60AMMGUN_3_SFX);
            }
        }else if((input&GE_ORIGINAL_FRONTEND_INPUT_CONFIRM)
                &&frontend->file_action_bounds_ready
                &&frontend->file_action_hover>=0){
            frontend->file_action=(uint8_t)(frontend->file_action_hover+1);
            ge_frontend_play_sfx(frontend,DOOR_LOCK_SFX);
        }else if(input&GE_ORIGINAL_FRONTEND_INPUT_FILE_COPY){
            frontend->file_action=GE_ORIGINAL_FRONTEND_FILE_COPY;
            if(frontend->services.play_sfx!=NULL)
                frontend->services.play_sfx(frontend->services.context,
                    DOOR_LOCK_SFX);
        }else if(input&GE_ORIGINAL_FRONTEND_INPUT_FILE_ERASE){
            frontend->file_action=GE_ORIGINAL_FRONTEND_FILE_ERASE;
            if(frontend->services.play_sfx!=NULL)
                frontend->services.play_sfx(frontend->services.context,
                    DOOR_LOCK_SFX);
        }else if((input&GE_ORIGINAL_FRONTEND_INPUT_BACK)
                &&frontend->file_action!=GE_ORIGINAL_FRONTEND_FILE_SELECT){
            frontend->file_action=GE_ORIGINAL_FRONTEND_FILE_SELECT;
            if(frontend->services.play_sfx!=NULL)
                frontend->services.play_sfx(frontend->services.context,
                    GUN_M60AMMGUN_3_SFX);
        }else if(!frontend->wallet_bounds_ready
                &&(input&GE_ORIGINAL_FRONTEND_INPUT_LEFT)
                &&frontend->folder>FOLDER1)
            --frontend->folder;
        else if(!frontend->wallet_bounds_ready
                &&(input&GE_ORIGINAL_FRONTEND_INPUT_RIGHT)
                &&frontend->folder<MAX_FOLDER_COUNT-1)
            ++frontend->folder;
        else if(!frontend->wallet_bounds_ready
                &&(input&GE_ORIGINAL_FRONTEND_INPUT_UP)
                &&frontend->folder>=FOLDER1+2)
            frontend->folder-=2;
        else if(!frontend->wallet_bounds_ready
                &&(input&GE_ORIGINAL_FRONTEND_INPUT_DOWN)
                &&frontend->folder+2<MAX_FOLDER_COUNT)
            frontend->folder+=2;
        else if((input&GE_ORIGINAL_FRONTEND_INPUT_CONFIRM)
                &&(!frontend->wallet_bounds_ready
                    ||frontend->wallet_hover>=FOLDER1)){
            if(frontend->file_action==GE_ORIGINAL_FRONTEND_FILE_COPY){
                if(frontend->services.copy_folder_to_first_free!=NULL)
                    (void)frontend->services.copy_folder_to_first_free(
                        frontend->services.context,frontend->folder);
                ge_frontend_refresh_folder_summaries(frontend);
                frontend->file_action=GE_ORIGINAL_FRONTEND_FILE_SELECT;
                if(frontend->services.play_sfx!=NULL)
                    frontend->services.play_sfx(frontend->services.context,
                        COPY_FILE_SFX);
            }else if(frontend->file_action
                    ==GE_ORIGINAL_FRONTEND_FILE_ERASE){
                if(frontend->services.folder_has_progress!=NULL
                        &&frontend->services.folder_has_progress(
                            frontend->services.context,frontend->folder)){
                    frontend->erase_pending=1U;
                    frontend->erase_confirm_selected=0U;
                }
                frontend->file_action=GE_ORIGINAL_FRONTEND_FILE_SELECT;
                if(frontend->services.play_sfx!=NULL)
                    frontend->services.play_sfx(frontend->services.context,
                        OPTION_CLICK2_SFX);
            }else if(frontend->services.select_folder(
                    frontend->services.context,frontend->folder)){
                if(frontend->services.play_sfx!=NULL)
                    frontend->services.play_sfx(frontend->services.context,
                        PAPER_TURN_SFX);
                ge_frontend_front_change_menu(frontend,MENU_MODE_SELECT);
                frontend->cursor.x=126.0f;
                frontend->cursor.y=226.0f;
            }
        }
        if(frontend->menu_timer>=1801U)
            ge_frontend_front_change_menu_reload(frontend,MENU_LEGAL_SCREEN);
        break;
    case MENU_MODE_SELECT:
        if(input&GE_ORIGINAL_FRONTEND_INPUT_BACK){
            ge_frontend_play_sfx(frontend,DOOR_METAL_CLOSE2_SFX);
            ge_frontend_front_change_menu(frontend,MENU_FILE_SELECT);
        }else if(input&(GE_ORIGINAL_FRONTEND_INPUT_CONFIRM
                    |GE_ORIGINAL_FRONTEND_INPUT_START)){
            if(frontend->cursor_previous_tab){
                ge_frontend_play_sfx(frontend,DOOR_METAL_CLOSE2_SFX);
                ge_frontend_front_change_menu(frontend,MENU_FILE_SELECT);
            }else{
                ge_frontend_play_sfx(frontend,DOOR_METAL_CLOSE_SFX);
                ge_frontend_front_change_menu(frontend,MENU_MISSION_SELECT);
                ge_original_frontend_cursor_set_mission(
                    &frontend->cursor,frontend->mission);
            }
        }
        break;
    case MENU_MISSION_SELECT:
        if(input&GE_ORIGINAL_FRONTEND_INPUT_BACK){
            ge_frontend_play_sfx(frontend,DOOR_METAL_CLOSE2_SFX);
            ge_frontend_front_change_menu(frontend,MENU_MODE_SELECT);
        }
        else if(input&GE_ORIGINAL_FRONTEND_INPUT_LEFT)
            ge_frontend_select_adjacent_mission(frontend,-1);
        else if(input&GE_ORIGINAL_FRONTEND_INPUT_RIGHT)
            ge_frontend_select_adjacent_mission(frontend,1);
        else if(input&GE_ORIGINAL_FRONTEND_INPUT_UP)
            ge_frontend_select_adjacent_mission(frontend,-5);
        else if(input&GE_ORIGINAL_FRONTEND_INPUT_DOWN)
            ge_frontend_select_adjacent_mission(frontend,5);
        else if((input&(GE_ORIGINAL_FRONTEND_INPUT_CONFIRM
                    |GE_ORIGINAL_FRONTEND_INPUT_START))
                &&ge_frontend_highest(frontend)>=DIFFICULTY_AGENT){
            if(frontend->cursor_previous_tab){
                ge_frontend_play_sfx(frontend,DOOR_METAL_CLOSE2_SFX);
                ge_frontend_front_change_menu(frontend,MENU_MODE_SELECT);
                frontend->cursor.x=126.0f;
                frontend->cursor.y=226.0f;
                break;
            }
            const GeFrontendMissionContract *mission=
                ge_frontend_mission(frontend);
            if(mission==NULL)break;
            frontend->stage=mission->stage;
            ge_frontend_play_sfx(frontend,DOOR_METAL_CLOSE2_SFX);
            ge_frontend_front_change_menu(frontend,MENU_DIFFICULTY);
            ge_original_frontend_cursor_set_difficulty(
                &frontend->cursor,ge_frontend_highest(frontend));
        }
        break;
    case MENU_DIFFICULTY:
        highest=ge_frontend_highest(frontend);
        if(input&GE_ORIGINAL_FRONTEND_INPUT_BACK){
            ge_frontend_play_sfx(frontend,DOOR_METAL_CLOSE2_SFX);
            ge_frontend_front_change_menu(frontend,MENU_MISSION_SELECT);
        }
        else if((input&GE_ORIGINAL_FRONTEND_INPUT_UP)
                &&frontend->difficulty>DIFFICULTY_AGENT)
            --frontend->difficulty;
        else if((input&GE_ORIGINAL_FRONTEND_INPUT_DOWN)
                &&frontend->difficulty<highest)
            ++frontend->difficulty;
        else if((input&(GE_ORIGINAL_FRONTEND_INPUT_CONFIRM
                    |GE_ORIGINAL_FRONTEND_INPUT_START))
                &&frontend->difficulty>=DIFFICULTY_AGENT)
        {
            if(frontend->cursor_previous_tab){
                ge_frontend_play_sfx(frontend,DOOR_METAL_CLOSE2_SFX);
                ge_frontend_front_change_menu(frontend,MENU_MISSION_SELECT);
                ge_original_frontend_cursor_set_mission(
                    &frontend->cursor,frontend->mission);
                break;
            }
            ge_frontend_play_sfx(frontend,PAPER_TURN_SFX);
            ge_original_frontend_cursor_set_next_tab(&frontend->cursor);
            ge_frontend_front_change_menu(frontend,
                frontend->difficulty==DIFFICULTY_007
                    ?MENU_007_OPTIONS:MENU_BRIEFING);
        }
        break;
    case MENU_007_OPTIONS:
        if(input&GE_ORIGINAL_FRONTEND_INPUT_BACK){
            ge_frontend_play_sfx(frontend,DOOR_METAL_CLOSE2_SFX);
            ge_frontend_front_change_menu(frontend,MENU_DIFFICULTY);
            ge_original_frontend_cursor_set_difficulty(
                &frontend->cursor,frontend->difficulty);
        }else if(input&GE_ORIGINAL_FRONTEND_INPUT_START){
            ge_frontend_play_sfx(frontend,DOOR_METAL_CLOSE2_SFX);
            ge_frontend_front_change_menu_reload(frontend,MENU_RUN_STAGE);
        }else if(input&GE_ORIGINAL_FRONTEND_INPUT_CONFIRM){
            if(frontend->cursor_previous_tab){
                ge_frontend_play_sfx(frontend,DOOR_METAL_CLOSE2_SFX);
                ge_frontend_front_change_menu(frontend,MENU_DIFFICULTY);
                ge_original_frontend_cursor_set_difficulty(
                    &frontend->cursor,frontend->difficulty);
            }else if(frontend->cursor.x>390.0f
                    &&frontend->cursor.y<=130.5f){
                ge_frontend_play_sfx(frontend,DOOR_METAL_CLOSE2_SFX);
                ge_frontend_front_change_menu_reload(
                    frontend,MENU_RUN_STAGE);
            }else if(ge_original_frontend_cursor_on_next_tab(
                        &frontend->cursor)
                    ||frontend->cursor.y<164.0f){
                ge_frontend_play_sfx(frontend,DOOR_METAL_CLOSE2_SFX);
                ge_frontend_front_change_menu(frontend,MENU_BRIEFING);
                ge_original_frontend_cursor_set_next_tab(&frontend->cursor);
            }
        }
        break;
    case MENU_BRIEFING:
        if((input&(GE_ORIGINAL_FRONTEND_INPUT_CONFIRM
                    |GE_ORIGINAL_FRONTEND_INPUT_START))
                &&frontend->cursor_previous_tab){
            ge_frontend_play_sfx(frontend,DOOR_METAL_CLOSE2_SFX);
            if(frontend->briefing_page>BRIEFING_TITLE)
                --frontend->briefing_page;
            else{
                ge_frontend_front_change_menu(frontend,MENU_MISSION_SELECT);
                ge_original_frontend_cursor_set_mission(
                    &frontend->cursor,frontend->mission);
            }
        }else if(input&GE_ORIGINAL_FRONTEND_INPUT_START){
            ge_frontend_play_sfx(frontend,DOOR_METAL_CLOSE2_SFX);
            ge_frontend_front_change_menu_reload(frontend,MENU_RUN_STAGE);
        }
        else if(input&GE_ORIGINAL_FRONTEND_INPUT_BACK){
            ge_frontend_play_sfx(frontend,DOOR_METAL_CLOSE2_SFX);
            if(frontend->briefing_page>BRIEFING_TITLE)
                --frontend->briefing_page;
            else ge_frontend_front_change_menu(frontend,MENU_MISSION_SELECT);
        }else if(input&GE_ORIGINAL_FRONTEND_INPUT_CONFIRM){
            ge_frontend_play_sfx(frontend,DOOR_METAL_CLOSE2_SFX);
            if(frontend->briefing_page<BRIEFING_MONEYPENNY)
                ++frontend->briefing_page;
            else ge_frontend_front_change_menu_reload(
                frontend,MENU_RUN_STAGE);
        }
        break;
    case MENU_MISSION_FAILED:
        if(input&GE_ORIGINAL_FRONTEND_INPUT_BACK){
            ge_frontend_play_sfx(frontend,DOOR_METAL_CLOSE2_SFX);
            ge_frontend_front_change_menu(frontend,MENU_MISSION_SELECT);
        }
        else if(input&(GE_ORIGINAL_FRONTEND_INPUT_CONFIRM
                    |GE_ORIGINAL_FRONTEND_INPUT_START)){
            ge_frontend_play_sfx(frontend,DOOR_METAL_CLOSE2_SFX);
            if(frontend->cursor_previous_tab){
                ge_frontend_front_change_menu(frontend,MENU_MISSION_SELECT);
                ge_original_frontend_cursor_set_mission(
                    &frontend->cursor,frontend->mission);
            }else ge_frontend_front_change_menu(
                frontend,MENU_MISSION_COMPLETE);
        }
        break;
    case MENU_MISSION_COMPLETE:
        if(input&GE_ORIGINAL_FRONTEND_INPUT_BACK){
            ge_frontend_play_sfx(frontend,DOOR_METAL_CLOSE2_SFX);
            ge_frontend_front_change_menu(frontend,MENU_MISSION_SELECT);
        }
        else if(input&(GE_ORIGINAL_FRONTEND_INPUT_CONFIRM
                    |GE_ORIGINAL_FRONTEND_INPUT_START)){
            ge_frontend_play_sfx(frontend,DOOR_METAL_CLOSE2_SFX);
            if(frontend->cursor_previous_tab){
                ge_frontend_front_change_menu(frontend,MENU_MISSION_SELECT);
                ge_original_frontend_cursor_set_mission(
                    &frontend->cursor,frontend->mission);
                break;
            }
            if(!frontend->result.all_objectives_complete_alive
                    ||frontend->result.append_cheat_single_player)
                ge_frontend_front_change_menu(frontend,MENU_BRIEFING);
            else if(frontend->mission==SP_LEVEL_CRADLE){
                /* Exact interface_menu0D_missioncomplete campaign ending:
                 * Cradle continues into the Cuba credits stage rather than
                 * the bonus mission entries which follow it in the grid. */
                frontend->stage=LEVELID_CUBA;
                ge_frontend_front_change_menu_reload(
                    frontend,MENU_RUN_STAGE);
            }else if(frontend->mission>=SP_LEVEL_AZTEC){
                /* Aztec and Egypt are bonus missions.  Their successful Next
                 * tab returns to the mission grid instead of wrapping or
                 * requesting a non-existent subsequent briefing. */
                ge_frontend_front_change_menu(frontend,MENU_MISSION_SELECT);
            }else{
                const GeFrontendMissionContract *next=
                    ge_frontend_next_mission(frontend);
                /* Unchanged interface_menu0D_missioncomplete advances from
                 * the mission-folder entry to the next real stage entry,
                 * installs its selected_stage, then opens that briefing. */
                if(next==NULL)return 0;
                frontend->mission=next->mission;
                frontend->stage=next->stage;
                ge_frontend_front_change_menu(frontend,MENU_BRIEFING);
                ge_original_frontend_cursor_set_next_tab(&frontend->cursor);
            }
        }
        break;
    case MENU_RUN_STAGE:break;
    default:return 0;
    }
    return 1;
}

int ge_original_frontend_start_cursor_tick(
    GeOriginalFrontendStart *frontend,int8_t stick_x,int8_t stick_y,
    float timer_delta)
{
    uint8_t unlocked[20]={0};
    int32_t selected;
    int32_t index;
    if(frontend==NULL)return 0;
    ge_original_frontend_cursor_tick(
        &frontend->cursor,stick_x,stick_y,timer_delta);
    if(frontend->current_menu==MENU_FILE_SELECT
            &&frontend->wallet_bounds_ready){
        frontend->wallet_hover=(int8_t)ge_original_frontend_cursor_wallet(
            &frontend->cursor,frontend->wallet_bounds);
        if(frontend->wallet_hover>=FOLDER1)
            frontend->folder=frontend->wallet_hover;
    }
    if(frontend->current_menu==MENU_FILE_SELECT
            &&frontend->file_action_bounds_ready){
        int action;
        frontend->file_action_hover=-1;
        for(action=0;action<2;++action)
            if(frontend->file_action_bounds[action].left<=frontend->cursor.x
                    &&frontend->cursor.x
                        <=frontend->file_action_bounds[action].right
                    &&frontend->file_action_bounds[action].top
                        <=frontend->cursor.y
                    &&frontend->cursor.y
                        <=frontend->file_action_bounds[action].bottom){
                frontend->file_action_hover=(int8_t)action;
                break;
            }
    }
    frontend->cursor_previous_tab=(uint8_t)
        ge_original_frontend_cursor_on_previous_tab(&frontend->cursor);
    if(frontend->current_menu==MENU_MISSION_SELECT){
        for(index=0;index<20;++index)
            unlocked[index]=(uint8_t)(frontend->services
                .highest_unlocked_difficulty(frontend->services.context,index)
                    >=DIFFICULTY_AGENT);
        selected=ge_original_frontend_cursor_mission(
            &frontend->cursor,unlocked);
        if(selected>=0)frontend->mission=selected;
    }else if(frontend->current_menu==MENU_DIFFICULTY){
        selected=ge_original_frontend_cursor_difficulty(
            &frontend->cursor,ge_frontend_highest(frontend));
        if(selected>=DIFFICULTY_AGENT)frontend->difficulty=selected;
    }
    return 1;
}

int ge_original_frontend_start_set_wallet_bounds(
    GeOriginalFrontendStart *frontend,
    const GeOriginalFrontendWalletBounds bounds[4])
{
    if(frontend==NULL||bounds==NULL)return 0;
    memcpy(frontend->wallet_bounds,bounds,sizeof(frontend->wallet_bounds));
    frontend->wallet_bounds_ready=1U;
    frontend->wallet_hover=(int8_t)ge_original_frontend_cursor_wallet(
        &frontend->cursor,frontend->wallet_bounds);
    if(frontend->wallet_hover>=FOLDER1)
        frontend->folder=frontend->wallet_hover;
    return 1;
}

int ge_original_frontend_start_set_file_action_bounds(
    GeOriginalFrontendStart *frontend,
    const GeOriginalFrontendWalletBounds bounds[2])
{
    int action;
    if(frontend==NULL||bounds==NULL)return 0;
    memcpy(frontend->file_action_bounds,bounds,
        sizeof(frontend->file_action_bounds));
    frontend->file_action_bounds_ready=1U;
    frontend->file_action_hover=-1;
    for(action=0;action<2;++action)
        if(frontend->file_action_bounds[action].left<=frontend->cursor.x
                &&frontend->cursor.x
                    <=frontend->file_action_bounds[action].right
                &&frontend->file_action_bounds[action].top
                    <=frontend->cursor.y
                &&frontend->cursor.y
                    <=frontend->file_action_bounds[action].bottom){
            frontend->file_action_hover=(int8_t)action;
            break;
        }
    return 1;
}

int ge_original_frontend_start_mission_caption(
    int32_t mission,const char **chapter_number,const char **part_number)
{
    size_t index;
    if(chapter_number==NULL||part_number==NULL)return 0;
    for(index=0;index<sizeof(ge_frontend_contract_missions)
            /sizeof(ge_frontend_contract_missions[0]);++index)
        if(ge_frontend_contract_missions[index].mission==mission){
            *chapter_number=
                ge_frontend_contract_missions[index].chapter_number;
            *part_number=ge_frontend_contract_missions[index].part_number;
            return 1;
        }
    return 0;
}

int ge_original_frontend_start_007_drag(
    GeOriginalFrontendStart *frontend,int confirm_held)
{
    float value;
    int y;
    if(frontend==NULL)return 0;
    if(frontend->current_menu!=MENU_007_OPTIONS||!confirm_held)return 1;
    if(ge_original_frontend_cursor_on_previous_tab(&frontend->cursor)
            ||ge_original_frontend_cursor_on_next_tab(&frontend->cursor)
            ||(frontend->cursor.x>390.0f&&frontend->cursor.y<=130.5f))
        return 1;
    value=(frontend->cursor.x-55.0f)/300.0f;
    if(value>1.0f)value=1.0f;
    if(value<0.0f)value=0.0f;
    y=(int)frontend->cursor.y;
    if(y>=0x107)frontend->slider_007_reaction=value;
    else if(y>=0xe6)frontend->slider_007_accuracy=value*value*10.0f;
    else if(y>=0xc5)frontend->slider_007_damage=value*value*10.0f;
    else if(y>=0xa4)frontend->slider_007_health=value*value*10.0f;
    if(frontend->services.set_007_sliders!=NULL)
        frontend->services.set_007_sliders(frontend->services.context,
            frontend->slider_007_reaction,frontend->slider_007_health,
            frontend->slider_007_damage,frontend->slider_007_accuracy);
    return 1;
}

int ge_original_frontend_start_sequence_complete(
    GeOriginalFrontendStart *frontend)
{
    if(frontend==NULL
            ||(frontend->current_menu!=MENU_RAREWARE_LOGO
                &&frontend->current_menu!=MENU_EYE_INTRO))return 0;
    frontend->sequence_complete=1U;
    return 1;
}

int ge_original_frontend_start_cast_event(
    GeOriginalFrontendStart *frontend,GeOriginalFrontendCastEvent event)
{
    if(frontend==NULL||frontend->current_menu!=MENU_DISPLAY_CAST)return 0;
    switch(event){
    case GE_ORIGINAL_FRONTEND_CAST_EVENT_NONE:
    case GE_ORIGINAL_FRONTEND_CAST_EVENT_RELOAD:
        return 1;
    case GE_ORIGINAL_FRONTEND_CAST_EVENT_FILE_SELECT:
        ge_frontend_front_change_menu_reload(frontend,MENU_FILE_SELECT);
        return 1;
    case GE_ORIGINAL_FRONTEND_CAST_EVENT_MISSION_SELECT:
        /* End-credit cast path from interface_menu18_displaycast. */
        frontend->mission=SP_LEVEL_CRADLE;
        ge_frontend_front_change_menu_reload(frontend,MENU_MISSION_SELECT);
        return 1;
    case GE_ORIGINAL_FRONTEND_CAST_EVENT_RAMROM:
    default:
        /* select_ramrom_to_play needs the authored demo-file service. */
        return 0;
    }
}

int ge_original_frontend_start_stage_ended(
    GeOriginalFrontendStart *frontend,
    const GeOriginalFrontendMissionResult *result)
{
    if(frontend==NULL||result==NULL
            ||frontend->current_menu!=MENU_RUN_STAGE
            ||!frontend->stage_requested)return 0;
    frontend->result=*result;frontend->result_valid=1U;
    frontend->stage_requested=0U;
    ge_frontend_front_change_menu_reload(frontend,MENU_MISSION_FAILED);
    return 1;
}

static void ge_line(GeOriginalFrontendSnapshot *snapshot,uint16_t text,
                    int selected,int objective)
{
    GeOriginalFrontendTextLine *line;
    if(snapshot->line_count>=GE_ORIGINAL_FRONTEND_MAX_LINES)return;
    line=&snapshot->lines[snapshot->line_count++];line->text_id=text;
    line->selected=(uint8_t)(selected!=0);line->objective=(uint8_t)(objective!=0);
    line->status=0U;
}

static void ge_tab_line(GeOriginalFrontendSnapshot *snapshot,uint16_t text,
                        uint8_t tab,int selected)
{
    GeOriginalFrontendTextLine *line;
    if(snapshot->tab_count>=3U)return;
    line=&snapshot->tabs[snapshot->tab_count++];
    memset(line,0,sizeof(*line));
    line->text_id=text;line->selected=(uint8_t)(selected!=0);
    line->status=tab;
}

static uint8_t ge_frontend_clamp_u8(int value)
{
    if(value<0)return 0U;
    if(value>255)return 255U;
    return (uint8_t)value;
}

static void ge_frontend_presentation(
    const GeOriginalFrontendStart *frontend,
    GeOriginalFrontendPresentation *presentation)
{
    uint32_t frame;
    memset(presentation,0,sizeof(*presentation));
    presentation->model_prop=-1;
    presentation->frame=frontend->menu_timer;
    /* menu_init selects the authored 440x330 frontend framebuffer/view while
     * the startup constructors retain their explicit 4:3 perspective. */
    presentation->logical_width=440U;
    presentation->logical_height=330U;
    presentation->projection_aspect=320.0f/240.0f;
    presentation->projection_fov_y_degrees=60.0f;
    presentation->projection_near=100.0f;
    presentation->projection_far=10000.0f;
    presentation->camera_up[1]=1.0f;
    switch(frontend->current_menu){
    case MENU_LEGAL_SCREEN:
        presentation->startup_active=1U;
        presentation->renderer=GE_ORIGINAL_FRONTEND_RENDERER_PITEM_MODEL;
        presentation->model_prop=PROP_LEGALPAGE;
        presentation->duration_frames=GE_FRONTEND_LEGAL_TIMER_MAX;
        presentation->opacity=255U;
        presentation->model_uniform_scale=1.0f;
        presentation->camera_eye_z=4000.0f;
        presentation->camera_eye[2]=4000.0f;
        presentation->model_cull_both=1U;
        presentation->model_uses_authored_origin=1U;
        break;
    case MENU_NINTENDO_LOGO:
        presentation->startup_active=1U;
        presentation->renderer=GE_ORIGINAL_FRONTEND_RENDERER_PITEM_MODEL;
        presentation->model_prop=PROP_NINTENDOLOGO;
        presentation->duration_frames=GE_FRONTEND_NINTENDO_TIMER_MAX;
        presentation->opacity=255U;
        presentation->model_uniform_scale=1.0f;
        presentation->camera_eye_z=4000.0f;
        presentation->camera_eye[2]=4000.0f;
        presentation->model_cull_both=1U;
        presentation->model_uses_authored_origin=1U;
        /* constructor_menu01_nintendo advances rotation before use, then
         * advances scale after use.  interface_menu01_nintendo has already
         * advanced g_MenuTimer when this snapshot is produced. */
        frame=frontend->menu_timer>0U?frontend->menu_timer-1U:0U;
        presentation->nintendo_rotation_radians=-1.39626348019f
            +(float)(frame+1U)*0.017453292f;
        presentation->nintendo_scale=0.0183333326131f;
        while(frame-- >0U&&presentation->nintendo_scale<1.1f){
            presentation->nintendo_scale*=1.07977f;
            if(presentation->nintendo_scale>1.1f)
                presentation->nintendo_scale=1.1f;
        }
        presentation->nintendo_ambient=ge_frontend_clamp_u8(
            255-(int)(((int64_t)frontend->menu_timer*255-94350)/100));
        break;
    case MENU_RAREWARE_LOGO:
        presentation->startup_active=1U;
        presentation->renderer=GE_ORIGINAL_FRONTEND_RENDERER_RAREWARE;
        presentation->duration_frames=290U;
        presentation->projection_far=5000.0f;
        presentation->camera_eye_z=880.0f;
        presentation->camera_target_z=879.0f;
        presentation->camera_eye[2]=880.0f;
        presentation->camera_target[2]=879.0f;
        presentation->model_cull_back=1U;
        presentation->model_lighting_enabled=1U;
        presentation->model_texture_gen_enabled=1U;
        presentation->model_smooth_shading_enabled=1U;
        frame=frontend->menu_timer>0U?frontend->menu_timer-1U:0U;
        {
            const int fade_in=ge_frontend_clamp_u8((int)(frame*255U/70U));
            const int fade_out=ge_frontend_clamp_u8(
                255-(int)(((int64_t)frame*255-40800)/70));
            presentation->opacity=(uint8_t)(fade_in*fade_out/255);
        }
        presentation->rareware_light_ambient=presentation->opacity;
        presentation->rareware_light_diffuse=255U;
        presentation->rareware_primary_rgb[0]=presentation->opacity;
        presentation->rareware_primary_rgb[1]=presentation->opacity;
        presentation->rareware_primary_rgb[2]=presentation->opacity;
        presentation->rareware_secondary_rgb[0]=(uint8_t)(
            (uint32_t)presentation->opacity*0xf0U/0xffU);
        presentation->rareware_secondary_rgb[1]=(uint8_t)(
            (uint32_t)presentation->opacity*0xd0U/0xffU);
        presentation->rareware_secondary_rgb[2]=(uint8_t)(
            (uint32_t)presentation->opacity*0xf0U/0xffU);
        presentation->rareware_rotation_degrees=-40.0f+(float)frame*2.0f;
        break;
    case MENU_EYE_INTRO:
        presentation->startup_active=1U;
        presentation->renderer=GE_ORIGINAL_FRONTEND_RENDERER_GUNBARREL;
        presentation->opacity=255U;
        presentation->projection_fov_y_degrees=46.0f;
        presentation->projection_near=10.0f;
        presentation->camera_eye_z=684.28143f;
        presentation->camera_target_z=684.52143f;
        presentation->camera_eye[0]=1758.2957f;
        presentation->camera_eye[1]=220.0f;
        presentation->camera_eye[2]=684.28143f;
        presentation->camera_target[0]=1758.2957f-0.97f;
        presentation->camera_target[1]=220.0f;
        presentation->camera_target[2]=684.28143f+0.24f;
        break;
    case MENU_GOLDENEYE_LOGO:
        presentation->startup_active=(uint8_t)(frontend->canonical_startup!=0);
        presentation->renderer=GE_ORIGINAL_FRONTEND_RENDERER_PITEM_MODEL;
        presentation->model_prop=PROP_GOLDENEYELOGO;
        presentation->duration_frames=180U;
        presentation->opacity=255U;
        presentation->model_uniform_scale=1.2f;
        presentation->camera_eye_z=3000.0f;
        presentation->camera_eye[2]=3000.0f;
        presentation->reflection_camera_eye_z=4000.0f;
        presentation->model_cull_both=1U;
        presentation->model_lighting_enabled=1U;
        presentation->model_texture_gen_enabled=1U;
        presentation->model_uses_authored_origin=1U;
        presentation->title_texture_gen=1U;
        presentation->title_light_ambient=0x96U;
        presentation->title_light_diffuse=0xffU;
        presentation->title_light_direction[0]=77;
        presentation->title_light_direction[1]=77;
        presentation->title_light_direction[2]=46;
        break;
    default:break;
    }
}

int ge_original_frontend_start_snapshot(
    const GeOriginalFrontendStart *frontend,
    GeOriginalFrontendSnapshot *snapshot)
{
    const GeFrontendMissionContract *mission;
    int index;
    if(frontend==NULL||snapshot==NULL)return 0;
    memset(snapshot,0,sizeof(*snapshot));snapshot->menu=frontend->current_menu;
    snapshot->folder=frontend->folder;snapshot->mission=frontend->mission;
    snapshot->stage=frontend->stage;snapshot->difficulty=frontend->difficulty;
    snapshot->briefing_page=frontend->briefing_page;
    snapshot->stage_requested=frontend->stage_requested;
    snapshot->result_valid=frontend->result_valid;
    snapshot->file_action=frontend->file_action;
    snapshot->erase_pending=frontend->erase_pending;
    snapshot->erase_confirm_selected=frontend->erase_confirm_selected;
    snapshot->result=frontend->result;
    snapshot->cursor=frontend->cursor;
    snapshot->slider_007_reaction=frontend->slider_007_reaction;
    snapshot->slider_007_health=frontend->slider_007_health;
    snapshot->slider_007_damage=frontend->slider_007_damage;
    snapshot->slider_007_accuracy=frontend->slider_007_accuracy;
    ge_frontend_presentation(frontend,&snapshot->presentation);
    mission=ge_frontend_mission(frontend);
    if(mission!=NULL){
        snapshot->chapter_number=mission->chapter_number;
        snapshot->part_number=mission->part_number;
        snapshot->chapter_title=mission->chapter_title;
        snapshot->part_title=mission->title;
        if(frontend->difficulty>=DIFFICULTY_AGENT
                &&frontend->difficulty<=DIFFICULTY_007)
            snapshot->difficulty_title=
                ge_frontend_contract_difficulty_text[frontend->difficulty];
    }
    switch(frontend->current_menu){
    case MENU_SWITCH_SCREENS:break;
    case MENU_LEGAL_SCREEN:
    {
        static const int16_t positions[12][2]={
            {220,30},{34,83},{226,84},{226,97},{226,110},{226,122},
            {227,134},{219,211},{60,169},{60,201},{99,266},{80,280},
        };
        static const uint8_t horizontal_align[12]={
            CENTER_ALIGN,LEFT_ALIGN,LEFT_ALIGN,LEFT_ALIGN,LEFT_ALIGN,
            LEFT_ALIGN,LEFT_ALIGN,LEFT_ALIGN,LEFT_ALIGN,LEFT_ALIGN,
            LEFT_ALIGN,LEFT_ALIGN,
        };
        for(index=TITLE_STR_07_TWY;index<=TITLE_STR_18_EMI;++index){
            const size_t line_index=snapshot->line_count;
            const size_t legal_index=(size_t)(index-TITLE_STR_07_TWY);
            ge_line(snapshot,getStringID(LTITLE,index),0,0);
            snapshot->lines[line_index].x=positions[legal_index][0];
            snapshot->lines[line_index].y=positions[legal_index][1];
            snapshot->lines[line_index].horizontal_align=
                horizontal_align[legal_index];
            snapshot->lines[line_index].vertical_align=CENTER_ALIGN;
            snapshot->lines[line_index].has_authored_position=1U;
        }
        break;
    }
    case MENU_NINTENDO_LOGO:
    case MENU_RAREWARE_LOGO:
    case MENU_EYE_INTRO:
    case MENU_DISPLAY_CAST:
        break;
    case MENU_GOLDENEYE_LOGO:
        ge_line(snapshot,getStringID(LTITLE,TITLE_STR_04_START),1,0);break;
    case MENU_FILE_SELECT:
        for(index=FOLDER1;index<MAX_FOLDER_COUNT;++index){
            snapshot->folder_has_progress[index]=
                frontend->folder_has_progress[index];
            snapshot->folder_mission[index]=frontend->folder_mission[index];
            snapshot->folder_difficulty[index]=
                frontend->folder_difficulty[index];
        }
        if(frontend->erase_pending){
            ge_line(snapshot,getStringID(LTITLE,TITLE_STR_23_ERASEFILE),0,0);
            snapshot->lines[0].status=(uint8_t)(frontend->folder-FOLDER1);
            ge_line(snapshot,getStringID(LTITLE,TITLE_STR_24_CANCEL),
                !frontend->erase_confirm_selected,0);
            ge_line(snapshot,getStringID(LTITLE,TITLE_STR_25_CONFIRM),
                frontend->erase_confirm_selected,0);
        }else{
            /* constructor_menu05_fileselect draws all three authored action
             * labels below the four wallets.  The status byte on line zero
             * remains the platform renderer's selected-wallet channel. */
            ge_line(snapshot,getStringID(LTITLE,TITLE_STR_25_CONFIRM),
                frontend->file_action==GE_ORIGINAL_FRONTEND_FILE_SELECT,0);
            snapshot->lines[0].status=(uint8_t)(frontend->folder-FOLDER1);
            ge_line(snapshot,getStringID(LTITLE,TITLE_STR_27_COPY),
                frontend->file_action==GE_ORIGINAL_FRONTEND_FILE_COPY,0);
            ge_line(snapshot,getStringID(LTITLE,TITLE_STR_28_ERASE),
                frontend->file_action==GE_ORIGINAL_FRONTEND_FILE_ERASE,0);
        }
        break;
    case MENU_MODE_SELECT:
        /* constructor_menu06_modesel uses the full SELECT MISSION label;
         * TITLE_STR_26 is only the file-card "Mission " prefix. */
        ge_line(snapshot,getStringID(LTITLE,TITLE_STR_29_SELECTMISSION),1,0);
        ge_line(snapshot,getStringID(LTITLE,TITLE_STR_30_MULTIPLAYER),0,0);
        ge_tab_line(snapshot,getStringID(LTITLE,TITLE_STR_06_PREVIOUS),3U,
            frontend->cursor_previous_tab);break;
    case MENU_MISSION_SELECT:
        ge_line(snapshot,getStringID(LTITLE,TITLE_STR_29_SELECTMISSION),0,0);
        for(index=0;index<(int)(sizeof(ge_frontend_contract_missions)
                /sizeof(ge_frontend_contract_missions[0]));++index){
            const int unlocked=frontend->services.highest_unlocked_difficulty(
                    frontend->services.context,
                    ge_frontend_contract_missions[index].mission)
                    >=DIFFICULTY_AGENT;
            ge_line(snapshot,
                ge_frontend_contract_missions[index].grid_title,
                ge_frontend_contract_missions[index].mission
                    ==frontend->mission,0);
            snapshot->lines[snapshot->line_count-1U].status=
                unlocked?0U:3U;
        }
        ge_tab_line(snapshot,getStringID(LTITLE,TITLE_STR_06_PREVIOUS),3U,
            frontend->cursor_previous_tab);
        break;
    case MENU_DIFFICULTY:
        ge_line(snapshot,getStringID(LTITLE,TITLE_STR_35_DIFFICULTY),0,0);
        for(index=DIFFICULTY_AGENT;index<=ge_frontend_highest(frontend);++index)
            ge_line(snapshot,ge_frontend_contract_difficulty_text[index],
                index==frontend->difficulty,0);
        ge_tab_line(snapshot,getStringID(LTITLE,TITLE_STR_06_PREVIOUS),3U,
            frontend->cursor_previous_tab);
        break;
    case MENU_007_OPTIONS:
        if(mission==NULL)return 0;
        ge_line(snapshot,mission->title,0,0);
        ge_line(snapshot,getStringID(LTITLE,TITLE_STR_40_SPECOPS),0,0);
        ge_line(snapshot,getStringID(LTITLE,TITLE_STR_42_HEALTH),0,0);
        snapshot->lines[2].value=frontend->slider_007_health;
        ge_line(snapshot,getStringID(LTITLE,TITLE_STR_43_DAMAGE),0,0);
        snapshot->lines[3].value=frontend->slider_007_damage;
        ge_line(snapshot,getStringID(LTITLE,TITLE_STR_44_ACCURACY),0,0);
        snapshot->lines[4].value=frontend->slider_007_accuracy;
        ge_line(snapshot,getStringID(LTITLE,TITLE_STR_41_REACTION),0,0);
        snapshot->lines[5].value=frontend->slider_007_reaction;
        ge_tab_line(snapshot,getStringID(LTITLE,TITLE_STR_04_START),1U,
            frontend->cursor.x>390.0f&&frontend->cursor.y<=130.5f);
        ge_tab_line(snapshot,getStringID(LTITLE,TITLE_STR_05_NEXT),2U,
            ge_original_frontend_cursor_on_next_tab(&frontend->cursor));
        ge_tab_line(snapshot,getStringID(LTITLE,TITLE_STR_06_PREVIOUS),3U,
            frontend->cursor_previous_tab);
        break;
    case MENU_BRIEFING:
        if(mission==NULL)return 0;
        ge_line(snapshot,mission->title,0,0);
        ge_line(snapshot,ge_frontend_contract_briefing_heading[
            frontend->briefing_page],0,0);
        if(frontend->briefing_page==BRIEFING_TITLE){
            for(index=0;index<mission->objective_count;++index)
                if(frontend->difficulty
                        >=mission->objective_difficulty[index])
                    ge_line(snapshot,mission->objective_text[index],0,1);
        }else ge_line(snapshot,getStringID(
            mission->text_bank,frontend->briefing_page-1),0,0);
        ge_tab_line(snapshot,getStringID(LTITLE,TITLE_STR_04_START),1U,
            frontend->cursor.x>390.0f&&frontend->cursor.y<=130.5f);
        if(frontend->briefing_page<BRIEFING_MONEYPENNY)
            ge_tab_line(snapshot,getStringID(LTITLE,TITLE_STR_05_NEXT),2U,
                ge_original_frontend_cursor_on_next_tab(&frontend->cursor));
        ge_tab_line(snapshot,getStringID(LTITLE,TITLE_STR_06_PREVIOUS),3U,
            frontend->cursor_previous_tab);
        break;
    case MENU_MISSION_FAILED:
        if(mission==NULL)return 0;
        ge_line(snapshot,mission->title,0,0);
        ge_line(snapshot,getStringID(LTITLE,TITLE_STR_98_REPORT),0,0);
        ge_line(snapshot,getStringID(LTITLE,TITLE_STR_99_MISSIONSTATUS),0,0);
        if(frontend->result.bond_kia)
            ge_line(snapshot,getStringID(LTITLE,TITLE_STR_100_KIA),0,0);
        else if(frontend->result.mission_failed_or_aborted)
            ge_line(snapshot,getStringID(LTITLE,TITLE_STR_101_ABORTED),0,0);
        else if(frontend->result.all_objectives_complete_alive)
            ge_line(snapshot,getStringID(LTITLE,TITLE_STR_102_COMPLETED),0,0);
        else ge_line(snapshot,getStringID(LTITLE,TITLE_STR_103_FAILED),0,0);
        snapshot->lines[3].status=(uint8_t)(
            frontend->result.all_objectives_complete_alive
                ?OBJECTIVESTATUS_COMPLETE:OBJECTIVESTATUS_FAILED);
        for(index=0;index<mission->objective_count;++index)
            if(frontend->difficulty
                    >=mission->objective_difficulty[index]){
                ge_line(snapshot,mission->objective_text[index],0,1);
                snapshot->lines[snapshot->line_count-1U].status=
                    frontend->result.objective_status[index];
            }
        ge_tab_line(snapshot,getStringID(LTITLE,TITLE_STR_05_NEXT),2U,
            !frontend->cursor_previous_tab);
        ge_tab_line(snapshot,getStringID(LTITLE,TITLE_STR_06_PREVIOUS),3U,
            frontend->cursor_previous_tab);
        break;
    case MENU_MISSION_COMPLETE:
        if(mission==NULL)return 0;
        ge_line(snapshot,mission->title,0,0);
        ge_line(snapshot,getStringID(LTITLE,TITLE_STR_104_STATS),0,0);
        ge_line(snapshot,getStringID(LTITLE,TITLE_STR_105_TIME),0,0);
        ge_line(snapshot,getStringID(LTITLE,TITLE_STR_106_ACCURACY),0,0);
        ge_line(snapshot,getStringID(LTITLE,TITLE_STR_107_WEAPONOFCHOICE),0,0);
        ge_line(snapshot,getStringID(LTITLE,TITLE_STR_108_SHOTTOTAL),0,0);
        ge_line(snapshot,getStringID(LTITLE,TITLE_STR_109_HEADHITS),0,0);
        ge_line(snapshot,getStringID(LTITLE,TITLE_STR_110_BODYHITS),0,0);
        ge_line(snapshot,getStringID(LTITLE,TITLE_STR_111_LIMBHITS),0,0);
        ge_line(snapshot,getStringID(LTITLE,TITLE_STR_112_OTHER),0,0);
        ge_line(snapshot,getStringID(LTITLE,TITLE_STR_113_KILLTOTAL),0,0);
        ge_tab_line(snapshot,getStringID(LTITLE,TITLE_STR_05_NEXT),2U,
            !frontend->cursor_previous_tab);
        ge_tab_line(snapshot,getStringID(LTITLE,TITLE_STR_06_PREVIOUS),3U,
            frontend->cursor_previous_tab);
        break;
    case MENU_RUN_STAGE:break;
    default:return 0;
    }
    return 1;
}
