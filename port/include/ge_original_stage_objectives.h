#ifndef GE_ORIGINAL_STAGE_OBJECTIVES_H
#define GE_ORIGINAL_STAGE_OBJECTIVES_H

#include "ge_original_stage_setup.h"

#include <stddef.h>
#include <stdint.h>

enum {
    GE_ORIGINAL_STAGE_OBJECTIVE_MAX = 10,
    GE_ORIGINAL_STAGE_OBJECTIVE_NO_INDEX = -1
};

typedef enum GeOriginalStageObjectiveBlocker {
    GE_ORIGINAL_STAGE_OBJECTIVE_READY = 0,
    GE_ORIGINAL_STAGE_OBJECTIVE_NO_OBJECT_PROVIDER,
    GE_ORIGINAL_STAGE_OBJECTIVE_LIVE_OBJECT_UNAVAILABLE,
    GE_ORIGINAL_STAGE_OBJECTIVE_TARGET_NOT_OBJECT
} GeOriginalStageObjectiveBlocker;

/* The callback returns the already-constructed native ObjectRecord definition
 * for one authored setup command.  The registry deliberately treats it as an
 * opaque pointer: ownership and object construction remain with the existing
 * prop services. */
typedef struct GeOriginalStageObjectiveProviders {
    void *context;
    void *(*object_definition_by_command)(
        void *context, size_t command_index,
        const GeOriginalStagePropRecord *record);
} GeOriginalStageObjectiveProviders;

typedef struct GeOriginalStageTagEntry {
    const GeOriginalStagePropRecord *record;
    void *tagged_object;
    size_t command_index;
    size_t target_command_index;
    int32_t next_tag;
    uint16_t tag_id;
    uint8_t blocker;
} GeOriginalStageTagEntry;

typedef struct GeOriginalStageObjectiveTextEntry {
    const GeOriginalStagePropRecord *record;
    size_t command_index;
    int32_t next_text;
    uint32_t menu;
    uint16_t reserved;
    uint16_t text_id;
} GeOriginalStageObjectiveTextEntry;

typedef struct GeOriginalStageObjectiveEntry {
    const GeOriginalStagePropRecord *record;
    size_t command_index;
    size_t end_command_index;
    size_t first_criterion;
    size_t criterion_count;
    uint32_t menu;
    uint16_t reserved;
    uint16_t text_id;
    uint16_t unknown_c;
    uint8_t unknown_e;
    int8_t difficulty;
} GeOriginalStageObjectiveEntry;

/* One native sidecar for every command between OBJECTIVE_START and END.
 * `words` retain the exact four authored payload words after the command
 * header.  The named fields expose the three pointer-bearing criteria whose
 * unchanged status handlers consume mutable runtime state. */
typedef struct GeOriginalStageObjectiveCriterion {
    const GeOriginalStagePropRecord *record;
    void *tagged_object;
    size_t command_index;
    size_t objective_index;
    int32_t next_same_type;
    int32_t tag_index;
    uint32_t words[4];
    int32_t value_a;
    int32_t value_b;
    int32_t status;
    uint8_t type;
    uint8_t blocker;
} GeOriginalStageObjectiveCriterion;

typedef struct GeOriginalStageObjectiveRegistry {
    const GeOriginalStageSetupRuntime *setup;
    GeOriginalStageTagEntry *tags;
    GeOriginalStageObjectiveTextEntry *texts;
    GeOriginalStageObjectiveEntry *objectives;
    GeOriginalStageObjectiveCriterion *criteria;
    size_t tag_count;
    size_t text_count;
    size_t objective_entry_count;
    size_t criterion_count;
    size_t blocked_tag_count;
    size_t blocked_criterion_count;
    int32_t tag_head;
    int32_t text_head;
    int32_t enter_room_head;
    int32_t deposit_room_head;
    int32_t photograph_head;
    int32_t objective_by_menu[GE_ORIGINAL_STAGE_OBJECTIVE_MAX];
    int32_t objective_count;
    void *runtime_close_context;
    void (*runtime_close)(void *context);
    uint8_t initialized;
} GeOriginalStageObjectiveRegistry;

typedef enum GeOriginalStageObjectiveStatus {
    GE_ORIGINAL_STAGE_OBJECTIVE_OK = 0,
    GE_ORIGINAL_STAGE_OBJECTIVE_INVALID_ARGUMENT,
    GE_ORIGINAL_STAGE_OBJECTIVE_INVALID_SETUP,
    GE_ORIGINAL_STAGE_OBJECTIVE_NO_MEMORY,
    GE_ORIGINAL_STAGE_OBJECTIVE_INVALID_ORDER,
    GE_ORIGINAL_STAGE_OBJECTIVE_INVALID_RELATION
} GeOriginalStageObjectiveStatus;

/* Replays the objective-related branches of the original proplvreset2 in
 * authored command order.  Serialized pointer slots are never overwritten;
 * all runtime links live in the native sidecars above. */
GeOriginalStageObjectiveStatus ge_original_stage_objectives_build(
    GeOriginalStageObjectiveRegistry *registry,
    const GeOriginalStageSetupRuntime *setup,
    const GeOriginalStageObjectiveProviders *providers);

const GeOriginalStageTagEntry *ge_original_stage_objectives_find_tag(
    const GeOriginalStageObjectiveRegistry *registry, uint16_t tag_id);
void ge_original_stage_objectives_close(
    GeOriginalStageObjectiveRegistry *registry);
const char *ge_original_stage_objective_status_name(
    GeOriginalStageObjectiveStatus status);
const char *ge_original_stage_objective_blocker_name(
    GeOriginalStageObjectiveBlocker blocker);

#endif
