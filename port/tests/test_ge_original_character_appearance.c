#include "ge_original_character_appearance.h"

#include <bondconstants.h>
#include <bondtypes.h>
#include <ultra64.h>

#include <assert.h>
#include <stddef.h>

static const u32 random_samples[] = {100U, 7U, 3U, 2U, 4U};
static size_t random_index;

u32 randomGetNext(void)
{
    assert(random_index < sizeof(random_samples) / sizeof(random_samples[0]));
    return random_samples[random_index++];
}

/* The generated canonical character table retains chrkalash's original
 * loader hook. Appearance selection does not relocate that model. */
void modelCalculateRwDataLen(ModelFileHeader *header)
{
    (void)header;
}

int main(void)
{
    int32_t head = -1;
    int sunglasses = -1;

    ge_original_character_appearance_begin_stage();
    assert(random_index == 3U);

    assert(ge_original_character_appearance_choose_head(
        NULL, BODY_Russian_Soldier, &head));
    assert(head == HEAD_Male_Lee);
    assert(random_index == 4U);

    assert(ge_original_character_appearance_choose_head(
        NULL, BODY_Natalya_Skirt, &head));
    assert(head == HEAD_Female_Vivien);
    assert(random_index == 4U);

    assert(ge_original_character_appearance_choose_sunglasses(
        NULL, 2U, &sunglasses));
    assert(sunglasses == 1);
    assert(random_index == 5U);

    assert(!ge_original_character_appearance_choose_sunglasses(
        NULL, 0U, &sunglasses));
    assert(!ge_original_character_appearance_choose_head(
        NULL, -1, &head));
    assert(random_index == 5U);
    return 0;
}
