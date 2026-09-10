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

/* The native player keeps its original 32-bit animation-address word. Wide
 * hosts retain that word as a table-base + authored-offset token, resolving
 * it at the typed animation consumer. Lookup is read-only and requires an
 * already materialized record in the current animation owner lifetime. */
int32_t ge_original_guard_animation_legacy_offset(uint32_t offset,uintptr_t table_base);
int32_t ge_original_guard_animation_legacy_token(const void *animation,uintptr_t table_base);
void *ge_original_guard_animation_legacy_resolve(int32_t token,uintptr_t table_base);
int ge_original_guard_animation_legacy_reference(int32_t token,uintptr_t table_base,
    const void **animation,uint32_t *offset);

#endif
