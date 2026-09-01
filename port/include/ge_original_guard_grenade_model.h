#ifndef GE_ORIGINAL_GUARD_GRENADE_MODEL_H
#define GE_ORIGINAL_GUARD_GRENADE_MODEL_H

#include <stddef.h>

/* Exact generated PchrgrenadeZ model used by chrGiveWeapon when an AI guard
 * takes the fresh-grenade branch of TRYThrowingGrenade. */
int ge_original_guard_grenade_model_prepare(void);
void *ge_original_guard_grenade_model_header(void);
size_t ge_original_guard_grenade_model_rw_words(void);

#endif
