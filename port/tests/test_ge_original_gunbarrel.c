#include "ge_original_gunbarrel.h"

#include <bondconstants.h>

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

extern signed short sins(unsigned short x);

typedef struct BloodHarness {
    int reset_calls;
    int advance_calls;
} BloodHarness;

static int blood_tick(void *context, int mode)
{
    BloodHarness *harness = context;
    if (mode == 0) {
        ++harness->reset_calls;
        harness->advance_calls = 0;
        return 0;
    }
    assert(mode == 1);
    ++harness->advance_calls;
    return harness->advance_calls >= 42;
}

int main(void)
{
    GeOriginalGunbarrelAssets assets;
    GeOriginalGunbarrelState state;
    GeOriginalGunbarrelFrame frame;
    GeOriginalGunbarrelHoleVertex hole[
        GE_ORIGINAL_GUNBARREL_HOLE_VERTEX_COUNT];
    GeOriginalGunbarrelLayerHoleVertex layer_holes[
        GE_ORIGINAL_GUNBARREL_MAX_LAYER_HOLE_VERTICES];
    GeOriginalGunbarrelSightRect sight_rect;
    BloodHarness blood = {0};
    uint32_t mode_frames[10] = {0};
    int animation_starts = 0;
    int animation_speedups = 0;
    int shots = 0;
    int first_sway_seen = 0;
    int ticks;

    assert(strlen(ge_original_gunbarrel_contract_sha256()) == 64U);
    ge_original_gunbarrel_assets(&assets);
    assert(assets.body_model == BODY_Brosnan_Tuxedo);
    assert(assets.head_model == BODY_Male_Pierce_Bond_Tuxedo);
    assert(assets.gun_model == PROP_CHRWPPK);
    assert(assets.model_scale == 0.18779343f);
    assert(assets.animation_play_speed == 0.5f);
    assert(assets.animation_translation_scale == 1.0f);
    assert(assets.walk_animation_frame_backstep == 0x44);
    assert(assets.camera_position[0] == 1758.2957f);
    assert(assets.camera_position[1] == 220.0f);
    assert(assets.camera_position[2] == 684.28143f);
    assert(assets.field_of_view_degrees == 46.0f);
    assert(assets.perspective_aspect == 320.0f / 240.0f);
    assert(assets.perspective_near == 10.0f
        && assets.perspective_far == 10000.0f);
    assert(assets.logical_width == 1280U && assets.logical_height == 960U);
    assert(assets.native_width == 440U && assets.native_height == 330U);
    assert(assets.sight_width == 440U && assets.sight_height == 299U
        && assets.sight_y == 16U);
    assert(assets.backdrop_offset_x == 768.0f
        && assets.backdrop_offset_y == -40.0f);
    assert(assets.backdrop_scale_x == 2.7f
        && assets.backdrop_scale_y == 2.57f);
    assert(assets.blood_width == 96U && assets.blood_height == 80U);
    assert(assets.blood_red == 150U && assets.blood_green == 0U
        && assets.blood_blue == 0U && assets.blood_alpha == 180U);
    assert(ge_original_gunbarrel_build_hole(NULL, 0U) == 0U);
    assert(ge_original_gunbarrel_build_hole(hole,
        GE_ORIGINAL_GUNBARREL_HOLE_VERTEX_COUNT - 1U) == 0U);
    assert(ge_original_gunbarrel_build_hole(hole,
        GE_ORIGINAL_GUNBARREL_HOLE_VERTEX_COUNT)
        == GE_ORIGINAL_GUNBARREL_HOLE_VERTEX_COUNT);
    assert(hole[0].x == 0 && hole[0].y == -64);
    assert(hole[0].z == 0 && hole[0].s == 0 && hole[0].t == 0);
    assert(hole[0].red == 254U && hole[0].green == 254U
        && hole[0].blue == 254U && hole[0].alpha == 0U);
    assert(hole[1].x == 13 && hole[2].x == -13);
    assert(hole[29].y == 64 && hole[29].red == 32U);

    ge_original_gunbarrel_reset(&state);
    assert(state.mode == 2U && state.title_x == -30.0f);
    assert(state.title_y == 482.0f && state.transition_x == -100.0f);
    assert(ge_original_gunbarrel_tick(&state, blood_tick, &blood, &frame)
        == GE_ORIGINAL_GUNBARREL_TICK_RUNNING);
    assert(frame.mode == 2U && frame.title_x == -30.0f);
    assert(frame.layers == (GE_ORIGINAL_GUNBARREL_LAYER_CLEAR_BLACK
        | GE_ORIGINAL_GUNBARREL_LAYER_MOVING_HOLE));
    assert(ge_original_gunbarrel_build_frame_holes(&frame, layer_holes,
        GE_ORIGINAL_GUNBARREL_MAX_LAYER_HOLE_VERTICES)
        == GE_ORIGINAL_GUNBARREL_MAX_LAYER_HOLE_VERTICES);
    assert(layer_holes[0].x == -30.0f
        && layer_holes[0].y == 418.0f);
    assert(layer_holes[0].red == 0xe6U
        && layer_holes[0].alpha == 0xffU);
    assert(layer_holes[GE_ORIGINAL_GUNBARREL_HOLE_VERTEX_COUNT].x
        == -100.0f);
    ++mode_frames[frame.mode];

    for (ticks = 1; ticks < 1000 && !state.complete; ++ticks) {
        GeOriginalGunbarrelTickResult result = ge_original_gunbarrel_tick(
            &state, blood_tick, &blood, &frame);
        assert(result == GE_ORIGINAL_GUNBARREL_TICK_RUNNING
            || result == GE_ORIGINAL_GUNBARREL_TICK_COMPLETE);
        assert(frame.mode >= 2U && frame.mode <= 8U);
        ++mode_frames[frame.mode];
        animation_starts += frame.animation_start;
        animation_speedups += frame.animation_speedup;
        shots += frame.fire_shot;
        if (frame.mode == 6U && !first_sway_seen) {
            float expected = (float)sins(0x38eU) * 64.0f / 32768.0f
                + state.transition_x;
            assert(frame.title_x == expected);
            first_sway_seen = 1;
        }
    }
    assert(state.complete && state.mode == 9U);
    assert(ticks == 745);
    assert(state.rendered_frames == 745U);
    assert(mode_frames[2] == 237U);
    assert(mode_frames[3] == 234U);
    assert(mode_frames[4] == 21U);
    assert(mode_frames[5] == 83U);
    assert(mode_frames[6] == 108U);
    assert(mode_frames[7] == 31U);
    assert(mode_frames[8] == 31U);
    assert(state.animation_tick == 720);
    assert(animation_starts == 1);
    assert(animation_speedups == 1);
    assert(shots == 1);
    assert(blood.reset_calls == 1);
    assert(blood.advance_calls == 42);
    assert(frame.sequence_complete && frame.mode_after == 9U);

    memset(&frame, 0, sizeof(frame));
    frame.layers = GE_ORIGINAL_GUNBARREL_LAYER_SNIPER_SIGHT
        | GE_ORIGINAL_GUNBARREL_LAYER_SIGHT_BACKDROP;
    frame.title_x = 1276.0f;
    frame.title_y = 482.0f;
    assert(ge_original_gunbarrel_sight_rect(&frame, &sight_rect));
    assert(sight_rect.destination_left == 438);
    assert(sight_rect.destination_right == 440);
    assert(sight_rect.source_left == 0 && sight_rect.source_right == 2);
    assert(sight_rect.destination_top == 16
        && sight_rect.destination_bottom == 315);
    assert(ge_original_gunbarrel_build_frame_holes(&frame, layer_holes,
        GE_ORIGINAL_GUNBARREL_MAX_LAYER_HOLE_VERTICES)
        == GE_ORIGINAL_GUNBARREL_HOLE_VERTEX_COUNT);
    assert(layer_holes[0].x == 1276.0f + 768.0f);
    assert(layer_holes[0].y == 442.0f - 64.0f * 2.57f);
    assert(layer_holes[0].red == hole[0].red);
    frame.title_x = -80.0f;
    assert(ge_original_gunbarrel_sight_rect(&frame, &sight_rect));
    assert(sight_rect.destination_left == 0);
    assert(sight_rect.source_left == 28);
    assert(sight_rect.source_right == 468);

    {
        GeOriginalGunbarrelState blocked;
        GeOriginalGunbarrelState before;
        ge_original_gunbarrel_reset(&blocked);
        blocked.mode = 4U;
        blocked.intro_counter = 20;
        before = blocked;
        assert(ge_original_gunbarrel_tick(&blocked, NULL, NULL, &frame)
            == GE_ORIGINAL_GUNBARREL_TICK_NEEDS_BLOOD_DECODER);
        assert(memcmp(&blocked, &before, sizeof(blocked)) == 0);
    }
    return 0;
}
