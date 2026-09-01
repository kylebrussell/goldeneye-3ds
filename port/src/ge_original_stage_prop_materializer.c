#include "ge_original_stage_prop_materializer.h"

#include "bondconstants.h"

#include <string.h>

static GeOriginalStagePropService ge_stage_prop_service(uint8_t type)
{
    switch (type) {
    case PROPDEF_PROP: case PROPDEF_GLASS:
        return GE_ORIGINAL_STAGE_PROP_SERVICE_DEFAULT_OBJECT;
    case PROPDEF_DOOR:
        return GE_ORIGINAL_STAGE_PROP_SERVICE_DOOR;
    case PROPDEF_GUARD:
        return GE_ORIGINAL_STAGE_PROP_SERVICE_GUARD;
    case PROPDEF_KEY: case PROPDEF_COLLECTABLE: case PROPDEF_HAT:
        return GE_ORIGINAL_STAGE_PROP_SERVICE_ITEM;
    case PROPDEF_ALARM: case PROPDEF_CCTV: case PROPDEF_MAGAZINE:
    case PROPDEF_MONITOR: case PROPDEF_MULTI_MONITOR: case PROPDEF_RACK:
    case PROPDEF_AUTOGUN: case PROPDEF_AMMO: case PROPDEF_ARMOUR:
    case PROPDEF_GAS_RELEASING: case PROPDEF_VEHICHLE:
    case PROPDEF_AIRCRAFT: case PROPDEF_UNK41: case PROPDEF_SAFE:
    case PROPDEF_TANK: case PROPDEF_TINTED_GLASS:
        return GE_ORIGINAL_STAGE_PROP_SERVICE_SPECIAL_OBJECT;
    default:
        return GE_ORIGINAL_STAGE_PROP_SERVICE_CONTROL;
    }
}

static uint32_t ge_stage_prop_capability(GeOriginalStagePropService service)
{
    switch (service) {
    case GE_ORIGINAL_STAGE_PROP_SERVICE_DEFAULT_OBJECT:
        return GE_ORIGINAL_STAGE_PROP_CAP_DEFAULT_OBJECT;
    case GE_ORIGINAL_STAGE_PROP_SERVICE_DOOR:
        return GE_ORIGINAL_STAGE_PROP_CAP_DOOR;
    case GE_ORIGINAL_STAGE_PROP_SERVICE_GUARD:
        return GE_ORIGINAL_STAGE_PROP_CAP_GUARD;
    case GE_ORIGINAL_STAGE_PROP_SERVICE_ITEM:
        return GE_ORIGINAL_STAGE_PROP_CAP_ITEM;
    default: return 0U;
    }
}

static GeOriginalStagePropConstructCallback ge_stage_prop_callback(
    const GeOriginalStagePropMaterializerProviders *providers,
    GeOriginalStagePropService service)
{
    switch (service) {
    case GE_ORIGINAL_STAGE_PROP_SERVICE_DEFAULT_OBJECT:
        return providers->construct_default_object;
    case GE_ORIGINAL_STAGE_PROP_SERVICE_DOOR:
        return providers->construct_door;
    case GE_ORIGINAL_STAGE_PROP_SERVICE_GUARD:
        return providers->construct_guard;
    case GE_ORIGINAL_STAGE_PROP_SERVICE_ITEM:
        return providers->construct_item;
    case GE_ORIGINAL_STAGE_PROP_SERVICE_SPECIAL_OBJECT:
        return providers->construct_special_object;
    default: return NULL;
    }
}

int ge_original_stage_prop_construction_request(
    const GeOriginalStageSetupRuntime *runtime, size_t command_index,
    GeOriginalStagePropConstructionRequest *request)
{
    const GeOriginalStagePropRecord *record;
    GeOriginalStagePropService service;
    if (runtime == NULL || request == NULL || runtime->loaded == 0U
            || command_index >= runtime->prop_record_count) return 0;
    record = &runtime->prop_records[command_index];
    service = ge_stage_prop_service(record->type);
    memset(request, 0, sizeof(*request));
    request->runtime = runtime;
    request->record = record;
    request->command_index = command_index;
    request->service = service;
    request->model_id = record->model_id;
    request->pad_id = record->pad_id;
    if (service == GE_ORIGINAL_STAGE_PROP_SERVICE_DEFAULT_OBJECT
            || service == GE_ORIGINAL_STAGE_PROP_SERVICE_DOOR
            || service == GE_ORIGINAL_STAGE_PROP_SERVICE_ITEM
            || service == GE_ORIGINAL_STAGE_PROP_SERVICE_SPECIAL_OBJECT) {
        request->flags = record->words[2];
        request->flags2 = record->words[3];
        request->runtime_flags = record->words[25];
    }
    /* setupDoor indexes g_CurrentSetup.boundpads directly: the authored low
     * halfword is a bound-pad index, unlike the 10000-based encoding consumed
     * by ordinary object setup. Normalize only at this adapter boundary while
     * preserving the authored pad_id in the request and native definition. */
    request->placement_resolved = (uint8_t)
        ge_original_stage_setup_pad_placement(
            runtime,
            service == GE_ORIGINAL_STAGE_PROP_SERVICE_DOOR
                    && record->pad_id >= 0 && record->pad_id < 10000
                ? record->pad_id + 10000 : record->pad_id,
            &request->placement);
    return 1;
}

GeOriginalStagePropClassification ge_original_stage_prop_classify(
    const GeOriginalStagePropRecord *record,
    const GeOriginalStagePropMaterializerProviders *providers)
{
    GeOriginalStagePropClassification result = {
        GE_ORIGINAL_STAGE_PROP_SERVICE_CONTROL,
        GE_ORIGINAL_STAGE_PROP_UNSUPPORTED_BRANCH
    };
    uint32_t capability;
    if (record == NULL || providers == NULL) return result;
    result.service = ge_stage_prop_service(record->type);
    if (result.service == GE_ORIGINAL_STAGE_PROP_SERVICE_CONTROL) {
        result.blocker = GE_ORIGINAL_STAGE_PROP_CONTROL_ONLY;
        return result;
    }
    if (result.service == GE_ORIGINAL_STAGE_PROP_SERVICE_SPECIAL_OBJECT) {
        const uint32_t special_capability =
            record->type == PROPDEF_TINTED_GLASS
                ? GE_ORIGINAL_STAGE_PROP_CAP_TINTED_GLASS
                : (record->type == PROPDEF_MONITOR
                        || record->type == PROPDEF_MULTI_MONITOR)
                    ? GE_ORIGINAL_STAGE_PROP_CAP_MONITOR
                    : record->type == PROPDEF_CCTV
                        ? GE_ORIGINAL_STAGE_PROP_CAP_CCTV
                    : record->type == PROPDEF_AUTOGUN
                        ? GE_ORIGINAL_STAGE_PROP_CAP_AUTOGUN
                    : (record->type == PROPDEF_MAGAZINE
                            || record->type == PROPDEF_AMMO
                            || record->type == PROPDEF_ARMOUR)
                        ? GE_ORIGINAL_STAGE_PROP_CAP_SUPPLY
                    : record->type == PROPDEF_SAFE
                        ? GE_ORIGINAL_STAGE_PROP_CAP_SAFE
                    : record->type == PROPDEF_ALARM
                        ? GE_ORIGINAL_STAGE_PROP_CAP_ALARM
                    : record->type == PROPDEF_GAS_RELEASING
                        ? GE_ORIGINAL_STAGE_PROP_CAP_GAS_RELEASING
                    : (record->type == PROPDEF_RACK
                            || record->type == PROPDEF_VEHICHLE
                            || record->type == PROPDEF_AIRCRAFT
                            || record->type == PROPDEF_TANK)
                        ? GE_ORIGINAL_STAGE_PROP_CAP_MISC_OBJECT : 0U;
        const uint32_t accepted_capabilities =
            record->type == PROPDEF_SAFE
                ? GE_ORIGINAL_STAGE_PROP_CAP_SAFE
                    | GE_ORIGINAL_STAGE_PROP_CAP_MISC_OBJECT
                : record->type == PROPDEF_ALARM
                    ? GE_ORIGINAL_STAGE_PROP_CAP_ALARM
                        | GE_ORIGINAL_STAGE_PROP_CAP_MISC_OBJECT
                : record->type == PROPDEF_GAS_RELEASING
                    ? GE_ORIGINAL_STAGE_PROP_CAP_GAS_RELEASING
                        | GE_ORIGINAL_STAGE_PROP_CAP_MISC_OBJECT
                : record->type == PROPDEF_CCTV
                    ? GE_ORIGINAL_STAGE_PROP_CAP_CCTV
                        | GE_ORIGINAL_STAGE_PROP_CAP_SECURITY
                : record->type == PROPDEF_AUTOGUN
                    ? GE_ORIGINAL_STAGE_PROP_CAP_AUTOGUN
                        | GE_ORIGINAL_STAGE_PROP_CAP_SECURITY
                : special_capability;
        if (special_capability != 0U
                && (providers->capabilities & accepted_capabilities) != 0U
                && providers->construct_special_object != NULL) {
            const uint32_t owner_flags = record->words[2]
                & (PROPFLAG_ASSIGNEDTOCHR | PROPFLAG_INSIDEANOTHEROBJ);
            const int backward_owned_magazine =
                record->type == PROPDEF_MAGAZINE && record->pad_id < 0
                && owner_flags == PROPFLAG_INSIDEANOTHEROBJ;
            const int monitor_owned =
                (record->type == PROPDEF_MONITOR
                 || record->type == PROPDEF_MULTI_MONITOR)
                && (providers->capabilities
                    & GE_ORIGINAL_STAGE_PROP_CAP_MONITOR) != 0U
                && ((record->type == PROPDEF_MONITOR
                        && record->pad_id < 0
                        && owner_flags == 0U)
                    || owner_flags == PROPFLAG_INSIDEANOTHEROBJ);
            if (!backward_owned_magazine && !monitor_owned
                    && (record->pad_id < 0 || owner_flags != 0U)) {
                result.blocker = GE_ORIGINAL_STAGE_PROP_UNSUPPORTED_BRANCH;
                return result;
            }
            if (record->model_id >= 0
                    && (providers->model_available == NULL
                        || !providers->model_available(
                            providers->context, record->model_id))) {
                result.blocker = GE_ORIGINAL_STAGE_PROP_MISSING_MODEL;
                return result;
            }
            result.blocker = GE_ORIGINAL_STAGE_PROP_READY;
            return result;
        }
        result.blocker = GE_ORIGINAL_STAGE_PROP_UNSUPPORTED_BRANCH;
        return result;
    }
    capability = ge_stage_prop_capability(result.service);
    if ((providers->capabilities & capability) == 0U
            || (providers->construct == NULL
                && ge_stage_prop_callback(providers, result.service) == NULL)) {
        result.blocker = GE_ORIGINAL_STAGE_PROP_MISSING_SERVICE;
        return result;
    }
    /* The currently closed default-object body is the ordinary positive-pad
     * branch. Embedded and guard-assigned objects retain their exact records
     * but must wait for the canonical reparent/assignment passes. */
    if (result.service == GE_ORIGINAL_STAGE_PROP_SERVICE_DEFAULT_OBJECT
            && ((record->words[2] & PROPFLAG_ASSIGNEDTOCHR) != 0U
                || (record->pad_id < 0
                    && (record->words[2]
                        & PROPFLAG_INSIDEANOTHEROBJ) == 0U))) {
        result.blocker = GE_ORIGINAL_STAGE_PROP_UNSUPPORTED_BRANCH;
        return result;
    }
    if (record->model_id >= 0 && (providers->model_available == NULL
            || !providers->model_available(providers->context,
                                           record->model_id))) {
        result.blocker = GE_ORIGINAL_STAGE_PROP_MISSING_MODEL;
        return result;
    }
    result.blocker = GE_ORIGINAL_STAGE_PROP_READY;
    return result;
}

int ge_original_stage_prop_materialize_ready(
    const GeOriginalStageSetupRuntime *runtime,
    const GeOriginalStagePropMaterializerProviders *providers,
    GeOriginalStagePropMaterializerReport *report)
{
    size_t index;
    if (runtime == NULL || runtime->loaded == 0U || providers == NULL
            || report == NULL) return 0;
    memset(report, 0, sizeof(*report));
    report->records = runtime->prop_record_count;
    for (index = 0U; index < runtime->prop_record_count; ++index) {
        const GeOriginalStagePropRecord *record = &runtime->prop_records[index];
        GeOriginalStagePropClassification classification =
            ge_original_stage_prop_classify(record, providers);
        if ((unsigned)classification.service
                < GE_ORIGINAL_STAGE_PROP_SERVICE_COUNT) {
            ++report->service_counts[classification.service];
        }
        switch (classification.blocker) {
        case GE_ORIGINAL_STAGE_PROP_READY:
        {
            GeOriginalStagePropConstructCallback callback =
                ge_stage_prop_callback(providers, classification.service);
            GeOriginalStagePropConstructionRequest request;
            int request_ready = 0;
            const uint32_t owner_flags = record->words[2]
                & (PROPFLAG_ASSIGNEDTOCHR | PROPFLAG_INSIDEANOTHEROBJ);
            const int embedded_default = classification.service
                    == GE_ORIGINAL_STAGE_PROP_SERVICE_DEFAULT_OBJECT
                && (record->type == PROPDEF_PROP
                    || record->type == PROPDEF_GLASS)
                && owner_flags == PROPFLAG_INSIDEANOTHEROBJ;

            /* domakedefaultobj reaches getposstan only with a resolved STAN;
             * when init_pathtable_something left an authored pad unresolved,
             * getposstan returns zero and the original constructor deliberately
             * skips objInitWithAutoModel. Keep that canonical non-live branch
             * out of the platform's constructable READY subset. Its exact
             * INSIDEANOTHEROBJ branch is the exception: pad is a signed setup
             * command offset there, and the body performs objInit/model scale
             * without reading a pad, STAN, or placement. */
            if (classification.service
                    == GE_ORIGINAL_STAGE_PROP_SERVICE_DEFAULT_OBJECT
                    || (classification.service
                            == GE_ORIGINAL_STAGE_PROP_SERVICE_SPECIAL_OBJECT
                        && ((record->type==PROPDEF_MAGAZINE
                                && !(record->pad_id<0
                                    && (record->words[2]
                                        &(PROPFLAG_ASSIGNEDTOCHR
                                            |PROPFLAG_INSIDEANOTHEROBJ))
                                        ==PROPFLAG_INSIDEANOTHEROBJ))
                            ||record->type==PROPDEF_AMMO
                            ||record->type==PROPDEF_ARMOUR
                            ||record->type==PROPDEF_ALARM
                            ||record->type==PROPDEF_RACK
                            ||record->type==PROPDEF_GAS_RELEASING
                            ||record->type==PROPDEF_VEHICHLE
                            ||record->type==PROPDEF_AIRCRAFT
                            ||record->type==PROPDEF_SAFE
                            ||record->type==PROPDEF_TANK))) {
                request_ready = ge_original_stage_prop_construction_request(
                    runtime, index, &request);
                if (!request_ready || (!embedded_default
                        && (!request.placement_resolved
                            || request.placement.has_stan == 0U))) {
                    ++report->unsupported_branch;
                    break;
                }
            }
            ++report->ready;
            if (callback != NULL) {
                if ((request_ready
                        || ge_original_stage_prop_construction_request(
                            runtime, index, &request))
                        && callback(providers->context, &request)) {
                    ++report->constructed;
                } else {
                    ++report->failed;
                }
            } else if (providers->construct(providers->context,
                    classification.service, record, index)) {
                ++report->constructed;
            } else {
                ++report->failed;
            }
            break;
        }
        case GE_ORIGINAL_STAGE_PROP_CONTROL_ONLY:
            ++report->control_only;
            break;
        case GE_ORIGINAL_STAGE_PROP_MISSING_SERVICE:
            ++report->missing_service;
            break;
        case GE_ORIGINAL_STAGE_PROP_MISSING_MODEL:
            ++report->missing_model;
            break;
        case GE_ORIGINAL_STAGE_PROP_UNSUPPORTED_BRANCH:
            ++report->unsupported_branch;
            break;
        }
    }
    return report->constructed == report->ready && report->failed == 0U;
}

size_t ge_original_stage_prop_model_dependencies(
    const GeOriginalStageSetupRuntime *runtime,
    GeOriginalStagePropService service, int32_t *model_ids,
    size_t model_capacity)
{
    size_t count = 0U;
    size_t index;
    if (runtime == NULL || runtime->loaded == 0U
            || (unsigned)service >= GE_ORIGINAL_STAGE_PROP_SERVICE_COUNT) {
        return 0U;
    }
    for (index = 0U; index < runtime->prop_record_count; ++index) {
        const GeOriginalStagePropRecord *record = &runtime->prop_records[index];
        size_t prior;
        if (ge_stage_prop_service(record->type) != service
                || record->model_id < 0) continue;
        for (prior = 0U; prior < index; ++prior) {
            const GeOriginalStagePropRecord *candidate =
                &runtime->prop_records[prior];
            if (ge_stage_prop_service(candidate->type) == service
                    && candidate->model_id == record->model_id) break;
        }
        if (prior < index) continue;
        if (model_ids != NULL && count < model_capacity) {
            model_ids[count] = record->model_id;
        }
        ++count;
    }
    return count;
}

const char *ge_original_stage_prop_service_name(
    GeOriginalStagePropService service)
{
    switch (service) {
    case GE_ORIGINAL_STAGE_PROP_SERVICE_CONTROL: return "control";
    case GE_ORIGINAL_STAGE_PROP_SERVICE_DEFAULT_OBJECT: return "default object";
    case GE_ORIGINAL_STAGE_PROP_SERVICE_DOOR: return "door";
    case GE_ORIGINAL_STAGE_PROP_SERVICE_GUARD: return "guard";
    case GE_ORIGINAL_STAGE_PROP_SERVICE_ITEM: return "item";
    case GE_ORIGINAL_STAGE_PROP_SERVICE_SPECIAL_OBJECT: return "special object";
    default: return "unknown";
    }
}

const char *ge_original_stage_prop_blocker_name(
    GeOriginalStagePropBlocker blocker)
{
    switch (blocker) {
    case GE_ORIGINAL_STAGE_PROP_READY: return "ready";
    case GE_ORIGINAL_STAGE_PROP_CONTROL_ONLY: return "control only";
    case GE_ORIGINAL_STAGE_PROP_MISSING_SERVICE: return "missing service";
    case GE_ORIGINAL_STAGE_PROP_MISSING_MODEL: return "missing model";
    case GE_ORIGINAL_STAGE_PROP_UNSUPPORTED_BRANCH: return "unsupported branch";
    default: return "unknown";
    }
}
