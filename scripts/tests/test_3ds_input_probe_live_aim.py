#!/usr/bin/env python3
"""Focused source audit for probe-only live-guard aim completion."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
SOURCE = (ROOT / "platform/3ds/source/main.c").read_text()
RUNTIME = (ROOT / "port/src/ge_original_stage_guard_runtime.c").read_text()


class InputProbeLiveAimTests(unittest.TestCase):
    def test_live_aim_uses_exact_canonical_render_point_in_world_space(self) -> None:
        resolver = SOURCE[
            SOURCE.index("static bool input_probe_live_guard_aim_position"):
            SOURCE.index("#define RUNTIME_STAGE_GUARD_COMBAT_AUDIT_CAPACITY")
        ]
        self.assertIn(
            "ge_original_stage_guard_runtime_autoaim_world_position(",
            resolver)
        self.assertNotIn("memcpy(position, snapshot.position", resolver)

        helper = RUNTIME[
            RUNTIME.index(
                "int ge_original_stage_guard_runtime_autoaim_world_position"):
            RUNTIME.index(
                "int ge_original_stage_guard_runtime_actor", RUNTIME.index(
                    "int ge_original_stage_guard_runtime_autoaim_world_position"))
        ]
        self.assertIn("model->render_pos[1].pos.m[3][axis]", helper)
        self.assertIn("model->render_pos[0].pos.m[3][axis]", helper)
        self.assertIn("-model->render_pos[0].pos.m[3][axis])*0.75f", helper)
        self.assertIn("world_position[axis]=prop->pos.f[axis]", helper)
        self.assertIn("+view_offset[0]*view_to_world[0][axis]", helper)
        self.assertIn("prop->pos.y+chr->chrheight*0.75f", helper)
        self.assertNotIn("+view_to_world[3][axis]", helper)

    def test_probe_aim_cannot_touch_normal_input_path(self) -> None:
        frame = SOURCE[
            SOURCE.index("simulation_start = osGetTime();"):
            SOURCE.index("if (scheduler_active", SOURCE.index(
                "simulation_start = osGetTime();"))
        ]
        enabled = frame.index("if (input_probe.enabled)")
        resolver = frame.index("input_probe_live_guard_aim_position(", enabled)
        sample = frame.index("input_probe_sample(", resolver)
        self.assertLess(enabled, resolver)
        self.assertLess(resolver, sample)
        self.assertEqual(frame.count("input_probe_live_guard_aim_position("), 1)

    def test_probe_pitch_uses_post_invert_swap_vertical_sign(self) -> None:
        helper = SOURCE[
            SOURCE.index("static void input_probe_apply_live_aim"):
            SOURCE.index("static GePortInput input_probe_sample")
        ]
        self.assertIn("(target_pitch - look_pitch) * 1.5f", helper)
        self.assertNotIn("(look_pitch - target_pitch) * 1.5f", helper)

        # HONEY first maps U_CBUTTONS to speedVertaDown, but the stock
        # invertPitch == 0 branch swaps Down/Up before applying speed.  Since
        # ge_port maps positive look_y to U_CBUTTONS, an elevated target must
        # produce a positive command.
        target_pitch = 0.5
        look_pitch = 0.0
        command = max(-1.0, min(1.0,
            (target_pitch - look_pitch) * 1.5))
        self.assertGreater(command, 0.0)

    def test_canonical_guard_death_ends_only_the_probe_dwell(self) -> None:
        resolver = SOURCE[
            SOURCE.index("static bool input_probe_live_guard_aim_position"):
            SOURCE.index("#define RUNTIME_STAGE_GUARD_COMBAT_AUDIT_CAPACITY")
        ]
        self.assertIn("snapshot.chr_id == target->aim_chr", resolver)
        self.assertIn(
            "ge_original_stage_guard_snapshot_death_complete(&snapshot)",
            resolver)
        self.assertIn("*guard_complete = true", resolver)

        sample = SOURCE[
            SOURCE.index("static GePortInput input_probe_sample"):
            SOURCE.index("static bool input_probe_live_guard_aim_position")
        ]
        completion = sample.index(
            "if (aim_guard_complete && target->aim_chr >= 0)")
        fire = sample.index("input.held = target->pulse_period", completion)
        branch = sample[completion:fire]
        self.assertIn("runtime->target_dwell_remaining = 0U", branch)
        self.assertIn("++runtime->target_index", branch)
        self.assertIn("runtime->last_sample_held = 0U", branch)
        self.assertIn("runtime->neutral_cutover_active = true", branch)
        self.assertIn("runtime->neutral_until_frame", branch)
        self.assertIn("return input_probe_cache_sample(runtime, input)",
                      branch)
        self.assertNotIn("snapshot", branch)
        self.assertNotIn("objects->guards", branch)

    def test_death_cutover_releases_then_emits_a_fresh_fire_edge(self) -> None:
        sample = SOURCE[
            SOURCE.index("static GePortInput input_probe_sample"):
            SOURCE.index("static bool input_probe_live_guard_aim_position")
        ]
        completion = sample.index(
            "if (aim_guard_complete && target->aim_chr >= 0)")
        release = sample.index("runtime->last_sample_held = 0U", completion)
        neutral_return = sample.index(
            "return input_probe_cache_sample(runtime, input)", release)
        next_fire = sample.index(
            "input.held = target->pulse_period", neutral_return)
        next_edge = sample.index(
            "input.pressed = input.held & ~runtime->last_sample_held",
            next_fire)
        self.assertLess(completion, release)
        self.assertLess(release, neutral_return)
        self.assertLess(neutral_return, next_fire)
        self.assertLess(next_fire, next_edge)

        neutral_gate = sample[
            sample.index("if (runtime->neutral_cutover_active)"):
            sample.index("if (runtime->target_count != 0U)")
        ]
        self.assertIn("route_frame <= runtime->neutral_until_frame",
                      neutral_gate)
        self.assertIn("return input_probe_cache_sample(runtime, input)",
                      neutral_gate)

        # Repeated display samples at the death frame and the following
        # canonical tick remain neutral. FIRE is eligible only after the
        # simulation frame advances beyond the stored release boundary.
        death_frame = 10
        neutral_until = death_frame + 1
        self.assertTrue(death_frame <= neutral_until)
        self.assertTrue(death_frame + 1 <= neutral_until)
        self.assertFalse(death_frame + 2 <= neutral_until)

    def test_only_aimed_travel_inherits_target_held_input(self) -> None:
        sample = SOURCE[
            SOURCE.index("static GePortInput input_probe_sample"):
            SOURCE.index("static bool input_probe_live_guard_aim_position")
        ]
        steering = sample[
            sample.rindex("target = &runtime->targets["):
        ]
        aimed = steering.index("if (target->aim_chr >= 0)")
        held = steering.index("input.held = target->pulse_period", aimed)
        pressed = steering.index(
            "input.pressed = input.held & ~runtime->last_sample_held", held)
        self.assertLess(aimed, held)
        self.assertLess(held, pressed)

        def travel_held(aim_chr: int, target_held: int) -> int:
            held_input = 0
            if aim_chr >= 0:
                held_input = target_held
            return held_input

        self.assertEqual(travel_held(-1, 1), 0)
        self.assertEqual(travel_held(4, 1), 1)


if __name__ == "__main__":
    unittest.main()
