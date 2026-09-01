#ifndef GE_ORIGINAL_GUNBARREL_BLOOD_H
#define GE_ORIGINAL_GUNBARREL_BLOOD_H

#include <stdint.h>

#define GE_ORIGINAL_GUNBARREL_BLOOD_WIDTH 96U
#define GE_ORIGINAL_GUNBARREL_BLOOD_HEIGHT 80U
#define GE_ORIGINAL_GUNBARREL_BLOOD_I4_BYTES \
    (GE_ORIGINAL_GUNBARREL_BLOOD_WIDTH \
        * GE_ORIGINAL_GUNBARREL_BLOOD_HEIGHT / 2U)

typedef struct GeOriginalGunbarrelBloodFrame {
    uint8_t pixels[GE_ORIGINAL_GUNBARREL_BLOOD_I4_BYTES];
    uint32_t generation;
    uint8_t ready;
} GeOriginalGunbarrelBloodFrame;

void ge_original_gunbarrel_blood_reset(
    GeOriginalGunbarrelBloodFrame *frame);
int ge_original_gunbarrel_blood_tick(void *context, int mode);

#endif
