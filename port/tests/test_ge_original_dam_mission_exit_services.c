#include "ge_original_dam_mission_exit_services.h"
#include "ge_original_mission_result.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"

static struct player test_player;
struct player *g_CurrentPlayer = &test_player;
stagesetup g_CurrentSetup;
f32 g_GlobalTimerDelta = 1.0f;
s32 g_ClockTimer = 1;
enum CAMERAMODE g_CameraMode = CAMERAMODE_NONE;
s32 selected_folder_num = FOLDER1;
static s32 test_controls_locked;
static bool test_objectives_complete;
static s32 test_title_requests;
static LEVELID test_stage = LEVELID_DAM;
static int test_unlock_stage_calls;
static int test_unlock_cheat_calls;
static int test_save;
static int test_blood_frames;
static f32 test_model_frame;
static f32 test_model_end_frame;
s32 g_musicXTrack1Fade;
s32 g_musicXTrack2Fade;
s32 g_bondviewForceDisarm;

s32 getPlayerCount(void)
{
    return 1;
}

void chrpropCleanupForRemoval(PropRecord *prop)
{
    (void)prop;
}

void bondviewUpdatePlayerRoom(struct player *player)
{
    (void)player;
}

void solo_char_load(void)
{
}

s32 die_blood_image_routine(s32 mode)
{
    (void)mode;
    return ++test_blood_frames >= 2;
}

f32 modelGetAnimFrame(Model *model)
{
    (void)model;
    return test_model_frame;
}

f32 modelGetAnimEndFrame(Model *model)
{
    (void)model;
    return test_model_end_frame;
}

void musicStopSlot(s32 slot) { assert(slot == -1); }
void set_missionstate(s32 state) { assert(state == 0); }
u16 sub_GAME_7F0C0BF0(void) { return 0x1234; }
void musicTrack1ApplySeqpVol(u16 volume) { assert(volume == 0x1234); }
void musicTrack2ApplySeqpVol(u16 volume) { assert(volume == 0); }
void musicTrack1Play(s32 track) { assert(track == 27); }

static void test_unlock_stage(void *context, int32_t folder,
                              int32_t mission, int32_t difficulty,
                              int32_t seconds)
{
    assert(context == &test_save && folder == FOLDER1
        && mission == SP_LEVEL_DAM && difficulty == DIFFICULTY_SECRET
        && seconds == 120);
    ++test_unlock_stage_calls;
}

static void *test_save_for_folder(void *context, int32_t folder)
{
    assert(context == &test_save && folder == FOLDER1);
    return &test_save;
}

static int test_cheat_unlocked(void *context, void *save, int32_t mission)
{
    assert(context == &test_save && save == &test_save
        && mission == SP_LEVEL_DAM);
    return 0;
}

static void test_unlock_cheat(void *context, int32_t folder, int32_t mission)
{
    assert(context == &test_save && folder == FOLDER1
        && mission == SP_LEVEL_DAM);
    ++test_unlock_cheat_calls;
}

DIFFICULTY lvlGetSelectedDifficulty(void)
{
    return DIFFICULTY_SECRET;
}

s32 getMissiontimer(void)
{
    return 60 * 120;
}

s32 lvlGetControlsLockedFlag(void)
{
    return test_controls_locked;
}

bool objectiveIsAllComplete(void)
{
    return test_objectives_complete;
}

LEVELID bossGetStageNum(void)
{
    return test_stage;
}

void bossRunTitleStage(void)
{
    ++test_title_requests;
}

void currentPlayerSetFadeColour(s32 red, s32 green, s32 blue, f32 fraction)
{
    g_CurrentPlayer->colourscreenred = red;
    g_CurrentPlayer->colourscreengreen = green;
    g_CurrentPlayer->colourscreenblue = blue;
    g_CurrentPlayer->colourscreenfrac = fraction;
}

void currentPlayerAdjustFade(f32 maximum, s32 red, s32 green, s32 blue,
                             f32 fraction)
{
    g_CurrentPlayer->colourfadetime60 = 0;
    g_CurrentPlayer->colourfadetimemax60 = maximum;
    g_CurrentPlayer->colourfaderedold = g_CurrentPlayer->colourscreenred;
    g_CurrentPlayer->colourfaderednew = red;
    g_CurrentPlayer->colourfadegreenold = g_CurrentPlayer->colourscreengreen;
    g_CurrentPlayer->colourfadegreennew = green;
    g_CurrentPlayer->colourfadeblueold = g_CurrentPlayer->colourscreenblue;
    g_CurrentPlayer->colourfadebluenew = blue;
    g_CurrentPlayer->colourfadefracold = g_CurrentPlayer->colourscreenfrac;
    g_CurrentPlayer->colourfadefracnew = fraction;
}

extern s32 stop_time_flag;
extern s32 cameraBufferToggle;
extern s32 cameraFrameCounter2;
extern s32 credits_state;
extern PadRecord *g_CameraLookAtBondPad;
extern CutsceneRecord *gBondViewCutscene;
extern f32 flt_CODE_bss_80079A00;
extern f32 flt_CODE_bss_80079A04;
extern f32 flt_CODE_bss_80079A08;
extern f32 flt_CODE_bss_80079A0C;
extern f32 flt_CODE_bss_80079A10;
extern s32 dword_CODE_bss_80079A14;
extern enum CAMERAMODE dword_CODE_bss_80079A18;

int main(void)
{
    GeOriginalDamMissionExitSnapshot snapshot;
    GeOriginalMissionResultSnapshot result;
    PropRecord player_prop;
    ChrRecord player_chr;

    memset(&test_player, 0, sizeof(test_player));
    ge_original_dam_mission_exit_services_reset();
    ge_original_dam_mission_exit_services_snapshot(&snapshot);
    assert(snapshot.fade_ticks == 0U);
    assert(snapshot.stop_time == 0);
    assert(snapshot.timer_active == 1);
    assert(snapshot.camera_mode == CAMERAMODE_NONE);

    /* AI_EndLevel must wait for the unchanged double-buffer drain before it
     * calls bossReturnTitleStage.  This is shared by all authored campaign
     * terminal lists, not a Dam-specific frontend transition. */
    test_objectives_complete = false;
    test_title_requests = 0;
    cameraBufferToggle = TRUE;
    cameraFrameCounter2 = FALSE;
    ge_original_campaign_end_level_dispatch_exact();
    assert(cameraFrameCounter2 == TRUE);
    assert(test_title_requests == 0);
    ge_original_campaign_end_level_dispatch_exact();
    assert(cameraFrameCounter2 == TRUE);
    assert(test_title_requests == 0);
    cameraBufferToggle = FALSE;
    ge_original_campaign_end_level_dispatch_exact();
    ge_original_dam_mission_exit_services_snapshot(&snapshot);
    ge_original_mission_result_snapshot(&result);
    assert(test_title_requests == 1);
    assert(snapshot.title_stage_requests == 1U);
    assert(result.apply_calls == 0U
        && result.completion_mutations == 0U
        && result.cheat_mutations == 0U);
    ge_original_dam_mission_exit_services_reset();
    test_title_requests = 0;

    /* The exact viewport tail accepts a fresh gameplay button, advances the
     * authored stop state to 2, and starts the canonical 60-tick black fade. */
    stop_time_flag = 1;
    test_player.buttons_pressed = 0;
    test_player.colourscreenfrac = 0.0f;
    test_player.colourfadetime60 = -1.0f;
    test_player.colourfadetimemax60 = -1.0f;
    test_controls_locked = 1;
    ge_original_dam_mission_exit_process_input_exact(Z_TRIG);
    assert(stop_time_flag == 1);
    assert(test_player.buttons_pressed == Z_TRIG);
    assert(test_player.colourfadetime60 == -1.0f);
    test_controls_locked = 0;
    ge_original_dam_mission_exit_process_input_exact(0U);
    assert(stop_time_flag == 1);
    ge_original_dam_mission_exit_process_input_exact(Z_TRIG);
    assert(stop_time_flag == 2);
    assert(test_player.buttons_pressed == Z_TRIG);
    assert(test_player.colourscreenred == 0);
    assert(test_player.colourscreengreen == 0);
    assert(test_player.colourscreenblue == 0);
    assert(test_player.colourfadetime60 == 0.0f);
    assert(test_player.colourfadetimemax60 == 60.0f);

    g_GlobalTimerDelta = 60.0f;
    ge_original_dam_mission_exit_services_tick();
    ge_original_dam_mission_exit_process_input_exact(Z_TRIG);
    assert(test_title_requests == 1);
    ge_original_dam_mission_exit_services_snapshot(&snapshot);
    ge_original_mission_result_snapshot(&result);
    assert(snapshot.title_stage_requests == 1U);
    assert(snapshot.briefing_frontiers == 0U);
    assert(result.apply_calls == 0U
        && result.completion_mutations == 0U
        && result.cheat_mutations == 0U);

    /* Objective success still publishes the unconditional canonical title
     * request while exposing the not-yet-linked briefing/save boundary. */
    test_objectives_complete = true;
    ge_original_dam_mission_return_title_exact();
    ge_original_dam_mission_exit_services_snapshot(&snapshot);
    assert(test_title_requests == 2);
    assert(snapshot.title_stage_requests == 2U);
    assert(snapshot.briefing_frontiers == 1U);
    assert(snapshot.briefing_commits == 0U);

    /* End-to-end exact viewport tail: after ai_24 has published stop_time=1,
     * one fresh fire edge owns fade -> title -> unchanged briefing/save.
     * This proves the live consumer does not need a port-authored completion
     * state machine between the canonical AI and persistence bodies. */
    {
        GeOriginalMissionResultProviders providers = {
            &test_save, test_unlock_stage, test_save_for_folder,
            test_cheat_unlocked, test_unlock_cheat
        };
        ge_original_dam_mission_exit_services_reset();
        ge_original_mission_result_bind(&providers);
        memset(&test_player, 0, sizeof(test_player));
        test_objectives_complete = true;
        test_controls_locked = 0;
        stop_time_flag = 1;
        test_player.colourscreenfrac = 0.0f;
        test_player.colourfadetime60 = -1.0f;
        test_player.colourfadetimemax60 = -1.0f;
        ge_original_dam_mission_exit_process_input_exact(Z_TRIG);
        assert(stop_time_flag == 2);
        g_GlobalTimerDelta = 60.0f;
        ge_original_dam_mission_exit_services_tick();
        ge_original_dam_mission_exit_process_input_exact(Z_TRIG);
        ge_original_dam_mission_exit_services_snapshot(&snapshot);
        ge_original_mission_result_snapshot(&result);
        assert(snapshot.title_stage_requests == 1U
            && snapshot.briefing_commits == 1U
            && snapshot.briefing_frontiers == 0U);
        assert(test_unlock_stage_calls == 1
            && test_unlock_cheat_calls == 1);
        assert(result.apply_calls == 1U
            && result.completion_mutations == 1U
            && result.cheat_mutations == 1U
            && result.persistence_frontiers == 0U);
    }

    g_GlobalTimerDelta = 1.0f;
    ge_original_dam_mission_exit_services_reset();

    test_player.colourscreenfrac = 0.0f;
    test_player.colourfadefracold = 0.0f;
    test_player.colourfadefracnew = 1.0f;
    test_player.colourfadetime60 = 0.0f;
    test_player.colourfadetimemax60 = 3.0f;
    ge_original_dam_mission_exit_services_tick();
    ge_original_dam_mission_exit_services_snapshot(&snapshot);
    assert(snapshot.fade_ticks == 1U);
    assert(fabsf(snapshot.fade_fraction - (1.0f / 3.0f)) < 0.0001f);
    assert(snapshot.fade_time == 1.0f);
    assert(snapshot.fade_time_max == 3.0f);

    ge_original_dam_mission_exit_services_tick();
    ge_original_dam_mission_exit_services_tick();
    ge_original_dam_mission_exit_services_snapshot(&snapshot);
    assert(snapshot.fade_ticks == 3U);
    assert(snapshot.fade_fraction == 1.0f);
    assert(snapshot.fade_time == -1.0f);
    assert(snapshot.fade_time_max == -1.0f);

    ge_original_dam_mission_set_camera_posend_exact(CAMERAMODE_POSEND);
    ge_original_dam_mission_exit_services_snapshot(&snapshot);
    assert(snapshot.camera_mode == CAMERAMODE_POSEND);
    assert(snapshot.posend_camera_requests == 1U);

    ge_original_dam_mission_set_camera_posend_exact(CAMERAMODE_INTRO);
    ge_original_dam_mission_exit_services_snapshot(&snapshot);
    assert(snapshot.camera_mode == CAMERAMODE_POSEND);
    assert(snapshot.posend_camera_requests == 1U);

    /* Cuba's native presentation consumes the endian-relocated authored
     * CreditsEntry table with the unchanged 16-pixel render window. */
    {
        CreditsEntry credits[3] = {
            {0x5008, 0x5011, 180, CREDITS_ALIGN_LEFT,
                -1, CREDITS_ALIGN_PREVIOUS},
            {0x5009, 0x500a, -1, CREDITS_ALIGN_PREVIOUS,
                240, CREDITS_ALIGN_CENTER},
            {0, 0, 0, 0, 0, 0},
        };
        GeOriginalCreditsRenderSnapshot credits_snapshot;
        unsigned tick;
        test_stage = LEVELID_CUBA;
        ge_original_campaign_credits_bind(credits, 2U);
        credits_state = 1;
        assert(ge_original_campaign_credits_render_tick_exact(
            0, 240, &credits_snapshot));
        assert(credits_snapshot.frame == 1U
            && credits_snapshot.visible
            && credits_snapshot.line_count == 1U);
        assert(credits_snapshot.lines[0].text_id == 0x5008
            && credits_snapshot.lines[0].position == 180
            && credits_snapshot.lines[0].alignment == CREDITS_ALIGN_LEFT
            && credits_snapshot.lines[0].y == 239);
        for (tick = 1U; tick < 16U; ++tick)
            assert(ge_original_campaign_credits_render_tick_exact(
                0, 240, &credits_snapshot));
        assert(credits_snapshot.frame == 16U
            && credits_snapshot.line_count == 3U);
        assert(credits_snapshot.lines[1].text_id == 0x5009
            && credits_snapshot.lines[1].position == 180
            && credits_snapshot.lines[1].alignment == CREDITS_ALIGN_LEFT);
        assert(credits_snapshot.lines[2].text_id == 0x500a
            && credits_snapshot.lines[2].position == 240
            && credits_snapshot.lines[2].alignment == CREDITS_ALIGN_CENTER);
        for (; tick < 288U; ++tick)
            assert(ge_original_campaign_credits_render_tick_exact(
                0, 240, &credits_snapshot));
        assert(credits_state == 2 && credits_snapshot.complete
            && !credits_snapshot.visible);
    }

    /* AI_CameraOrbitPad owns these exact globals. The retained POSEND branch
     * resolves its authored pad and advances/wraps the same angular state. */
    {
        PadRecord pads[2];
        unsigned char stan_storage[sizeof(StandTile)];
        StandTile *stan = (StandTile *)stan_storage;
        GeOriginalPosendCameraSnapshot camera;
        memset(pads, 0, sizeof(pads));
        memset(stan_storage, 0, sizeof(stan_storage));
        stan->room = 7U;
        pads[1].pos.f[0] = 10.0f;
        pads[1].pos.f[1] = 20.0f;
        pads[1].pos.f[2] = 30.0f;
        pads[1].stan = stan;
        g_CurrentSetup.pads = pads;
        g_CameraMode = CAMERAMODE_POSEND;
        dword_CODE_bss_80079A14 = 1;
        flt_CODE_bss_80079A00 = 0.0f;
        flt_CODE_bss_80079A04 = M_PI_F * 0.5f;
        flt_CODE_bss_80079A08 = 100.0f;
        flt_CODE_bss_80079A0C = 10.0f;
        flt_CODE_bss_80079A10 = 5.0f;
        g_GlobalTimerDelta = 1.0f;
        assert(ge_original_campaign_posend_camera_tick_exact(&camera));
        assert(camera.valid && camera.pad_id == 1 && camera.room == 7U);
        assert(fabsf(camera.position[0] - 10.0f) < 0.0001f
            && fabsf(camera.position[1] - 35.0f) < 0.0001f
            && fabsf(camera.position[2] - 130.0f) < 0.0001f);
        assert(fabsf(camera.look_direction[0]) < 0.0001f
            && fabsf(camera.look_direction[1] + 10.0f) < 0.0001f
            && fabsf(camera.look_direction[2] + 100.0f) < 0.0001f);
        assert(fabsf(flt_CODE_bss_80079A00 - M_PI_F * 0.5f) < 0.0001f);
        assert(ge_original_campaign_posend_camera_tick_exact(&camera));
        assert(fabsf(camera.position[0] - 110.0f) < 0.0001f
            && fabsf(camera.position[2] - 30.0f) < 0.0001f);
        flt_CODE_bss_80079A00 = M_TAU_F - 0.1f;
        flt_CODE_bss_80079A04 = 0.2f;
        assert(ge_original_campaign_posend_camera_tick_exact(&camera));
        assert(fabsf(flt_CODE_bss_80079A00 - 0.1f) < 0.0002f);

        /* Authored pad-look ending branch: camera sits on the pad and looks
         * at the unchanged player focus fields. */
        g_CameraLookAtBondPad = &pads[1];
        test_player.field_3C4 = 40.0f;
        test_player.field_3C8 = 50.0f;
        test_player.field_3CC = 70.0f;
        assert(ge_original_campaign_posend_camera_tick_exact(&camera));
        assert(camera.valid && camera.room == 7U && camera.pad_id == -1);
        assert(fabsf(camera.position[0] - 10.0f) < 0.0001f
            && fabsf(camera.position[1] - 20.0f) < 0.0001f
            && fabsf(camera.position[2] - 30.0f) < 0.0001f);
        assert(fabsf(camera.look_direction[0] - 30.0f) < 0.0001f
            && fabsf(camera.look_direction[1] - 30.0f) < 0.0001f
            && fabsf(camera.look_direction[2] - 40.0f) < 0.0001f);

        /* Authored camera_switch branch: retain the PROPDEF_CAMERAPOS
         * theta/verta direction and its pad-owned STAN room. */
        {
            CutsceneRecord cutscene;
            memset(&cutscene, 0, sizeof(cutscene));
            g_CameraLookAtBondPad = NULL;
            gBondViewCutscene = &cutscene;
            cutscene.pad = 1;
            cutscene.pos.f[0] = 100.0f;
            cutscene.pos.f[1] = 200.0f;
            cutscene.pos.f[2] = 300.0f;
            cutscene.theta = M_PI_F * 0.5f;
            cutscene.verta = 0.0f;
            dword_CODE_bss_80079A18 = CAMERAMODE_NONE;
            assert(ge_original_campaign_posend_camera_tick_exact(&camera));
            assert(camera.valid && camera.room == 7U && camera.pad_id == 1);
            assert(fabsf(camera.position[0] - 100.0f) < 0.0001f
                && fabsf(camera.position[1] - 200.0f) < 0.0001f
                && fabsf(camera.position[2] - 300.0f) < 0.0001f);
            assert(fabsf(camera.look_direction[0] - 1.0f) < 0.0001f
                && fabsf(camera.look_direction[1]) < 0.0001f
                && fabsf(camera.look_direction[2]) < 0.0001f);
            assert(fabsf(camera.anchor[0] - 10.0f) < 0.0001f
                && fabsf(camera.anchor[1] - 20.0f) < 0.0001f
                && fabsf(camera.anchor[2] - 30.0f) < 0.0001f);

            dword_CODE_bss_80079A18 = CAMERAMODE_INTRO;
            assert(ge_original_campaign_posend_camera_tick_exact(&camera));
            assert(fabsf(camera.look_direction[0] + 60.0f) < 0.0001f
                && fabsf(camera.look_direction[1] + 150.0f) < 0.0001f
                && fabsf(camera.look_direction[2] + 230.0f) < 0.0001f);
            gBondViewCutscene = NULL;
            dword_CODE_bss_80079A18 = CAMERAMODE_NONE;
        }
        memset(&g_CurrentSetup, 0, sizeof(g_CurrentSetup));
        test_stage = LEVELID_DAM;
    }

    /* maybe_mp_interface owned this state progression on N64. The native
     * renderer must still run its exact blood -> animation -> fade -> death
     * camera ordering or Bond remains frozen while the stage keeps ticking. */
    memset(&test_player, 0, sizeof(test_player));
    memset(&player_prop, 0, sizeof(player_prop));
    memset(&player_chr, 0, sizeof(player_chr));
    ge_original_dam_mission_exit_services_reset();
    player_prop.chr = &player_chr;
    test_player.prop = &player_prop;
    test_player.bonddead = 1;
    test_player.colourfadetime60 = -1.0f;
    test_player.colourfadetimemax60 = -1.0f;
    test_player.bondfadetimemax60 = -1.0f;
    test_model_frame = 10.0f;
    test_model_end_frame = 10.0f;
    test_blood_frames = 0;
    test_title_requests = 0;
    g_GlobalTimerDelta = 1.0f;
    ge_original_dam_mission_exit_services_tick();
    assert(test_player.bonddead == 2);
    assert(!test_player.redbloodfinished);
    ge_original_dam_mission_exit_services_tick();
    assert(test_player.redbloodfinished);
    assert(test_player.deathanimfinished);
    assert(test_player.colourfadetime60 == 0.0f
        && test_player.colourfadetimemax60 == 60.0f);
    g_GlobalTimerDelta = 60.0f;
    ge_original_dam_mission_exit_services_tick();
    ge_original_dam_mission_exit_services_snapshot(&snapshot);
    assert(snapshot.camera_mode == CAMERAMODE_DEATH_CAM_SP);
    assert(snapshot.death_starts == 1U);
    assert(snapshot.death_blood_frames == 2U);
    assert(snapshot.death_animation_finishes == 1U);
    assert(snapshot.death_camera_starts == 1U);
    ge_original_dam_mission_exit_process_input_exact(0U);
    ge_original_dam_mission_exit_process_input_exact(Z_TRIG);
    ge_original_dam_mission_exit_services_snapshot(&snapshot);
    assert(snapshot.death_title_requests == 1U);
    assert(snapshot.title_stage_requests == 1U);
    assert(test_title_requests == 1);

    /* A dead player also reaches the title/report boundary without a skip
     * button.  This is the live case that previously left Bond frozen while
     * guards continued ticking forever: the original three replay phases
     * advance on their 180-tick camera windows, then request the title stage. */
    memset(&test_player, 0, sizeof(test_player));
    ge_original_dam_mission_exit_services_reset();
    test_player.bonddead = 2;
    test_player.redbloodfinished = TRUE;
    test_player.deathanimfinished = TRUE;
    g_CameraMode = CAMERAMODE_DEATH_CAM_SP;
    g_GlobalTimerDelta = 180.0f;
    test_title_requests = 0;
    ge_original_dam_mission_exit_process_input_exact(0U);
    assert(test_title_requests == 0);
    ge_original_dam_mission_exit_process_input_exact(0U);
    assert(test_title_requests == 0);
    ge_original_dam_mission_exit_process_input_exact(0U);
    ge_original_dam_mission_exit_services_snapshot(&snapshot);
    assert(snapshot.death_camera_starts == 2U);
    assert(snapshot.death_title_requests == 1U);
    assert(snapshot.title_stage_requests == 1U);
    assert(test_title_requests == 1);

    puts("Dam ai_24 exit services: canonical input/fade/title and POSEND publication retained");
    return 0;
}
