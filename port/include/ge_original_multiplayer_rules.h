#ifndef GE_ORIGINAL_MULTIPLAYER_RULES_H
#define GE_ORIGINAL_MULTIPLAYER_RULES_H

#include <stddef.h>
#include <stdint.h>

#define GE_ORIGINAL_MP_PLAYERS 4
#define GE_ORIGINAL_MP_ARENAS 11

typedef struct GeOriginalMultiplayerConfig {
    int32_t player_count;
    int32_t scenario;
    int32_t weapon_set;
    int32_t level_id;
    uint8_t teams[GE_ORIGINAL_MP_PLAYERS];
} GeOriginalMultiplayerConfig;

typedef struct GeOriginalMultiplayerArena {
    int32_t level_id;
    int32_t max_players;
    int32_t unlock_after;
    const char *setup_key;
} GeOriginalMultiplayerArena;

typedef struct GeOriginalMultiplayerWeapon {
    int32_t item_id;
    int32_t model_id;
    float scale;
    int32_t ammo_type;
    int32_t ammo_amount;
    int32_t allow_pickup;
} GeOriginalMultiplayerWeapon;

typedef enum GeOriginalMultiplayerStatus {
    GE_ORIGINAL_MP_OK,
    GE_ORIGINAL_MP_INVALID_CONFIG,
    GE_ORIGINAL_MP_INVALID_PLAYER,
    GE_ORIGINAL_MP_INVALID_SETUP,
    GE_ORIGINAL_MP_RESPAWN_NOT_READY
} GeOriginalMultiplayerStatus;

/* Canonical frontend arena/scenario restrictions; unlock_after is an original
 * SP_LEVEL index and must be checked against the user's save by the frontend. */
int ge_original_multiplayer_arena(size_t index, GeOriginalMultiplayerArena *out);
GeOriginalMultiplayerStatus ge_original_multiplayer_validate(
    const GeOriginalMultiplayerConfig *config);
/* Caller allocates/publishes all players first; applies scenario, weapon set
 * and team identifiers to the canonical globals. Does not reset statistics. */
GeOriginalMultiplayerStatus ge_original_multiplayer_configure(
    const GeOriginalMultiplayerConfig *config);
int ge_original_multiplayer_weapon(int32_t slot, GeOriginalMultiplayerWeapon *out);
/* Suitable for stage supply provider context: pointer to the preceding authored
 * weapon slot (0..7), as carried by proplvreset2's lastmpweaponnum. */
int ge_original_multiplayer_ammo(void *context, int32_t *type, int32_t *amount);
GeOriginalMultiplayerStatus ge_original_multiplayer_score(int32_t player,
                                                          int32_t *score);
/* Exact death-render gate: death animation, red blood and fade must finish;
 * YOLT denies respawn after two recorded deaths. Trigger buttons 0xB000. */
int ge_original_multiplayer_can_respawn(int32_t player, uint16_t buttons);
/* Validate and publish all authored spawn pads before calling the original
 * handler. Pads must already have canonical STAN pointers resolved. */
GeOriginalMultiplayerStatus ge_original_multiplayer_publish_spawns(
    const int32_t *intro, size_t word_count, void *pads, size_t pad_count,
    int32_t demo_playback);
GeOriginalMultiplayerStatus ge_original_multiplayer_respawn(
    int32_t player, uint16_t buttons);

/* Canonical lv.c point/time/YOLT end decisions. Call once per simulation tick
 * before incrementing elapsed60. YOLT publishes original elimination order.
 * END means the caller must stop simulation and open canonical match results;
 * award/music/frontend presentation are separate dependencies. */
typedef enum GeOriginalMultiplayerEndDecision {
    GE_ORIGINAL_MP_END_INVALID = -1,
    GE_ORIGINAL_MP_END_RUNNING = 0,
    GE_ORIGINAL_MP_END_FREEZE = 1,
    GE_ORIGINAL_MP_END_RESULTS = 2
} GeOriginalMultiplayerEndDecision;
GeOriginalMultiplayerEndDecision ge_original_multiplayer_end_tick(
    int32_t tick60, int32_t elapsed60, int32_t time_limit60, int32_t point_limit);

#endif
