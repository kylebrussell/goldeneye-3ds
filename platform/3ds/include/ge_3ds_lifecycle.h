#ifndef GE_3DS_LIFECYCLE_H
#define GE_3DS_LIFECYCLE_H

#include <stdbool.h>

/* Register after APT initialization and close before APT shutdown. These calls
 * belong on the thread that calls aptMainLoop; repeated init/close is harmless.
 * The callback only records a pending resume and does not call any services. */
void ge_3ds_lifecycle_init(void);
void ge_3ds_lifecycle_close(void);

/* Consume after aptMainLoop, before elapsed-time or input processing. Multiple
 * ONWAKEUP/ONRESTORE notifications coalesce until consumed. The caller must
 * rebase its wall clock, gate held inputs, and invalidate interrupted probes.
 * Sleep/suspend/exit notifications alone do not create a resume event. */
bool ge_3ds_lifecycle_take_resume(void);

#endif
