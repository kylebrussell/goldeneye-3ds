#ifndef GE_ORIGINAL_GUARD_AI_TRACE_H
#define GE_ORIGINAL_GUARD_AI_TRACE_H

#include <stdint.h>

typedef struct GeOriginalGuardAiLosStats {
    uint64_t calls;
    uint64_t clear_results;
    uint64_t blocked_results;
    uint64_t destination_tile_matches;
    void *last_start_tile;
    void *last_result_tile;
    void *last_collision_prop;
    uint8_t last_collision_prop_type;
    int32_t last_cdtypes;
    float last_start_x;
    float last_start_z;
    float last_destination_x;
    float last_destination_z;
    float last_fraction;
    float shortest_distance_squared;
    uint64_t shortest_calls;
    int32_t shortest_result;
    void *shortest_start_tile;
    void *shortest_result_tile;
    void *shortest_player_tile;
    void *shortest_collision_prop;
    uint8_t shortest_collision_prop_type;
    float shortest_start_x;
    float shortest_start_z;
    float shortest_destination_x;
    float shortest_destination_z;
    float shortest_fraction;
    uint64_t sight_check_calls;
    uint64_t sight_check_passes;
    uint64_t chr7_sight_check_calls;
    uint64_t chr7_sight_check_passes;
    int32_t last_sight_check_chr;
    uint64_t stopped_check_calls;
    uint64_t stopped_check_passes;
    uint64_t chr7_stopped_check_calls;
    uint64_t chr7_stopped_check_passes;
    uint64_t action_tick_calls;
    uint64_t chr7_action_tick_calls;
    uint64_t unknown_opcode_calls;
    uint64_t chr7_unknown_opcode_calls;
    uint64_t unknown_opcode_histogram[256];
    int32_t last_unknown_chr;
    int32_t last_unknown_offset;
    uint8_t last_unknown_opcode;
    void *last_unknown_list;
} GeOriginalGuardAiLosStats;

struct ChrRecord;
void ge_original_guard_ai_trace_action_tick(struct ChrRecord *self);
void ge_original_guard_ai_trace_unknown_opcode(struct ChrRecord *self,
    void *list, int32_t offset, uint8_t opcode);
void ge_original_guard_ai_los_trace_snapshot(
    GeOriginalGuardAiLosStats *stats);

#endif
