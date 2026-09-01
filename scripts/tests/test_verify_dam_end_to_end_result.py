from __future__ import annotations

import tempfile
import unittest
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from scripts.verify_dam_end_to_end_result import verify


GOOD = """GE_INPUT_PROBE_RESULT 1
status=complete
route_targets=124,124
simulation_frames=7200
actor_ticks=6760
actor_status=1,0,0,1,1,0,0,0
rooms_visited=21
mission_objectives=0x00001500
objective_status=0:1:0:0,1:1:0:0,2:1:0:0,3:1:0:0
door_interaction=1000,4,4,4,4,2
gate=267,1,1,0.95,0.95,-1,319
gate=268,1,0,0.0,0.95,-1,320
gate_route=267,1,3,319,2,0.95,1,0.95,0.95,-1
gate_route=268,1,3,320,2,0.95,0,0.0,0.95,-1
gate_both_open_frames=0
modem=1,1,0
pp7=8,4,4,2,2
pp7_object=8,8,4,7,1
player_combat=0.75,0.0,0.75,0.0,-1,0,0
armour_probe=1.000015,312
guard_ai_unknown=0,0,-1,-1,0,0
mission_exit=2,1,10,1.0,-1.0,-1.0,500,1,1,0
mission_result=1,0,1
sound=12,12,0,2
frame_average_ms=33
"""

STAN_BLOCKED = """GE_INPUT_PROBE_RESULT 1
status=failed
route_targets=7,160
pp7=7,0,0,0,0
"""

HIT_LIST_BLOCKED = """GE_INPUT_PROBE_RESULT 1
status=failed
route_targets=7,160
pp7=7,7,0,0,0
guard_hit_test=0,0,0,0,-1,0,0
"""


class EndToEndVerifierTests(unittest.TestCase):
    def check(self, text: str) -> str:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "result"
            path.write_text(text)
            return verify(path)

    def test_accepts_complete_authored_flow(self) -> None:
        self.assertIn("Dam end-to-end verified", self.check(GOOD))

    def test_reports_shot_origin_stan_blocker_before_route_failure(self) -> None:
        with self.assertRaisesRegex(
                ValueError, "all 7 PP7 shots failed.*player-to-muzzle STAN"):
            self.check(STAN_BLOCKED)

    def test_reports_live_guard_hit_list_boundary_before_route_failure(
            self) -> None:
        with self.assertRaisesRegex(
                ValueError, "all 7 PP7 shots reached canonical STAN.*zero rays"):
            self.check(HIT_LIST_BLOCKED)

    def test_rejects_missing_second_gate(self) -> None:
        with self.assertRaisesRegex(ValueError, "gate 268"):
            self.check(GOOD.replace("gate_route=268,1", "gate_route=268,0"))

    def test_rejects_gate_without_per_command_activation(self) -> None:
        with self.assertRaisesRegex(ValueError, "gate 268.*not activated"):
            self.check(GOOD.replace(
                "gate_route=268,1,3,320,2",
                "gate_route=268,1,3,320,0"))

    def test_rejects_gate_without_generation_change(self) -> None:
        with self.assertRaisesRegex(ValueError, "gate 267.*generation change"):
            self.check(GOOD.replace(
                "gate_route=267,1,3,319",
                "gate_route=267,1,3,3"))

    def test_rejects_gate_that_never_fully_opened(self) -> None:
        with self.assertRaisesRegex(ValueError, "gate 268 never fully opened"):
            self.check(GOOD.replace(
                "gate_route=268,1,3,320,2,0.95",
                "gate_route=268,1,3,320,2,0.90"))

    def test_rejects_any_interlock_overlap_frame(self) -> None:
        with self.assertRaisesRegex(ValueError, "both gates open"):
            self.check(GOOD.replace(
                "gate_both_open_frames=0", "gate_both_open_frames=1"))

    def test_rejects_route_and_final_gate_divergence(self) -> None:
        with self.assertRaisesRegex(ValueError, "telemetry diverged"):
            self.check(GOOD.replace(
                "gate=268,1,0,0.0,0.95,-1,320",
                "gate=268,1,0,0.0,0.95,-1,319"))

    def test_rejects_incomplete_backup_objective(self) -> None:
        with self.assertRaisesRegex(ValueError, "not all four"):
            self.check(GOOD.replace("2:1:0:0", "2:0:0:0"))

    def test_rejects_active_prop_failure_after_watch_pause(self) -> None:
        with self.assertRaisesRegex(ValueError, "actor scheduler was not healthy"):
            self.check(GOOD.replace(
                "actor_status=1,0,0,1,1,0,0,0",
                "actor_status=8,6,0,1,1,0,0,0"))

    def test_rejects_missing_actor_ticks(self) -> None:
        with self.assertRaisesRegex(ValueError, "actor scheduler tick evidence"):
            self.check(GOOD.replace("actor_ticks=6760\n", ""))

    def test_accepts_canonical_paused_frames_without_actor_advancement(self) -> None:
        # Watch and exit-flow pauses intentionally make actor_ticks smaller
        # than simulation_frames; the scheduler must resume and remain live.
        self.assertIn("Dam end-to-end verified", self.check(
            GOOD.replace("actor_ticks=6760", "actor_ticks=124")))

    def test_rejects_implausibly_short_or_overrun_actor_history(self) -> None:
        with self.assertRaisesRegex(ValueError, "tick evidence is inconsistent"):
            self.check(GOOD.replace("actor_ticks=6760", "actor_ticks=123"))
        with self.assertRaisesRegex(ValueError, "tick evidence is inconsistent"):
            self.check(GOOD.replace("actor_ticks=6760", "actor_ticks=7201"))

    def test_rejects_exit_without_fresh_player_input(self) -> None:
        with self.assertRaisesRegex(ValueError, "title stage"):
            self.check(GOOD.replace(
                "mission_exit=2,1,10,1.0,-1.0,-1.0,500,1,1,0",
                "mission_exit=2,1,10,1.0,-1.0,-1.0,500,1,0,0"))

    def test_rejects_unpersisted_success(self) -> None:
        with self.assertRaisesRegex(ValueError, "persisted canonically"):
            self.check(GOOD.replace("mission_result=1,0,1",
                                    "mission_result=1,1,0"))

    def test_rejects_dead_or_invincible_route(self) -> None:
        with self.assertRaisesRegex(ValueError, "Bond died"):
            self.check(GOOD.replace(
                "player_combat=0.75,0.0,0.75,0.0,-1,0,0",
                "player_combat=0.0,0.0,1.0,1.0,-1,1,0"))
        with self.assertRaisesRegex(ValueError, "invincibility"):
            self.check(GOOD.replace(
                "player_combat=0.75,0.0,0.75,0.0,-1,0,0",
                "player_combat=1.0,0.0,1.0,0.0,-1,0,1"))

    def test_rejects_missing_authored_armour_pickup(self) -> None:
        with self.assertRaisesRegex(ValueError, "not collected canonically"):
            self.check(GOOD.replace("armour_probe=1.000015,312",
                                    "armour_probe=0.000000,0"))


if __name__ == "__main__":
    unittest.main()
