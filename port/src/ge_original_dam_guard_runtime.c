#include "ge_original_dam_guard_runtime.h"

#include <stddef.h>
#include <string.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#ifndef PLAYERFLAG
typedef int PLAYERFLAG;
#endif
#include "game/chrai.h"

#include "ge_original_bond_input_provider.h"
#include "ge_original_dam_guard_chr_scheduler.h"
#include "ge_original_dam_guards.h"
#include "ge_original_gun_frame_arena.h"
#include "ge_original_gun_live.h"
#include "ge_original_gameplay_services.h"
#include "game/bondview.h"

static GeOriginalDamGuardRuntimeStats ge_guard_runtime_stats;

typedef struct GeDamGuardCombatAudit {
    s32 firecount[GE_ORIGINAL_DAM_INITIAL_GUARD_CAPACITY][2];
    uint32_t sound_play_calls;
    f32 health;
    f32 armour;
} GeDamGuardCombatAudit;

extern s32 g_ClockTimer;
extern s32 g_GlobalTimer;
extern f32 g_GlobalTimerDelta;
extern struct player *g_CurrentPlayer;
extern struct player *g_playerPointers[4];
extern ChrRecord *g_ChrSlots;
extern s32 g_NumChrSlots;
extern stagesetup g_CurrentSetup;

static GeOriginalDamGuardRuntimeStatus ge_guard_runtime_reject(
    GeOriginalDamGuardRuntimeStatus status)
{
    ge_guard_runtime_stats.rejected_ticks++;
    ge_guard_runtime_stats.last_status = (uint8_t)status;
    return status;
}

static int ge_guard_runtime_active_list_valid(void)
{
    unsigned seen[GE_ORIGINAL_DAM_INITIAL_GUARD_CAPACITY] = { 0, 0, 0, 0 };
    unsigned live[GE_ORIGINAL_DAM_INITIAL_GUARD_CAPACITY] = { 0, 0, 0, 0 };
    PropRecord *active;
    size_t guard_index;
    size_t visited = 0U;
#if defined(GE_DAM_FULL_PROPS_LIVE)
    unsigned player_seen = 0U;
#endif

    if (ge_original_dam_guards_count()
            != GE_ORIGINAL_DAM_INITIAL_GUARD_CAPACITY)
        return 0;
    if (g_ChrSlots != ge_original_dam_guard_chr(0)
            || g_NumChrSlots < GE_ORIGINAL_DAM_INITIAL_GUARD_CAPACITY)
        return 0;

    for (guard_index = 0U;
            guard_index < GE_ORIGINAL_DAM_INITIAL_GUARD_CAPACITY;
            guard_index++) {
        PropRecord *prop = ge_original_dam_guard_prop(guard_index);
        ChrRecord *chr = ge_original_dam_guard_chr(guard_index);
        size_t list_index;
        int authored_list = 0;
        if (prop == NULL || chr == NULL) return 0;
#if defined(GE_DAM_FULL_PROPS_LIVE)
        live[guard_index] = ge_original_dam_guard_is_live(guard_index) != 0;
        if (!live[guard_index]) {
            /* Canonical chrpropCleanupForRemoval clears both of these before
             * propsTick returns TICKOP_FREE. The old PropRecord may already
             * be back on the shared free list, so never inspect it here. */
            if (chr->model != NULL || chr->chrnum != -1) return 0;
            continue;
        }
#else
        live[guard_index] = 1U;
#endif
        if (prop->type != PROP_TYPE_CHR || prop->chr != chr
                || chr->prop != prop || chr->model == NULL
                || chr->ailist == NULL)
            return 0;
        if (g_CurrentSetup.ailists == NULL) return 0;
        for (list_index = 0U; list_index < 1024U
                && g_CurrentSetup.ailists[list_index].ailist != NULL;
                list_index++) {
            if (g_CurrentSetup.ailists[list_index].ailist == chr->ailist) {
                authored_list = 1;
                break;
            }
        }
        if (!authored_list) return 0;
    }

    for (active = chrpropGetActiveTail(); active != NULL;
            active = active->prev) {
        if (++visited > MAX_PROPS) return 0;
#if defined(GE_PLATFORM_3DS) && defined(GE_DAM_FULL_PROPS_LIVE)
        if (active->type == PROP_TYPE_OBJ
                || active->type == PROP_TYPE_WEAPON
                || active->type == PROP_TYPE_DOOR) {
            const ObjectRecord *obj = active->obj;
            if (obj == NULL || obj->prop != active || obj->model == NULL
                    || obj->model->obj == NULL)
                return 0;
        }
#endif
#if defined(GE_DAM_FULL_PROPS_LIVE)
        if ((active->flags & PROPFLAG_ENABLED) == 0U) return 0;
        if (active == g_CurrentPlayer->prop) player_seen++;
#endif
        for (guard_index = 0U;
                guard_index < GE_ORIGINAL_DAM_INITIAL_GUARD_CAPACITY;
                guard_index++) {
            if (live[guard_index]
                    && active == ge_original_dam_guard_prop(guard_index))
                seen[guard_index]++;
        }
    }
    for (guard_index = 0U;
            guard_index < GE_ORIGINAL_DAM_INITIAL_GUARD_CAPACITY;
            guard_index++) {
        if (seen[guard_index] != live[guard_index]) return 0;
    }
#if defined(GE_DAM_FULL_PROPS_LIVE)
    if (g_CurrentPlayer->prop == NULL
            || g_CurrentPlayer->prop->type != PROP_TYPE_VIEWER
            || player_seen != 1U)
        return 0;
#endif
    return 1;
}

static void ge_guard_runtime_combat_audit(GeDamGuardCombatAudit *audit)
{
    size_t guard_index;
    memset(audit, 0, sizeof(*audit));
    audit->health = g_CurrentPlayer->bondhealth;
    audit->armour = g_CurrentPlayer->bondarmour;
    {
        GeOriginalGameplayServiceStats services;
        ge_original_gameplay_services_snapshot(&services);
        audit->sound_play_calls = services.sound_play_calls;
    }
    for (guard_index = 0U;
            guard_index < GE_ORIGINAL_DAM_INITIAL_GUARD_CAPACITY;
            guard_index++) {
        ChrRecord *chr = ge_original_dam_guard_chr(guard_index);
        size_t hand;
        if (chr == NULL || chr->model == NULL) continue;
        for (hand = 0U; hand < 2U; hand++) {
            audit->firecount[guard_index][hand] = chr->firecount[hand];
        }
    }
}

static void ge_guard_runtime_record_combat(
    const GeDamGuardCombatAudit *before,
    const GeDamGuardCombatAudit *after)
{
    size_t guard_index;
    f32 health_damage = before->health - after->health;
    f32 armour_damage = before->armour - after->armour;
    for (guard_index = 0U;
            guard_index < GE_ORIGINAL_DAM_INITIAL_GUARD_CAPACITY;
            guard_index++) {
        size_t hand;
        for (hand = 0U; hand < 2U; hand++) {
            /* Exact chrlvTriggerFireWeapon can call each hand at most once in
             * this propsTick. Counting a state transition avoids relying on
             * signed firecount subtraction at its eventual wrap boundary. */
            if (after->firecount[guard_index][hand]
                    != before->firecount[guard_index][hand])
                ge_guard_runtime_stats.weapon_fire_dispatches++;
        }
    }
    if (after->sound_play_calls >= before->sound_play_calls)
        ge_guard_runtime_stats.weapon_sound_starts +=
            (uint32_t)(after->sound_play_calls - before->sound_play_calls);
    if (health_damage > 0.0f || armour_damage > 0.0f)
        ge_guard_runtime_stats.player_damage_events++;
    if (health_damage > 0.0f)
        ge_guard_runtime_stats.player_health_damage += health_damage;
    if (armour_damage > 0.0f)
        ge_guard_runtime_stats.player_armour_damage += armour_damage;
    ge_guard_runtime_stats.last_player_health = after->health;
    ge_guard_runtime_stats.last_player_armour = after->armour;
}

void ge_original_dam_guard_runtime_reset(void)
{
    memset(&ge_guard_runtime_stats, 0, sizeof(ge_guard_runtime_stats));
}

GeOriginalDamGuardRuntimeStatus ge_original_dam_guard_runtime_tick(
    const float world_to_view[4][4])
{
    GeOriginalBondInputProvider *provider =
        ge_original_bond_input_provider();
    GeOriginalDynFrameAudit before;
    GeOriginalDynFrameAudit after;
    GeOriginalGunLiveStats gun_stats;
    GeDamGuardCombatAudit combat_before;
    GeDamGuardCombatAudit combat_after;

    if (world_to_view == NULL
            || ge_original_dam_guards_count()
                != GE_ORIGINAL_DAM_INITIAL_GUARD_CAPACITY)
        return ge_guard_runtime_reject(
            GE_ORIGINAL_DAM_GUARD_RUNTIME_NOT_READY);
    if (provider == NULL || provider->current_player == NULL
            || provider->current_player != g_CurrentPlayer
            || g_playerPointers[0] != g_CurrentPlayer)
        return ge_guard_runtime_reject(
            GE_ORIGINAL_DAM_GUARD_RUNTIME_PLAYER_STATE);
    if (g_ClockTimer <= 0 || !(g_GlobalTimerDelta > 0.0f)
            || g_GlobalTimerDelta > MAXFLOAT
            || provider->clock_timer != g_ClockTimer
            || provider->global_timer != g_GlobalTimer
            || provider->global_timer_delta != g_GlobalTimerDelta)
        return ge_guard_runtime_reject(
            GE_ORIGINAL_DAM_GUARD_RUNTIME_TIMER_STATE);
    if (!ge_guard_runtime_active_list_valid())
        return ge_guard_runtime_reject(
            GE_ORIGINAL_DAM_GUARD_RUNTIME_ACTIVE_LIST);
    if (!ge_original_gun_frame_arena_audit(&before) || !before.active)
        return ge_guard_runtime_reject(
            GE_ORIGINAL_DAM_GUARD_RUNTIME_ARENA_STATE);
    ge_original_gun_live_snapshot(&gun_stats);
    if (gun_stats.ticks == 0U
            || gun_stats.last_frame_generation != before.generation)
        return ge_guard_runtime_reject(
            GE_ORIGINAL_DAM_GUARD_RUNTIME_GUN_ORDER);
    if (ge_guard_runtime_stats.ticks != 0U
            && before.generation
                == ge_guard_runtime_stats.last_arena_generation)
        return ge_guard_runtime_reject(
            GE_ORIGINAL_DAM_GUARD_RUNTIME_ALREADY_TICKED);

    ge_guard_runtime_combat_audit(&combat_before);
    ge_original_dam_guard_props_tick_exact();
    ge_guard_runtime_combat_audit(&combat_after);
    ge_guard_runtime_record_combat(&combat_before, &combat_after);

    if (!ge_original_gun_frame_arena_audit(&after) || !after.active
            || after.generation != before.generation
            || after.used < before.used)
        return ge_guard_runtime_reject(
            GE_ORIGINAL_DAM_GUARD_RUNTIME_ARENA_OVERFLOW);
    /* chrTick has already run the unchanged posIsOnScreen decision. Preserve
     * it while copying the transient dynAllocate matrices into the native
     * persistent render slots; forcing all four guards visible here defeated
     * original culling and transformed every skeleton behind the camera. */
    if (ge_original_dam_guards_update_visible_matrices(world_to_view)
            != GE_ORIGINAL_DAM_GUARD_OK)
        return ge_guard_runtime_reject(
            GE_ORIGINAL_DAM_GUARD_RUNTIME_MATRIX_REFRESH);

    ge_guard_runtime_stats.ticks++;
    ge_guard_runtime_stats.matrix_refreshes++;
    ge_guard_runtime_stats.last_arena_generation = before.generation;
    ge_guard_runtime_stats.last_global_timer = (uint32_t)g_GlobalTimer;
    ge_guard_runtime_stats.last_arena_bytes_before = (uint32_t)before.used;
    ge_guard_runtime_stats.last_arena_bytes_after = (uint32_t)after.used;
    ge_guard_runtime_stats.last_status = GE_ORIGINAL_DAM_GUARD_RUNTIME_OK;
    return GE_ORIGINAL_DAM_GUARD_RUNTIME_OK;
}

void ge_original_dam_guard_runtime_snapshot(
    GeOriginalDamGuardRuntimeStats *stats)
{
    if (stats != NULL) *stats = ge_guard_runtime_stats;
}

void ge_original_dam_guard_player_vitals_snapshot(float *health, float *armour)
{
    if (health != NULL)
        *health = g_CurrentPlayer != NULL ? g_CurrentPlayer->bondhealth : 0.0f;
    if (armour != NULL)
        *armour = g_CurrentPlayer != NULL ? g_CurrentPlayer->bondarmour : 0.0f;
}

void ge_original_dam_guard_player_combat_snapshot(
    GeOriginalPlayerCombatSnapshot *snapshot)
{
    if (snapshot == NULL) return;
    memset(snapshot, 0, sizeof(*snapshot));
    if (g_CurrentPlayer == NULL) return;
    snapshot->health = g_CurrentPlayer->bondhealth;
    snapshot->armour = g_CurrentPlayer->bondarmour;
    snapshot->actual_health = g_CurrentPlayer->actual_health;
    snapshot->actual_armour = g_CurrentPlayer->actual_armor;
    snapshot->damage_show_time = g_CurrentPlayer->damageshowtime;
    snapshot->dead = (uint8_t)(g_CurrentPlayer->bonddead != 0);
    snapshot->invincible = (uint8_t)(g_CurrentPlayer->cheatBondInvincible != 0);
}
