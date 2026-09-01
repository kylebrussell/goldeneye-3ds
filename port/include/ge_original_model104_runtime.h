#ifndef GE_ORIGINAL_MODEL104_RUNTIME_H
#define GE_ORIGINAL_MODEL104_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#define GE_ORIGINAL_MODEL104_ID 104
#define GE_ORIGINAL_MODEL104_BLOB_SIZE 448U
#define GE_ORIGINAL_MODEL104_PITEM_SCALE 0.1f
#define GE_ORIGINAL_MODEL104_ASSET_PATH "converted/models/window/model.bin"

typedef enum GeOriginalModel104Status {
    GE_ORIGINAL_MODEL104_OK = 0,
    GE_ORIGINAL_MODEL104_INVALID_ARGUMENT,
    GE_ORIGINAL_MODEL104_INVALID_SIZE,
    GE_ORIGINAL_MODEL104_INVALID_LAYOUT,
    GE_ORIGINAL_MODEL104_ALLOCATION_FAILED
} GeOriginalModel104Status;

typedef struct GeOriginalModel104 GeOriginalModel104;

GeOriginalModel104 *ge_original_model104_create(
    const void *source_blob, size_t source_size,
    GeOriginalModel104Status *status);
void ge_original_model104_destroy(GeOriginalModel104 *runtime);
int32_t ge_original_model104_model_load(void *context, int32_t model_id);
int ge_original_model104_resolve_instance(
    void *context, int32_t model_id, void **model_header,
    void **model_instance, float *pitem_scale);

#endif
