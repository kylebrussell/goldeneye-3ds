#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#ifndef PLAYERFLAG
typedef int PLAYERFLAG;
#endif
#include "game/chrai.h"
#include "game/chr.h"
#include "game/player.h"
#include "game/propobj.h"

#include "ge_original_covert_modem_object.h"
#include "ge_original_dam_guard_chr_scheduler.h"
#include "ge_original_dam_world.h"
#include "ge_original_stage_active_props.h"
#include "ge_original_stage_setup.h"

enum {
    MIXED_PROP_CAPACITY = 16,
    MIXED_DEFINITION_CAPACITY = 4,
    MIXED_GUARD_COUNT = 4,
    MIXED_EXPECTED_PROP_COUNT = 10,
    MIXED_FIRST_GUARD_COMMAND = 23,
    MIXED_GLASS_COMMAND = 107,
    MIXED_PROP_COMMAND = 122,
    MIXED_FIRST_DOOR_COMMAND = 267,
    MIXED_SECOND_DOOR_COMMAND = 268,
    MIXED_GUARD_BODY = 37,
    MIXED_MODEM_HAS_PROJECTILE = 0x80,
    MIXED_MODEM_IS_RETICK = 0x08
};

typedef enum MixedEventKind {
    MIXED_EVENT_REUSED_SMOKE,
    MIXED_EVENT_MODEM,
    MIXED_EVENT_GUARD_3,
    MIXED_EVENT_GUARD_2,
    MIXED_EVENT_GUARD_1,
    MIXED_EVENT_GUARD_0,
    MIXED_EVENT_DOOR_2,
    MIXED_EVENT_DOOR_1,
    MIXED_EVENT_PROP,
    MIXED_EVENT_GLASS,
    MIXED_EVENT_VIEWER
} MixedEventKind;

typedef struct MixedDefinitionSlot {
    union {
        max_align_t alignment;
        unsigned char bytes[sizeof(DoorRecord)];
    } storage;
} MixedDefinitionSlot;

typedef struct MixedHarness {
    PropRecord props[MIXED_PROP_CAPACITY];
    MixedDefinitionSlot definitions[MIXED_DEFINITION_CAPACITY];
    ChrRecord guards[MIXED_GUARD_COUNT];
    Projectile modem_projectile;
    struct {
        uint32_t id : 24;
        uint8_t room;
        int16_t mid;
        int16_t tail;
    } stan;
    GeOriginalDamWorldState world;
    size_t free_prop_index;
    size_t definition_count;
    size_t event_count;
    MixedEventKind events[MIXED_EXPECTED_PROP_COUNT];
    unsigned commits;
    unsigned global_tail_calls;
    unsigned all_chr_ticks;
    unsigned canonical_player_ticks;
    unsigned canonical_door_ticks;
    unsigned separate_movement_ticks;
    unsigned separate_door_ticks;
    unsigned matrix_refreshes;
    unsigned remove_guard_zero;
    unsigned guard_zero_removed;
    unsigned remove_glass;
    unsigned glass_removed;
} MixedHarness;

extern stagesetup UsetupdamZ;
stagesetup g_CurrentSetup;
PropRecord *g_ActivePropsTail;
PropRecord *g_ActivePropsHead;
PropRecord *g_FreeProps;
ChrRecord *g_ChrSlots;
s32 g_NumChrSlots;
s32 g_ClockTimer;
s32 g_GlobalTimer;
f32 g_GlobalTimerDelta;
static struct player mixed_player;
struct player *g_CurrentPlayer=&mixed_player;
struct player *g_playerPointers[4];
static u8 mixed_ai_list[]={AI_Yield,AI_EndList};

static MixedHarness harness;

void ge_original_dam_guard_all_chr_tick_exact(void)
{
    ++harness.all_chr_ticks;
}

static void unexpected_boundary(void)
{
    assert(!"unexpected mixed propsTick boundary");
    abort();
}

static s32 setup_record_words(u8 type)
{
    switch (type) {
    case PROPDEF_GUARD: return 7;
    case PROPDEF_DOOR: return 64;
    case PROPDEF_DOOR_SCALE: return 2;
    case PROPDEF_PROP: case PROPDEF_GLASS: case PROPDEF_SAFE:
    case PROPDEF_GAS_RELEASING: case PROPDEF_ALARM: case PROPDEF_RACK:
    case PROPDEF_HAT: return 32;
    case PROPDEF_KEY: return 33;
    case PROPDEF_TINTED_GLASS: return 37;
    case PROPDEF_CCTV: return 0x3b;
    case PROPDEF_MAGAZINE: return 0x21;
    case PROPDEF_COLLECTABLE: return 0x22;
    case PROPDEF_MONITOR: return 0x40;
    case PROPDEF_MULTI_MONITOR: return 0x95;
    case PROPDEF_AUTOGUN: return 0x36;
    case PROPDEF_LINK: case PROPDEF_GUARD_ATTRIBUTE: return 3;
    case PROPDEF_SWITCH: return 4;
    case PROPDEF_SAFE_ITEM: return 5;
    case PROPDEF_AMMO: return 0x2d;
    case PROPDEF_ARMOUR: return 0x22;
    case PROPDEF_TAG: return 4;
    case PROPDEF_RENAME: return 10;
    case PROPDEF_OBJECTIVE_START: return 4;
    case PROPDEF_OBJECTIVE_END: return 1;
    case PROPDEF_OBJECTIVE_DESTROY_OBJECT:
    case PROPDEF_OBJECTIVE_COMPLETE_CONDITION:
    case PROPDEF_OBJECTIVE_FAIL_CONDITION:
    case PROPDEF_OBJECTIVE_COLLECT_OBJECT:
    case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT: return 2;
    case PROPDEF_OBJECTIVE_PHOTOGRAPH:
    case PROPDEF_OBJECTIVE_ENTER_ROOM: return 4;
    case PROPDEF_OBJECTIVE_NULL: case PROPDEF_OBJECTIVE_COPY_ITEM: return 1;
    case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM: return 5;
    case PROPDEF_WATCH_MENU_OBJECTIVE_TEXT: case PROPDEF_LOCK_DOOR: return 4;
    case PROPDEF_VEHICHLE: return 0x2c;
    case PROPDEF_AIRCRAFT: return 0x2d;
    case PROPDEF_TANK: return 0x38;
    case PROPDEF_CAMERAPOS: return 7;
    default: return 1;
    }
}

static const u32 *setup_command(s32 wanted)
{
    const u32 *command = (const u32 *)g_CurrentSetup.propDefs;
    s32 index = 0;
    while ((command[0] & 0xffU) != PROPDEF_END) {
        s32 words;
        if (index == wanted) return command;
        words = setup_record_words((u8)(command[0] & 0xffU));
        assert(words > 0);
        command += words;
        index++;
    }
    return NULL;
}

static void bind_command_stan(const u32 *command, int force_bound_pad)
{
    s16 pad = (s16)command[1];
    assert(pad >= 0);
    if (pad >= 10000) {
        g_CurrentSetup.boundpads[pad - 10000].stan =
            (StandTile *)&harness.stan;
    } else {
        g_CurrentSetup.pads[pad].stan = (StandTile *)&harness.stan;
        if (force_bound_pad)
            g_CurrentSetup.boundpads[pad].stan =
                (StandTile *)&harness.stan;
    }
}

PropRecord *chrpropAllocate(void)
{
    PropRecord *prop;
    assert(harness.free_prop_index < MIXED_PROP_CAPACITY);
    prop = &harness.props[harness.free_prop_index++];
    memset(prop, 0, sizeof(*prop));
    prop->rooms[0] = 0xffU;
    return prop;
}

void chrpropActivate(PropRecord *prop)
{
    assert(prop != NULL);
    if (g_ActivePropsTail != NULL) {
        g_ActivePropsTail->next = prop;
        prop->prev = g_ActivePropsTail;
        prop->next = NULL;
        g_ActivePropsTail = prop;
    } else {
        prop->prev = NULL;
        prop->next = NULL;
        g_ActivePropsHead = g_ActivePropsTail = prop;
    }
}

static void *allocate_definition(void *context, uint8_t type,
                                 size_t size_bytes)
{
    MixedHarness *mixed = context;
    (void)type;
    assert(size_bytes <= sizeof(DoorRecord));
    assert(mixed->definition_count < MIXED_DEFINITION_CAPACITY);
    return mixed->definitions[mixed->definition_count++].storage.bytes;
}

static void *allocate_prop(void *context, void *definition)
{
    (void)context;
    assert(definition != NULL);
    return chrpropAllocate();
}

static void activate_prop(void *context, void *prop)
{
    (void)context;
    chrpropActivate(prop);
}

static void enable_prop(void *context, void *opaque_prop)
{
    PropRecord *prop = opaque_prop;
    (void)context;
    prop->flags |= PROPFLAG_ENABLED;
}

static void register_room(void *context, void *opaque_prop, int16_t room)
{
    PropRecord *prop = opaque_prop;
    (void)context;
    assert(room == (int16_t)harness.stan.room);
    prop->rooms[0] = (u8)room;
    prop->rooms[1] = 0xffU;
}

PropRecord *chrpropGetActiveTail(void)
{
    return g_ActivePropsTail;
}

static void record_event(MixedEventKind event)
{
    assert(harness.event_count < MIXED_EXPECTED_PROP_COUNT);
    harness.events[harness.event_count++] = event;
}

static int world_entry_matches(const GeOriginalDamWorldEntry *entry,
                               const PropRecord *prop)
{
    return entry->prop == prop && entry->definition == prop->obj;
}

s32 objTick(PropRecord *prop)
{
    uint8_t definition_type = PROPDEF_END;
    assert(prop != NULL && prop->obj != NULL);
    if (prop->type == PROP_TYPE_WEAPON) {
        const PropDefHeaderRecord *header =
            (const PropDefHeaderRecord *)(const void *)prop->obj;
        int32_t weapon_id = -1;
        int32_t timer = -1;
        assert(ge_original_covert_modem_object_inspect(
            prop->obj, &weapon_id, NULL, &timer, NULL, &definition_type));
        assert(definition_type == PROPDEF_COLLECTABLE);
        assert(header->type == PROPDEF_COLLECTABLE);
        assert(header->state == 0U);
        assert(header->extrascale == 0x100U);
        assert(weapon_id == ITEM_BUG && timer == 1);
        assert((prop->obj->runtime_bitflags
                & MIXED_MODEM_HAS_PROJECTILE) != 0U);
        assert((prop->obj->runtime_bitflags & MIXED_MODEM_IS_RETICK) != 0U);
        assert(prop->obj->projectile == &harness.modem_projectile);
        record_event(MIXED_EVENT_MODEM);
        return TICKOP_NONE;
    }
    assert(ge_dam_setup_world_definition_header(
        prop->obj, NULL, NULL, &definition_type));
    assert((prop->obj->runtime_bitflags
            & (0x04U | MIXED_MODEM_HAS_PROJECTILE)) == 0U);
    if (world_entry_matches(&harness.world.first_glass, prop)) {
        assert(prop->type == PROP_TYPE_OBJ
            && definition_type == PROPDEF_GLASS);
        record_event(MIXED_EVENT_GLASS);
        if (harness.remove_glass && !harness.glass_removed)
            return TICKOP_FREE;
    } else if (world_entry_matches(&harness.world.first_object, prop)) {
        assert(prop->type == PROP_TYPE_OBJ
            && definition_type == PROPDEF_PROP);
        record_event(MIXED_EVENT_PROP);
    } else if (world_entry_matches(&harness.world.first_door, prop)) {
        assert(prop->type == PROP_TYPE_DOOR
            && definition_type == PROPDEF_DOOR);
        harness.canonical_door_ticks++;
        record_event(MIXED_EVENT_DOOR_1);
    } else if (world_entry_matches(&harness.world.second_door, prop)) {
        assert(prop->type == PROP_TYPE_DOOR
            && definition_type == PROPDEF_DOOR);
        harness.canonical_door_ticks++;
        record_event(MIXED_EVENT_DOOR_2);
    } else {
        unexpected_boundary();
    }
    return TICKOP_NONE;
}

s32 playerTick(PropRecord *prop)
{
    assert(prop == g_ActivePropsHead && prop->type == PROP_TYPE_VIEWER);
    /* Exact single-player playerTick takes clear_and_return when this viewer
     * has no multiplayer ChrRecord. It does not own MoveBond. */
    assert(prop->chr == NULL);
    prop->flags &= ~PROPFLAG_ONSCREEN;
    harness.canonical_player_ticks++;
    record_event(MIXED_EVENT_VIEWER);
    return TICKOP_NONE;
}

s32 ge_mixed_chr_tick_boundary(PropRecord *prop)
{
    size_t index;
    assert(prop != NULL && prop->type == PROP_TYPE_CHR && prop->chr != NULL);
    for (index = 0U; index < MIXED_GUARD_COUNT; index++) {
        if (prop->chr == &harness.guards[index]) {
            record_event((MixedEventKind)(MIXED_EVENT_GUARD_0
                - (int)index));
            if (index == 0U && harness.remove_guard_zero
                    && !harness.guard_zero_removed)
                return TICKOP_FREE;
            return TICKOP_NONE;
        }
    }
    unexpected_boundary();
    return TICKOP_NONE;
}

void propExecuteTickOperation(PropRecord *prop, TICKOP operation)
{
    assert(prop != NULL);
    if (operation == TICKOP_FREE) {
        if (prop->type == PROP_TYPE_CHR) {
            ChrRecord *chr = prop->chr;
            assert(chr == &harness.guards[0]);
            assert(chr->model != NULL && chr->chrnum >= 0);
            chr->model = NULL;
            chr->chrnum = -1;
            harness.guard_zero_removed = 1U;
        } else {
            assert(prop == harness.world.first_glass.prop
                    && prop->type == PROP_TYPE_OBJ);
            harness.glass_removed = 1U;
        }
        if (prop->prev != NULL) prop->prev->next = prop->next;
        else g_ActivePropsHead = prop->next;
        if (prop->next != NULL) prop->next->prev = prop->prev;
        else g_ActivePropsTail = prop->prev;
        prop->prev = prop->next = NULL;
        prop->flags &= (u8)~PROPFLAG_ENABLED;
    } else {
        assert(operation == TICKOP_NONE);
    }
    harness.commits++;
}

void chrpropDelist(PropRecord *prop)
{
    (void)prop;
    unexpected_boundary();
}

void chrpropActivateThisFrame(PropRecord *prop)
{
    (void)prop;
    unexpected_boundary();
}

u8 explosionChrpropExplosionTick(PropRecord *prop)
{
    (void)prop;
    unexpected_boundary();
    return TICKOP_NONE;
}

u8 explosionChrpropSmokeTick(PropRecord *prop)
{
    assert(prop == harness.guards[0].prop
            && harness.guard_zero_removed);
    record_event(MIXED_EVENT_REUSED_SMOKE);
    return TICKOP_NONE;
}

s32 get_cur_playernum(void) { return 0; }
s32 get_player_position_in_shuffled(s32 playernum)
{
    assert(playernum == 0);
    return 0;
}

void handle_alarm_gas_timer_calldamage(void)
{
    harness.global_tail_calls++;
}

void loop_set_sound_effect_all_slots(void)
{
    harness.global_tail_calls++;
}

void propsDefragRoomProps(void)
{
    harness.global_tail_calls++;
}

/* Exact objInit's native header sidecar is irrelevant for the zero-flag modem
 * constructor, but these typed providers preserve that original branch. */
u8 ge_port_default_object_state(ObjectRecord *object)
{
    (void)object;
    return 0;
}

void ge_port_default_object_set_state(ObjectRecord *object, u8 state)
{
    (void)object;
    assert(state == 0U);
}

static int exclusive_tick_ownership(void)
{
    return !(harness.canonical_door_ticks != 0U
                && harness.separate_door_ticks != 0U);
}

static void refresh_live_guard_matrices(void)
{
    size_t index;
    size_t live = 0U;
    for (index = 0U; index < MIXED_GUARD_COUNT; index++) {
        ChrRecord *chr = &harness.guards[index];
        PropRecord *prop = chr->prop;
        if (chr->model == NULL) {
            assert(index == 0U && harness.guard_zero_removed);
            continue;
        }
        assert(prop != NULL && prop->type == PROP_TYPE_CHR
                && prop->chr == chr);
        live++;
    }
    assert(live == (harness.guard_zero_removed ? 3U : 4U));
    harness.matrix_refreshes++;
}

int main(void)
{
    GeOriginalDamWorldProviders providers;
    GeOriginalStageActiveProps active_props={0};
    GeOriginalStageActivePropInput active_inputs[9];
    GeOriginalStageSetupRuntime setup_runtime={0};
    stagesetup authored_setup;
    PropRecord *viewer;
    ObjectRecord *modem;
    PropRecord *modem_prop;
    const u32 *command;
    size_t index;
    static const MixedEventKind expected[] = {
        MIXED_EVENT_MODEM,
        MIXED_EVENT_DOOR_2, MIXED_EVENT_DOOR_1,
        MIXED_EVENT_PROP, MIXED_EVENT_GLASS,
        MIXED_EVENT_GUARD_3, MIXED_EVENT_GUARD_2,
        MIXED_EVENT_GUARD_1, MIXED_EVENT_GUARD_0,
        MIXED_EVENT_VIEWER,
    };

    memset(&harness, 0, sizeof(harness));
    harness.stan.room = 135;
    g_CurrentSetup = UsetupdamZ;
    g_ActivePropsTail = g_ActivePropsHead = NULL;
    g_FreeProps = NULL;
    g_ClockTimer=1;g_GlobalTimer=60;g_GlobalTimerDelta=1.0f;

    viewer = chrpropAllocate();
    viewer->type = PROP_TYPE_VIEWER;
    viewer->stan = (StandTile *)&harness.stan;
    viewer->rooms[0] = harness.stan.room;
    viewer->rooms[1] = 0xffU;
    viewer->flags |= PROPFLAG_ONSCREEN;
    viewer->flags |= PROPFLAG_ENABLED;
    chrpropActivate(viewer);
    memset(&mixed_player,0,sizeof(mixed_player));
    mixed_player.prop=viewer;g_playerPointers[0]=&mixed_player;

    for (index = 0U; index <= MIXED_SECOND_DOOR_COMMAND; index++) {
        command = setup_command((s32)index);
        assert(command != NULL);
        if (index == MIXED_SECOND_DOOR_COMMAND
                || (command[0] & 0xffU) == PROPDEF_GLASS
                || (command[0] & 0xffU) == PROPDEF_PROP
                || (command[0] & 0xffU) == PROPDEF_DOOR)
            bind_command_stan(command,
                index == MIXED_SECOND_DOOR_COMMAND);
    }

    memset(&providers, 0, sizeof(providers));
    providers.context = &harness;
    providers.allocate_definition = allocate_definition;
    providers.allocate_prop = allocate_prop;
    providers.activate_prop = activate_prop;
    providers.enable_prop = enable_prop;
    providers.register_room = register_room;
    ge_dam_setup_world_materializer_bind(&providers, &harness.world);
    ge_dam_setup_world_materialize_first_authored();
    assert(harness.world.loaded
        && harness.world.definitions_materialized == 3U);
    assert(harness.world.first_glass.command_index == MIXED_GLASS_COMMAND);
    assert(harness.world.first_object.command_index == MIXED_PROP_COMMAND);
    assert(harness.world.first_door.command_index
        == MIXED_FIRST_DOOR_COMMAND);
    {
        const ObjectRecord *glass = harness.world.first_glass.definition;
        const ObjectRecord *object = harness.world.first_object.definition;
        const ObjectRecord *door = harness.world.first_door.definition;
        assert(glass != NULL && glass->type == PROPDEF_GLASS);
        assert(glass->type != PROPDEF_VEHICHLE
            && glass->type != PROPDEF_AIRCRAFT);
        assert(object != NULL && object->type == PROPDEF_PROP);
        assert(door != NULL && door->type == PROPDEF_DOOR);
    }
    assert(ge_dam_setup_world_materialize_linked_door());
    assert(harness.world.second_door.command_index
        == MIXED_SECOND_DOOR_COMMAND);
    assert(((const ObjectRecord *)harness.world.second_door.definition)->type
        == PROPDEF_DOOR);
    assert(ge_dam_setup_world_activate_entry(&harness.world.first_glass));
    assert(ge_dam_setup_world_activate_entry(&harness.world.first_object));
    assert(ge_dam_setup_world_activate_entry(&harness.world.first_door));
    assert(ge_dam_setup_world_activate_entry(&harness.world.second_door));

    for (index = 0U; index < MIXED_GUARD_COUNT; index++) {
        PropRecord *prop = chrpropAllocate();
        ChrRecord *chr = &harness.guards[index];
        const u32 *guard = setup_command(
            MIXED_FIRST_GUARD_COMMAND + (s32)index);
        u16 body;
        assert(guard != NULL && (guard[0] & 0xffU) == PROPDEF_GUARD);
        body = (u16)(guard[2] >> 16);
        assert(body == MIXED_GUARD_BODY && (s16)guard[5] == -1);
        memset(chr, 0, sizeof(*chr));
        prop->type = PROP_TYPE_CHR;
        prop->chr = chr;
        prop->stan = (StandTile *)&harness.stan;
        chr->prop = prop;
        chr->chrnum = (s16)(guard[1] >> 16);
        chr->model = (Model *)(uintptr_t)(index + 1U);
        chr->ailist = (AIRecord *)(void *)mixed_ai_list;
        chr->bodynum = (s8)body;
        chr->aioffset = (u16)guard[2];
        chrpropActivate(prop);
    }

    ge_original_covert_modem_object_reset();
    modem = ge_original_covert_modem_object_create(PROP_CHRBUG, ITEM_BUG);
    assert(modem != NULL && ge_original_covert_modem_object_prepare_throw(
        modem, 0U));
    modem_prop = modem->prop;
    assert(modem_prop != NULL && modem_prop->type == PROP_TYPE_WEAPON);
    memset(&harness.modem_projectile, 0,
           sizeof(harness.modem_projectile));
    harness.modem_projectile.obj = modem;
    modem->projectile = &harness.modem_projectile;
    modem->runtime_bitflags |= MIXED_MODEM_HAS_PROJECTILE
        | MIXED_MODEM_IS_RETICK;
    chrpropActivate(modem_prop);

    assert(harness.free_prop_index == MIXED_EXPECTED_PROP_COUNT);
    assert(ge_original_stage_active_props_tick_exact(&active_props)
           ==GE_ORIGINAL_STAGE_ACTIVE_PROP_NOT_BOUND);
    authored_setup=g_CurrentSetup;setup_runtime.setup=&authored_setup;
    setup_runtime.prop_record_count=329U;
    for(index=0U;index<MIXED_GUARD_COUNT;++index){
        active_inputs[index].command_index=MIXED_FIRST_GUARD_COMMAND+index;
        active_inputs[index].prop=harness.guards[index].prop;
        active_inputs[index].kind=GE_ORIGINAL_STAGE_ACTIVE_PROP_AUTHORED;
    }
    active_inputs[4]=(GeOriginalStageActivePropInput){
        MIXED_GLASS_COMMAND,harness.world.first_glass.prop,
        GE_ORIGINAL_STAGE_ACTIVE_PROP_AUTHORED};
    active_inputs[5]=(GeOriginalStageActivePropInput){
        MIXED_PROP_COMMAND,harness.world.first_object.prop,
        GE_ORIGINAL_STAGE_ACTIVE_PROP_AUTHORED};
    active_inputs[6]=(GeOriginalStageActivePropInput){
        MIXED_FIRST_DOOR_COMMAND,harness.world.first_door.prop,
        GE_ORIGINAL_STAGE_ACTIVE_PROP_AUTHORED};
    active_inputs[7]=(GeOriginalStageActivePropInput){
        MIXED_SECOND_DOOR_COMMAND,harness.world.second_door.prop,
        GE_ORIGINAL_STAGE_ACTIVE_PROP_AUTHORED};
    active_inputs[8]=(GeOriginalStageActivePropInput){
        SIZE_MAX,modem_prop,GE_ORIGINAL_STAGE_ACTIVE_PROP_DYNAMIC};
    assert(ge_original_stage_active_props_compose(&active_props,
        &setup_runtime,viewer,harness.guards,MIXED_GUARD_COUNT,
        active_inputs,9U)==GE_ORIGINAL_STAGE_ACTIVE_PROP_OK);
    assert(g_ActivePropsHead==viewer&&g_ActivePropsTail==modem_prop
           &&g_ChrSlots==harness.guards&&g_NumChrSlots==MIXED_GUARD_COUNT);
    {
    const u16 paused_ai_offset=harness.guards[0].aioffset;
    const unsigned paused_events=harness.event_count;
    g_ClockTimer=0;
    assert(ge_original_stage_active_props_pre_tick_exact(&active_props)
           ==GE_ORIGINAL_STAGE_ACTIVE_PROP_OK);
    assert(ge_original_stage_active_props_tick_exact(&active_props)
           ==GE_ORIGINAL_STAGE_ACTIVE_PROP_OK);
    assert(active_props.paused_ticks==1U&&active_props.ticks==0U
           &&harness.all_chr_ticks==0U&&harness.event_count==paused_events
           &&harness.guards[0].aioffset==paused_ai_offset);
    g_ClockTimer=1;g_GlobalTimerDelta=0.0f;
    assert(ge_original_stage_active_props_pre_tick_exact(&active_props)
           ==GE_ORIGINAL_STAGE_ACTIVE_PROP_OK);
    assert(ge_original_stage_active_props_tick_exact(&active_props)
           ==GE_ORIGINAL_STAGE_ACTIVE_PROP_OK);
    assert(active_props.paused_ticks==2U&&active_props.ticks==0U
           &&harness.all_chr_ticks==0U&&harness.event_count==paused_events
           &&harness.guards[0].aioffset==paused_ai_offset);
    g_GlobalTimerDelta=-1.0f;
    assert(ge_original_stage_active_props_tick_exact(&active_props)
           ==GE_ORIGINAL_STAGE_ACTIVE_PROP_TIMER_UNBOUND);
    g_GlobalTimerDelta=NAN;
    assert(ge_original_stage_active_props_tick_exact(&active_props)
           ==GE_ORIGINAL_STAGE_ACTIVE_PROP_TIMER_UNBOUND);
    g_ClockTimer=-1;g_GlobalTimerDelta=1.0f;
    assert(ge_original_stage_active_props_tick_exact(&active_props)
           ==GE_ORIGINAL_STAGE_ACTIVE_PROP_TIMER_UNBOUND);
    }
    g_ClockTimer=1;g_playerPointers[0]=NULL;
    assert(ge_original_stage_active_props_pre_tick_exact(&active_props)
           ==GE_ORIGINAL_STAGE_ACTIVE_PROP_PLAYER_UNBOUND);
    g_playerPointers[0]=&mixed_player;g_CurrentSetup.propDefs=NULL;
    assert(ge_original_stage_active_props_pre_tick_exact(&active_props)
           ==GE_ORIGINAL_STAGE_ACTIVE_PROP_SETUP_UNBOUND);
    g_CurrentSetup=*setup_runtime.setup;
    assert(ge_original_stage_active_props_pre_tick_exact(&active_props)
           ==GE_ORIGINAL_STAGE_ACTIVE_PROP_OK);
    assert(active_props.pre_tick_pending&&harness.all_chr_ticks==1U);
    assert(ge_original_stage_active_props_pre_tick_exact(&active_props)
           ==GE_ORIGINAL_STAGE_ACTIVE_PROP_OK);
    assert(active_props.pre_ticks==1U&&harness.all_chr_ticks==1U);
    assert(ge_original_stage_active_props_tick_exact(&active_props)
           ==GE_ORIGINAL_STAGE_ACTIVE_PROP_OK);
    assert(harness.event_count == sizeof(expected) / sizeof(expected[0]));
    assert(memcmp(harness.events, expected, sizeof(expected)) == 0);
    assert(harness.commits == MIXED_EXPECTED_PROP_COUNT);
    assert(harness.all_chr_ticks == 1U);
    assert(active_props.paused_ticks==2U&&active_props.pre_ticks==1U
           &&active_props.ticks==1U&&!active_props.pre_tick_pending);
    assert(harness.canonical_player_ticks == 1U);
    assert((viewer->flags & PROPFLAG_ONSCREEN) == 0U);
    assert(harness.canonical_door_ticks == 2U);
    assert(harness.global_tail_calls == 3U);

    /* Exact single-player playerTick only clears the viewer's onscreen bit, so
     * the separate MoveBond owner remains required. Exact objTick does own the
     * two doors, making the bounded door-runtime service mutually exclusive. */
    harness.separate_movement_ticks = 1U;
    harness.separate_door_ticks = 1U;
    assert(!exclusive_tick_ownership());
    harness.separate_door_ticks = 0U;
    assert(exclusive_tick_ownership());
    assert(harness.separate_movement_ticks == 1U);

    /* Exercise the canonical TICKOP_FREE list lifecycle, then keep the exact
     * mixed dispatcher running. A removed authored guard is not resurrected,
     * revisited, or included in later renderer matrix refreshes; viewer,
     * objects, doors, modem, and the three living guards continue ticking. */
    harness.remove_guard_zero = 1U;
    for (index = 0U; index < 4U; index++) {
        harness.event_count = 0U;
        assert(ge_original_stage_active_props_pre_tick_exact(&active_props)
               ==GE_ORIGINAL_STAGE_ACTIVE_PROP_OK);
        assert(ge_original_stage_active_props_tick_exact(&active_props)
               ==GE_ORIGINAL_STAGE_ACTIVE_PROP_OK);
        refresh_live_guard_matrices();
        assert(harness.event_count
                == (index == 0U ? 10U : index == 1U ? 10U : 9U));
        assert(harness.guard_zero_removed == 1U);
        if (index == 0U) {
            PropRecord *reused = harness.guards[0].prop;
            /* The exact shared pool may immediately reuse a freed guard prop
             * for another dynamic type. Keep the stale authored slot pointer
             * unchanged and prove later traversal treats this as smoke only. */
            reused->type = PROP_TYPE_SMOKE;
            reused->voidp = NULL;
            reused->flags = PROPFLAG_ENABLED;
            chrpropActivate(reused);
            harness.remove_glass = 1U;
        }
    }
    assert(harness.matrix_refreshes == 4U);
    assert(harness.all_chr_ticks == 5U);
    assert(harness.canonical_player_ticks == 5U);
    assert(harness.canonical_door_ticks == 10U);
    assert(harness.global_tail_calls == 15U);
    assert(harness.glass_removed == 1U);
    assert(active_props.pre_ticks==5U&&active_props.ticks==5U
           &&!active_props.pre_tick_pending);
    ge_original_stage_active_props_close(&active_props);

    puts("mixed exact propsTick: viewer + 4 guards + glass/prop + 2 doors + "
         "thrown modem visited tail-to-head once");
    puts("objTick frontier: static model instances, canonical projectile "
         "retick/physics, and exact door body remain required");
    puts("switch-over: retain separate MoveBond; disable the bounded door "
         "runtime in the frame exact objTick is installed");
    puts("lifecycle: canonical TICKOP_FREE removes one guard and one static "
         "object; reused guard prop ticks as smoke while four later mixed "
         "ticks/matrix refreshes retain only the three living guards");
    return 0;
}
