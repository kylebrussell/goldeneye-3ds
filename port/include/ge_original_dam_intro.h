#ifndef GE_ORIGINAL_DAM_INTRO_H
#define GE_ORIGINAL_DAM_INTRO_H

#include <stdint.h>

typedef struct stagesetup stagesetup;

typedef struct GeOriginalSetupPadProviders {
    void *context;
    stagesetup *(*load_setup)(void *context, int32_t stage_id);
    float (*get_room_scale_reciprocal)(void *context);
    void *(*resolve_stan)(void *context, const char *name);
} GeOriginalSetupPadProviders;

typedef struct GeOriginalSetupPadState {
    int32_t stage_id;
    uint32_t pad_count;
    uint32_t resolved_pad_count;
    float room_scale_reciprocal;
    int loaded;
} GeOriginalSetupPadState;

void ge_original_setup_pad_bind(const GeOriginalSetupPadProviders *providers,
                                GeOriginalSetupPadState *state);
void ge_original_setup_pad_load(int32_t stage_id);

typedef struct GeOriginalIntroProviders {
    void *context;
    int32_t (*get_demo_slot)(void *context);
    float (*get_floor_y)(void *context, void *stan, float x, float z);
    float (*get_eye_height)(void *context);
    int32_t (*commit_player_spawn)(void *context,
                                   const float position[3],
                                   float floor_y,
                                   float eye_height,
                                   float look_angle_radians,
                                   void *stan);
} GeOriginalIntroProviders;

typedef struct GeOriginalIntroSpawnState {
    int32_t pad_index;
    uint32_t matching_spawn_count;
    uint32_t camera_count;
    uint32_t camera_index;
    float position[3];
    float floor_y;
    float look_angle_radians;
    float look_angle_degrees;
    const char *stan_name;
    void *stan;
    int player_committed;
    int loaded;
} GeOriginalIntroSpawnState;

typedef struct GeOriginalIntroLoadoutState {
    int32_t starting_weapon[2];
    uint32_t item_records;
    uint32_t ammo_records;
    uint32_t projectile_model_requests;
    int32_t bondtype;
    int loaded;
} GeOriginalIntroLoadoutState;

void ge_original_bond_intro_bind(const GeOriginalIntroProviders *providers,
                                 GeOriginalIntroSpawnState *state);

/* Bounded decompiled entry points retained in their original source files. */
void proplvreset2PadSlice(int32_t stage_id);
void bondviewLoadSetupIntroSpawnSlice(void);
/* Exact four random samples/state writes at the tail of
 * sets_a_bunch_of_BONDdata_values_to_default. */
void ge_original_spawn_player_initialize_idle_roll(void);
/* Exact INTROTYPE_ITEM/AMMO/CUFF and starting-weapon portion of
 * bondviewLoadSetupIntroSection, run after canonical player inventory and
 * hand storage have been initialized. */
int bondviewLoadSetupIntroLoadoutSlice(GeOriginalIntroLoadoutState *state);

#endif
