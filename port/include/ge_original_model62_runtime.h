#ifndef GE_ORIGINAL_MODEL62_RUNTIME_H
#define GE_ORIGINAL_MODEL62_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#define GE_ORIGINAL_MODEL62_ID 62
#define GE_ORIGINAL_MODEL62_BLOB_SIZE 1808U
#define GE_ORIGINAL_MODEL62_PITEM_SCALE 0.1f
#define GE_ORIGINAL_MODEL62_ASSET_PATH \
    "converted/models/chrwppksil/model.bin"

typedef enum GeOriginalModel62Status {
    GE_ORIGINAL_MODEL62_OK = 0,
    GE_ORIGINAL_MODEL62_INVALID_ARGUMENT,
    GE_ORIGINAL_MODEL62_INVALID_SIZE,
    GE_ORIGINAL_MODEL62_HASH_MISMATCH,
    GE_ORIGINAL_MODEL62_INVALID_LAYOUT,
    GE_ORIGINAL_MODEL62_ALLOCATION_FAILED
} GeOriginalModel62Status;

typedef struct GeOriginalModel62 GeOriginalModel62;

GeOriginalModel62 *ge_original_model62_create(
    const void *source_blob, size_t source_size,
    GeOriginalModel62Status *status);
void ge_original_model62_destroy(GeOriginalModel62 *runtime);

int32_t ge_original_model62_model_load(void *context, int32_t model_id);
int ge_original_model62_resolve_instance(
    void *context, int32_t model_id, void **model_header,
    void **model_instance, float *pitem_scale);
const char *ge_original_model62_status_name(GeOriginalModel62Status status);

#endif
