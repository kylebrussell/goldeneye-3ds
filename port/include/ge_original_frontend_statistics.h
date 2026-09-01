#ifndef GE_ORIGINAL_FRONTEND_STATISTICS_H
#define GE_ORIGINAL_FRONTEND_STATISTICS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    GE_ORIGINAL_FRONTEND_SHOT_REGISTER_COUNT = 7,
    GE_ORIGINAL_FRONTEND_HELD_WEAPON_COUNT = 10
};

typedef struct GeOriginalFrontendHeldWeapon {
    int32_t weapon1;
    int32_t weapon2;
    int32_t total_time;
} GeOriginalFrontendHeldWeapon;

typedef struct GeOriginalFrontendStatistics {
    int32_t shot_register[GE_ORIGINAL_FRONTEND_SHOT_REGISTER_COUNT];
    int32_t kill_count;
    int32_t favorite_weapon_right;
    int32_t favorite_weapon_left;
    uint8_t favorite_weapon_dual;
} GeOriginalFrontendStatistics;

/* Exact single-player statistic copy used by constructor_menu0D_missioncomplete
 * plus the unchanged bondinvGetWeaponOfChoice scan.  The portable contract
 * deliberately accepts snapshots: platform/live adapters retain ownership of
 * the large original player ABI, while this boundary preserves the source
 * ordering and first-on-tie behavior without mirroring that ABI. */
int ge_original_frontend_statistics_snapshot(
    const int32_t shot_register[GE_ORIGINAL_FRONTEND_SHOT_REGISTER_COUNT],
    int32_t kill_count,
    const GeOriginalFrontendHeldWeapon
        held[GE_ORIGINAL_FRONTEND_HELD_WEAPON_COUNT],
    GeOriginalFrontendStatistics *statistics);

#ifdef __cplusplus
}
#endif

#endif
