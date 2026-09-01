#include <ultra64.h>
#include <bondtypes.h>

#include "game/gun.h"
#include "game/lv.h"
#include "random.h"

/* Unchanged decompiled chrpropTick beam-age body. */
void gunAdvanceBeamTimer(BeamRecord* beam)
{
    if (beam->unk00 >= 0)
    {
        if (g_ClockTimer < 3)
        {
#ifdef VERSION_US
            beam->unk28 += beam->unk20 * g_GlobalTimerDelta;
#else
            beam->unk28 += beam->unk20 * g_JP_GlobalTimerDelta;
#endif
        }
        else
        {
            beam->unk28 += beam->unk20 * (2.0f + ((f32) randomGetNext() * 2.3283064e-10f * 0.5f));
        }

        if (beam->unk1c <= beam->unk28)
        {
            beam->unk00 = -1;
            return;
        }

        beam->unk00++;
    }
}
