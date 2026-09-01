#ifndef GE_ORIGINAL_DAM_MONITOR_RENDER_H
#define GE_ORIGINAL_DAM_MONITOR_RENDER_H

#include <stdint.h>

typedef struct GeOriginalDamMonitorRenderVertex {
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t s;
    int16_t t;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
} GeOriginalDamMonitorRenderVertex;

typedef struct GeOriginalDamMonitorRenderSnapshot {
    const void *switch_node;
    const void *texture_config;
    uint32_t texture_id;
    uint8_t image_slot;
    uint8_t width;
    uint8_t height;
    uint8_t level;
    uint8_t format;
    uint8_t depth;
    uint8_t flags_s;
    uint8_t flags_t;
    uint8_t texture_mode;
    uint8_t texture_alpha_mode;
    uint16_t command_offset;
    int16_t pause60;
    GeOriginalDamMonitorRenderVertex vertices[4];
} GeOriginalDamMonitorRenderSnapshot;

/* Runs the unchanged process_monitor_animation_microcode body against the
 * model's authored Switches[0] collision-display-list node. The N64 display
 * list sink is captured as portable texture/vertex state for the 3DS
 * renderer; controller timing and mutations remain owned by the original. */
int ge_original_dam_monitor_render_tick(
    void *model_instance, void *monitor_record,
    uint32_t object_flags, uint32_t object_flags2,
    GeOriginalDamMonitorRenderSnapshot *snapshot);

#endif
