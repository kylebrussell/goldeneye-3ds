#ifndef GE_ORIGINAL_MODEL178_RUNTIME_H
#define GE_ORIGINAL_MODEL178_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#define GE_ORIGINAL_MODEL178_ID 178
#define GE_ORIGINAL_MODEL178_BLOB_SIZE 1488U
#define GE_ORIGINAL_MODEL178_PITEM_SCALE 1.0f
#define GE_ORIGINAL_MODEL178_ASSET_PATH "converted/models/damgatedoor/model.bin"

typedef enum GeOriginalModel178Status {
    GE_ORIGINAL_MODEL178_OK = 0,
    GE_ORIGINAL_MODEL178_INVALID_ARGUMENT,
    GE_ORIGINAL_MODEL178_INVALID_SIZE,
    GE_ORIGINAL_MODEL178_INVALID_LAYOUT,
    GE_ORIGINAL_MODEL178_ALLOCATION_FAILED
} GeOriginalModel178Status;

typedef struct GeOriginalModel178 GeOriginalModel178;
GeOriginalModel178 *ge_original_model178_create(
    const void *source_blob, size_t source_size,
    GeOriginalModel178Status *status);
void ge_original_model178_destroy(GeOriginalModel178 *runtime);
int32_t ge_original_model178_model_load(void *context, int32_t model_id);
int ge_original_model178_resolve_instance(
    void *context, int32_t model_id, void **model_header,
    void **model_instance, float *pitem_scale);

#endif
