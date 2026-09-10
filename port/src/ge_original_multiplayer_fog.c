#include "ge_original_multiplayer_fog.h"
#include <ultra64.h>
#include "game/bgfog.h"
#include "game/player.h"
#include <math.h>
#include <string.h>

static int32_t ge_fog_level;
static uint32_t ge_fog_players;
static float ge_fog_visibility_scale;
static uint8_t ge_fog_ready, ge_fog_cinematic;

float ge_original_multiplayer_fog_visibility_scale(void)
{
    return ge_fog_visibility_scale;
}

void ge_original_multiplayer_fog_close(void)
{
    ge_original_multiplayer_fog_reset_exact();
    ge_fog_level = 0;
    ge_fog_players = 0;
    ge_fog_visibility_scale = 0;
    ge_fog_ready = ge_fog_cinematic = 0;
}

int ge_original_multiplayer_fog_load(int32_t level_id, int cinematic,
    uint32_t player_count, float visibility_scale)
{
    if (level_id < 0 || level_id > INT32_MAX - 900 || player_count == 0
            || player_count > 4 || getPlayerCount() != (s32)player_count
            || !isfinite(visibility_scale) || visibility_scale <= 0) return 0;
    if (!ge_fog_ready) ge_original_multiplayer_fog_reset_exact();
    ge_fog_visibility_scale = visibility_scale;
    fogLoadLevelEnvironment((s32)level_id, cinematic != 0);
    ge_fog_level = level_id;
    ge_fog_players = player_count;
    ge_fog_cinematic = cinematic != 0;
    ge_fog_ready = 1;
    return 1;
}

int ge_original_multiplayer_fog_snapshot(GeOriginalMultiplayerFogSnapshot *snapshot)
{
    if (!ge_fog_ready || snapshot == NULL) return 0;
    memset(snapshot, 0, sizeof(*snapshot));
    ge_original_multiplayer_fog_snapshot_exact(snapshot);
    snapshot->level_id = ge_fog_level;
    snapshot->player_count = ge_fog_players;
    snapshot->cinematic = ge_fog_cinematic;
    snapshot->visibility_scale = ge_fog_visibility_scale;
    return 1;
}
