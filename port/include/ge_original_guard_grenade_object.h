#ifndef GE_ORIGINAL_GUARD_GRENADE_OBJECT_H
#define GE_ORIGINAL_GUARD_GRENADE_OBJECT_H

#include <stdint.h>

typedef struct GeOriginalGuardGrenadeObjectStats {
    uint32_t construction_calls;
    uint32_t successful_constructions;
    uint32_t model_slot_exhaustions;
    uint32_t weapon_slot_exhaustions;
} GeOriginalGuardGrenadeObjectStats;

void ge_original_guard_grenade_object_reset(void);

/* Exact fresh-slot something_with_generating_object branch for
 * PROP_CHRGRENADE/ITEM_GRENADE, through objInit and chrEquipWeapon. */
void *ge_original_guard_grenade_object_create(
    void *chr, int32_t model_id, int32_t weapon_id, int32_t flags);

void ge_original_guard_grenade_object_snapshot(
    GeOriginalGuardGrenadeObjectStats *stats);

/* Test/audit access to the concrete native WeaponObjRecord and Model state. */
int ge_original_guard_grenade_object_inspect(
    const void *prop, int32_t *model_id, int32_t *weapon_id,
    int32_t *timer, uint32_t *runtime_bitflags,
    const void **model_header, const void **parent);

#endif
