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
/* Materializes an authored animation referenced either by its original table
 * offset or by an already-expanded pointer into the native table.  The
 * canonical animation pointer tables are expanded only once, while each stage
 * bind replaces their ROM-backed records, so callers must republish those
 * records after every bind.  Zero and one retain their canonical sentinel
 * meanings and are accepted without publishing a record. */
int ge_original_guard_animation_materialize_direct_entry(
    uintptr_t entry, uintptr_t native_table_base);
int32_t ge_port_guard_animation_load_frame(void *animation, int32_t frame);
const uint8_t *ge_port_guard_animation_frame_data(int32_t handle);
int ge_port_guard_animation_owns(const void *animation);

#endif
