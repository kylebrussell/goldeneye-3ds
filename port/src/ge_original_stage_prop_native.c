#include <ultra64.h>
#include <bondtypes.h>

#include "ge_original_stage_prop_materializer.h"

#include <stddef.h>
#include <string.h>

_Static_assert(offsetof(ObjectRecord, obj) == sizeof(PropDefHeaderRecord),
    "native stage definitions require promoted inherited headers");
_Static_assert(offsetof(DoorRecord, linkedDoorOffset) == sizeof(ObjectRecord),
    "native door tail must immediately follow ObjectRecord");
_Static_assert(offsetof(KeyRecord, keyflags) == sizeof(ObjectRecord),
    "native key tail must immediately follow ObjectRecord");
_Static_assert(offsetof(WeaponObjRecord, weaponnum) == sizeof(ObjectRecord),
    "native collectable tail must immediately follow ObjectRecord");
_Static_assert(offsetof(MonitorObjRecord, Monitor) == sizeof(ObjectRecord),
    "native monitor controller must immediately follow ObjectRecord");
_Static_assert(offsetof(MultiMonitorObjRecord, Monitor) == sizeof(ObjectRecord),
    "native multi-monitor controllers must immediately follow ObjectRecord");
_Static_assert(offsetof(CCTVRecord, cctv_lookpad) == sizeof(ObjectRecord),
    "native CCTV tail must immediately follow ObjectRecord");
_Static_assert(offsetof(AutogunRecord, padID) == sizeof(ObjectRecord),
    "native autogun tail must immediately follow ObjectRecord");
_Static_assert(offsetof(AmmoCrateRecord, ammoType) == sizeof(ObjectRecord),
    "native magazine tail must immediately follow ObjectRecord");
_Static_assert(offsetof(MultiAmmoCrateRecord, slots) == sizeof(ObjectRecord),
    "native multi-ammo tail must immediately follow ObjectRecord");
_Static_assert(offsetof(BodyArmourRecord, initialamount) == sizeof(ObjectRecord),
    "native armour tail must immediately follow ObjectRecord");
_Static_assert(offsetof(VehichleRecord, ailist) == sizeof(ObjectRecord),
    "native vehicle tail must immediately follow ObjectRecord");
_Static_assert(offsetof(AircraftRecord, ailist) == sizeof(ObjectRecord),
    "native aircraft tail must immediately follow ObjectRecord");
_Static_assert(offsetof(SafeRecord, normal) == sizeof(ObjectRecord),
    "native safe tail must immediately follow ObjectRecord");
_Static_assert(offsetof(TankRecord, collision) == sizeof(ObjectRecord),
    "native tank tail must immediately follow ObjectRecord");
_Static_assert(offsetof(SafeObjectRecord, Index1)
        >= sizeof(PropDefHeaderRecord),
    "native safe-item relation must follow its header");

size_t ge_original_stage_prop_native_definition_size(
    const GeOriginalStagePropConstructionRequest *request)
{
    if (request == NULL || request->record == NULL
            || (request->record->type != PROPDEF_SAFE_ITEM
                && request->service
                    != GE_ORIGINAL_STAGE_PROP_SERVICE_DEFAULT_OBJECT
                && request->service != GE_ORIGINAL_STAGE_PROP_SERVICE_DOOR
                && request->service != GE_ORIGINAL_STAGE_PROP_SERVICE_ITEM
                && request->service
                    != GE_ORIGINAL_STAGE_PROP_SERVICE_SPECIAL_OBJECT))
        return 0U;
    switch (request->record->type) {
    case PROPDEF_PROP: case PROPDEF_GLASS: case PROPDEF_HAT:
    case PROPDEF_ALARM: case PROPDEF_RACK: case PROPDEF_GAS_RELEASING:
        return sizeof(ObjectRecord);
    case PROPDEF_SAFE: return sizeof(SafeRecord);
    case PROPDEF_VEHICHLE: return sizeof(VehichleRecord);
    case PROPDEF_AIRCRAFT: return sizeof(AircraftRecord);
    case PROPDEF_TANK: return sizeof(TankRecord);
    case PROPDEF_SAFE_ITEM: return sizeof(SafeObjectRecord);
    case PROPDEF_KEY: return sizeof(KeyRecord);
    case PROPDEF_COLLECTABLE: return sizeof(WeaponObjRecord);
    case PROPDEF_DOOR: return sizeof(DoorRecord);
    case PROPDEF_MONITOR: return sizeof(MonitorObjRecord);
    case PROPDEF_MULTI_MONITOR: return sizeof(MultiMonitorObjRecord);
    case PROPDEF_TINTED_GLASS: return sizeof(TintedGlassRecord);
    case PROPDEF_CCTV: return sizeof(CCTVRecord);
    case PROPDEF_AUTOGUN: return sizeof(AutogunRecord);
    case PROPDEF_MAGAZINE: return sizeof(AmmoCrateRecord);
    case PROPDEF_AMMO: return sizeof(MultiAmmoCrateRecord);
    case PROPDEF_ARMOUR: return sizeof(BodyArmourRecord);
    default: return 0U;
    }
}

int ge_original_stage_prop_native_definition_init(
    const GeOriginalStagePropConstructionRequest *request,
    void *definition, size_t definition_size)
{
    ObjectRecord *object = definition;
    PropDefHeaderRecord *header;
    const uint32_t *words;
    if (request == NULL || request->record == NULL || definition == NULL
            || definition_size
                < ge_original_stage_prop_native_definition_size(request)
            || request->record->word_count < 5U) return 0;
    words = request->record->words;
    /* Every native tail contains runtime-owned pointers/state which the ROM
     * setup stream serializes as zero.  Clear the complete promoted record so
     * callers are not required to provide calloc-backed storage. */
    memset(definition, 0,
        ge_original_stage_prop_native_definition_size(request));
    header = (PropDefHeaderRecord *)(void *)object;
    header->extrascale = (uint16_t)(words[0] >> 16U);
    header->state = (uint8_t)(words[0] >> 8U);
    header->type = (uint8_t)words[0];
    if (request->record->type == PROPDEF_SAFE_ITEM) {
        SafeObjectRecord *relation = definition;
        if (request->record->word_count != 5U) return 0;
        relation->Index1 = (int32_t)words[1];
        relation->Index2 = (int32_t)words[2];
        relation->Index3 = (int32_t)words[3];
        if (words[4] != 0U) return 0;
        relation->next = NULL;
        return 1;
    }
    if (request->record->word_count < 32U) return 0;
    object->obj = (int16_t)(words[1] >> 16U);
    object->pad = (int16_t)words[1];
    object->flags = words[2];
    object->flags2 = words[3];
    object->runtime_bitflags = words[25];
    memcpy(&object->maxdamage, &words[28], sizeof(object->maxdamage));
    memcpy(&object->damage, &words[29], sizeof(object->damage));
    memcpy(&object->shadecol, &words[30], sizeof(object->shadecol));
    memcpy(&object->nextcol, &words[31], sizeof(object->nextcol));
    switch (request->record->type) {
    case PROPDEF_DOOR:
    {
        DoorRecord *door = definition;
        if (request->record->word_count != 64U) return 0;
        door->linkedDoorOffset = (int32_t)words[32];
        memcpy(&door->maxFrac, &words[33], sizeof(door->maxFrac));
        memcpy(&door->perimFrac, &words[34], sizeof(door->perimFrac));
        memcpy(&door->accel, &words[35], sizeof(door->accel));
        memcpy(&door->decel, &words[36], sizeof(door->decel));
        memcpy(&door->maxSpeed, &words[37], sizeof(door->maxSpeed));
        door->doorFlags = (uint16_t)(words[38] >> 16U);
        door->doorType = (uint16_t)words[38];
        door->keyflags = words[39];
        door->autoCloseFrames = words[40];
        door->doorOpenSound = words[41];
        memcpy(&door->frac, &words[42], sizeof(door->frac));
        memcpy(&door->unkac, &words[43], sizeof(door->unkac));
        memcpy(&door->unkb0, &words[44], sizeof(door->unkb0));
        memcpy(&door->openPosition, &words[45], sizeof(door->openPosition));
        memcpy(&door->speed, &words[46], sizeof(door->speed));
        door->openstate = (int8_t)(words[47] >> 24U);
        door->unkbd = (int8_t)(words[47] >> 16U);
        door->calculatedopacity = (int16_t)words[47];
        door->TintDist = (int32_t)words[48];
        door->CullDist = (int16_t)(words[49] >> 16U);
        door->soundType = (int8_t)(words[49] >> 8U);
        door->fadeTime60 = (int8_t)words[49];
        /* Serialized runtime pointers/bbox/timers must remain NULL/zero until
         * setupDoor/doorInit publishes their native graph. */
        for (size_t word = 50U; word < 64U; ++word)
            if (words[word] != 0U) return 0;
        break;
    }
    case PROPDEF_KEY:
        if (request->record->word_count != 33U) return 0;
        ((KeyRecord *)definition)->keyflags = words[32];
        break;
    case PROPDEF_COLLECTABLE:
    {
        WeaponObjRecord *weapon = definition;
        if (request->record->word_count != 34U) return 0;
        weapon->weaponnum = (int8_t)(words[32] >> 24U);
        weapon->LinkedWeaponType = (int8_t)(words[32] >> 16U);
        weapon->timer = (int16_t)words[32];
        /* The serialized dualweapon slot is a canonical NULL runtime pointer.
         * A nonzero value would require a relocated native relation. */
        if (words[33] != 0U) return 0;
        weapon->dualweapon = NULL;
        break;
    }
    case PROPDEF_TINTED_GLASS:
    {
        TintedGlassRecord *glass = definition;
        if (request->record->word_count != 37U) return 0;
        glass->TintDist = (int32_t)words[32];
        glass->CullDist = (int32_t)words[33];
        glass->calculatedopacity = (int32_t)words[34];
        glass->portalnum = (int32_t)words[35];
        memcpy(&glass->unk90, &words[36], sizeof(glass->unk90));
        break;
    }
    case PROPDEF_MONITOR:
    {
        MonitorObjRecord *monitor = definition;
        size_t word;
        if (request->record->word_count != 64U) return 0;
        for (word = 32U; word < 61U; ++word)
            if (words[word] != 0U) return 0;
        monitor->OwnerOffset = (int32_t)words[61];
        monitor->OwnerPart = (int32_t)words[62];
        monitor->ImageNum = (int32_t)words[63];
        break;
    }
    case PROPDEF_MULTI_MONITOR:
    {
        MultiMonitorObjRecord *monitor = definition;
        size_t word;
        if (request->record->word_count != 149U) return 0;
        for (word = 32U; word < 148U; ++word)
            if (words[word] != 0U) return 0;
        monitor->ImageNums[0] = (uint8_t)(words[148] >> 24U);
        monitor->ImageNums[1] = (uint8_t)(words[148] >> 16U);
        monitor->ImageNums[2] = (uint8_t)(words[148] >> 8U);
        monitor->ImageNums[3] = (uint8_t)words[148];
        break;
    }
    case PROPDEF_CCTV:
    {
        CCTVRecord *cctv = definition;
        if (request->record->word_count != 59U) return 0;
        cctv->cctv_lookpad = (int32_t)words[32];
        for (size_t row = 0U; row < 4U; ++row)
            for (size_t column = 0U; column < 4U; ++column)
                memcpy(&cctv->unk84.m[row][column],
                    &words[33U + row * 4U + column], sizeof(float));
        memcpy(&cctv->unkC4, &words[49], sizeof(float));
        memcpy(&cctv->unkC8, &words[50], sizeof(float));
        memcpy(&cctv->unkCC, &words[51], sizeof(float));
        memcpy(&cctv->unkD0, &words[52], sizeof(float));
        cctv->unkD4 = (int32_t)words[53];
        memcpy(&cctv->unkD8, &words[54], sizeof(float));
        memcpy(&cctv->unkDC, &words[55], sizeof(float));
        cctv->timer = (int32_t)words[56];
        cctv->convert_to_f32 = (int32_t)words[57];
        memcpy(&cctv->unkE8, &words[58], sizeof(float));
        break;
    }
    case PROPDEF_AUTOGUN:
    {
        AutogunRecord *autogun = definition;
        if (request->record->word_count != 54U) return 0;
        autogun->padID = (int32_t)words[32];
        memcpy(&autogun->rot_related, &words[33], sizeof(float));
        memcpy(&autogun->unk88, &words[34], sizeof(float));
        memcpy(&autogun->unk8C, &words[35], sizeof(float));
        memcpy(&autogun->unk90, &words[36], sizeof(float));
        memcpy(&autogun->unk94, &words[37], sizeof(float));
        memcpy(&autogun->unk98, &words[38], sizeof(float));
        memcpy(&autogun->unk9C, &words[39], sizeof(float));
        memcpy(&autogun->unkA0, &words[40], sizeof(float));
        memcpy(&autogun->speed, &words[41], sizeof(float));
        memcpy(&autogun->aimdist, &words[42], sizeof(float));
        autogun->unkAC = (int32_t)words[43];
        memcpy(&autogun->unkB0, &words[44], sizeof(float));
        memcpy(&autogun->unkB4, &words[45], sizeof(float));
        autogun->unkB8 = (int32_t)words[46];
        autogun->unkBC = (int32_t)words[47];
        autogun->unkC0 = (int32_t)words[48];
        if (words[49] != 0U || words[50] != 0U || words[51] != 0U)
            return 0;
        autogun->unkC4 = NULL;
        autogun->unkC8 = NULL;
        autogun->beam = NULL;
        autogun->is_active = (int32_t)words[52];
        memcpy(&autogun->unkD4, &words[53], sizeof(float));
        break;
    }
    case PROPDEF_MAGAZINE:
        if (request->record->word_count != 33U) return 0;
        ((AmmoCrateRecord *)definition)->ammoType = (AMMOTYPE)words[32];
        break;
    case PROPDEF_AMMO:
    {
        MultiAmmoCrateRecord *ammo = definition;
        size_t slot;
        if (request->record->word_count != 45U
                || AMMOTYPE_GLOBAL_MAX != 13) return 0;
        for (slot = 0U; slot < AMMOTYPE_GLOBAL_MAX; ++slot) {
            ammo->slots[slot].modelnum = (uint16_t)(words[32U + slot] >> 16U);
            ammo->slots[slot].quantity = (uint16_t)words[32U + slot];
        }
        break;
    }
    case PROPDEF_ARMOUR:
    {
        BodyArmourRecord *armour = definition;
        if (request->record->word_count != 34U) return 0;
        memcpy(&armour->initialamount, &words[32], sizeof(float));
        memcpy(&armour->amount, &words[33], sizeof(float));
        break;
    }
    case PROPDEF_VEHICHLE:
    {
        VehichleRecord *vehicle = definition;
        if (request->record->word_count != 44U) return 0;
        vehicle->ailist = (AIRecord *)(uintptr_t)words[32];
        vehicle->aioffset = (uint16_t)(words[33] >> 16U);
        vehicle->aireturnlist = (int16_t)words[33];
        memcpy(&vehicle->speed, &words[34], sizeof(float));
        memcpy(&vehicle->wheelxrot, &words[35], sizeof(float));
        memcpy(&vehicle->wheelyrot, &words[36], sizeof(float));
        memcpy(&vehicle->speedaim, &words[37], sizeof(float));
        memcpy(&vehicle->speedtime60, &words[38], sizeof(float));
        memcpy(&vehicle->turnrot60, &words[39], sizeof(float));
        memcpy(&vehicle->roty, &words[40], sizeof(float));
        vehicle->path = (PathRecord *)(uintptr_t)words[41];
        vehicle->nextstep = (int32_t)words[42];
        vehicle->Sound = (struct ALSoundState *)(uintptr_t)words[43];
        break;
    }
    case PROPDEF_AIRCRAFT:
    {
        AircraftRecord *aircraft = definition;
        if (request->record->word_count != 45U) return 0;
        aircraft->ailist = (AIRecord *)(uintptr_t)words[32];
        aircraft->aioffset = (uint16_t)(words[33] >> 16U);
        aircraft->aireturnlist = (int16_t)words[33];
        memcpy(&aircraft->rotoryrot, &words[34], sizeof(float));
        memcpy(&aircraft->rotaryspeed, &words[35], sizeof(float));
        memcpy(&aircraft->rotaryspeedaim, &words[36], sizeof(float));
        memcpy(&aircraft->rotaryspeedtime, &words[37], sizeof(float));
        memcpy(&aircraft->speed, &words[38], sizeof(float));
        memcpy(&aircraft->speedaim, &words[39], sizeof(float));
        memcpy(&aircraft->speedtime60, &words[40], sizeof(float));
        memcpy(&aircraft->yrot, &words[41], sizeof(float));
        aircraft->nextstep = (int32_t)words[42];
        aircraft->path = (PathRecord *)(uintptr_t)words[43];
        aircraft->Sound = (struct ALSoundState *)(uintptr_t)words[44];
        break;
    }
    case PROPDEF_TANK:
    {
        TankRecord *tank = definition;
        if (request->record->word_count != 56U) return 0;
        tank->collision = (collision_data *)(uintptr_t)words[32];
        memcpy(tank->rect.f, &words[33], sizeof(tank->rect.f));
        tank->unkA4 = (int32_t)words[41];
        tank->unkA8 = (int32_t)words[42];
        tank->unkAC = (int32_t)words[43];
        tank->unkB0 = (int32_t)words[44];
        tank->unkB4 = (int32_t)words[45];
        tank->unkB8 = (int32_t)words[46];
        tank->unkBC = (int32_t)words[47];
        tank->unkC0 = (int32_t)words[48];
        tank->is_firing_tank = (int32_t)words[49];
        memcpy(&tank->turret_vertical_angle, &words[50], sizeof(float));
        memcpy(&tank->turret_orientation_angle, &words[51], sizeof(float));
        memcpy(&tank->unkD0, &words[52], sizeof(float));
        memcpy(&tank->stan_y, &words[53], sizeof(float));
        tank->unkD8 = (int32_t)words[54];
        memcpy(&tank->tank_orientation_angle, &words[55], sizeof(float));
        break;
    }
    case PROPDEF_PROP: case PROPDEF_GLASS: case PROPDEF_HAT:
    case PROPDEF_ALARM: case PROPDEF_RACK: case PROPDEF_GAS_RELEASING:
    case PROPDEF_SAFE:
        break;
    default: return 0;
    }
    return 1;
}

int ge_original_stage_prop_native_definition_header(
    const void *definition, uint16_t *extrascale,
    uint8_t *state, uint8_t *type)
{
    const PropDefHeaderRecord *header = definition;
    if (header == NULL) return 0;
    if (extrascale != NULL) *extrascale = header->extrascale;
    if (state != NULL) *state = header->state;
    if (type != NULL) *type = header->type;
    return 1;
}

int ge_original_stage_prop_native_definition_set_state(
    void *definition, uint8_t state)
{
    PropDefHeaderRecord *header = definition;
    if (header == NULL) return 0;
    header->state = state;
    return 1;
}

size_t ge_original_stage_prop_native_prop_size(void)
{
    return sizeof(PropRecord);
}

int ge_original_stage_prop_native_bind_prop(
    const GeOriginalStagePropConstructionRequest *request,
    void *definition, void *opaque_prop, size_t prop_size)
{
    ObjectRecord *object = definition;
    PropRecord *prop = opaque_prop;
    if (request == NULL || !request->placement_resolved
            || request->placement.stan == NULL || definition == NULL
            || request->placement.room < 0
            || request->placement.room >= UINT8_MAX
            || prop == NULL || prop_size < sizeof(*prop)) return 0;
    memset(prop, 0, sizeof(*prop));
    prop->type = request->record->type == PROPDEF_DOOR
        ? PROP_TYPE_DOOR : PROP_TYPE_OBJ;
    prop->obj = object;
    memcpy(prop->pos.f, request->placement.position, sizeof(prop->pos.f));
    prop->stan = request->placement.stan;
    prop->rooms[0] = (uint8_t)request->placement.room;
    prop->rooms[1] = prop->rooms[2] = UINT8_MAX;
    object->prop = prop;
    return 1;
}
