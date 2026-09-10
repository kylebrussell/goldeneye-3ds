#ifndef GE_ORIGINAL_MODEL_CALLBACKS_H
#define GE_ORIGINAL_MODEL_CALLBACKS_H

#include <stdint.h>

struct Model;
struct coord3d;
typedef int32_t (*GeOriginalModelRootCallback)(struct Model *, struct coord3d *, struct coord3d *, float *);

/* Read the actual typed callback, never its process-local host token. The
 * caller supplies a readable full Model. Invalid host tokens fail closed. */
int ge_original_model_root_callback_read(const void *native_model,
                                         GeOriginalModelRootCallback *callback);

#if UINTPTR_MAX > UINT32_MAX

/* Host-only handles for the N64 model's 32-bit callback word. Functions have
 * program lifetime, so copied models keep valid handles and animInit's native
 * zero clears the binding. No Model address or allocation lifetime is cached. */
void ge_original_model_root_callback_set(struct Model *model,
                                         GeOriginalModelRootCallback callback);
int32_t ge_original_model_root_callback_invoke(struct Model *model, struct coord3d *source,
                                           struct coord3d *destination, float *ground);
#endif

#endif
