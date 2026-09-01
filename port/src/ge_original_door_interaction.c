#include "ge_original_door_interaction.h"

#include <math.h>
#include <string.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/player.h"
#include "ge_original_door_runtime.h"
#include "ge_original_door_runtime_internal.h"
#include "ge_original_dam_world.h"
#include "ge_original_player_spawn_internal.h"

#define GE_DOOR_INTERACTION_VISIBLE_CAPACITY 64U
/* ACTIVATED is entry 14 of the canonical RUNTIMEBITFLAG declaration. */
#define GE_DOOR_INTERACTION_RUNTIME_ACTIVATED (UINT32_C(1) << 14U)

extern stagesetup g_CurrentSetup;
extern PropRecord *g_OnScreenPropList[];
extern PropRecord **g_LastOnScreenProp;

static GeOriginalDoorInteractionProviders interaction_providers;
static GeOriginalDoorInteractionState *interaction_state;
static PropRecord *visible_doors[GE_DOOR_INTERACTION_VISIBLE_CAPACITY];
static size_t visible_door_count;
static PropRecord *interact_prop;

static DoorRecord *native_door_for_prop(PropRecord *prop)
{
    if (prop == NULL || prop->type != PROP_TYPE_DOOR || prop->obj == NULL)
        return NULL;
    return ge_port_door_runtime_native_definition(prop->obj);
}

static ObjectRecord *object_for_door(DoorRecord *door)
{
    return ge_port_door_runtime_object(door);
}

/* Exact body of bondinvCheckHasKeyFlags, kept local to avoid pulling the
 * unrelated inventory UI into the interaction boundary. */
static bool player_has_key_flags(struct player *player, u32 wantkeyflags)
{
    u32 heldkeyflags = 0;
    InvItem *item = player->ptr_inventory_first_in_cycle;

    while (item)
    {
        if (item->type == INV_ITEM_PROP)
        {
            PropRecord *prop = item->type_inv_item.type_prop.prop;

            if (prop->type == PROP_TYPE_OBJ)
            {
                ObjectRecord *obj = prop->obj;

                uint8_t object_type = 0;
                if (ge_dam_setup_world_definition_header(
                        obj, NULL, NULL, &object_type)
                        && object_type == PROPDEF_KEY)
                {
                    KeyRecord *key = (KeyRecord *)prop->obj;

                    heldkeyflags |= key->keyflags;

                    if ((wantkeyflags & heldkeyflags) == wantkeyflags)
                    {
                        return TRUE;
                    }
                }
            }
        }

        item = item->next;
    }

    return FALSE;
}

/* Exact chrprop room-list semantics used by doorTestForInteract. */
static void prop_room_ids(PropRecord *prop, s32 roomids[32])
{
    s32 i;
    if (prop->stan == NULL) {
        roomids[0] = -1;
    } else if (prop->type == PROP_TYPE_VIEWER && prop->obj == NULL) {
        roomids[0] = prop->stan->room;
        roomids[1] = -1;
    } else {
        for (i = 0; i < 31 && prop->rooms[i] != 0xff; i++)
            roomids[i] = prop->rooms[i];
        roomids[i] = -1;
    }
}

static bool room_lists_overlap(s32 *rooms1, s32 *rooms2)
{
    s32 *first;
    s32 *second;
    for (first = rooms1; *first >= 0; first++)
        for (second = rooms2; *second >= 0; second++)
            if (*first == *second) return TRUE;
    return FALSE;
}

/* Exact body of chrpropTestPointInPaddedBoundPad. */
static bool point_in_padded_bound_pad(
    coord3d *pos, f32 radius, BoundPadRecord *pad)
{
    f32 dx = pos->x - pad->pos.x;
    f32 dy = pos->y - pad->pos.y;
    f32 dz = pos->z - pad->pos.z;
    f32 side[3];
    f32 d;

    side[0] = (pad->up.y * pad->look.z) - (pad->look.y * pad->up.z);
    side[1] = (pad->up.z * pad->look.x) - (pad->look.z * pad->up.x);
    side[2] = (pad->up.x * pad->look.y) - (pad->look.x * pad->up.y);
    d = pad->look.z * dz + (dx * pad->look.x + dy * pad->look.y);
    if (pad->bbox.zmax + radius < d || d < pad->bbox.zmin - radius)
        return FALSE;
    d = pad->up.z * dz + (dx * pad->up.x + dy * pad->up.y);
    if (pad->bbox.ymax + radius < d || d < pad->bbox.ymin - radius)
        return FALSE;
    d = dx * side[0] + dy * side[1] + side[2] * dz;
    if (pad->bbox.xmax + radius < d || d < pad->bbox.xmin - radius)
        return FALSE;
    return TRUE;
}

/* Exact door7F05522C angle construction, with the committed player passed
 * explicitly across the native boundary. */
static void door_angle_extents(DoorRecord *door, struct player *player,
                               f32 *arg1, f32 *arg2, s32 altcoordsystem)
{
    ObjectRecord *object = object_for_door(door);
    BoundPadRecord *pad = &g_CurrentSetup.boundpads[object->pad];
    PropRecord *playerprop = player->prop;
    coord3d normal;
    coord3d playerpos;
    f32 xmin;
    f32 xmax;
    f32 angle2;
    f32 cosine;
    f32 sine;
    f32 angle;
    f32 y1;
    f32 x1;
    f32 playerangle;
    f32 anglediff;
    f32 anglediff2;
    f32 scale = 1.0f;
    f32 xbound;

    playerpos.x = player->field_488.theta_transform.x * 30.0f
        * scale * 0.75f + playerprop->pos.x;
    playerpos.y = playerprop->pos.y;
    playerpos.z = player->field_488.theta_transform.z * 30.0f
        * scale * 0.75f + playerprop->pos.z;
    if (altcoordsystem != 0) {
        xmin = pad->bbox.xmin;
        xmax = pad->bbox.xmax;
        normal.x = pad->up.y * pad->look.z - pad->look.y * pad->up.z;
        normal.y = pad->up.z * pad->look.x - pad->look.z * pad->up.x;
        normal.z = pad->up.x * pad->look.y - pad->look.x * pad->up.y;
    } else {
        xmin = pad->bbox.ymin;
        xmax = pad->bbox.ymax;
        normal = pad->up;
    }
    x1 = pad->pos.x + normal.x * xmin - playerpos.x;
    y1 = pad->pos.z + normal.z * xmin - playerpos.z;
    angle = atan2f(x1, y1);
    playerangle = (360.0f - player->vv_theta) * (M_TAU_F / 360.0f);
    anglediff = angle - playerangle;
    scale = angle - playerangle + M_TAU_F;
    if (angle < playerangle) anglediff = scale;
    if (anglediff > M_PI_F) anglediff -= M_TAU_F;

    if (door->doorType == DOORTYPE_SWINGING) {
        angle2 = door->openPosition * M_TAU_F / 360.0f;
        if (object->flags & PROPFLAG_DOOR_OPENTOFRONT)
            angle2 = M_TAU_F - angle2;
        cosine = cosf(angle2);
        sine = sinf(angle2);
        xbound = xmax - xmin;
        x1 = pad->pos.x + normal.x * xmin
            + xbound * (normal.x * cosine + normal.z * sine) - playerpos.x;
        y1 = pad->pos.z + normal.z * xmin
            + xbound * (-normal.x * sine + normal.z * cosine) - playerpos.z;
    } else {
        x1 = pad->pos.x + normal.x * xmax - playerpos.x;
        y1 = pad->pos.z + normal.z * xmax - playerpos.z;
    }
    angle = atan2f(x1, y1);
    playerangle = (360.0f - player->vv_theta) * (M_TAU_F / 360.0f);
    anglediff2 = angle - playerangle;
    if (angle < playerangle) anglediff2 += M_TAU_F;
    if (anglediff2 > M_PI_F) anglediff2 -= M_TAU_F;
    if (anglediff < anglediff2) {
        *arg1 = anglediff;
        *arg2 = anglediff2;
    } else {
        *arg1 = anglediff2;
        *arg2 = anglediff;
    }
}

static bool door_angle_test(DoorRecord *door, struct player *player,
                            bool altcoordsystem)
{
    bool checkmore = TRUE;
    f32 low;
    f32 high;
    DoorRecord *sibling;
    const f32 limit = 0.34906587f;

    if (interact_prop == NULL) {
        door_angle_extents(door, player, &low, &high, altcoordsystem);
        if (low >= -limit && low <= limit && high >= -limit && high <= limit) {
            interact_prop = object_for_door(door)->prop;
            checkmore = FALSE;
        } else {
            sibling = door->linkedDoor;
            while (sibling != NULL && sibling != door
                    && (low >= 0.0f || high < 0.0f)) {
                f32 sibling_low;
                f32 sibling_high;
                door_angle_extents(sibling, player, &sibling_low,
                                   &sibling_high, altcoordsystem);
                if (low > 0.0f && sibling_low < low) low = sibling_low;
                if (high < 0.0f && high < sibling_high) high = sibling_high;
                sibling = sibling->linkedDoor;
            }
            if (high - low < M_PI_F && low < 0.0f && high > 0.0f) {
                interact_prop = object_for_door(door)->prop;
                checkmore = FALSE;
            }
        }
    }
    return checkmore;
}

static bool door_test_for_interact(PropRecord *prop, struct player *player)
{
    DoorRecord *door = native_door_for_prop(prop);
    ObjectRecord *object;
    bool checkmore = TRUE;
    bool maybe = FALSE;
    f32 xdiff;
    f32 ydiff;
    f32 zdiff;

    if (interaction_state != NULL) interaction_state->interaction_tests++;
    if (door == NULL || player == NULL || player->prop == NULL
            || g_CurrentSetup.boundpads == NULL) return TRUE;
    object = object_for_door(door);
    if (object == NULL) return TRUE;
    if ((object->flags & PROPFLAG_CANNOT_ACTIVATE) == 0
            && door->maxFrac > 0.0f && (prop->flags & PROPFLAG_ONSCREEN)) {
        xdiff = object->runtime_pos.x - player->prop->pos.x;
        ydiff = object->runtime_pos.y - player->prop->pos.y;
        zdiff = object->runtime_pos.z - player->prop->pos.z;
        if (xdiff * xdiff + zdiff * zdiff < 40000.0f
                && ydiff < 200.0f && ydiff > -200.0f) {
            maybe = TRUE;
        } else {
            s32 rooms1[32];
            s32 rooms2[32];
            prop_room_ids(prop, rooms1);
            prop_room_ids(player->prop, rooms2);
            if (room_lists_overlap(rooms1, rooms2)
                    && point_in_padded_bound_pad(
                        &player->prop->pos, 150.0f,
                        &g_CurrentSetup.boundpads[object->pad])) maybe = TRUE;
        }
        if (maybe) {
            checkmore = door_angle_test(door, player, FALSE);
            if (checkmore && (object->flags2
                    & PROPFLAG2_DOOR_ALTCOORDSYSTEM))
                checkmore = door_angle_test(door, player, TRUE);
        }
    }
    if (!checkmore && interaction_state != NULL)
        interaction_state->interaction_hits++;
    return checkmore;
}

static bool position_is_in_front(PropRecord *prop, DoorRecord *door)
{
    ObjectRecord *object = object_for_door(door);
    BoundPadRecord *pad = &g_CurrentSetup.boundpads[object->pad];
    coord3d normal;
    coord3d diff;
    f32 side;
    normal.x = pad->up.y * pad->look.z - pad->look.y * pad->up.z;
    normal.y = pad->up.z * pad->look.x - pad->look.z * pad->up.x;
    normal.z = pad->up.x * pad->look.y - pad->look.x * pad->up.y;
    diff.x = prop->pos.x - pad->pos.x;
    diff.y = prop->pos.y - pad->pos.y;
    diff.z = prop->pos.z - pad->pos.z;
    side = diff.x * normal.x + diff.y * normal.y + diff.z * normal.z;
    if (door->doorFlags & DOORFLAG_FLIP) side = -side;
    return side >= 0.0f;
}

static void choose_swing_direction(PropRecord *playerprop, DoorRecord *door)
{
    ObjectRecord *object = object_for_door(door);
    if ((object->flags & PROPFLAG_DOOR_TWOWAY)
            && door->openstate == PROPSTATE_NONE
            && door->openPosition == 0.0f) {
        bool infront = position_is_in_front(playerprop, door);
        u32 wantflag = 0;
        DoorRecord *sibling;
        if ((door->doorFlags & DOORFLAG_FLIP) == 0) {
            if (!infront) wantflag = PROPFLAG_DOOR_OPENTOFRONT;
        } else if (infront) {
            wantflag = PROPFLAG_DOOR_OPENTOFRONT;
        }
        if ((s32)((object->flags ^ wantflag) << 2) < 0) {
            sibling = door;
            do {
                object_for_door(sibling)->flags ^= PROPFLAG_DOOR_OPENTOFRONT;
                sibling = sibling->linkedDoor;
            } while (sibling && sibling != door);
        }
    }
}

static GeOriginalDoorInteractionResult activate_wrapper(
    PropRecord *prop, DoorRecord *door)
{
    s32 state = -1;
    if (door->openstate == DOORSTATE_OPENING
            || door->openstate == DOORSTATE_WAITING)
        state = DOORSTATE_CLOSING;
    else if (door->openstate == DOORSTATE_CLOSING)
        state = DOORSTATE_OPENING;
    else if (door->openstate == DOORSTATE_STATIONARY)
        state = door->openPosition > 0.5f
            ? DOORSTATE_CLOSING : DOORSTATE_OPENING;
    if (state >= 0 && ge_original_door_runtime_activate(prop->obj, state)
            != GE_ORIGINAL_DOOR_RUNTIME_OK)
        return GE_ORIGINAL_DOOR_INTERACTION_INVALID_STATE;
    object_for_door(door)->runtime_bitflags
        |= GE_DOOR_INTERACTION_RUNTIME_ACTIVATED;
    object_for_door(door)->flags2 &= ~PROPFLAG2_00000008;
    if (object_for_door(door)->runtime_bitflags
            & RUNTIMEBITFLAG_00000001) {
        if (interaction_providers.activate_linked_switches == NULL)
            return GE_ORIGINAL_DOOR_INTERACTION_MISSING_SWITCH_PROVIDER;
        interaction_providers.activate_linked_switches(
            interaction_providers.context, prop);
    }
    return GE_ORIGINAL_DOOR_INTERACTION_ACTIVATED;
}

static GeOriginalDoorInteractionResult interact_door(
    PropRecord *doorprop, struct player *player)
{
    DoorRecord *door = native_door_for_prop(doorprop);
    ObjectRecord *object;
    s32 can_open = 0;
    if (door == NULL || player == NULL || player->prop == NULL)
        return GE_ORIGINAL_DOOR_INTERACTION_INVALID_STATE;
    object = object_for_door(door);
    if (object == NULL) return GE_ORIGINAL_DOOR_INTERACTION_INVALID_STATE;
    if (door->keyflags == 0 || player_has_key_flags(player, door->keyflags)) {
        can_open = 1;
    } else if (position_is_in_front(player->prop, door)) {
        if ((object->flags2 & PROPFLAG2_10000000)
                && !(object->flags2 & PROPFLAG2_08000000)) can_open = 1;
    } else if (!(object->flags2 & PROPFLAG2_10000000)
            && (object->flags2 & PROPFLAG2_08000000)) {
        can_open = 1;
    }
    if (object->runtime_bitflags & RUNTIMEBITFLAG_PADLOCKEDDOOR) {
        if (interaction_providers.padlock_free == NULL)
            return GE_ORIGINAL_DOOR_INTERACTION_MISSING_PADLOCK_PROVIDER;
        if (!interaction_providers.padlock_free(
                interaction_providers.context, doorprop->obj)) can_open = 0;
    }
    if (can_open) {
        choose_swing_direction(player->prop, door);
        return activate_wrapper(doorprop, door);
    }
    if (door->openstate == DOORSTATE_STATIONARY
            && door->openPosition < 0.5f) {
        if (!(object->flags2 & PROPFLAG2_00000004)
                && interaction_providers.show_locked_message != NULL)
            interaction_providers.show_locked_message(
                interaction_providers.context, doorprop->obj);
        object->runtime_bitflags |= GE_DOOR_INTERACTION_RUNTIME_ACTIVATED;
        object->flags2 |= PROPFLAG2_00000008;
    }
    return GE_ORIGINAL_DOOR_INTERACTION_LOCKED;
}

void ge_original_door_interaction_bind(
    const GeOriginalDoorInteractionProviders *providers,
    GeOriginalDoorInteractionState *state)
{
    memset(&interaction_providers, 0, sizeof(interaction_providers));
    if (providers != NULL) interaction_providers = *providers;
    interaction_state = state;
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
        state->result = GE_ORIGINAL_DOOR_INTERACTION_IDLE;
    }
}

int ge_original_door_interaction_bind_visible_doors(
    void *const *door_props, size_t count)
{
    size_t index;
    for (index = 0; index < visible_door_count; index++)
        visible_doors[index]->flags &= (u8)~PROPFLAG_ONSCREEN;
    visible_door_count = 0;
    memset(visible_doors, 0, sizeof(visible_doors));
    if (count > GE_DOOR_INTERACTION_VISIBLE_CAPACITY) return 0;
    for (index = 0; index < count; index++) {
        PropRecord *prop = door_props != NULL ? door_props[index] : NULL;
        if (native_door_for_prop(prop) == NULL) return 0;
        prop->flags |= PROPFLAG_ONSCREEN;
        visible_doors[visible_door_count++] = prop;
    }
    return 1;
}

int ge_original_door_interaction_bind_onscreen_doors(void)
{
    PropRecord **cursor;
    size_t count;
    if (g_LastOnScreenProp == NULL
            || g_LastOnScreenProp < g_OnScreenPropList) return 0;
    count = (size_t)(g_LastOnScreenProp - g_OnScreenPropList);
    visible_door_count = 0U;
    memset(visible_doors, 0, sizeof(visible_doors));
    for (cursor = g_OnScreenPropList;
            cursor < g_OnScreenPropList + count; ++cursor) {
        if (*cursor == NULL || native_door_for_prop(*cursor) == NULL) continue;
        if (visible_door_count >= GE_DOOR_INTERACTION_VISIBLE_CAPACITY)
            return 0;
        visible_doors[visible_door_count++] = *cursor;
    }
    return 1;
}

GeOriginalDoorInteractionResult ge_original_door_interaction_tick(void)
{
    struct player *player = ge_original_spawn_player_get();
    GeOriginalDoorInteractionResult result = GE_ORIGINAL_DOOR_INTERACTION_IDLE;
    size_t index;
    if (interaction_state != NULL) interaction_state->ticks++;
    if (player == NULL || player->prop == NULL) {
        result = GE_ORIGINAL_DOOR_INTERACTION_INVALID_STATE;
        goto done;
    }
    if (!player->field_D0) goto done;
    if (interaction_state != NULL) interaction_state->activate_edges++;
    interact_prop = NULL;
    for (index = visible_door_count; index > 0; index--)
        if (!door_test_for_interact(visible_doors[index - 1U], player)) break;
    if (interact_prop != NULL) {
        result = interact_door(interact_prop, player);
        if (interaction_state != NULL) {
            interaction_state->last_prop = interact_prop;
            interaction_state->tick_operations++; /* TICKOP_NONE */
            if (result == GE_ORIGINAL_DOOR_INTERACTION_ACTIVATED)
                interaction_state->activations++;
            else if (result == GE_ORIGINAL_DOOR_INTERACTION_LOCKED)
                interaction_state->locked_attempts++;
        }
    } else {
        result = GE_ORIGINAL_DOOR_INTERACTION_RELOAD_REQUESTED;
        if (interaction_state != NULL) interaction_state->reload_requests++;
    }
done:
    if (interaction_state != NULL) interaction_state->result = result;
    return result;
}

const char *ge_original_door_interaction_result_name(
    GeOriginalDoorInteractionResult result)
{
    switch (result) {
    case GE_ORIGINAL_DOOR_INTERACTION_IDLE: return "idle";
    case GE_ORIGINAL_DOOR_INTERACTION_RELOAD_REQUESTED: return "reload requested";
    case GE_ORIGINAL_DOOR_INTERACTION_ACTIVATED: return "activated";
    case GE_ORIGINAL_DOOR_INTERACTION_LOCKED: return "locked";
    case GE_ORIGINAL_DOOR_INTERACTION_INVALID_STATE: return "invalid state";
    case GE_ORIGINAL_DOOR_INTERACTION_MISSING_PADLOCK_PROVIDER:
        return "missing padlock provider";
    case GE_ORIGINAL_DOOR_INTERACTION_MISSING_SWITCH_PROVIDER:
        return "missing switch provider";
    default: return "unknown";
    }
}
