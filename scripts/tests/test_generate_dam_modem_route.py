#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


def load_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


route = load_module("generate_dam_modem_route",
                    ROOT / "scripts/generate_dam_modem_route.py")
verifier = load_module("verify_dam_modem_result",
                       ROOT / "scripts/verify_dam_modem_result.py")


class DamModemRouteTests(unittest.TestCase):
    def manifest(self) -> dict[str, object]:
        return {
            "level_scale": 0.25,
            "views": [{"position_runtime": [4.0, 2.0, 8.0]}],
            "mission_landmarks": {"modem": [
                {"prop": 290, "bound_pad": 57,
                 "position_raw": [10.0, 1.0, 20.0]},
                {"prop": 292, "bound_pad": 58,
                 "position_raw": [11.0, 1.0, 21.0]},
            ], "gates": [
                {"prop": 267, "bound_pad": 6,
                 "position_raw": [30.0, 1.0, 60.0]},
                {"prop": 268, "bound_pad": 9,
                 "position_raw": [30.0, 1.0, 40.0]},
            ]},
        }

    def test_route_uses_authored_monitor_and_normal_action_phases(self) -> None:
        lines = route.build_route(self.manifest(), 6000, 135.0)
        self.assertEqual(lines[:3], ["GE_INPUT_PROBE 6", "frames 6000",
                                     "targets 6"])
        self.assertEqual(lines[3],
                         "target 4.000000 8.000000 135.000000 0 0 0.0 0")
        self.assertEqual(lines[4],
                         "target 40.000000 80.000000 135.000000 0 0 0.0 0")
        self.assertEqual(lines[5],
                         "target 40.000000 80.000000 135.000000 32 1 0.0 0")
        self.assertEqual(lines[7],
                         "target 40.000000 80.000000 135.000000 1 120 0.0 0")

    def test_verifier_requires_throw_and_exact_ai_objective_bit(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            result = Path(temporary) / "result.txt"
            result.write_text("\n".join([
                "GE_INPUT_PROBE_RESULT 1", "status=complete",
                "route_targets=5,5", "modem=1,1,0",
                "mission_objectives=0x00000100", "",
            ]))
            self.assertIn("objective registers 0x00000100",
                          verifier.verify(result))
            result.write_text(result.read_text().replace(
                "mission_objectives=0x00000100",
                "mission_objectives=0x00000000"))
            with self.assertRaisesRegex(ValueError, "did not observe"):
                verifier.verify(result)


if __name__ == "__main__":
    unittest.main()
