#include "ge_original_guard_grenade_object.h"

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/model.h"

#include "ge_original_default_object_internal.h"
#include "ge_original_guard_grenade_model.h"

#include <string.h>

#define GE_ORIGINAL_GUARD_GRENADE_SLOT_CAPACITY 30U
#define GE_ORIGINAL_GUARD_GRENADE_RW_WORD_CAPACITY 1U

typedef struct GeGuardGrenadeSlot {
    WeaponObjRecord weapon;
    Model model;
    u32 rwdata[GE_ORIGINAL_GUARD_GRENADE_RW_WORD_CAPACITY];
    PropRecord *prop;
} GeGuardGrenadeSlot;

static GeGuardGrenadeSlot
    ge_guard_grenade_slots[GE_ORIGINAL_GUARD_GRENADE_SLOT_CAPACITY];
static GeOriginalGuardGrenadeObjectStats ge_guard_grenade_stats;
static uint32_t ge_guard_grenade_next_slot;

extern PropRecord *chrpropAllocate(void);
extern PropRecord *g_FreeProps;
extern void modelInit(Model *model, ModelFileHeader *header, u32 *rwdata);
extern void modelSetScale(Model *model, f32 scale);

static void ge_grenade_prop_free(PropRecord *prop)
{
    /* Exact chrpropFree free-list operation for a constructor rollback. */
    prop->prev = g_FreeProps;
    prop->next = NULL;
    prop->stan = NULL;
    g_FreeProps = prop;
}

static void ge_grenade_reparent(PropRecord *child, PropRecord *host)
{
    /* Exact chrpropReparent body. */
    child->parent = host;
    if (host->child != NULL) host->child->next = child;
    child->prev = host->child;
    child->next = NULL;
    child->stan = NULL;
    host->child = child;
}

static WeaponObjRecord ge_blank_weapon(void)
{
    WeaponObjRecord weapon;
    ObjectRecord *object = (ObjectRecord *)(void *)&weapon;

    /* blank_08_object_preset_1, assigned field-wise so native pointer width
     * cannot perturb the canonical inherited ObjectRecord prefix. */
    memset(&weapon, 0, sizeof(weapon));
    object->extrascale = 0x0100;
    object->state = 0;
    object->type = PROPDEF_COLLECTABLE;
    object->obj = 0;
    object->pad = 1;
    object->mtx.m[0][0] = 1.0f;
    object->mtx.m[1][1] = 1.0f;
    object->mtx.m[2][2] = 1.0f;
    object->mtx.m[3][3] = 1.0f;
    object->damage = 1000.0f;
    object->shadecol.r = object->shadecol.g = object->shadecol.b = 0xff;
    object->nextcol.r = object->nextcol.g = object->nextcol.b = 0xff;
    weapon.weaponnum = ITEM_UNARMED;
    weapon.LinkedWeaponType = -1;
    weapon.timer = -1;
    weapon.dualweapon = NULL;
    return weapon;
}

static GeGuardGrenadeSlot *ge_allocate_fresh_slot(void)
{
    uint32_t scanned;
    for (scanned = 0U;
         scanned < GE_ORIGINAL_GUARD_GRENADE_SLOT_CAPACITY; scanned++) {
        uint32_t index = (ge_guard_grenade_next_slot + scanned)
            % GE_ORIGINAL_GUARD_GRENADE_SLOT_CAPACITY;
        GeGuardGrenadeSlot *slot = &ge_guard_grenade_slots[index];
        if (slot->weapon.prop == NULL) {
            ge_guard_grenade_next_slot =
                (index + 1U) % GE_ORIGINAL_GUARD_GRENADE_SLOT_CAPACITY;
            return slot;
        }
    }
    ge_guard_grenade_stats.weapon_slot_exhaustions++;
    return NULL;
}

void ge_original_guard_grenade_object_reset(void)
{
    memset(ge_guard_grenade_slots, 0, sizeof(ge_guard_grenade_slots));
    memset(&ge_guard_grenade_stats, 0, sizeof(ge_guard_grenade_stats));
    ge_guard_grenade_next_slot = 0U;
}

void *ge_original_guard_grenade_object_create(
    void *opaque_chr, int32_t model_id, int32_t weapon_id, int32_t flags)
{
    ChrRecord *chr = opaque_chr;
    ModelFileHeader *header;
    GeGuardGrenadeSlot *slot;
    ObjectRecord *object;
    PropRecord *prop;
    GUNHAND hand;

    ge_guard_grenade_stats.construction_calls++;
    if (chr == NULL || chr->prop == NULL || chr->model == NULL
            || chr->model->obj == NULL
            || model_id != PROP_CHRGRENADE || weapon_id != ITEM_GRENADE)
        return NULL;
    hand = (flags & PROPFLAG_WEAPON_LEFTHANDED) ? GUNLEFT : GUNRIGHT;
    if (chr->weapons_held[hand] != NULL
            || chr->model->obj->numSwitches <= (hand == GUNLEFT ? 5 : 3)
            || chr->model->obj->Switches[hand == GUNLEFT ? 5 : 3] == NULL)
        return NULL;
    header = ge_original_guard_grenade_model_header();
    if (header == NULL
            || (uint32_t)header->numRecords
                 > GE_ORIGINAL_GUARD_GRENADE_RW_WORD_CAPACITY) {
        ge_guard_grenade_stats.model_slot_exhaustions++;
        return NULL;
    }

    /* Exact something_with_generating_object happy-path ordering: prop, model,
     * then fresh weapon slot, followed by blank preset and objInit. */
    prop = chrpropAllocate();
    if (prop == NULL) return NULL;
    slot = ge_allocate_fresh_slot();
    if (slot == NULL) {
        ge_grenade_prop_free(prop);
        return NULL;
    }
    memset(&slot->model, 0, sizeof(slot->model));
    memset(slot->rwdata, 0, sizeof(slot->rwdata));
    modelInit(&slot->model, header, slot->rwdata);
    slot->weapon = ge_blank_weapon();
    object = (ObjectRecord *)(void *)&slot->weapon;
    slot->weapon.weaponnum = (s8)weapon_id;
    object->obj = (s16)model_id;
    object->flags = (u32)flags | PROPFLAG_ASSIGNEDTOCHR;
    object->pad = chr->chrnum;

    if (ge_original_objInitPreallocatedSlice(
            object, header, prop, &slot->model, 0.1f, NULL) == NULL) {
        memset(slot, 0, sizeof(*slot));
        ge_grenade_prop_free(prop);
        return NULL;
    }
    prop->type = PROP_TYPE_WEAPON;
    modelSetScale(&slot->model,
                  slot->model.scale
                    * ((f32)object->extrascale * (1.0f / 256.0f)));

    /* Exact successful chrEquipWeapon arm and child publication. */
    slot->model.attachedto = chr->model;
    slot->model.attachedto_objinst =
        chr->model->obj->Switches[hand == GUNLEFT ? 5 : 3];
    chr->weapons_held[hand] = prop;
    ge_grenade_reparent(prop, chr->prop);
    slot->prop = prop;
    ge_guard_grenade_stats.successful_constructions++;
    return prop;
}

void ge_original_guard_grenade_object_snapshot(
    GeOriginalGuardGrenadeObjectStats *stats)
{
    if (stats != NULL) *stats = ge_guard_grenade_stats;
}

int ge_original_guard_grenade_object_inspect(
    const void *opaque_prop, int32_t *model_id, int32_t *weapon_id,
    int32_t *timer, uint32_t *runtime_bitflags,
    const void **model_header, const void **parent)
{
    uint32_t index;
    for (index = 0U; index < GE_ORIGINAL_GUARD_GRENADE_SLOT_CAPACITY;
         index++) {
        const GeGuardGrenadeSlot *slot = &ge_guard_grenade_slots[index];
        const ObjectRecord *object = (const ObjectRecord *)(const void *)
            &slot->weapon;
        if (slot->prop != opaque_prop || object->prop != opaque_prop)
            continue;
        if (model_id != NULL) *model_id = object->obj;
        if (weapon_id != NULL) *weapon_id = slot->weapon.weaponnum;
        if (timer != NULL) *timer = slot->weapon.timer;
        if (runtime_bitflags != NULL)
            *runtime_bitflags = object->runtime_bitflags;
        if (model_header != NULL) *model_header = slot->model.obj;
        if (parent != NULL) *parent = slot->prop->parent;
        return 1;
    }
    return 0;
}
