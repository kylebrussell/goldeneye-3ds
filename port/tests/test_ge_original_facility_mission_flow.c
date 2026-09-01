#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bondview.h"
#include "chraction.h"
#include "chrai.h"
#include "ge_original_global_ai.h"
#include "gun.h"
#include "initanitable.h"
#include "lv.h"
#include "objective_status.h"
#include "player.h"
#include "propobj.h"

extern AIListRecord ailists[];
extern AIRecord ai_37[];
extern AIRecord ai_38[];
extern AIRecord ai_47[];
extern void ai(PropDefHeaderRecord *entity, PROP_TYPE entity_type);
extern s32 chraiGetAIListID(AIRecord *list, bool *is_global);

stagesetup g_CurrentSetup;
struct player *g_CurrentPlayer;
ChrRecord *g_ChrSlots;
s32 g_ClockTimer;
s32 g_GlobalTimer;
s32 cameraBufferToggle;
s32 cameraFrameCounter2;
s32 stop_time_flag;
s32 is_timer_active;
bool g_PlayerInvincible;
PadRecord *g_CameraLookAtBondPad;
CutsceneRecord *gBondViewCutscene;
enum CAMERAMODE dword_CODE_bss_80079A18;
s32 dword_CODE_bss_80079A1C;
ALBank *g_musicSfxBufferPtr;
struct ModelAnimation *animation_table_ptrs2[1];

static struct player player_state;
static PropRecord player_prop;
static PropRecord bond_prop;
static StandTile player_stan;
static PadRecord pads[311];
static StandTile pad_3501_stan;
static StandTile pad_8700_stan;
static ChrRecord stage_actor;
static ChrRecord bond_actor;
static ChrRecord visible_chr_slots[2];
static bool objectives_complete;
static int boss_return_count;
static int fade_to_black_count;
static int hide_weapon_count;
static int hud_lock_count;
static int removed_chr_item_count;

/* Exact small chraction services used by the three authored lists. */
void chrSetFlags2(ChrRecord *self, u8 flags2) { self->flags2 |= flags2; }
void chrUnsetFlags2(ChrRecord *self, u8 flags2) { self->flags2 &= ~flags2; }
s32 chrHasFlags2(ChrRecord *self, u8 flags2)
{
    return (self->flags2 & flags2) != 0;
}
void chrRestartTimer(ChrRecord *self)
{
    self->timer60 = 0;
    self->hidden |= CHRHIDDEN_TIMER_ACTIVE;
}
f32 chrGetTimer(ChrRecord *self) { return self->timer60 / CHRLV_FRAMERATE_F; }
s32 chrResolvePadId(ChrRecord *self, s32 padnum)
{
    if (padnum == PAD_PRESET1) padnum = self->padpreset1;
    return padnum;
}
PropRecord *getCurrentPlayerProp(void) { return g_CurrentPlayer->prop; }
bool check_if_actor_is_at_preset(ChrRecord *self, s32 padnum)
{
    PropRecord *bondprop = getCurrentPlayerProp();
    PadRecord *pad;
    padnum = chrResolvePadId(self, padnum);
    if (isNotBoundPad(padnum)) pad = &g_CurrentSetup.pads[padnum];
    else pad = (PadRecord *)&g_CurrentSetup.boundpads[getBoundPadNum(padnum)];
    return pad->stan && pad->stan->room == bondprop->stan->room;
}

ChrRecord *chrFindById(ChrRecord *self, s32 chrnum)
{
    if ((s8)chrnum == (s8)CHR_SELF) return self;
    if ((s8)chrnum == (s8)CHR_BOND_CINEMA) return &bond_actor;
    return NULL;
}

bool chrHasStoppedOrPatroling(ChrRecord *self)
{
    return self->actiontype == ACT_STAND || self->actiontype == ACT_PATROL;
}

s32 check_if_able_to_then_perform_animation(ChrRecord *self, s32 anim_id,
        s32 startframe, s32 endframe, u8 bitfield, s32 interpol_time60)
{
    (void)anim_id;
    (void)startframe;
    (void)endframe;
    (void)bitfield;
    (void)interpol_time60;
    self->actiontype = ACT_ANIM;
    return TRUE;
}

void chrSetStageFlags(ChrRecord *self, s32 flags) { (void)self; (void)flags; }
bool chrHasStageFlag(ChrRecord *self, s32 flags)
{
    (void)self;
    (void)flags;
    return FALSE;
}

bool objectiveIsAllComplete(void) { return objectives_complete; }

void currentPlayerSetFadeColour(s32 r, s32 g, s32 b, f32 frac)
{
    g_CurrentPlayer->colourscreenred = r;
    g_CurrentPlayer->colourscreengreen = g;
    g_CurrentPlayer->colourscreenblue = b;
    g_CurrentPlayer->colourscreenfrac = frac;
}

void currentPlayerSetFadeFrac(f32 maxfadetime, f32 frac)
{
    g_CurrentPlayer->colourfadetime60 = 0;
    g_CurrentPlayer->colourfadetimemax60 = maxfadetime;
    g_CurrentPlayer->colourfadefracnew = frac;
    if (frac == 1.0f) fade_to_black_count++;
}

ITEM_IDS getCurrentPlayerWeaponId(GUNHAND hand)
{
    (void)hand;
    return ITEM_WPPK;
}

void remove_item_in_hand(GUNHAND hand) { (void)hand; hide_weapon_count++; }
void currentPlayerUnEquipWeaponWrapper(GUNHAND hand, ITEM_IDS item)
{
    (void)hand;
    (void)item;
}

void ge_original_dam_mission_return_title_exact(void) { boss_return_count++; }
void gunSetSightVisible(s32 reason, s32 visible)
{
    (void)reason;
    (void)visible;
    hud_lock_count++;
}
void gunSetGunAmmoVisible(s32 reason, s32 visible)
{
    (void)reason;
    (void)visible;
}
void hudmsgsSetOff(s32 flags) { (void)flags; }
void bondviewSetUpperTextDisplayFlag(int flags) { (void)flags; }
void countdownTimerSetVisible(int bits, bool visible)
{
    (void)bits;
    (void)visible;
}
void ge_original_dam_mission_set_camera_posend_exact(int32_t mode)
{
    (void)mode;
}

/* Explicitly unbound service frontiers are canonical no-ops for absent tags. */
TagObjectRecord *sub_GAME_7F057080(s32 tag) { (void)tag; return NULL; }
ObjectRecord *objFindByTagId(s32 tag) { (void)tag; return NULL; }
ObjectRecord *setupGetPtrToCommandByIndex(s32 index)
{
    (void)index;
    return NULL;
}
s32 tagGetCommandIndex(ObjectRecord *tag) { (void)tag; return -1; }
void doorUpdateBbox(DoorRecord *door) { (void)door; }
void doorActivatePortal(DoorRecord *door) { (void)door; }
void door7F053B10(DoorRecord *door) { (void)door; }
void chrSetWeaponFlag4(ChrRecord *chr, GUNHAND hand)
{
    (void)chr;
    (void)hand;
    removed_chr_item_count++;
}

bool chrAdjustPosForSpawn(coord3d *pos, StandTile **stan, f32 angle, bool force)
{
    (void)pos;
    (void)stan;
    (void)angle;
    (void)force;
    return TRUE;
}
void sub_GAME_7F03D058(PropRecord *prop, bool unset)
{
    (void)prop;
    (void)unset;
}
void chrDetectRooms(ChrRecord *chr) { (void)chr; }
void setsubroty(Model *model, f32 angle) { (void)model; (void)angle; }
void setsuboffset(Model *model, coord3d *pos) { (void)model; (void)pos; }

s32 get_numguards(void) { return 2; }
void modelSetAnimation(Model *model, struct ModelAnimation *anim, s32 flip,
        f32 start, f32 speed, s32 merge)
{
    (void)model; (void)anim; (void)flip; (void)start; (void)speed; (void)merge;
}
void modelSetAnimEndFrame(Model *model, f32 endframe)
{
    (void)model;
    (void)endframe;
}

KeyRecord *weaponFindThrown(s32 weapon) { (void)weapon; return NULL; }
bool objIsHealthy(ObjectRecord *obj) { (void)obj; return TRUE; }
ALSoundState *sndPlaySfx(struct ALBankAlt_s *bank, s16 sound,
        ALSoundState *state)
{
    (void)bank; (void)sound; (void)state; return NULL;
}
u8 sndGetPlayingState(ALSoundState *state) { (void)state; return FALSE; }
void sndCreatePostEvent(ALSoundState *state, s16 type, s32 value)
{
    (void)state; (void)type; (void)value;
}
void sndDeactivate(ALSoundState *state) { (void)state; }
s32 lvlGetControlsLockedFlag(void) { return FALSE; }
s32 sub_GAME_7F0539E4(coord3d *pos) { (void)pos; return 0; }
char *langGet(s32 id) { (void)id; return NULL; }
void hudmsgTopShow(char *text) { (void)text; }

static AIRecord *setup_list(s32 id)
{
    s32 index;
    for (index = 0; ailists[index].ailist; index++)
        if (ailists[index].ID == id) return ailists[index].ailist;
    return NULL;
}

static void reset_flow(s16 room, bool complete)
{
    memset(&g_CurrentSetup, 0, sizeof(g_CurrentSetup));
    memset(&player_state, 0, sizeof(player_state));
    memset(&player_prop, 0, sizeof(player_prop));
    memset(&bond_prop, 0, sizeof(bond_prop));
    memset(&player_stan, 0, sizeof(player_stan));
    memset(pads, 0, sizeof(pads));
    memset(&pad_3501_stan, 0, sizeof(pad_3501_stan));
    memset(&pad_8700_stan, 0, sizeof(pad_8700_stan));
    memset(&stage_actor, 0, sizeof(stage_actor));
    memset(&bond_actor, 0, sizeof(bond_actor));
    memset(visible_chr_slots, 0, sizeof(visible_chr_slots));
    boss_return_count = 0;
    fade_to_black_count = 0;
    hide_weapon_count = 0;
    hud_lock_count = 0;
    removed_chr_item_count = 0;
    objectives_complete = complete;
    stop_time_flag = 0;
    g_PlayerInvincible = FALSE;

    g_CurrentSetup.ailists = ailists;
    g_CurrentSetup.pads = pads;
    pad_3501_stan.room = 0x35;
    pad_8700_stan.room = 0x87;
    pads[0x135].stan = &pad_3501_stan;
    pads[0x087].stan = &pad_8700_stan;
    pads[0x086].stan = &pad_8700_stan;
    pads[0x136].stan = &pad_3501_stan;
    player_stan.room = room;
    player_prop.stan = &player_stan;
    player_state.prop = &player_prop;
    player_state.colourfadetimemax60 = -1.0f;
    g_CurrentPlayer = &player_state;

    stage_actor.chrnum = CHR_OBJECTIVE;
    stage_actor.actiontype = ACT_STAND;
    stage_actor.ailist = ailistFindById(0x1006);
    bond_actor.chrnum = CHR_BOND_CINEMA;
    bond_actor.actiontype = ACT_STAND;
    bond_actor.prop = &bond_prop;
    g_ChrSlots = visible_chr_slots;
}

static void finish_fade_between_ticks(void)
{
    if (g_CurrentPlayer->colourfadetimemax60 >= 0.0f)
        g_CurrentPlayer->colourfadetimemax60 = -1.0f;
}

static void run_complete_cutscene_branch(s16 room,
        AIRecord *expected_cutscene)
{
    s32 ticks;
    reset_flow(room, TRUE);
    assert(stage_actor.ailist == ai_47);
    for (ticks = 0; ticks < 12 && bond_actor.ailist == NULL; ticks++) {
        ai((PropDefHeaderRecord *)&stage_actor, PROP_TYPE_CHR);
        finish_fade_between_ticks();
    }
    assert(ticks >= 5);
    assert(bond_actor.ailist == expected_cutscene);
    assert(stop_time_flag == TRUE);
    assert(g_PlayerInvincible == TRUE);
    assert(fade_to_black_count == 1);
    assert(hud_lock_count == 1);
    assert(hide_weapon_count == 2);
    assert(boss_return_count == 0);

    /* Continue the selected authored list through teleport/animation/timer,
     * stopped-animation wait, fade, and unchanged AI_EndLevel. */
    ai((PropDefHeaderRecord *)&bond_actor, PROP_TYPE_CHR);
    assert(bond_actor.actiontype == ACT_ANIM);
    assert((bond_actor.hidden & CHRHIDDEN_TIMER_ACTIVE) != 0);
    bond_actor.timer60 = 10000;
    ai((PropDefHeaderRecord *)&bond_actor, PROP_TYPE_CHR);
    bond_actor.actiontype = ACT_STAND;
    ai((PropDefHeaderRecord *)&bond_actor, PROP_TYPE_CHR);
    finish_fade_between_ticks();
    ai((PropDefHeaderRecord *)&bond_actor, PROP_TYPE_CHR);
    assert(boss_return_count == 1);
    assert(bond_actor.ailist == ge_original_global_ai_find(1));
    if (expected_cutscene == ai_38) assert(removed_chr_item_count == 1);
}

int main(void)
{
    bool is_global = FALSE;
    s32 ticks;

    assert(setup_list(0x1006) == ai_47);
    assert(setup_list(0x0426) == ai_37);
    assert(setup_list(0x0427) == ai_38);
    assert(ailistFindById(15) == ge_original_global_ai_find(15));
    assert(chraiGetAIListID(ge_original_global_ai_find(15), &is_global) == 15);
    assert(is_global);

    /* Pad 0x3501 takes the unflagged authored camera/cutscene branch (ai_38). */
    run_complete_cutscene_branch(0x35, ai_38);
    /* Pad 0x8700 sets flags2 bit 1 and takes the distinct ai_37 branch. */
    run_complete_cutscene_branch(0x87, ai_37);

    /* An incomplete objective falls through to exact global list 15 and its
     * unchanged AI_EndLevel -> bossReturnTitleStage failure chain. */
    reset_flow(0x35, FALSE);
    for (ticks = 0; ticks < 6 && boss_return_count == 0; ticks++) {
        ai((PropDefHeaderRecord *)&stage_actor, PROP_TYPE_CHR);
        finish_fade_between_ticks();
    }
    assert(boss_return_count == 1);
    assert(stage_actor.ailist == ge_original_global_ai_find(1));
    assert(g_PlayerInvincible == TRUE);

    printf("Facility mission flow: exact 0x1006 multi-tick branches ai_37/ai_38 "
           "and global AI_EndLevel path ok\n");
    return 0;
}
