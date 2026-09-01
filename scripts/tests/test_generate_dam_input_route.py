#!/usr/bin/env python3

from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/generate_dam_input_route.py"


class DamInputRouteTests(unittest.TestCase):
    def generate(self, views: list[dict], *arguments: str) -> list[str]:
        with tempfile.TemporaryDirectory() as temporary_name:
            directory = Path(temporary_name)
            manifest = directory / "route.json"
            output = directory / "route.cfg"
            manifest.write_text(json.dumps({"views": views}))
            subprocess.run(
                [sys.executable, str(SCRIPT), str(manifest), str(output),
                 *arguments], check=True, capture_output=True, text=True,
            )
            return output.read_text().splitlines()

    def test_full_authored_objective_route_fits_probe_capacity(self) -> None:
        views = [{"position_runtime": [float(index), 0.0, -float(index)]}
                 for index in range(102)]
        lines = self.generate(views, "--frames", "12000")
        self.assertEqual(lines[0], "GE_INPUT_PROBE 3")
        self.assertEqual(lines[1], "frames 12000")
        self.assertEqual(lines[2], "targets 102")
        self.assertEqual(len(lines), 105)

    def test_action_pulses_use_version_six_target_records(self) -> None:
        lines = self.generate(
            [{"position_runtime": [12.5, 0.0, -7.25]}],
            "--frames", "300", "--held", "2", "--final-dwell", "90",
            "--pulse-period", "15",
        )
        self.assertEqual(lines, [
            "GE_INPUT_PROBE 6",
            "frames 300",
            "targets 1",
            "target 12.500000 -7.250000 120.000000 2 90 0.0 15",
        ])

    def test_route_larger_than_runtime_capacity_is_rejected(self) -> None:
        views = [{"position_runtime": [0.0, 0.0, 0.0]}
                 for _ in range(161)]
        with self.assertRaises(subprocess.CalledProcessError):
            self.generate(views)


if __name__ == "__main__":
    unittest.main()
