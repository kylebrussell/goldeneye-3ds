#ifndef GE_ORIGINAL_DAM_SETUP_H
#define GE_ORIGINAL_DAM_SETUP_H

#include <stdint.h>

/*
 * Keep the generated setup's original stagesetup type opaque here.  Runtime
 * code which owns the original stage loader already gets the full definition
 * from bondtypes.h; platform code does not need a second layout definition.
 */
typedef struct stagesetup stagesetup;

/*
 * Returns the writable, generated UsetupdamZ object.  The original loader
 * rebases and scales fields in place, so this deliberately is not const.
 */
stagesetup *ge_original_dam_setup_get(void);

typedef struct GeOriginalDamSpawn {
    int32_t pad_id;
    float position[3];
    float up[3];
    float look[3];
    const char *plink;
} GeOriginalDamSpawn;

/* Reads the normal-play spawn from the generated intro and pad arrays. */
int ge_original_dam_setup_normal_spawn(GeOriginalDamSpawn *spawn);

#endif
