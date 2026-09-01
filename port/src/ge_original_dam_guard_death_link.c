#include "ge_original_dam_guard_death_link.h"
#include "ge_original_dam_guard_ai_tick.h"

void ge_original_dam_guard_death_force_link(void)
{
    void (*volatile handler)(ChrRecord *);

    handler = ge_original_dam_guard_tick_die_exact;
    handler = ge_original_dam_guard_tick_dead_exact;
    (void)handler;
}
