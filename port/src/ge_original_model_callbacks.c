#include <stdint.h>
#include "ge_original_model_callbacks.h"
#include <ultra64.h>
#include <bondtypes.h>

#if UINTPTR_MAX > UINT32_MAX
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

int ge_original_model_root_callback_read(const void *native_model,
                                         GeOriginalModelRootCallback *callback)
{
    const Model *model=native_model;
    if(!callback)return 0;
    *callback=NULL;
    if(!model)return 0;
    if(!model->unka0)return 1;
#if UINTPTR_MAX > UINT32_MAX
    if(model->unka0<=0 || (size_t)model->unka0>root_callback_count)return 0;
    *callback=root_callbacks[model->unka0-1];
#else
    *callback=(GeOriginalModelRootCallback)(uintptr_t)(uint32_t)model->unka0;
#endif
    return 1;
}
