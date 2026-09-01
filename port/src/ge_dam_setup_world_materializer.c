#include "ge_original_dam_world.h"

#include <ultra64.h>
#include <bondtypes.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ge_original_dam_monitor.h"

extern stagesetup g_CurrentSetup;
static GeOriginalDamWorldProviders providers;
static GeOriginalDamWorldState *world;

typedef struct GeDamDefinitionHeader {
    void *definition;
    u16 extrascale;
    u8 state;
    u8 type;
    GeOriginalDamDoorSetup door;
} GeDamDefinitionHeader;

static GeDamDefinitionHeader definition_headers[16];
static size_t definition_header_count;
static TagObjectRecord mission_tags[2];

/* Native expansion of the exact N64 MonitorObjRecord prefix. Keeping the
 * ObjectRecord explicit makes the setup ABI identical on host builds where
 * the decomp's anonymous `inherits` extension is unavailable. */
typedef struct GeDamNativeMonitorDefinition {
    ObjectRecord object;
    MonitorRecord monitor;
    s32 owner_offset;
    s32 owner_part;
    s32 image_num;
} GeDamNativeMonitorDefinition;

_Static_assert(offsetof(GeDamNativeMonitorDefinition, monitor)
                   == sizeof(ObjectRecord),
               "monitor controller must follow the object prefix");
#ifdef GE_PORT_MS_INHERITS
_Static_assert(offsetof(GeDamNativeMonitorDefinition, monitor)
                   == offsetof(MonitorObjRecord, Monitor),
               "native monitor controller must match MonitorObjRecord");
_Static_assert(offsetof(GeDamNativeMonitorDefinition, image_num)
                   == offsetof(MonitorObjRecord, ImageNum),
               "native monitor image must match MonitorObjRecord");
#endif

#ifdef GE_PORT_MS_INHERITS
_Static_assert(offsetof(ObjectRecord, obj) == 0x04,
               "ObjectRecord must retain its canonical definition header");
_Static_assert(offsetof(ObjectRecord, prop) == 0x10,
               "ObjectRecord prop ABI must match canonical propobj.c");
_Static_assert(offsetof(ObjectRecord, model)
                   == offsetof(ObjectRecord, prop) + sizeof(void *),
               "ObjectRecord model must immediately follow its prop pointer");
_Static_assert(offsetof(ObjectRecord, runtime_bitflags)
                   == offsetof(ObjectRecord, runtime_pos) + sizeof(coord3d),
               "ObjectRecord runtime flags must follow its position");
_Static_assert(offsetof(WeaponObjRecord, weaponnum) == sizeof(ObjectRecord),
               "WeaponObjRecord tail must follow the canonical object prefix");
_Static_assert(offsetof(VehichleRecord, ailist) == sizeof(ObjectRecord),
               "VehichleRecord AI list must follow the canonical object prefix");
_Static_assert(offsetof(AircraftRecord, ailist) == sizeof(ObjectRecord),
               "AircraftRecord AI list must follow the canonical object prefix");
#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(ObjectRecord) == 0x80,
               "32-bit ObjectRecord must retain its canonical definition header");
_Static_assert(offsetof(ObjectRecord, model) == 0x14,
               "32-bit ObjectRecord model ABI must match canonical propobj.c");
_Static_assert(offsetof(ObjectRecord, runtime_bitflags) == 0x64,
               "32-bit ObjectRecord runtime ABI must match canonical propobj.c");
_Static_assert(offsetof(WeaponObjRecord, weaponnum) == 0x80,
               "32-bit WeaponObjRecord tail must match canonical propobj.c");
_Static_assert(offsetof(WeaponObjRecord, dualweapon) == 0x84,
               "32-bit WeaponObjRecord pointer ABI must match canonical propobj.c");
_Static_assert(offsetof(VehichleRecord, ailist) == 0x80,
               "32-bit VehichleRecord AI list ABI must match canonical chrai.c");
_Static_assert(offsetof(VehichleRecord, aioffset) == 0x84,
               "32-bit VehichleRecord AI offset must match canonical chrai.c");
_Static_assert(offsetof(AircraftRecord, ailist) == 0x80,
               "32-bit AircraftRecord AI list ABI must match canonical chrai.c");
_Static_assert(offsetof(AircraftRecord, aioffset) == 0x84,
               "32-bit AircraftRecord AI offset must match canonical chrai.c");
#endif

static void publish_definition_header(ObjectRecord *object, u16 extrascale,
                                      u8 state, u8 type)
{
    PropDefHeaderRecord *header = (PropDefHeaderRecord *)(void *)object;
    header->extrascale = extrascale;
    header->state = state;
    header->type = type;
}
#else
static void publish_definition_header(ObjectRecord *object, u16 extrascale,
                                      u8 state, u8 type)
{
    (void)object;
    (void)extrascale;
    (void)state;
    (void)type;
}
#endif

extern void set_parent_cur_tag_entry(TagObjectRecord *tag);

/* Generated setup arrays preserve N64 record widths; native host structures
 * can be wider because of pointers.  This is the explicit setup ABI table. */
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
    /* The authored US setup record includes five words after ObjectRecord. */
    case PROPDEF_TINTED_GLASS: return 37;
    case PROPDEF_CCTV: return 0x3b;
    case PROPDEF_MAGAZINE: return 0x21;
    case PROPDEF_COLLECTABLE: return 0x22;
    case PROPDEF_MONITOR: return 0x40;
    case PROPDEF_MULTI_MONITOR: return 0x95;
    case PROPDEF_AUTOGUN: return 0x36;
    case PROPDEF_LINK: return 3;
    case PROPDEF_GUARD_ATTRIBUTE: return 3;
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

int ge_dam_setup_world_validate_authored_stream(
    size_t *command_count, size_t *tinted_glass_count)
{
    const u32 *command = (const u32 *)g_CurrentSetup.propDefs;
    size_t index = 0U;
    size_t tinted = 0U;

    if (command_count != NULL) *command_count = 0U;
    if (tinted_glass_count != NULL) *tinted_glass_count = 0U;
    if (command == NULL) return 0;
    for (;;) {
        const u8 type = (u8)(command[0] & 0xffU);
        s32 words;
        if (index >= 304U && index <= 308U) {
            if (type != PROPDEF_TINTED_GLASS) return 0;
            tinted++;
        } else if (index == 309U && type != PROPDEF_TAG) {
            return 0;
        }
        if (type == PROPDEF_END) {
            if (index != 328U || tinted != 5U) return 0;
            if (command_count != NULL) *command_count = index;
            if (tinted_glass_count != NULL) *tinted_glass_count = tinted;
            return 1;
        }
        if (index >= 328U) return 0;
        words = setup_record_words(type);
        if (words <= 0) return 0;
        command += words;
        index++;
    }
}

void ge_dam_setup_world_materializer_bind(
    const GeOriginalDamWorldProviders *new_providers,
    GeOriginalDamWorldState *state)
{
    memset(&providers, 0, sizeof(providers));
    if (new_providers != NULL) providers = *new_providers;
    world = state;
    memset(definition_headers, 0, sizeof(definition_headers));
    memset(mission_tags, 0, sizeof(mission_tags));
    definition_header_count = 0U;
    if (world != NULL) {
        size_t index;
        memset(world, 0, sizeof(*world));
        world->first_glass.command_index = -1;
        world->first_object.command_index = -1;
        world->first_door.command_index = -1;
        world->second_door.command_index = -1;
        for (index = 0U; index < GE_ORIGINAL_DAM_SPAWN_WINDOW_COUNT;
                ++index)
            world->spawn_windows[index].command_index = -1;
    }
}

static int materialize_mission_tag_target(
    const u32 *tag_command, const u32 *object_command, s32 tag_index,
    s32 object_index, TagObjectRecord *tag,
    GeOriginalDamWorldEntry *entry)
{
    ObjectRecord *object;
    PropRecord *prop;
    const coord3d *pad_position = NULL;
    StandTile *pad_stan = NULL;
    s16 pad_id;
    u8 type;

    if (tag_command == NULL || object_command == NULL || tag == NULL
            || entry == NULL || providers.allocate_definition == NULL
            || providers.allocate_prop == NULL) return 0;
    if ((tag_command[0] & 0xffU) != PROPDEF_TAG) return 0;
    type = (u8)(object_command[0] & 0xffU);
    if (type != PROPDEF_MONITOR && type != PROPDEF_PROP) return 0;
    if ((s16)object_command[1] < 0) return 0;

    object = providers.allocate_definition(
        providers.context, type,
        type == PROPDEF_MONITOR ? sizeof(GeDamNativeMonitorDefinition)
                                : sizeof(ObjectRecord));
    if (object == NULL || definition_header_count
            >= sizeof(definition_headers) / sizeof(definition_headers[0]))
        return 0;
    memset(object, 0, sizeof(*object));
    definition_headers[definition_header_count].definition = object;
    definition_headers[definition_header_count].extrascale =
        (u16)(object_command[0] >> 16);
    definition_headers[definition_header_count].state =
        (u8)(object_command[0] >> 8);
    definition_headers[definition_header_count].type = type;
    publish_definition_header(object,
        definition_headers[definition_header_count].extrascale,
        definition_headers[definition_header_count].state, type);
    definition_header_count++;

    object->obj = (s16)(object_command[1] >> 16);
    object->pad = pad_id = (s16)object_command[1];
    object->flags = object_command[2];
    object->flags2 = object_command[3];
    object->runtime_bitflags = object_command[4];
    memcpy(&object->damage, &object_command[29], sizeof(object->damage));
    if (type == PROPDEF_MONITOR) {
        GeDamNativeMonitorDefinition *monitor_definition =
            (GeDamNativeMonitorDefinition *)(void *)object;
        monitor_definition->owner_offset = (s32)object_command[61];
        monitor_definition->owner_part = (s32)object_command[62];
        monitor_definition->image_num = (s32)object_command[63];
        if (ge_original_dam_monitor_initialize(
                &monitor_definition->monitor,
                monitor_definition->image_num)
                != GE_ORIGINAL_DAM_MONITOR_OK)
            return 0;
    }
    if (pad_id >= 10000 && g_CurrentSetup.boundpads != NULL) {
        const BoundPadRecord *boundpad =
            &g_CurrentSetup.boundpads[pad_id - 10000];
        pad_position = &boundpad->pos;
        pad_stan = boundpad->stan;
    } else if (g_CurrentSetup.pads != NULL) {
        const PadRecord *pad = &g_CurrentSetup.pads[pad_id];
        pad_position = &pad->pos;
        pad_stan = pad->stan;
    }
    if (pad_position == NULL || pad_stan == NULL) return 0;

    prop = providers.allocate_prop(providers.context, object);
    if (prop == NULL) return 0;
    memset(prop, 0, sizeof(*prop));
    prop->type = PROP_TYPE_OBJ;
    prop->obj = object;
    prop->pos = *pad_position;
    prop->stan = pad_stan;
    prop->rooms[0] = pad_stan->room;
    prop->rooms[1] = prop->rooms[2] = 0xffU;
    object->prop = prop;
    if (providers.register_room != NULL) {
        providers.register_room(providers.context, prop, (s16)pad_stan->room);
    }

    memset(tag, 0, sizeof(*tag));
    tag->ID = (u16)(tag_command[1] >> 16);
    tag->OffsetToObj = (s16)tag_command[1];
    tag->TaggedObject = object;
    /* Canonical RUNTIMEBITFLAG_TAGGED; BITFLAG enums are unavailable in the
     * GCC setup-data ABI build. */
    object->runtime_bitflags |= 0x00000010U;
    set_parent_cur_tag_entry(tag);

    memset(entry, 0, sizeof(*entry));
    entry->command_index = object_index;
    entry->propdef_type = type;
    entry->object_id = object->obj;
    entry->pad_id = pad_id;
    entry->flags = object->flags;
    entry->flags2 = object->flags2;
    memcpy(entry->position, prop->pos.f, sizeof(entry->position));
    entry->stan = prop->stan;
    entry->room = (s16)prop->stan->room;
    entry->definition = object;
    entry->prop = prop;
    (void)tag_index;
    return 1;
}

int ge_dam_setup_world_mission_monitor_snapshot(
    const GeOriginalDamWorldEntry *entry,
    GeOriginalDamMonitorSnapshot *snapshot)
{
    const GeDamNativeMonitorDefinition *monitor_definition;
    u8 type;
    if (entry == NULL || entry->definition == NULL || snapshot == NULL
            || !ge_dam_setup_world_definition_header(
                entry->definition, NULL, NULL, &type)
            || type != PROPDEF_MONITOR)
        return 0;
    monitor_definition = (const GeDamNativeMonitorDefinition *)
        entry->definition;
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->commands = monitor_definition->monitor.cmdlist;
    snapshot->owner_offset = monitor_definition->owner_offset;
    snapshot->owner_part = monitor_definition->owner_part;
    snapshot->image_num = monitor_definition->image_num;
    snapshot->command_offset = monitor_definition->monitor.offset;
    snapshot->pause60 = monitor_definition->monitor.pause60;
    snapshot->rotation = monitor_definition->monitor.rot;
    snapshot->xscale = monitor_definition->monitor.xscale;
    snapshot->yscale = monitor_definition->monitor.yscale;
    snapshot->xmid = monitor_definition->monitor.xmid;
    snapshot->ymid = monitor_definition->monitor.ymid;
    snapshot->red = monitor_definition->monitor.red;
    snapshot->green = monitor_definition->monitor.green;
    snapshot->blue = monitor_definition->monitor.blue;
    snapshot->alpha = monitor_definition->monitor.alpha;
    return 1;
}

int ge_dam_setup_world_mission_monitor_render_tick(
    const GeOriginalDamWorldEntry *entry, void *model_instance,
    GeOriginalDamMonitorRenderSnapshot *snapshot)
{
    GeDamNativeMonitorDefinition *monitor_definition;
    u8 type;
    if (entry == NULL || entry->definition == NULL || model_instance == NULL
            || snapshot == NULL
            || !ge_dam_setup_world_definition_header(
                entry->definition, NULL, NULL, &type)
            || type != PROPDEF_MONITOR)
        return 0;
    monitor_definition = (GeDamNativeMonitorDefinition *)entry->definition;
    return ge_original_dam_monitor_render_tick(
        model_instance, &monitor_definition->monitor,
        (uint32_t)monitor_definition->object.flags,
        (uint32_t)monitor_definition->object.flags2, snapshot);
}

int ge_dam_setup_world_materialize_mission_tags(
    GeOriginalDamMissionTagState *state)
{
    const u32 *command = (const u32 *)g_CurrentSetup.propDefs;
    const u32 *tag5 = NULL;
    const u32 *tag4 = NULL;
    const u32 *object5 = NULL;
    const u32 *object4 = NULL;
    s32 command_index = 0;

    if (state == NULL || command == NULL) return 0;
    memset(state, 0, sizeof(*state));
    while ((command[0] & 0xffU) != PROPDEF_END) {
        s32 words = setup_record_words((u8)(command[0] & 0xffU));
        if (words <= 0) return 0;
        if (command_index == 289) tag5 = command;
        if (command_index == 290) object5 = command;
        if (command_index == 291) tag4 = command;
        if (command_index == 292) object4 = command;
        command += words;
        command_index++;
    }
    if (tag5 == NULL || object5 == NULL || tag4 == NULL || object4 == NULL)
        return 0;
    if (!materialize_mission_tag_target(tag5, object5, 289, 290,
            &mission_tags[0], &state->tag5_object)) return 0;
    state->tags_registered++;
    if (!materialize_mission_tag_target(tag4, object4, 291, 292,
            &mission_tags[1], &state->tag4_object)) return 0;
    state->tags_registered++;
    state->loaded = 1;
    return 1;
}

static GeOriginalDamWorldEntry *target_for(u8 type)
{
    if (world == NULL) return NULL;
    if (type == PROPDEF_GLASS && world->first_glass.command_index < 0)
        return &world->first_glass;
    if (type == PROPDEF_PROP && world->first_object.command_index < 0)
        return &world->first_object;
    if (type == PROPDEF_DOOR && world->first_door.command_index < 0)
        return &world->first_door;
    return NULL;
}

void ge_dam_setup_world_materialize_first_authored(void)
{
    const u32 *command = (const u32 *)g_CurrentSetup.propDefs;
    s32 command_index = 0;

    if (command == NULL || world == NULL
            || providers.allocate_definition == NULL
            || providers.allocate_prop == NULL) return;

    while ((command[0] & 0xffU) != PROPDEF_END) {
        const u8 type = (u8)(command[0] & 0xffU);
        GeOriginalDamWorldEntry *entry;
        ObjectRecord *object;
        PropRecord *prop;
        const coord3d *pad_position = NULL;
        StandTile *pad_stan = NULL;
        s16 pad_id;
        s32 words;
        size_t bytes;

        words = setup_record_words(type);
        if (words <= 0) break;
        world->commands_scanned++;
        entry = target_for(type);
        if (entry != NULL) {
            bytes = type == PROPDEF_DOOR
                ? sizeof(DoorRecord) : sizeof(ObjectRecord);
            object = providers.allocate_definition(providers.context,
                                                   type, bytes);
            if (object == NULL) break;
            memset(object, 0, bytes);
            if (definition_header_count >= 3U) break;
            definition_headers[definition_header_count].definition = object;
            definition_headers[definition_header_count].extrascale =
                (u16)(command[0] >> 16);
            definition_headers[definition_header_count].state =
                (u8)(command[0] >> 8);
            definition_headers[definition_header_count].type = type;
            publish_definition_header(object,
                definition_headers[definition_header_count].extrascale,
                definition_headers[definition_header_count].state, type);
            if (type == PROPDEF_DOOR) {
                GeOriginalDamDoorSetup *door =
                    &definition_headers[definition_header_count].door;
                door->linked_door_offset = (s32)command[32];
                door->max_frac_fixed = command[33];
                door->perim_frac_fixed = command[34];
                door->accel_fixed = command[35];
                door->decel_fixed = command[36];
                door->max_speed_fixed = command[37];
                door->door_flags = (u16)(command[38] >> 16);
                door->door_type = (u16)command[38];
                door->key_flags = command[39];
                door->auto_close_frames = command[40];
                door->door_open_sound = command[41];
            }
            definition_header_count++;
            object->obj = (s16)(command[1] >> 16);
            object->pad = pad_id = (s16)command[1];
            object->flags = command[2];
            object->flags2 = command[3];
            object->runtime_bitflags = command[4];
            /* Original setup word 29 is the fixed-point damage field read
             * and converted in domakedefaultobj. Preserve its bit pattern. */
            memcpy(&object->damage, &command[29], sizeof(object->damage));
            if (pad_id >= 10000 && g_CurrentSetup.boundpads != NULL) {
                const BoundPadRecord *boundpad =
                    &g_CurrentSetup.boundpads[pad_id - 10000];
                pad_position = &boundpad->pos;
                pad_stan = boundpad->stan;
            } else if (pad_id >= 0 && g_CurrentSetup.pads != NULL) {
                const PadRecord *pad = &g_CurrentSetup.pads[pad_id];
                pad_position = &pad->pos;
                pad_stan = pad->stan;
            }
            if (pad_position == NULL || pad_stan == NULL) break;

            prop = providers.allocate_prop(providers.context, object);
            if (prop == NULL) break;
            memset(prop, 0, sizeof(*prop));
            prop->type = type == PROPDEF_DOOR ? PROP_TYPE_DOOR : PROP_TYPE_OBJ;
            prop->obj = object;
            prop->pos = *pad_position;
            prop->stan = pad_stan;
            prop->rooms[0] = pad_stan->room;
            prop->rooms[1] = prop->rooms[2] = 0xffU;
            object->prop = prop;
            if (providers.register_room != NULL) {
                providers.register_room(providers.context, prop,
                                        (s16)pad_stan->room);
                world->rooms_registered++;
            }
            entry->command_index = command_index;
            entry->propdef_type = type;
            entry->object_id = object->obj;
            entry->pad_id = pad_id;
            entry->flags = object->flags;
            entry->flags2 = object->flags2;
            memcpy(entry->position, prop->pos.f, sizeof(entry->position));
            entry->stan = prop->stan;
            entry->room = (s16)prop->stan->room;
            entry->definition = object;
            entry->prop = prop;
            world->definitions_materialized++;
            if (world->definitions_materialized == 3U) break;
        }
        command += words;
        command_index++;
    }
    world->loaded = world->definitions_materialized == 3U;
}

int ge_dam_setup_world_activate_entry(GeOriginalDamWorldEntry *entry)
{
    if (entry == NULL || entry->prop == NULL || entry->lifecycle_active
            || providers.activate_prop == NULL
            || providers.enable_prop == NULL) return 0;
    providers.activate_prop(providers.context, entry->prop);
    providers.enable_prop(providers.context, entry->prop);
    entry->lifecycle_active = 1;
    return 1;
}

int ge_dam_setup_world_definition_header(const void *definition,
                                          uint16_t *extrascale,
                                          uint8_t *state,
                                          uint8_t *type)
{
    size_t index;

    if (definition == NULL) return 0;
    for (index = 0U; index < definition_header_count; index++) {
        if (definition_headers[index].definition == definition) {
            if (extrascale != NULL)
                *extrascale = definition_headers[index].extrascale;
            if (state != NULL) *state = definition_headers[index].state;
            if (type != NULL) *type = definition_headers[index].type;
            return 1;
        }
    }
    return 0;
}

int ge_dam_setup_world_door_setup(const void *definition,
                                  GeOriginalDamDoorSetup *setup)
{
    size_t index;
    if (definition == NULL || setup == NULL) return 0;
    for (index = 0U; index < definition_header_count; index++) {
        if (definition_headers[index].definition == definition
                && definition_headers[index].type == PROPDEF_DOOR) {
            *setup = definition_headers[index].door;
            return 1;
        }
    }
    return 0;
}

int ge_dam_setup_world_materialize_linked_door(void)
{
    const u32 *command = (const u32 *)g_CurrentSetup.propDefs;
    GeOriginalDamWorldEntry *entry;
    ObjectRecord *object;
    PropRecord *prop;
    const BoundPadRecord *boundpad;
    GeOriginalDamDoorSetup *door;
    s32 command_index = 0;

    if (world == NULL || command == NULL
            || world->second_door.command_index >= 0
            || providers.allocate_definition == NULL
            || providers.allocate_prop == NULL) return 0;
    while ((command[0] & 0xffU) != PROPDEF_END && command_index < 268) {
        s32 words = setup_record_words((u8)(command[0] & 0xffU));
        if (words <= 0) return 0;
        command += words;
        command_index++;
    }
    if (command_index != 268 || (command[0] & 0xffU) != PROPDEF_DOOR
            || (s16)command[1] < 0
            || g_CurrentSetup.boundpads == NULL
            || definition_header_count >= 6U) return 0;

    object = providers.allocate_definition(
        providers.context, PROPDEF_DOOR, sizeof(DoorRecord));
    if (object == NULL) return 0;
    memset(object, 0, sizeof(DoorRecord));
    definition_headers[definition_header_count].definition = object;
    definition_headers[definition_header_count].extrascale =
        (u16)(command[0] >> 16);
    definition_headers[definition_header_count].state =
        (u8)(command[0] >> 8);
    definition_headers[definition_header_count].type = PROPDEF_DOOR;
    publish_definition_header(object,
        definition_headers[definition_header_count].extrascale,
        definition_headers[definition_header_count].state, PROPDEF_DOOR);
    door = &definition_headers[definition_header_count].door;
    door->linked_door_offset = (s32)command[32];
    door->max_frac_fixed = command[33];
    door->perim_frac_fixed = command[34];
    door->accel_fixed = command[35];
    door->decel_fixed = command[36];
    door->max_speed_fixed = command[37];
    door->door_flags = (u16)(command[38] >> 16);
    door->door_type = (u16)command[38];
    door->key_flags = command[39];
    door->auto_close_frames = command[40];
    door->door_open_sound = command[41];
    definition_header_count++;

    object->obj = (s16)(command[1] >> 16);
    object->pad = (s16)command[1];
    object->flags = command[2];
    object->flags2 = command[3];
    object->runtime_bitflags = command[4];
    memcpy(&object->damage, &command[29], sizeof(object->damage));
    boundpad = &g_CurrentSetup.boundpads[object->pad];
    if (boundpad->stan == NULL) return 0;
    prop = providers.allocate_prop(providers.context, object);
    if (prop == NULL) return 0;
    memset(prop, 0, sizeof(*prop));
    prop->type = PROP_TYPE_DOOR;
    prop->obj = object;
    prop->pos = boundpad->pos;
    prop->stan = boundpad->stan;
    prop->rooms[0] = boundpad->stan->room;
    prop->rooms[1] = prop->rooms[2] = 0xffU;
    object->prop = prop;
    if (providers.register_room != NULL) {
        providers.register_room(providers.context, prop,
                                (s16)boundpad->stan->room);
        world->rooms_registered++;
    }
    entry = &world->second_door;
    entry->command_index = command_index;
    entry->propdef_type = PROPDEF_DOOR;
    entry->object_id = object->obj;
    entry->pad_id = object->pad;
    entry->flags = object->flags;
    entry->flags2 = object->flags2;
    memcpy(entry->position, prop->pos.f, sizeof(entry->position));
    entry->stan = prop->stan;
    entry->room = (s16)prop->stan->room;
    entry->definition = object;
    entry->prop = prop;
    world->definitions_materialized++;
    return 1;
}

size_t ge_dam_setup_world_materialize_spawn_windows(void)
{
    const u32 *command = (const u32 *)g_CurrentSetup.propDefs;
    s32 command_index = 0;
    size_t materialized = 0U;

    if (world == NULL || command == NULL
            || providers.allocate_definition == NULL
            || providers.allocate_prop == NULL) return 0U;

    while ((command[0] & 0xffU) != PROPDEF_END && command_index <= 121) {
        const u8 type = (u8)(command[0] & 0xffU);
        const s32 words = setup_record_words(type);
        if (words <= 0) return materialized;
        if (command_index >= 117 && command_index <= 121) {
            GeOriginalDamWorldEntry *entry =
                &world->spawn_windows[command_index - 117];
            ObjectRecord *object;
            PropRecord *prop;
            const BoundPadRecord *boundpad;
            const s16 pad_id = (s16)command[1];

            if (entry->command_index >= 0) {
                materialized++;
                command += words;
                command_index++;
                continue;
            }
            /* These invariants are authored by UsetupdamZ, not inferred by
             * the platform: model 104 at bound pads 10090--10094. */
            if (type != PROPDEF_GLASS
                    || (s16)(command[1] >> 16) != 104
                    || pad_id != 10090 + (command_index - 117)
                    || g_CurrentSetup.boundpads == NULL
                    || definition_header_count
                        >= sizeof(definition_headers)
                            / sizeof(definition_headers[0]))
                return materialized;
            boundpad = &g_CurrentSetup.boundpads[pad_id - 10000];
            if (boundpad->stan == NULL) return materialized;
            object = providers.allocate_definition(
                providers.context, type, sizeof(ObjectRecord));
            if (object == NULL) return materialized;
            memset(object, 0, sizeof(*object));
            definition_headers[definition_header_count].definition = object;
            definition_headers[definition_header_count].extrascale =
                (u16)(command[0] >> 16);
            definition_headers[definition_header_count].state =
                (u8)(command[0] >> 8);
            definition_headers[definition_header_count].type = type;
            publish_definition_header(object,
                definition_headers[definition_header_count].extrascale,
                definition_headers[definition_header_count].state, type);
            definition_header_count++;

            object->obj = (s16)(command[1] >> 16);
            object->pad = pad_id;
            object->flags = command[2];
            object->flags2 = command[3];
            object->runtime_bitflags = command[4];
            memcpy(&object->damage, &command[29], sizeof(object->damage));
            prop = providers.allocate_prop(providers.context, object);
            if (prop == NULL) return materialized;
            memset(prop, 0, sizeof(*prop));
            prop->type = PROP_TYPE_OBJ;
            prop->obj = object;
            prop->pos = boundpad->pos;
            prop->stan = boundpad->stan;
            prop->rooms[0] = boundpad->stan->room;
            prop->rooms[1] = prop->rooms[2] = 0xffU;
            object->prop = prop;
            if (providers.register_room != NULL) {
                providers.register_room(providers.context, prop,
                                        (s16)boundpad->stan->room);
                world->rooms_registered++;
            }
            entry->command_index = command_index;
            entry->propdef_type = type;
            entry->object_id = object->obj;
            entry->pad_id = pad_id;
            entry->flags = object->flags;
            entry->flags2 = object->flags2;
            memcpy(entry->position, prop->pos.f, sizeof(entry->position));
            entry->stan = prop->stan;
            entry->room = (s16)prop->stan->room;
            entry->definition = object;
            entry->prop = prop;
            world->definitions_materialized++;
            materialized++;
        }
        command += words;
        command_index++;
    }
    return materialized;
}

int ge_dam_setup_world_link_authored_doors(void)
{
    GeOriginalDamDoorSetup first_setup;
    GeOriginalDamDoorSetup second_setup;

    if (world == NULL || world->first_door.command_index < 0
            || world->second_door.command_index < 0
            || world->first_door.definition == NULL
            || world->second_door.definition == NULL
            || !ge_dam_setup_world_door_setup(
                world->first_door.definition, &first_setup)
            || !ge_dam_setup_world_door_setup(
                world->second_door.definition, &second_setup)) return 0;
    if (world->first_door.command_index + first_setup.linked_door_offset
                != world->second_door.command_index
            || world->second_door.command_index
                    + second_setup.linked_door_offset
                != world->first_door.command_index) return 0;
    return 1;
}

int ge_dam_setup_world_definition_set_state(void *definition, uint8_t state)
{
    size_t index;

    if (definition == NULL) return 0;
    for (index = 0U; index < definition_header_count; index++) {
        if (definition_headers[index].definition == definition) {
            definition_headers[index].state = state;
            publish_definition_header((ObjectRecord *)definition,
                definition_headers[index].extrascale, state,
                definition_headers[index].type);
            return 1;
        }
    }
    return 0;
}
