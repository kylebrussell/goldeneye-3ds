#ifndef GE_ORIGINAL_PLAYER_BODY_H
#define GE_ORIGINAL_PLAYER_BODY_H

#include <stdint.h>

struct player;

typedef enum GeOriginalPlayerBodyStatus {
    GE_ORIGINAL_PLAYER_BODY_OK = 0,
    GE_ORIGINAL_PLAYER_BODY_UNBOUND,
    GE_ORIGINAL_PLAYER_BODY_INVALID_PLAYER,
    GE_ORIGINAL_PLAYER_BODY_MULTIPLAYER_UNAVAILABLE,
    GE_ORIGINAL_PLAYER_BODY_CONSTRUCT_FAILED
} GeOriginalPlayerBodyStatus;

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
const char *ge_original_player_body_status_name(
    GeOriginalPlayerBodyStatus status);

/* Canonical game ABI reached by bondviewSetCameraMode. */
void solo_char_load(void);

#endif
