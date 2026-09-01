#include "ge_original_dam_monitor.h"

#include <ultra64.h>
#include <bondtypes.h>
#include "assets/oddtextures.h"
#include "game/chrai.h"

/* Exact authored propobj.c monitor command arrays used by the default
 * controller and Dam's command-290 image selection. Neither contains pointer
 * commands, so their original u32 command ABI is native on host and ARM. */
static u32 ge_mon_anim_00_bond[] = {
    MONUSEIMAGE(IMGBOND),
    MONHORZSCROLL(0x400, 20),
    MONHOLDTIME(20),
    MONVERTSCROLL(0x400, 20),
    MONRGBA(COLOR_BLACK, 20),
    MONHOLDTIME(20),
    MONZOOMSQUARE(0x200, 20),
    MONRGBA(COLOR_WHITE, 20),
    MONHOLDTIME(20),
    MONZOOMSQUARE(0x400, 20),
    MONHOLDTIME(20),
    MONLOOP()
};

static u32 ge_mon_anim_05_green_text_up[] = {
    MONUSEIMAGE(IMGTEXT),
    MONRGBA(COLOR_BARELYGREENOPAQUE, 1),
    MONVERTSCROLL(0xFFFFFE00, 80),
    MONHOLDTIME(120),
    MONVERTSCROLL(0xFFFFFF00, 20),
    MONHOLDTIME(120),
    MONVERTSCROLL(0xFFFFFF80, 10),
    MONHOLDTIME(40),
    MONVERTSCROLL(0xFFFFFE00, 40),
    MONHOLDTIME(60),
    MONVERTSCROLL(0xFFFFFFC0, 30),
    MONHOLDTIME(120),
    MONLOOP()
};

static const MonitorRecord ge_initial_monitor_controller = {
    ge_mon_anim_00_bond, 0, 0xFFFF, 0,
    0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
    1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
    0.5f, 0.0f, 0.0f, 0.5f, 0.5f,
    0.5f, 0.0f, 0.0f, 0.5f, 0.5f,
    0xff, 0xff, 0xff,
    0xff, 0xff, 0xff,
    0xff, 0xff, 0xff,
    0xff, 0xff, 0xff,
    1.0f, 0.0f
};

GeOriginalDamMonitorStatus ge_original_dam_monitor_initialize(
    void *monitor_record, int32_t image_num)
{
    MonitorRecord *monitor = monitor_record;
    u32 *image;
    if (monitor == NULL)
        return GE_ORIGINAL_DAM_MONITOR_INVALID_ARGUMENT;
    if (image_num != 5)
        return GE_ORIGINAL_DAM_MONITOR_UNSUPPORTED_IMAGE;

    /* setupSingleMonitor: monitor->Monitor = g_MonitorAnimController. */
    *monitor = ge_initial_monitor_controller;
    /* Exact selected monitorSetImageByNum case 5 followed by
     * save_ptr_monitor_ani_code_to_obj_ani_slot. */
    image = ge_mon_anim_05_green_text_up;
    monitor->cmdlist = image;
    monitor->offset = 0;
    return GE_ORIGINAL_DAM_MONITOR_OK;
}

const void *ge_original_dam_monitor_bond_commands(size_t *word_count)
{
    if (word_count != NULL)
        *word_count = sizeof(ge_mon_anim_00_bond)
            / sizeof(ge_mon_anim_00_bond[0]);
    return ge_mon_anim_00_bond;
}

const void *ge_original_dam_monitor_green_text_commands(
    size_t *word_count)
{
    if (word_count != NULL)
        *word_count = sizeof(ge_mon_anim_05_green_text_up)
            / sizeof(ge_mon_anim_05_green_text_up[0]);
    return ge_mon_anim_05_green_text_up;
}
