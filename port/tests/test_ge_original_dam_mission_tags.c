#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <bondconstants.h>
#include <bondtypes.h>
#include "game/chrai.h"
#include "game/objective_status.h"
#include "game/propobj.h"
#include "ge_original_dam_intro.h"
#include "ge_original_dam_mission_flow.h"
#include "ge_original_dam_mission_hud.h"
#include "ge_original_dam_world.h"
#include "ge_original_prop_state.h"

extern stagesetup UsetupdamZ;
extern ChrRecord *g_ActiveChrs;
extern s32 g_ActiveChrsCount;
extern s32 objectiveregisters1;
extern void ai(PropDefHeaderRecord *entity, PROP_TYPE entity_type);
extern char *LdamE[];
extern sfxRecord sfx_related[];
extern uint32_t ge_test_mission_sound_starts;
extern int16_t ge_test_mission_last_sound;
extern void hudmsgTopShow(char *message);

stagesetup g_CurrentSetup;
s32 g_UpperTextDisplayFlag;

typedef struct DamMissionTagHarness {
    GeOriginalPropState props;
    union {
        max_align_t alignment;
        DoorRecord door;
        struct {
            ObjectRecord object;
            MonitorRecord monitor;
            int32_t owner_offset;
            int32_t owner_part;
            int32_t image_num;
        } monitor;
    } definitions[11];
    uint32_t definition_count;
    struct {
        uint32_t id : 24;
        uint8_t room;
        int16_t mid;
        int16_t tail;
    } stan;
} DamMissionTagHarness;

static stagesetup *load_setup(void *context, int32_t stage_id)
{
    (void)context;
    return stage_id == LEVELID_DAM ? &UsetupdamZ : NULL;
}

static float get_room_scale_reciprocal(void *context)
{
    (void)context;
    return 1.0f / 0.23363999f;
}

static void *resolve_stan(void *context, const char *name)
{
    DamMissionTagHarness *harness = context;
    assert(name != NULL);
    return &harness->stan;
}

static void *allocate_definition(void *context, uint8_t type,
                                 size_t size_bytes)
{
    DamMissionTagHarness *harness = context;
    (void)type;
    assert(size_bytes <= sizeof(harness->definitions[0]));
    assert(harness->definition_count < 11U);
    return &harness->definitions[harness->definition_count++];
}

static void *allocate_prop(void *context, void *definition)
{
    DamMissionTagHarness *harness = context;
    return ge_original_prop_state_allocate(&harness->props, definition);
}

static void register_room(void *context, void *prop, int16_t room)
{
    DamMissionTagHarness *harness = context;
    ge_original_prop_state_register_room(&harness->props, prop, room);
}

int main(void)
{
    DamMissionTagHarness harness;
    GeOriginalSetupPadProviders pad_providers;
    GeOriginalSetupPadState pad_state;
    GeOriginalDamWorldProviders world_providers;
    GeOriginalDamWorldState world;
    GeOriginalDamMissionTagState tags;
    GeOriginalDamMissionFlowState flow;
    GeOriginalDamMonitorSnapshot monitor_snapshot;
    GeOriginalDamMissionHudRenderSnapshot hud_snapshot;
    MonitorRecord isolated_monitor;
    ObjectRecord native_header_only;
    ObjectRecord *tag5;
    ObjectRecord *tag4;
    const uint32_t *monitor_commands;
    size_t monitor_command_words;
    size_t setup_command_count;
    size_t setup_tinted_glass_count;
    PropRecord tracker_prop;
    WeaponObjRecord tracker_weapon;
    uint8_t tag_state;
    unsigned tick;

    memset(&harness, 0, sizeof(harness));
    harness.stan.room = 135;
    assert(ge_original_prop_state_reset(&harness.props, 137U));

    memset(&pad_providers, 0, sizeof(pad_providers));
    pad_providers.context = &harness;
    pad_providers.load_setup = load_setup;
    pad_providers.get_room_scale_reciprocal = get_room_scale_reciprocal;
    pad_providers.resolve_stan = resolve_stan;
    ge_original_setup_pad_bind(&pad_providers, &pad_state);
    ge_original_setup_pad_load(LEVELID_DAM);
    assert(pad_state.loaded);
    assert(ge_dam_setup_world_validate_authored_stream(
        &setup_command_count, &setup_tinted_glass_count));
    assert(setup_command_count == 328U);
    assert(setup_tinted_glass_count == 5U);

    memset(&world_providers, 0, sizeof(world_providers));
    world_providers.context = &harness;
    world_providers.allocate_definition = allocate_definition;
    world_providers.allocate_prop = allocate_prop;
    world_providers.register_room = register_room;
    ge_dam_setup_world_materializer_bind(&world_providers, &world);
    ge_dam_setup_world_materialize_first_authored();
    assert(ge_dam_setup_world_materialize_linked_door());
    assert(ge_dam_setup_world_materialize_spawn_windows()
        == GE_ORIGINAL_DAM_SPAWN_WINDOW_COUNT);
    assert(world.definitions_materialized == 9U);
    assert(harness.definition_count == 9U);
    assert(ge_dam_setup_world_materialize_mission_tags(&tags));
    assert(tags.loaded && tags.tags_registered == 2U);
    assert(tags.tag5_object.command_index == 290);
    assert(tags.tag5_object.propdef_type == PROPDEF_MONITOR);
    assert(tags.tag5_object.object_id == 335);
    assert(tags.tag5_object.pad_id == 10057);
    assert(tags.tag4_object.command_index == 292);
    assert(tags.tag4_object.propdef_type == PROPDEF_PROP);
    assert(tags.tag4_object.object_id == 70);
    assert(tags.tag4_object.pad_id == 10058);
    assert(harness.props.allocation_calls == 11U);
    assert(harness.props.room_registration_calls == 11U);

    /* Command 290 is an exact native MonitorObjRecord prefix/tail: authored
     * owner fields and image 5 select the original green-text-up command
     * stream after setupSingleMonitor copies the canonical controller. */
    assert(ge_dam_setup_world_mission_monitor_snapshot(
        &tags.tag5_object, &monitor_snapshot));
    assert(!ge_dam_setup_world_mission_monitor_snapshot(
        &tags.tag4_object, &monitor_snapshot));
    monitor_commands = ge_original_dam_monitor_green_text_commands(
        &monitor_command_words);
    assert(monitor_command_words == 31U);
    assert(monitor_snapshot.commands == monitor_commands);
    assert(monitor_snapshot.owner_offset == -1);
    assert(monitor_snapshot.owner_part == -1);
    assert(monitor_snapshot.image_num == 5);
    assert(monitor_snapshot.command_offset == 0U);
    assert(monitor_snapshot.pause60 == -1);
    assert(monitor_snapshot.rotation == 0.0f);
    assert(monitor_snapshot.xscale == 1.0f);
    assert(monitor_snapshot.yscale == 1.0f);
    assert(monitor_snapshot.xmid == 0.5f);
    assert(monitor_snapshot.ymid == 0.5f);
    assert(monitor_snapshot.red == 0xffU);
    assert(monitor_snapshot.green == 0xffU);
    assert(monitor_snapshot.blue == 0xffU);
    assert(monitor_snapshot.alpha == 0xffU);
    assert(monitor_commands[0] == 0x7U);
    assert(monitor_commands[monitor_command_words - 1U] == 0xbU);
    monitor_commands = ge_original_dam_monitor_bond_commands(
        &monitor_command_words);
    assert(monitor_command_words == 35U);
    assert(monitor_commands[0] == 0x7U);
    assert(monitor_commands[monitor_command_words - 1U] == 0xbU);
    memset(&isolated_monitor, 0xa5, sizeof(isolated_monitor));
    assert(ge_original_dam_monitor_initialize(NULL, 5)
        == GE_ORIGINAL_DAM_MONITOR_INVALID_ARGUMENT);
    assert(ge_original_dam_monitor_initialize(&isolated_monitor, 4)
        == GE_ORIGINAL_DAM_MONITOR_UNSUPPORTED_IMAGE);

    tag5 = objFindByTagId(5);
    tag4 = objFindByTagId(4);
    assert(tag5 == tags.tag5_object.definition);
    assert(tag4 == tags.tag4_object.definition);
    assert(tag5->prop == tags.tag5_object.prop);
    assert(tag4->prop == tags.tag4_object.prop);
    assert(ge_dam_setup_world_definition_header(
        tag5, NULL, &tag_state, NULL));
    assert((tag_state & PROPSTATE_DESTROYED) == 0U);
    assert(ge_dam_setup_world_definition_header(
        tag4, NULL, &tag_state, NULL));
    assert((tag_state & PROPSTATE_DESTROYED) == 0U);
    assert(objIsHealthy(tag5));
    assert(objIsHealthy(tag4));

    /* The common all-stage runtime owns native promoted definitions rather
     * than the legacy Dam materializer sidecar. Exercise the exact generated
     * objIsHealthy body against that same header ABI. */
    memset(&native_header_only, 0, sizeof(native_header_only));
    native_header_only.maxdamage = 1000.0f;
    assert(!ge_dam_setup_world_definition_header(
        &native_header_only, NULL, NULL, NULL));
    assert(objIsHealthy(&native_header_only));
    ((PropDefHeaderRecord *)(void *)&native_header_only)->state =
        PROPSTATE_DESTROYED;
    assert(!objIsHealthy(&native_header_only));

    assert(ge_original_dam_mission_flow_begin(&flow));
    assert(objectiveregisters1 == 0);
    assert(g_ActiveChrs != NULL && g_ActiveChrsCount == 8);
    assert(g_ActiveChrs[0].aioffset == 3);

    /* Exact sustained normal scheduling: objective incomplete, tags healthy,
     * no tracker, then authored goto 0x2a + yield on every frame. */
    for (tick = 0U; tick < 120U; tick++)
        assert(ge_original_dam_mission_flow_tick(&flow));
    assert(objectiveregisters1 == 0);
    assert(g_ActiveChrs[0].aioffset == 3);
    assert(flow.ticks == 120U);
    assert(flow.yield_transitions == 0U);
    assert(objFindByTagId(5) == tag5 && objIsHealthy(tag5));
    assert(objFindByTagId(4) == tag4 && objIsHealthy(tag4));

    /* Exact covert-modem attached branch: the original item-child traversal,
     * HUD enqueue, SFX start, object-emitter binding and objective bit all run
     * in one authored ai_20 quantum. */
    memset(&tracker_prop, 0, sizeof(tracker_prop));
    memset(&tracker_weapon, 0, sizeof(tracker_weapon));
    tracker_prop.type = PROP_TYPE_WEAPON;
    tracker_prop.weapon = &tracker_weapon;
    tracker_weapon.weaponnum = 0x2f;
    tag5->prop->child = &tracker_prop;
    ge_original_dam_mission_hud_reset();
    assert(!ge_original_dam_mission_hud_render_snapshot(NULL));
    assert(ge_original_dam_mission_hud_render_snapshot(&hud_snapshot));
    assert(hud_snapshot.count == 0U);
    ge_test_mission_sound_starts = 0U;
    assert(ge_original_dam_mission_flow_tick(&flow));
    assert(objectiveregisters1 == 0x00000100);
    assert(flow.objective_registers == 0x00000100U);
    assert(flow.hud_message_count == 1U);
    assert(ge_original_dam_mission_hud_count() == 1U);
    assert(strcmp(ge_original_dam_mission_hud_message(0), LdamE[8]) == 0);
    assert(ge_original_dam_mission_hud_render_snapshot(&hud_snapshot));
    assert(hud_snapshot.count == 1U);
    assert(strcmp(hud_snapshot.messages[0], LdamE[8]) == 0);
    hudmsgTopShow(LdamE[14]);
    assert(ge_original_dam_mission_hud_render_snapshot(&hud_snapshot));
    assert(hud_snapshot.count == 2U);
    assert(strcmp(hud_snapshot.messages[0], LdamE[8]) == 0);
    assert(strcmp(hud_snapshot.messages[1], LdamE[14]) == 0);
    assert(ge_test_mission_sound_starts == 1U);
    assert(ge_test_mission_last_sound == 0x00e3);
    assert(sfx_related[0].Obj == tag5);

    tag5->prop->child = NULL;
    objectiveregisters1 = 0;
    ge_original_dam_mission_hud_reset();

    /* Model the completed object-damage boundary in the native PropDef header
     * sidecar. No objective state is injected: ai_20 must detect destruction
     * and perform its own authored mission-register writes. */
    assert(ge_dam_setup_world_definition_header(
        tag5, NULL, &tag_state, NULL));
    assert(ge_dam_setup_world_definition_set_state(
        tag5, (uint8_t)(tag_state | PROPSTATE_DESTROYED)));
    assert(!objIsHealthy(tag5));
    assert(objIsHealthy(tag4));
    ge_original_dam_mission_hud_reset();
    assert(ge_original_dam_mission_flow_tick(&flow));
    assert(objectiveregisters1 == 0x00000a00);
    assert(g_ActiveChrs[0].aioffset == 113);
    assert(ge_original_dam_mission_hud_count() == 1U);
    assert(strcmp(ge_original_dam_mission_hud_message(0), LdamE[14]) == 0);
    assert(strcmp(ge_original_dam_mission_hud_message(0),
        "Satellite communications link destroyed.\n"
        "Data cannot be intercepted by MI6!\n") == 0);

    puts("original Dam setup: 328 aligned records including five 37-word "
         "tinted-glass records; ai_20: 120 live healthy ticks; modem attach "
         "SFX/HUD; tag-5 destroyed 3 -> 113, objective 0x00000a00");
    return 0;
}
