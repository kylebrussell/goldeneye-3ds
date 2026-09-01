#ifndef GE_ORIGINAL_DAM_OBJECTIVE_MODELS_RUNTIME_H
#define GE_ORIGINAL_DAM_OBJECTIVE_MODELS_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#define GE_ORIGINAL_MODEMBOX_MODEL_ID 335
#define GE_ORIGINAL_MODEMBOX_BLOB_SIZE 1984U
#define GE_ORIGINAL_MODEMBOX_ASSET_PATH \
    "converted/models/modembox/model.bin"
#define GE_ORIGINAL_MODEMBOX_PRIMARY_GDL_OFFSET UINT32_C(0x6a8)
#define GE_ORIGINAL_MODEMBOX_PRIMARY_VERTEX_OFFSET UINT32_C(0x0e8)
#define GE_ORIGINAL_MODEMBOX_SCREEN_GDL_OFFSET UINT32_C(0x768)
#define GE_ORIGINAL_MODEMBOX_SCREEN_VERTEX_OFFSET UINT32_C(0x600)
#define GE_ORIGINAL_SATDISH_MODEL_ID 70
#define GE_ORIGINAL_SATDISH_BLOB_SIZE 2864U
#define GE_ORIGINAL_SATDISH_ASSET_PATH \
    "converted/models/satdish/model.bin"
#define GE_ORIGINAL_SATDISH_PRIMARY_GDL_OFFSET UINT32_C(0x9d0)
#define GE_ORIGINAL_SATDISH_SECONDARY_GDL_OFFSET UINT32_C(0xad0)
#define GE_ORIGINAL_SATDISH_VERTEX_OFFSET UINT32_C(0x098)
#define GE_ORIGINAL_DAM_OBJECTIVE_PITEM_SCALE 0.1f

typedef enum GeOriginalDamObjectiveModelsStatus {
    GE_ORIGINAL_DAM_OBJECTIVE_MODELS_OK = 0,
    GE_ORIGINAL_DAM_OBJECTIVE_MODELS_INVALID_ARGUMENT,
    GE_ORIGINAL_DAM_OBJECTIVE_MODELS_INVALID_SIZE,
    GE_ORIGINAL_DAM_OBJECTIVE_MODELS_INVALID_LAYOUT,
    GE_ORIGINAL_DAM_OBJECTIVE_MODELS_ALLOCATION_FAILED
} GeOriginalDamObjectiveModelsStatus;

typedef struct GeOriginalDamObjectiveModels GeOriginalDamObjectiveModels;

GeOriginalDamObjectiveModels *ge_original_dam_objective_models_create(
    const void *modembox_blob, size_t modembox_size,
    const void *satdish_blob, size_t satdish_size,
    GeOriginalDamObjectiveModelsStatus *status);
void ge_original_dam_objective_models_destroy(
    GeOriginalDamObjectiveModels *runtime);
int32_t ge_original_dam_objective_models_model_load(
    void *context, int32_t model_id);
int ge_original_dam_objective_models_resolve_instance(
    void *context, int32_t model_id, void **model_header,
    void **model_instance, float *pitem_scale);

#endif
