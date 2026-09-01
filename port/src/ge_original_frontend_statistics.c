#include "ge_original_frontend_statistics.h"

#include <stddef.h>
#include <string.h>

int ge_original_frontend_statistics_snapshot(
    const int32_t shot_register[GE_ORIGINAL_FRONTEND_SHOT_REGISTER_COUNT],
    int32_t kill_count,
    const GeOriginalFrontendHeldWeapon
        held[GE_ORIGINAL_FRONTEND_HELD_WEAPON_COUNT],
    GeOriginalFrontendStatistics *statistics)
{
    int32_t favorite_right = 0;
    int32_t favorite_left = 0;
    int32_t most_time = -1;
    size_t index;
    if (statistics == NULL || shot_register == NULL || held == NULL) return 0;
    memset(statistics, 0, sizeof(*statistics));
    memcpy(statistics->shot_register, shot_register,
        sizeof(statistics->shot_register));
    statistics->kill_count = kill_count;
    /* Unchanged bondinvGetWeaponOfChoice: ties retain the first held record,
     * and an empty table reports unarmed in both hands. */
    for (index = 0U; index < GE_ORIGINAL_FRONTEND_HELD_WEAPON_COUNT; ++index) {
        if (held[index].total_time >= 0
                && held[index].total_time > most_time) {
            most_time = held[index].total_time;
            favorite_right = held[index].weapon1;
            favorite_left = held[index].weapon2;
        }
    }
    statistics->favorite_weapon_right = favorite_right;
    statistics->favorite_weapon_left = favorite_left;
    statistics->favorite_weapon_dual = (uint8_t)(favorite_right > 0
        && favorite_left == favorite_right);
    return 1;
}
