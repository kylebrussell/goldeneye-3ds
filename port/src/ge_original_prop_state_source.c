/* Compile the selected original prop-state bodies from their canonical file.
 * Keep allocation itself unchanged; observe the returned physical slot. */
#define GE_PROP_ALLOCATION_SERIAL_IMPLEMENTATION
#include "ge_original_prop_lifetime.h"
#define chrpropAllocate ge_original_chrprop_allocate_exact
static struct PropRecord *ge_original_chrprop_allocate_exact(void);
#define GE_PORT_CHRPROP_STATE_SLICE
#include "../../src/game/chrprop.c"
#undef chrpropAllocate

static uint64_t ge_prop_allocation_serial;
static uint8_t ge_prop_allocation_overflow;
int ge_original_prop_allocation_serial(uint64_t *serial)
{
    if(!serial || ge_prop_allocation_overflow)return 0;
    *serial=ge_prop_allocation_serial;return 1;
}
PropRecord *chrpropAllocate(void)
{
    PropRecord *prop=ge_original_chrprop_allocate_exact();
    if(prop) {
        if(ge_prop_allocation_serial==UINT64_MAX)ge_prop_allocation_overflow=1;
        else ++ge_prop_allocation_serial;
        ge_mp_state_props_allocated(prop);
    }
    return prop;
}
