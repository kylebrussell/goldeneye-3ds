#include "ge_original_pp7_fire.h"

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/bondview.h"
#include "game/gun.h"
#include "game/matrixmath.h"
#include "game/propobj.h"
#include "game/stan.h"

#include "ge_original_bond_input_internal.h"
#include "ge_original_guard_bullet_hit.h"

#include <math.h>
#include <string.h>

#define GE_GUN_SCREEN_ASPECT_RATIO (4.0f / 3.0f)
#define GE_PP7_TRACE_DISTANCE 65536.0f
#define GE_PP7_UNBOUNDED_VIEW_DEPTH 4294967296.0f

static GeOriginalPp7FireStats ge_pp7_stats;
static void *ge_pp7_object_hit_context;
static GeOriginalPp7ObjectHitReady ge_pp7_object_hit_ready;

extern u32 randomGetNext(void);
extern GunModelFileRecord gitem_structs[];
extern PropRecord *g_OnScreenPropList[];
extern PropRecord **g_LastOnScreenProp;
extern void sub_GAME_7F04E9BC(PropRecord *prop, ShotData *shotdata);
extern void objHit(ShotData *shotdata, BulletHit *hit);

static s32 ge_matrix_is_finite_nonzero(const Mtxf *matrix)
{
    s32 row;
    s32 column;
    f32 magnitude = 0.0f;

    if (matrix == NULL) return FALSE;
    for (row = 0; row < 4; row++)
    {
        for (column = 0; column < 4; column++)
        {
            if (!isfinite(matrix->m[row][column])) return FALSE;
            magnitude += fabsf(matrix->m[row][column]);
        }
    }
    return magnitude > 0.0f;
}

static void ge_transform_screen_to_direction(coord2d *in, coord3d *out,
                                               f32 length)
{
    f32 y = (g_CurrentPlayer->c_halfheight
        - (in->y - g_CurrentPlayer->c_screentop))
        * g_CurrentPlayer->c_scaley;
    f32 x = ((in->x - g_CurrentPlayer->c_screenleft)
        - g_CurrentPlayer->c_halfwidth) * g_CurrentPlayer->c_scalex;
    f32 z = -1.0f;
    f32 norm = length / sqrtf(x * x + y * y + z * z);
    out->x = x * norm;
    out->y = y * norm;
    out->z = -1.0f * norm;
}

/* Exact bullet_path_from_screen_center body used by
 * chraiDefaultWeaponFireHandler. */
static void ge_bullet_path_from_screen_center(coord3d *origin,
                                               coord3d *result,
                                               GUNHAND hand)
{
    coord2d crosspos;
    WeaponStats *stats =
        gitem_structs[g_CurrentPlayer->hands[hand].weaponnum]
            .item_weapon_stats;
    f32 inaccuracy = stats->Inaccuracy;
    f32 scaledspread;
    f32 randfactor;

    if ((stats->BitFlags & WEAPONSTATBITFLAG_FIRST_SHOT_ACCURACY)
            && g_CurrentPlayer->hands[hand].volley == 1)
        inaccuracy *= 0.25f;

    scaledspread = (120.0f * inaccuracy) / viGetFovY();
    randfactor = (RANDOMFRAC() - 0.5f) * RANDOMFRAC();
    crosspos.x = g_CurrentPlayer->crosshair_angle.x
        + randfactor * scaledspread * g_CurrentPlayer->c_screenwidth
        * GE_GUN_SCREEN_ASPECT_RATIO
        / (g_CurrentPlayer->c_perspaspect * 320.0f);
    randfactor = (RANDOMFRAC() - 0.5f) * RANDOMFRAC();
    crosspos.y = g_CurrentPlayer->crosshair_angle.y
        + randfactor * scaledspread * g_CurrentPlayer->c_screenheight
        / 240.0f;

    origin->x = origin->y = origin->z = 0.0f;
    ge_transform_screen_to_direction(&crosspos, result, 1.0f);
}

/* Exact chrlvLineLineIntersection and chrlvStanLineDirIntersection bodies.
 * Keeping them local avoids pulling unrelated actor actions into this slice. */
static void ge_line_line_intersection(coord3d *line1_p1,
                                      coord3d *line1_p2,
                                      coord3d *line2_p3,
                                      coord3d *dir,
                                      coord3d *result)
{
    f32 denom = (dir->f[2] * (line1_p2->f[0] - line1_p1->f[0]))
        - (dir->f[0] * (line1_p2->f[2] - line1_p1->f[2]));

    if (denom != 0.0f)
    {
        f32 v = (((line1_p1->f[2] - line2_p3->f[2])
                    * (line1_p2->f[0] - line1_p1->f[0]))
                + ((line2_p3->f[0] - line1_p1->f[0])
                    * (line1_p2->f[2] - line1_p1->f[2]))) / denom;
        result->f[0] = line2_p3->f[0] + dir->f[0] * v;
        result->f[1] = line2_p3->f[1] + dir->f[1] * v;
        result->f[2] = line2_p3->f[2] + dir->f[2] * v;
    }
    else if (dir->f[0] == 0.0f && dir->f[2] == 0.0f)
    {
        *result = *line2_p3;
    }
    else
    {
        *result = *line1_p1;
    }
}

static void ge_stan_line_dir_intersection(coord3d *line,
                                           coord3d *dir,
                                           coord3d *result)
{
    coord3d edge_a;
    coord3d edge_b;
    getCollisionEdge_maybe(&edge_a, &edge_b);
    ge_line_line_intersection(&edge_a, &edge_b, line, dir, result);
}

static void ge_remember_vector(float out[3], const coord3d *in)
{
    out[0] = in->x;
    out[1] = in->y;
    out[2] = in->z;
}

static GeOriginalPp7FireStatus ge_dispatch_visible_hits(
    const coord3d *view_gunpos, const coord3d *view_dir,
    const coord3d *gunpos, const coord3d *world_dir, ITEM_IDS weapon,
    f32 max_distance)
{
    ShotData shot;
    PropRecord **pp;
    s32 hit_index;
    GeOriginalPp7FireStatus result = GE_ORIGINAL_PP7_FIRE_IDLE;

    memset(&shot, 0, sizeof(shot));
    shot.viewOrigin = *view_gunpos;
    shot.viewDir = *view_dir;
    shot.weapon = weapon;
    shot.gunpos = *gunpos;
    shot.dir = *world_dir;
    shot.maxdist = max_distance;

    (void)ge_original_guard_bullet_hit_populate_shot(&shot);
    if (g_LastOnScreenProp != NULL
            && g_LastOnScreenProp >= g_OnScreenPropList) {
        for (pp = g_LastOnScreenProp; pp > g_OnScreenPropList;) {
            --pp;
            PropRecord *prop = *pp;
            if (prop == NULL || prop->obj == NULL) continue;
            if (prop->type == PROP_TYPE_OBJ
                    || prop->type == PROP_TYPE_WEAPON
                    || prop->type == PROP_TYPE_DOOR) {
                if (prop->obj->model == NULL
                        || ge_pp7_object_hit_ready == NULL
                        || !ge_pp7_object_hit_ready(
                            ge_pp7_object_hit_context, prop->obj->model))
                    continue;
                sub_GAME_7F04E9BC(prop, &shot);
            }
        }
    }

    /* This is the unchanged chraiDefaultWeaponFireHandler hit-list order.
     * chrpropAddBulletHit has already depth-sorted characters and objects into
     * this one ShotData array. */
    for (hit_index = 0; hit_index < 10; ++hit_index) {
        BulletHit *hit = &shot.hits[hit_index];
        if (hit->prop == NULL) continue;
        if (hit->prop->type == PROP_TYPE_CHR
                || hit->prop->type == PROP_TYPE_VIEWER) {
            GeOriginalGuardBulletHitStats guard_stats;
            GeOriginalGuardBulletHitStatus status =
                ge_original_guard_bullet_hit_apply_shot_hit(
                    &shot, hit_index);
            ge_original_guard_bullet_hit_snapshot(&guard_stats);
            ge_pp7_stats.guard_hits_registered++;
            ge_pp7_stats.last_guard_hitpart =
                (int8_t)guard_stats.last_hitpart;
            if (status == GE_ORIGINAL_GUARD_BULLET_DAMAGE_APPLIED) {
                ge_pp7_stats.guard_damage_applied++;
                result = GE_ORIGINAL_PP7_FIRE_GUARD_DAMAGE_APPLIED;
            } else {
                ge_pp7_stats.guard_damage_frontiers++;
                if (result == GE_ORIGINAL_PP7_FIRE_IDLE)
                    result = GE_ORIGINAL_PP7_FIRE_GUARD_HIT_REGISTERED;
            }
        } else if (hit->prop->type == PROP_TYPE_OBJ
                || hit->prop->type == PROP_TYPE_WEAPON
                || hit->prop->type == PROP_TYPE_DOOR) {
            ObjectRecord *obj = hit->prop->obj;
            f32 damage_before;
            s32 destroyed_before;
            s32 destroyed_after;
            if (obj == NULL) continue;
            damage_before = obj->maxdamage;
            destroyed_before = objGetDestroyedLevel(obj);
            ge_pp7_stats.object_hits_registered++;
            ge_pp7_stats.last_object_type =
                (int8_t)((PropDefHeaderRecord *)obj)->type;
            objHit(&shot, hit);
            destroyed_after = objGetDestroyedLevel(obj);
            ge_pp7_stats.last_object_destroyed_level =
                (uint8_t)destroyed_after;
            if (obj->maxdamage != damage_before
                    || destroyed_after != destroyed_before) {
                ge_pp7_stats.object_damage_applied++;
                if (destroyed_after > destroyed_before)
                    ge_pp7_stats.object_destroyed++;
                result = GE_ORIGINAL_PP7_FIRE_OBJECT_DAMAGE_APPLIED;
            } else if (result == GE_ORIGINAL_PP7_FIRE_IDLE) {
                result = GE_ORIGINAL_PP7_FIRE_OBJECT_HIT_REGISTERED;
            }
        }
    }
    return result;
}

void ge_original_pp7_fire_bind_object_hit_ready(
    void *context, GeOriginalPp7ObjectHitReady hit_ready)
{
    ge_pp7_object_hit_context = context;
    ge_pp7_object_hit_ready = hit_ready;
}

static GeOriginalPp7FireStatus ge_fire_hand(GUNHAND hand)
{
    struct hand *hand_state = &g_CurrentPlayer->hands[hand];
    PropRecord *player_prop = g_CurrentPlayer->prop;
    WeaponStats *weapon_stats;
    StandTile *from_tile;
    coord3d gunpos;
    coord3d dir;
    coord3d dest;
    coord3d endpoint;
    coord3d view_gunpos;
    coord3d view_dir;
    ITEM_IDS weapon = hand_state->weaponnum;

    ge_pp7_stats.hand_dispatches++;
    if (hand_state->weapon_firing_status == 0
            || (weapon != ITEM_WPPK && weapon != ITEM_WPPKSIL))
        return GE_ORIGINAL_PP7_FIRE_IDLE;

    if (player_prop == NULL) return GE_ORIGINAL_PP7_FIRE_NO_PLAYER;
    if (player_prop->stan == NULL) return GE_ORIGINAL_PP7_FIRE_NO_STAN;
    if (!ge_matrix_is_finite_nonzero(g_CurrentPlayer->viewtoworldmtxf)
            || !(g_CurrentPlayer->c_perspaspect > 0.0f)
            || !(viGetFovY() > 0.0f))
    {
        ge_pp7_stats.view_rejections++;
        return GE_ORIGINAL_PP7_FIRE_VIEW_UNAVAILABLE;
    }

    weapon_stats = gitem_structs[weapon].item_weapon_stats;
    if (weapon_stats == NULL) return GE_ORIGINAL_PP7_FIRE_VIEW_UNAVAILABLE;
    ge_pp7_stats.pp7_shots++;
    ge_pp7_stats.last_hand = hand;
    ge_pp7_stats.last_weapon = weapon;
    ge_pp7_stats.last_ammo_after_hand_tick =
        hand_state->weapon_ammo_in_magazine;
    ge_pp7_stats.last_shot_sound = weapon_stats->Sound;
    ge_pp7_stats.last_beam_pose_ready =
        isfinite(hand_state->field_B58.x)
        && isfinite(hand_state->field_B58.y)
        && isfinite(hand_state->field_B58.z)
        && (fabsf(hand_state->field_B58.x)
            + fabsf(hand_state->field_B58.y)
            + fabsf(hand_state->field_B58.z) > 0.0f);

    ge_bullet_path_from_screen_center(&gunpos, &dir, hand);
    view_gunpos = gunpos;
    view_dir = dir;
    mtx4TransformVecInPlace(g_CurrentPlayer->viewtoworldmtxf, &gunpos);
    mtx4RotateVecInPlace(g_CurrentPlayer->viewtoworldmtxf, &dir);
    dest.x = dir.x * GE_PP7_TRACE_DISTANCE + gunpos.x;
    dest.y = dir.y * GE_PP7_TRACE_DISTANCE + gunpos.y;
    dest.z = dir.z * GE_PP7_TRACE_DISTANCE + gunpos.z;
    ge_remember_vector(ge_pp7_stats.last_origin, &gunpos);
    ge_remember_vector(ge_pp7_stats.last_direction, &dir);

    from_tile = player_prop->stan;
    if (!walkTilesBetweenPoints_NoCallback(&from_tile,
            player_prop->pos.x, player_prop->pos.z, gunpos.x, gunpos.z))
        return GE_ORIGINAL_PP7_FIRE_ORIGIN_OUTSIDE_STAN;

    stanResetHits();
    if (!walkTilesBetweenPoints_NoCallback(&from_tile, gunpos.x, gunpos.z,
            dest.x, dest.z))
    {
        GeOriginalPp7FireStatus hit_status;
        coord3d delta;
        f32 ray_distance;
        f32 view_depth;
        ge_stan_line_dir_intersection(&gunpos, &dir, &endpoint);
        ge_pp7_stats.stan_hits++;
        ge_remember_vector(ge_pp7_stats.last_endpoint, &endpoint);
        delta.x = endpoint.x - gunpos.x;
        delta.y = endpoint.y - gunpos.y;
        delta.z = endpoint.z - gunpos.z;
        ray_distance = delta.x * dir.x + delta.y * dir.y + delta.z * dir.z;
        view_depth = -(view_gunpos.z + view_dir.z * ray_distance);
        hit_status = ge_dispatch_visible_hits(&view_gunpos, &view_dir,
            &gunpos, &dir, weapon, view_depth);
        if (hit_status != GE_ORIGINAL_PP7_FIRE_IDLE) return hit_status;
        return GE_ORIGINAL_PP7_FIRE_STAN_HIT;
    }

    ge_pp7_stats.clear_stan_paths++;
    ge_remember_vector(ge_pp7_stats.last_endpoint, &dest);
    {
        GeOriginalPp7FireStatus hit_status = ge_dispatch_visible_hits(
            &view_gunpos, &view_dir, &gunpos, &dir, weapon,
            GE_PP7_UNBOUNDED_VIEW_DEPTH);
        if (hit_status != GE_ORIGINAL_PP7_FIRE_IDLE) return hit_status;
    }
    return GE_ORIGINAL_PP7_FIRE_BACKGROUND_PROP_FRONTIER;
}

void ge_original_pp7_fire_reset(void)
{
    memset(&ge_pp7_stats, 0, sizeof(ge_pp7_stats));
    ge_pp7_stats.last_hand = -1;
    ge_pp7_stats.last_weapon = ITEM_UNARMED;
    ge_pp7_stats.last_ammo_after_hand_tick = -1;
    ge_pp7_stats.last_guard_hitpart = 0;
    ge_original_guard_bullet_hit_reset();
}

GeOriginalPp7FireStatus ge_original_pp7_fire_tick(void)
{
    GeOriginalPp7FireStatus right;
    GeOriginalPp7FireStatus left;

    if (g_CurrentPlayer == NULL) return GE_ORIGINAL_PP7_FIRE_NO_PLAYER;
    ge_pp7_stats.both_hands_ticks++;
    right = ge_fire_hand(GUNRIGHT);
    left = ge_fire_hand(GUNLEFT);
    return right != GE_ORIGINAL_PP7_FIRE_IDLE ? right : left;
}

void ge_original_pp7_fire_snapshot(GeOriginalPp7FireStats *stats)
{
    if (stats != NULL) *stats = ge_pp7_stats;
}
