#ifndef GE_ORIGINAL_DAM_MONITOR_H
#define GE_ORIGINAL_DAM_MONITOR_H

#include <stddef.h>
#include <stdint.h>

typedef enum GeOriginalDamMonitorStatus {
    GE_ORIGINAL_DAM_MONITOR_OK = 0,
    GE_ORIGINAL_DAM_MONITOR_INVALID_ARGUMENT,
    GE_ORIGINAL_DAM_MONITOR_UNSUPPORTED_IMAGE
} GeOriginalDamMonitorStatus;

typedef struct GeOriginalDamMonitorSnapshot {
    const void *commands;
    int32_t owner_offset;
    int32_t owner_part;
    int32_t image_num;
    uint16_t command_offset;
    int16_t pause60;
    float rotation;
    float xscale;
    float yscale;
    float xmid;
    float ymid;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
} GeOriginalDamMonitorSnapshot;

/* Initializes the exact controller state copied by setupSingleMonitor and
 * executes Dam command 290's monitorSetImageByNum image-5 branch. */
GeOriginalDamMonitorStatus ge_original_dam_monitor_initialize(
    void *monitor_record, int32_t image_num);

/* Authored command arrays retained from propobj.c for validation and for the
 * future unchanged monitor animation interpreter. */
const void *ge_original_dam_monitor_bond_commands(size_t *word_count);
const void *ge_original_dam_monitor_green_text_commands(
    size_t *word_count);

#endif
