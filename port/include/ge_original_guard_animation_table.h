#ifndef GE_ORIGINAL_GUARD_ANIMATION_TABLE_H
#define GE_ORIGINAL_GUARD_ANIMATION_TABLE_H

#include <stddef.h>
#include <stdint.h>

int ge_original_guard_animation_table_bind(const uint8_t *segment,
                                            size_t segment_size);
int ge_original_guard_animation_entries_bind(const uint8_t *entries,
                                               size_t entries_size);
void ge_original_guard_animation_table_reset(void);
void *ge_port_guard_animation_resolve(uint32_t record_offset);
int32_t ge_port_guard_animation_load_frame(void *animation, int32_t frame);
const uint8_t *ge_port_guard_animation_frame_data(int32_t handle);
int ge_port_guard_animation_owns(const void *animation);

#endif
