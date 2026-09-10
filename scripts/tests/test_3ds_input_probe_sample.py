#!/usr/bin/env python3
"""Execute the actual route helper; no emulator or gameplay state is needed."""

import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.dont_write_bytecode = True
sys.path.insert(0, str(ROOT / "scripts"))
from extract_dam_guard_chr_scheduler_slice import function_text


def harness(source: str) -> str:
    start = source.index("enum {", source.index("static uint64_t runtime_profile_clock"))
    end = source.index("static bool input_probe_load_retraces", start)
    definitions = source[start:end]
    helpers = "\n".join(function_text(source, name) for name in (
        "input_probe_cache_sample", "input_probe_route_frame",
        "input_probe_apply_live_aim", "input_probe_sample"))
    return r'''
#include <assert.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "ge_port.h"
#include "ge_original_player_spawn.h"
#define M_PI_F 3.14159265358979323846f
#define M_TAU_F (2.0f * M_PI_F)
''' + definitions + helpers + r'''

static GeOriginalPlayerViewState player;
static RuntimeInputProbe probe;

static void reset(uint32_t held, int32_t aim_chr)
{
    memset(&probe, 0, sizeof(probe));
    memset(&player, 0, sizeof(player));
    player.initialized = 1;
    player.camera_look[2] = 1.0f;
    probe.enabled = true;
    probe.target_count = 1;
    probe.targets[0] = (RuntimeInputProbeTarget){
        .z = 100.0f, .radius = 1.0f, .held = held, .aim_chr = aim_chr};
}

static GePortInput sample(uint32_t frame, const float *aim, bool dead)
{
    probe.simulation_frames = frame;
    return input_probe_sample(&probe, &player, aim, dead);
}

static void neutral(GePortInput input)
{
    assert(input.held == 0 && input.pressed == 0);
    assert(input.move_x == 0 && input.move_y == 0);
    assert(input.look_x == 0 && input.look_y == 0);
}

static void movement_masks_and_edges(void)
{
    /* Every combination of the eight public controller actions passes
     * through exactly, whether or not an optional guard target exists. */
    for (int guard = -1; guard <= 7; guard += 8) {
        for (uint32_t mask = 0; mask < 256; ++mask) {
            reset(mask, guard);
            GePortInput first = sample(0, NULL, false);
            assert(first.held == mask && first.pressed == mask);
            assert(first.move_y == 1 && first.move_x == 0);
            assert(first.look_x == 0 && first.look_y == 0);
            GePortInput repeat = sample(0, NULL, false);
            assert(memcmp(&first, &repeat, sizeof(first)) == 0);
            assert(probe.trace_count == 1 && probe.target_index == 0);
            GePortInput held = sample(1, NULL, false);
            assert(held.held == mask && held.pressed == 0);
            probe.targets[0].held = 0;
            GePortInput released = sample(2, NULL, false);
            assert(released.held == 0 && released.pressed == 0);
            GePortInput fresh = sample(3, NULL, false);
            assert(fresh.held == 0 && fresh.pressed == 0);
            probe.targets[0].held = mask;
            fresh = sample(4, NULL, false);
            assert(fresh.held == mask && fresh.pressed == mask);
        }
    }
}

static void pulse_and_optional_aim(void)
{
    const float aim[3] = {30, 40, 100};
    for (int guard = -1; guard <= 7; guard += 8) {
        reset(GE_PORT_ACTION_FIRE | GE_PORT_ACTION_USE, guard);
        probe.targets[0].pulse_period = 45;
        for (uint32_t frame = 0; frame <= 91; ++frame) {
            GePortInput input = sample(frame, guard < 0 ? NULL : aim, false);
            uint32_t expected = frame % 45 == 0 ? probe.targets[0].held : 0;
            assert(input.held == expected && input.pressed == expected);
            assert(input.move_y == 1 && input.move_x == 0);
            if (guard >= 0) assert(input.look_x < 0 && input.look_y > 0);
            else assert(input.look_x == 0 && input.look_y == 0);
        }
    }
    /* A missing resolver point must not suppress aimed travel either. */
    reset(GE_PORT_ACTION_FIRE, 7);
    GePortInput input = sample(0, NULL, false);
    assert(input.held == GE_PORT_ACTION_FIRE && input.look_x == 0);
}

static void travel_dwell_and_target_edges(void)
{
    reset(GE_PORT_ACTION_FIRE, -1);
    probe.target_count = 3;
    probe.targets[0].dwell_frames = 2;
    probe.targets[0].dwell_look_y = -0.65f;
    probe.targets[1] = (RuntimeInputProbeTarget){.z=200, .radius=1,
        .held=GE_PORT_ACTION_FIRE|GE_PORT_ACTION_USE, .aim_chr=-1};
    probe.targets[2] = (RuntimeInputProbeTarget){.z=300, .radius=1,
        .held=GE_PORT_ACTION_FIRE, .aim_chr=-1};
    assert(sample(0, NULL, false).pressed == GE_PORT_ACTION_FIRE);
    player.collision_position[2] = 100;
    GePortInput input = sample(1, NULL, true);
    assert(input.held == GE_PORT_ACTION_FIRE && input.pressed == 0);
    assert(input.move_y == 0 && input.look_y == -0.65f);
    assert(probe.target_dwell_remaining == 1 && probe.target_index == 0);
    /* Guard completion cannot cancel an ordinary target's dwell. */
    (void)sample(1, NULL, true);
    assert(probe.target_dwell_remaining == 1);
    input = sample(2, NULL, true);
    assert(input.held == GE_PORT_ACTION_FIRE && input.pressed == 0);
    assert(probe.target_index == 1 && probe.target_reached_frames[0] == 2);
    input = sample(3, NULL, false);
    assert(input.held == (GE_PORT_ACTION_FIRE|GE_PORT_ACTION_USE));
    assert(input.pressed == GE_PORT_ACTION_USE && input.move_y == 1);
    player.collision_position[2] = 200;
    input = sample(4, NULL, false);
    assert(probe.target_index == 2 && input.held == GE_PORT_ACTION_FIRE);
    assert(input.pressed == 0); /* FIRE stayed held across both targets. */
    player.collision_position[2] = 300;
    neutral(sample(5, NULL, false));
    assert(probe.target_index == probe.target_count);

    reset(GE_PORT_ACTION_FIRE, -1);
    probe.targets[0].dwell_frames = 3;
    probe.targets[0].pulse_period = 2;
    assert(sample(0, NULL, false).pressed == GE_PORT_ACTION_FIRE);
    player.collision_position[2] = 100;
    assert(sample(1, NULL, false).held == 0);
    assert(sample(2, NULL, false).pressed == GE_PORT_ACTION_FIRE);
    assert(sample(3, NULL, false).held == 0);
    assert(probe.target_index == 1);
}

static void guard_death_cutover(void)
{
    reset(GE_PORT_ACTION_FIRE, 7);
    probe.target_count = 2;
    probe.targets[0].dwell_frames = 100;
    probe.targets[1] = (RuntimeInputProbeTarget){.z=200, .radius=1,
        .held=GE_PORT_ACTION_FIRE, .aim_chr=-1};
    player.collision_position[2] = 100;
    assert(sample(8, NULL, false).pressed == GE_PORT_ACTION_FIRE);
    neutral(sample(9, NULL, true));
    assert(probe.target_index == 1 && probe.target_dwell_remaining == 0);
    assert(probe.target_reached_frames[0] == 9 && probe.neutral_cutover_active);
    neutral(sample(9, NULL, false));
    neutral(sample(10, NULL, false));
    GePortInput input = sample(11, NULL, false);
    assert(input.held == GE_PORT_ACTION_FIRE && input.pressed == GE_PORT_ACTION_FIRE);
    assert(input.move_y == 1 && !probe.neutral_cutover_active);
}

static void inactive_and_segment_paths(void)
{
    reset(GE_PORT_ACTION_FIRE, -1);
    neutral(input_probe_sample(NULL, &player, NULL, false));
    probe.enabled = false;
    neutral(sample(0, NULL, false));
    assert(!probe.route_sampled && probe.target_index == 0);
    probe.enabled = true;
    player.initialized = 0;
    neutral(sample(1, NULL, false));
    player.initialized = 1;
    probe.simulation_frames = 2;
    neutral(input_probe_sample(&probe, NULL, NULL, false));
    assert(probe.target_index == 0 && probe.last_sample_held == 0);
    player.camera_look[2] = 0;
    neutral(sample(3, NULL, false));

    reset(0, -1);
    probe.target_count = 0;
    probe.active_frames = 2;
    probe.input.held = GE_PORT_ACTION_USE;
    assert(sample(0, NULL, false).pressed == GE_PORT_ACTION_USE);
    GePortInput input = sample(1, NULL, false);
    assert(input.held == GE_PORT_ACTION_USE && input.pressed == 0);
    neutral(sample(2, NULL, false));
    reset(0, -1);
    probe.target_count = 0;
    probe.segment_count = 2;
    probe.segments[0] = (RuntimeInputProbeSegment){
        .input={.move_y=1, .held=GE_PORT_ACTION_FIRE}, .frames=2};
    probe.segments[1] = (RuntimeInputProbeSegment){
        .input={.held=GE_PORT_ACTION_FIRE|GE_PORT_ACTION_AIM}, .frames=2};
    assert(sample(0, NULL, false).pressed == GE_PORT_ACTION_FIRE);
    assert(sample(1, NULL, false).pressed == 0);
    assert(sample(2, NULL, false).pressed == GE_PORT_ACTION_AIM);
    assert(sample(3, NULL, false).pressed == 0);
    neutral(sample(4, NULL, false));
}

int main(void)
{
    movement_masks_and_edges();
    pulse_and_optional_aim();
    travel_dwell_and_target_edges();
    guard_death_cutover();
    inactive_and_segment_paths();
    puts("Actual input probe helper: ordinary/aimed travel, masks, pulses, dwell, edges, cutover and inactive paths passed");
    return 0;
}
'''


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path,
                        default=ROOT / "platform/3ds/source/main.c",
                        help="alternative main.c for regression reproduction")
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="ge-input-probe-sample-") as directory:
        work = Path(directory)
        (work / "test.c").write_text(harness(args.source.read_text()))
        subprocess.run([
            "cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
            "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
            "-I", str(ROOT / "port/include"), str(work / "test.c"),
            "-lm", "-o", str(work / "test")], check=True)
        subprocess.run([str(work / "test")], cwd=work, check=True, env={
            **os.environ, "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1",
            "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"})


if __name__ == "__main__":
    main()
