#include <bondtypes.h>
#include "game/player.h"
#include <assert.h>

#define BLOOD_FRAME_BYTES (80U * 96U)

static struct player test_player;
struct player *g_CurrentPlayer = &test_player;
static u8 test_frame_arena[BLOOD_FRAME_BYTES * 2U + 16U];
static size_t test_frame_used;

extern u8 die_blood_image_1[];
extern s32 die_blood_image_routine(s32 mode);

void *dynAllocate(s32 size)
{
    size_t aligned = ((size_t)size + 15U) & ~(size_t)15U;
    void *allocation;

    assert(size >= 0);
    assert(test_frame_used + aligned <= sizeof(test_frame_arena));
    allocation = test_frame_arena + test_frame_used;
    test_frame_used += aligned;
    return allocation;
}

int main(void)
{
    unsigned frames = 0U;
    s32 finished = 0;

    while (!finished) {
        test_frame_used = 0U;
        finished = die_blood_image_routine(frames == 0U ? 0 : 1);
        assert(test_frame_used == BLOOD_FRAME_BYTES * 2U);
        assert(test_player.bloodImgNxt > die_blood_image_1);
        assert(++frames <= 42U);
    }
    assert(frames == 42U);

    /* Once the authored stream is exhausted, mode 1 must remain at its exact
     * endpoint instead of advancing into an unrelated native ELF section. */
    {
        u8 *endpoint = test_player.bloodImgNxt;
        test_frame_used = 0U;
        assert(die_blood_image_routine(1));
        assert(test_player.bloodImgNxt == endpoint);
    }

    return 0;
}
