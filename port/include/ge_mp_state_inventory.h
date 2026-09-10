#ifndef GE_MP_STATE_INVENTORY_H
#define GE_MP_STATE_INVENTORY_H
#include "ge_mp_state.h"
#include "ge_original_multiplayer_players.h"

#define GE_MP_INVENTORY_KIND 0xE1U /* Physical slot storage, not item lifetime. */
#define GE_MP_PERMISSION_KIND 0xE2U
#define GE_MP_INVENTORY_SLOTS 30U
#define GE_MP_INVENTORY_RECORDS 124U
#define GE_MP_INVENTORY_FIELDS 7U
#define GE_MP_PERMISSION_FIELDS 42U

enum GeMpInventoryField {
    GE_MP_INV_TYPE=256, GE_MP_INV_NEXT, GE_MP_INV_PREV, GE_MP_INV_WEAPON,
    GE_MP_INV_DUAL_RIGHT, GE_MP_INV_DUAL_LEFT, GE_MP_INV_PROP
};
enum GeMpPermissionField {
    GE_MP_PERM_ACTIVE=256, GE_MP_PERM_BASE, GE_MP_PERM_HEAD,
    GE_MP_PERM_CAPACITY, GE_MP_PERM_ALL_GUNS, GE_MP_PERM_CURRENT_ITEM,
    GE_MP_PERM_AMMO, GE_MP_PERM_HELD_RIGHT, GE_MP_PERM_HELD_LEFT,
    GE_MP_PERM_HELD_TIME, GE_MP_PERM_DEATHCOUNT, GE_MP_PERM_SUICIDES, GE_MP_PERM_KILLS_THIS_LIFE,
    GE_MP_PERM_LIFE_START, GE_MP_PERM_LAST_KILL_TIME,
    GE_MP_PERM_SHOT_COUNT, GE_MP_PERM_KILL_COUNT, GE_MP_PERM_GG_KILLS,
    GE_MP_PERM_KILL_COUNTS, GE_MP_PERM_ON_SCREEN_TIME, GE_MP_PERM_DISTANCE,
    GE_MP_PERM_BACKSIDE_DAMAGE, GE_MP_PERM_ARMOUR_PICKUPS,
    GE_MP_PERM_MIN_KILL_TIME, GE_MP_PERM_MAX_KILL_TIME, GE_MP_PERM_LONGEST_INNING,
    GE_MP_PERM_SHORTEST_INNING, GE_MP_PERM_MOST_KILLED_LIFE,
    GE_MP_PERM_MOST_KILLED_TIME, GE_MP_PERM_HANDICAP, GE_MP_PERM_FLAG_COUNTER,
    GE_MP_PERM_PERSPECTIVE_HEIGHT, GE_MP_PERM_YOLT_ORDER,
    GE_MP_PERM_TEAM_OR_TOKEN, GE_MP_PERM_AUTOAIM, GE_MP_PERM_SIGHT,
    GE_MP_PERM_CIVILIANS, GE_MP_PERM_SCORE, GE_MP_PERM_TEXT_OVERRIDE_PRESENT,
    GE_MP_PERM_RESERVED_CAPACITY
};

typedef struct GeMpInventoryValues {
    int32_t type,weapon,right,left;
    uint32_t next,prev;
    uint64_t prop;
} GeMpInventoryValues;
typedef struct GeMpPermissionValues {
    uint32_t active_order[30],free_order[30],base,head,reserved_capacity;
    uint8_t active,text_override_present;
    int32_t capacity,all_guns,current_item,ammo[30],held_right[10],held_left[10],held_time[10];
    int32_t deathcount,suicides,kills_this_life,life_start,last_kill_time[4];
    int32_t shot_count[7],kill_count,gg_kills,kill_counts[4],on_screen_time;
    float distance;
    int32_t backside_damage;
    float armour_pickups;
    int32_t min_kill_time,max_kill_time,longest_inning,shortest_inning;
    int32_t most_killed_life,most_killed_time;
    float handicap;
    int32_t flag_counter;
    float perspective_height;
    uint8_t yolt_order,team_or_token,autoaim,sight;
    int32_t civilians,score;
} GeMpPermissionValues;

typedef struct GeMpStateInventoryOwner {
    GeMpRegistry *registry;
    GeOriginalMultiplayerInventoryStorage storage[4];
    uint64_t ids[GE_MP_INVENTORY_RECORDS];
    uint32_t player_count;
    uint8_t live;
} GeMpStateInventoryOwner;

typedef struct GeMpStateInventorySnapshot {
    GeMpStateRecord records[GE_MP_INVENTORY_RECORDS];
    GeMpStateField inventory_fields[120][GE_MP_INVENTORY_FIELDS];
    GeMpStateField permission_fields[4][GE_MP_PERMISSION_FIELDS];
    GeMpInventoryValues inventory[120];
    GeMpPermissionValues permission[4];
    size_t record_count;
    GeMpStateCoverage coverage;
} GeMpStateInventorySnapshot;

typedef struct GeMpStateInventoryProviders {
    void *context;
    /* Required: resolve native PropRecord identity through the registry owner.
     * Export also checks that the returned actor ID is currently registered.
     * The final snapshot must include that prop's complete native record. */
    GeMpStateStatus (*prop_actor)(void *context,const void *native_prop,uint64_t *actor_id);
} GeMpStateInventoryProviders;

/* Zero-init owner. Define/acquire all 124 physical records once per native
 * storage session. Reuse released keys to advance their generations. Pickup,
 * removal and respawn do not change physical slot lifetimes. End before
 * players_destroy; the host lifetime guard catches missed teardown/reuse. */
GeMpStateStatus ge_mp_state_inventory_begin(GeMpStateInventoryOwner *owner,GeMpRegistry *registry);
GeMpStateStatus ge_mp_state_inventory_end(GeMpStateInventoryOwner *owner);
/* Copies typed fields only. Output borrows only its own arrays: do not move
 * the output after export. Failure clears record_count and reports gaps.
 * NONE/-1 unions are inactive; unknown union types or text overrides fail
 * INCOMPLETE. Physical next/prev/head/base/order links use SLOT_REF keys.
 * This domain never sets schema.complete or attests whole players/rules. */
GeMpStateStatus ge_mp_state_inventory_export(const GeMpStateInventoryOwner *owner,
    const GeMpStateInventoryProviders *providers,GeMpStateInventorySnapshot *snapshot);
/* Two entries in increasing E1/E2 order, for an aggregate schema. */
const GeMpRecordSchema *ge_mp_state_inventory_schemas(size_t *count);
#endif
