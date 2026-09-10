#include "ge_original_multiplayer_regen.h"
#include "ge_original_default_object_internal.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/chrai.h"
#include "game/propobj.h"
#include "snd.h"
#include <string.h>
#include <limits.h>

extern ALBank *g_musicSfxBufferPtr;
extern PropRecord g_Props[MAX_PROPS];
extern PropRecord *g_ActivePropsHead, *g_ActivePropsTail;

static void enable_prop(void *context, void *prop)
{
    (void)context;
    chrpropEnable((PropRecord *)prop);
}

static void update_rooms(void *context, void *opaque)
{
    PropRecord *prop=opaque;
    (void)context;
    /* Exact OBJ/WEAPON arm of sub_GAME_7F03E134 in chrpropTick. */
    ge_original_setup_update_object_room_position_slice(prop->obj);
}

static void play_sound(void *context, const float position[3])
{
    coord3d pos;
    (void)context;
    memcpy(pos.f,position,sizeof(pos.f));
    chrobjSndCreatePostEventDefault(
        sndPlaySfx((struct ALBankAlt_s *)g_musicSfxBufferPtr,OBJ_REGEN_SFX,NULL),&pos);
}

int ge_original_multiplayer_regen_native_providers(
    GeOriginalMultiplayerRegenProviders *providers)
{
    if(!providers) return 0;
    providers->context=NULL;
    providers->enable_prop=enable_prop;
    providers->update_rooms=update_rooms;
    providers->play_regen_sound=play_sound;
    return 1;
}

GeOriginalMultiplayerRegenStatus ge_original_multiplayer_regen_tick_all(
    int32_t tick60, GeOriginalMultiplayerRegenBatch *batch)
{
    PropRecord *ordered[MAX_PROPS];
    uint8_t visited[MAX_PROPS] = {0};
    GeOriginalMultiplayerRegenBatch result = {0,0,0,-1};
    GeOriginalMultiplayerRegenProviders providers;
    PropRecord *prop = g_ActivePropsTail;
    PropRecord *next = NULL;
    uint32_t count = 0, i;
    if (!batch || tick60 < 0 || tick60 > INT16_MAX)
        return GE_ORIGINAL_MP_REGEN_INVALID_ARGUMENT;
    /* Validate the complete list before callbacks can mutate flags/rooms. */
    while (prop) {
        const uintptr_t address = (uintptr_t)prop;
        const uintptr_t first = (uintptr_t)&g_Props[0];
        const uintptr_t end = (uintptr_t)&g_Props[MAX_PROPS];
        uint32_t slot;
        if (address < first || address >= end
                || (address-first) % sizeof(*prop) || count == MAX_PROPS) {
            *batch = result;
            return GE_ORIGINAL_MP_REGEN_INVALID_POOL;
        }
        slot = (uint32_t)((address-first) / sizeof(*prop));
        if (visited[slot] || prop->next != next) {
            result.failed_pool_index = (int32_t)slot;
            *batch = result;
            return GE_ORIGINAL_MP_REGEN_INVALID_POOL;
        }
        visited[slot] = 1;
        ordered[count++] = prop;
        next = prop;
        prop = prop->prev;
    }
    if (next != g_ActivePropsHead) {
        *batch = result;
        return GE_ORIGINAL_MP_REGEN_INVALID_POOL;
    }
    ge_original_multiplayer_regen_native_providers(&providers);
    for (i=0; i<count; ++i) {
        GeOriginalMultiplayerRegenStatus status;
        prop = ordered[i];
        ++result.visited;
        status = ge_original_multiplayer_regen_tick(prop,tick60,&providers);
        if (status == GE_ORIGINAL_MP_REGEN_OK) ++result.ticked;
        else if (status == GE_ORIGINAL_MP_REGEN_UNSUPPORTED_OBJECT)
            ++result.unsupported;
        else if (status != GE_ORIGINAL_MP_REGEN_IDLE) {
            result.failed_pool_index = (int32_t)(prop-g_Props);
            *batch = result;
            return status;
        }
    }
    *batch = result;
    return result.unsupported ? GE_ORIGINAL_MP_REGEN_UNSUPPORTED_OBJECT
        : (result.ticked ? GE_ORIGINAL_MP_REGEN_OK : GE_ORIGINAL_MP_REGEN_IDLE);
}
