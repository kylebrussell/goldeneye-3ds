#include "ge_original_covert_modem_object.h"

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <string.h>

#include "ge_original_bug_model.h"
#include "ge_original_default_object_internal.h"

#define GE_ORIGINAL_WEAPON_SLOT_CAPACITY 30U

/* Keep one concrete native record per canonical slot.  This is the decomp's
 * WeaponObjRecord layout expressed without its `inherits ObjectRecord`
 * extension: the inherited ObjectRecord (including PropDefHeaderRecord) is
 * followed by the exact collectable fields. */
typedef struct GeNativeWeaponObject {
    ObjectRecord object;
    s8 weaponnum;
    s8 linked_weapon_type;
    s16 timer;
    struct GeNativeWeaponObject *dualweapon;
#if !defined(GE_PORT_MS_INHERITS)
    /* Legacy host builds expand `inherits` to a declaration with no storage.
     * This test-only sidecar preserves observable header metadata without
     * changing the byte-exact live ARM record above. */
    u16 host_extrascale;
    u8 host_state;
    u8 host_type;
#endif
} GeNativeWeaponObject;

/* The original pool has one non-animated model slot available for every
 * weapon slot. Runtime-data storage is unnecessary for Pchrbug: its canonical
 * header has zero runtime records and its graph contains only GROUP and DL. */
static Model ge_covert_modem_model_slots[GE_ORIGINAL_WEAPON_SLOT_CAPACITY];
static GeNativeWeaponObject
    ge_covert_modem_weapon_slots[GE_ORIGINAL_WEAPON_SLOT_CAPACITY];
static GeOriginalCovertModemObjectStats ge_covert_modem_stats;

extern PropRecord *chrpropAllocate(void);
extern PropRecord *g_FreeProps;

static void ge_original_covert_modem_prop_free(PropRecord *prop)
{
    /* Exact chrpropFree body.  The bounded chrprop state slice deliberately
     * exports only its canonical allocator, so keep the matching free-list
     * operation beside this transactional constructor instead of adding a
     * second public definition of chrpropFree. */
    prop->prev = g_FreeProps;
    prop->next = NULL;
    prop->stan = NULL;
    g_FreeProps = prop;
}

/* Canonical propobj.c global. Only the exact fresh-slot branch is executable
 * here; occupied-slot reuse still belongs to setupFindObjForReuse/objFree. */
s32 g_NextWeaponSlot;

static void ge_model_init_zero_rw(Model *model, ModelFileHeader *header)
{
    /* Exact modelInit assignments. modelInitRwData has no observable work for
     * Pchrbug's GROUP -> DL graph and its zero-record header. */
    model->obj = header;
    model->datas = NULL;
    model->rwdatalen = -1;
    model->attachedto = NULL;
    model->attachedto_objinst = NULL;
    model->scale = 1.0f;
}

static Model *ge_allocate_fresh_model(ModelFileHeader *header)
{
    size_t index;
    for (index = 0U; index < GE_ORIGINAL_WEAPON_SLOT_CAPACITY; index++) {
        Model *model = &ge_covert_modem_model_slots[index];
        if (model->obj == NULL) {
            ge_model_init_zero_rw(model, header);
            return model;
        }
    }
    ge_covert_modem_stats.model_slot_exhaustions++;
    return NULL;
}

static void ge_release_model(Model *model)
{
    if (model != NULL) model->obj = NULL;
}

static GeNativeWeaponObject *ge_weapon_create_fresh(void)
{
    s32 index;
    s32 start = g_NextWeaponSlot;

    /* Exact weaponCreate selection and next-slot update when both prop and
     * model were already obtained by create_new_item_instance_of_model. */
    for (index = start;;
         index = (index + 1) % (s32)GE_ORIGINAL_WEAPON_SLOT_CAPACITY) {
        GeNativeWeaponObject *weapon = &ge_covert_modem_weapon_slots[index];
        if (weapon->object.prop == NULL) {
            g_NextWeaponSlot =
                (index + 1) % (s32)GE_ORIGINAL_WEAPON_SLOT_CAPACITY;
            return weapon;
        }
        if ((index + 1) % (s32)GE_ORIGINAL_WEAPON_SLOT_CAPACITY == start)
            break;
    }
    ge_covert_modem_stats.weapon_slot_exhaustions++;
    return NULL;
}

static GeNativeWeaponObject ge_blank_weapon(void)
{
    GeNativeWeaponObject weapon;
    ObjectRecord *object = &weapon.object;

    /* blank_08_object_preset_1 from canonical propobj.c. Field assignment is
     * used because native pointer widths make the original positional N64
     * initializer unsuitable for a host sanitizer build. */
    memset(&weapon, 0, sizeof(weapon));
    /* The native all-stage scheduler reads the real inherited header through
     * ObjectRecord. Publishing these canonical fields lets unchanged objTick
     * classify a thrown modem as a sticky collectable. */
#if defined(GE_PORT_MS_INHERITS)
    object->extrascale = 0x0100;
    object->state = 0;
    object->type = PROPDEF_COLLECTABLE;
#else
    weapon.host_extrascale = 0x0100;
    weapon.host_state = 0;
    weapon.host_type = PROPDEF_COLLECTABLE;
#endif
    object->obj = 0;
    object->pad = 1;
    object->flags = 0;
    object->flags2 = 0;
    object->mtx.m[0][0] = 1.0f;
    object->mtx.m[1][1] = 1.0f;
    object->mtx.m[2][2] = 1.0f;
    object->mtx.m[3][3] = 1.0f;
    object->damage = 1000.0f;
    object->shadecol.r = object->shadecol.g = object->shadecol.b = 0xff;
    object->nextcol.r = object->nextcol.g = object->nextcol.b = 0xff;
    weapon.weaponnum = ITEM_UNARMED;
    weapon.linked_weapon_type = -1;
    weapon.timer = -1;
    weapon.dualweapon = NULL;
    return weapon;
}

void ge_original_covert_modem_object_reset(void)
{
    s32 index;
    memset(&ge_covert_modem_stats, 0, sizeof(ge_covert_modem_stats));
    memset(ge_covert_modem_model_slots, 0, sizeof(ge_covert_modem_model_slots));
    memset(ge_covert_modem_weapon_slots, 0,
           sizeof(ge_covert_modem_weapon_slots));
    /* Exact relevant portion of initobjects: a weapon slot is free when its
     * inherited ObjectRecord prop pointer is null. */
    for (index = 0; index < (s32)GE_ORIGINAL_WEAPON_SLOT_CAPACITY; index++) {
        ge_covert_modem_weapon_slots[index].object.prop = NULL;
    }
    g_NextWeaponSlot = 0;
}

void *ge_original_covert_modem_object_create(int32_t model_id,
                                             int32_t weapon_id)
{
    ModelFileHeader *model_header;
    PropRecord *prop;
    Model *model;
    GeNativeWeaponObject *weapon;
    ObjectRecord *object;

    ge_covert_modem_stats.construction_calls++;
    if (model_id != PROP_CHRBUG || weapon_id != ITEM_BUG) return NULL;
    if (!ge_original_bug_model_prepare()) return NULL;
    model_header = ge_original_bug_model_header();
    if (model_header == NULL || model_header->numRecords != 0) return NULL;

    /* Exact create_new_item_instance_of_model allocation order. */
    prop = chrpropAllocate();
    model = ge_allocate_fresh_model(model_header);
    weapon = ge_weapon_create_fresh();
    if (prop == NULL || model == NULL || weapon == NULL) {
        if (model != NULL) ge_release_model(model);
        if (prop != NULL) ge_original_covert_modem_prop_free(prop);
        return NULL;
    }

    *weapon = ge_blank_weapon();
    object = &weapon->object;
    weapon->weaponnum = (s8)weapon_id;
    object->obj = (s16)model_id;

    /* Exact successful preallocated objInit branch, followed by
     * complete_object_data_block_return_position_entry's weapon type. */
    if (ge_original_objInitPreallocatedSlice(
            object, model_header, prop, model, 0.1f, NULL) == NULL) {
        ge_release_model(model);
        ge_original_covert_modem_prop_free(prop);
        object->prop = NULL;
        return NULL;
    }
    prop->type = PROP_TYPE_WEAPON;
    ge_covert_modem_stats.successful_constructions++;
    return object;
}

int ge_original_covert_modem_object_prepare_throw(void *opaque_object,
                                                  uint32_t player_index)
{
    GeNativeWeaponObject *weapon;
    size_t index;
    if (opaque_object == NULL || player_index > 3U) return 0;
    for (index = 0U; index < GE_ORIGINAL_WEAPON_SLOT_CAPACITY; index++) {
        weapon = &ge_covert_modem_weapon_slots[index];
        if (&weapon->object != opaque_object) continue;
        if (weapon->weaponnum != ITEM_BUG || weapon->object.prop == NULL)
            return 0;

        /* Exact ITEM_BUG timer and owner assignments from
         * generate_player_thrown_object. */
        weapon->timer = 1;
        weapon->object.runtime_bitflags &= ~RUNTIMEBITFLAG_OWNER;
        weapon->object.runtime_bitflags |=
            player_index << RUNTIMEBITSHIFT_OWNER;
        return 1;
    }
    return 0;
}

void ge_original_covert_modem_object_snapshot(
    GeOriginalCovertModemObjectStats *stats)
{
    if (stats != NULL) *stats = ge_covert_modem_stats;
}

size_t ge_original_covert_modem_object_model_capacity(void)
{
    return GE_ORIGINAL_WEAPON_SLOT_CAPACITY;
}

int ge_original_covert_modem_object_inspect(
    const void *opaque_object, int32_t *weapon_id, int32_t *linked_weapon_id,
    int32_t *timer, uint16_t *extrascale, uint8_t *definition_type)
{
    const GeNativeWeaponObject *weapon;
    size_t index;
    if (opaque_object == NULL) return 0;
    for (index = 0U; index < GE_ORIGINAL_WEAPON_SLOT_CAPACITY; index++) {
        weapon = &ge_covert_modem_weapon_slots[index];
        if (&weapon->object != opaque_object) continue;
        if (weapon_id != NULL) *weapon_id = weapon->weaponnum;
        if (linked_weapon_id != NULL)
            *linked_weapon_id = weapon->linked_weapon_type;
        if (timer != NULL) *timer = weapon->timer;
#if defined(GE_PORT_MS_INHERITS)
        if (extrascale != NULL) *extrascale = weapon->object.extrascale;
        if (definition_type != NULL)
            *definition_type = weapon->object.type;
#else
        if (extrascale != NULL) *extrascale = weapon->host_extrascale;
        if (definition_type != NULL) *definition_type = weapon->host_type;
#endif
        return 1;
    }
    return 0;
}
