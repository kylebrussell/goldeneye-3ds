#include "ge_original_dam_guard_ai_tick.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

s32 g_GlobalTimer;

#include <bondconstants.h>
#include <bondtypes.h>
#include <bondaicommands.h>
#include "game/chraction.h"
#include "game/initanitable.h"
#include "game/lv.h"
#include "game/player.h"
#include "game/propobj.h"

s32 g_ClockTimer = 1;
stagesetup g_CurrentSetup;
static s32 stand_tick_count;
static s32 walk_count;
static s32 walk_pad;
static SPEED walk_speed;
static s32 animation_count;
static s32 animation_id;
static s32 animation_start;
static s32 animation_end;
static s32 animation_interpolation;
static s32 door_close_count;
static s32 attack_attempt_count;
static bool sees_bond;
static bool could_see_bond;
static s32 see_check_count;
static s32 could_see_check_count;
static u32 random_value;
static f32 bond_distance;
static bool alarm_active;
static s32 alarm_start_attempt_count;
static s32 alarm_start_pad;
static bool alarm_start_succeeds;
static s32 surprised_look_count;
static bool surprised_look_succeeds;
static s8 num_arghs;
static s32 pad_distance_check_count;
static s32 pad_distance_id;
static f32 pad_distance;
static s32 grenade_construction_count;
static s32 grenade_construction_model;
static s32 grenade_construction_item;
static s32 grenade_construction_flags;
static bool grenade_construction_succeeds;
static DoorRecord test_door;
static MonitorObjRecord test_monitor;
static s32 monitor_image_count;
static void *monitor_image_record;
static s32 monitor_image_num;
static Model test_models[5];
static PropRecord fresh_grenade_prop;
static WeaponObjRecord fresh_grenade_weapon;
static struct player test_player;
ChrRecord *g_ChrSlots;
s32 g_NumChrSlots;
ChrRecord *g_ActiveChrs;
s32 g_ActiveChrsCount;
struct player *g_CurrentPlayer = &test_player;
static u8 target_0408[] = { AI_Yield, AI_EndList };
static u8 target_040c[] = { AI_Yield, AI_EndList };
static u8 see_branch_list[] = {
    AI_IFISeeBond, 0x44,
    AI_Yield,
    AI_Label, 0x44,
    AI_Yield,
    AI_EndList,
};
static u8 could_see_branch_list[] = {
    AI_IFICouldSeeBond, 0x45,
    AI_Yield,
    AI_Label, 0x45,
    AI_Yield,
    AI_EndList,
};
static u8 grenade_branch_list[] = {
    AI_TRYThrowingGrenade, 0x46,
    AI_Yield,
    AI_Label, 0x46,
    AI_Yield,
    AI_EndList,
};
static u8 monitor_bank_list[] = {
    AI_TvChangeScreenBank, 0x11, 0x00, 0x30,
    AI_Yield,
    AI_EndList,
};
static u8 alertness_list[] = {
    AI_SetMyAlertness, 0x64,
    AI_Yield,
    AI_EndList,
};
static u8 door_state_list[] = {
    AI_IFDoorStateEqual, 0x08, AI_DOOR_STATE_CLOSED, 0x47,
    AI_Yield,
    AI_Label, 0x47,
    AI_Yield,
    AI_EndList,
};
static u8 alarm_raiser_list[] = {
    AI_LookSurprised,
    AI_RunToPad, 0x23, 0x28,
    AI_IFChrDistanceToPadGreaterThanDecimeter,
        (u8)CHR_SELF, 0x00, 0x0a, 0x23, 0x28, 0x48,
    AI_TRYTriggeringAlarmAtPad, 0x23, 0x28, 0x49,
    AI_Yield,
    AI_Label, 0x48,
    AI_Yield,
    AI_Label, 0x49,
    AI_Yield,
    AI_EndList,
};
static u8 argh_branch_list[] = {
    AI_IFMyNumArghsGreaterThan, 0x01, 0x4a,
    AI_Yield,
    AI_Label, 0x4a,
    AI_Yield,
    AI_EndList,
};
static AIListRecord test_lists[] = {
    { (AIRecord *)target_0408, 0x0408 },
    { (AIRecord *)target_040c, 0x040c },
    { NULL, 0 }
};
ModelAnimation *animation_table_ptrs2[1];
static struct animation_table_data test_animation_data;
struct animation_table_data *ptr_animation_table = &test_animation_data;

u32 randomGetNext(void)
{
    return random_value;
}

f32 chrGetDistanceToBond(ChrRecord *self)
{
    assert(self != NULL);
    return bond_distance;
}

PropRecord *something_with_generating_object(ChrRecord *self, s32 propid,
        ITEM_IDS itemid, s32 flags, WeaponObjRecord *weapon,
        ItemModelFileRecord *prop_header)
{
    GUNHAND hand;
    assert(self != NULL);
    assert(weapon == NULL && prop_header == NULL);
    grenade_construction_count++;
    grenade_construction_model = propid;
    grenade_construction_item = itemid;
    grenade_construction_flags = flags;
    if (!grenade_construction_succeeds) return NULL;
    memset(&fresh_grenade_prop, 0, sizeof(fresh_grenade_prop));
    memset(&fresh_grenade_weapon, 0, sizeof(fresh_grenade_weapon));
    fresh_grenade_prop.weapon = &fresh_grenade_weapon;
    fresh_grenade_weapon.weaponnum = itemid;
    hand = (flags & 0x10000000) != 0 ? GUNLEFT : GUNRIGHT;
    self->weapons_held[hand] = &fresh_grenade_prop;
    self->model->gunhand = hand;
    return &fresh_grenade_prop;
}

s32 chrCheckTargetInSight(ChrRecord *self)
{
    assert(self != NULL);
    see_check_count++;
    return sees_bond;
}

bool chrCanSeeBond(ChrRecord *self)
{
    assert(self != NULL);
    could_see_check_count++;
    return could_see_bond;
}

bool if_actor_able_set_on_path(ChrRecord *self, s32 pathid)
{
    (void)self;
    (void)pathid;
    return TRUE;
}

s32 alarmIsActive(void)
{
    return alarm_active;
}

s8 chrGetNumArghs(ChrRecord *self)
{
    assert(self != NULL);
    return num_arghs;
}

f32 chrGetDistanceToPad(ChrRecord *self, s32 padid)
{
    assert(self != NULL);
    pad_distance_check_count++;
    pad_distance_id = padid;
    return pad_distance;
}

bool chrTrySurprisedLookAround(ChrRecord *self)
{
    assert(self != NULL);
    surprised_look_count++;
    return surprised_look_succeeds;
}

bool chrTryStartAlarm(ChrRecord *self, s32 padid)
{
    assert(self != NULL);
    alarm_start_attempt_count++;
    alarm_start_pad = padid;
    return alarm_start_succeeds;
}

bool chrSawTargetRecently(ChrRecord *self)
{
    return self->chrflags & CHRFLAG_WAS_HIT;
}

s8 chrGetNumCloseArghs(ChrRecord *self)
{
    return self->numclosearghs;
}

u32 bondwalkItemCheckBitflags(ITEM_IDS item, u32 flags)
{
    (void)item;
    (void)flags;
    return FALSE;
}

void modelSetAnimation(Model *model, ModelAnimation *animation, s32 flip,
        f32 startframe, f32 speed, f32 merge)
{
    size_t index;
    (void)model; (void)flip; (void)speed;
    animation_count++;
    animation_start = (s32)startframe;
    animation_interpolation = (s32)merge;
    animation_id = -1;
    for (index = 0; index < 184U; index++) {
        if ((ModelAnimation *)(uintptr_t)(u32)animation_table_ptrs1[index]
                == animation) {
            animation_id = (s32)index;
            break;
        }
    }
}

void modelSetAnimEndFrame(Model *model, f32 endframe)
{
    (void)model;
    animation_end = (s32)endframe;
}

void modelSetAnimLooping(Model *model, f32 loopframe, f32 mergeframe)
{
    (void)model; (void)loopframe; (void)mergeframe;
}

ModelAnimation *objecthandlerGetModelAnim(Model *model)
{
    (void)model;
    return NULL;
}

f32 modelGetAnimFrame(Model *model)
{
    (void)model;
    return 0.0f;
}

f32 modelGetAnimEndFrame(Model *model)
{
    (void)model;
    return 0.0f;
}

f32 modelGetAnimSpeed(Model *model)
{
    (void)model;
    return 0.5f;
}

DIFFICULTY lvlGetSelectedDifficulty(void)
{
    return DIFFICULTY_AGENT;
}

PropRecord *chrGetEquippedWeaponProp(ChrRecord *self, GUNHAND hand)
{
    return self->weapons_held[hand];
}

void weaponSetGunfireVisible(PropRecord *prop, s32 visible)
{
    (void)prop;
    (void)visible;
}

bool chrGoToPad(ChrRecord *self, s32 padid, SPEED speed)
{
    (void)self;
    walk_count++;
    walk_pad = padid;
    walk_speed = speed;
    return TRUE;
}

bool actor_aim_at_actor(ChrRecord *self, s32 attack_type, s32 target)
{
    assert(self != NULL);
    assert(attack_type == 1);
    assert(target == 0);
    attack_attempt_count++;
    return FALSE;
}

bool actor_kneel_aim_at_actor(ChrRecord *self, s32 attack_type, s32 target)
{
    assert(self != NULL);
    assert(attack_type == 1);
    assert(target == 0);
    attack_attempt_count++;
    return FALSE;
}

bool actor_steps_sideways(ChrRecord *self) { (void)self; return FALSE; }
bool actor_hops_sideways(ChrRecord *self) { (void)self; return FALSE; }
bool actor_jogs_sideways(ChrRecord *self) { (void)self; return FALSE; }
bool actor_walks_and_fires(ChrRecord *self) { (void)self; return FALSE; }
bool actor_runs_and_fires(ChrRecord *self) { (void)self; return FALSE; }
bool actor_rolls_fires_crouched(ChrRecord *self)
{
    (void)self;
    return FALSE;
}

bool chrGoToBond(ChrRecord *self, SPEED speed)
{
    (void)self;
    (void)speed;
    return FALSE;
}

void ge_original_stage_monitor_set_image_exact(
        void *monitor, int32_t image_num)
{
    monitor_image_count++;
    monitor_image_record = monitor;
    monitor_image_num = image_num;
}

bool chrTrySurprisedOneHand(ChrRecord *self)
{
    assert(self != NULL);
    return TRUE;
}

ObjectRecord *objFindByTagId(s32 tag_id)
{
    if (tag_id == 8) return (ObjectRecord *)&test_door;
    assert(tag_id == 0x11);
    return (ObjectRecord *)&test_monitor;
}

void doorActivate(DoorRecord *door, DOORSTATE state)
{
    assert(door == &test_door && state == DOORSTATE_CLOSING);
    door_close_count++;
}

#define TICK_STUB(name) void name(ChrRecord *self) { (void)self; }
void chrlvTickStand(ChrRecord *self) { (void)self; stand_tick_count++; }
TICK_STUB(chrlvTickKneel)
TICK_STUB(chrlvTickAnim)
TICK_STUB(chrlvTickDie)
TICK_STUB(chrlvTickArgh)
TICK_STUB(chrlvTickPreArgh)
TICK_STUB(chrlvTickSidestep)
TICK_STUB(chrlvTickJumpout)
TICK_STUB(chrlvTickDead)
TICK_STUB(chrlvTickAttack)
TICK_STUB(chrlvTickAttackWalk)
TICK_STUB(chrlvTickAttackRoll)
TICK_STUB(chrlvTickRunPos)
TICK_STUB(chrlvTickPatrol)
TICK_STUB(chrlvTickGoPos)
TICK_STUB(chrlvTickSurrender)
TICK_STUB(chrlvTickTest)
TICK_STUB(chrlvTickSurprised)
TICK_STUB(chrlvTickStartAlarm)
TICK_STUB(chrlvTickThrowGrenade)
TICK_STUB(chrlvTickBondIntro)
TICK_STUB(chrlvTickBondDieRemoved)

static void reset_chr(ChrRecord *chr, AIRecord *list)
{
    memset(chr, 0, sizeof(*chr));
    chr->ailist = list;
    chr->actiontype = ACT_INIT;
    chr->sleep = 0;
    chr->chrseeshot = -1;
    chr->chrseedie = -1;
    chr->chrnum = 7;
    g_ChrSlots = chr;
    g_NumChrSlots = 1;
    g_ActiveChrs = NULL;
    g_ActiveChrsCount = 0;
}

int main(void)
{
    ChrRecord combat;
    ChrRecord timed;
    ChrRecord walking;
    PropRecord door_prop;
    ChrRecord sight;
    ChrRecord grenade_guard;
    ChrRecord monitor_actor;
    ChrRecord door_state_actor;
    ChrRecord alarm_raiser;
    PropRecord held_grenade_prop;
    WeaponObjRecord held_grenade_weapon;
    PropRecord held_rifle_prop;
    WeaponObjRecord held_rifle_weapon;

    memset(&test_door, 0, sizeof(test_door));
    memset(&test_monitor, 0, sizeof(test_monitor));
    memset(&door_prop, 0, sizeof(door_prop));
    door_prop.type = PROP_TYPE_DOOR;
    test_door.type = PROPDEF_DOOR;
    test_door.prop = &door_prop;
    test_monitor.type = PROPDEF_MONITOR;
    test_monitor.prop = &door_prop;
    g_CurrentSetup.ailists = test_lists;

    reset_chr(&combat, (AIRecord *)ge_original_dam_guard_ai_040d);
    combat.model = &test_models[0];
    combat.model->playspeed = 1.0f;
    ge_original_dam_guard_action_tick_exact(&combat);
    assert(combat.aioffset == 15);
    assert(fabsf(combat.maxdamage - 4.0f) < 0.0001f);
    assert(fabsf(combat.damage + 2.0f) < 0.0001f);
    /* Authored 0x20000000 is emitted by CharArrayFrom32Rev and consumed by
     * the canonical byte reader as CHRFLAG_00000020. */
    assert((combat.chrflags & 0x20U) != 0U);
    assert(combat.actiontype == ACT_STAND);
    assert(stand_tick_count == 1);

    /* Continue the authored 0x040d list from its first yield. Each alert is
     * consumed by the unchanged command body, reaches the authentic attack
     * and grenade decisions, then commits the Yield after label 0x0a. */
    combat.chrflags |= CHRFLAG_NEAR_MISS;
    ge_original_dam_guard_action_tick_exact(&combat);
    assert(attack_attempt_count == 1);
    assert((combat.chrflags & CHRFLAG_NEAR_MISS) == 0);
    assert(combat.aioffset == 46);

    combat.aioffset = 15;
    combat.chrseeshot = 2;
    ge_original_dam_guard_action_tick_exact(&combat);
    assert(attack_attempt_count == 2);
    assert(combat.chrseeshot == CHR_FREE);
    assert(combat.aioffset == 46);

    combat.aioffset = 15;
    combat.chrseedie = 3;
    ge_original_dam_guard_action_tick_exact(&combat);
    assert(attack_attempt_count == 3);
    assert(combat.chrseedie == CHR_FREE);
    assert(combat.aioffset == 46);

    reset_chr(&timed, (AIRecord *)ge_original_dam_guard_ai_0413);
    timed.model = &test_models[1];
    timed.model->playspeed = 1.0f;
    ge_original_dam_guard_action_tick_exact(&timed);
    assert(timed.aioffset == 4);
    assert(timed.timer60 == 0);
    assert((timed.hidden & CHRHIDDEN_TIMER_ACTIVE) != 0);
    ge_original_dam_guard_action_tick_exact(&timed);
    assert(timed.aioffset == 4 && timed.timer60 == 1);
    timed.timer60 = 121;
    animation_count = 0;
    ge_original_dam_guard_action_tick_exact(&timed);
    assert(timed.aioffset == 25);
    assert(animation_count == 1 && animation_id == 0xac);
    assert(animation_start == 0 && animation_end == 0x45);
    assert(animation_interpolation == 0x10 && timed.actiontype == ACT_ANIM);
    timed.actiontype = ACT_STAND;
    ge_original_dam_guard_action_tick_exact(&timed);
    assert(timed.aioffset == 39);
    assert(door_close_count == 1 && walk_pad == 5 && walk_speed == SPEED_WALK);
    timed.actiontype = ACT_STAND;
    ge_original_dam_guard_action_tick_exact(&timed);
    assert(timed.act_stand.face_entitytype == 1
           && timed.act_stand.face_entityid == 0);
    assert(timed.ailist == (AIRecord *)target_040c && timed.aioffset == 1);

    reset_chr(&walking, (AIRecord *)ge_original_dam_guard_ai_0414);
    walking.model = &test_models[2];
    walking.model->playspeed = 1.0f;
    ge_original_dam_guard_action_tick_exact(&walking);
    assert(walking.aioffset == 6);
    assert(walk_count == 2 && walk_pad == 4 && walk_speed == SPEED_WALK);
    assert(walking.actiontype == ACT_STAND);
    ge_original_dam_guard_action_tick_exact(&walking);
    assert(walking.ailist == (AIRecord *)target_0408 && walking.aioffset == 1);

    /* Dam's monitor-loop actor reaches this exact command before its first
     * Yield.  Preserve the authored tag/bank operands and advance by the
     * original four-byte record so the loop can continue next frame. */
    reset_chr(&monitor_actor, (AIRecord *)monitor_bank_list);
    monitor_image_count = 0;
    ge_original_dam_guard_ai_interpret_exact(
        (PropDefHeaderRecord *)&monitor_actor, PROP_TYPE_CHR);
    assert(monitor_actor.aioffset == 5);
    assert(monitor_image_count == 1);
    assert(monitor_image_record == &test_monitor.Monitor.cmdlist);
    assert(monitor_image_num == 0x30);

    /* Dam's live guard lists execute AI_SetMyAlertness heavily.  Retain the
     * unchanged two-byte command body and prove it continues to the authored
     * Yield rather than terminating at the bounded dispatch frontier. */
    reset_chr(&monitor_actor, (AIRecord *)alertness_list);
    monitor_actor.alertness = 0;
    ge_original_dam_guard_ai_interpret_exact(
        (PropDefHeaderRecord *)&monitor_actor, PROP_TYPE_CHR);
    assert(monitor_actor.alertness == 0x64);
    assert(monitor_actor.aioffset == 3);

    reset_chr(&door_state_actor, (AIRecord *)door_state_list);
    test_door.openstate = DOORSTATE_STATIONARY;
    test_door.openPosition = 0.0f;
    ge_original_dam_guard_ai_interpret_exact(
        (PropDefHeaderRecord *)&door_state_actor, PROP_TYPE_CHR);
    assert(door_state_actor.aioffset == 8);
    door_state_actor.aioffset = 0;
    test_door.openstate = DOORSTATE_OPENING;
    ge_original_dam_guard_ai_interpret_exact(
        (PropDefHeaderRecord *)&door_state_actor, PROP_TYPE_CHR);
    assert(door_state_actor.aioffset == 5);

    /* Dam's authored global list 9 uses this exact command sequence after
     * spotting Bond: flustered look, run to the alarm pad, wait until close,
     * then start the canonical alarm action.  Exercise both the wait and
     * successful activation branches so an omitted opcode cannot strand the
     * live alarm-raiser guard at an unknown command. */
    reset_chr(&alarm_raiser, (AIRecord *)alarm_raiser_list);
    alarm_raiser.model = &test_models[0];
    surprised_look_count = 0;
    surprised_look_succeeds = TRUE;
    walk_count = 0;
    pad_distance_check_count = 0;
    pad_distance = 200.0f;
    alarm_start_attempt_count = 0;
    alarm_start_succeeds = TRUE;
    ge_original_dam_guard_ai_interpret_exact(
        (PropDefHeaderRecord *)&alarm_raiser, PROP_TYPE_CHR);
    assert(alarm_raiser.aioffset == 19);
    assert(surprised_look_count == 1);
    assert(walk_count == 1 && walk_pad == PAD_PRESET1
           && walk_speed == SPEED_RUN);
    assert(pad_distance_check_count == 1
           && pad_distance_id == PAD_PRESET1);
    assert(alarm_start_attempt_count == 0);

    alarm_raiser.aioffset = 4;
    pad_distance = 50.0f;
    ge_original_dam_guard_ai_interpret_exact(
        (PropDefHeaderRecord *)&alarm_raiser, PROP_TYPE_CHR);
    assert(alarm_raiser.aioffset == 22);
    assert(alarm_start_attempt_count == 1
           && alarm_start_pad == PAD_PRESET1);

    reset_chr(&alarm_raiser, (AIRecord *)argh_branch_list);
    num_arghs = 1;
    ge_original_dam_guard_ai_interpret_exact(
        (PropDefHeaderRecord *)&alarm_raiser, PROP_TYPE_CHR);
    assert(alarm_raiser.aioffset == 4);
    alarm_raiser.aioffset = 0;
    num_arghs = 2;
    ge_original_dam_guard_ai_interpret_exact(
        (PropDefHeaderRecord *)&alarm_raiser, PROP_TYPE_CHR);
    assert(alarm_raiser.aioffset == 7);

    /* The unchanged conditional bodies must commit the post-Yield offset on
     * both fallthrough and goto paths.  This is the scheduling contract that
     * keeps the authored 0x040d loop from running past its frame budget. */
    reset_chr(&sight, (AIRecord *)see_branch_list);
    sees_bond = FALSE;
    ge_original_dam_guard_ai_interpret_exact(
        (PropDefHeaderRecord *)&sight, PROP_TYPE_CHR);
    assert(sight.aioffset == 3 && see_check_count == 1);
    sight.aioffset = 0;
    sees_bond = TRUE;
    ge_original_dam_guard_ai_interpret_exact(
        (PropDefHeaderRecord *)&sight, PROP_TYPE_CHR);
    assert(sight.aioffset == 6 && see_check_count == 2);

    reset_chr(&sight, (AIRecord *)could_see_branch_list);
    could_see_bond = FALSE;
    ge_original_dam_guard_ai_interpret_exact(
        (PropDefHeaderRecord *)&sight, PROP_TYPE_CHR);
    assert(sight.aioffset == 3 && could_see_check_count == 1);
    sight.aioffset = 0;
    could_see_bond = TRUE;
    ge_original_dam_guard_ai_interpret_exact(
        (PropDefHeaderRecord *)&sight, PROP_TYPE_CHR);
    assert(sight.aioffset == 6 && could_see_check_count == 2);

    /* Exact grenade eligibility gates: a zero probability rejects before
     * distance/equipment work, and a qualifying roll still rejects within
     * the canonical ten-unit safety radius. */
    reset_chr(&grenade_guard, (AIRecord *)grenade_branch_list);
    grenade_guard.model = &test_models[3];
    grenade_guard.grenadeprob = 0;
    random_value = 0;
    bond_distance = 100.0f;
    grenade_construction_count = 0;
    ge_original_dam_guard_ai_interpret_exact(
        (PropDefHeaderRecord *)&grenade_guard, PROP_TYPE_CHR);
    assert(grenade_guard.aioffset == 3);
    assert(grenade_construction_count == 0);

    grenade_guard.aioffset = 0;
    grenade_guard.grenadeprob = 255;
    random_value = 254;
    bond_distance = 9.0f;
    ge_original_dam_guard_ai_interpret_exact(
        (PropDefHeaderRecord *)&grenade_guard, PROP_TYPE_CHR);
    assert(grenade_guard.aioffset == 3);
    assert(grenade_construction_count == 0);

    /* With both hands occupied by non-grenades the unchanged service cannot
     * create a third held item, so the opcode follows its fallthrough Yield. */
    memset(&held_rifle_prop, 0, sizeof(held_rifle_prop));
    memset(&held_rifle_weapon, 0, sizeof(held_rifle_weapon));
    held_rifle_prop.weapon = &held_rifle_weapon;
    held_rifle_weapon.weaponnum = ITEM_AK47;
    grenade_guard.aioffset = 0;
    grenade_guard.weapons_held[GUNRIGHT] = &held_rifle_prop;
    grenade_guard.weapons_held[GUNLEFT] = &held_rifle_prop;
    bond_distance = 100.0f;
    ge_original_dam_guard_ai_interpret_exact(
        (PropDefHeaderRecord *)&grenade_guard, PROP_TYPE_CHR);
    assert(grenade_guard.aioffset == 3);
    assert(grenade_construction_count == 0);

    /* An existing grenade takes the exact right-hand branch, begins the
     * original throw animation, then the interpreter goes to the label and
     * commits the post-label Yield offset. */
    memset(&held_grenade_prop, 0, sizeof(held_grenade_prop));
    memset(&held_grenade_weapon, 0, sizeof(held_grenade_weapon));
    held_grenade_prop.weapon = &held_grenade_weapon;
    held_grenade_weapon.weaponnum = ITEM_GRENADE;
    grenade_guard.aioffset = 0;
    grenade_guard.actiontype = ACT_STAND;
    grenade_guard.weapons_held[GUNRIGHT] = &held_grenade_prop;
    grenade_guard.weapons_held[GUNLEFT] = NULL;
    grenade_guard.model->gunhand = GUNRIGHT;
    ge_original_dam_guard_ai_interpret_exact(
        (PropDefHeaderRecord *)&grenade_guard, PROP_TYPE_CHR);
    assert(grenade_guard.aioffset == 6);
    assert(grenade_guard.actiontype == ACT_THROWGRENADE);
    assert(animation_end == 193);

    /* The fresh-item branch uses the exact PROP_CHRGRENADE/ITEM_GRENADE
     * request, marks the canonical hidden-held bit, equips the empty hand and
     * starts at frame zero. The host fixture supplies a concrete native prop;
     * production deliberately requires the shared object service to do so. */
    reset_chr(&grenade_guard, (AIRecord *)grenade_branch_list);
    grenade_guard.model = &test_models[4];
    grenade_guard.grenadeprob = 255;
    grenade_guard.weapons_held[GUNRIGHT] = &held_rifle_prop;
    bond_distance = 100.0f;
    grenade_construction_succeeds = TRUE;
    grenade_construction_count = 0;
    ge_original_dam_guard_ai_interpret_exact(
        (PropDefHeaderRecord *)&grenade_guard, PROP_TYPE_CHR);
    assert(grenade_guard.aioffset == 6);
    assert(grenade_construction_count == 1);
    assert(grenade_construction_model == PROP_CHRGRENADE);
    assert(grenade_construction_item == ITEM_GRENADE);
    assert(grenade_construction_flags == 0x10000000);
    assert(grenade_guard.weapons_held[GUNLEFT] == &fresh_grenade_prop);
    assert((fresh_grenade_weapon.runtime_bitflags & 0x800U) != 0U);
    assert(grenade_guard.actiontype == ACT_THROWGRENADE);
    assert(animation_start == 0 && animation_end == 193);

    puts("exact Dam guard AI/action tick: 040d combat init, 0413 timed "
         "animation/gate/walk/facing, 0414 walk-to-pad/jump, exact sight "
         "alarm-raiser, and grenade eligibility/construction/goto/yield "
         "ordering retained");
    return 0;
}
