#include "ge_original_multiplayer_regen.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include <math.h>
#include <limits.h>

extern void ge_original_mp_regen_tick_exact(PropRecord *, s32,
    const GeOriginalMultiplayerRegenProviders *);

static int ordinary_pickup(const PropRecord *prop)
{
    const ObjectRecord *obj;
    if (!prop || (prop->type != PROP_TYPE_OBJ && prop->type != PROP_TYPE_WEAPON)
            || !(obj = prop->obj) || obj->prop != prop) return 0;
    if (obj->flags & (PROPFLAG_INSIDEANOTHEROBJ | PROPFLAG_ASSIGNEDTOCHR)
            || prop->parent || obj->maxdamage != 0.0f
            || (obj->state & PROPSTATE_DESTROYED)) return 0;
    return obj->type == PROPDEF_COLLECTABLE || obj->type == PROPDEF_MAGAZINE
        || obj->type == PROPDEF_AMMO || obj->type == PROPDEF_ARMOUR
        || obj->type == PROPDEF_KEY || obj->type == PROPDEF_HAT;
}

GeOriginalMultiplayerRegenStatus ge_original_multiplayer_regen_tick(
    void *opaque, int32_t tick60,
    const GeOriginalMultiplayerRegenProviders *providers)
{
    PropRecord *prop = opaque;
    int count, player;
    if (!prop || tick60 < 0 || tick60 > INT16_MAX)
        return GE_ORIGINAL_MP_REGEN_INVALID_ARGUMENT;
    if (prop->timetoregen <= 0 || tick60 == 0) return GE_ORIGINAL_MP_REGEN_IDLE;
    if (!ordinary_pickup(prop)) return GE_ORIGINAL_MP_REGEN_UNSUPPORTED_OBJECT;
    if (!providers || !providers->enable_prop || !providers->update_rooms
            || !providers->play_regen_sound)
        return GE_ORIGINAL_MP_REGEN_MISSING_PROVIDER;
    count = getPlayerCount();
    if (count < 2 || count > 4) return GE_ORIGINAL_MP_REGEN_INVALID_ROSTER;
    if (!isfinite(prop->pos.x) || !isfinite(prop->pos.y) || !isfinite(prop->pos.z))
        return GE_ORIGINAL_MP_REGEN_INVALID_ARGUMENT;
    for (player = 0; player < count; ++player) {
        const PropRecord *viewer;
        if (!g_playerPointers[player] || !(viewer = g_playerPointers[player]->prop)
                || !isfinite(viewer->pos.x) || !isfinite(viewer->pos.y)
                || !isfinite(viewer->pos.z)) return GE_ORIGINAL_MP_REGEN_INVALID_ROSTER;
    }
    ge_original_mp_regen_tick_exact(prop, tick60, providers);
    return GE_ORIGINAL_MP_REGEN_OK;
}
