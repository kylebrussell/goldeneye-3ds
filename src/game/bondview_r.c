#ifdef GE_PORT_BOND_INTRO_SPAWN_SLICE

#include <math.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

#include "ge_original_dam_intro.h"
#include "ge_original_player_spawn_internal.h"
/* AIPARSE suppresses generated BITFLAG typedefs, while bondview.h retains
 * this prototype type.  The slice does not inspect player flags. */
typedef int PLAYERFLAG;
#include "game/bondview.h"

extern stagesetup g_CurrentSetup;
extern s32 g_bondviewBondDeathAnimations[];
extern s32 g_bondviewBondDeathAnimationsCount;
extern u32 randomGetNext(void);

static GeOriginalIntroProviders g_GeOriginalIntroProviders;
static GeOriginalIntroSpawnState *g_GeOriginalIntroSpawnState;

/* Real decompiled player storage for the bounded spawn slice. */
static struct player g_GeOriginalSpawnPlayerStorage;
#define g_CurrentPlayer (&g_GeOriginalSpawnPlayerStorage)

struct player *ge_original_spawn_player_get(void)
{
    return g_CurrentPlayer;
}

void ge_original_spawn_player_reset(float eye_height)
{
    memset(g_CurrentPlayer, 0, sizeof(*g_CurrentPlayer));
    /* Exact single-player combat state from initBONDdataforPlayer.  The
     * bounded spawn storage is zeroed above instead of coming from MEMPOOL_STAGE,
     * so every nonzero/sentinel field consumed by the retained damage path
     * must be published before the first actor tick. */
    g_CurrentPlayer->bondhealth = 1.0f;
    g_CurrentPlayer->bondarmour = 0.0f;
    g_CurrentPlayer->oldhealth = 1.0f;
    g_CurrentPlayer->oldarmour = 0.0f;
    g_CurrentPlayer->apparenthealth = 1.0f;
    g_CurrentPlayer->apparentarmour = 0.0f;
    g_CurrentPlayer->damageshowtime = -1;
    g_CurrentPlayer->healthshowtime = -1;
    g_CurrentPlayer->actual_health = 1.0f;
    g_CurrentPlayer->actual_armor = 1.0f;
    g_CurrentPlayer->healthdamagetype = 7;
    g_CurrentPlayer->damagetype = 7;
    g_CurrentPlayer->eyeheight = eye_height;
    g_CurrentPlayer->speedboost = 1.0f;
    g_CurrentPlayer->registeredroom = -1;
    /* Exact nonzero/sentinel death state from init_player_data.  Without
     * startnewbonddie the unchanged MoveBond death branch keeps ticking the
     * standing gait forever, so maybe_mp_interface can never observe the
     * authored death animation reaching its end. */
    g_CurrentPlayer->startnewbonddie = TRUE;
    g_CurrentPlayer->redbloodfinished = FALSE;
    g_CurrentPlayer->deathanimfinished = FALSE;

    /* Exact NTSC head-state subset from init_player_data in player.c. These
     * fields are required before the original bhead damping body can consume
     * its first animation-root sample. */
    g_CurrentPlayer->resetheadpos = TRUE;
    g_CurrentPlayer->resetheadrot = TRUE;
    g_CurrentPlayer->resetheadtick = TRUE;
    g_CurrentPlayer->headanim = 0;
    g_CurrentPlayer->headdamp = 0.93f;
    g_CurrentPlayer->headamplitude = 1.0f;
    g_CurrentPlayer->sideamplitude = 1.0f;
    g_CurrentPlayer->headlook.f[2] = 1.0f;
    g_CurrentPlayer->headup.f[1] = 1.0f;
    g_CurrentPlayer->headlooksum.f[2] = 14.285716f;
    g_CurrentPlayer->headupsum.f[1] = 14.285716f;
    g_CurrentPlayer->standlook[0].f[2] = 1.0f;
    g_CurrentPlayer->standlook[1].f[2] = 1.0f;
    g_CurrentPlayer->standup[0].f[1] = 1.0f;
    g_CurrentPlayer->standup[1].f[1] = 1.0f;
}

void ge_original_spawn_player_initialize_idle_roll(void)
{
    f32 mult = 1.0f / (f32)UINT_MAX;

    /* Unchanged bheadUpdateIdleRoll body, applied to this slice's canonical
     * player storage at the same stage-load boundary as the original
     * sets_a_bunch_of_BONDdata_values_to_default tail. */
    g_CurrentPlayer->standlook[g_CurrentPlayer->standcnt].f[0] =
        ((f32)randomGetNext() * mult - 0.5f) * 0.02f;
    g_CurrentPlayer->standlook[g_CurrentPlayer->standcnt].f[2] = 1.0f;
    g_CurrentPlayer->standup[g_CurrentPlayer->standcnt].f[0] =
        ((f32)randomGetNext() * mult - 0.5f) * 0.02f;
    g_CurrentPlayer->standup[g_CurrentPlayer->standcnt].f[1] = 1.0f;
    if (g_CurrentPlayer->standcnt) {
        g_CurrentPlayer->standlook[g_CurrentPlayer->standcnt].f[1] =
            (f32)randomGetNext() * mult * 0.01f;
        g_CurrentPlayer->standup[g_CurrentPlayer->standcnt].f[2] =
            (f32)randomGetNext() * mult * -0.01f;
    } else {
        g_CurrentPlayer->standlook[g_CurrentPlayer->standcnt].f[1] =
            (f32)randomGetNext() * mult * -0.01f;
        g_CurrentPlayer->standup[g_CurrentPlayer->standcnt].f[2] =
            (f32)randomGetNext() * mult * 0.01f;
    }
    g_CurrentPlayer->standcnt = 1 - g_CurrentPlayer->standcnt;
}

/* Exact post-selection player/prop assignments from
 * bondviewLoadSetupIntroSection, bounded at external prop/room services. */
int ge_original_commit_intro_player_spawn_slice(const float position[3],
                                                float floor_y,
                                                float look_angle_radians,
                                                void *stan,
                                                int16_t room)
{
    struct coord3d start_pos;
    StandTile *start_stan = (StandTile *)stan;

    if (position == NULL || start_stan == NULL || room < 0) {
        return 0;
    }
    start_pos.f[0] = position[0];
    start_pos.f[1] = position[1];
    start_pos.f[2] = position[2];

    g_CurrentPlayer->field_78 = 0.0f;
    g_CurrentPlayer->field_7C = -0.0001f;
    g_CurrentPlayer->field_80 = 0.0f;
    g_CurrentPlayer->field_70 = floor_y;
    g_CurrentPlayer->vv_theta = (look_angle_radians * 360.0f) / M_TAU_F;
    g_CurrentPlayer->stanHeight = floor_y;
    g_CurrentPlayer->field_6C = floor_y / 0.170000016689f;
    change_player_pos_to_target(&g_CurrentPlayer->field_488, &start_pos,
                                start_stan);
    g_CurrentPlayer->field_488.theta_transform.f[0] =
        -sinf(look_angle_radians);
    g_CurrentPlayer->field_488.theta_transform.f[1] = 0.0f;
    g_CurrentPlayer->field_488.theta_transform.f[2] =
        cosf(look_angle_radians);

    g_CurrentPlayer->prop = (PropRecord *)ge_port_player_spawn_allocate_prop();
    if (g_CurrentPlayer->prop == NULL) {
        return 0;
    }
    g_CurrentPlayer->prop->obj = NULL;
    g_CurrentPlayer->prop->type = PROP_TYPE_VIEWER;
    g_CurrentPlayer->prop->pos.f[0] =
        g_CurrentPlayer->bondprevpos.f[0] = start_pos.f[0];
    g_CurrentPlayer->prop->pos.f[1] =
        g_CurrentPlayer->bondprevpos.f[1] = start_pos.f[1];
    g_CurrentPlayer->prop->pos.f[2] =
        g_CurrentPlayer->bondprevpos.f[2] = start_pos.f[2];
    g_CurrentPlayer->prop->stan = start_stan;
    ge_port_player_spawn_activate_prop(g_CurrentPlayer->prop);
    ge_port_player_spawn_enable_prop(g_CurrentPlayer->prop);
    g_CurrentPlayer->field_3B8.f[0] =
        g_CurrentPlayer->field_488.pos.f[0] / 0.100000023842f;
    g_CurrentPlayer->field_3B8.f[1] =
        g_CurrentPlayer->field_488.pos.f[1] / 0.100000023842f;
    g_CurrentPlayer->field_3B8.f[2] =
        g_CurrentPlayer->field_488.pos.f[2] / 0.100000023842f;

    ge_port_player_spawn_deregister_room(g_CurrentPlayer->prop,
                                         g_CurrentPlayer->registeredroom);
    g_CurrentPlayer->registeredroom = room;
    ge_port_player_spawn_register_room(g_CurrentPlayer->prop,
                                       g_CurrentPlayer->registeredroom);

    ge_port_player_spawn_publish(
        g_CurrentPlayer->field_488.collision_position.f,
        g_CurrentPlayer->field_488.pos.f,
        g_CurrentPlayer->field_488.applied_view.f,
        g_CurrentPlayer->field_488.applied_view2.f,
        g_CurrentPlayer->field_488.theta_transform.f,
        g_CurrentPlayer->bondprevpos.f,
        g_CurrentPlayer->field_3B8.f,
        g_CurrentPlayer->stanHeight,
        g_CurrentPlayer->eyeheight,
        g_CurrentPlayer->vv_theta,
        g_CurrentPlayer->field_488.collision_radius,
        g_CurrentPlayer->field_488.current_tile_ptr,
        g_CurrentPlayer->field_488.current_tile_ptr_for_portals,
        g_CurrentPlayer->prop,
        g_CurrentPlayer->registeredroom);

    /* Exact tail of bondviewLoadSetupIntroSection.  The bounded spawn slice
     * does not execute the later original function body, so publish the same
     * zero-terminated authored death-animation count here. */
    g_bondviewBondDeathAnimationsCount = 0;
    while (g_bondviewBondDeathAnimations[
            g_bondviewBondDeathAnimationsCount] != 0) {
        g_bondviewBondDeathAnimationsCount++;
    }
    return 1;
}

void ge_original_bond_intro_bind(const GeOriginalIntroProviders *providers,
                                 GeOriginalIntroSpawnState *state)
{
    memset(&g_GeOriginalIntroProviders, 0,
           sizeof(g_GeOriginalIntroProviders));
    if (providers != NULL) {
        g_GeOriginalIntroProviders = *providers;
    }
    g_GeOriginalIntroSpawnState = state;
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
        state->pad_index = -1;
    }
}

static struct SetupIntroEmpty *ge_original_intro_advance(
    struct SetupIntroEmpty *record, size_t size)
{
    return (struct SetupIntroEmpty *)((uint8_t *)record + size);
}

extern s32 modelLoad(s32 modelid);
extern void bondinvReinitInv(void);
extern int bondinvAddInvItem(ITEM_IDS item);
extern int bondinvAddDoublesInvItem(ITEM_IDS right, ITEM_IDS left);
extern void give_cur_player_ammo(s32 ammo_type, s32 ammo_amount);
extern s32 currentPlayerEquipWeaponWrapper(GUNHAND hand, s32 next_weapon);

enum {
    GE_INTRO_EMPTY_SIZE = 0x04,
    GE_INTRO_SPAWN_SIZE = 0x0c,
    GE_INTRO_ITEM_SIZE = 0x10,
    GE_INTRO_AMMO_SIZE = 0x10,
    GE_INTRO_SWIRL_SIZE = 0x20,
    GE_INTRO_ANIM_SIZE = 0x08,
    GE_INTRO_CUFF_SIZE = 0x08,
    GE_INTRO_CAMERA_SIZE = 0x28,
    GE_INTRO_WATCH_SIZE = 0x0c,
    GE_INTRO_CREDITS_SIZE = 0x08
};

/* Exact decompiled body. Dam's normal PP7 and key records do not request a
 * projectile model, but retaining this preserves setup semantics for every
 * item record rather than special-casing the level. */
u32 weaponLoadProjectileModels(ITEM_IDS modelid)
{
    s32 model;

    model = -1;
    switch(modelid)
    {
    case ITEM_THROWKNIFE:
        model = PROJECTILES_TYPE_KNIFE;
        break;
    case ITEM_GRENADELAUNCH:
        model = PROJECTILES_TYPE_GLAUNCH_ROUND;
        break;
    case ITEM_ROCKETLAUNCH:
        model = PROJECTILES_TYPE_ROCKET_ROUND;
        break;
    case ITEM_GRENADE:
        model = PROJECTILES_TYPE_GRENADE;
        break;
    case ITEM_TIMEDMINE:
        model = PROJECTILES_TYPE_TIMED_MINE;
        break;
    case ITEM_PROXIMITYMINE:
        model = PROJECTILES_TYPE_PROX_MINE;
        break;
    case ITEM_REMOTEMINE:
        model = PROJECTILES_TYPE_REMOTE_MINE;
        break;
    case ITEM_TANKSHELLS:
        model = PROJECTILES_TYPE_ROCKET_ROUND2;
        break;
    case ITEM_BOMBCASE:
        model = PROJECTILES_TYPE_BOMBCASE;
        break;
    case ITEM_PLASTIQUE:
        model = PROJECTILES_TYPE_PLASTIQUE;
        break;
    case ITEM_BUG:
        model = PROJECTILES_TYPE_BUG;
        break;
    case ITEM_MICROCAMERA:
        model = PROJECTILES_TYPE_MICROCAMERA;
    }

    if (-1 < model)
    {
        return modelLoad(model);
    }
    return 0;
}

/* Exact inventory/ammo/cuff and starting-weapon behavior from
 * bondviewLoadSetupIntroSection. The platform boundary only separates it
 * from spawn placement so canonical inventory storage can exist first. */
int bondviewLoadSetupIntroLoadoutSlice(GeOriginalIntroLoadoutState *state)
{
    struct SetupIntroEmpty *intro_record;
    s32 set_starting_weapon = 0;
    s32 demo_slot = 0;

    if (state == NULL || g_CurrentPlayer == NULL) return 0;
    memset(state, 0, sizeof(*state));
    state->starting_weapon[GUNLEFT] = ITEM_UNARMED;
    state->starting_weapon[GUNRIGHT] = ITEM_UNARMED;
    if (g_GeOriginalIntroProviders.get_demo_slot != NULL) {
        demo_slot = g_GeOriginalIntroProviders.get_demo_slot(
            g_GeOriginalIntroProviders.context);
    }

    bondinvReinitInv();
    intro_record = (struct SetupIntroEmpty *)g_CurrentSetup.intro;
    if (intro_record != NULL) {
        while (intro_record->type != INTROTYPE_END) {
            switch (intro_record->type) {
            case INTROTYPE_SPAWN:
                intro_record = ge_original_intro_advance(
                    intro_record, GE_INTRO_SPAWN_SIZE);
                break;
            case INTROTYPE_ITEM: {
                struct SetupIntroItem *intro_item =
                    (struct SetupIntroItem *)intro_record;
                if (demo_slot == intro_item->is_demo_playback) {
                    weaponLoadProjectileModels(intro_item->item_right);
                    state->projectile_model_requests++;
                    if (intro_item->item_left >= 0) {
                        weaponLoadProjectileModels(intro_item->item_left);
                        state->projectile_model_requests++;
                        bondinvAddDoublesInvItem(intro_item->item_right,
                                                  intro_item->item_left);
                    } else {
                        bondinvAddInvItem(intro_item->item_right);
                    }
                    state->item_records++;
                    if (set_starting_weapon == 0) {
                        state->starting_weapon[GUNRIGHT] =
                            intro_item->item_right;
                        set_starting_weapon = 1;
                        if (intro_item->item_left >= 0) {
                            state->starting_weapon[GUNLEFT] =
                                intro_item->item_left;
                        }
                    }
                }
                intro_record = ge_original_intro_advance(
                    intro_record, GE_INTRO_ITEM_SIZE);
                break;
            }
            case INTROTYPE_AMMO: {
                struct SetupIntroAmmo *intro_ammo =
                    (struct SetupIntroAmmo *)intro_record;
                if (demo_slot == intro_ammo->is_demo_playback) {
                    give_cur_player_ammo(intro_ammo->ammo_type,
                                         intro_ammo->ammo_amount);
                    state->ammo_records++;
                }
                intro_record = ge_original_intro_advance(
                    intro_record, GE_INTRO_AMMO_SIZE);
                break;
            }
            case INTROTYPE_SWIRL:
                intro_record = ge_original_intro_advance(
                    intro_record, GE_INTRO_SWIRL_SIZE);
                break;
            case INTROTYPE_ANIM:
                intro_record = ge_original_intro_advance(
                    intro_record, GE_INTRO_ANIM_SIZE);
                break;
            case INTROTYPE_CUFF:
                g_CurrentPlayer->bondtype =
                    ((struct SetupIntroCuff *)intro_record)->bondtype;
                state->bondtype = g_CurrentPlayer->bondtype;
                intro_record = ge_original_intro_advance(
                    intro_record, GE_INTRO_CUFF_SIZE);
                break;
            case INTROTYPE_CAMERA:
                intro_record = ge_original_intro_advance(
                    intro_record, GE_INTRO_CAMERA_SIZE);
                break;
            case INTROTYPE_WATCH:
                intro_record = ge_original_intro_advance(
                    intro_record, GE_INTRO_WATCH_SIZE);
                break;
            case INTROTYPE_CREDITS:
                intro_record = ge_original_intro_advance(
                    intro_record, GE_INTRO_CREDITS_SIZE);
                break;
            default:
                intro_record = ge_original_intro_advance(
                    intro_record, GE_INTRO_EMPTY_SIZE);
                break;
            }
        }
    }

    bondinvAddInvItem(ITEM_FIST);
    if (set_starting_weapon == 0)
        state->starting_weapon[GUNRIGHT] = ITEM_FIST;
    currentPlayerEquipWeaponWrapper(GUNLEFT,
        state->starting_weapon[GUNLEFT]);
    currentPlayerEquipWeaponWrapper(GUNRIGHT,
        state->starting_weapon[GUNRIGHT]);
    state->loaded = 1;
    return 1;
}

/*
 * Exact spawn-selection and placement slice from
 * bondviewLoadSetupIntroSection. Non-spawn records retain the original record
 * sizes so this walks the complete generated intro stream; their inventory,
 * camera, watch, and cuff side effects remain outside this bounded slice.
 */
void bondviewLoadSetupIntroSpawnSlice(void)
{
    struct SetupIntroEmpty *intro_record;
    struct PadRecord *startpads[0x10];
    struct PadRecord *startpad;
    struct coord3d start_pos = { 0 };
    f32 start_look_angle = 0.0f;
    f32 stan_height;
    s32 startpadcount = 0;
    s32 camera_count = 0;
    s32 demo_slot = 0;

    if (g_GeOriginalIntroSpawnState == NULL) {
        return;
    }

    if (g_GeOriginalIntroProviders.get_demo_slot != NULL) {
        demo_slot = g_GeOriginalIntroProviders.get_demo_slot(
            g_GeOriginalIntroProviders.context);
    }

    intro_record = (struct SetupIntroEmpty *)g_CurrentSetup.intro;

    if (intro_record != NULL)
    {
        while (intro_record->type != INTROTYPE_END)
        {
            switch (intro_record->type)
            {
                case INTROTYPE_SPAWN:
                {
                    if (g_CurrentSetup.pads != NULL
                        && (demo_slot == ((struct SetupIntroSpawn*)intro_record)->is_demo_playback)
                        && startpadcount < (s32)(sizeof(startpads) / sizeof(startpads[0])))
                    {
                        startpads[startpadcount] = &g_CurrentSetup.pads[((struct SetupIntroSpawn*)intro_record)->index];
                        startpadcount++;
                    }

                    intro_record = ge_original_intro_advance(
                        intro_record, GE_INTRO_SPAWN_SIZE);
                }
                break;

                case INTROTYPE_ITEM:
                    intro_record = ge_original_intro_advance(
                        intro_record, GE_INTRO_ITEM_SIZE);
                    break;

                case INTROTYPE_AMMO:
                    intro_record = ge_original_intro_advance(
                        intro_record, GE_INTRO_AMMO_SIZE);
                    break;

                case INTROTYPE_SWIRL:
                    intro_record = ge_original_intro_advance(
                        intro_record, GE_INTRO_SWIRL_SIZE);
                    break;

                case INTROTYPE_ANIM:
                    intro_record = ge_original_intro_advance(
                        intro_record, GE_INTRO_ANIM_SIZE);
                    break;

                case INTROTYPE_CUFF:
                    intro_record = ge_original_intro_advance(
                        intro_record, GE_INTRO_CUFF_SIZE);
                    break;

                case INTROTYPE_CAMERA:
                    camera_count++;
                    intro_record = ge_original_intro_advance(
                        intro_record, GE_INTRO_CAMERA_SIZE);
                    break;

                case INTROTYPE_WATCH:
                    intro_record = ge_original_intro_advance(
                        intro_record, GE_INTRO_WATCH_SIZE);
                    break;

                case INTROTYPE_CREDITS:
                    intro_record = ge_original_intro_advance(
                        intro_record, GE_INTRO_CREDITS_SIZE);
                    break;

                default:
                    intro_record = ge_original_intro_advance(
                        intro_record, GE_INTRO_EMPTY_SIZE);
                    break;
            }
        }
    }

    g_GeOriginalIntroSpawnState->matching_spawn_count =
        (uint32_t)startpadcount;
    g_GeOriginalIntroSpawnState->camera_count = (uint32_t)camera_count;
    if (camera_count > 0) {
        /* Exact bondviewLoadSetupIntroSection choice. The linked-list walk
         * selects from the tail; publish that tail-relative index even though
         * the bounded live camera owner does not consume it yet. */
        g_GeOriginalIntroSpawnState->camera_index =
            randomGetNext() % (u32)camera_count;
    }

    if (startpadcount > 0)
    {
        /* Single-player uses the first matching spawn exactly as the original. */
        startpad = startpads[0];
        start_pos.f[0] = startpad->pos.f[0];
        start_pos.f[2] = startpad->pos.f[2];

        if (g_GeOriginalIntroProviders.get_floor_y == NULL
                || startpad->stan == NULL) {
            return;
        }

        stan_height = g_GeOriginalIntroProviders.get_floor_y(
            g_GeOriginalIntroProviders.context, startpad->stan,
            start_pos.f[0], start_pos.f[2]);
        start_pos.f[1] = stan_height;
        if (g_GeOriginalIntroProviders.get_eye_height != NULL) {
            start_pos.f[1] += g_GeOriginalIntroProviders.get_eye_height(
                g_GeOriginalIntroProviders.context);
        }
        start_look_angle = M_TAU_F - atan2f(startpad->look.f[0],
                                             startpad->look.f[2]);

        g_GeOriginalIntroSpawnState->pad_index =
            (int32_t)(startpad - g_CurrentSetup.pads);
        g_GeOriginalIntroSpawnState->position[0] = start_pos.f[0];
        g_GeOriginalIntroSpawnState->position[1] = start_pos.f[1];
        g_GeOriginalIntroSpawnState->position[2] = start_pos.f[2];
        g_GeOriginalIntroSpawnState->floor_y = stan_height;
        g_GeOriginalIntroSpawnState->look_angle_radians = start_look_angle;
        g_GeOriginalIntroSpawnState->look_angle_degrees =
            (start_look_angle * 360.0f) / M_TAU_F;
        g_GeOriginalIntroSpawnState->stan_name = startpad->plink;
        g_GeOriginalIntroSpawnState->stan = startpad->stan;
        if (g_GeOriginalIntroProviders.commit_player_spawn != NULL) {
            g_GeOriginalIntroSpawnState->player_committed =
                g_GeOriginalIntroProviders.commit_player_spawn(
                    g_GeOriginalIntroProviders.context,
                    g_GeOriginalIntroSpawnState->position,
                    stan_height,
                    g_GeOriginalIntroSpawnState->position[1] - stan_height,
                    start_look_angle,
                    startpad->stan);
        }
        g_GeOriginalIntroSpawnState->loaded = 1;
    }
}

#else

#include <ultra64.h>
#include <memp.h>
#include <bondconstants.h>
#include <boss.h>
#include "math_atan2f.h"
#include "bondview_r.h"
#include "bondview.h"
#include "random.h"
#include "game/bondinv.h"
#include "game/chrai.h"
#include "game/front.h"
#include "game/gun.h"
#include "game/language.h"
#include "game/player.h"
#include "game/ramromreplay.h"
#include "game/stan.h"


/**
 * Address 0x8002A780.
*/
struct coord3d default_start_position = { 0 };

u32 weaponLoadProjectileModels(ITEM_IDS modelid)
{
    s32 model;

    model = -1;
    switch(modelid)
    {
    case ITEM_THROWKNIFE:
        model = PROJECTILES_TYPE_KNIFE;
        break;
    case ITEM_GRENADELAUNCH:
        model = PROJECTILES_TYPE_GLAUNCH_ROUND;
        break;
    case ITEM_ROCKETLAUNCH:
        model = PROJECTILES_TYPE_ROCKET_ROUND;
        break;
    case ITEM_GRENADE:
        model = PROJECTILES_TYPE_GRENADE;
        break;
    case ITEM_TIMEDMINE:
        model = PROJECTILES_TYPE_TIMED_MINE;
        break;
    case ITEM_PROXIMITYMINE:
        model = PROJECTILES_TYPE_PROX_MINE;
        break;
    case ITEM_REMOTEMINE:
        model = PROJECTILES_TYPE_REMOTE_MINE;
        break;
    case ITEM_TANKSHELLS:
        model = PROJECTILES_TYPE_ROCKET_ROUND2;
        break;
    case ITEM_BOMBCASE:
        model = PROJECTILES_TYPE_BOMBCASE;
        break;
    case ITEM_PLASTIQUE:
        model = PROJECTILES_TYPE_PLASTIQUE;
        break;
    case ITEM_BUG:
        model = PROJECTILES_TYPE_BUG;
        break;
    case ITEM_MICROCAMERA:
        model = PROJECTILES_TYPE_MICROCAMERA;
    }

    if (-1 < model)
    {
        return modelLoad(model);
    }
    return 0;
}

void bondviewLoadSetupIntroSection(void)
{

#define FLOAT_INIT 0


#if defined(VERSION_EU)
#define FIELD_6C_FACTOR 0.20039999485f
#define FIELD_3B8_FACTOR 0.118799984455f
#else
#define FIELD_6C_FACTOR 0.170000016689f
#define FIELD_3B8_FACTOR 0.100000023842f
#endif

    // declarations

    struct coord3d start_pos;
    f32 start_look_angle;
    StandTile *start_stan;
    struct SetupIntroEmpty *intro_record;
    s32 set_starting_weapon;
    s32 rand_camera_index;
    CreditsEntry *credits;
    s32 rand_pad_index;
    f32 stan_height;
    s32 i;
    struct SetupIntroItem *intro_item;
    struct SetupIntroSwirl *intro_swirl;
    struct SetupIntroWatch *intro_watch;
    struct SetupIntroCredits *intro_credits;
    s32 padding[5];

    // done with declarations

    start_pos = default_start_position;

    intro_record = (struct SetupIntroEmpty *)g_CurrentSetup.intro;
    g_isBondKIA = 0;
    g_bondviewForceDisarm = 0;
    resolution = 0;
    cameraBufferToggle = 0;
    cameraFrameCounter1 = 0;
    set_starting_weapon = 0;
    cameraFrameCounter2 = 0;
    start_look_angle = FLOAT_INIT;

    if (bossGetStageNum() == LEVELID_CUBA)
    {
        resolution = (s32)mempAllocBytesInBank(0x46EA0, MEMPOOL_STAGE);
        resolution = (resolution + 0x3f) & ~0x3F;
        cameraFrameCounter1 = 1;
    }

    camera_80036438 = 0;
    credits_state = 0;
    credits_pointer = NULL;
    g_ForceBondMoveOffset.f[0] = FLOAT_INIT;
    g_ForceBondMoveOffset.f[1] = FLOAT_INIT;
    g_ForceBondMoveOffset.f[2] = FLOAT_INIT;
    g_SurroundBondWithExplosionsFlag = 0;
    startpadcount = 0;
    g_PlayerIsInTank = 0;
    g_WorldTankProp = 0;
    g_PlayerTankProp = NULL;
    g_PlayerTankYOffset = FLOAT_INIT;
    g_TankSfxState[0] = NULL;
    g_TankSfxState[1] = NULL;
    g_TankTurnSpeed = FLOAT_INIT;
    g_TankOrientationAngle = FLOAT_INIT;
    tank_turret_unused_angle = FLOAT_INIT;
    g_TankTurretVerticalAngle = FLOAT_INIT;
    g_TankTurretVerticalAngleRelated = FLOAT_INIT;
    g_TankTurretOrientationAngleRad = FLOAT_INIT;
    g_TankTurretOrientationAngleDeg = FLOAT_INIT;
    tank_turret_turn_speed = FLOAT_INIT;
    g_BondCanEnterTank = 0;
    g_TankTurretAngle = FLOAT_INIT;
    g_TankTurretTurn = FLOAT_INIT;
    g_ExplodeTankOnDeathFlag = 0;
    is_timer_active = 1;
    g_PlayerInvincible = FALSE;
    g_CameraMode = 0;
    g_CameraAfterCinema = 0;
    camera_fade_active = 0;
    stop_time_flag = 0;
    camera_transition_timer = FLOAT_INIT;
    intro_camera_index = CAMERAMODE_INTRO;
    g_IntroSwirl = NULL;
    ptr_random06cam_entry = NULL;
    g_CurrentSetupIntroCamera = NULL;
    g_SetupIntroCameraCount = 0;
    mission_timer = 0;
    watch_time_0 = 0;
    g_IntroAnimationIndex = 0;
    watch_transition_time = 0.9090909f;
    starting_weapon[GUNLEFT] = ITEM_UNARMED;
    starting_weapon[GUNRIGHT] = ITEM_UNARMED;

    if (intro_record != NULL)
    {
        while (intro_record->type != INTROTYPE_END)
        {
            switch (intro_record->type)
            {
                case INTROTYPE_SPAWN:
                {
                    if (g_CurrentSetup.pads != NULL
                        && (check_ramrom_flags() == ((struct SetupIntroSpawn*)intro_record)->is_demo_playback))
                    {
                        g_Startpad[startpadcount] = &g_CurrentSetup.pads[((struct SetupIntroSpawn*)intro_record)->index];
                        startpadcount++;
                    }

                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroSpawn));
                }
                break;

                case INTROTYPE_ITEM:
                {
                    intro_item = (struct SetupIntroItem*)intro_record;

                    if (check_ramrom_flags() == intro_item->is_demo_playback)
                    {
                        weaponLoadProjectileModels(intro_item->item_right);

                        if (intro_item->item_left >= 0)
                        {
                            weaponLoadProjectileModels(intro_item->item_left);
                            bondinvAddDoublesInvItem(intro_item->item_right, intro_item->item_left);
                        }
                        else
                        {
                            bondinvAddInvItem(intro_item->item_right);
                        }

                        if (set_starting_weapon == 0)
                        {
                            starting_weapon[GUNRIGHT] = intro_item->item_right;

                            if(intro_item->item_left);

                            set_starting_weapon = 1;

                            if (intro_item->item_left >= 0)
                            {
                                starting_weapon[GUNLEFT] = intro_item->item_left;
                            }
                        }
                    }

                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroItem));
                }
                break;

                case INTROTYPE_AMMO:
                {
                    if (check_ramrom_flags() == ((struct SetupIntroAmmo*)intro_record)->is_demo_playback)
                    {
                        give_cur_player_ammo(((struct SetupIntroAmmo*)intro_record)->ammo_type, ((struct SetupIntroAmmo*)intro_record)->ammo_amount);
                    }

                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroAmmo));
                }
                break;

                case INTROTYPE_SWIRL:
                {
                    intro_swirl = (struct SetupIntroSwirl*)intro_record;

                    if (g_IntroSwirl == NULL)
                    {
                        g_IntroSwirl = intro_swirl;
                    }

                    intro_swirl->unk08.fval = intro_swirl->unk08.ival / M_U16_MAX_VALUE_F;
                    intro_swirl->unk0C.fval = intro_swirl->unk0C.ival / M_U16_MAX_VALUE_F;
                    intro_swirl->unk10.fval = intro_swirl->unk10.ival / M_U16_MAX_VALUE_F;
                    intro_swirl->unk14.fval = intro_swirl->unk14.ival / M_U16_MAX_VALUE_F;
                    intro_swirl->unk18.fval = intro_swirl->unk18.ival / M_U16_MAX_VALUE_F;

                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroSwirl));
                }
                break;

                case INTROTYPE_ANIM:
                {
                    g_IntroAnimationIndex = ((struct SetupIntroAnim*)intro_record)->intro_anim;

                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroAnim));
                }
                break;

                case INTROTYPE_CUFF:
                {
                    g_CurrentPlayer->bondtype = ((struct SetupIntroCuff*)intro_record)->bondtype;

                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroCuff));
                }
                break;

                case INTROTYPE_CAMERA:
                {
                    if (get_cur_playernum() == 0)
                    {
                        ((struct SetupIntroCamera*)intro_record)->prev = g_CurrentSetupIntroCamera;
                        g_CurrentSetupIntroCamera = (struct SetupIntroCamera*)intro_record;
                        g_SetupIntroCameraCount = g_SetupIntroCameraCount + 1;

                        ((struct SetupIntroCamera*)intro_record)->unk04.fval = ((struct SetupIntroCamera*)intro_record)->unk04.ival / 100.0f;
                        ((struct SetupIntroCamera*)intro_record)->unk08.fval = ((struct SetupIntroCamera*)intro_record)->unk08.ival / 100.0f;
                        ((struct SetupIntroCamera*)intro_record)->unk0C.fval = ((struct SetupIntroCamera*)intro_record)->unk0C.ival / 100.0f;
                        ((struct SetupIntroCamera*)intro_record)->unk10.fval = ((struct SetupIntroCamera*)intro_record)->unk10.ival / M_U16_MAX_VALUE_F;
                        ((struct SetupIntroCamera*)intro_record)->unk14.fval = ((struct SetupIntroCamera*)intro_record)->unk14.ival / M_U16_MAX_VALUE_F;

                        ((struct SetupIntroCamera*)intro_record)->lang1c.lang_ptr = langGet(((struct SetupIntroCamera*)intro_record)->lang1c.lang_index[1]);

                        if (((struct SetupIntroCamera*)intro_record)->lang20.lang_index != 0)
                        {
                            ((struct SetupIntroCamera*)intro_record)->lang20.lang_ptr = langGet((u16)((struct SetupIntroCamera*)intro_record)->lang20.lang_index);
                        }
                    }

                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroCamera));
                }
                break;

                case INTROTYPE_WATCH:
                {
                    intro_watch = (struct SetupIntroWatch*)intro_record;

                    watch_time_0 = 0;

                    if (intro_watch->minutes > 0)
                    {
                        watch_time_0 += (intro_watch->minutes % 60) * (60*60);
                    }

                    if (intro_watch->hours > 0)
                    {
                        watch_time_0 += ((intro_watch->hours % 12) * (60*60*60));
                    }

                    if (watch_time_0);

                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroWatch));
                }
                break;

                case INTROTYPE_CREDITS:
                {
                    intro_credits = (struct SetupIntroCredits*)intro_record;

                    // hack: bad address math
                    credits = (CreditsEntry*)((s32)g_ptrStageSetupFile + (s32)intro_credits->unk04);
                    credits_pointer = credits;

                    // what is the point of this?
                    while (credits->TextId1 != 0 || credits->TextId2 != 0)
                    {
                        credits++;
                    }

                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroCredits));
                }
                break;

                default:
                {
                    #ifdef DEBUG
                        ossyncprintf("unknown bondstart type %d!\n",intro_record->type);
                    #endif
                    intro_record = (struct SetupIntroEmpty*)((s32)intro_record + sizeof(struct SetupIntroEmpty));
                }
                break;

            }
        }
    }

    if (g_CurrentSetupIntroCamera != NULL)
    {
        ptr_random06cam_entry = g_CurrentSetupIntroCamera;
        rand_camera_index = (s32)(randomGetNext() % (u32) g_SetupIntroCameraCount);
        while (rand_camera_index > 0)
        {
            rand_camera_index--;
            ptr_random06cam_entry = ptr_random06cam_entry->prev;
        }
    }

    bondinvAddInvItem(ITEM_FIST);

    if (set_starting_weapon == 0)
    {
        starting_weapon[GUNRIGHT] = ITEM_FIST;
    }

    g_CurrentPlayer->field_78 = FLOAT_INIT;
    g_CurrentPlayer->field_7C = -0.0001f;
    g_CurrentPlayer->field_80 = FLOAT_INIT;

    if (startpadcount > 0)
    {
        if ((getPlayerCount() >= 2) && (startpadcount > 0))
        {
            rand_pad_index = bondviewGetRandomSpawnPadIndex();
        }
        else
        {
            rand_pad_index = 0;
        }

#ifdef DEBUG
        assert(g_Startpad[rand_pad_index]->stan); //              (".\\ported\\bondview_r.cpp",0x171,"Assertion failed: g_Startpad[sp]->stan");
#endif

        start_pos.f[0] = g_Startpad[rand_pad_index]->pos.f[0];
        start_pos.f[2] = g_Startpad[rand_pad_index]->pos.f[2];


        start_stan = g_Startpad[rand_pad_index]->stan;

        stan_height = bondviewYPositionRelated(start_stan, start_pos.f[0], start_pos.f[2]);
        start_pos.f[1] = g_CurrentPlayer->eyeheight + stan_height;
        g_CurrentPlayer->field_70 = stan_height;
        start_look_angle = M_TAU_F - atan2f(g_Startpad[rand_pad_index]->look.f[0], g_Startpad[rand_pad_index]->look.f[2]);
    }
    else
    {
        start_stan = sub_GAME_7F0AFB78(&start_pos.f[0], &start_pos.f[1], &start_pos.f[2], 30.0f);
        stan_height = bondviewYPositionRelated(start_stan, start_pos.f[0], start_pos.f[2]);
        start_pos.f[1] = g_CurrentPlayer->eyeheight + stan_height;
        g_CurrentPlayer->field_70 = stan_height;
    }

    g_CurrentPlayer->vv_theta = (start_look_angle * 360.0f) / M_TAU_F;
    g_CurrentPlayer->stanHeight = stan_height;
    g_CurrentPlayer->field_6C = stan_height / FIELD_6C_FACTOR;
    change_player_pos_to_target(&g_CurrentPlayer->field_488, &start_pos, start_stan);
    g_CurrentPlayer->field_488.theta_transform.f[0] = -sinf(start_look_angle);
    g_CurrentPlayer->field_488.theta_transform.f[1] = FLOAT_INIT;
    g_CurrentPlayer->field_488.theta_transform.f[2] = cosf(start_look_angle);
    sub_GAME_7F089718(D_800364D0);
    dword_CODE_bss_80079DA0 = 0;


    for (i=0; i<BSS_80079DA8_LENGTH; i++)
    {
        dword_CODE_bss_80079DA4 = 0;
        dword_CODE_bss_80079DA8[i] = 0;
    }

    bondviewResetIntroCameraMessageDialogs();
    bondviewResetUpperTextDisplay();
    g_CurrentPlayer->prop = chrpropAllocate();
    g_CurrentPlayer->prop->obj = NULL;
    g_CurrentPlayer->prop->type = PROP_TYPE_VIEWER;

    g_CurrentPlayer->prop->pos.f[0] =
        g_CurrentPlayer->bondprevpos.f[0] = start_pos.f[0];

    g_CurrentPlayer->prop->pos.f[1] =
        g_CurrentPlayer->bondprevpos.f[1] = start_pos.f[1];

    g_CurrentPlayer->prop->pos.f[2] =
        g_CurrentPlayer->bondprevpos.f[2] = start_pos.f[2];

    g_CurrentPlayer->prop->stan = start_stan;

    chrpropActivate(g_CurrentPlayer->prop);
    chrpropEnable(g_CurrentPlayer->prop);
    g_CurrentPlayer->field_3B8.f[0] = (g_CurrentPlayer->field_488.pos.f[0] / FIELD_3B8_FACTOR);
    g_CurrentPlayer->field_3B8.f[1] = (g_CurrentPlayer->field_488.pos.f[1] / FIELD_3B8_FACTOR);
    g_CurrentPlayer->field_3B8.f[2] = (g_CurrentPlayer->field_488.pos.f[2] / FIELD_3B8_FACTOR);

    if (getPlayerCount() == 1)
    {
        bondviewSetCameraMode(CAMERAMODE_INTRO);
    }
    else
    {
        bondviewSetCameraMode(CAMERAMODE_MP);
    }

    g_bondviewBondDeathAnimationsCount = 0;
    while (g_bondviewBondDeathAnimations[g_bondviewBondDeathAnimationsCount] != 0)
    {
        g_bondviewBondDeathAnimationsCount++;
    }

    g_CurrentPlayer->startnewbonddie = TRUE;
    g_CurrentPlayer->redbloodfinished = FALSE;
    g_CurrentPlayer->deathanimfinished = FALSE;
    camera_mode = CAMERAMODE_NONE;
}

#endif /* GE_PORT_BOND_INTRO_SPAWN_SLICE */
