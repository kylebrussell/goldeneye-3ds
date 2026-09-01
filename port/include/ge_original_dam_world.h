#ifndef GE_ORIGINAL_DAM_WORLD_H
#define GE_ORIGINAL_DAM_WORLD_H

#include <stddef.h>
#include <stdint.h>

#include "ge_original_dam_monitor.h"
#include "ge_original_dam_monitor_render.h"

typedef struct GeOriginalDamWorldProviders {
    void *context;
    void *(*allocate_definition)(void *context, uint8_t propdef_type,
                                 size_t size_bytes);
    void *(*allocate_prop)(void *context, void *definition);
    void (*activate_prop)(void *context, void *prop);
    void (*enable_prop)(void *context, void *prop);
    void (*register_room)(void *context, void *prop, int16_t room);
} GeOriginalDamWorldProviders;

typedef struct GeOriginalDamWorldEntry {
    int32_t command_index;
    uint8_t propdef_type;
    int16_t object_id;
    int16_t pad_id;
    uint32_t flags;
    uint32_t flags2;
    float position[3];
    void *stan;
    int16_t room;
    void *definition;
    void *prop;
    int lifecycle_active;
} GeOriginalDamWorldEntry;

#define GE_ORIGINAL_DAM_SPAWN_WINDOW_COUNT 5U

/* Native sidecar for the N64 door words that follow ObjectRecord. IDO's
 * anonymous `inherits ObjectRecord` is not representable by GCC/Clang. */
typedef struct GeOriginalDamDoorSetup {
    int32_t linked_door_offset;
    uint32_t max_frac_fixed;
    uint32_t perim_frac_fixed;
    uint32_t accel_fixed;
    uint32_t decel_fixed;
    uint32_t max_speed_fixed;
    uint16_t door_flags;
    uint16_t door_type;
    uint32_t key_flags;
    uint32_t auto_close_frames;
    uint32_t door_open_sound;
} GeOriginalDamDoorSetup;

typedef struct GeOriginalDamWorldState {
    uint32_t commands_scanned;
    uint32_t definitions_materialized;
    uint32_t rooms_registered;
    GeOriginalDamWorldEntry first_glass;
    GeOriginalDamWorldEntry first_object;
    GeOriginalDamWorldEntry first_door;
    GeOriginalDamWorldEntry second_door;
    GeOriginalDamWorldEntry spawn_windows[
        GE_ORIGINAL_DAM_SPAWN_WINDOW_COUNT];
    int loaded;
} GeOriginalDamWorldState;

typedef struct GeOriginalDamMissionTagState {
    GeOriginalDamWorldEntry tag5_object;
    GeOriginalDamWorldEntry tag4_object;
    uint32_t tags_registered;
    int loaded;
} GeOriginalDamMissionTagState;

void ge_dam_setup_world_materializer_bind(
    const GeOriginalDamWorldProviders *providers,
    GeOriginalDamWorldState *state);

/* Walks the complete authored Dam prop stream and validates the post-tinted-
 * glass landmarks through EndProps. This guards every consumer of the local
 * setup word-size table against silent record-boundary drift. */
int ge_dam_setup_world_validate_authored_stream(
    size_t *command_count, size_t *tinted_glass_count);

/* Native ABI adapter for the generated setup word stream. */
void ge_dam_setup_world_materialize_first_authored(void);
/* Materializes authored command 268 without changing the existing bounded
 * command-107/122/267 bootstrap. This is the reciprocal half of Dam's gate
 * pair and is intentionally opt-in until the platform owns a second Model
 * instance and collision allocation. */
int ge_dam_setup_world_materialize_linked_door(void);
/* Materializes the exact five PwindowZ definitions at setup commands
 * 117--121. Their authored pads live in room 132, the first portal-connected
 * exterior building visible from the normal Dam spawn. */
size_t ge_dam_setup_world_materialize_spawn_windows(void);
/* Resolves the two authored relative command offsets (+1 and -1) after both
 * exact setupDoor constructors have populated their native DoorRecords. */
int ge_dam_setup_world_link_authored_doors(void);
int ge_dam_setup_world_materialize_mission_tags(
    GeOriginalDamMissionTagState *state);
int ge_dam_setup_world_mission_monitor_snapshot(
    const GeOriginalDamWorldEntry *entry,
    GeOriginalDamMonitorSnapshot *snapshot);
int ge_dam_setup_world_mission_monitor_render_tick(
    const GeOriginalDamWorldEntry *entry, void *model_instance,
    GeOriginalDamMonitorRenderSnapshot *snapshot);
int ge_dam_setup_world_activate_entry(GeOriginalDamWorldEntry *entry);

/* Native sidecar for IDO inherited PropDefHeaderRecord fields, which GCC and
 * Clang do not lay out from the decomp's `inherits Type;` declaration. */
int ge_dam_setup_world_definition_header(const void *definition,
                                          uint16_t *extrascale,
                                          uint8_t *state,
                                          uint8_t *type);
int ge_dam_setup_world_definition_set_state(void *definition, uint8_t state);
int ge_dam_setup_world_door_setup(const void *definition,
                                  GeOriginalDamDoorSetup *setup);

#endif
