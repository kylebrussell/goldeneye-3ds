#include "ge_original_dam_guard_ai_tick.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <bondconstants.h>
#include <bondtypes.h>
#include "game/initanitable.h"
#include "game/chraction.h"
#include "game/player.h"
#include "game/propobj.h"
#include "music.h"
#include "snd.h"

static struct animation_table_data animation_data;
static struct player player;
static PropRecord player_prop;
static f32 anim_frame;
static f32 anim_end;
static ModelAnimation *active_animation;
static f32 subroty;
static unsigned collision_calls;
static unsigned animation_sets;
static unsigned idle_sound_calls;
static struct weapon_firing_animation_table attack_animation;
static struct anim_group_info attack_group;

struct animation_table_data *ptr_animation_table = &animation_data;
struct player *g_CurrentPlayer = &player;
ALBank *g_musicSfxBufferPtr;
f32 g_GlobalTimerDelta = 1.0f;
s32 D_80048380;
s32 g_ClockTimer = 1;
stagesetup g_CurrentSetup;
ChrRecord *g_ChrSlots;
s32 g_NumChrSlots;
ChrRecord *g_ActiveChrs;
s32 g_ActiveChrsCount;
struct anim_group_info *ptr_rifle_firing_animation_groups[32];
struct anim_group_info *ptr_pistol_firing_animation_groups[32];
struct anim_group_info *ptr_doubles_firing_animation_groups[32];

u32 randomGetNext(void)
{
    return 0;
}

f32 modelGetAnimFrame(Model *model)
{
    (void)model;
    return anim_frame;
}

f32 modelGetAnimEndFrame(Model *model)
{
    (void)model;
    return anim_end;
}

ModelAnimation *objecthandlerGetModelAnim(Model *model)
{
    (void)model;
    return active_animation;
}

void modelSetAnimation(Model *model, ModelAnimation *animation, s32 flip,
        f32 startframe, f32 speed, f32 merge)
{
    (void)model;
    (void)flip;
    (void)startframe;
    (void)speed;
    (void)merge;
    active_animation = animation;
    animation_sets++;
}

void modelSetAnimEndFrame(Model *model, f32 endframe)
{
    (void)model;
    anim_end = endframe;
}

void modelSetAnimLooping(Model *model, f32 loopframe, f32 mergeframe)
{
    (void)model;
    (void)loopframe;
    (void)mergeframe;
}

f32 getsubroty(Model *model)
{
    (void)model;
    return subroty;
}

void setsubroty(Model *model, f32 value)
{
    (void)model;
    subroty = value;
}

union ModelRwData *modelGetNodeRwData(Model *model, ModelNode *node)
{
    static union ModelRwData rwdata;
    (void)model;
    (void)node;
    return &rwdata;
}

PropRecord *getCurrentPlayerProp(void)
{
    return &player_prop;
}

f32 chrlvPathingCollisionRelated(PropRecord *prop, f32 angle,
        f32 distance, s32 cdtypes, f32 height, f32 radius)
{
    (void)prop;
    (void)angle;
    (void)distance;
    (void)cdtypes;
    (void)height;
    (void)radius;
    collision_calls++;
    return 1000.0f;
}

f32 chrGetDistanceToBond(ChrRecord *guard)
{
    (void)guard;
    return 10000.0f;
}

ALSoundState *sndPlaySfx(struct ALBankAlt_s *bank, s16 sound,
        ALSoundState *pending)
{
    (void)bank;
    (void)sound;
    idle_sound_calls++;
    return pending;
}

void chrobjSndCreatePostEventDefault(ALSoundState *state, coord3d *position)
{
    (void)state;
    (void)position;
}

void modelSetAnimTranslationScale(Model *model, f32 scale)
{
    (void)model;
    (void)scale;
}

u32 bondwalkItemCheckBitflags(ITEM_IDS item, u32 flags)
{
    return item == ITEM_AK47 && flags == WEAPONSTATBITFLAG_HOLD_AS_GUN;
}

s8 bondwalkItemGetAutomaticFiringRate(ITEM_IDS item)
{
    (void)item;
    return -1;
}

PropRecord *chrGetEquippedWeaponProp(ChrRecord *guard, GUNHAND hand)
{
    return guard->weapons_held[hand];
}

void weaponSetGunfireVisible(PropRecord *weapon, s32 visible)
{
    (void)weapon;
    (void)visible;
}

s32 chrResolvePadId(ChrRecord *guard, s32 padid)
{
    (void)guard;
    return padid;
}

DIFFICULTY lvlGetSelectedDifficulty(void)
{
    return DIFFICULTY_AGENT;
}

static void reset_guard(ChrRecord *guard, Model *model, PropRecord *prop)
{
    memset(guard, 0, sizeof(*guard));
    memset(model, 0, sizeof(*model));
    memset(prop, 0, sizeof(*prop));
    guard->model = model;
    guard->prop = prop;
    guard->actiontype = ACT_STAND;
    model->playspeed = 1.0f;
    prop->type = PROP_TYPE_CHR;
}

int main(void)
{
    ChrRecord guard;
    Model model;
    PropRecord prop;
    PropRecord weapon_prop;
    WeaponObjRecord weapon;
    size_t index;

    memset(&attack_animation, 0, sizeof(attack_animation));
    attack_animation.recoil_start_frame = -1.0f;
    attack_animation.shoot_start_frame = 1.0f;
    attack_animation.end_frame = 2.0f;
    attack_group.table = (void *)&attack_animation;
    attack_group.len = 1;
    for (index = 0U; index < 32U; index++) {
        ptr_rifle_firing_animation_groups[index] = &attack_group;
        ptr_pistol_firing_animation_groups[index] = &attack_group;
        ptr_doubles_firing_animation_groups[index] = &attack_group;
    }

    reset_guard(&guard, &model, &prop);
    ge_original_dam_guard_tick_stand_exact(&guard);
    assert(guard.sleep == 14);

    reset_guard(&guard, &model, &prop);
    guard.act_stand.checkfacingwall = 1;
    guard.act_stand.wallcount = -1;
    ge_original_dam_guard_tick_stand_exact(&guard);
    assert(collision_calls == 8);
    assert(guard.act_stand.checkfacingwall == 0);

    reset_guard(&guard, &model, &prop);
    guard.actiontype = ACT_ANIM;
    guard.act_init.padding[1] = 1;
    guard.act_init.padding[3] = 1;
    guard.sleep = 0;
    active_animation = NULL;
    ge_original_dam_guard_tick_anim_exact(&guard);
    assert(guard.sleep == 14);
    assert(animation_sets == 0);
    assert(idle_sound_calls == 0);

    reset_guard(&guard, &model, &prop);
    guard.actiontype = ACT_KNEEL;
    guard.sleep = 33;
    ge_original_dam_guard_tick_kneel_exact(&guard);
    assert(guard.sleep == 0);

    reset_guard(&guard, &model, &prop);
    guard.actiontype = ACT_DEAD;
    guard.act_init.padding[0] = -1;
    ge_original_dam_guard_tick_dead_exact(&guard);
    assert(guard.act_init.padding[0] == 0);
    ge_original_dam_guard_tick_dead_exact(&guard);
    assert(guard.act_init.padding[0] == 1);
    assert(guard.fadealpha < 255);

    reset_guard(&guard, &model, &prop);
    guard.actiontype = ACT_STARTALARM;
    anim_frame = 60.0f;
    anim_end = 100.0f;
    alarm_timer = 0;
    ge_original_dam_guard_tick_start_alarm_exact(&guard);
    assert(alarm_timer == 1);

    reset_guard(&guard, &model, &prop);
    guard.actiontype = ACT_BONDDIE;
    ge_original_dam_guard_tick_bond_die_removed_exact(&guard);
    assert(guard.actiontype == ACT_BONDDIE);

    /* The unchanged aim entry rejects empty hands, then accepts the exact Dam
     * collectable record once it is attached through the canonical child and
     * held-prop relations. */
    reset_guard(&guard, &model, &prop);
    assert(actor_aim_at_actor(&guard, 0x0100, 0) == FALSE);
    assert(guard.actiontype == ACT_STAND);
    memset(&weapon, 0, sizeof(weapon));
    memset(&weapon_prop, 0, sizeof(weapon_prop));
    weapon.extrascale = 0x0100U;
    weapon.state = PROPSTATE_NORMAL;
    weapon.type = PROPDEF_COLLECTABLE;
    weapon.obj = PROP_CHRKALASH;
    weapon.pad = 0;
    weapon.flags = PROPFLAG_ASSIGNEDTOCHR;
    weapon.weaponnum = ITEM_AK47;
    weapon.prop = &weapon_prop;
    weapon.model = &model;
    weapon_prop.type = PROP_TYPE_WEAPON;
    weapon_prop.weapon = &weapon;
    weapon_prop.parent = &prop;
    prop.child = &weapon_prop;
    guard.weapons_held[GUNRIGHT] = &weapon_prop;
    assert(actor_aim_at_actor(&guard, 0x0100, 0) == TRUE);
    assert(guard.actiontype == ACT_ATTACK);
    assert(guard.act_attack.attacktype == 0x0100);
    assert(guard.act_attack.entityid == 0);

    puts("exact Dam action handlers: stand/anim maintenance, kneel, dead "
         "fade, alarm activation, attack eligibility, and removed "
         "Bond-death state retained");
    return 0;
}
