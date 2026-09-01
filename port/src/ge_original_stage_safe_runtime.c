#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>
#include <bondtypes.h>

#include "ge_original_stage_safe_runtime.h"

#include <stddef.h>
#include <string.h>

/* propobj.c owns this canonical list in the original. This bounded extraction
 * is its sole port owner until the complete unchanged propobj object is linked. */
ObjectRecord *g_LevelLoadPropSafeItem;

static GeOriginalStageSafeRuntime *ge_safe_runtime;

/* Unchanged src/game/initobjects.c body. */
void initSetLevelLoadPropSafeItem(struct ObjectRecord *arg0)
{
#if UINTPTR_MAX > UINT32_MAX
    /* The original cast aliases ObjectRecord::prop to SafeObjectRecord::next
     * in the 32-bit game ABI. Host sanitizers use 64-bit pointers, so spell
     * that same field publication without relying on the N64 offset alias. */
    ((SafeObjectRecord *)(void *)arg0)->next =
        (SafeObjectRecord *)(void *)g_LevelLoadPropSafeItem;
    g_LevelLoadPropSafeItem = arg0;
#else
    _Static_assert(offsetof(ObjectRecord, prop)
                       == offsetof(SafeObjectRecord, next),
        "safe relation next must alias ObjectRecord::prop in original ABI");
    arg0->prop = (PropRecord *)g_LevelLoadPropSafeItem;
    g_LevelLoadPropSafeItem = arg0;
#endif
}

/* Unchanged src/game/chrprop.c body. */
bool objCanPickupFromSafe(ObjectRecord *obj)
{
    if (obj->flags2 & PROPFLAG2_LINKEDTOSAFE)
    {
        SafeObjectRecord *link = (SafeObjectRecord *)g_LevelLoadPropSafeItem;

        while (link)
        {
            ObjectRecord *loopobj = link->item;

            if (obj == link->item && link->door && link->door->prop)
            {
                if (link->door->openPosition <= 0.5f)
                {
                    return FALSE;
                }
            }

            link = link->next;
        }
    }

    return TRUE;
}

void ge_original_stage_safe_runtime_bind(GeOriginalStageSafeRuntime *runtime)
{
    g_LevelLoadPropSafeItem = NULL;
    ge_safe_runtime = runtime;
    if (runtime == NULL) return;
    memset(runtime, 0, sizeof(*runtime));
    runtime->status = GE_ORIGINAL_STAGE_SAFE_RUNTIME_OK;
    runtime->bound = UINT8_C(1);
}

void ge_original_stage_safe_runtime_close(GeOriginalStageSafeRuntime *runtime)
{
    if (runtime == NULL) return;
    if (ge_safe_runtime == runtime) {
        g_LevelLoadPropSafeItem = NULL;
        ge_safe_runtime = NULL;
    }
    memset(runtime, 0, sizeof(*runtime));
}

int ge_original_stage_safe_runtime_register_relation(
    void *context, void *relation)
{
    GeOriginalStageSafeRuntime *runtime = context;
    SafeObjectRecord *safe_link = relation;
    SafeObjectRecord *cursor;
    if (runtime == NULL) return 0;
    if (safe_link == NULL) {
        runtime->status = GE_ORIGINAL_STAGE_SAFE_RUNTIME_INVALID_ARGUMENT;
        return 0;
    }
    if (runtime != ge_safe_runtime || runtime->bound == 0U) {
        runtime->status = GE_ORIGINAL_STAGE_SAFE_RUNTIME_NOT_BOUND;
        return 0;
    }
    if (safe_link->item == NULL || safe_link->item->prop == NULL
            || safe_link->safe == NULL || safe_link->safe->prop == NULL
            || safe_link->safe->type != PROPDEF_SAFE
            || safe_link->door == NULL || safe_link->door->prop == NULL
            || safe_link->door->type != PROPDEF_DOOR) {
        runtime->status = GE_ORIGINAL_STAGE_SAFE_RUNTIME_INVALID_RELATION;
        return 0;
    }
    for (cursor = (SafeObjectRecord *)g_LevelLoadPropSafeItem;
            cursor != NULL; cursor = cursor->next) {
        if (cursor == safe_link) {
            runtime->status =
                GE_ORIGINAL_STAGE_SAFE_RUNTIME_DUPLICATE_RELATION;
            return 0;
        }
    }
    initSetLevelLoadPropSafeItem((ObjectRecord *)safe_link);
    runtime->head = g_LevelLoadPropSafeItem;
    ++runtime->relation_count;
    ++runtime->generation;
    runtime->status = GE_ORIGINAL_STAGE_SAFE_RUNTIME_OK;
    return 1;
}

int ge_original_stage_safe_runtime_can_pickup(
    GeOriginalStageSafeRuntime *runtime, void *object_definition)
{
    ObjectRecord *object = object_definition;
    int allowed;
    if (runtime == NULL) return 0;
    if (object == NULL) {
        runtime->status = GE_ORIGINAL_STAGE_SAFE_RUNTIME_INVALID_ARGUMENT;
        return 0;
    }
    if (runtime != ge_safe_runtime || runtime->bound == 0U) {
        runtime->status = GE_ORIGINAL_STAGE_SAFE_RUNTIME_NOT_BOUND;
        return 0;
    }
    ++runtime->pickup_tests;
    allowed = objCanPickupFromSafe(object) != FALSE;
    runtime->blocked_pickups += allowed == 0;
    runtime->status = GE_ORIGINAL_STAGE_SAFE_RUNTIME_OK;
    return allowed;
}

const char *ge_original_stage_safe_runtime_status_name(
    GeOriginalStageSafeRuntimeStatus status)
{
    switch (status) {
    case GE_ORIGINAL_STAGE_SAFE_RUNTIME_OK: return "ok";
    case GE_ORIGINAL_STAGE_SAFE_RUNTIME_INVALID_ARGUMENT:
        return "invalid argument";
    case GE_ORIGINAL_STAGE_SAFE_RUNTIME_NOT_BOUND:return "not bound";
    case GE_ORIGINAL_STAGE_SAFE_RUNTIME_INVALID_RELATION:
        return "invalid relation";
    case GE_ORIGINAL_STAGE_SAFE_RUNTIME_DUPLICATE_RELATION:
        return "duplicate relation";
    }
    return "unknown";
}
