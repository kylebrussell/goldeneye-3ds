#include "ge_original_guard_bullet_hit.h"

#include <float.h>
#include <math.h>
#include <string.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "bondaicommands.h"
typedef int PLAYERFLAG;
#include "game/bondview.h"
#include "game/bg.h"
#include "game/bgfog.h"
#include "game/chr.h"
#include "game/chrai.h"
#include "game/gun.h"
#include "game/model.h"
#include "game/objecthandler.h"
#include "game/player.h"
#include "game/propobj.h"

#include "ge_original_dam_guards.h"
#include "ge_original_bg_visibility.h"

enum { GE_MODEL_HIT_ENTRY_COUNT = 600 };

static ModelHitEntry ge_model_hit_entries[GE_MODEL_HIT_ENTRY_COUNT];
static GeOriginalGuardBulletHitStats ge_hit_stats;
static ShotData ge_pending_shot;
static int ge_pending_hit_index = -1;
static void *ge_stage_guard_context;
static GeOriginalGuardBulletHitCount ge_stage_guard_count;
static GeOriginalGuardBulletHitActor ge_stage_guard_actor;
static void *ge_player_context;
static GeOriginalGuardBulletHitCount ge_player_count;
static GeOriginalGuardBulletHitPlayer ge_player_record;
static int32_t ge_observed_guard_index = -1;
ModelHitEntry *g_ModelHitFreeList;
/* Focused host binaries which retain reset-only paths do not install the live
 * input slice. The strong canonical symbol wins in the 3DS runtime. */
struct player *g_CurrentPlayer __attribute__((weak)) = NULL;

extern ModelHitEntry *sub_GAME_7F06B120(ModelHitEntry *head, Model *context);
extern void sub_GAME_7F06B248(ModelHitEntry *entry);
extern void chrTestHit(PropRecord *prop, ShotData *shotdata);
extern bool sub_GAME_7F054C58(coord3d *coord, f32 radius);
extern GunModelFileRecord gitem_structs[];

static size_t ge_authored_guard_count(void)
{
    return ge_stage_guard_context != NULL && ge_stage_guard_count != NULL
        ? ge_stage_guard_count(ge_stage_guard_context)
        : ge_original_dam_guards_count();
}

static size_t ge_bound_player_count(void)
{
    size_t count = ge_player_context != NULL && ge_player_count != NULL
        && ge_player_record != NULL ? ge_player_count(ge_player_context) : 0U;
    return count >= 2U && count <= 4U ? count : 0U;
}

static size_t ge_guard_count(void)
{
    return ge_authored_guard_count() + ge_bound_player_count();
}

static struct player *ge_player_for_prop(const PropRecord *prop)
{
    size_t slot;
    if (prop == NULL || prop->type != PROP_TYPE_VIEWER) return NULL;
    for (slot = 0U; slot < ge_bound_player_count(); ++slot) {
        struct player *player = ge_player_record(ge_player_context, slot);
        if (player != NULL && player->prop == prop) return player;
    }
    return NULL;
}

static int ge_guard_actor(size_t index, PropRecord **prop, ChrRecord **chr)
{
    void *runtime_prop = NULL;
    void *runtime_chr = NULL;
    const size_t authored_count = ge_authored_guard_count();
    if (prop == NULL || chr == NULL) return 0;
    if (index >= authored_count) {
        const size_t slot = index - authored_count;
        struct player *player;
        if (slot >= ge_bound_player_count()) return 0;
        player = ge_player_record(ge_player_context, slot);
        if (player == NULL || player->prop == NULL
                || player->prop->type != PROP_TYPE_VIEWER) return 0;
        *prop = player->prop;
        *chr = player->prop->chr;
        if (*chr == NULL || player->bodyModel != (*chr)->model) return 0;
    } else if (ge_stage_guard_context != NULL && ge_stage_guard_actor != NULL) {
        if (!ge_stage_guard_actor(ge_stage_guard_context, index,
                &runtime_prop, &runtime_chr)) return 0;
        *prop = runtime_prop;
        *chr = runtime_chr;
    } else {
        *prop = ge_original_dam_guard_prop(index);
        *chr = ge_original_dam_guard_chr(index);
    }
    return *prop != NULL && *chr != NULL && (*chr)->model != NULL
        && (*chr)->prop == *prop && (*prop)->chr == *chr;
}

/* Exact bondview.c getter body. */
Mtxf *currentPlayerGetViewToWorldMtxf(void)
{
    return g_CurrentPlayer->viewtoworldmtxf;
}

/* The live boundary is PP7-only, so this reads the same authored PP7
 * WeaponStats record returned by get_ptr_item_statistics without exporting a
 * second copy of that already-live input-slice symbol. */
u8 bondwalkItemGetObjectsShootThrough(ITEM_IDS item)
{
    return gitem_structs[item].item_weapon_stats->ObjectsShootThrough;
}

static void ge_initialize_model_hit_free_list(void)
{
    size_t index;
    memset(ge_model_hit_entries, 0, sizeof(ge_model_hit_entries));
    for (index = 0U; index < GE_MODEL_HIT_ENTRY_COUNT; index++) {
        ge_model_hit_entries[index].prev = index > 0U
            ? &ge_model_hit_entries[index - 1U] : NULL;
        ge_model_hit_entries[index].next =
            index + 1U < GE_MODEL_HIT_ENTRY_COUNT
            ? &ge_model_hit_entries[index + 1U] : NULL;
    }
    g_ModelHitFreeList = &ge_model_hit_entries[0];
    ge_hit_stats.pool_resets++;
}

/* The live renderer owns ChrRecord.field_20.  Unchanged chrTick releases the
 * preceding frame's list, evaluates posIsOnScreen, calculates matrices and
 * rebuilds this list only for a character which will be rendered.  Eagerly
 * allocating lists for every authored campaign guard here both violated that
 * ordering and could exhaust the original 600-entry pool before the first ray
 * was tested.  The isolated legacy Dam fixture has no chrTick/render pass, so
 * retain its bounded list preparation solely for that host-test owner. */
static int ge_prepare_legacy_guard_hit_lists(void)
{
    size_t index;
    for (index = 0U; index < ge_guard_count(); index++) {
        PropRecord *prop;
        ChrRecord *chr;
        if (!ge_guard_actor(index, &prop, &chr)) continue;
        if (prop->type == PROP_TYPE_VIEWER) continue;
        if (chr->field_20 == NULL) {
            chr->field_20 = sub_GAME_7F06B120(NULL, chr->model);
            if (chr->field_20 == NULL) return 0;
            ge_hit_stats.model_lists_built++;
        }
    }
    return 1;
}

static uint32_t ge_shot_registered_hit_count(const ShotData *shot)
{
    uint32_t count = 0U;
    size_t index;
    if (shot == NULL) return 0U;
    for (index = 0U; index < 10U; ++index)
        if (shot->hits[index].prop != NULL) ++count;
    return count;
}

static void ge_observe_guard_onscreen_gates(size_t guard_index,
                                             PropRecord *prop,
                                             ChrRecord *chr)
{
    s32 room_ids[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
    s32 *room;
    bbox2d bbox;
    const f32 radius = getinstsize(chr->model);
    int rendered = 0;

#if defined(__3DS__)
    chraiGetPropRoomIds(prop, room_ids);
    if ((int32_t)guard_index == ge_observed_guard_index) {
        size_t index;
        ge_hit_stats.observed_guard_index = ge_observed_guard_index;
        ge_hit_stats.observed_guard_samples++;
        if ((prop->flags & PROPFLAG_ONSCREEN) != 0U)
            ge_hit_stats.observed_guard_onscreen_samples++;
        ge_hit_stats.observed_prop_flags = prop->flags;
        ge_hit_stats.observed_prop_zdepth = prop->zDepth;
        for (index = 0U; index < 4U; ++index) {
            GeOriginalBgRoomVisibilitySnapshot snapshot = {0};
            ge_hit_stats.observed_prop_rooms[index] = prop->rooms[index];
            ge_hit_stats.observed_chrai_rooms[index] = room_ids[index];
            ge_hit_stats.observed_room_rendered[index] =
                room_ids[index] >= 0 && room_ids[index] <= UINT8_MAX
                    && ge_original_bg_visibility_room_snapshot(
                        (uint8_t)room_ids[index], &snapshot)
                    ? snapshot.rendered : 0U;
            if (index == 0U && room_ids[index] >= 0
                    && room_ids[index] <= UINT8_MAX) {
                ge_hit_stats.observed_shared_current_room =
                    snapshot.current_room;
                ge_hit_stats.observed_shared_rooms_drawn =
                    snapshot.rooms_drawn;
            }
        }
    }
    if (room_ids[0] < 0) return;
    ge_hit_stats.valid_room_lists++;
    for (room = room_ids; *room >= 0; ++room) {
        if (getROOMID_isRendered(*room) != 0) {
            rendered = 1;
            break;
        }
    }
    if (!rendered) return;
    ge_hit_stats.rendered_room_gate_passes++;
    if (!fogPositionIsVisibleThroughFog(&prop->pos, radius)) return;
    ge_hit_stats.fog_gate_passes++;
    if (!sub_GAME_7F054C58(&prop->pos, radius)) return;
    ge_hit_stats.near_fog_gate_passes++;
    if (getPropCombinedRoomsBBox2D(prop, &bbox) != 0
            ? camIsPosInScreenBox(&prop->pos, radius, &bbox)
            : camIsPosInScreen(&prop->pos, radius))
        ge_hit_stats.frustum_gate_passes++;
#else
    if ((int32_t)guard_index == ge_observed_guard_index) {
        size_t index;
        ge_hit_stats.observed_guard_index = ge_observed_guard_index;
        ge_hit_stats.observed_guard_samples++;
        if ((prop->flags & PROPFLAG_ONSCREEN) != 0U)
            ge_hit_stats.observed_guard_onscreen_samples++;
        ge_hit_stats.observed_prop_flags = prop->flags;
        ge_hit_stats.observed_prop_zdepth = prop->zDepth;
        for (index = 0U; index < 4U; ++index) {
            ge_hit_stats.observed_prop_rooms[index] = prop->rooms[index];
            ge_hit_stats.observed_chrai_rooms[index] = -1;
        }
    }
    if (prop->rooms[0] == UINT8_MAX) return;
    ge_hit_stats.valid_room_lists++;
    (void)room_ids;
    (void)room;
    (void)bbox;
    (void)radius;
#endif
}

void ge_original_guard_bullet_hit_reset(void)
{
    size_t index;
    for (index = 0U; index < ge_guard_count(); index++) {
        PropRecord *prop;
        ChrRecord *chr;
        if (ge_guard_actor(index, &prop, &chr)) chr->field_20 = NULL;
    }
    memset(&ge_hit_stats, 0, sizeof(ge_hit_stats));
    memset(&ge_pending_shot, 0, sizeof(ge_pending_shot));
    ge_observed_guard_index = -1;
    ge_pending_hit_index = -1;
    ge_hit_stats.last_guard_index = -1;
    ge_hit_stats.last_hitpart = HIT_NULL_PART;
    ge_hit_stats.observed_guard_index = -1;
    ge_hit_stats.observed_shared_current_room = -1;
    ge_hit_stats.observed_shared_rooms_drawn = -1;
    memset(ge_hit_stats.observed_chrai_rooms, 0xff,
           sizeof(ge_hit_stats.observed_chrai_rooms));
    ge_initialize_model_hit_free_list();
}

void ge_original_guard_bullet_hit_observe_guard(int32_t guard_index)
{
    ge_observed_guard_index = guard_index;
}

void ge_original_guard_bullet_hit_bind_stage_guards(
    void *context, GeOriginalGuardBulletHitCount count,
    GeOriginalGuardBulletHitActor actor)
{
    ge_stage_guard_context = context;
    ge_stage_guard_count = count;
    ge_stage_guard_actor = actor;
    /* Binding is an ownership hand-off, not a render-state reset.  In the
     * live path field_20 belongs to chrTick and may already describe this
     * frame's visible model. */
}

void ge_original_guard_bullet_hit_bind_players(void *context,
    GeOriginalGuardBulletHitCount count, GeOriginalGuardBulletHitPlayer player)
{
    ge_player_context = context;
    ge_player_count = count;
    ge_player_record = player;
    ge_pending_hit_index = -1;
}

GeOriginalGuardBulletHitStatus ge_original_guard_bullet_hit_test(
    const float view_origin[3], const float view_direction[3],
    const float world_direction[3], int32_t weapon, float max_distance)
{
    ShotData shot;
    size_t guard_index;
    size_t hit_index;

    if (ge_guard_count() == 0U)
        return GE_ORIGINAL_GUARD_BULLET_NO_GUARDS;
    if (view_origin == NULL || view_direction == NULL
            || world_direction == NULL || !(max_distance > 0.0f))
        return GE_ORIGINAL_GUARD_BULLET_IDLE;
    memset(&shot, 0, sizeof(shot));
    shot.viewOrigin.x = view_origin[0];
    shot.viewOrigin.y = view_origin[1];
    shot.viewOrigin.z = view_origin[2];
    shot.viewDir.x = view_direction[0];
    shot.viewDir.y = view_direction[1];
    shot.viewDir.z = view_direction[2];
    shot.dir.x = world_direction[0];
    shot.dir.y = world_direction[1];
    shot.dir.z = world_direction[2];
    shot.weapon = (ITEM_IDS)weapon;
    shot.maxdist = max_distance;
    if (ge_original_guard_bullet_hit_populate_shot(&shot)
            == GE_ORIGINAL_GUARD_BULLET_HIT_POOL_EXHAUSTED)
        return GE_ORIGINAL_GUARD_BULLET_HIT_POOL_EXHAUSTED;

    for (hit_index = 0U; hit_index < 10U; hit_index++) {
        BulletHit *hit = &shot.hits[hit_index];
        if (hit->prop == NULL) continue;
        ge_hit_stats.registered_hits++;
        ge_pending_shot = shot;
        ge_pending_hit_index = (int)hit_index;
        ge_hit_stats.last_hitpart = hit->hitpart;
        ge_hit_stats.last_distance = hit->dist;
        ge_hit_stats.last_guard_index = -1;
        for (guard_index = 0U; guard_index < ge_guard_count(); guard_index++) {
            PropRecord *prop;
            ChrRecord *chr;
            if (!ge_guard_actor(guard_index, &prop, &chr)) continue;
            if (hit->prop == prop) {
                ge_hit_stats.last_guard_index = (int32_t)guard_index;
                break;
            }
        }
        return GE_ORIGINAL_GUARD_BULLET_HIT_REGISTERED;
    }
    return GE_ORIGINAL_GUARD_BULLET_MISS;
}

GeOriginalGuardBulletHitStatus
ge_original_guard_bullet_hit_populate_shot(void *shot_record)
{
    ShotData *shot = (ShotData *)shot_record;
    size_t guard_index;
    uint32_t bounds_before = 0U;
    if (ge_guard_count() == 0U)
        return GE_ORIGINAL_GUARD_BULLET_NO_GUARDS;
    if (shot == NULL || !(shot->maxdist > 0.0f))
        return GE_ORIGINAL_GUARD_BULLET_IDLE;
    if (g_ModelHitFreeList == NULL && ge_hit_stats.pool_resets == 0U)
        ge_initialize_model_hit_free_list();
    if (ge_stage_guard_context == NULL
            && !ge_prepare_legacy_guard_hit_lists())
        return GE_ORIGINAL_GUARD_BULLET_HIT_POOL_EXHAUSTED;
    ge_hit_stats.rays_tested++;
    for (guard_index = 0U; guard_index < ge_guard_count(); guard_index++) {
        PropRecord *prop;
        ChrRecord *chr;
        s32 near_miss_before;
        uint32_t hits_before;
        if (!ge_guard_actor(guard_index, &prop, &chr)) continue;
        /* Original chrprop.c admits another player's VIEWER body, never the
         * player firing this shot. Keep original chrTestHit geometry/gates. */
        if (prop->type == PROP_TYPE_VIEWER && (g_CurrentPlayer == NULL
                || prop == g_CurrentPlayer->prop || chr->field_20 == NULL)) continue;
        ge_hit_stats.guard_candidates++;
        ge_observe_guard_onscreen_gates(guard_index, prop, chr);
        if ((prop->flags & PROPFLAG_ONSCREEN) != 0U) {
            ge_hit_stats.onscreen_gate_passes++;
            if ((prop->zDepth - getinstsize(chr->model)) < shot->maxdist)
                ge_hit_stats.depth_gate_passes++;
        }
        near_miss_before = chr->numclosearghs;
        hits_before = ge_shot_registered_hit_count(shot);
        chrTestHit(prop, shot);
        if (chr->numclosearghs != near_miss_before
                || ge_shot_registered_hit_count(shot) != hits_before) {
            bounds_before++;
            ge_hit_stats.sphere_gate_passes++;
        }
    }
    ge_hit_stats.bounding_sphere_hits += bounds_before;
    return GE_ORIGINAL_GUARD_BULLET_MISS;
}

GeOriginalGuardBulletHitStatus ge_original_guard_bullet_hit_apply_shot_hit(
    void *shot_record, int32_t hit_index)
{
    ShotData *shot = (ShotData *)shot_record;
    BulletHit *hit;
    ChrRecord *chr = NULL;
    struct player *victim;
    f32 health_before = 0.0f, armour_before = 0.0f;
    s32 dead_before = 0;
    f32 damage_before;
    u32 flags_before;
    s32 action_before;
    size_t guard_index;
    if (shot == NULL || hit_index < 0 || hit_index >= 10)
        return GE_ORIGINAL_GUARD_BULLET_DAMAGE_FRONTIER;
    hit = &shot->hits[hit_index];
    if (hit->prop == NULL)
        return GE_ORIGINAL_GUARD_BULLET_DAMAGE_FRONTIER;
    for (guard_index = 0U; guard_index < ge_guard_count(); guard_index++) {
        PropRecord *prop;
        ChrRecord *candidate;
        if (!ge_guard_actor(guard_index, &prop, &candidate)) continue;
        if (hit->prop == prop) {
            chr = candidate;
            ge_hit_stats.last_guard_index = (int32_t)guard_index;
            break;
        }
    }
    if (chr == NULL || chr->model == NULL || chr->prop != hit->prop)
        return GE_ORIGINAL_GUARD_BULLET_DAMAGE_FRONTIER;
    victim = ge_player_for_prop(hit->prop);
    if (hit->prop->type == PROP_TYPE_VIEWER) {
        if (victim == NULL || victim == g_CurrentPlayer)
            return GE_ORIGINAL_GUARD_BULLET_DAMAGE_FRONTIER;
        health_before = victim->bondhealth;
        armour_before = victim->bondarmour;
        dead_before = victim->bonddead;
    }
    ge_hit_stats.registered_hits++;
    ge_hit_stats.last_hitpart = hit->hitpart;
    ge_hit_stats.last_distance = hit->dist;
    damage_before = chr->damage;
    flags_before = chr->chrflags;
    action_before = chr->actiontype;
    ge_hit_stats.damage_attempts++;
    chrHandleBulletHit(shot, hit);
    if ((victim != NULL && (victim->bondhealth != health_before
                || victim->bondarmour != armour_before || victim->bonddead != dead_before))
            || chr->damage != damage_before || chr->chrflags != flags_before
            || chr->actiontype != action_before) {
        ge_hit_stats.damage_applied++;
        return GE_ORIGINAL_GUARD_BULLET_DAMAGE_APPLIED;
    }
    return GE_ORIGINAL_GUARD_BULLET_DAMAGE_FRONTIER;
}

GeOriginalGuardBulletHitStatus ge_original_guard_bullet_hit_apply_pending(void)
{
    BulletHit *hit;
    ChrRecord *chr = NULL;
    struct player *victim;
    f32 health_before = 0.0f, armour_before = 0.0f;
    s32 dead_before = 0;
    f32 damage_before;
    u32 flags_before;
    s32 action_before;
    size_t guard_index;
    if (ge_pending_hit_index < 0 || ge_pending_hit_index >= 10)
        return GE_ORIGINAL_GUARD_BULLET_DAMAGE_FRONTIER;
    hit = &ge_pending_shot.hits[ge_pending_hit_index];
    if (hit->prop == NULL)
        return GE_ORIGINAL_GUARD_BULLET_DAMAGE_FRONTIER;
    /* A canonical chrTick removal returns the PropRecord to the shared pool,
     * while the authored ChrRecord slot remains stable with model == NULL.
     * Resolve the pending pointer through the live authored slots before
     * dereferencing it so a delayed apply can never interpret a reused object
     * prop as the character which originally occupied that pool address. */
    for (guard_index = 0U; guard_index < ge_guard_count(); guard_index++) {
        PropRecord *prop;
        ChrRecord *candidate;
        if (!ge_guard_actor(guard_index, &prop, &candidate)) continue;
        if (hit->prop == prop) {
            chr = candidate;
            break;
        }
    }
    if (chr == NULL || chr->model == NULL || chr->prop != hit->prop)
        return GE_ORIGINAL_GUARD_BULLET_DAMAGE_FRONTIER;
    victim = ge_player_for_prop(hit->prop);
    if (hit->prop->type == PROP_TYPE_VIEWER) {
        if (victim == NULL || victim == g_CurrentPlayer)
            return GE_ORIGINAL_GUARD_BULLET_DAMAGE_FRONTIER;
        health_before = victim->bondhealth;
        armour_before = victim->bondarmour;
        dead_before = victim->bonddead;
    }
    damage_before = chr->damage;
    flags_before = chr->chrflags;
    action_before = chr->actiontype;
    ge_hit_stats.damage_attempts++;
    chrHandleBulletHit(&ge_pending_shot, hit);
    ge_pending_hit_index = -1;
    if ((victim != NULL && (victim->bondhealth != health_before
                || victim->bondarmour != armour_before || victim->bonddead != dead_before))
            || chr->damage != damage_before || chr->chrflags != flags_before
            || chr->actiontype != action_before) {
        ge_hit_stats.damage_applied++;
        return GE_ORIGINAL_GUARD_BULLET_DAMAGE_APPLIED;
    }
    return GE_ORIGINAL_GUARD_BULLET_DAMAGE_FRONTIER;
}

void ge_original_guard_bullet_hit_snapshot(
    GeOriginalGuardBulletHitStats *stats)
{
    if (stats != NULL) *stats = ge_hit_stats;
}
