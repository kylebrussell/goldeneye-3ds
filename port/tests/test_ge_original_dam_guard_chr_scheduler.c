#include "ge_original_dam_guard_chr_scheduler.h"

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "src/game/cheat.h"
#include "bondaicommands.h"
#include "src/game/chrai.h"
#include "src/game/chrobjdata.h"
#include "src/game/debugmenu_handler.h"
#include "src/game/dyn.h"
#include "src/game/explosion.h"
#include "src/game/file2.h"
#include "src/game/initanitable.h"
#include "src/game/lv.h"
#include "src/game/matrixmath.h"
#include "src/game/model.h"
#include "src/game/objecthandler.h"
#include "src/game/player.h"
#include "src/game/propobj.h"
#include "src/game/stan.h"
#include "music.h"
#include "snd.h"
#if defined(GE_PORT_DAM_GUARD_SUSTAINED_TEST)
#include "ge_original_gun_frame_arena.h"
#include "ge_original_bond_input_provider.h"
#include "ge_original_dam_guard_runtime.h"
#include "ge_original_dam_guards.h"
#include "ge_original_gun_live.h"
#include "ge_original_gameplay_services.h"
#endif

static _Noreturn void unexpected_boundary(void);

void ge_original_stage_monitor_set_image_exact(
        void *monitor, int32_t image_num)
{
    (void)monitor;
    (void)image_num;
}

bool if_actor_able_set_on_path(ChrRecord *self, s32 pathid)
{
    (void)self;
    (void)pathid;
    return TRUE;
}

s8 chrGetNumCloseArghs(ChrRecord *self)
{
    return self->numclosearghs;
}

s8 chrGetNumArghs(ChrRecord *self)
{
    return self->numarghs;
}

f32 chrGetDistanceToPad(ChrRecord *self, s32 padid)
{
    (void)self;
    (void)padid;
    unexpected_boundary();
}

bool chrTrySurprisedLookAround(ChrRecord *self)
{
    (void)self;
    unexpected_boundary();
}

bool chrTryStartAlarm(ChrRecord *self, s32 padid)
{
    (void)self;
    (void)padid;
    unexpected_boundary();
}

s32 alarmIsActive(void)
{
    return FALSE;
}

struct weapon_firing_animation_table D_80030660[11];
f32 D_80030988 = 1.0f;
f32 D_80030994 = 1.0f;
f32 g_AiReactionSpeed = 1.0f;

void chrGetChrWidthHeight(PropRecord *prop, f32 *width, f32 *height,
        f32 *always_20)
{
    (void)prop;
    *width = 30.0f;
    *height = 185.0f;
    *always_20 = 20.0f;
}

PathRecord *pathFindById(s32 id)
{
    (void)id;
    return NULL;
}

s32 plot_course_for_actor(ChrRecord *self, coord3d *position,
        StandTile *stan, SPEED speed)
{
    (void)self;
    (void)position;
    (void)stan;
    (void)speed;
    return FALSE;
}

bool chrSawTargetRecently(ChrRecord *self)
{
    return self->chrflags & CHRFLAG_WAS_HIT;
}

enum { GUARD_COUNT = 4 };
#if defined(GE_PORT_DAM_GUARD_SUSTAINED_TEST)
enum { SUSTAINED_FRAMES = 8, GUN_PREFIX_BYTES = 64,
       MAX_EVENTS = GUARD_COUNT * 5 * SUSTAINED_FRAMES };
#else
enum { MAX_EVENTS = GUARD_COUNT * 5 };
#endif
enum { EVENT_ACTION = 100, EVENT_ANIM = 200, EVENT_AIM = 300,
       EVENT_MATRIX = 350,
       EVENT_FIRE = 400, EVENT_COMMIT = 500 };

static PropRecord props[GUARD_COUNT];
static ChrRecord chrs[GUARD_COUNT];
static Model models[GUARD_COUNT];
static int anim_ticks[GUARD_COUNT];
static int events[MAX_EVENTS];
static int event_count;
static PropRecord *active_tail;
#if defined(GE_PORT_DAM_GUARD_SUSTAINED_TEST)
static StandTile test_stan;
static unsigned same_room_line_tests;
static unsigned moving_toggles;
static unsigned root_rotation_reads;
static unsigned durable_refreshes;
static unsigned distance_scale_restores;
static unsigned empty_hand_render_checks;
static unsigned attack_ticks;
static WeaponObjRecord authored_weapon;
static PropRecord authored_weapon_prop;
static Mtxf durable_matrices[GUARD_COUNT];
static unsigned char *current_frame_begin;
static unsigned char *current_frame_end;
static u8 fixture_ailist_bytes[GUARD_COUNT][SUSTAINED_FRAMES + 1];
static AIListRecord fixture_ailist_table[GUARD_COUNT + 1];
static GeOriginalGunLiveStats fixture_gun_stats;
static GeOriginalGameplayServiceStats fixture_service_stats;
#if defined(GE_PORT_DAM_GUARD_AI_ATTACK_DISPATCH_TEST)
struct weapon_firing_animation_table D_80030078[21];
static struct weapon_firing_animation_table attack_animation_fixture;
#endif
#endif

s32 D_8002C904;
s32 g_AnimationTablePointerCountRelated;
s32 D_8002C90C;
s32 D_8002C910;
ModelRenderData D_8002CC6C;
coord3d D_8002CCAC;
s32 animation_table_ptrs1[1];
s32 g_ClockTimer;
s32 g_GlobalTimer;
f32 g_GlobalTimerDelta;
struct player *g_playerPointers[4];
ChrRecord *g_CurModelChr;
void (*g_ModelJointPositionedFunc)(s32 mtxindex, Mtxf *mtx);
struct headHat headHat_array_8003E464[168];
ChrRecord *g_ChrSlots;
s32 g_NumChrSlots;
ChrRecord *g_ActiveChrs;
s32 g_ActiveChrsCount;
s32 g_SeenBondRecentlyGuardCount;

#if !defined(GE_PORT_DAM_GUARD_AI_ACTION_GRAPH_TEST)
s32 get_numguards(void)
{
    return GUARD_COUNT;
}
#endif

#if defined(GE_PORT_DAM_GUARD_SUSTAINED_TEST)
static struct player test_player;
struct player *g_CurrentPlayer = &test_player;
stagesetup g_CurrentSetup;
static struct animation_table_data animation_data;
struct animation_table_data *ptr_animation_table = &animation_data;
ModelAnimation *animation_table_ptrs2[1];
struct anim_group_info *ptr_rifle_firing_animation_groups[1];
struct anim_group_info *ptr_pistol_firing_animation_groups[1];
struct anim_group_info *ptr_doubles_firing_animation_groups[1];
#if defined(GE_PORT_DAM_GUARD_AI_ATTACK_DISPATCH_TEST)
struct anim_group_info *ptr_crouched_rifle_firing_animation_groups[1];
struct anim_group_info *ptr_crouched_pistol_firing_animation_groups[1];
struct anim_group_info *ptr_crouched_doubles_firing_animation_groups[1];
#endif
#endif

static int guard_index_from_chr(const ChrRecord *chr)
{
    assert(chr >= chrs && chr < chrs + GUARD_COUNT);
    return (int)(chr - chrs);
}

static int guard_index_from_model(const Model *model)
{
    assert(model >= models && model < models + GUARD_COUNT);
    return (int)(model - models);
}

static void record_event(int kind, int index)
{
    assert(event_count < MAX_EVENTS);
    events[event_count++] = kind + index;
}

static _Noreturn void unexpected_boundary(void)
{
    abort();
}

#if defined(GE_PORT_DAM_GUARD_AI_ATTACK_DISPATCH_TEST)
f32 bondviewGetPlayerDuckingHeightRelated(struct player *player)
{ (void)player; unexpected_boundary(); }
coord3d *chrlvGetChrOrPresetLocation(
    ChrRecord *chr, s32 flags, s32 id, StandTile **stan)
{ (void)chr; (void)flags; (void)id; (void)stan; unexpected_boundary(); }
void chrlvLineLineIntersection(coord3d *a, coord3d *b, coord3d *c,
                              coord3d *d, coord3d *result)
{ (void)a; (void)b; (void)c; (void)d; (void)result; unexpected_boundary(); }
s32 chrlvSetSubroty(ChrRecord *chr, s32 state, f32 end, f32 speed,
                    f32 offset)
{ (void)chr; (void)state; (void)end; (void)speed; (void)offset;
  unexpected_boundary(); }
void chrlvSetTargetToPlayer(ChrRecord *chr)
{ (void)chr; unexpected_boundary(); }
PropRecord *getCurrentPlayerProp(void) { unexpected_boundary(); }
void matrix_4x4_multiply_homogeneous_in_place(Mtxf *lhs, Mtxf *rhs)
{ (void)lhs; (void)rhs; unexpected_boundary(); }
void mtx4TransformVecInPlace(Mtxf *matrix, coord3d *vector)
{ (void)matrix; (void)vector; unexpected_boundary(); }
void sub_GAME_7F058E78(Mtxf *lhs, Mtxf *rhs)
{ (void)lhs; (void)rhs; unexpected_boundary(); }
#endif

#if !defined(GE_PORT_DAM_GUARD_SUSTAINED_TEST)
void ge_original_dam_guard_action_tick_exact(ChrRecord *chr)
{
    record_event(EVENT_ACTION, guard_index_from_chr(chr));
}
#else
/* The sustained fixture drives the unchanged full dispatcher through exact
 * ACT_ATTACK, ACT_DIE and ACT_DEAD. Other dispatcher arms are retained and
 * linked to branch traps here; their exact bodies have separate focused
 * sanitizer coverage, while entering one in this fixture is a test failure. */
#define SUSTAINED_TICK_TRAP(name) \
    void name(ChrRecord *chr) { (void)chr; unexpected_boundary(); }
SUSTAINED_TICK_TRAP(chrlvTickStand)
SUSTAINED_TICK_TRAP(chrlvTickKneel)
SUSTAINED_TICK_TRAP(chrlvTickAnim)
SUSTAINED_TICK_TRAP(chrlvTickArgh)
SUSTAINED_TICK_TRAP(chrlvTickPreArgh)
SUSTAINED_TICK_TRAP(chrlvTickSidestep)
SUSTAINED_TICK_TRAP(chrlvTickJumpout)
#if !defined(GE_PORT_DAM_GUARD_AI_ATTACK_DISPATCH_TEST)
void chrlvTickAttack(ChrRecord *chr)
{
    WeaponObjRecord *weapon;
    assert(chr == &chrs[GUARD_COUNT - 1]);
    assert(chr->actiontype == ACT_ATTACK);
    assert(chr->weapons_held[GUNRIGHT] == &authored_weapon_prop);
    weapon = authored_weapon_prop.weapon;
    assert(weapon == &authored_weapon);
    assert(weapon->extrascale == 0x0100U);
    assert(weapon->type == PROPDEF_COLLECTABLE);
    assert(weapon->obj == PROP_CHRKALASH);
    assert(weapon->flags == PROPFLAG_ASSIGNEDTOCHR);
    assert(weapon->weaponnum == ITEM_AK47);
    chr->firecount[GUNRIGHT]++;
    fixture_service_stats.sound_play_calls++;
    test_player.bondhealth -= 0.01f;
    attack_ticks++;
}
#endif
SUSTAINED_TICK_TRAP(chrlvTickAttackWalk)
SUSTAINED_TICK_TRAP(chrlvTickAttackRoll)
SUSTAINED_TICK_TRAP(chrlvTickRunPos)
SUSTAINED_TICK_TRAP(chrlvTickPatrol)
SUSTAINED_TICK_TRAP(chrlvTickGoPos)
SUSTAINED_TICK_TRAP(chrlvTickSurrender)
SUSTAINED_TICK_TRAP(chrlvTickTest)
SUSTAINED_TICK_TRAP(chrlvTickSurprised)
SUSTAINED_TICK_TRAP(chrlvTickStartAlarm)
SUSTAINED_TICK_TRAP(chrlvTickThrowGrenade)
SUSTAINED_TICK_TRAP(chrlvTickBondIntro)
SUSTAINED_TICK_TRAP(chrlvTickBondDieRemoved)
#undef SUSTAINED_TICK_TRAP
#endif

void modelTickAnim(Model *model, s32 ticks, s32 update_chrstuff)
{
    int index = guard_index_from_model(model);
    assert(ticks == 1);
    assert(update_chrstuff == 1);
#if defined(GE_PORT_DAM_GUARD_SUSTAINED_TEST)
    model->animframe1 += (f32)ticks;
#endif
    anim_ticks[index]++;
    record_event(EVENT_ANIM, index);
}

void getsuboffset(Model *model, coord3d *offset)
{
    *offset = props[guard_index_from_model(model)].pos;
}

void subcalcpos(Model *model)
{
    (void)guard_index_from_model(model);
}

void set_color_shading_from_tile(PropRecord *prop, u8 col[4])
{
    (void)prop;
    memset(col, 0x80, 4);
}

void chrpropDeregisterRooms(PropRecord *prop) { (void)prop; }
void chrpropUpdateRoomList(PropRecord *prop, coord3d *bbmin,
                           coord3d *bbmax, f32 radius)
{
    (void)prop; (void)bbmin; (void)bbmax;
    assert(radius == 50.0f);
}
void chrpropRegisterRooms(PropRecord *prop) { (void)prop; }

f32 getinstsize(Model *model)
{
    (void)guard_index_from_model(model);
    return 20.0f;
}

bool posIsOnScreen(PropRecord *prop, coord3d *pos, f32 radius, bool arg3)
{
    (void)prop; (void)pos; (void)radius;
    assert(arg3 == TRUE);
#if defined(GE_PORT_DAM_GUARD_SUSTAINED_TEST)
    return TRUE;
#else
    return FALSE;
#endif
}

void chrUpdateAimProperties(ChrRecord *chr)
{
    record_event(EVENT_AIM, guard_index_from_chr(chr));
}

void chrlvTriggerFireWeapon(ChrRecord *chr)
{
#if defined(GE_PORT_DAM_GUARD_AI_ATTACK_DISPATCH_TEST)
    if (chr == &chrs[GUARD_COUNT - 1]) {
        assert((chr->hidden & CHRHIDDEN_FIRE_WEAPON_RIGHT) != 0U
               && (chr->hidden & CHRHIDDEN_FIRE_WEAPON_LEFT) == 0U);
        chr->hidden &= ~CHRHIDDEN_FIRE_WEAPON_RIGHT;
        chr->firecount[GUNRIGHT]++;
        fixture_service_stats.sound_play_calls++;
        test_player.bondhealth -= 0.01f;
        attack_ticks++;
    }
#endif
    record_event(EVENT_FIRE, guard_index_from_chr(chr));
}

PropRecord *chrpropGetActiveTail(void) { return active_tail; }

void propExecuteTickOperation(PropRecord *prop, TICKOP op)
{
    assert(op == TICKOP_NONE);
    record_event(EVENT_COMMIT, guard_index_from_chr(prop->chr));
}

s32 get_cur_playernum(void) { return 0; }
s32 get_player_position_in_shuffled(s32 playernum)
{
    assert(playernum == 0);
    return 1;
}

/* These are real canonical branches in the retained bodies, but the four-CHR
 * fixture must never cross them. Trapping them makes the focused test prove
 * its audited scheduler path instead of silently accepting a host no-op. */
#if defined(GE_PORT_DAM_GUARD_SUSTAINED_TEST)
static Mtxf sustained_world_to_screen;
Mtxf *camGetWorldToScreenMtxf(void) { return &sustained_world_to_screen; }
bool cheatIsActive(CHEAT_ID id) { (void)id; return FALSE; }
#else
Mtxf *camGetWorldToScreenMtxf(void) { unexpected_boundary(); }
bool cheatIsActive(CHEAT_ID id) { (void)id; unexpected_boundary(); }
#endif
bool chrCanUseDKModeScaling(s32 body, s32 head)
{ (void)body; (void)head; unexpected_boundary(); }
#if defined(GE_PORT_DAM_GUARD_SUSTAINED_TEST)
void chrHandleJointPositioned(enum CHR_RENDER_PART part, Mtxf *matrix)
{ (void)part; (void)matrix; }
void chrRenderHeldWeapon(void *context, GUNHAND hand, Gfx **gdl)
{
    PropRecord *prop = context;
    assert(prop != NULL && prop->chr != NULL);
    assert(hand == GUNRIGHT || hand == GUNLEFT);
    if (prop->chr == &chrs[GUARD_COUNT - 1] && hand == GUNRIGHT) {
        assert(prop->chr->weapons_held[hand] == &authored_weapon_prop);
    } else {
        assert(prop->chr->weapons_held[hand] == NULL);
    }
    assert(gdl != NULL);
    empty_hand_render_checks++;
}
#else
void chrHandleJointPositioned(enum CHR_RENDER_PART part, Mtxf *matrix)
{ (void)part; (void)matrix; unexpected_boundary(); }
void chrRenderHeldWeapon(void *context, GUNHAND hand, Gfx **gdl)
{ (void)context; (void)hand; (void)gdl; unexpected_boundary(); }
#endif
void chrpropActivateThisFrame(PropRecord *prop)
{ (void)prop; unexpected_boundary(); }
void chrpropCleanupForRemoval(PropRecord *prop)
{ (void)prop; unexpected_boundary(); }
void chrpropDelist(PropRecord *prop) { (void)prop; unexpected_boundary(); }
#if !defined(GE_PORT_DAM_GUARD_SUSTAINED_TEST)
void *dynAllocate(s32 size) { (void)size; unexpected_boundary(); }
#endif
u8 explosionChrpropExplosionTick(PropRecord *prop)
{ (void)prop; unexpected_boundary(); }
u8 explosionChrpropSmokeTick(PropRecord *prop)
{ (void)prop; unexpected_boundary(); }
s32 getPlayerPointerIndex(PropRecord *prop)
{ (void)prop; unexpected_boundary(); }
s32 get_debug_chrnum_flag(void) {
#if defined(GE_PORT_DAM_GUARD_SUSTAINED_TEST)
    return FALSE;
#else
    unexpected_boundary();
#endif
}
s32 get_debug_render_raster(void) { unexpected_boundary(); }
HATTYPE get_hat_model(PropRecord *prop)
{ (void)prop; unexpected_boundary(); }
void instcalcmatrices(ModelRenderData *data, Model *model)
{ (void)data; (void)model; unexpected_boundary(); }
u16 joyGetButtons(s8 player, u16 mask)
{ (void)player; (void)mask; unexpected_boundary(); }
void matrix_4x4_copy(Mtxf *src, Mtxf *dst)
{ (void)src; (void)dst; unexpected_boundary(); }
void matrix_4x4_multiply_homogeneous(Mtxf *lhs, Mtxf *rhs, Mtxf *result)
{ (void)lhs; (void)rhs; (void)result; unexpected_boundary(); }
void matrix_4x4_set_identity_and_position(coord3d *pos, Mtxf *matrix)
{ (void)pos; (void)matrix; unexpected_boundary(); }
void matrix_column_1_scalar_multiply(f32 scale, f32 *matrix)
{ (void)scale; (void)matrix; unexpected_boundary(); }
void matrix_column_2_scalar_multiply(f32 scale, f32 *matrix)
{ (void)scale; (void)matrix; unexpected_boundary(); }
void matrix_column_3_scalar_multiply_2(f32 scale, f32 *matrix)
{ (void)scale; (void)matrix; unexpected_boundary(); }
Mtxf *modelFindNodeMtx(Model *model, ModelNode *node, s32 arg2)
{ (void)model; (void)node; (void)arg2; unexpected_boundary(); }
union ModelRwData *modelGetNodeRwData(Model *model, ModelNode *node)
{ (void)model; (void)node; unexpected_boundary(); }
void modelSetAnimation(Model *model, ModelAnimation *anim, s32 flip,
                       f32 start, f32 speed, f32 merge)
{
    (void)model; (void)anim; (void)flip; (void)start; (void)speed;
    (void)merge; unexpected_boundary();
}
#if defined(GE_PORT_DAM_GUARD_SUSTAINED_TEST)
void modelSetDistanceScale(f32 scale)
{ assert(scale == 1.0f); distance_scale_restores++; }
#else
void modelSetDistanceScale(f32 scale) { (void)scale; unexpected_boundary(); }
#endif
void modelSetScale(Model *model, f32 scale)
{ (void)model; (void)scale; unexpected_boundary(); }
s32 objDrop(PropRecord *prop) { (void)prop; unexpected_boundary(); }
void objFreePermanently(ObjectRecord *obj, bool freeprop)
{ (void)obj; (void)freeprop; unexpected_boundary(); }
s32 objTick(PropRecord *prop) { (void)prop; unexpected_boundary(); }
#if defined(GE_PORT_DAM_GUARD_SUSTAINED_TEST)
ModelAnimation *objecthandlerGetModelAnim(Model *model) { return model->anim; }
f32 modelGetAnimFrame(Model *model) { return model->animframe1; }
f32 modelGetAnimEndFrame(Model *model) { return model->endframe; }
f32 modelGetAnimSpeed(Model *model) { return model->playspeed; }
void modelSetAnimSpeed(Model *model, f32 speed, f32 frame)
{
#if defined(GE_PORT_DAM_GUARD_AI_ATTACK_DISPATCH_TEST)
    assert(model == &models[GUARD_COUNT - 1]);
    assert(fabsf(speed - 0.5f) < 0.0001f);
    assert(frame == 0.0f);
    model->playspeed = speed;
#else
    (void)model; (void)speed; (void)frame; unexpected_boundary();
#endif
}
void modelSetAnimEndFrame(Model *model, f32 frame)
{ (void)model; (void)frame; unexpected_boundary(); }
void modelSetAnimLooping(Model *model, f32 frame, f32 merge)
{ (void)model; (void)frame; (void)merge; unexpected_boundary(); }
f32 getsubroty(Model *model)
{ (void)model; root_rotation_reads++; return 0.0f; }
s8 objecthandlerGetModelGunhand(Model *model)
{ (void)model; unexpected_boundary(); }
#else
ModelAnimation *objecthandlerGetModelAnim(Model *model)
{ (void)model; unexpected_boundary(); }
#endif
s32 playerTick(PropRecord *prop) { (void)prop; unexpected_boundary(); }
void propsDefragRoomProps(void) { unexpected_boundary(); }
#if defined(GE_PORT_DAM_GUARD_SUSTAINED_TEST)
ModelHitEntry *sub_GAME_7F06B120(ModelHitEntry *head, Model *model)
{ (void)model; return head; }
void sub_GAME_7F06B248(ModelHitEntry *entry) { (void)entry; }
void sub_GAME_7F06B29C(ModelHitEntry *entry) { (void)entry; }
ModelHitEntry *sub_GAME_7F06BB28(ModelHitEntry *entry) { return entry; }
f32 sub_GAME_7F06C768(Model *model) { (void)model; return 0.0f; }
void subcalcmatrices(ModelRenderData *data, Model *model)
{
    int index = guard_index_from_model(model);
    assert(model->obj != NULL && model->obj->numMatrices == 1);
    model->render_pos = (RenderPosView *)data->mtxlist;
    record_event(EVENT_MATRIX, index);
}
void update_color_shading(rgba_u8 *dest, rgba_u8 *src) { *dest = *src; }
#else
ModelHitEntry *sub_GAME_7F06B120(ModelHitEntry *head, Model *model)
{ (void)head; (void)model; unexpected_boundary(); }
void sub_GAME_7F06B248(ModelHitEntry *entry)
{ (void)entry; unexpected_boundary(); }
void sub_GAME_7F06B29C(ModelHitEntry *entry)
{ (void)entry; unexpected_boundary(); }
ModelHitEntry *sub_GAME_7F06BB28(ModelHitEntry *entry)
{ (void)entry; unexpected_boundary(); }
f32 sub_GAME_7F06C768(Model *model) { (void)model; unexpected_boundary(); }
void subcalcmatrices(ModelRenderData *data, Model *model)
{ (void)data; (void)model; unexpected_boundary(); }
void update_color_shading(rgba_u8 *dest, rgba_u8 *src)
{ (void)dest; (void)src; unexpected_boundary(); }
#endif
void handle_alarm_gas_timer_calldamage(void) { unexpected_boundary(); }
void loop_set_sound_effect_all_slots(void) { unexpected_boundary(); }

#if defined(GE_PORT_DAM_GUARD_SUSTAINED_TEST)
ALBank *g_musicSfxBufferPtr;

u32 randomGetNext(void) { unexpected_boundary(); }
f32 chrGetDistanceToBond(ChrRecord *chr)
{ (void)chr; unexpected_boundary(); }
PropRecord *something_with_generating_object(ChrRecord *chr, s32 propid,
        ITEM_IDS itemid, s32 flags, WeaponObjRecord *weapon,
        ItemModelFileRecord *header)
{
    (void)chr; (void)propid; (void)itemid; (void)flags;
    (void)weapon; (void)header;
    unexpected_boundary();
}
u32 bondwalkItemCheckBitflags(ITEM_IDS item, u32 flags)
{ (void)item; (void)flags; unexpected_boundary(); }
s8 bondwalkItemGetAutomaticFiringRate(ITEM_IDS item)
{ (void)item; unexpected_boundary(); }
f32 chrlvDistanceToChrRelated(ChrRecord *chr, s32 type, s32 id)
{ (void)chr; (void)type; (void)id; unexpected_boundary(); }
DIFFICULTY lvlGetSelectedDifficulty(void)
{
#if defined(GE_PORT_DAM_GUARD_AI_ATTACK_DISPATCH_TEST)
    return DIFFICULTY_AGENT;
#else
    unexpected_boundary();
#endif
}
PropRecord *chrGetEquippedWeaponProp(ChrRecord *chr, GUNHAND hand)
{ return chr->weapons_held[hand]; }
void weaponSetGunfireVisible(PropRecord *prop, s32 visible)
{ (void)prop; (void)visible; unexpected_boundary(); }
void chrSetMoving(ChrRecord *chr, s32 moving)
{
    assert(chr >= chrs && chr < chrs + GUARD_COUNT);
    assert(moving == 0 || moving == 1);
    moving_toggles++;
}
s32 stanTestLineUnobstructed(StandTile **stan, f32 x, f32 z,
        f32 destx, f32 destz, s32 cdtypes, f32 y1, f32 y2, f32 y3, f32 y4)
{
    assert(stan != NULL && *stan == &test_stan);
    (void)x; (void)z; (void)destx; (void)destz;
    (void)cdtypes; (void)y1; (void)y2; (void)y3; (void)y4;
    same_room_line_tests++;
    return TRUE;
}
ALSoundState *sndPlaySfx(struct ALBankAlt_s *bank, s16 sound,
        ALSoundState *pending)
{ (void)bank; (void)sound; (void)pending; unexpected_boundary(); }
void chrobjSndCreatePostEventDefault(ALSoundState *state, coord3d *pos)
{ (void)state; (void)pos; unexpected_boundary(); }

AIRecord *ailistFindById(s32 id) { (void)id; unexpected_boundary(); }
s32 chraiGoToLabel(AIRecord *list, s32 offset, u8 label)
{ (void)list; (void)offset; (void)label; unexpected_boundary(); }
bool chrGoToPad(ChrRecord *chr, s32 pad, SPEED speed)
{ (void)chr; (void)pad; (void)speed; unexpected_boundary(); }
s32 chrCheckTargetInSight(ChrRecord *chr)
{ (void)chr; unexpected_boundary(); }
bool chrCanSeeBond(ChrRecord *chr)
{ (void)chr; unexpected_boundary(); }
ObjectRecord *objFindByTagId(s32 tag) { (void)tag; unexpected_boundary(); }
void doorActivate(DoorRecord *door, DOORSTATE state)
{ (void)door; (void)state; unexpected_boundary(); }

void ge_original_gun_live_snapshot(GeOriginalGunLiveStats *stats)
{ assert(stats != NULL); *stats = fixture_gun_stats; }

void ge_original_gameplay_services_snapshot(
        GeOriginalGameplayServiceStats *stats)
{ assert(stats != NULL); *stats = fixture_service_stats; }

size_t ge_original_dam_guards_count(void) { return GUARD_COUNT; }
void *ge_original_dam_guard_prop(size_t index)
{ return index < GUARD_COUNT ? &props[index] : NULL; }
void *ge_original_dam_guard_chr(size_t index)
{ return index < GUARD_COUNT ? &chrs[index] : NULL; }
GeOriginalDamGuardStatus ge_original_dam_guards_update_matrices(
        const float world_to_view[4][4])
{
    size_t index;
    assert(world_to_view != NULL);
    for (index = 0U; index < GUARD_COUNT; index++) {
        unsigned char *transient =
            (unsigned char *)(void *)models[index].render_pos;
        assert(transient >= current_frame_begin + GUN_PREFIX_BYTES);
        assert(transient + sizeof(Mtxf) <= current_frame_end);
        models[index].render_pos = (RenderPosView *)&durable_matrices[index];
    }
    durable_refreshes++;
    return GE_ORIGINAL_DAM_GUARD_OK;
}
GeOriginalDamGuardStatus ge_original_dam_guards_update_visible_matrices(
        const float world_to_view[4][4])
{
    /* The sustained scheduler fixture keeps every guard canonical-onscreen;
     * the live-only entry point must therefore retain the same publication. */
    return ge_original_dam_guards_update_matrices(world_to_view);
}
#endif

int main(void)
{
    int expected = 0;
    int index;
#if defined(GE_PORT_DAM_GUARD_SUSTAINED_TEST)
    int frame;
    ModelFileHeader headers[GUARD_COUNT];
    ModelNode *switches[GUARD_COUNT][5];
    GeOriginalBondInputProvider *provider;
    GeOriginalDamGuardRuntimeStats runtime_stats;
    float world_to_view[4][4];
    static unsigned char frame_storage[2][4096] __attribute__((aligned(16)));
#endif
    memset(props, 0, sizeof(props));
    memset(chrs, 0, sizeof(chrs));
    memset(models, 0, sizeof(models));
    memset(anim_ticks, 0, sizeof(anim_ticks));
    event_count = 0;
    g_ClockTimer = 1;
    g_GlobalTimer = 60;
    g_GlobalTimerDelta = 1.0f;
#if defined(GE_PORT_DAM_GUARD_SUSTAINED_TEST)
    memset(&test_stan, 0, sizeof(test_stan));
    memset(headers, 0, sizeof(headers));
    memset(switches, 0, sizeof(switches));
    memset(fixture_ailist_bytes, AI_Yield, sizeof(fixture_ailist_bytes));
    memset(fixture_ailist_table, 0, sizeof(fixture_ailist_table));
    memset(durable_matrices, 0, sizeof(durable_matrices));
    memset(&fixture_gun_stats, 0, sizeof(fixture_gun_stats));
    memset(&fixture_service_stats, 0, sizeof(fixture_service_stats));
    memset(&authored_weapon, 0, sizeof(authored_weapon));
    memset(&authored_weapon_prop, 0, sizeof(authored_weapon_prop));
    memset(world_to_view, 0, sizeof(world_to_view));
    world_to_view[0][0] = world_to_view[1][1] = 1.0f;
    world_to_view[2][2] = world_to_view[3][3] = 1.0f;
    memset(&test_player, 0, sizeof(test_player));
    test_player.bondhealth = 1.0f;
    ge_original_bond_input_bind_player(&test_player, NULL);
    ge_original_bond_input_provider_reset_normal_dam();
    assert(g_CurrentPlayer == &test_player);
    assert(g_playerPointers[0] == &test_player);
    provider = ge_original_bond_input_provider();
    assert(provider != NULL && provider->current_player == &test_player);
    g_ChrSlots = chrs;
    g_NumChrSlots = GUARD_COUNT;
    g_ActiveChrs = NULL;
    g_ActiveChrsCount = 0;
    for (index = 0; index < GUARD_COUNT; index++) {
        fixture_ailist_table[index].ailist =
            (AIRecord *)fixture_ailist_bytes[index];
        fixture_ailist_table[index].ID = (s16)(0x400 + index);
    }
    g_CurrentSetup.ailists = fixture_ailist_table;
    ge_original_dam_guard_runtime_reset();
#endif

    for (index = 0; index < GUARD_COUNT; index++) {
        props[index].type = PROP_TYPE_CHR;
        props[index].chr = &chrs[index];
#if defined(GE_PORT_DAM_GUARD_SUSTAINED_TEST)
        props[index].stan = &test_stan;
#endif
        props[index].prev = index == 0 ? NULL : &props[index - 1];
        props[index].next = index + 1 == GUARD_COUNT
            ? NULL : &props[index + 1];
        chrs[index].prop = &props[index];
        chrs[index].model = &models[index];
        chrs[index].actiontype = ACT_GOPOS;
        chrs[index].ground = 10.0f;
        chrs[index].chrnum = (s16)index;
#if defined(GE_PORT_DAM_GUARD_SUSTAINED_TEST)
        chrs[index].ailist = (AIRecord *)fixture_ailist_bytes[index];
#endif
        models[index].chr = &chrs[index];
#if defined(GE_PORT_DAM_GUARD_SUSTAINED_TEST)
        headers[index].numMatrices = 1;
        headers[index].Switches = switches[index];
        models[index].obj = &headers[index];
        models[index].animframe1 = 10.0f;
        models[index].endframe = 10.0f;
        chrs[index].actiontype = ACT_DEAD;
        chrs[index].act_init.padding[0] = -1;
#endif
    }
    active_tail = &props[GUARD_COUNT - 1];

#if defined(GE_PORT_DAM_GUARD_SUSTAINED_TEST)
    authored_weapon.extrascale = 0x0100U;
    authored_weapon.state = PROPSTATE_NORMAL;
    authored_weapon.type = PROPDEF_COLLECTABLE;
    authored_weapon.obj = PROP_CHRKALASH;
    authored_weapon.pad = GUARD_COUNT - 1;
    authored_weapon.flags = PROPFLAG_ASSIGNEDTOCHR;
    authored_weapon.weaponnum = ITEM_AK47;
    authored_weapon.prop = &authored_weapon_prop;
    authored_weapon_prop.type = PROP_TYPE_WEAPON;
    authored_weapon_prop.weapon = &authored_weapon;
    authored_weapon_prop.parent = &props[GUARD_COUNT - 1];
    props[GUARD_COUNT - 1].child = &authored_weapon_prop;
    chrs[GUARD_COUNT - 1].weapons_held[GUNRIGHT] = &authored_weapon_prop;
    chrs[GUARD_COUNT - 1].actiontype = ACT_ATTACK;
#if defined(GE_PORT_DAM_GUARD_AI_ATTACK_DISPATCH_TEST)
    /* This bounded action-state fixture keeps the unchanged attack body in
     * its firing interval for all eight frames. It deliberately excludes
     * animation restart and aim interpolation, whose exact bodies have their
     * own focused tests, so reaching either branch remains a hard failure. */
    memset(&attack_animation_fixture, 0,
           sizeof(attack_animation_fixture));
    attack_animation_fixture.end_frame = 1000.0f;
    attack_animation_fixture.shoot_start_frame = 0.0f;
    attack_animation_fixture.shoot_end_frame = 1000.0f;
    attack_animation_fixture.recoil_start_frame = -1.0f;
    attack_animation_fixture.recoil_end_frame = -1.0f;
    attack_animation_fixture.aim_start_frame = 1000.0f;
    attack_animation_fixture.aim_end_frame = 1001.0f;
    chrs[GUARD_COUNT - 1].act_attack.animfloats =
        &attack_animation_fixture;
    chrs[GUARD_COUNT - 1].act_attack.attacktype = TARGET_DONTTURN;
    chrs[GUARD_COUNT - 1].act_attack.unk38[GUNRIGHT] = 1;
    chrs[GUARD_COUNT - 1].act_attack.attack_item = ITEM_AK47;
    models[GUARD_COUNT - 1].animframe1 = 0.0f;
    models[GUARD_COUNT - 1].endframe = 1000.0f;
#endif
    {
        bool seen[GUARD_COUNT] = { false, false, false, false };
        PropRecord *active = active_tail;
        int count = 0;
        while (active != NULL) {
            int active_index;
            assert(active->type == PROP_TYPE_CHR && active->chr != NULL);
            active_index = guard_index_from_chr(active->chr);
            assert(!seen[active_index]);
            seen[active_index] = true;
            count++;
            active = active->prev;
        }
        assert(count == GUARD_COUNT);
        for (index = 0; index < GUARD_COUNT; index++) {
            assert(seen[index]);
            assert(&g_ChrSlots[index] == &chrs[index]);
        }
    }
    chrs[0].actiontype = ACT_DIE;
    chrs[0].act_die.notifychrindex = GUARD_COUNT;
    chrs[0].act_die.thudframe1 = -1.0f;
    chrs[0].act_die.thudframe2 = -1.0f;
    for (frame = 0; frame < SUSTAINED_FRAMES; frame++) {
        unsigned char *frame_begin = frame_storage[frame & 1];
        unsigned char *frame_end = frame_begin + sizeof(frame_storage[0]);
        unsigned char *gun_prefix;
        size_t used_before;
        assert(ge_original_gun_frame_arena_begin(
            frame_begin, sizeof(frame_storage[0])));
        current_frame_begin = frame_begin;
        current_frame_end = frame_end;
        gun_prefix = dynAllocate(GUN_PREFIX_BYTES);
        assert(gun_prefix == frame_begin);
        memset(gun_prefix, 0xa5, GUN_PREFIX_BYTES);
        used_before = ge_original_gun_frame_arena_used();
        assert(used_before == GUN_PREFIX_BYTES);
        provider->clock_timer = 1;
        provider->global_timer = 60 + frame;
        provider->global_timer_delta = 1.0f;
        g_ClockTimer = provider->clock_timer;
        g_GlobalTimer = 60 + frame;
        g_GlobalTimerDelta = provider->global_timer_delta;
        assert(g_GlobalTimer == provider->global_timer);
        assert(g_CurrentPlayer == provider->current_player);
        if (frame == 0) {
            int events_before = event_count;
            assert(ge_original_dam_guard_runtime_tick(world_to_view)
                   == GE_ORIGINAL_DAM_GUARD_RUNTIME_GUN_ORDER);
            assert(event_count == events_before);
            assert(ge_original_gun_frame_arena_used() == used_before);
        }
        {
            GeOriginalDynFrameAudit gun_audit;
            assert(ge_original_gun_frame_arena_audit(&gun_audit));
            fixture_gun_stats.ticks++;
            fixture_gun_stats.last_frame_generation = gun_audit.generation;
            fixture_gun_stats.last_frame_bytes = GUN_PREFIX_BYTES;
        }
        assert(ge_original_dam_guard_runtime_tick(world_to_view)
               == GE_ORIGINAL_DAM_GUARD_RUNTIME_OK);
        if (frame == 0) {
            size_t used = ge_original_gun_frame_arena_used();
            int events_before = event_count;
            assert(ge_original_dam_guard_runtime_tick(world_to_view)
                   == GE_ORIGINAL_DAM_GUARD_RUNTIME_ALREADY_TICKED);
            assert(ge_original_gun_frame_arena_used() == used);
            assert(event_count == events_before);
        }
        if (frame == 0) {
            assert(chrs[0].chrseedie == CHR_FREE);
            for (index = 1; index < GUARD_COUNT; index++)
                assert(chrs[index].chrseedie == chrs[0].chrnum);
        }
        assert(ge_original_gun_frame_arena_used()
               == GUN_PREFIX_BYTES + GUARD_COUNT * sizeof(Mtxf));
        for (index = 0; index < GUN_PREFIX_BYTES; index++)
            assert(gun_prefix[index] == 0xa5);
        for (index = 0; index < GUARD_COUNT; index++) {
            const unsigned char *matrices =
                (const unsigned char *)models[index].render_pos;
            assert(matrices == (const unsigned char *)&durable_matrices[index]);
        }
    }
    assert(chrs[0].actiontype == ACT_DEAD);
    assert(same_room_line_tests == GUARD_COUNT);
    assert(moving_toggles == GUARD_COUNT * 2U);
    assert(root_rotation_reads == GUARD_COUNT);
    for (index = 0; index < GUARD_COUNT; index++)
        assert(chrs[index].chrseedie == CHR_FREE);
    assert(chrs[0].act_init.padding[0] == SUSTAINED_FRAMES - 2);
    for (index = 1; index < GUARD_COUNT - 1; index++)
        assert(chrs[index].act_init.padding[0] == SUSTAINED_FRAMES - 1);
    assert(chrs[GUARD_COUNT - 1].actiontype == ACT_ATTACK);
    assert(attack_ticks == SUSTAINED_FRAMES);
    for (index = 0; index < GUARD_COUNT; index++)
        assert(anim_ticks[index] == SUSTAINED_FRAMES);
    ge_original_dam_guard_runtime_snapshot(&runtime_stats);
    assert(runtime_stats.ticks == SUSTAINED_FRAMES);
    assert(runtime_stats.matrix_refreshes == SUSTAINED_FRAMES);
    assert(runtime_stats.weapon_fire_dispatches == SUSTAINED_FRAMES);
    assert(runtime_stats.weapon_sound_starts == SUSTAINED_FRAMES);
    assert(runtime_stats.player_damage_events == SUSTAINED_FRAMES);
    assert(fabsf(runtime_stats.player_health_damage - 0.08f) < 0.0001f);
    assert(runtime_stats.player_armour_damage == 0.0f);
    assert(fabsf(runtime_stats.last_player_health - 0.92f) < 0.0001f);
    assert(runtime_stats.rejected_ticks == 2U);
    assert(runtime_stats.last_arena_bytes_before == GUN_PREFIX_BYTES);
    assert(runtime_stats.last_arena_bytes_after
           == GUN_PREFIX_BYTES + GUARD_COUNT * sizeof(Mtxf));
    assert(durable_refreshes == SUSTAINED_FRAMES);
    assert(distance_scale_restores
           == GUARD_COUNT * SUSTAINED_FRAMES);
    assert(empty_hand_render_checks
           == GUARD_COUNT * 2U * SUSTAINED_FRAMES);
    assert(event_count == MAX_EVENTS);
    for (frame = 0; frame < SUSTAINED_FRAMES; frame++) {
        for (index = GUARD_COUNT - 1; index >= 0; index--) {
            assert(events[expected++] == EVENT_ANIM + index);
            assert(events[expected++] == EVENT_AIM + index);
            assert(events[expected++] == EVENT_MATRIX + index);
            assert(events[expected++] == EVENT_FIRE + index);
            assert(events[expected++] == EVENT_COMMIT + index);
        }
    }
#else
    ge_original_dam_guard_props_tick_exact();

    assert(event_count == MAX_EVENTS);
    for (index = GUARD_COUNT - 1; index >= 0; index--) {
        assert(anim_ticks[index] == 1);
        assert(events[expected++] == EVENT_ACTION + index);
        assert(events[expected++] == EVENT_ANIM + index);
        assert(events[expected++] == EVENT_AIM + index);
        assert(events[expected++] == EVENT_FIRE + index);
        assert(events[expected++] == EVENT_COMMIT + index);
    }
#endif
    return 0;
}
