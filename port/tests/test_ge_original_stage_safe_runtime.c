#include "ge_original_stage_safe_runtime.h"

#include <bondconstants.h>
#include <bondtypes.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    GeOriginalStageSafeRuntime runtime;
    GeOriginalStageSafeRuntime other_runtime;
    ObjectRecord item;
    ObjectRecord free_item;
    SafeRecord safe;
    DoorRecord door;
    PropRecord item_prop;
    PropRecord safe_prop;
    PropRecord door_prop;
    SafeObjectRecord relation;
    SafeObjectRecord invalid;

    memset(&item, 0, sizeof(item));
    memset(&free_item, 0, sizeof(free_item));
    memset(&safe, 0, sizeof(safe));
    memset(&door, 0, sizeof(door));
    memset(&item_prop, 0, sizeof(item_prop));
    memset(&safe_prop, 0, sizeof(safe_prop));
    memset(&door_prop, 0, sizeof(door_prop));
    memset(&relation, 0, sizeof(relation));
    memset(&invalid, 0, sizeof(invalid));
    memset(&other_runtime, 0, sizeof(other_runtime));
    item.type = PROPDEF_COLLECTABLE;
    item.prop = &item_prop;
    item.flags2 = PROPFLAG2_LINKEDTOSAFE;
    free_item.type = PROPDEF_COLLECTABLE;
    safe.type = PROPDEF_SAFE;
    safe.prop = &safe_prop;
    door.type = PROPDEF_DOOR;
    door.prop = &door_prop;
    relation.type = PROPDEF_SAFE_ITEM;
    relation.item = &item;
    relation.safe = &safe;
    relation.door = &door;

    ge_original_stage_safe_runtime_bind(&runtime);
    assert(runtime.bound == 1U && runtime.relation_count == 0U
           && runtime.head == NULL);
    assert(ge_original_stage_safe_runtime_register_relation(
        &runtime, &relation));
    assert(runtime.relation_count == 1U && runtime.generation == 1U
           && runtime.head == &relation && relation.next == NULL);
    assert(!ge_original_stage_safe_runtime_register_relation(
        &runtime, &relation));
    assert(runtime.status
           == GE_ORIGINAL_STAGE_SAFE_RUNTIME_DUPLICATE_RELATION
           && runtime.relation_count == 1U && relation.next == NULL);

    door.openPosition = 0.0f;
    assert(!ge_original_stage_safe_runtime_can_pickup(&runtime, &item));
    door.openPosition = 0.5f;
    assert(!ge_original_stage_safe_runtime_can_pickup(&runtime, &item));
    door.openPosition = 0.50001f;
    assert(ge_original_stage_safe_runtime_can_pickup(&runtime, &item));
    assert(ge_original_stage_safe_runtime_can_pickup(&runtime, &free_item));
    assert(runtime.pickup_tests == 4U && runtime.blocked_pickups == 2U);

    invalid.type = PROPDEF_SAFE_ITEM;
    invalid.item = &item;
    assert(!ge_original_stage_safe_runtime_register_relation(
        &runtime, &invalid));
    assert(runtime.status == GE_ORIGINAL_STAGE_SAFE_RUNTIME_INVALID_RELATION
           && runtime.relation_count == 1U && runtime.head == &relation);
    assert(!ge_original_stage_safe_runtime_register_relation(&runtime, NULL)
           && runtime.status
                == GE_ORIGINAL_STAGE_SAFE_RUNTIME_INVALID_ARGUMENT);
    assert(!ge_original_stage_safe_runtime_register_relation(
        &other_runtime, &invalid));
    assert(other_runtime.status == GE_ORIGINAL_STAGE_SAFE_RUNTIME_NOT_BOUND);

    ge_original_stage_safe_runtime_close(&runtime);
    assert(runtime.bound == 0U && runtime.head == NULL);
    assert(!ge_original_stage_safe_runtime_can_pickup(&runtime, &item));
    assert(runtime.status == GE_ORIGINAL_STAGE_SAFE_RUNTIME_NOT_BOUND);
    assert(strcmp(ge_original_stage_safe_runtime_status_name(
        GE_ORIGINAL_STAGE_SAFE_RUNTIME_OK), "ok") == 0);
    puts("canonical safe relation runtime passed");
    return 0;
}
