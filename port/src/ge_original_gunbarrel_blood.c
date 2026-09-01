#include "ge_original_gunbarrel_blood.h"

#include <stddef.h>
#include <string.h>

#include "ge_original_gun_live.h"
#include "ge_original_player_spawn_internal.h"

extern int ge_port_blood_decode_frame(
    void *player, int mode, unsigned char *destination,
    uint32_t destination_size);

void ge_original_gunbarrel_blood_reset(
    GeOriginalGunbarrelBloodFrame *frame)
{
    if (frame != NULL) memset(frame, 0, sizeof(*frame));
}

int ge_original_gunbarrel_blood_tick(void *context, int mode)
{
    GeOriginalGunbarrelBloodFrame *frame = context;
    void *player = ge_original_spawn_player_get();
    GeOriginalDynFrameAudit audit;
    int complete;
    int owns_frame;

    if (frame == NULL || player == NULL || (mode != 0 && mode != 1))
        return 0;
    owns_frame = ge_original_gun_live_frame_begin();
    if (!owns_frame) return 0;
    complete = ge_port_blood_decode_frame(
        player, mode, frame->pixels, sizeof(frame->pixels));
    frame->ready = 1U;
    ++frame->generation;
    if (!ge_original_gun_live_frame_finalize(&audit)) {
        frame->ready = 0U;
        return 0;
    }
    return complete;
}
