#include "ge_original_dam_mission_exit_services.h"

#include <math.h>
#include <string.h>

#include <ultra64.h>

/* The 3DS libultra compatibility header exposes the canonical controller
 * values under the game-facing aliases only. Keep the original source names
 * in the retained viewport body below. */
#ifndef CONT_A
#define CONT_A A_BUTTON
#define CONT_R R_TRIG
#define CONT_L L_TRIG
#endif

#include "boss.h"
#include "game/bondview.h"
#include "game/chraction.h"
#include "game/gun.h"
#include "game/loadobjectmodel.h"
#include "game/lv.h"
#include "game/model.h"
#include "game/objective_status.h"
#include "game/player.h"
#include "ge_original_mission_result.h"

extern stagesetup g_CurrentSetup;
extern f32 g_GlobalTimerDelta;
extern s32 g_ClockTimer;
extern s32 sizepropdef(PropDefHeaderRecord *pdef);

/* These are unchanged decompiled services in the native build.  Keeping the
 * declarations weak lets the narrow host contract test exercise the state
 * machine without linking the full blood decoder, music engine, and model
 * runtime.  A missing production service is recorded as an explicit
 * frontier and can never silently complete the death sequence. */
extern s32 die_blood_image_routine(s32 arg0) __attribute__((weak));
extern f32 modelGetAnimFrame(Model *model) __attribute__((weak));
extern f32 modelGetAnimEndFrame(Model *model) __attribute__((weak));
extern void musicStopSlot(s32 slot) __attribute__((weak));
extern void set_missionstate(s32 state) __attribute__((weak));
extern u16 sub_GAME_7F0C0BF0(void) __attribute__((weak));
extern void musicTrack1ApplySeqpVol(u16 volume) __attribute__((weak));
extern void musicTrack2ApplySeqpVol(u16 volume) __attribute__((weak));
extern void musicTrack1Play(s32 track) __attribute__((weak));
extern s32 g_musicXTrack1Fade __attribute__((weak));
extern s32 g_musicXTrack2Fade __attribute__((weak));
extern void solo_char_load(void) __attribute__((weak));
extern s32 g_bondviewForceDisarm;
extern void bondviewUpdatePlayerRoom(struct player *player);

/* Canonical bondview.c / bondview2.c state omitted by their bounded port
 * slices.  Initial values are the original data/BSS initializers. */
s32 cameraBufferToggle = 0;
s32 cameraFrameCounter2 = 0;
enum CAMERAMODE g_CameraAfterCinema = CAMERAMODE_NONE;
s32 stop_time_flag = 0;
s32 is_timer_active = 1;
PadRecord *g_CameraLookAtBondPad;
CutsceneRecord *gBondViewCutscene;
enum CAMERAMODE dword_CODE_bss_80079A18;
s32 dword_CODE_bss_80079A1C;
enum CAMERAMODE camera_mode = CAMERAMODE_NONE;
/* Canonical bondview.c / bondview2.c BSS used by campaign camera and credits
 * opcodes. These remain zero-initialized exactly as on N64. */
f32 flt_CODE_bss_80079A00;
f32 flt_CODE_bss_80079A04;
f32 flt_CODE_bss_80079A08;
f32 flt_CODE_bss_80079A0C;
f32 flt_CODE_bss_80079A10;
s32 dword_CODE_bss_80079A14;
s32 credits_state = 0;
/* Canonical bondview.c globals. The native setup relocation keeps the table
 * in host-addressable storage instead of reconstructing the original ROM
 * segment pointer arithmetic from bondview_r.c. */
s32 camera_80036438;
CreditsEntry *credits_pointer;

static uint32_t ge_credits_entry_count;

static GeOriginalDamMissionExitSnapshot ge_exit_snapshot;
static f32 ge_death_camera_timer;
static uint16_t ge_death_previous_buttons;

/* Exact loadobjectmodel.c body. */
ObjectRecord *setupGetPtrToCommandByIndex(s32 index)
{
    PropDefHeaderRecord *object = g_CurrentSetup.propDefs;

    if (index >= 0 && object)
    {
        s32 i;
        for (i = 0; object->type != PROPDEF_END; i++)
        {
            if (i == index)
            {
                return object;
            }

            object = sizepropdef(object) + object;
        }
    }

    return NULL;
}

/* Exact loadobjectmodel.c body. */
s32 tagGetCommandIndex(struct ObjectRecord *tag)
{
    PropDefHeaderRecord *object;
    s32 i;

    object = g_CurrentSetup.propDefs;

    if (object != NULL)
    {
        for (i = 0; object->type != PROPDEF_END; i++)
        {
            if ((void*)object == (void*)tag)
            {
                return i;
            }

            object = sizepropdef(object) + object;
        }
    }

    return -1;
}

/* Exact chraction.c body. */
bool check_if_actor_is_at_preset(ChrRecord *self, s32 padnum)
{
    PropRecord *bondprop;
    PadRecord  *pad;

    bondprop = getCurrentPlayerProp();
    padnum   = chrResolvePadId(self, padnum);

    if (isNotBoundPad(padnum))
    {
        pad = (PadRecord *)&g_CurrentSetup.pads[padnum];
    }
    else
    {
        pad = (PadRecord *)&g_CurrentSetup.boundpads[getBoundPadNum(padnum)];
    }

    if (pad->stan && (pad->stan->room == bondprop->stan->room))
    {
        return TRUE;
    }

    return FALSE;
}

/* Exact bondview2.c bodies. */
void currentPlayerSetFadeFrac(f32 maxfadetime, f32 frac)
{
    currentPlayerAdjustFade(maxfadetime,
        g_CurrentPlayer->colourscreenred,
        g_CurrentPlayer->colourscreengreen,
        g_CurrentPlayer->colourscreenblue, frac);
}

static bool ge_current_player_is_fade_complete_exact(void)
{
	return g_CurrentPlayer->colourfadetimemax60 < 0;
}

static void ge_current_player_update_colour_screen_properties_exact(void)
{
    if (g_CurrentPlayer->colourfadetimemax60 >= 0)
    {
        g_CurrentPlayer->colourfadetime60 += g_GlobalTimerDelta;

        if (g_CurrentPlayer->colourfadetime60
                < g_CurrentPlayer->colourfadetimemax60)
        {
            f32 mult = g_CurrentPlayer->colourfadetime60
                / g_CurrentPlayer->colourfadetimemax60;
            g_CurrentPlayer->colourscreenfrac =
                g_CurrentPlayer->colourfadefracold
                + (g_CurrentPlayer->colourfadefracnew
                    - g_CurrentPlayer->colourfadefracold) * mult;
            g_CurrentPlayer->colourscreenred =
                g_CurrentPlayer->colourfaderedold
                + (s32)((g_CurrentPlayer->colourfaderednew
                    - g_CurrentPlayer->colourfaderedold) * mult);
            g_CurrentPlayer->colourscreengreen =
                g_CurrentPlayer->colourfadegreenold
                + (s32)((g_CurrentPlayer->colourfadegreennew
                    - g_CurrentPlayer->colourfadegreenold) * mult);
            g_CurrentPlayer->colourscreenblue =
                g_CurrentPlayer->colourfadeblueold
                + (s32)((g_CurrentPlayer->colourfadebluenew
                    - g_CurrentPlayer->colourfadeblueold) * mult);
        }
        else
        {
            g_CurrentPlayer->colourscreenfrac =
                g_CurrentPlayer->colourfadefracnew;
            g_CurrentPlayer->colourscreenred =
                g_CurrentPlayer->colourfaderednew;
            g_CurrentPlayer->colourscreengreen =
                g_CurrentPlayer->colourfadegreennew;
            g_CurrentPlayer->colourscreenblue =
                g_CurrentPlayer->colourfadebluenew;
            g_CurrentPlayer->colourfadetime60 = -1;
            g_CurrentPlayer->colourfadetimemax60 = -1;
        }
    }
}

/* Exact bondview2.c body. */
static void ge_current_player_start_chr_fade_exact(
    f32 duration60, f32 targetfrac)
{
    ChrRecord *chr = g_CurrentPlayer->prop != NULL
        ? g_CurrentPlayer->prop->chr : NULL;

    if (chr)
    {
        g_CurrentPlayer->bondfadetime60    = 0;
        g_CurrentPlayer->bondfadetimemax60 = duration60;
        g_CurrentPlayer->bondfadefracold   = chr->fadealpha / 255.0f;
        g_CurrentPlayer->bondfadefracnew   = targetfrac;
    }
}

/* Exact bondview2.c body, retained here because the native viewport owns the
 * VI portion which formerly called it. */
static void ge_current_player_tick_chr_fade_exact(void)
{
    if (g_CurrentPlayer->bondfadetimemax60 >= 0)
    {
        ChrRecord *chr = g_CurrentPlayer->prop != NULL
            ? g_CurrentPlayer->prop->chr : NULL;
        f32 frac;

        g_CurrentPlayer->bondfadetime60 += g_GlobalTimerDelta;

        if (g_CurrentPlayer->bondfadetime60
                < g_CurrentPlayer->bondfadetimemax60)
        {
            frac = g_CurrentPlayer->bondfadefracold
                + (g_CurrentPlayer->bondfadefracnew
                    - g_CurrentPlayer->bondfadefracold)
                * g_CurrentPlayer->bondfadetime60
                / g_CurrentPlayer->bondfadetimemax60;
        }
        else
        {
            frac = g_CurrentPlayer->bondfadefracnew;
            g_CurrentPlayer->bondfadetimemax60 = -1;
        }

        if (chr)
        {
            chr->fadealpha = (s8)(frac * 255);
        }
    }
}

static void ge_original_death_music_start_exact(void)
{
    if (musicStopSlot == NULL || set_missionstate == NULL
            || sub_GAME_7F0C0BF0 == NULL
            || musicTrack1ApplySeqpVol == NULL
            || musicTrack2ApplySeqpVol == NULL
            || musicTrack1Play == NULL
            || &g_musicXTrack1Fade == NULL
            || &g_musicXTrack2Fade == NULL) {
        ++ge_exit_snapshot.death_service_frontiers;
        return;
    }
    musicStopSlot(-1);
    set_missionstate(0);
    musicTrack1ApplySeqpVol(sub_GAME_7F0C0BF0());
    g_musicXTrack1Fade = 0;
    musicTrack2ApplySeqpVol(0);
    g_musicXTrack2Fade = 0;
    musicTrack1Play(27);
}

/* This is the state-owning portion of maybe_mp_interface's unchanged death
 * block.  The N64 display-list call which draws the decoded blood image is a
 * renderer sink; the native renderer consumes the exact colour/fade fields.
 * Blood frame decode, animation completion, and transition ordering remain
 * owned by the original services and player/model state. */
static void ge_original_player_death_render_tick_exact(void)
{
    s32 doblood;

    if (g_CurrentPlayer->bonddead == 0) return;
    if (g_CurrentPlayer->deathanimfinished == 0)
    {
        doblood = 0;
        if (g_CurrentPlayer->bonddead == 1)
        {
            doblood = 1;
            g_CurrentPlayer->bonddead = 2;
            ++ge_exit_snapshot.death_starts;
        }
        if (doblood)
        {
            if (die_blood_image_routine != NULL) {
                (void)die_blood_image_routine(0);
                ++ge_exit_snapshot.death_blood_frames;
            } else {
                ++ge_exit_snapshot.death_service_frontiers;
                return;
            }
            ge_original_death_music_start_exact();
        }
        else
        {
            if (g_CurrentPlayer->redbloodfinished)
            {
                currentPlayerSetFadeColour(150, 0, 0, 0.7058824f);
            }
            else
            {
                doblood = g_ClockTimer > 0 ? 1 : 2;
                if (die_blood_image_routine == NULL) {
                    ++ge_exit_snapshot.death_service_frontiers;
                    return;
                }
                if (die_blood_image_routine(doblood))
                    g_CurrentPlayer->redbloodfinished = TRUE;
                ++ge_exit_snapshot.death_blood_frames;
            }
        }
    }
    if (modelGetAnimFrame == NULL || modelGetAnimEndFrame == NULL) {
        ++ge_exit_snapshot.death_service_frontiers;
        return;
    }
    if (modelGetAnimFrame(&g_CurrentPlayer->model)
            >= modelGetAnimEndFrame(&g_CurrentPlayer->model))
    {
        if (g_CurrentPlayer->redbloodfinished)
        {
            if (!g_CurrentPlayer->deathanimfinished)
            {
                g_CurrentPlayer->deathanimfinished = TRUE;
                currentPlayerAdjustFade(60.0f, 0, 0, 0, 1.0f);
                ge_current_player_start_chr_fade_exact(120.0f, 0.0f);
                ++ge_exit_snapshot.death_animation_finishes;
            }
            if (ge_current_player_is_fade_complete_exact()
                    && g_CameraMode != CAMERAMODE_DEATH_CAM_SP)
            {
                /* bondviewSetCameraMode's single-player death entry begins
                 * the replay at this exact point.  Native camera rendering
                 * still uses the live player matrices, but the canonical
                 * mode/replay counter and fade own the eventual stage exit. */
                g_CameraMode = CAMERAMODE_DEATH_CAM_SP;
                g_CameraAfterCinema = CAMERAMODE_NONE;
                ge_death_camera_timer = 0.0f;
                currentPlayerSetFadeColour(0, 0, 0, 1.0f);
                currentPlayerSetFadeFrac(60.0f, 0.0f);
                ++ge_exit_snapshot.death_camera_starts;
            }
        }
    }
}

static void ge_original_player_death_camera_tick_exact(uint16_t buttons)
{
    const uint16_t skip = CONT_A | B_BUTTON | Z_TRIG | START_BUTTON;
    if (g_CurrentPlayer->bonddead == 0
            || g_CameraMode != CAMERAMODE_DEATH_CAM_SP) {
        ge_death_previous_buttons = buttons;
        return;
    }
    ge_death_camera_timer += g_GlobalTimerDelta;
    if ((buttons & ~ge_death_previous_buttons & skip)
            && g_CurrentPlayer->redbloodfinished
            && g_CurrentPlayer->deathanimfinished)
        camera_mode = CAMERAMODE_FADESWIRL;
    if (ge_death_camera_timer >= 180.0f
            || camera_mode == CAMERAMODE_FADESWIRL) {
        ++camera_mode;
        ge_death_camera_timer = 0.0f;
        if (camera_mode >= CAMERAMODE_SWIRL) {
            bossRunTitleStage();
            ++ge_exit_snapshot.title_stage_requests;
            ++ge_exit_snapshot.death_title_requests;
        } else {
            ++ge_exit_snapshot.death_camera_starts;
        }
    }
    ge_death_previous_buttons = buttons;
}

/* Exact gun.c body. */
void remove_item_in_hand(GUNHAND hand)
{
    g_CurrentPlayer->hand_invisible[hand] = 0;
    g_CurrentPlayer->hand_item[hand] = ITEM_UNARMED;
    g_CurrentPlayer->field_2A44[hand] = -1;
    g_CurrentPlayer->lock_hand_model[hand] = 1;
    return;
}

static void ge_bondview_remove_player_body_exact(void)
{
    if ((g_CurrentPlayer->prop->chr) && (getPlayerCount() == 1))
    {
        chrpropCleanupForRemoval(g_CurrentPlayer->prop);
        g_CurrentPlayer->prop->chr = NULL;
        g_CurrentPlayer->bodyModel = 0;
        g_bondviewForceDisarm = 1;
        bondviewUpdatePlayerRoom(g_CurrentPlayer);
    }
}

void bondviewSetCameraMode(s32 mode)
{
    g_CameraMode = mode;
    g_CameraAfterCinema = 0;

    if (g_CameraMode == CAMERAMODE_POSEND)
    {
        if (solo_char_load != NULL) solo_char_load();
        else ++ge_exit_snapshot.camera_service_frontiers;
        g_CurrentPlayer->cameratile = NULL;
        ++ge_exit_snapshot.posend_camera_requests;
    }
    else if (g_CameraMode == CAMERAMODE_FP_NOINPUT)
    {
        ge_bondview_remove_player_body_exact();
        g_CameraMode = CAMERAMODE_FP;
        ++ge_exit_snapshot.fp_noinput_camera_requests;
    }
    else
    {
        /* Full intro/death/swirl camera construction remains owned by the
         * native viewport integration. Never silently pretend those modes
         * completed when reached through a future caller. */
        ++ge_exit_snapshot.camera_service_frontiers;
    }
}

void ge_original_dam_mission_set_camera_posend_exact(int32_t mode)
{
    /* ai_24's only call uses CAMERAMODE_POSEND.  For that input the complete
     * canonical bondviewSetCameraMode body is exactly these two assignments;
     * all of its remaining branches test other camera modes. */
    if (mode != CAMERAMODE_POSEND) return;
    bondviewSetCameraMode(mode);
}

void ge_original_campaign_credits_bind(
    const CreditsEntry *entries, uint32_t entry_count)
{
    credits_pointer = (CreditsEntry *)entries;
    ge_credits_entry_count = entries != NULL ? entry_count : 0U;
    camera_80036438 = 0;
    credits_state = 0;
}

static int ge_original_credits_append(
    GeOriginalCreditsRenderSnapshot *snapshot, uint16_t text_id,
    int32_t position, int32_t y, int32_t alignment)
{
    GeOriginalCreditsRenderLine *line;
    if (snapshot->line_count
            >= GE_ORIGINAL_CREDITS_VISIBLE_LINE_CAPACITY) return 0;
    line = &snapshot->lines[snapshot->line_count++];
    line->text_id = text_id;
    line->position = (int16_t)position;
    line->y = (int16_t)y;
    line->alignment = (int16_t)alignment;
    return 1;
}

int ge_original_campaign_credits_render_tick_exact(
    int16_t view_top, int16_t view_height,
    GeOriginalCreditsRenderSnapshot *snapshot)
{
    s32 frame;
    s32 start;
    s32 xpos1;
    s32 xpos2;
    s32 i;
    s32 end;
    s32 align1;
    s32 align2;

    if (snapshot == NULL) return 0;
    memset(snapshot, 0, sizeof(*snapshot));

    /* Exact bondviewRenderCredits gate and initial state. */
    if (bossGetStageNum() != LEVELID_CUBA || credits_state != 1
            || credits_pointer == NULL) {
        snapshot->complete = credits_state == 2;
        return 1;
    }

    xpos1 = 0xdc;
    xpos2 = 0xdc;
    align1 = CREDITS_ALIGN_RIGHT;
    align2 = CREDITS_ALIGN_RIGHT;
    camera_80036438++;
    frame = camera_80036438;
    start = (frame - view_height) / 16;
    end = (frame / 16) + 1;

    if (start < 0) start = 0;

    for (i = 0; i < start; i++) {
        /* The relocated table retains its canonical zero terminator. The
         * sidecar count is a corruption guard, not a different end rule. */
        if ((uint32_t)i > ge_credits_entry_count) return 0;
        if (credits_pointer[i].TextId1 == 0
                && credits_pointer[i].TextId2 == 0) {
            end = i;
            start = i;
            credits_state = 2;
            break;
        }
        if (credits_pointer[i].TextId1 != 0x5011) {
            if (credits_pointer[i].Position1 >= 0)
                xpos1 = credits_pointer[i].Position1;
            if ((s16)credits_pointer[i].Alignment1 >= CREDITS_ALIGN_RIGHT)
                align1 = (s16)credits_pointer[i].Alignment1;
        }
        if (credits_pointer[i].TextId2 != 0x5011) {
            if (credits_pointer[i].Position2 >= 0)
                xpos2 = credits_pointer[i].Position2;
            if ((s16)credits_pointer[i].Alignment2 >= CREDITS_ALIGN_RIGHT)
                align2 = (s16)credits_pointer[i].Alignment2;
        }
    }

    for (i = start; i < end; i++) {
        const s32 y = view_top + i * 16 - frame + view_height;
        if ((uint32_t)i > ge_credits_entry_count) return 0;
        if (credits_pointer[i].TextId1 == 0
                && credits_pointer[i].TextId2 == 0) break;
        if ((u32)credits_pointer[i].TextId1 != 0x5011) {
            if (credits_pointer[i].Position1 >= 0)
                xpos1 = credits_pointer[i].Position1;
            if ((s16)credits_pointer[i].Alignment1 >= CREDITS_ALIGN_RIGHT)
                align1 = (s16)credits_pointer[i].Alignment1;
            if (!ge_original_credits_append(snapshot,
                    credits_pointer[i].TextId1, xpos1, y, align1)) return 0;
        }
        if (credits_pointer[i].TextId2 != 0x5011) {
            if (credits_pointer[i].Position2 >= 0)
                xpos2 = credits_pointer[i].Position2;
            if ((s16)credits_pointer[i].Alignment2 >= CREDITS_ALIGN_RIGHT)
                align2 = (s16)credits_pointer[i].Alignment2;
            if (!ge_original_credits_append(snapshot,
                    credits_pointer[i].TextId2, xpos2, y, align2)) return 0;
        }
    }

    snapshot->frame = (uint32_t)frame;
    snapshot->visible = snapshot->line_count != 0U;
    snapshot->complete = credits_state == 2;
    return 1;
}

int ge_original_campaign_posend_camera_tick_exact(
    GeOriginalPosendCameraSnapshot *snapshot)
{
    PadRecord *setupPad;
    f32 angle;

    if (snapshot == NULL) return 0;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->pad_id = -1;
    if (g_CameraMode != CAMERAMODE_POSEND) return 1;

    /* Exact first CAMERAMODE_POSEND branch from bondviewFrozenMoveBond. */
    if (g_CameraLookAtBondPad != NULL) {
        if (g_CurrentPlayer == NULL || g_CameraLookAtBondPad->stan == NULL)
            return 0;
        snapshot->position[0] = g_CameraLookAtBondPad->pos.f[0];
        snapshot->position[1] = g_CameraLookAtBondPad->pos.f[1];
        snapshot->position[2] = g_CameraLookAtBondPad->pos.f[2];
        snapshot->look_direction[0] = g_CurrentPlayer->field_3C4
            - snapshot->position[0];
        snapshot->look_direction[1] = g_CurrentPlayer->field_3C8
            - snapshot->position[1];
        snapshot->look_direction[2] = g_CurrentPlayer->field_3CC
            - snapshot->position[2];
        memcpy(snapshot->anchor, snapshot->position,
            sizeof(snapshot->anchor));
        snapshot->up[1] = 1.0f;
        snapshot->room = g_CameraLookAtBondPad->stan->room;
        snapshot->valid = 1U;
        return 1;
    }

    /* Exact tagged PROPDEF_CAMERAPOS branch used by the authored terminal
     * cutscenes throughout the solo campaign. */
    if (gBondViewCutscene != NULL) {
        if (g_CurrentSetup.pads == NULL) return 0;
        if (isNotBoundPad(gBondViewCutscene->pad))
            setupPad = &g_CurrentSetup.pads[gBondViewCutscene->pad];
        else {
            if (g_CurrentSetup.boundpads == NULL) return 0;
            setupPad = (PadRecord *)&g_CurrentSetup.boundpads[
                getBoundPadNum(gBondViewCutscene->pad)];
        }
        if (setupPad->stan == NULL) return 0;
        snapshot->position[0] = gBondViewCutscene->pos.f[0];
        snapshot->position[1] = gBondViewCutscene->pos.f[1];
        snapshot->position[2] = gBondViewCutscene->pos.f[2];
        snapshot->anchor[0] = setupPad->pos.f[0];
        snapshot->anchor[1] = setupPad->pos.f[1];
        snapshot->anchor[2] = setupPad->pos.f[2];
        if (dword_CODE_bss_80079A18 == CAMERAMODE_INTRO) {
            if (g_CurrentPlayer == NULL) return 0;
            snapshot->look_direction[0] = g_CurrentPlayer->field_3C4
                - snapshot->position[0];
            snapshot->look_direction[1] = g_CurrentPlayer->field_3C8
                - snapshot->position[1];
            snapshot->look_direction[2] = g_CurrentPlayer->field_3CC
                - snapshot->position[2];
        } else {
            snapshot->look_direction[0] =
                cosf(gBondViewCutscene->verta)
                    * sinf(gBondViewCutscene->theta);
            snapshot->look_direction[1] = sinf(gBondViewCutscene->verta);
            snapshot->look_direction[2] =
                -cosf(gBondViewCutscene->verta)
                    * cosf(gBondViewCutscene->theta);
        }
        snapshot->up[1] = 1.0f;
        snapshot->pad_id = gBondViewCutscene->pad;
        snapshot->room = setupPad->stan->room;
        snapshot->valid = 1U;
        return 1;
    }

    if (g_CurrentSetup.pads == NULL) return 0;

    if (isNotBoundPad(dword_CODE_bss_80079A14))
        setupPad = &g_CurrentSetup.pads[dword_CODE_bss_80079A14];
    else {
        if (g_CurrentSetup.boundpads == NULL) return 0;
        setupPad = (PadRecord *)&g_CurrentSetup.boundpads[
            getBoundPadNum(dword_CODE_bss_80079A14)];
    }
    if (setupPad->stan == NULL) return 0;

    angle = flt_CODE_bss_80079A00;
    snapshot->anchor[0] = setupPad->pos.f[0];
    snapshot->anchor[1] = setupPad->pos.f[1];
    snapshot->anchor[2] = setupPad->pos.f[2];
    snapshot->position[0] = setupPad->pos.f[0]
        + sinf(angle) * flt_CODE_bss_80079A08 + cosf(angle) * 0.0f;
    snapshot->position[1] = setupPad->pos.f[1]
        + flt_CODE_bss_80079A10 + flt_CODE_bss_80079A0C;
    snapshot->position[2] = setupPad->pos.f[2]
        + cosf(angle) * flt_CODE_bss_80079A08 + sinf(angle) * 0.0f;
    /* bondviewFrozenMoveBond publishes pos2 - pos and the canonical default
     * frozen camera up vector to bondviewUpdateCameraMatrices. */
    snapshot->look_direction[0] = setupPad->pos.f[0]
        + cosf(angle) * 0.0f - snapshot->position[0];
    snapshot->look_direction[1] = setupPad->pos.f[1]
        + flt_CODE_bss_80079A10 - snapshot->position[1];
    snapshot->look_direction[2] = setupPad->pos.f[2]
        + sinf(angle) * 0.0f - snapshot->position[2];
    snapshot->up[1] = 1.0f;
    snapshot->pad_id = dword_CODE_bss_80079A14;
    snapshot->room = setupPad->stan->room;
    snapshot->valid = 1U;

    flt_CODE_bss_80079A00 += flt_CODE_bss_80079A04 * g_GlobalTimerDelta;
    while (flt_CODE_bss_80079A00 >= M_TAU_F)
        flt_CODE_bss_80079A00 -= M_TAU_F;
    while (flt_CODE_bss_80079A00 < 0.0f)
        flt_CODE_bss_80079A00 += M_TAU_F;
    return 1;
}

u32 bondviewGetCameraMode(void)
{
    return (u32)g_CameraMode;
}

void ge_original_dam_mission_return_title_exact(void)
{
    /* Preserve bossReturnTitleStage's unconditional title request. The exact
     * successful mutation now runs when its persistence provider is bound;
     * otherwise the platform frontier remains explicit. */
    if (bossGetStageNum() != LEVELID_CUBA && objectiveIsAllComplete()) {
        if (ge_original_mission_result_apply_exact(
                lvlGetSelectedDifficulty(), getMissiontimer()))
            ++ge_exit_snapshot.briefing_commits;
        else
            ++ge_exit_snapshot.briefing_frontiers;
    }
    bossRunTitleStage();
    ++ge_exit_snapshot.title_stage_requests;
}

void bossReturnTitleStage(void)
{
    ge_original_dam_mission_return_title_exact();
}

/* Exact AI_EndLevel body from chrai.c.  Keep the command's frame-buffer
 * synchronization here with the canonical boss service rather than teaching
 * the platform frontend how or when a mission ends. */
void ge_original_campaign_end_level_dispatch_exact(void)
{
    if (cameraBufferToggle)
    {
        if (cameraFrameCounter2 == FALSE)
        {
            cameraFrameCounter2 = TRUE;
        }
    }
    else
    {
        bossReturnTitleStage();
    }
}

#define currentPlayerIsFadeComplete \
    ge_current_player_is_fade_complete_exact
#define bossReturnTitleStage ge_original_dam_mission_return_title_exact
void ge_original_dam_mission_exit_process_input_exact(uint16_t buttons)
{
    if (g_CurrentPlayer == NULL) return;

    /* Exact post-MoveBond mission-exit block from
     * bondviewMovePlayerUpdateViewport. */
    if (stop_time_flag != 0)
    {
        if ((lvlGetControlsLockedFlag() == 0) && ((buttons & ~(g_CurrentPlayer->buttons_pressed) & (CONT_A | B_BUTTON | Z_TRIG | START_BUTTON | CONT_R | CONT_L))))
        {
            stop_time_flag = 2;

            if (currentPlayerIsFadeComplete())
            {
                if (g_CurrentPlayer->colourscreenfrac == 0.0f)
                {
                    currentPlayerSetFadeColour(0, 0, 0, 0.0f);
                    currentPlayerSetFadeFrac(60.0f, 1.0f);
                }
            }
            else
            {
                if (g_CurrentPlayer->colourfadefracnew == 0.0f)
                {
                    currentPlayerSetFadeFrac(g_CurrentPlayer->colourfadetime60, 1.0f);
                }
            }
        }

        if ((stop_time_flag == 2) && currentPlayerIsFadeComplete() && (g_CurrentPlayer->colourscreenfrac == 1.0f))
        {
            bossReturnTitleStage();
        }
    }

    ge_original_player_death_camera_tick_exact(buttons);

    g_CurrentPlayer->buttons_pressed = buttons;
}
#undef bossReturnTitleStage
#undef currentPlayerIsFadeComplete

void ge_original_dam_mission_exit_services_reset(void)
{
    memset(&ge_exit_snapshot, 0, sizeof(ge_exit_snapshot));
    cameraBufferToggle = 0;
    cameraFrameCounter2 = 0;
    g_CameraAfterCinema = CAMERAMODE_NONE;
    stop_time_flag = 0;
    is_timer_active = 1;
    g_CameraLookAtBondPad = NULL;
    gBondViewCutscene = NULL;
    dword_CODE_bss_80079A18 = CAMERAMODE_NONE;
    dword_CODE_bss_80079A1C = 0;
    camera_mode = CAMERAMODE_NONE;
    camera_80036438 = 0;
    credits_state = 0;
    credits_pointer = NULL;
    ge_credits_entry_count = 0U;
    ge_death_camera_timer = 0.0f;
    ge_death_previous_buttons = 0U;
    ge_original_mission_result_reset();
}

void ge_original_dam_mission_exit_services_tick(void)
{
    if (g_CurrentPlayer == NULL) return;
    ge_current_player_update_colour_screen_properties_exact();
    ge_current_player_tick_chr_fade_exact();
    ge_original_player_death_render_tick_exact();
    ++ge_exit_snapshot.fade_ticks;
}

void ge_original_dam_mission_exit_services_snapshot(
    GeOriginalDamMissionExitSnapshot *snapshot)
{
    if (snapshot == NULL) return;
    *snapshot = ge_exit_snapshot;
    snapshot->stop_time = stop_time_flag;
    snapshot->timer_active = is_timer_active;
    snapshot->camera_mode = g_CameraMode;
    if (g_CurrentPlayer != NULL) {
        snapshot->fade_fraction = g_CurrentPlayer->colourscreenfrac;
        snapshot->fade_red = g_CurrentPlayer->colourscreenred;
        snapshot->fade_green = g_CurrentPlayer->colourscreengreen;
        snapshot->fade_blue = g_CurrentPlayer->colourscreenblue;
        snapshot->fade_time = g_CurrentPlayer->colourfadetime60;
        snapshot->fade_time_max = g_CurrentPlayer->colourfadetimemax60;
    }
}
