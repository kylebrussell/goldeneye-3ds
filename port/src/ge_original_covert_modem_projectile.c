#include "ge_original_covert_modem_projectile.h"

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/bg.h"
#include "game/bondview.h"
#include "game/matrixmath.h"
#include "game/stan.h"
#include "snd.h"

#include "ge_original_default_object_internal.h"
#undef modelLoad
#undef getPlayerCount
#undef get_scenario
#include "ge_original_bond_input_internal.h"

#include <limits.h>
#include <math.h>
#include <string.h>

#define GE_ORIGINAL_PROJECTILE_CAPACITY 20U
#define GE_ORIGINAL_THROW_REFRESH_RATE 60

/* Canonical chrprop.c storage. The bounded port does not compile the full
 * object module, so this module owns the exact 20-record projectile pool. */
Projectile g_Projectiles[GE_ORIGINAL_PROJECTILE_CAPACITY];

static GeOriginalCovertModemProjectileStats ge_projectile_stats;

extern s32 D_80048380;
extern ALBank *g_musicSfxBufferPtr;
extern bg_portal_data_entry *g_BgPortals;

static void ge_projectile_reset(Projectile *projectile)
{
    /* Exact projectileReset body from propobj.c. */
    projectile->flags = 0;
    projectile->speed.x = 0.0f;
    projectile->speed.y = 0.0f;
    projectile->speed.z = 0.0f;
    projectile->unk10.x = 0.0f;
    projectile->unk10.y = 0.0f;
    projectile->unk10.z = 0.0f;
    projectile->unk1C = 0.0f;
    projectile->unk60 = 1.0f;
    projectile->ownerprop = NULL;
    projectile->unk8C = 0.05f;
    projectile->unk90 = 0;
    projectile->unk94 = 0.0f;
    projectile->lastSfxTimer = (u32)-1;
    projectile->soundSlot = 0;
    projectile->unkA8 = 0;
    projectile->unkAC = (u32)-1;
    projectile->droptype = DROPTYPE_DEFAULT;
    projectile->refreshrate = 0;
    projectile->unkC0 = 1.0f;
    projectile->unkC4 = 1.0f;
    projectile->unkC8 = 1.0f;
    projectile->age = 0;
    projectile->obj = NULL;
    projectile->unkE8 = 0;
}

static Projectile *ge_projectile_allocate_fresh(void)
{
    size_t index;
    /* Exact happy path of projectileAllocate. Eviction remains outside this
     * tranche because it requires the complete objFree lifecycle. */
    for (index = 0U; index < GE_ORIGINAL_PROJECTILE_CAPACITY; index++) {
        if (g_Projectiles[index].flags & PROJECTILEFLAG_FREE) {
            ge_projectile_reset(&g_Projectiles[index]);
            ge_projectile_stats.pool_allocations++;
            return &g_Projectiles[index];
        }
    }
    ge_projectile_stats.pool_exhaustions++;
    return NULL;
}

static s32 ge_player_pointer_index(PropRecord *prop)
{
    s32 index;
    for (index = 0; index < getPlayerCount(); index++) {
        if (g_playerPointers[index]->prop == prop) return index;
    }
    return -1;
}

static void ge_chr_set_moving(ChrRecord *chr, bool unset)
{
    /* Exact chrSetMoving body. */
    if (unset) chr->hidden &= ~CHRHIDDEN_MOVING;
    else chr->hidden |= CHRHIDDEN_MOVING;
}

static void ge_tank_set_moving(PropRecord *prop, s32 flag)
{
    /* Exact sub_GAME_7F04F218 body. */
    ChrRecord *chr = prop->chr;
    if (flag != 0) chr->accuracyrating = (u8)chr->accuracyrating & ~0x20;
    else chr->accuracyrating = (u8)chr->accuracyrating | 0x20;
}

static s32 ge_update_guard_tank_flags(PropRecord *prop, s32 flag)
{
    s32 player_index = ge_player_pointer_index(prop);
    if (player_index < 0 || g_playerPointers[player_index] == NULL) return 0;
    if (prop->chr != NULL) ge_chr_set_moving(prop->chr, flag != 0);
    if (g_PlayerTankProp != NULL) ge_tank_set_moving(g_PlayerTankProp, flag);
    g_playerPointers[player_index]->field_AC = flag;
    return 1;
}

static coord3d *ge_current_player_position3(void)
{
    if (g_CurrentPlayer->cameramode == 1) return &g_CurrentPlayer->pos3;
    return &g_CurrentPlayer->field_488.pos3;
}

static u8 ge_current_player_room(void)
{
    if (g_CurrentPlayer->cameramode == 1 && g_CurrentPlayer->cameratile != NULL)
        return g_CurrentPlayer->cameratile->room;
    if (g_CurrentPlayer->field_488.current_tile_ptr_for_portals != NULL)
        return g_CurrentPlayer->field_488.current_tile_ptr_for_portals->room;
    return g_CurrentPlayer->prop->stan != NULL
        ? g_CurrentPlayer->prop->stan->room : (u8)-1;
}

extern s32 sub_GAME_7F0B9F14(
    s32 portalnum, coord3d *pos1, coord3d *pos2);

static void ge_add_to_byte_set(u8 *set, u8 element)
{
    s32 index = 0;
    /* Exact addToByteSetMaxSize15 body. */
    while (index < 0x10 && set[index] != 0xff) {
        if (element == set[index]) return;
        index++;
    }
    if (index < 0xf) {
        set[index] = element;
        set[index + 1] = (u8)-1;
    }
}

static s32 ge_find_rooms_along_segment(
    coord3d *from, coord3d *to, u8 *from_rooms, u8 *final_rooms,
    s32 *traversed_rooms, s32 *traversed_room_count,
    s32 max_traversed_rooms)
{
    u8 source_rooms[16];
    u8 found_rooms[16];
    u8 all_rooms[16];
    u8 portal_statuses[PORTMAX];
    s32 portal;
    s32 index;
    s32 count;

    /* Exact bgFindRoomsAlongSegment traversal with deterministic initialization
     * for the unused half of the N64 caller's short initial-room array. */
    memset(source_rooms, 0xff, sizeof(source_rooms));
    memset(found_rooms, 0xff, sizeof(found_rooms));
    memset(all_rooms, 0xff, sizeof(all_rooms));
    memset(portal_statuses, 0, sizeof(portal_statuses));
    if (g_BgPortals == NULL) return 0;
    for (portal = 0; portal < PORTMAX
            && g_BgPortals[portal].offset_portal != NULL; portal++)
        portal_statuses[portal] = (u8)sub_GAME_7F0B9F14(portal, from, to);
    for (count = 0; count < 8; count++) {
        source_rooms[count] = from_rooms[count];
        all_rooms[count] = from_rooms[count];
    }
    do {
        found_rooms[0] = (u8)-1;
        for (portal = 0; portal < PORTMAX
                && g_BgPortals[portal].offset_portal != NULL; portal++) {
            for (index = 0; index < 16
                    && source_rooms[index] != (u8)-1; index++) {
                if (portal_statuses[portal] == 1
                        && g_BgPortals[portal].connectedRoom1
                            == source_rooms[index]) {
                    ge_add_to_byte_set(found_rooms,
                        g_BgPortals[portal].connectedRoom2);
                    ge_add_to_byte_set(all_rooms,
                        g_BgPortals[portal].connectedRoom2);
                    portal_statuses[portal] = 0;
                }
                if (portal_statuses[portal] == 2
                        && g_BgPortals[portal].connectedRoom2
                            == source_rooms[index]) {
                    ge_add_to_byte_set(found_rooms,
                        g_BgPortals[portal].connectedRoom1);
                    ge_add_to_byte_set(all_rooms,
                        g_BgPortals[portal].connectedRoom1);
                    portal_statuses[portal] = 0;
                }
            }
        }
        if (found_rooms[0] == (u8)-1) break;
        memcpy(source_rooms, found_rooms, sizeof(source_rooms));
    } while (source_rooms[0] != (u8)-1);
    for (count = 0; count < 7 && source_rooms[count] != (u8)-1; count++)
        final_rooms[count] = source_rooms[count];
    final_rooms[count] = (u8)-1;
    for (count = 0; count < max_traversed_rooms
            && all_rooms[count] != (u8)-1; count++)
        traversed_rooms[count] = all_rooms[count];
    *traversed_room_count = count;
    return 1;
}

static s32 ge_sound_volume_for_distance(f32 distance, f32 low, f32 high)
{
    /* Exact sub_GAME_7F0537B8 curve. */
    if (distance <= 200.0f) return SHRT_MAX;
    if (high <= distance) return 0;
    if (low <= distance)
        return (s32)(((high - distance) * 10000.0f) / (high - low));
    return SHRT_MAX - (s32)((sqrtf(distance - 200.0f) * 22767.0f)
        / sqrtf(low - 200.0f));
}

static void ge_spatialize_sound(ALSoundState *state, coord3d *position)
{
    f32 shortest = 6000.0f;
    s32 index;
    for (index = 0; index < getPlayerCount(); index++) {
        PropRecord *prop = g_playerPointers[index]->prop;
        f32 dx = prop->pos.x - position->x;
        f32 dy = prop->pos.y - position->y;
        f32 dz = prop->pos.z - position->z;
        f32 distance = sqrtf(dx * dx + dy * dy + dz * dz);
        if (distance < shortest) shortest = distance;
    }
    sndCreatePostEvent(state, AL_SNDP_VOL_EVT,
        ge_sound_volume_for_distance(shortest, 5000.0f, 6000.0f));
}

static Projectile *ge_attach_projectile(ObjectRecord *object,
                                        PropRecord *owner)
{
    Projectile *projectile;
    object->projectile = ge_projectile_allocate_fresh();
    projectile = object->projectile;
    if (projectile == NULL) return NULL;
    /* bondconstants.h exposes the canonical bit-7 literal in portable builds;
     * the auto-generated RUNTIMEBITFLAG_HASPROJECTILE name only exists under
     * the original SGI preprocessor. */
    object->runtime_bitflags |= RUNTIMEBITFLAG_00000080;
    projectile->flags |= 0x41;
    projectile->ownerprop = owner;
    projectile->flags |= PROJECTILEFLAG_STICKY;
    if (object->prop->stan != NULL) {
        projectile->unkCC[0] = object->prop->stan->room;
        projectile->unkCC[1] = (u8)-1;
    } else {
        projectile->unkCC[0] = (u8)-1;
    }
    return projectile;
}

void ge_original_covert_modem_projectile_reset(void)
{
    size_t index;
    memset(&ge_projectile_stats, 0, sizeof(ge_projectile_stats));
    memset(g_Projectiles, 0, sizeof(g_Projectiles));
    /* Exact relevant initobjects pool initialization. */
    for (index = 0U; index < GE_ORIGINAL_PROJECTILE_CAPACITY; index++) {
        g_Projectiles[index].flags = PROJECTILEFLAG_FREE;
        g_Projectiles[index].sounds[0] = NULL;
        g_Projectiles[index].sounds[1] = NULL;
    }
}

GeOriginalCovertModemProjectileStatus
ge_original_covert_modem_projectile_init_from_player(
    void *opaque_object, const void *opaque_target, void *opaque_launch_matrix,
    const void *opaque_velocity, const void *opaque_orientation_matrix)
{
    ObjectRecord *object = opaque_object;
    const coord3d *target = opaque_target;
    Mtxf *launch_matrix = opaque_launch_matrix;
    const coord3d *velocity = opaque_velocity;
    const Mtxf *orientation_matrix = opaque_orientation_matrix;
    PropRecord *player_prop;
    Projectile *projectile;
    coord3d position;
    StandTile *tile;
    f32 high;
    f32 low;
    s32 used_fallback = 0;
    u8 rooms[8];
    s32 traversed[0x14];
    s32 traversed_count = 0;

    ge_projectile_stats.launch_calls++;
    if (object == NULL || object->prop == NULL || object->model == NULL
            || target == NULL || launch_matrix == NULL || velocity == NULL
            || orientation_matrix == NULL)
        return GE_ORIGINAL_COVERT_MODEM_PROJECTILE_INVALID_ARGUMENT;
    player_prop = g_CurrentPlayer != NULL ? g_CurrentPlayer->prop : NULL;
    if (player_prop == NULL || player_prop->stan == NULL
            || ge_player_pointer_index(player_prop) < 0)
        return GE_ORIGINAL_COVERT_MODEM_PROJECTILE_NO_PLAYER;
    if (g_BgPortals == NULL)
        return GE_ORIGINAL_COVERT_MODEM_PROJECTILE_ROOM_ABI_UNAVAILABLE;

    if (target->y < player_prop->pos.y) {
        high = player_prop->pos.y - g_CurrentPlayer->stanHeight;
        low = target->y - g_CurrentPlayer->stanHeight;
    } else {
        high = target->y - g_CurrentPlayer->stanHeight;
        low = player_prop->pos.y - g_CurrentPlayer->stanHeight;
    }
    tile = player_prop->stan;
    if (!ge_update_guard_tank_flags(player_prop, 0))
        return GE_ORIGINAL_COVERT_MODEM_PROJECTILE_NO_PLAYER;
    if (stanTestLineUnobstructed(&tile, player_prop->pos.x,
            player_prop->pos.z, target->x, target->z, 0x1f,
            high, low, 0.0f, 1.0f)) {
        position = *target;
    } else {
        tile = player_prop->stan;
        position = player_prop->pos;
        used_fallback = 1;
    }
    (void)ge_update_guard_tank_flags(player_prop, 1);

    chrpropActivate(object->prop);
    chrpropEnable(object->prop);
    matrix_scalar_multiply(object->model->scale, launch_matrix->m[0]);
    if (!ge_original_obj_change_shading_slice(
            object, &position, launch_matrix, tile))
        return GE_ORIGINAL_COVERT_MODEM_PROJECTILE_SHADING_UNAVAILABLE;
    ge_original_setup_update_object_room_position_slice(object);
    chrobjCollisionRelated(object);
    projectile = ge_attach_projectile(object, player_prop);
    if (projectile == NULL)
        return GE_ORIGINAL_COVERT_MODEM_PROJECTILE_POOL_EXHAUSTED;
    matrix_4x4_copy((Mtxf *)orientation_matrix, &projectile->mtx);
    projectile->speed = *velocity;
    projectile->obj = object;
    projectile->unkE8 = D_80048380;
    if (used_fallback) {
        projectile->flags |= PROJECTILEFLAG_00000100;
        ((coord3d *)&projectile->unkd4)->x = target->x;
        ((coord3d *)&projectile->unkd4)->y = target->y;
        ((coord3d *)&projectile->unkd4)->z = target->z;
        ge_projectile_stats.fallback_launches++;
    }
    memset(rooms, 0xff, sizeof(rooms));
    rooms[0] = ge_current_player_room();
    if (!ge_find_rooms_along_segment(
            ge_current_player_position3(), &position, rooms,
            projectile->unkCC, traversed, &traversed_count, 0x14))
        return GE_ORIGINAL_COVERT_MODEM_PROJECTILE_ROOM_ABI_UNAVAILABLE;

    return GE_ORIGINAL_COVERT_MODEM_PROJECTILE_OK;
}

void ge_original_gun_init_projectile_from_player_exact(
    void *object, void *target, void *launch_matrix,
    void *velocity, void *orientation_matrix)
{
    (void)ge_original_covert_modem_projectile_init_from_player(
        object, target, launch_matrix, velocity, orientation_matrix);
}

GeOriginalCovertModemProjectileStatus
ge_original_covert_modem_projectile_launch(
    void *opaque_object, const void *opaque_target, void *opaque_launch_matrix,
    const void *opaque_velocity, const void *opaque_orientation_matrix)
{
    ObjectRecord *object = opaque_object;
    GeOriginalCovertModemProjectileStatus status;
    Projectile *projectile;
    ALSoundState *sound;

    status = ge_original_covert_modem_projectile_init_from_player(
        opaque_object, opaque_target, opaque_launch_matrix,
        opaque_velocity, opaque_orientation_matrix);
    if (status != GE_ORIGINAL_COVERT_MODEM_PROJECTILE_OK) return status;
    projectile = object->projectile;

    /* Exact generate_player_thrown_object ITEM_BUG projectile tail. */
    projectile->flags |= 2;
    projectile->unk8C = 0.1f;
    projectile->refreshrate = GE_ORIGINAL_THROW_REFRESH_RATE;
    sound = sndPlaySfx((struct ALBankAlt_s *)g_musicSfxBufferPtr,
                       GRENADE_THROW_SFX, NULL);
    if (sound != NULL) {
        ge_spatialize_sound(sound, &object->runtime_pos);
        ge_projectile_stats.sound_events++;
    }
    ge_projectile_stats.successful_launches++;
    return GE_ORIGINAL_COVERT_MODEM_PROJECTILE_OK;
}

void ge_original_covert_modem_projectile_snapshot(
    GeOriginalCovertModemProjectileStats *stats)
{
    if (stats != NULL) *stats = ge_projectile_stats;
}
