#ifndef GE_ORIGINAL_PROPS_PROFILE_H
#define GE_ORIGINAL_PROPS_PROFILE_H
#include <stdint.h>

/* Optional observational dispatch timing. Counters are character, object and
 * other (explosion/smoke/player) ticks followed by their call counts, then
 * nested action, animation, matrices and firing ticks. */
typedef uint64_t (*GeOriginalPropsProfileClock)(void *context);
extern uint64_t ge_original_props_profile[10];
void ge_original_props_profile_bind(GeOriginalPropsProfileClock clock,
                                    void *context);


#endif
