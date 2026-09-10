#include <stdint.h>

#if UINTPTR_MAX > UINT32_MAX
#include "ge_original_model_callbacks.h"
#include <stdlib.h>

/* The retained source currently registers one function. Keep a checked table
 * for additional typed callbacks; its tokens never alias freed model storage. */
static GeOriginalModelRootCallback root_callbacks[32];
static size_t root_callback_count;

void ge_original_model_root_callback_set(Model *model,
                                         GeOriginalModelRootCallback callback)
{
    size_t index;
    if (!model) abort();
    if (!callback) {
        model->unka0 = 0;
        return;
    }
    for (index = 0; index < root_callback_count; ++index)
        if (root_callbacks[index] == callback) break;
    if (index == root_callback_count) {
        if (index == sizeof(root_callbacks) / sizeof(root_callbacks[0])) abort();
        root_callbacks[root_callback_count++] = callback;
    }
    model->unka0 = (s32)(index + 1);
}

s32 ge_original_model_root_callback_invoke(Model *model, coord3d *source,
                                           coord3d *destination, f32 *ground)
{
    if (!model || model->unka0 <= 0 ||
        (size_t)model->unka0 > root_callback_count) abort();
    return root_callbacks[model->unka0 - 1](model, source, destination, ground);
}
#endif
