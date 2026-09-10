#include "ge_original_multiplayer_fog.h"
#include "ge_original_multiplayer_fog_state.h"
#include <ultra64.h>
#include "game/bgfog.h"
#include "game/player.h"
#include <math.h>
#include <string.h>

static int32_t ge_fog_level;
static uint32_t ge_fog_players;
static float ge_fog_visibility_scale;
static uint8_t ge_fog_ready, ge_fog_cinematic;
static uint64_t ge_fog_lifetime;

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
    if (!ge_fog_ready) {
        if (ge_fog_lifetime == UINT64_MAX) return 0;
        ge_original_multiplayer_fog_reset_exact();
        ++ge_fog_lifetime;
    }
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

uint64_t ge_original_multiplayer_fog_lifetime(void)
{
    return ge_fog_ready ? ge_fog_lifetime : 0;
}

int ge_original_multiplayer_fog_state_read(GeOriginalMultiplayerFogState *out)
{
    if (out == NULL) return 0;
    memset(out, 0, sizeof(*out));
    if (!ge_fog_ready || getPlayerCount() != (s32)ge_fog_players
            || !ge_original_multiplayer_fog_state_read_exact(out)) return 0;
    out->level_id = ge_fog_level;
    out->player_count = ge_fog_players;
    out->cinematic = ge_fog_cinematic;
    out->visibility_scale = ge_fog_visibility_scale;
    return 1;
}
