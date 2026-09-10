#ifndef GE_MP_STATE_CORE_H
#define GE_MP_STATE_CORE_H

#include "ge_mp_state.h"
#include "ge_port.h"
#include "ge_original_input_state.h"

#define GE_MP_CORE_KIND 0xe0U
#define GE_MP_CORE_CREATION_NAMESPACE 255U
#define GE_MP_CORE_SLOT 0U
#define GE_MP_CORE_SCHEMA_VERSION 1U
#define GE_MP_CORE_SYSTEMS (GE_MP_SYSTEM_SESSION | GE_MP_SYSTEM_PLAYERS | GE_MP_SYSTEM_INPUT | GE_MP_SYSTEM_RNG)

/* Stable native field IDs. Paths name typed captured values, never raw regions. */
#define GE_MP_CORE_FIELD_LIST(X) \
    X(RNG, 0x100, U64, 1, rng) \
    X(CHR_RNG, 0x101, U64, 1, chr_rng) \
    X(RANDOM_BYTE, 0x102, I32, 1, random_byte) \
    X(PORT_TICKS, 0x110, U64, 1, port_ticks) \
    X(LASTFRAMECOUNTER, 0x120, I32, 1, lastframecounter) \
    X(CURRENTFRAMECOUNTER, 0x121, I32, 1, currentframecounter) \
    X(SPEEDGRAPHFRAMES, 0x122, I32, 1, speedgraphframes) \
    X(PREVIOUSFRAMECOUNTER, 0x123, I32, 1, previousframecounter) \
    X(HALFFRAMECOUNTER, 0x124, I32, 1, halfframecounter) \
    X(ISFRAMECOUNTERODD, 0x125, I32, 1, isframecounterodd) \
    X(HALFMINUSPREVIOUSCOUNTER, 0x126, I32, 1, halfminuspreviouscounter) \
    X(FRAMEDELAY, 0x127, I32, 1, framedelay) \
    X(G_CONTROLSLOCKEDFLAG, 0x140, I32, 1, g_controlslockedflag) \
    X(G_CLOCKTIMER, 0x141, I32, 1, g_clocktimer) \
    X(G_GLOBALTIMERDELTA, 0x142, F32, 1, g_globaltimerdelta) \
    X(G_GLOBALTIMER, 0x143, I32, 1, g_globaltimer) \
    X(D_80048380, 0x144, I32, 1, d_80048380) \
    X(G_CURRENTSTAGETOLOAD, 0x145, I32, 1, g_currentstagetoload) \
    X(D_80048388, 0x146, I32, 1, d_80048388) \
    X(D_8004838C, 0x147, I32, 1, d_8004838c) \
    X(D_80048390, 0x148, I32, 1, d_80048390) \
    X(D_80048394, 0x149, I32, 1, d_80048394) \
    X(G_MPTIME, 0x14a, I32, 1, g_mptime) \
    X(G_CURRENTMULTIPLAYERSEC, 0x14b, F32, 1, g_currentmultiplayersec) \
    X(D_800483A8, 0x14c, I32, 1, d_800483a8) \
    X(G_CURRENTMULTIPLAYERMIN, 0x14d, F32, 1, g_currentmultiplayermin) \
    X(D_800483B0, 0x14e, I32, 1, d_800483b0) \
    X(G_STAGETIMESEC, 0x14f, F32, 1, g_stagetimesec) \
    X(D_800483B8, 0x150, I32, 1, d_800483b8) \
    X(G_POWERONTIMESEC, 0x151, F32, 1, g_powerontimesec) \
    X(G_STAGENUM, 0x160, I32, 1, g_stagenum) \
    X(G_MAINSTAGENUM, 0x161, I32, 1, g_mainstagenum) \
    X(G_BOSSISDEBUGMENUOPEN, 0x162, I32, 1, g_bossisdebugmenuopen) \
    X(G_DEBUGANDUPDATESTAGEFLAG, 0x163, I32, 1, g_debugandupdatestageflag) \
    X(G_SHOWMEMUSEFLAG, 0x164, I32, 1, g_showmemuseflag) \
    X(G_SHOWMEMBARSFLAG, 0x165, I32, 1, g_showmembarsflag) \
    X(G_CURENTMMALLOCVALUE, 0x166, U32, 1, g_curentmmallocvalue) \
    X(G_CURENTMAMALLOCVALUE, 0x167, U32, 1, g_curentmamallocvalue) \
    X(PLAYER_NUM, 0x180, I32, 1, player_num) \
    X(SHUFFLED_ORDER, 0x181, I32, 4, shuffled_order) \
    X(PLAYER_TICK_COUNT, 0x182, I32, 1, player_tick_count) \
    X(CONTROL_STYLE, 0x183, I32, 1, control_style) \
    X(INPUT_MOVE_X, 0x200, F32, 4, input.move_x) \
    X(INPUT_MOVE_Y, 0x201, F32, 4, input.move_y) \
    X(INPUT_LOOK_X, 0x202, F32, 4, input.look_x) \
    X(INPUT_LOOK_Y, 0x203, F32, 4, input.look_y) \
    X(INPUT_BUTTONS, 0x204, U16, 4, input.buttons) \
    X(INPUT_SECONDARY_BUTTONS, 0x205, U16, 4, input.secondary_buttons) \
    X(INPUT_RAW_BUTTONS, 0x206, U16, 8, input.raw_buttons) \
    X(INPUT_RAW_X, 0x207, I8, 8, input.raw_x) \
    X(INPUT_RAW_Y, 0x208, I8, 8, input.raw_y) \
    X(INPUT_RAW_PADS, 0x209, I32, 1, input.raw_pads) \
    X(INPUT_DUAL_ANALOG, 0x20a, I32, 1, input.dual_analog) \
    X(INPUT_PLAYER_COUNT, 0x20b, U32, 1, input.player_count) \
    X(INPUT_SELECTED_PLAYER, 0x20c, U32, 1, input.selected_player) \
    X(INPUT_STICK_LIMIT, 0x20d, F32, 1, input.stick_limit) \
    X(INPUT_RAMROM_CONTROLLER_COUNT, 0x20e, U32, 1, input.ramrom_controller_count) \
    X(INPUT_PLAYBACK_KIND, 0x20f, U8, 1, input.playback_kind) \
    X(INPUT_RAMROM_REPLAY_PRESENT, 0x210, U8, 1, input.ramrom_replay_present) \
    X(INPUT_RAMROM_BLOCK_PRESENT, 0x211, U8, 1, input.ramrom_block_present) \
    X(JOY_BUTTONS, 0x240, U16, 320, joy.buttons) \
    X(JOY_STICK_X, 0x241, I8, 320, joy.stick_x) \
    X(JOY_STICK_Y, 0x242, I8, 320, joy.stick_y) \
    X(JOY_ERRORS, 0x243, U8, 320, joy.errors) \
    X(JOY_CURLAST, 0x244, I32, 2, joy.curlast) \
    X(JOY_CURSTART, 0x245, I32, 2, joy.curstart) \
    X(JOY_NEXTLAST, 0x246, I32, 2, joy.nextlast) \
    X(JOY_NEXTSECONDLAST, 0x247, I32, 2, joy.nextsecondlast) \
    X(JOY_PLAYBACK_COUNT, 0x248, I32, 2, joy.playback_count) \
    X(JOY_PRESSED, 0x249, U16, 16, joy.pressed) \
    X(JOY_SELECTED_BANK, 0x24a, I32, 1, joy.selected_bank) \
    X(JOY_CONNECTED_CONTROLLERS, 0x24b, U8, 1, joy.connected_controllers) \
    X(JOY_CONTROLLER_STATES, 0x24c, U8, 1, joy.controller_states) \
    X(JOY_PLAYBACK_PRESENT, 0x24d, U8, 1, joy.playback_present) \
    X(JOY_RECORD_PRESENT, 0x24e, U8, 1, joy.record_present)

enum {
#define GE_MP_CORE_ID(name, id, type, count, path) GE_MP_CORE_FIELD_##name = id,
    GE_MP_CORE_FIELD_LIST(GE_MP_CORE_ID)
#undef GE_MP_CORE_ID
};
enum {
    GE_MP_CORE_FIELD_COUNT = 0
#define GE_MP_CORE_COUNT(name, id, type, count, path) + 1
    GE_MP_CORE_FIELD_LIST(GE_MP_CORE_COUNT)
#undef GE_MP_CORE_COUNT
};

typedef struct GeMpCoreValues {
    uint64_t rng;
    uint64_t chr_rng;
    int32_t random_byte;
    int32_t lastframecounter;
    int32_t currentframecounter;
    int32_t speedgraphframes;
    int32_t previousframecounter;
    int32_t halfframecounter;
    int32_t isframecounterodd;
    int32_t halfminuspreviouscounter;
    int32_t framedelay;
    int32_t g_controlslockedflag;
    int32_t g_clocktimer;
    float g_globaltimerdelta;
    int32_t g_globaltimer;
    int32_t d_80048380;
    int32_t g_currentstagetoload;
    int32_t d_80048388;
    int32_t d_8004838c;
    int32_t d_80048390;
    int32_t d_80048394;
    int32_t g_mptime;
    float g_currentmultiplayersec;
    int32_t d_800483a8;
    float g_currentmultiplayermin;
    int32_t d_800483b0;
    float g_stagetimesec;
    int32_t d_800483b8;
    float g_powerontimesec;
    int32_t g_stagenum;
    int32_t g_mainstagenum;
    int32_t g_bossisdebugmenuopen;
    int32_t g_debugandupdatestageflag;
    int32_t g_showmemuseflag;
    int32_t g_showmembarsflag;
    uint32_t g_curentmmallocvalue;
    uint32_t g_curentmamallocvalue;
    int32_t player_num;
    int32_t player_tick_count;
    uint64_t port_ticks;
    int32_t shuffled_order[4], control_style;
    GeOriginalInputState input;
    GeOriginalJoyState joy;
} GeMpCoreValues;

typedef enum GeMpCoreBoundary {
    GE_MP_CORE_BOUNDARY_UNCLASSIFIED = 0,
    GE_MP_CORE_BOUNDARY_COMMITTED_RAW = 1
} GeMpCoreBoundary;

#define GE_MP_CORE_OMIT_OTHER_NATIVE_DOMAINS (UINT64_C(1) << 0)
#define GE_MP_CORE_OMIT_LEVEL_PROVIDER_CONTEXT (UINT64_C(1) << 1)
#define GE_MP_CORE_OMIT_INPUT_PROVIDER_ROSTER (UINT64_C(1) << 2)
#define GE_MP_CORE_OMIT_RAMROM_CONTENT (UINT64_C(1) << 3)
#define GE_MP_CORE_OMIT_UNKNOWN_PLAYBACK (UINT64_C(1) << 4)
#define GE_MP_CORE_OMIT_RECORDING_CALLBACK (UINT64_C(1) << 5)
#define GE_MP_CORE_OMIT_UNKNOWN_JOY_BANK (UINT64_C(1) << 6)
#define GE_MP_CORE_OMIT_NONCOMMITTED_CLOCK (UINT64_C(1) << 7)
#define GE_MP_CORE_OMIT_BUILD_VARIANT (UINT64_C(1) << 8)
#define GE_MP_CORE_OMIT_MULTIPLAYER_DEATH_GUARDS (UINT64_C(1) << 9)

typedef struct GeMpCoreDiagnostics {
    uint64_t omissions;
    /* Exact observations excluded only under COMMITTED_RAW: updateFrameCounters
     * writes wall-clock stamps; waitForNextFrame consumes them. Never hash these
     * local stamps as canonical or normalize them to fake peer convergence. */
    uint32_t os_count_previous, os_count_current;
} GeMpCoreDiagnostics;

typedef struct GeMpCoreState {
    GeMpCoreValues values;
    GeMpStateField fields[GE_MP_CORE_FIELD_COUNT];
    GeMpStateRecord record;
    GeMpCoreDiagnostics diagnostics;
} GeMpCoreState;

const GeMpRecordSchema *ge_mp_state_core_schema(void);
/* Capture only at a quiescent committed tick boundary. The caller owns the
 * singleton registry lifetime and the actual GePortState. Does not set any
 * schema.complete/coverage attestation. Returns INCOMPLETE because cross-domain
 * obligations remain; captured fields are still available for diagnostics.
 * INVALID arguments leave output untouched. Keep the result in place: record
 * fields and typed value pointers refer into this caller-owned object. */
GeMpStateStatus ge_mp_state_core_capture(GeMpCoreState *state,
    const GePortState *port_state, uint64_t actor_id, GeMpCoreBoundary boundary);

#endif
