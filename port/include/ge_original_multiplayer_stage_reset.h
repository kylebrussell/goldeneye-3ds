#ifndef GE_ORIGINAL_MULTIPLAYER_STAGE_RESET_H
#define GE_ORIGINAL_MULTIPLAYER_STAGE_RESET_H

/* Original pre-record intro assignments for verified live native owners.
 * The generated subset preserves source order and defines no new game state;
 * explicit missing cinematic/provider-only owners remain omitted. Call after
 * configure/setup preparation, before the first spawn rebuilds startpads and
 * starting weapons. See docs/MultiplayerStageIntroReset.md for the boundary. */
void ge_original_multiplayer_stage_intro_shared_reset_exact(void);

#endif
