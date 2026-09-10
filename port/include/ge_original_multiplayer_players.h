#ifndef GE_ORIGINAL_MULTIPLAYER_PLAYERS_H
#define GE_ORIGINAL_MULTIPLAYER_PLAYERS_H
#include <stdint.h>
struct player;
/* Stage-owned canonical storage. All allocation completes before publication;
 * a failed create preserves the prior game state. Close after freeing props
 * and model consumers. This allocates players, not a playable arena.
 * seed and chr_seed are independently agreed full 64-bit initial states of
 * the original gameplay and character/object RNG streams. Initialization
 * consumes the gameplay stream for random_byte. Both streams and the prior
 * persistent control setting are restored on destroy. */
int ge_original_multiplayer_players_create(uint32_t count, uint32_t local_slot,
                                          uint64_t seed, uint64_t chr_seed);
void ge_original_multiplayer_players_destroy(void);
int ge_original_multiplayer_select_player(uint32_t slot);
int ge_original_multiplayer_select_local_player(void);
uint32_t ge_original_multiplayer_player_count(void);
struct player *ge_original_multiplayer_player(uint32_t slot);
int32_t ge_original_multiplayer_control_style(int32_t slot);
/* Run once per player after allocating distinct hand buffers. */
int ge_original_multiplayer_player_bind_inventory(uint32_t slot);
#endif
