#include "ge_original_dam_setup.h"

#include <stddef.h>

#include "bondtypes.h"

extern stagesetup UsetupdamZ;

enum {
    DAM_NORMAL_SPAWN_RECORD_WORD = 63
};

stagesetup *ge_original_dam_setup_get(void)
{
    return &UsetupdamZ;
}

int ge_original_dam_setup_normal_spawn(GeOriginalDamSpawn *spawn)
{
    stagesetup *setup = ge_original_dam_setup_get();
    const s32 *record;
    s32 pad_id;
    const PadRecord *pad;

    if (spawn == NULL || setup == NULL || setup->intro == NULL
            || setup->pads == NULL) {
        return 0;
    }

    record = &setup->intro[DAM_NORMAL_SPAWN_RECORD_WORD];
    if (record[0] != INTROTYPE_SPAWN || record[2] != 0) {
        return 0;
    }
    pad_id = record[1];
    if (pad_id < 0) {
        return 0;
    }
    pad = &setup->pads[pad_id];
    if (pad->plink == NULL) {
        return 0;
    }

    spawn->pad_id = pad_id;
    spawn->position[0] = pad->pos.x;
    spawn->position[1] = pad->pos.y;
    spawn->position[2] = pad->pos.z;
    spawn->up[0] = pad->up.x;
    spawn->up[1] = pad->up.y;
    spawn->up[2] = pad->up.z;
    spawn->look[0] = pad->look.x;
    spawn->look[1] = pad->look.y;
    spawn->look[2] = pad->look.z;
    spawn->plink = pad->plink;
    return 1;
}
