#ifndef GE_ORIGINAL_COVERT_MODEM_OBJECT_H
#define GE_ORIGINAL_COVERT_MODEM_OBJECT_H

#include <stddef.h>
#include <stdint.h>

typedef struct GeOriginalCovertModemObjectStats {
    uint32_t construction_calls;
    uint32_t successful_constructions;
    uint32_t model_slot_exhaustions;
    uint32_t weapon_slot_exhaustions;
} GeOriginalCovertModemObjectStats;

/* Stage-reset boundary for the original 30-entry weapon pool and the native
 * non-animated model slots used by its exact fresh-slot construction path. */
void ge_original_covert_modem_object_reset(void);

/* Exact fresh-slot branch of create_new_item_instance_of_model, bounded to
 * ITEM_BUG/PROP_CHRBUG. The shared original g_Props pool owns the returned
 * prop; a concrete native record supplies the collectable tail that GCC does
 * not flatten from WeaponObjRecord's `inherits ObjectRecord` declaration.
 * Reuse/eviction remains an explicit frontier. */
void *ge_original_covert_modem_object_create(int32_t model_id,
                                             int32_t weapon_id);

/* Exact ITEM_BUG branch immediately after construction in
 * generate_player_thrown_object: timer=1 and owner-bit replacement.  This
 * intentionally stops before gunInitProjectileFromPlayer until the native
 * Projectile/WeaponObjRecord ABI is concrete. */
int ge_original_covert_modem_object_prepare_throw(void *object,
                                                  uint32_t player_index);

void ge_original_covert_modem_object_snapshot(
    GeOriginalCovertModemObjectStats *stats);
size_t ge_original_covert_modem_object_model_capacity(void);
int ge_original_covert_modem_object_inspect(
    const void *object, int32_t *weapon_id, int32_t *linked_weapon_id,
    int32_t *timer, uint16_t *extrascale, uint8_t *definition_type);

#endif
