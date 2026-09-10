#ifndef GE_ORIGINAL_MODEL_CALLBACKS_H
#define GE_ORIGINAL_MODEL_CALLBACKS_H

#include <stdint.h>

#if UINTPTR_MAX > UINT32_MAX
#include <ultra64.h>
#include <bondtypes.h>

typedef s32 (*GeOriginalModelRootCallback)(Model *, coord3d *, coord3d *, f32 *);

/* Host-only handles for the N64 model's 32-bit callback word. Functions have
 * program lifetime, so copied models keep valid handles and animInit's native
 * zero clears the binding. No Model address or allocation lifetime is cached. */
void ge_original_model_root_callback_set(Model *model,
                                         GeOriginalModelRootCallback callback);
s32 ge_original_model_root_callback_invoke(Model *model, coord3d *source,
                                           coord3d *destination, f32 *ground);
#endif

#endif
