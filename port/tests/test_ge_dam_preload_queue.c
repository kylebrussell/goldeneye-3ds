#include "ge_dam_preload_queue.h"

#include <assert.h>
#include <stdio.h>

int main(void)
{
    static const uint8_t initial[] = {
        135U, 133U, 134U, 132U, 136U, 124U, 125U, 126U, 127U, 128U,
    };
    uint8_t controls[194] = {0};
    GeDamPreloadQueue queue;
    GeOriginalBgVisibilityProviders providers;
    uint8_t room;

    assert(ge_dam_preload_queue_init(&queue, 137U, 2U,
        initial, sizeof(initial)) == GE_DAM_PRELOAD_OK);
    assert(ge_dam_preload_queue_room_state(&queue, 135U)
        == GE_DAM_PRELOAD_ROOM_RESIDENT);
    assert(ge_dam_preload_queue_request(&queue, 135U) == 0U);
    assert(ge_dam_preload_queue_request(&queue, 51U) != 0U);
    assert(queue.pending_count == 1U && queue.accepted_count == 1U);
    assert(ge_dam_preload_queue_request(&queue, 51U) != 0U);
    assert(queue.pending_count == 1U && queue.duplicate_count == 1U);
    assert(ge_dam_preload_queue_request(&queue, 52U) != 0U);
    assert(queue.pending_count == 2U);
    assert(ge_dam_preload_queue_request(&queue, 53U) != 0U);
    assert(queue.pending_count == 2U && queue.overflow_count == 1U);
    assert(ge_dam_preload_queue_peek(&queue, &room) == GE_DAM_PRELOAD_OK);
    assert(room == 51U);

    assert(ge_dam_preload_queue_pop(&queue, &room) == GE_DAM_PRELOAD_OK);
    assert(room == 51U);
    assert(ge_dam_preload_queue_complete(&queue, room, 1U)
        == GE_DAM_PRELOAD_OK);
    assert(ge_dam_preload_queue_request(&queue, 51U) == 0U);
    assert(ge_dam_preload_queue_pop(&queue, &room) == GE_DAM_PRELOAD_OK);
    assert(room == 52U);
    assert(ge_dam_preload_queue_complete(&queue, room, 0U)
        == GE_DAM_PRELOAD_OK);
    assert(ge_dam_preload_queue_request(&queue, 52U) != 0U);
    assert(ge_dam_preload_queue_pop(&queue, &room) == GE_DAM_PRELOAD_OK);
    assert(room == 52U);
    assert(ge_dam_preload_queue_complete(&queue, room, 1U)
        == GE_DAM_PRELOAD_OK);
    assert(ge_dam_preload_queue_pop(&queue, &room) == GE_DAM_PRELOAD_EMPTY);
    {
        static const uint8_t evicted[] = {135U, 51U};
        static const uint8_t duplicate[] = {133U, 133U};
        assert(ge_dam_preload_queue_evict_resident(
            &queue, duplicate, sizeof(duplicate))
            == GE_DAM_PRELOAD_INVALID_STATE);
        assert(ge_dam_preload_queue_room_state(&queue, 133U)
            == GE_DAM_PRELOAD_ROOM_RESIDENT);
        assert(ge_dam_preload_queue_evict_resident(
            &queue, evicted, sizeof(evicted)) == GE_DAM_PRELOAD_OK);
        assert(queue.eviction_count == 2U);
        assert(ge_dam_preload_queue_room_state(&queue, 135U)
            == GE_DAM_PRELOAD_ROOM_UNLOADED);
        assert(ge_dam_preload_queue_room_state(&queue, 51U)
            == GE_DAM_PRELOAD_ROOM_UNLOADED);
    }

    providers = ge_dam_preload_queue_providers(&queue, controls,
                                               sizeof(controls));
    assert(providers.context == &queue);
    assert(providers.preload_room == ge_dam_preload_queue_request);
    assert(providers.portal_controls == controls);
    assert(providers.portal_control_count == sizeof(controls));
    assert(ge_dam_preload_queue_init(&queue, 138U, 2U, NULL, 0U)
        == GE_DAM_PRELOAD_INVALID_ARGUMENT);
    puts("Dam exact global-vis preload request queue passed");
    return 0;
}
