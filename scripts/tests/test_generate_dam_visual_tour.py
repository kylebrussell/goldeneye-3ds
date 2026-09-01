#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import json
import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/generate_dam_visual_tour.py"
SETUP = ROOT / "assets/obseg/setup/UsetupdamZ.c"
STAN = ROOT / "assets/obseg/stan/Tbg_dam_all_p_stanZ.c"
SPEC = importlib.util.spec_from_file_location("generate_dam_visual_tour", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


@unittest.skipUnless(SETUP.is_file() and STAN.is_file(),
                     "decompiled Dam setup/STAN sources unavailable")
class DamVisualTourTests(unittest.TestCase):
    def generate(self, *arguments: str) -> tuple[list[str], dict]:
        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            output = root / "tour.geview"
            manifest = root / "tour.json"
            subprocess.run(
                [sys.executable, str(SCRIPT), "--setup", str(SETUP),
                 "--stan", str(STAN), "--output", str(output),
                 "--manifest", str(manifest), *arguments],
                check=True, capture_output=True, text=True,
            )
            return output.read_text().splitlines(), json.loads(
                manifest.read_text())

    def test_default_follows_authored_main_player_route(self) -> None:
        lines, manifest = self.generate("--frames", "7")
        self.assertEqual(lines[0], "GEVIEW1")
        self.assertEqual(manifest["selection"], "main-player-route")
        self.assertEqual(manifest["directions"], ["forward", "left", "right"])
        self.assertEqual(manifest["view_count"],
                         len(MODULE.MAIN_PLAYER_ROUTE_PADS) * 3)
        self.assertEqual(manifest["total_frames"], manifest["view_count"] * 7)
        forward = [view for view in manifest["views"]
                   if view["direction"] == "forward"]
        self.assertEqual([view["pad"] for view in forward],
                         list(MODULE.MAIN_PLAYER_ROUTE_PADS))
        self.assertEqual(forward[0]["pad_name"], "PAD_mk11_dam_all_p")
        self.assertEqual(forward[12]["pad_name"], "PAD_mtun1_dam_all_p")
        self.assertEqual(forward[-1]["pad_name"], "PAD_mdam6_dam_all_p")
        self.assertEqual(forward[0]["room"], 135)
        self.assertAlmostEqual(forward[0]["floor_y_raw"], -25.0)
        for view in manifest["views"]:
            self.assertAlmostEqual(math.hypot(view["look"][0],
                                              view["look"][2]), 1.0)

    def test_mission_targets_come_from_exact_prop_and_ai_records(self) -> None:
        _, manifest = self.generate("--frames", "1")
        targets = manifest["mission_targets"]
        self.assertEqual(targets["modem_prop_indices"], [290, 292])
        self.assertEqual(targets["modem_bound_pads"], [57, 58])
        self.assertEqual(targets["alarm_tags"], [0, 1, 2, 3])
        self.assertEqual(targets["alarm_prop_indices"], [310, 312, 314, 316])
        self.assertEqual(targets["alarm_bound_pads"], [70, 71, 72, 74])
        self.assertEqual(targets["backup_tags"], [6, 7])
        self.assertEqual(targets["backup_prop_indices"], [262, 264])
        self.assertEqual(targets["backup_pads"], [100, 101])
        self.assertEqual(targets["bungee_ai_list"], 24)
        self.assertEqual(targets["bungee_pad"], 330)
        self.assertEqual(targets["armour_prop_indices"], [318, 319])
        self.assertEqual(targets["armour_pads"], [113, 114])
        self.assertEqual(targets["armour_initial_amounts"], [65536, 32768])
        landmarks = manifest["mission_landmarks"]
        self.assertEqual(len(landmarks["guards"]), 36)
        self.assertEqual(landmarks["guards"][0], {
            "prop": 23, "chr": 0, "pad": 1,
            "position_raw": [4194.0, -13.0, 2386.0],
            "stan": "p12295e",
        })
        self.assertEqual(landmarks["guards"][-1]["chr"], 13)
        self.assertEqual(landmarks["modem"][0]["position_raw"],
                         [3415.0, 14.0, 408.0])
        self.assertEqual([entry["bound_pad"] for entry in landmarks["alarms"]],
                         [70, 71, 72, 74])
        self.assertEqual([entry["tag"] for entry in landmarks["alarms"]],
                         [0, 1, 2, 3])
        self.assertEqual(landmarks["backup_terminals"], [
            {"tag": 6, "prop": 262, "pad": 100,
             "position_raw": [3283.0, -142.0, 361.0],
             "stan": "p13013d"},
            {"tag": 7, "prop": 264, "pad": 101,
             "position_raw": [3302.0, -142.0, 336.0],
             "stan": "p12983d2"},
        ])
        self.assertEqual(landmarks["bungee"], {
            "ai_list": 24,
            "pad": 330,
            "position_raw": [-1.0, 0.0, -709.0],
            "stan": "p11380c1",
        })
        self.assertEqual(landmarks["armour"][0], {
            "prop": 318, "model": 115, "pad": 113,
            "initial_amount_fixed": 65536,
            "position_raw": [3924.0, 90.0, 2925.0],
            "route_detour": {
                "branch_waypoint": 45,
                "branch_pad": 284,
                "approach": [{
                    "waypoint": 202,
                    "pad": 117,
                    "position_raw": [3841.0, -13.0, 2933.0],
                }],
            },
            "stan": "p12339e2",
        })

    def test_modem_route_uses_authored_waypoint_links(self) -> None:
        _, manifest = self.generate("--route", "modem", "--directions",
                                    "forward", "--frames", "1")
        views = manifest["views"]
        self.assertEqual(manifest["selection"], "authored-modem-route")
        self.assertEqual(manifest["route"], "modem")
        self.assertEqual(len(views), 19)
        self.assertEqual((views[0]["waypoint"], views[0]["pad"]), (8, 321))
        self.assertEqual((views[-1]["waypoint"], views[-1]["pad"]),
                         (156, 163))
        self.assertTrue(all(view["route_segment"] == "modem-approach"
                            for view in views))

    def test_alarm_route_covers_each_exact_object_area(self) -> None:
        _, manifest = self.generate("--route", "alarms", "--directions",
                                    "forward", "--frames", "1")
        segments: dict[str, list[dict]] = {}
        for view in manifest["views"]:
            segments.setdefault(view["route_segment"], []).append(view)
        self.assertEqual(list(segments),
                         ["alarm-1", "alarm-2", "alarm-3", "alarm-4"])
        self.assertEqual([(views[0]["pad"], views[-1]["pad"])
                          for views in segments.values()],
                         [(321, 161), (171, 187), (187, 251), (251, 215)])
        self.assertEqual(manifest["mission_targets"]["alarm_bound_pads"],
                         [70, 71, 72, 74])

    def test_bungee_route_ends_at_ai24_room_test_pad(self) -> None:
        _, manifest = self.generate("--route", "bungee", "--directions",
                                    "forward", "--frames", "1")
        views = manifest["views"]
        self.assertEqual(views[0]["pad"], 171)
        self.assertEqual(views[-2]["pad"], 277)
        self.assertEqual(views[-1]["pad"], 330)
        self.assertEqual(views[-1]["route_segment"], "bungee-exit")
        self.assertIsNone(views[-1]["waypoint"])
        self.assertEqual(manifest["mission_targets"]["bungee_pad"], 330)

    def test_objective_route_retains_mission_order(self) -> None:
        _, manifest = self.generate("--route", "objectives", "--frames", "1")
        segments = []
        for view in manifest["views"]:
            if not segments or segments[-1] != view["route_segment"]:
                segments.append(view["route_segment"])
        self.assertEqual(segments, [
            "modem-approach", "alarm-1", "alarm-2", "alarm-3",
            "alarm-4", "bungee-after-alarms", "bungee-exit",
        ])
        self.assertEqual(manifest["view_count"], 258)
        self.assertEqual(manifest["views"][-1]["pad"], 330)
        self.assertEqual(
            [(gate["prop"], gate["bound_pad"])
             for gate in manifest["mission_landmarks"]["gates"]],
            [(267, 6), (268, 9)])

    def test_room_coverage_remains_an_explicit_secondary_mode(self) -> None:
        _, manifest = self.generate("--room-coverage", "--frames", "1")
        self.assertEqual(manifest["selection"], "first-pad-per-room")
        self.assertEqual(manifest["directions"], ["authored"])
        rooms = [view["room"] for view in manifest["views"]]
        self.assertEqual(len(rooms), len(set(rooms)))
        self.assertEqual(manifest["views"][0]["pad"], MODULE.SPAWN_PAD)


if __name__ == "__main__":
    unittest.main()
