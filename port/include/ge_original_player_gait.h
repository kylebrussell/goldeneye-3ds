#ifndef GE_ORIGINAL_PLAYER_GAIT_H
#define GE_ORIGINAL_PLAYER_GAIT_H

#include <stddef.h>
#include <stdint.h>

#include "ge_original_animation_root.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GeOriginalPlayerGait GeOriginalPlayerGait;

typedef struct GeOriginalPlayerGaitTick {
    float max_speed;
    float percent_speed;
    float sideways_motion;
    float matrix0[4][4];
    float root_velocity[3];
} GeOriginalPlayerGaitTick;

typedef enum GeOriginalPlayerGaitStatus {
    GE_ORIGINAL_PLAYER_GAIT_OK = 0,
    GE_ORIGINAL_PLAYER_GAIT_INVALID_ARGUMENT,
    GE_ORIGINAL_PLAYER_GAIT_INVALID_EMBEDDED_MODEL,
    GE_ORIGINAL_PLAYER_GAIT_ALLOCATION_FAILED
} GeOriginalPlayerGaitStatus;

GeOriginalPlayerGait *ge_original_player_gait_create(
    const GeOriginalAnimationRoot *animation,
    GeOriginalPlayerGaitStatus *status);
/* Binds the exact gait header and animation runtime to an existing Model. This
 * is the path for g_CurrentPlayer->model, field_654 rwdata and
 * bondheadmatrices; the caller retains all supplied storage. */
GeOriginalPlayerGait *ge_original_player_gait_create_bound(
    const GeOriginalAnimationRoot *animation,
    void *native_model,
    uint32_t *native_rwdata,
    size_t native_rw_word_capacity,
    void *native_matrices,
    size_t native_matrix_capacity,
    GeOriginalPlayerGaitStatus *status);
GeOriginalPlayerGait *ge_original_player_gait_create_current_player(
    const GeOriginalAnimationRoot *animation,
    GeOriginalPlayerGaitStatus *status);
void ge_original_player_gait_bind_bond_animations(
    const GeOriginalAnimationRoot *walking,
    const GeOriginalAnimationRoot *sprinting);
/* Performs the exact standing-height calibration ordering from
 * initBondDATAdefaults: render authored idle frame zero, capture matrix 0/1,
 * then restore Bond's walking animation at its canonical loop frame. */
int ge_original_player_gait_calibrate_current_player_standing(
    GeOriginalPlayerGait *gait,
    const GeOriginalAnimationRoot *idle,
    const GeOriginalAnimationRoot *walking);
void ge_original_player_gait_destroy(GeOriginalPlayerGait *gait);

int ge_original_player_gait_set_animation(
    GeOriginalPlayerGait *gait,
    const GeOriginalAnimationRoot *animation,
    int flip,
    float start_frame,
    float speed,
    float merge);
void ge_original_player_gait_set_loop(GeOriginalPlayerGait *gait,
                                      float loop_frame,
                                      float end_frame,
                                      float loop_merge);

/* Advances the exact original model clock, subcalcpos and subcalcmatrices
 * bodies. The full authored four-matrix gait graph is decoded from ROM frames;
 * root_delta is derived from Bond's matrix 0 consumer. */
int ge_original_player_gait_tick_root(GeOriginalPlayerGait *gait,
                                      int32_t ticks,
                                      float matrices[4][4][4],
                                      float root_delta[3]);

/* Consumes only the original clock and movement fields already written on
 * the current player. It runs exact bhead selection/model matrices, derives
 * the canonical head-root velocity, then enters bheadUpdatePos/MoveBond/STAN. */
int ge_original_player_gait_current_player_movement_tick(
    GeOriginalPlayerGait *gait,
    int32_t clock_timer,
    float global_timer_delta,
    GeOriginalPlayerGaitTick *tick);

uint16_t ge_original_player_gait_rw_words(
    const GeOriginalPlayerGait *gait);
void *ge_original_player_gait_native_model(GeOriginalPlayerGait *gait);
const char *ge_original_player_gait_status_name(
    GeOriginalPlayerGaitStatus status);

#ifdef __cplusplus
}
#endif
#endif
