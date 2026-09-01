#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/player.h"
#include "ge_original_door_interaction.h"
#include "ge_original_door_runtime.h"

stagesetup g_CurrentSetup;
PropRecord *g_OnScreenPropList[16];
PropRecord **g_LastOnScreenProp = g_OnScreenPropList;

typedef struct TestDoorMap {
    ObjectRecord *definition;
    DoorRecord *runtime;
} TestDoorMap;

static struct player test_player;
static TestDoorMap door_maps[2];
static uint32_t locked_messages;

struct player *ge_original_spawn_player_get(void)
{
    return &test_player;
}

DoorRecord *ge_port_door_runtime_native_definition(void *definition)
{
    size_t index;
    for (index = 0; index < 2U; index++)
        if (door_maps[index].definition == definition)
            return door_maps[index].runtime;
    return NULL;
}

ObjectRecord *ge_port_door_runtime_object(DoorRecord *door)
{
    size_t index;
    for (index = 0; index < 2U; index++)
        if (door_maps[index].runtime == door)
            return door_maps[index].definition;
    return NULL;
}

GeOriginalDoorRuntimeStatus ge_original_door_runtime_activate(
    void *definition, int32_t state)
{
    DoorRecord *door = ge_port_door_runtime_native_definition(definition);
    DoorRecord *linked;
    if (door == NULL) return GE_ORIGINAL_DOOR_RUNTIME_INVALID_ARGUMENT;
    door->openstate = (s8)state;
    linked = door->linkedDoor;
    while (linked != NULL && linked != door) {
        linked->openstate = (s8)state;
        linked = linked->linkedDoor;
    }
    return GE_ORIGINAL_DOOR_RUNTIME_OK;
}

int ge_dam_setup_world_definition_header(const void *definition,
                                          uint16_t *extrascale,
                                          uint8_t *state,
                                          uint8_t *type)
{
    (void)definition;
    if (extrascale != NULL) *extrascale = 0;
    if (state != NULL) *state = 0;
    if (type != NULL) *type = PROPDEF_DOOR;
    return 1;
}

static void show_locked(void *context, void *definition)
{
    (void)context;
    (void)definition;
    locked_messages++;
}

int main(void)
{
    BoundPadRecord pads[1];
    ObjectRecord definitions[2];
    DoorRecord runtimes[2];
    PropRecord door_props[2];
    PropRecord player_prop;
    void *visible[2];
    GeOriginalDoorInteractionProviders providers;
    GeOriginalDoorInteractionState state;

    memset(pads, 0, sizeof(pads));
    memset(definitions, 0, sizeof(definitions));
    memset(runtimes, 0, sizeof(runtimes));
    memset(door_props, 0, sizeof(door_props));
    memset(&player_prop, 0, sizeof(player_prop));
    memset(&test_player, 0, sizeof(test_player));
    memset(&providers, 0, sizeof(providers));

    pads[0].pos.x = 0.0f;
    pads[0].pos.y = 0.0f;
    pads[0].pos.z = 0.0f;
    pads[0].up.x = 1.0f;
    pads[0].look.z = 1.0f;
    pads[0].bbox.ymin = -50.0f;
    pads[0].bbox.ymax = 50.0f;
    pads[0].bbox.xmin = -20.0f;
    pads[0].bbox.xmax = 20.0f;
    pads[0].bbox.zmin = -10.0f;
    pads[0].bbox.zmax = 10.0f;
    g_CurrentSetup.boundpads = pads;

    player_prop.type = PROP_TYPE_PLAYER;
    player_prop.pos.z = -180.0f;
    player_prop.rooms[0] = 1;
    player_prop.rooms[1] = UINT8_MAX;
    test_player.prop = &player_prop;
    test_player.vv_theta = 360.0f;
    test_player.field_488.theta_transform.z = 1.0f;

    for (size_t index = 0; index < 2U; index++) {
        definitions[index].pad = 0;
        definitions[index].runtime_pos = pads[0].pos;
        definitions[index].prop = &door_props[index];
        door_props[index].type = PROP_TYPE_DOOR;
        door_props[index].obj = &definitions[index];
        door_props[index].pos = pads[0].pos;
        door_props[index].rooms[0] = 1;
        door_props[index].rooms[1] = UINT8_MAX;
        runtimes[index].maxFrac = 0.95f;
        runtimes[index].openstate = DOORSTATE_STATIONARY;
        door_maps[index].definition = &definitions[index];
        door_maps[index].runtime = &runtimes[index];
        visible[index] = &door_props[index];
    }
    runtimes[0].linkedDoor = &runtimes[1];
    runtimes[1].linkedDoor = &runtimes[0];

    providers.show_locked_message = show_locked;
    ge_original_door_interaction_bind(&providers, &state);
    assert(ge_original_door_interaction_bind_visible_doors(visible, 2U));
    assert((door_props[0].flags & PROPFLAG_ONSCREEN) != 0);
    assert(ge_original_door_interaction_tick()
           == GE_ORIGINAL_DOOR_INTERACTION_IDLE);

    test_player.field_D0 = 1;
    assert(ge_original_door_interaction_tick()
           == GE_ORIGINAL_DOOR_INTERACTION_ACTIVATED);
    assert(runtimes[1].openstate == DOORSTATE_OPENING);
    assert(runtimes[0].openstate == DOORSTATE_OPENING);
    assert(state.interaction_hits == 1U && state.tick_operations == 1U);
    assert(state.activations == 1U);

    g_OnScreenPropList[0] = &door_props[0];
    g_OnScreenPropList[1] = &door_props[1];
    g_LastOnScreenProp = g_OnScreenPropList + 2U;
    assert(ge_original_door_interaction_bind_onscreen_doors());

    test_player.field_D0 = 0;
    (void)ge_original_door_interaction_tick();
    runtimes[0].openstate = runtimes[1].openstate = DOORSTATE_STATIONARY;
    runtimes[0].keyflags = runtimes[1].keyflags = UINT32_C(1);
    test_player.field_D0 = 1;
    assert(ge_original_door_interaction_tick()
           == GE_ORIGINAL_DOOR_INTERACTION_LOCKED);
    assert(locked_messages == 1U);
    assert((definitions[1].flags2 & PROPFLAG2_00000008) != 0U);

    runtimes[0].keyflags = runtimes[1].keyflags = 0;
    definitions[1].runtime_bitflags = RUNTIMEBITFLAG_PADLOCKEDDOOR;
    assert(ge_original_door_interaction_tick()
           == GE_ORIGINAL_DOOR_INTERACTION_MISSING_PADLOCK_PROVIDER);
    definitions[1].runtime_bitflags = 0;

    player_prop.pos.z = -1000.0f;
    assert(ge_original_door_interaction_tick()
           == GE_ORIGINAL_DOOR_INTERACTION_RELOAD_REQUESTED);
    assert(state.reload_requests == 1U);

    assert(ge_original_door_interaction_bind_visible_doors(NULL, 0U));
    assert((door_props[0].flags & PROPFLAG_ONSCREEN) == 0);
    puts("exact door interaction adapter tests passed");
    return 0;
}
