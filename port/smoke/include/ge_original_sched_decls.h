#ifndef GE_ORIGINAL_SCHED_DECLS_H
#define GE_ORIGINAL_SCHED_DECLS_H

/* Modern-compiler declarations missing from the original translation unit. */

#include "sched.h"

s32 __scTaskComplete(OSSched *scheduler, OSScTask *task);
s32 __scSchedule(
        OSSched *scheduler,
        OSScTask **sp_task,
        OSScTask **dp_task,
        s32 available_rcp);
void speedgraphMarkerUpdate(void);

#endif
