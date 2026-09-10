#ifndef GE_ORIGINAL_PLAYER_BODY_H
#define GE_ORIGINAL_PLAYER_BODY_H

#include <stdint.h>

#define GE_ORIGINAL_PLAYER_BODY_MAX_PLAYERS 4U
#define GE_ORIGINAL_PLAYER_BODY_CHARACTER_COUNT 64U

struct player;

typedef enum GeOriginalPlayerBodyStatus {
    GE_ORIGINAL_PLAYER_BODY_OK = 0,
    GE_ORIGINAL_PLAYER_BODY_UNBOUND,
    GE_ORIGINAL_PLAYER_BODY_INVALID_PLAYER,
    GE_ORIGINAL_PLAYER_BODY_MULTIPLAYER_UNAVAILABLE,
    GE_ORIGINAL_PLAYER_BODY_CONSTRUCT_FAILED
} GeOriginalPlayerBodyStatus;

typedef struct GeOriginalPlayerBodyCharacter {
    int32_t body_id;
    int32_t head_id;
    uint8_t gender;
    float perspective_height;
} GeOriginalPlayerBodyCharacter;

typedef struct GeOriginalPlayerBodySnapshot {
    uint64_t load_requests;
    uint64_t successful_loads;
    uint64_t held_item_frontiers;
    int32_t body_id;
    int32_t head_id;
    GeOriginalPlayerBodyStatus status;
} GeOriginalPlayerBodySnapshot;

typedef int (*GeOriginalPlayerBodyConstruct)(
    void *context, struct player *player, int32_t body_id,
    int32_t head_id, float yaw);
typedef int (*GeOriginalPlayerBodyAttachHeldItem)(
    void *context, struct player *player, int32_t prop_id,
    int32_t item_id, uint32_t flags);

void ge_original_player_body_bind(
    void *context, GeOriginalPlayerBodyConstruct construct);
void ge_original_player_body_bind_held_item(
    void *context, GeOriginalPlayerBodyAttachHeldItem attach);
void ge_original_player_body_unbind(void *context);
void ge_original_player_body_reset(void);
void ge_original_player_body_snapshot(GeOriginalPlayerBodySnapshot *snapshot);
int ge_original_player_body_snapshot_player(
    uint32_t player_slot, GeOriginalPlayerBodySnapshot *snapshot);
/* Character index is the unchanged front.c multiplayer menu index. -1 uses
 * the original slot-specific default. Selection applies on next construction;
 * reset clears diagnostics only, so respawn keeps the selected character. */
int ge_original_player_body_set_character(int32_t player_slot, int32_t character);
int ge_original_player_body_character(
    int32_t character, GeOriginalPlayerBodyCharacter *selection);
uint16_t get_player_mp_char_head(int32_t player_slot);
uint16_t get_player_mp_char_body(int32_t player_slot);
uint8_t get_player_mp_char_gender(int player_slot);
float get_player_mp_char_height(int player_slot);
const char *ge_original_player_body_status_name(
    GeOriginalPlayerBodyStatus status);

/* Canonical game ABI reached by bondviewSetCameraMode. */
void solo_char_load(void);

#endif
