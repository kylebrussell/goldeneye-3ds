#include "ge_asset_pack.h"
#include "ge_port.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <random.h>
#include "game/frametiming.h"
#include "game/quaternion.h"

static void test_original_random_sequence(void)
{
    static const u32 expected[] = {
        0x40EC37CFU, 0x630AEDD7U, 0x1F58071EU, 0x0FDDE372U,
        0x59D9D424U, 0x31AEA908U, 0x7247D3A0U, 0x4419ED91U,
    };
    size_t i;

    g_randomSeed = 0xAB8D9F7781280783ULL;
    for (i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
        assert(randomGetNext() == expected[i]);
    }
}

static void test_original_quaternion_math(void)
{
    vec3f angles = {0.0f, 0.0f, 0.0f};
    quatf orientation;

    quaternion_set_rotation_around_xyzf(angles, orientation);
    assert(fabsf(orientation[0] - 1.0f) < 0.00001f);
    assert(fabsf(orientation[1]) < 0.00001f);
    assert(fabsf(orientation[2]) < 0.00001f);
    assert(fabsf(orientation[3]) < 0.00001f);
}

static void test_asset_pack(const char *filename)
{
    static const char expected[] = "GoldenEye asset pack beta\n";
    GeAssetPack pack;
    char data[sizeof(expected)] = {0};
    size_t bytes_read = 0;

    assert(ge_asset_pack_open(&pack, filename) == GE_ASSET_PACK_OK);
    assert(pack.entry_count == 2);
    assert(ge_asset_pack_find(&pack, "alpha.txt") != NULL);
    assert(ge_asset_pack_find(&pack, "missing.bin") == NULL);
    assert(ge_asset_pack_read(&pack, "sub/beta.txt", data, sizeof(data), &bytes_read) ==
           GE_ASSET_PACK_OK);
    assert(bytes_read == sizeof(expected) - 1);
    assert(memcmp(data, expected, sizeof(expected) - 1) == 0);
    assert(ge_asset_pack_read(&pack, "sub/beta.txt", data, 1, NULL) ==
           GE_ASSET_PACK_BUFFER_TOO_SMALL);
    ge_asset_pack_close(&pack);
}

typedef struct CatchupModelResult {
    uint64_t delivered_ticks;
    uint64_t dropped_ticks;
    unsigned peak_ticks_per_frame;
} CatchupModelResult;

/* Model a renderer whose frame costs one 60 Hz interval plus three intervals
 * for every gameplay tick it ran.  This is deliberately deterministic: an
 * unbounded scheduler feeds the work it just performed back into the next
 * frame, while the 3DS boundary admits at most one canonical tick. */
static CatchupModelResult run_catchup_model(int bounded)
{
    GePortState state;
    GePortInput input = {0};
    CatchupModelResult result = {0};
    double elapsed = 1.0 / GE_PORT_TICK_RATE;
    unsigned frame;

    ge_port_init(&state);
    for (frame = 0U; frame < 10U; ++frame) {
        unsigned ticks = bounded
            ? ge_port_advance_bounded(&state, elapsed, &input, 1U)
            : ge_port_advance(&state, elapsed, &input);

        result.delivered_ticks += ticks;
        if (ticks > result.peak_ticks_per_frame) {
            result.peak_ticks_per_frame = ticks;
        }
        elapsed = (1.0 + 3.0 * (double)ticks) / GE_PORT_TICK_RATE;
    }
    result.dropped_ticks = state.dropped_simulation_ticks;
    return result;
}

static void test_bounded_tick_input_and_catchup(void)
{
    GePortState state;
    GePortInput input = {0};
    CatchupModelResult unbounded;
    CatchupModelResult bounded;
    unsigned ticks;

    /* A render-rate input edge must wait for a simulation tick, be visible to
     * that one canonical joy sample, and then be consumed exactly once. */
    ge_port_init(&state);
    input.pressed = GE_PORT_ACTION_FIRE;
    ticks = ge_port_advance_bounded(
        &state, 1.0 / (GE_PORT_TICK_RATE * 2.0), &input, 1U);
    assert(ticks == 0U);
    assert((state.input.pressed & GE_PORT_ACTION_FIRE) != 0U);
    input.pressed = 0U;
    ticks = ge_port_advance_bounded(
        &state, 1.0 / (GE_PORT_TICK_RATE * 2.0), &input, 1U);
    assert(ticks == 1U);
    assert(state.original_buttons == Z_TRIG);
    assert(state.original_buttons_pressed == Z_TRIG);
    assert(state.input.pressed == 0U);
    ticks = ge_port_advance_bounded(
        &state, 1.0 / GE_PORT_TICK_RATE, &input, 1U);
    assert(ticks == 1U);
    assert(state.original_buttons == 0U);
    assert(state.original_buttons_pressed == 0U);

    /* Dropping overload after the admitted tick must not drop that tick's
     * edge or replay it on the following frame. */
    ge_port_init(&state);
    input.pressed = GE_PORT_ACTION_FIRE;
    ticks = ge_port_advance_bounded(&state, 0.1, &input, 1U);
    assert(ticks == 1U);
    assert(state.dropped_simulation_ticks == 5U);
    assert(state.original_buttons_pressed == Z_TRIG);
    input.pressed = 0U;
    ticks = ge_port_advance_bounded(
        &state, 1.0 / GE_PORT_TICK_RATE, &input, 1U);
    assert(ticks == 1U);
    assert(state.original_buttons_pressed == 0U);

    /* Whole excess ticks are discarded, but the fractional remainder stays
     * in the accumulator so a healthy subsequent frame keeps 60 Hz phase. */
    ge_port_init(&state);
    ticks = ge_port_advance_bounded(
        &state, 6.5 / GE_PORT_TICK_RATE, &input, 1U);
    assert(ticks == 1U);
    assert(state.dropped_simulation_ticks == 5U);
    assert(fabs(ge_port_frame_alpha(&state) - 0.5) < 0.000001);
    ticks = ge_port_advance_bounded(
        &state, 0.5 / GE_PORT_TICK_RATE, &input, 1U);
    assert(ticks == 1U);
    assert(state.dropped_simulation_ticks == 5U);
    assert(fabs(ge_port_frame_alpha(&state)) < 0.000001);

    unbounded = run_catchup_model(0);
    bounded = run_catchup_model(1);
    assert(unbounded.peak_ticks_per_frame >= 13U);
    assert(unbounded.delivered_ticks >= 100U);
    assert(unbounded.dropped_ticks == 0U);
    assert(bounded.peak_ticks_per_frame == 1U);
    assert(bounded.delivered_ticks == 10U);
    assert(bounded.dropped_ticks == 27U);
    printf("catch-up model: unbounded %llu ticks (peak %u/frame), "
           "bounded %llu ticks (peak %u/frame, %llu dropped)\n",
           (unsigned long long)unbounded.delivered_ticks,
           unbounded.peak_ticks_per_frame,
           (unsigned long long)bounded.delivered_ticks,
           bounded.peak_ticks_per_frame,
           (unsigned long long)bounded.dropped_ticks);
}

static void test_canonical_late_retrace_dispatch(void)
{
    GePortState state;
    GePortInput input = {0};
    unsigned dispatches;

    currentFrameCounter = 0;
    ge_port_init(&state);
    assert(ge_port_start_stage(&state, 33));
    input.pressed = GE_PORT_ACTION_FIRE;
    dispatches = ge_port_advance_retraces(&state, 15U, &input);
    assert(dispatches == 1U);
    assert(state.simulation_ticks == 1U);
    assert(state.dropped_simulation_ticks == 0U);
    assert(currentFrameCounter == 15);
    assert(state.original_clock_timer == 15);
    assert(state.original_global_timer == 15);
    assert(state.original_active_frame_updates == 1);
    assert(state.original_global_timer_delta == 15.0f);
    assert(state.original_stage_frames == 15);
    assert(fabsf(state.original_stage_seconds - 0.25f) < 0.00001f);
    assert(state.original_buttons_pressed == Z_TRIG);

    input.pressed = 0U;
    dispatches = ge_port_advance_retraces(&state, 0U, &input);
    assert(dispatches == 0U);
    assert(state.simulation_ticks == 1U);
    assert(currentFrameCounter == 15);
}

int main(int argc, char **argv)
{
    GePortState state;
    GePortState bounded_state;
    GePortInput input = {0};
    float view_x;
    float view_y;
    float view_z;
    unsigned ticks;

    test_original_random_sequence();
    test_original_quaternion_math();
    test_bounded_tick_input_and_catchup();
    test_canonical_late_retrace_dispatch();
    assert(argc == 2);
    test_asset_pack(argv[1]);

    currentFrameCounter = 0;
    ge_port_init(&state);
    assert(state.simulation_ticks == 0);
    assert(state.accumulator_seconds == 0.0);
    assert(state.view_orientation[0] == 1.0f);
    assert(state.original_stage == 90);
    assert(state.original_requested_stage == -1);
    assert(state.original_global_timer == 0);
    assert(state.original_active_frame_updates == 0);
    assert(ge_port_start_stage(&state, 33));
    assert(state.original_stage == 33);
    assert(state.original_requested_stage == -1);
    assert(!ge_port_start_stage(NULL, 33));

    input.pressed = GE_PORT_ACTION_FIRE;
    input.move_x = 0.5f;
    input.move_y = -0.25f;
    ticks = ge_port_advance(&state, 1.0 / 120.0, &input);
    assert(ticks == 0);
    assert((state.input.pressed & GE_PORT_ACTION_FIRE) != 0);

    input.pressed = 0;
    ticks = ge_port_advance(&state, 1.0 / 120.0, &input);
    assert(ticks == 1);
    assert(state.simulation_ticks == 1);
    assert(currentFrameCounter == 1);
    assert(state.original_clock_timer == 1);
    assert(state.original_global_timer == 1);
    assert(state.original_active_frame_updates == 1);
    assert(state.original_global_timer_delta == 1.0f);
    assert(state.input.pressed == 0);
    assert(fabsf(state.original_move_x - 0.5f) < 0.00001f);
    assert(fabsf(state.original_move_y + 0.25f) < 0.00001f);
    assert(state.original_buttons == Z_TRIG);
    assert(state.original_buttons_pressed == Z_TRIG);

    input.held = GE_PORT_ACTION_FIRE;
    ticks = ge_port_advance(&state, 1.0 / 60.0, &input);
    assert(ticks == 1);
    assert(state.original_buttons == Z_TRIG);
    assert(state.original_buttons_pressed == 0U);

    input.held = 0;
    ticks = ge_port_advance(&state, 1.0 / 60.0, &input);
    assert(ticks == 1);
    assert(state.original_buttons == 0U);
    assert(state.original_buttons_pressed == 0U);

    ticks = ge_port_advance(&state, 0.1, &input);
    assert(ticks == 6);
    assert(state.simulation_ticks == 9);
    assert(currentFrameCounter == 9);
    assert(state.original_global_timer == 9);
    assert(state.original_active_frame_updates == 9);
    assert(state.original_stage_frames == 9);
    assert(fabsf(state.original_stage_seconds - 9.0f / 60.0f) < 0.00001f);
    assert(ge_port_frame_alpha(&state) >= 0.0);
    assert(ge_port_frame_alpha(&state) < 1.0);

    input.look_x = 1.0f;
    input.look_y = -1.0f;
    ticks = ge_port_advance(&state, 1.0 / 60.0, &input);
    assert(ticks == 1);
    assert((state.original_buttons & R_CBUTTONS) != 0U);
    assert((state.original_buttons & D_CBUTTONS) != 0U);
    assert(state.view_yaw > 0.0f);
    assert(fabsf(state.original_look_x - 1.0f) < 0.00001f);
    assert(state.view_orientation[0] < 1.0f);
    ge_port_view_vector(&state, &view_x, &view_y, &view_z);
    assert(view_x > 0.0f);
    assert(view_y > 0.0f);
    assert(view_z > 0.0f);

    ticks = ge_port_advance(&state, -1.0, &input);
    assert(ticks == 0);

    /* A target with a fixed per-frame budget delivers the same canonical
     * ticks, but drops an overload backlog instead of replaying it forever. */
    ge_port_init(&bounded_state);
    input = (GePortInput){0};
    input.pressed = GE_PORT_ACTION_FIRE;
    input.held = GE_PORT_ACTION_FIRE;
    ticks = ge_port_advance_bounded(&bounded_state, 0.1, &input, 2U);
    assert(ticks == 2U);
    assert(bounded_state.simulation_ticks == 2U);
    assert(bounded_state.dropped_simulation_ticks == 4U);
    assert(bounded_state.original_buttons == Z_TRIG);
    assert(bounded_state.original_buttons_pressed == 0U);
    assert(ge_port_frame_alpha(&bounded_state) >= 0.0);
    assert(ge_port_frame_alpha(&bounded_state) < 1.0);
    ticks = ge_port_advance_bounded(
        &bounded_state, 1.0 / GE_PORT_TICK_RATE, &input, 1U);
    assert(ticks == 1U);
    assert(bounded_state.simulation_ticks == 3U);
    assert(bounded_state.dropped_simulation_ticks == 4U);

    puts("ge_port, asset pack, original boss/joy/level timer/RNG/frame timing/quaternion tests passed");
    return 0;
}
