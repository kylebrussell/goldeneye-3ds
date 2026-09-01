#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/verify_dam_firefight_result.py"


COMPLETE = """GE_INPUT_PROBE_RESULT 1
status=complete
frames=5000
simulation_frames=4700
route_targets=11,11
target_trace=0@398,1@892,2@895,3@1249,4@1700,5@2100,6@2400,7@2800,8@3600,9@3604,10@3904
displacement=7800.0
movement_blocker=0,0,00000000,0,0
pp7=11,11,0,1,1
guard_hit_test=11,6,1,1,24,3,852.545
guard=24,6,255,0,18892.0,0.0,17659.0,0,0
guard=25,7,132,13,16221.0,1.0,20309.0,1,1
guard_ai=25,7,2,3,48,13,00000000,0000,0.0,0,0,4.0,4.0
guard_combat=10,4,2,1.0,0.0
guard_ai_unknown=0,0,0,0,0,00000000
player_combat=0.5,0.0,1.0,1.0,-1,0,0
"""

EXHAUSTED = """GE_INPUT_PROBE_RESULT 1
status=failed
frames=3868
simulation_frames=1800
route_targets=4,9
target_trace=0@263,1@599,2@1012,3@1515
displacement=2463.252441
movement_blocker=0,0,00000000,0,0
"""


class DamFirefightVerifierTests(unittest.TestCase):
    def verify(self, contents: str) -> subprocess.CompletedProcess[str]:
        with tempfile.TemporaryDirectory() as temporary_name:
            result = Path(temporary_name) / "result.txt"
            result.write_text(contents)
            return subprocess.run(
                [sys.executable, str(SCRIPT), str(result)],
                capture_output=True, text=True,
            )

    def test_accepts_complete_canonical_combat_capture(self) -> None:
        completed = self.verify(COMPLETE)
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertIn("11 PP7 shots", completed.stdout)
        self.assertIn("1 original opening-guard death", completed.stdout)

    def test_accepts_either_authored_opening_guard_death(self) -> None:
        chr7_death = COMPLETE.replace(
            "guard=24,6,255,0,18892.0,0.0,17659.0,0,0",
            "guard=24,6,135,13,18892.0,1.0,17659.0,1,1",
        ).replace(
            "guard=25,7,132,13,16221.0,1.0,20309.0,1,1",
            "guard=25,7,255,0,16221.0,0.0,20309.0,0,0",
        )
        completed = self.verify(chr7_death)
        self.assertEqual(completed.returncode, 0, completed.stderr)

    def test_rejects_damage_without_an_opening_guard_death(self) -> None:
        no_death = COMPLETE.replace(
            "guard=24,6,255,0,18892.0,0.0,17659.0,0,0",
            "guard=24,6,135,13,18892.0,1.0,17659.0,1,1",
        )
        completed = self.verify(no_death)
        self.assertNotEqual(completed.returncode, 0)

    def test_rejects_unlinked_guard_without_death_lifecycle(self) -> None:
        inactive_only = COMPLETE.replace(
            "guard=24,6,255,0,18892.0,0.0,17659.0,0,0",
            "guard=24,6,135,3,18892.0,1.0,17659.0,0,0",
        )
        completed = self.verify(inactive_only)
        self.assertNotEqual(completed.returncode, 0)

    def test_budget_exhaustion_reports_route_evidence(self) -> None:
        completed = self.verify(EXHAUSTED)
        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("4/9 targets after 1800 simulation ticks", completed.stderr)
        self.assertIn("displacement=2463.252", completed.stderr)
        self.assertIn("exhausted the harness budget", completed.stderr)

    def test_authored_route_has_canonical_defensive_fire_and_budget(self) -> None:
        lines = (ROOT / "scripts/dam-authored-firefight.cfg").read_text().splitlines()
        self.assertEqual(lines[:3], ["GE_INPUT_PROBE 7", "frames 4500",
                                    "targets 11"])
        targets = [line.split() for line in lines[3:]]
        self.assertEqual(len(targets), 11)
        self.assertTrue(all(target[7] == "45"
                            for target in targets if int(target[8]) >= 0))
        self.assertTrue(all(target[7] == "0"
                            for target in targets if int(target[8]) < 0))
        self.assertEqual(targets[1][5:7], ["4", "1.0"])
        self.assertEqual(targets[2][4], "1")
        self.assertEqual(targets[3][4:6], ["1", "300"])
        self.assertTrue(all(target[4] == "0" for target in targets[4:9]))
        self.assertEqual(targets[8][5:7], ["4", "1.0"])
        self.assertEqual(targets[9][4], "1")
        self.assertEqual(targets[10][4:6], ["1", "900"])
        self.assertEqual(targets[-1][5], "900")


if __name__ == "__main__":
    unittest.main()
