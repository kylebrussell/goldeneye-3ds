#ifndef GE_ORIGINAL_CHARACTER_APPEARANCE_H
#define GE_ORIGINAL_CHARACTER_APPEARANCE_H

#include <stdint.h>

/* Exact reset_counter_rand_body_head + bodiesReset random-head state needed
 * before setup GUARD expansion. This consumes the original three RNG samples
 * in their original order (male head, female head, body). */
void ge_original_character_appearance_begin_stage(void);

/* Unchanged bodyChooseHead branch, exposed in the generic guard-runtime
 * callback ABI. */
int ge_original_character_appearance_choose_head(
    void *context, int32_t body_id, int32_t *head_id);

/* Unchanged retrieve_header_for_body_and_head random-sunglasses branch. */
int ge_original_character_appearance_choose_sunglasses(
    void *context, uint16_t appearance_flags, int *sunglasses);

#endif
