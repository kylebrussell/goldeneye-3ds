#ifndef GE_ORIGINAL_DAM_GUARD_MODEL_H
#define GE_ORIGINAL_DAM_GUARD_MODEL_H

#include <stddef.h>
#include <stdint.h>

typedef struct GeOriginalDamGuardDisplayList {
    const void *node;
    uint32_t primary_offset;
    uint32_t secondary_offset;
    uint32_t vertex_offset;
} GeOriginalDamGuardDisplayList;

/* Exact generated greatguard2 body (Dam authored body id 37). */
int ge_original_dam_guard_model_prepare(void);
void *ge_original_dam_guard_model_header(void);
size_t ge_original_dam_guard_model_matrix_count(void);
const GeOriginalDamGuardDisplayList *
ge_original_dam_guard_model_display_lists(size_t *count);

#endif
