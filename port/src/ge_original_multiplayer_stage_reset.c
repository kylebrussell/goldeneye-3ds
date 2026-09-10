#include "ge_original_multiplayer_stage_reset.h"
#include <ultra64.h>
#include <bondconstants.h>

extern enum CAMERAMODE g_CameraMode;
extern s32 g_bondviewForceDisarm, mission_timer;
#if defined(VERSION_JP) || defined(VERSION_EU)
extern f32 watch_time_0;
#else
extern s32 watch_time_0;
#endif

void ge_original_multiplayer_stage_intro_shared_reset_exact(void)
{
    /* bondview_r.c::bondviewLoadSetupIntroSection, before intro records and
     * spawn selection. Preserve the original value and native owner. */
    g_bondviewForceDisarm = 0;
    g_CameraMode = 0;
    mission_timer = 0;
    watch_time_0 = 0;
}
