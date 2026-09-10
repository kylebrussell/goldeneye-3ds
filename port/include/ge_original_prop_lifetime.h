#ifndef GE_ORIGINAL_PROP_LIFETIME_H
#define GE_ORIGINAL_PROP_LIFETIME_H
#include <stdint.h>
/* Optional observation only: weak no-op definitions keep standalone native
 * slices linkable on ELF and Mach-O. Linking the state domain replaces these
 * with its strong observers; an unbound domain also leaves gameplay alone.
 * Registry faults never replace or cancel a canonical prop operation. */
#ifdef GE_PROP_LIFETIME_OBSERVER_IMPLEMENTATION
void ge_mp_state_props_allocated(void *prop);
void ge_mp_state_props_freeing(void *prop);
void ge_mp_state_props_resetting(void);
#else
__attribute__((weak, noinline)) void ge_mp_state_props_allocated(void *prop) { (void)prop; }
__attribute__((weak, noinline)) void ge_mp_state_props_freeing(void *prop) { (void)prop; }
__attribute__((weak, noinline)) void ge_mp_state_props_resetting(void) {}
#endif
#ifdef GE_PROP_ALLOCATION_SERIAL_IMPLEMENTATION
int ge_original_prop_allocation_serial(uint64_t *serial);
#else
/* No serial provider means the state domain is unavailable, never serial 0. */
__attribute__((weak, noinline)) int ge_original_prop_allocation_serial(uint64_t *serial)
{ (void)serial; return 0; }
#endif
static inline void ge_original_prop_observe_free(void *prop)
{ ge_mp_state_props_freeing(prop); }
static inline void ge_original_prop_observe_reset(void)
{ ge_mp_state_props_resetting(); }
#endif
