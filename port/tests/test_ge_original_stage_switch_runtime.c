#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>
#include <bondtypes.h>
#include "ge_original_stage_switch_runtime.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

ObjectRecord *g_LevelLoadPropSwitch;
s32 g_ClockTimer = 1;
#define RUNTIMEBITFLAG_ACTIVATED (1U << 14U)
static void doorStartOpen(DoorRecord *door) { (void)door; }
static void doorStartClose(DoorRecord *door) { (void)door; }
void sub_GAME_7F03E6A0(PropRecord *prop);
#include "switch_exact.inc"

static ObjectRecord *definitions[277];
static void *resolve(void *context, size_t index)
{
    assert(context == definitions);
    assert(index < 277U);
    return definitions[index];
}

int main(void)
{
    const size_t panels[] = {269U, 271U, 273U, 275U};
    GeOriginalStagePropRecord records[277] = {0};
    GeOriginalStageSetupRuntime setup = {0};
    GeOriginalStageSwitchRuntime runtime = {0};
    ObjectRecord panel[4] = {0};
    DoorRecord gates[2] = {0};
    PropRecord panel_props[4] = {0}, gate_props[2] = {0};
    uint32_t commands[4][4] = {
        {PROPDEF_SWITCH, (uint32_t)-1, (uint32_t)-3, 0},
        {PROPDEF_SWITCH, (uint32_t)-1, (uint32_t)-5, 0},
        {PROPDEF_SWITCH, (uint32_t)-1, (uint32_t)-6, 0},
        {PROPDEF_SWITCH, (uint32_t)-1, (uint32_t)-8, 0},
    };
    setup.loaded = 1U;
    setup.prop_records = records;
    setup.prop_record_count = 277U;
    for (size_t i = 0U; i < 2U; ++i) {
        gates[i].type = PROPDEF_DOOR;
        gates[i].prop = &gate_props[i];
        gate_props[i].door = &gates[i];
        gates[i].flags2 = 0x40000004;
        gates[i].flags = PROPFLAG_CANNOT_ACTIVATE;
        gates[i].linkedDoor = &gates[1U - i];
        gates[i].maxFrac = (float)0xf333 / 65536.0f;
        gates[i].accel = gates[i].decel = 16.0f / 65536.0f;
        gates[i].maxSpeed = 204.0f / 65536.0f;
        definitions[267U + i] = (ObjectRecord *)&gates[i];
    }
    for (size_t i = 0U; i < 4U; ++i) {
        panel[i].type = PROPDEF_MULTI_MONITOR;
        panel[i].prop = &panel_props[i];
        panel[i].runtime_bitflags = 0x4010; /* Actual captured selected panel. */
        panel_props[i].obj = &panel[i];
        definitions[panels[i]] = &panel[i];
        records[panels[i] + 1U].type = PROPDEF_SWITCH;
        records[panels[i] + 1U].words = commands[i];
        records[panels[i] + 1U].word_count = 4U;
    }
    sub_GAME_7F03E6A0(&panel_props[0]);
    assert(gates[0].openstate == DOORSTATE_STATIONARY);
    assert(ge_original_stage_switch_runtime_materialize(
        &runtime, &setup, definitions, resolve));
    assert(runtime.relation_count == 4U && runtime.omitted_count == 0U);
    for (size_t i = 0U; i < 4U; ++i) {
        LinkRecord *link = runtime.links;
        assert(link[i].first == &panel_props[i]);
        assert(link[i].second == &gate_props[i / 2U]);
        assert(panel[i].runtime_bitflags == 0x4011);
    }
    /* The unchanged activation callback reaches the native gate wrapper,
     * enters Dam's waiting interlock, then starts real authored travel. */
    sub_GAME_7F03E6A0(&panel_props[0]);
    assert(gates[0].openstate == DOORSTATE_WAITING);
    tick_waiting(&gates[0]);
    assert(gates[0].openstate == DOORSTATE_OPENING);
    for (int i = 0; i < 60; ++i) updateDoorDisplacement(&gates[0]);
    assert(gates[0].openPosition > 0.1f);
    assert(gates[1].openPosition == 0.0f);
    assert(gates[0].runtime_bitflags & RUNTIMEBITFLAG_ACTIVATED);
    /* Opening the other gate closes its sibling first. */
    sub_GAME_7F03E6A0(&panel_props[2]);
    assert(gates[1].openstate == DOORSTATE_WAITING);
    assert(gates[0].openstate == DOORSTATE_CLOSING);
    tick_waiting(&gates[1]);
    assert(gates[1].openstate == DOORSTATE_WAITING);
    ge_original_stage_switch_runtime_close(&runtime);
    assert(g_LevelLoadPropSwitch == NULL && runtime.links == NULL);
    for (size_t i = 0; i < 4; ++i) assert(panel[i].runtime_bitflags == 0x4010);
    /* Authored-but-absent endpoints retain canonical omission behavior. */
    definitions[269] = NULL;
    assert(ge_original_stage_switch_runtime_materialize(
        &runtime, &setup, definitions, resolve));
    assert(runtime.relation_count == 3U && runtime.omitted_count == 1U);
    ge_original_stage_switch_runtime_close(&runtime);
    commands[1][2] = (uint32_t)-999;
    assert(!ge_original_stage_switch_runtime_materialize(
        &runtime, &setup, definitions, resolve));
    assert(g_LevelLoadPropSwitch == NULL && runtime.links == NULL);
    puts("stage switch runtime: native panel dispatch, interlock, travel and teardown passed");
    return 0;
}
