#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bondgame.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <bondaicommands.h>
#include "src/game/bondview.h"
#include "src/game/chraction.h"
#include "src/game/chr.h"
#include "src/game/gun.h"
#include "src/game/model.h"
#include "src/game/player.h"
#include "src/game/propobj.h"
#include "src/game/stan.h"
#include "snd.h"

static struct player player;
static struct player_data player_data;
static PropRecord player_prop;
static ChrRecord guard;
static PropRecord guard_prop;
static Model guard_model;
static WeaponObjRecord ak47;
static PropRecord ak47_prop;
static WeaponStats ak47_stats;
static struct weapon_firing_animation_table attack_anim;
static StandTile stan;
static ALSoundState sound_state;
static int line_calls;
static int damage_sound_calls;
static int weapon_sound_calls;
static s16 last_sound;
static int gunfire_visible;

s32 g_ClockTimer = 1;
s32 g_GlobalTimer = 100;
f32 g_GlobalTimerDelta = 1.0f;
f32 g_AiAccuracyModifier = 1.0f;
f32 g_AiDamageModifier = 1.0f;
struct player *g_CurrentPlayer = &player;
struct player *g_playerPointers[4] = { &player, NULL, NULL, NULL };
struct player_data g_playerPlayerData[4];
struct player_data *g_playerPerm = &player_data;
s32 g_PlayerIsInTank;
bool g_PlayerInvincible;
s32 g_stopPlayFlag;
s32 g_gameOverFlag;
struct ALBankAlt_s *g_musicSfxBufferPtr = (struct ALBankAlt_s *)(uintptr_t)1;
PropRecord *stanSavedColl_posData;

extern void chrlvTickAttack(ChrRecord *self);
extern void chrlvTriggerFireWeapon(ChrRecord *self);

WeaponStats *get_ptr_item_statistics(ITEM_IDS item)
{
    assert(item == ITEM_AK47);
    return &ak47_stats;
}

PropRecord *chrGetEquippedWeaponProp(ChrRecord *chr, GUNHAND hand)
{
    assert(chr == &guard);
    assert(hand == GUNRIGHT || hand == GUNLEFT);
    return chr->weapons_held[hand];
}

f32 modelGetAnimFrame(Model *model)
{ assert(model == &guard_model); return model->animframe1; }
f32 modelGetAnimEndFrame(Model *model)
{ assert(model == &guard_model); return model->endframe; }
void modelSetAnimSpeed(Model *model, f32 speed, f32 frame)
{ assert(model == &guard_model); (void)speed; (void)frame; }
void modelSetAnimEndFrame(Model *model, f32 frame)
{ assert(model == &guard_model); model->endframe = frame; }
void modelSetAnimation(Model *model, ModelAnimation *anim, s32 flip,
                       f32 start, f32 speed, f32 merge)
{
    assert(model == &guard_model); (void)anim; (void)flip;
    (void)speed; (void)merge; model->animframe1 = start;
}
ModelAnimation *objecthandlerGetModelAnim(Model *model)
{ assert(model == &guard_model); return model->anim; }

void chrlvResetAimend(ChrRecord *chr) { (void)chr; }
void chrlvKneelingAnimationRelated7F023E48(ChrRecord *chr) { (void)chr; abort(); }
s32 chrlvAttackrollAnimationRelated7F02E2E0(ChrRecord *chr)
{ (void)chr; abort(); }
void chrlvAttackrollAnimationRelated7F02E3B8(ChrRecord *chr)
{ (void)chr; abort(); }
void sub_GAME_7F025560(ChrRecord *chr, s32 flags, s32 target)
{ (void)chr; (void)flags; (void)target; abort(); }
void sub_GAME_7F0256F0(ChrRecord *chr, s32 flags, s32 target)
{ (void)chr; (void)flags; (void)target; abort(); }
f32 chrlvGetGuard007SpeedRating(ChrRecord *chr, f32 easy, f32 hard)
{ assert(chr == &guard); (void)hard; return easy; }
s32 chrlvSetSubroty(ChrRecord *chr, s32 turning, f32 target,
                    f32 scale, f32 acceleration)
{ (void)chr; (void)turning; (void)target; (void)scale; (void)acceleration; abort(); }
void chrlvSetTargetToPlayer(ChrRecord *chr) { (void)chr; abort(); }
s32 chrlvUpdateAimendsideback(ChrRecord *chr,
        struct weapon_firing_animation_table *anim, s32 left, s32 right,
        f32 scale)
{ (void)chr; (void)anim; (void)left; (void)right; (void)scale; abort(); }

void chrSetFiring(ChrRecord *chr, s32 hand, s32 firing)
{
    assert(chr == &guard && (hand == GUNRIGHT || hand == GUNLEFT));
    if (hand == GUNRIGHT) gunfire_visible = firing;
}
void chrSetMoving(ChrRecord *chr, s32 moving)
{ assert(chr == &guard); (void)moving; }
f32 getsubroty(Model *model) { assert(model == &guard_model); return 0.0f; }

PropRecord *getCurrentPlayerProp(void) { return &player_prop; }
s32 chrlvAttackRelated7F0292A8(ChrRecord *chr, coord3d *pos, StandTile *tile)
{ assert(chr == &guard && pos != NULL && tile == &stan); return TRUE; }
s32 stanTestLineUnobstructed(StandTile **tile, f32 x1, f32 z1,
        f32 x2, f32 z2, s32 cdtypes, f32 y1, f32 y2, f32 y3, f32 y4)
{
    (void)x1; (void)z1; (void)x2; (void)z2; (void)cdtypes;
    (void)y1; (void)y2; (void)y3; (void)y4;
    assert(tile != NULL && *tile == &stan); line_calls++; return TRUE;
}
void stanResetHits(void) { stanSavedColl_posData = NULL; }
void chrlvStanLineDirIntersection(coord3d *a, coord3d *b, coord3d *c)
{ (void)a; (void)b; (void)c; abort(); }

f32 get_007_accuracy_mod(void) { return 1.0f; }
f32 get_007_damage_mod(void) { return 1.0f; }

ALSoundState *sndPlaySfx(struct ALBankAlt_s *bank, s16 sound,
                        ALSoundState *pending)
{
    assert(bank == g_musicSfxBufferPtr);
    last_sound = sound;
    if (sound == 0x6d) weapon_sound_calls++;
    else if (sound == BOND_GET_HIT1_SFX) damage_sound_calls++;
    memset(&sound_state, 0, sizeof(sound_state));
    sound_state.playingState = SOUND_STATE_PLAYING;
    if (pending != NULL) pending->link.next = &sound_state;
    return &sound_state;
}
u8 sndGetPlayingState(ALSoundState *state)
{ return state != NULL ? SOUND_STATE_PLAYING : SOUND_STATE_NONE; }
void sndDeactivate(ALSoundState *state) { (void)state; }
void chrobjSndCreatePostEventDefault(ALSoundState *state, coord3d *pos)
{ assert(state == &sound_state && pos == &guard_prop.pos); }

s32 getPlayerCount(void) { return 1; }
s32 get_cur_playernum(void) { return 0; }
s32 cur_player_get_control_type(void) { return 0; }
s32 get_scenario(void) { return 0; }
void joyRumblePakStart(s8 playernum, f32 strength)
{ assert(playernum == 0 && strength == 0.25f); }
f32 currentPlayerGetArmor(void) { return player.bondarmour; }
f32 currentPlayerGetHealth(void) { return player.bondhealth; }
void hudMakeDamageSegments(f32 *values, s32 count, s32 direction, f32 amount)
{ (void)values; (void)count; (void)direction; (void)amount; abort(); }
void bondviewKillCurrentPlayer(void) { player.bonddead = TRUE; }
bool bondinvHasGoldenGun(void) { return FALSE; }
void drop_inventory(void) { abort(); }
void increment_num_deaths(void) { abort(); }
void increment_num_kills_display_text_in_MP(void) { abort(); }
void increment_num_suicides_display_MP(void) { abort(); }
void increment_num_times_killed_MwtGC(void) { abort(); }
void set_cur_player(s32 index) { (void)index; abort(); }

/* Branches excluded by this exact AK47 hitscan fixture. */
#define TRAP_VOID(name, args) void name args { abort(); }
TRAP_VOID(bullet_spark_create, (coord3d *p, s32 a, f32 b, s16 c))
TRAP_VOID(CapBeamLengthAndDecideIfRendered,
          (struct BeamRecord *b, ITEM_IDS i, coord3d *a, coord3d *z))
TRAP_VOID(chrobjMaybeDetonateObjectIfFlags,
          (ObjectRecord *o, f32 d, coord3d *p, ITEM_IDS i, s32 n))
TRAP_VOID(gunInitProjectileObject,
          (ObjectRecord *o, coord3d *p, StandTile *s, Mtxf *m,
           coord3d *v, Mtxf *a, PropRecord *owner))
TRAP_VOID(recall_joy2_hits_edit_flag, (ITEM_IDS i, coord3d *p, s32 q))
#undef TRAP_VOID
bool handles_shot_actors(ChrRecord *c, s32 h, coord3d *d, s32 i, bool q)
{ (void)c; (void)h; (void)d; (void)i; (void)q; abort(); }
void recall_joy2_hits_edit_detail_edit_flag(
        ITEM_IDS item, PropRecord *prop, s32 texture)
{
    assert(item == ITEM_AK47);
    assert(prop == &player_prop);
    assert(texture == -1);
}
ObjectRecord *create_new_item_instance_of_model(PROP model, s32 weapon)
{ (void)model; (void)weapon; abort(); }
Mtxf *currentPlayerGetViewToWorldMtxf(void) { abort(); }
Mtxf *modelFindNodeMtx(Model *model, ModelNode *node, s32 index)
{ (void)model; (void)node; (void)index; abort(); }
void matrix_4x4_multiply_homogeneous(Mtxf *a, Mtxf *b, Mtxf *out)
{ (void)a; (void)b; (void)out; abort(); }
void matrix_4x4_multiply_homogeneous_in_place(Mtxf *a, Mtxf *b)
{ (void)a; (void)b; abort(); }
void matrix_4x4_set_identity(Mtxf *m) { (void)m; abort(); }
void matrix_4x4_set_rotation_around_x(f32 a, Mtxf *m)
{ (void)a; (void)m; abort(); }
void matrix_4x4_set_rotation_around_y(f32 a, Mtxf *m)
{ (void)a; (void)m; abort(); }
void mtx4TransformVecInPlace(Mtxf *m, coord3d *v)
{ (void)m; (void)v; abort(); }

static void reset_fixture(void)
{
    memset(&player, 0, sizeof(player));
    memset(&player_data, 0, sizeof(player_data));
    memset(&player_prop, 0, sizeof(player_prop));
    memset(&guard, 0, sizeof(guard));
    memset(&guard_prop, 0, sizeof(guard_prop));
    memset(&guard_model, 0, sizeof(guard_model));
    memset(&ak47, 0, sizeof(ak47));
    memset(&ak47_prop, 0, sizeof(ak47_prop));
    memset(&ak47_stats, 0, sizeof(ak47_stats));
    memset(&attack_anim, 0, sizeof(attack_anim));
    memset(&stan, 0, sizeof(stan));
    memset(&sound_state, 0, sizeof(sound_state));
    line_calls = damage_sound_calls = weapon_sound_calls = 0;
    last_sound = 0;
    gunfire_visible = 0;
    g_GlobalTimer = 100;
    g_GlobalTimerDelta = 1.0f;
    g_PlayerInvincible = FALSE;
    g_PlayerIsInTank = FALSE;
    g_stopPlayFlag = g_gameOverFlag = FALSE;

    player_data.handicap = 1.0f;
    player.bondhealth = 1.0f;
    player.actual_health = 1.0f;
    player.actual_armor = 1.0f;
    player.damageshowtime = -1;
    player.watch_animation_state = WATCH_ANIMATION_0x0;
    player.prop = &player_prop;
    player_prop.type = PROP_TYPE_VIEWER;
    player_prop.pos.z = 100.0f;
    player_prop.stan = &stan;

    guard.prop = &guard_prop;
    guard.model = &guard_model;
    guard.actiontype = ACT_ATTACK;
    guard.seen_bond_time = g_GlobalTimer;
    guard.shotbondsum = 0.99f;
    guard.firecount[GUNRIGHT] = 2;
    guard.act_attack.animfloats = &attack_anim;
    guard.act_attack.attacktype = TARGET_BOND | TARGET_DONTTURN;
    guard.act_attack.unk38[GUNRIGHT] = 1;
    guard.act_attack.unk3a[GUNRIGHT] = 0;
    guard.act_attack.attack_time = g_GlobalTimer;
    guard.act_attack.unk44 = 0;
    guard_prop.type = PROP_TYPE_CHR;
    guard_prop.chr = &guard;
    guard_prop.stan = &stan;
    guard_model.chr = &guard;
    guard_model.animframe1 = 12.0f;
    guard_model.endframe = 40.0f;
    attack_anim.shoot_start_frame = 10.0f;
    attack_anim.shoot_end_frame = 20.0f;
    attack_anim.recoil_start_frame = -1.0f;
    attack_anim.recoil_end_frame = -1.0f;
    attack_anim.aim_start_frame = 100.0f;
    attack_anim.aim_end_frame = 101.0f;
    attack_anim.end_frame = 40.0f;

    ak47.weaponnum = ITEM_AK47;
    ak47.prop = &ak47_prop;
    ak47_prop.type = PROP_TYPE_WEAPON;
    ak47_prop.weapon = &ak47;
    guard.weapons_held[GUNRIGHT] = &ak47_prop;
    /* Exact US authored KF7 row in gunWeaponStats.inc.c. */
    ak47_stats.AutomaticFiringRate = 3;
    ak47_stats.SoundTriggerRate = 1;
    ak47_stats.Sound = 0x6d;
    ak47_stats.DestructionAmount = 1.0f;
}

int main(void)
{
    reset_fixture();
    chrlvTickAttack(&guard);
    assert((guard.hidden & CHRHIDDEN_FIRE_WEAPON_RIGHT) != 0);
    chrlvTriggerFireWeapon(&guard);
    assert((guard.hidden & CHRHIDDEN_FIRE_WEAPON_RIGHT) == 0);
    assert(guard.firecount[GUNRIGHT] == 3);
    assert(line_calls == 2);
    assert(gunfire_visible == TRUE);
    assert(weapon_sound_calls == 1 && damage_sound_calls == 1);
    assert(last_sound == 0x6d);
    assert(fabsf(player.bondhealth - 0.875f) < 0.0001f);
    assert(player.damageshowtime == 0.0f);
    assert(guard.shotbondsum == 0.0f);

    reset_fixture();
    guard_model.animframe1 = 5.0f;
    chrlvTickAttack(&guard);
    assert((guard.hidden & CHRHIDDEN_FIRE_WEAPON_RIGHT) == 0);
    chrlvTriggerFireWeapon(&guard);
    assert(guard.firecount[GUNRIGHT] == 2);
    assert(line_calls == 0 && weapon_sound_calls == 0);
    assert(player.bondhealth == 1.0f);

    reset_fixture();
    guard.weapons_held[GUNRIGHT] = NULL;
    chrlvTickAttack(&guard);
    assert((guard.hidden & CHRHIDDEN_FIRE_WEAPON_RIGHT) != 0);
    chrlvTriggerFireWeapon(&guard);
    assert(guard.firecount[GUNRIGHT] == 2);
    assert(line_calls == 0 && weapon_sound_calls == 0);
    assert(player.bondhealth == 1.0f);

    puts("exact guard AK47 attack/fire: fire frame, SFX, Bond damage, reject branches ok");
    return 0;
}
