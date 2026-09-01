#include "ge_original_prop_state.h"

#include <assert.h>
#include <string.h>

#include <ultra64.h>
#include <bondtypes.h>

enum { TEST_PROP_COUNT = 600 };
PropRecord g_Props[TEST_PROP_COUNT];

int main(void)
{
    PropRecord *guard = &g_Props[137];
    GeOriginalCharacterSceneState observed = {0};

    memset(g_Props, 0, sizeof(g_Props));
    guard->type = PROP_TYPE_CHR;
    guard->flags = PROPFLAG_ENABLED;
    guard->zDepth = 1234.5f;

    /* A portal-resident renderer may observe this state, but cannot turn a
     * canonical chrTick miss into ONSCREEN or manufacture a depth. */
    assert(ge_original_prop_state_observe_character_scene_state(
        guard, &observed));
    assert(observed.flags == PROPFLAG_ENABLED);
    assert(observed.zdepth == 1234.5f);
    assert(guard->flags == PROPFLAG_ENABLED);
    assert(guard->zDepth == 1234.5f);

    /* Conversely, an exact chrTick publication passes through unchanged. */
    guard->flags |= PROPFLAG_ONSCREEN;
    guard->zDepth = 77.25f;
    assert(ge_original_prop_state_observe_character_scene_state(
        guard, &observed));
    assert((observed.flags & PROPFLAG_ONSCREEN) != 0U);
    assert(observed.zdepth == 77.25f);
    assert((guard->flags & PROPFLAG_ONSCREEN) != 0U);
    assert(guard->zDepth == 77.25f);

    assert(!ge_original_prop_state_observe_character_scene_state(
        guard, NULL));
    guard->type = PROP_TYPE_OBJ;
    assert(!ge_original_prop_state_observe_character_scene_state(
        guard, &observed));
    return 0;
}
