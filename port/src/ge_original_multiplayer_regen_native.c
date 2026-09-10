#include "ge_original_multiplayer_regen.h"
#include "ge_original_default_object_internal.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/chrai.h"
#include "game/propobj.h"
#include "snd.h"
#include <string.h>

extern ALBank *g_musicSfxBufferPtr;

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
