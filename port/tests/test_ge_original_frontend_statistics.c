#include "ge_original_frontend_statistics.h"

#include <assert.h>
#include <string.h>

int main(void)
{
    GeOriginalFrontendStatistics statistics;
    GeOriginalFrontendHeldWeapon held[10];
    int32_t shots[7];
    size_t index;
    memset(held, 0, sizeof(held));
    for (index = 0U; index < 7U; ++index) shots[index] = (int32_t)(20U + index);
    for (index = 0U; index < 10U; ++index)
        held[index].total_time = -1;
    held[2].total_time = 120;
    held[2].weapon1 = 3;
    held[2].weapon2 = 3;
    held[5].total_time = 119;
    held[5].weapon1 = 10;
    /* A tie after the winner must retain the first source record. */
    held[8].total_time = 120;
    held[8].weapon1 = 12;
    assert(ge_original_frontend_statistics_snapshot(
        shots, 7, held, &statistics));
    for (index = 0U; index < 7U; ++index)
        assert(statistics.shot_register[index] == (int32_t)(20U + index));
    assert(statistics.kill_count == 7);
    assert(statistics.favorite_weapon_right == 3);
    assert(statistics.favorite_weapon_left == 3);
    assert(statistics.favorite_weapon_dual == 1U);
    assert(!ge_original_frontend_statistics_snapshot(NULL, 7, held,
        &statistics));
    assert(!ge_original_frontend_statistics_snapshot(shots, 7, NULL,
        &statistics));
    assert(!ge_original_frontend_statistics_snapshot(shots, 7, held, NULL));
    return 0;
}
