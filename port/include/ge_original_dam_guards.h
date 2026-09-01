#ifndef GE_ORIGINAL_DAM_GUARDS_H
#define GE_ORIGINAL_DAM_GUARDS_H

#include <stddef.h>
#include <stdint.h>

enum { GE_ORIGINAL_DAM_INITIAL_GUARD_CAPACITY = 4 };

typedef enum GeOriginalDamGuardStatus {
    GE_ORIGINAL_DAM_GUARD_OK = 0,
    GE_ORIGINAL_DAM_GUARD_NO_SETUP,
    GE_ORIGINAL_DAM_GUARD_MODEL_UNAVAILABLE,
    GE_ORIGINAL_DAM_GUARD_PROP_UNAVAILABLE,
    GE_ORIGINAL_DAM_GUARD_ILLEGAL_STAN,
    GE_ORIGINAL_DAM_GUARD_MODEL_ABI_UNAVAILABLE,
    GE_ORIGINAL_DAM_GUARD_WEAPON_UNAVAILABLE,
    GE_ORIGINAL_DAM_GUARD_WEAPON_ABI_UNAVAILABLE
} GeOriginalDamGuardStatus;

typedef struct GeOriginalDamGuardStats {
    uint32_t authored_guards;
    uint32_t attempted_guards;
    uint32_t constructed_guards;
    uint32_t matrix_updates;
    uint32_t authored_weapons;
    uint32_t attached_weapons;
    uint16_t first_command_index;
    uint16_t last_command_index;
    uint8_t last_status;
} GeOriginalDamGuardStats;

void ge_original_dam_guards_reset(void);

/* Canonical bounded expand_09_characters tranche for Dam records 23..26.
 * These four records all name body 37 (greatguard2), whose exact generated
 * body has an integrated head. The shared original prop pool must be reset
 * and the authored setup pads/STAN must already be rebound. */
GeOriginalDamGuardStatus ge_original_dam_guards_construct_initial(void);

/* Runs the original no-animation model matrix path using the supplied
 * world-to-view base matrix. */
GeOriginalDamGuardStatus ge_original_dam_guards_update_matrices(
    const float world_to_view[4][4]);

/* Republishes matrices only for guards that the unchanged chrTick ->
 * posIsOnScreen path marked visible. Unlike the bootstrap helper above, this
 * never manufactures PROPFLAG_ONSCREEN and is therefore the live renderer
 * boundary used after the canonical character scheduler has run. */
GeOriginalDamGuardStatus ge_original_dam_guards_update_visible_matrices(
    const float world_to_view[4][4]);

size_t ge_original_dam_guards_count(void);
/* A constructed authored slot remains addressable for mission/score state
 * after canonical chrTick removal. This reports only slots whose canonical
 * ChrRecord still owns its model and prop; renderers must use this instead of
 * assuming all originally constructed guards remain drawable forever. */
int ge_original_dam_guard_is_live(size_t index);
size_t ge_original_dam_guards_live_count(void);
void *ge_original_dam_guard_prop(size_t index);
void *ge_original_dam_guard_chr(size_t index);
void *ge_original_dam_guard_weapon_prop(size_t index);
void ge_original_dam_guards_snapshot(GeOriginalDamGuardStats *stats);

#endif
