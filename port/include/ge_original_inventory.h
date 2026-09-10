#ifndef GE_ORIGINAL_INVENTORY_H
#define GE_ORIGINAL_INVENTORY_H

#include <stddef.h>
#include <stdint.h>

enum {
    GE_ORIGINAL_INVENTORY_CAPACITY = 64,
    GE_ORIGINAL_INVENTORY_NAME_CAPACITY = 96
};

typedef enum GeOriginalInventoryResult {
    GE_ORIGINAL_INVENTORY_OK = 0,
    GE_ORIGINAL_INVENTORY_INVALID_ARGUMENT,
    GE_ORIGINAL_INVENTORY_UNAVAILABLE,
    GE_ORIGINAL_INVENTORY_BLOCKED,
    GE_ORIGINAL_INVENTORY_STALE,
    GE_ORIGINAL_INVENTORY_NOT_SELECTABLE,
    GE_ORIGINAL_INVENTORY_NO_CHANGE
} GeOriginalInventoryResult;

typedef struct GeOriginalInventoryEntry {
    int32_t index; /* Original watch index, never a weapon-cycle index. */
    int32_t weapon;
    uint16_t text_id;
    uint8_t selected;
    uint8_t selectable;
    char name[GE_ORIGINAL_INVENTORY_NAME_CAPACITY];
    /* Opaque comparison values; callers must not dereference or edit them. */
    uintptr_t item_identity;
    uintptr_t prop_identity;
} GeOriginalInventoryEntry;

typedef struct GeOriginalInventorySnapshot {
    size_t count;
    size_t total_count;
    uint8_t selection_allowed;
    uintptr_t player_identity;
    uintptr_t storage_identity;
    uint64_t epoch;
    GeOriginalInventoryEntry entries[GE_ORIGINAL_INVENTORY_CAPACITY];
} GeOriginalInventorySnapshot;

/* Copy original watch rows/names without changing equipcuritem, ammo, input,
 * or HUD caches. Dual pairs stay in the canonical weapon cycle. Snapshots
 * remain readable while input is blocked, but selection_allowed is false.
 * Missing text uses "Item"; no borrowed language/inventory pointers escape. */
GeOriginalInventoryResult ge_original_inventory_snapshot(
    GeOriginalInventorySnapshot *snapshot);

/* Invoke once on a released, captured touch before the gameplay tick. Pass
 * the snapshot captured at touch onset, not a freshly refreshed row. Rechecks
 * player ownership, row identity, mapping and gameplay/hand state. A rejected
 * request makes no gameplay changes. From idle gameplay only, preserves the
 * original watch's right-weapon/left-unarmed un-equip and ammo bookkeeping.
 * Selecting the already single-equipped item returns NO_CHANGE. */
GeOriginalInventoryResult ge_original_inventory_select(
    const GeOriginalInventorySnapshot *snapshot, size_t row);

/* Call at mission initialization/cleanup to expire touches across restarts,
 * including when canonical player/inventory storage is reused in place. */
void ge_original_inventory_invalidate(void);

#endif
