#ifndef GE_ORIGINAL_MULTIPLAYER_WATCH_H
#define GE_ORIGINAL_MULTIPLAYER_WATCH_H
#include <stdint.h>
typedef enum GeOriginalMultiplayerWatchStatus {
    GE_ORIGINAL_MP_WATCH_OK,
    GE_ORIGINAL_MP_WATCH_INVALID_ROSTER,
    GE_ORIGINAL_MP_WATCH_INVALID_STATS,
    GE_ORIGINAL_MP_WATCH_RESOURCE_FAILED
} GeOriginalMultiplayerWatchStatus;
typedef struct GeOriginalMultiplayerWatchState {
    int32_t who_paused,chevron_glow,chevron_timer,alternate_message,alternate_timer;
    int32_t gamemode,selected_count,favorite_weapon[4][2];
} GeOriginalMultiplayerWatchState;
/* Publishes actual frontend multiplayer mode/count for the already-created
 * roster. Does not reset who_paused, favorites, native menu state or RNG. */
GeOriginalMultiplayerWatchStatus ge_original_multiplayer_watch_bind(void);
/* Exact mpwatchMenuTick after MoveBond/watch time, before camera/props. All
 * players, including paused/game-over players, must reach this boundary. */
GeOriginalMultiplayerWatchStatus ge_original_multiplayer_watch_tick(void);
/* Full original mpCalculateAwards including metric/tie/award RNG, favorite
 * weapons, text publication and native SFX/music/mission transitions. */
GeOriginalMultiplayerWatchStatus ge_original_multiplayer_watch_awards(int delay);
int ge_original_multiplayer_watch_snapshot(GeOriginalMultiplayerWatchState *);
/* Resolves the two legacy native player text words. ARM keeps native pointer
 * words; wider hosts use explicit per-field tokens and retained text pointers.
 * Never cast a host's legacy word directly to a readable string pointer. */
int ge_original_multiplayer_watch_award_text(uint32_t player,uint32_t award,
    const char **text);
#endif
