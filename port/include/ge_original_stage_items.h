#ifndef GE_ORIGINAL_STAGE_ITEMS_H
#define GE_ORIGINAL_STAGE_ITEMS_H

#include "ge_original_default_object.h"
#include "ge_original_pitem_models.h"
#include "ge_original_stage_prop_materializer.h"

#include <stddef.h>

typedef enum GeOriginalStageItemStatus {
    GE_ORIGINAL_STAGE_ITEM_OK=0,
    GE_ORIGINAL_STAGE_ITEM_INVALID_ARGUMENT,
    GE_ORIGINAL_STAGE_ITEM_INVALID_BRANCH,
    GE_ORIGINAL_STAGE_ITEM_MODEL_UNAVAILABLE,
    GE_ORIGINAL_STAGE_ITEM_CONSTRUCTION_FAILED,
    GE_ORIGINAL_STAGE_ITEM_PLACEMENT_FAILED,
    GE_ORIGINAL_STAGE_ITEM_OWNER_UNAVAILABLE
} GeOriginalStageItemStatus;

/* setupKey/setupHat and weaponAssignToHome's unassigned solo branch all enter
 * unchanged domakedefaultobj. This adapter supplies its preallocated prop and
 * invokes the already-retained exact prepare/construct/place bodies. */
GeOriginalStageItemStatus ge_original_stage_item_construct_standard_exact(
    const GeOriginalStagePropConstructionRequest *request,void *definition,
    void *prop,size_t prop_size,
    const GeOriginalDefaultObjectProviders *providers,
    GeOriginalDefaultObjectPrepared *prepared);

/* Exact domakedefaultobj INSIDEANOTHEROBJ branch and the later proplvreset2
 * owner reparent pass, shared by ordinary PROP/GLASS records,
 * keys/collectables/hats, and authored magazine supplies. `owner_prop` must
 * be the live object at command_index + authored pad; no placement or
 * top-level activation is synthesized. */
GeOriginalStageItemStatus ge_original_stage_item_construct_embedded_exact(
    const GeOriginalStagePropConstructionRequest *request,void *definition,
    void *prop,size_t prop_size,GeOriginalPitemModelProvider *models,
    void *owner_prop,void *collision_data,void **model_instance);

/* Exact domakedefaultobj ASSIGNEDTOCHR branch for authored ordinary
 * PROP/GLASS records. The platform resolves the literal character ID in
 * request->pad_id to the supplied live character prop; construction and
 * chrpropReparent remain canonical. */
GeOriginalStageItemStatus ge_original_stage_item_construct_assigned_exact(
    const GeOriginalStagePropConstructionRequest *request,void *definition,
    void *prop,size_t prop_size,GeOriginalPitemModelProvider *models,
    void *character_prop,void *collision_data,void **model_instance);

const char *ge_original_stage_item_status_name(GeOriginalStageItemStatus status);

#endif
