#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
TOUR = ROOT / "scripts/generate_dam_visual_tour.py"
ROUTE = ROOT / "scripts/generate_dam_end_to_end_route.py"
spec = importlib.util.spec_from_file_location("dam_e2e_route", ROUTE)
assert spec is not None and spec.loader is not None
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class DamEndToEndRouteTests(unittest.TestCase):
    def test_cli_default_covers_full_route_and_rejects_over_runtime_cap(self) \
            -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            tour = directory / "objectives.geview"
            manifest = directory / "objectives.json"
            route = directory / "route.cfg"
            subprocess.run([
                sys.executable, str(TOUR), "--route", "objectives",
                "--directions", "forward", "--frames", "1",
                "--output", str(tour), "--manifest", str(manifest),
            ], cwd=ROOT, check=True, capture_output=True, text=True)
            subprocess.run([
                sys.executable, str(ROUTE), str(manifest), str(route),
            ], cwd=ROOT, check=True, capture_output=True, text=True)
            self.assertEqual(route.read_text().splitlines()[1],
                             "frames 55000")
            rejected = subprocess.run([
                sys.executable, str(ROUTE), str(manifest), str(route),
                "--frames", "60001",
            ], cwd=ROOT, capture_output=True, text=True)
        self.assertNotEqual(rejected.returncode, 0)
        self.assertIn("frames must be in 1..60000", rejected.stderr)

    def test_can_prefix_proven_controller_only_firefight(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            tour = directory / "objectives.geview"
            manifest_path = directory / "objectives.json"
            opening = directory / "opening.cfg"
            subprocess.run([
                sys.executable, str(TOUR), "--route", "objectives",
                "--directions", "forward", "--frames", "1",
                "--output", str(tour), "--manifest", str(manifest_path),
            ], cwd=ROOT, check=True, capture_output=True, text=True)
            manifest = json.loads(manifest_path.read_text())
            lines = module.build_route(manifest, 55000, 135.0)
            opening.write_text(
                "GE_INPUT_PROBE 6\nframes 100\ntargets 2\n"
                "target 1 2 3 0 0 0 0\n"
                "target 4 5 6 1 20 0 0\n")
            combined = module.prepend_controller_route(lines, opening, 11)
        self.assertEqual(combined[1], "frames 55000")
        self.assertEqual(combined[2], "targets 151")
        self.assertEqual(combined[3:5], [
            "target 1 2 3 0 0 0 0 -1",
            "target 4 5 6 1 20 0 0 -1",
        ])
        self.assertEqual(combined[5:], lines[14:])

    def test_authored_opening_firefight_targets_exact_guard_ids(self) -> None:
        opening_path = ROOT / "scripts/dam-authored-firefight.cfg"
        opening = opening_path.read_text().splitlines()
        self.assertEqual(opening[0], "GE_INPUT_PROBE 7")
        records = [line.split() for line in opening[3:]]
        aimed_fire = [(int(record[4]), int(record[5]), int(record[7]),
                       int(record[8]))
                      for record in records if int(record[8]) >= 0]
        self.assertEqual(aimed_fire, [
            (module.ACTION_FIRE, 0, module.DEFENSIVE_FIRE_PERIOD, 6),
            (module.ACTION_FIRE, 300, module.DEFENSIVE_FIRE_PERIOD, 6),
            (module.ACTION_FIRE, 0, module.DEFENSIVE_FIRE_PERIOD, 7),
            (module.ACTION_FIRE, 900, module.DEFENSIVE_FIRE_PERIOD, 7),
        ])
        combined = module.prepend_controller_route([
            "GE_INPUT_PROBE 7", "frames 55000", "targets 1",
            "target 0 0 1 0 0 0 0 -1",
        ], opening_path)
        merged_opening = [line.split() for line in combined[3:14]]
        self.assertEqual([int(record[8]) for record in merged_opening],
                         [-1, -1, 6, 6, -1, -1, -1, -1, -1, 7, 7])

    def test_opening_chr7_engages_from_authored_pad328_cover(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            tour = directory / "objectives.geview"
            manifest_path = directory / "objectives.json"
            subprocess.run([
                sys.executable, str(TOUR), "--route", "objectives",
                "--directions", "forward", "--frames", "1",
                "--output", str(tour), "--manifest", str(manifest_path),
            ], cwd=ROOT, check=True, capture_output=True, text=True)
            manifest = json.loads(manifest_path.read_text())
        pad328 = next(view for view in manifest["views"]
                      if view["pad"] == 328)
        expected_x = pad328["position_runtime"][0]
        expected_z = pad328["position_runtime"][2]
        records = [line.split() for line in
                   (ROOT / "scripts/dam-authored-firefight.cfg")
                   .read_text().splitlines()[3:]]
        chr7 = [record for record in records if int(record[8]) == 7]
        self.assertEqual(len(chr7), 2)
        for record in chr7:
            self.assertAlmostEqual(float(record[1]), expected_x, places=5)
            self.assertAlmostEqual(float(record[2]), expected_z, places=5)

    def test_authored_full_armour_uses_natural_waypoint_detour(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            tour = directory / "objectives.geview"
            manifest_path = directory / "objectives.json"
            subprocess.run([
                sys.executable, str(TOUR), "--route", "objectives",
                "--directions", "forward", "--frames", "1",
                "--output", str(tour), "--manifest", str(manifest_path),
            ], cwd=ROOT, check=True, capture_output=True, text=True)
            manifest = json.loads(manifest_path.read_text())
            lines = module.build_route(manifest, 55000, 135.0)
            opening_path = ROOT / "scripts/dam-authored-firefight.cfg"
            combined = module.prepend_controller_route(
                lines, opening_path, 11)
        armour = manifest["mission_landmarks"]["armour"][0]
        self.assertEqual((armour["prop"], armour["pad"]), (318, 113))
        self.assertEqual(armour["route_detour"], {
            "branch_waypoint": 45,
            "branch_pad": 284,
            "approach": [{
                "waypoint": 202,
                "pad": 117,
                "position_raw": [3841.0, -13.0, 2933.0],
            }],
        })
        x = armour["position_raw"][0] / manifest["level_scale"]
        z = armour["position_raw"][2] / manifest["level_scale"]
        self.assertEqual(combined[2], "targets 160")
        armour_target = module.target(
            x, z, module.OPENING_ARMOUR_PICKUP_RADIUS)
        armour_index = combined.index(armour_target)
        self.assertEqual(combined[3:14],
                         opening_path.read_text().splitlines()[3:])
        self.assertEqual(combined[armour_index - 1].split()[1:3],
                         ["15772.128735", "12878.788430"])
        self.assertEqual(combined[armour_index + 1].split()[1:3],
                         ["15772.128735", "12878.788430"])

    def test_route_uses_all_authored_objective_segments_and_actions(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            tour = directory / "objectives.geview"
            manifest = directory / "objectives.json"
            subprocess.run([
                sys.executable, str(TOUR), "--route", "objectives",
                "--directions", "forward", "--frames", "1",
                "--output", str(tour), "--manifest", str(manifest),
            ], cwd=ROOT, check=True, capture_output=True, text=True)
            route_manifest = json.loads(manifest.read_text())
            lines = module.build_route(route_manifest, 55000, 135.0)
        self.assertEqual(lines[:3], ["GE_INPUT_PROBE 7", "frames 55000",
                                     "targets 160"])
        records = lines[3:]
        self.assertEqual(len(records), 160)
        self.assertTrue(any(" 32 1 0.000000 0" in line for line in records))
        self.assertTrue(any(" 1 150 -0.650000 0" in line
                            for line in records))
        self.assertEqual(sum(
            f" 1 180 0.000000 {module.DEFENSIVE_FIRE_PERIOD}" in line
                             for line in records), 4)
        self.assertTrue(any(" 2 720 0.000000 0" not in line
                            and " 0 720 0.000000 0" in line
                            for line in records))
        defensive_route_records = [
            line for line in records
            if int(line.split()[7]) == module.DEFENSIVE_FIRE_PERIOD
        ]
        self.assertEqual(len(defensive_route_records), 61)
        self.assertTrue(all(int(line.split()[4]) == module.ACTION_FIRE
                            for line in defensive_route_records))
        engagement_lines = [
            record for record in records
            if int(record.split()[4]) == module.ACTION_FIRE
            and int(record.split()[5]) == module.GUARD_ENGAGEMENT_DWELL
            and float(record.split()[3]) == module.GUARD_ENGAGEMENT_RADIUS
        ]
        self.assertEqual(len(engagement_lines), 38)
        for engagement_line in engagement_lines:
            engagement = engagement_line.split()
            self.assertEqual(int(engagement[4]), module.ACTION_FIRE)
            self.assertEqual(int(engagement[5]),
                             module.GUARD_ENGAGEMENT_DWELL)
            self.assertEqual(float(engagement[3]),
                             module.GUARD_ENGAGEMENT_RADIUS)
        aimed_engagements = [
            record.split() for record in engagement_lines
            if int(record.split()[8]) >= 0
        ]
        self.assertTrue(any(int(record[8]) == 39
                            for record in aimed_engagements))
        self.assertTrue(any(int(record[8]) == 4
                            for record in aimed_engagements))
        self.assertTrue(any(int(record[8]) == 5
                            for record in aimed_engagements))
        road_aim_order = [int(record[8]) for record in aimed_engagements
                          if int(record[8]) in module.ROAD_GUARD_IDS]
        self.assertEqual(road_aim_order, [5, 4])
        road_aim_positions = [(record[1], record[2])
                              for record in aimed_engagements
                              if int(record[8]) in module.ROAD_GUARD_IDS]
        pad287 = next(view for view in route_manifest["views"]
                      if view["pad"] == 287)
        authored_cover = (
            f'{pad287["position_runtime"][0]:.6f}',
            f'{pad287["position_runtime"][2]:.6f}',
        )
        self.assertEqual(road_aim_positions,
                         [authored_cover, authored_cover])
        self.assertEqual(module.ROAD_GUARD_ENGAGEMENT_FRACTIONS,
                         (0.0, 0.0))
        road_pulse_periods = [int(record[7]) for record in aimed_engagements
                              if int(record[8]) in module.ROAD_GUARD_IDS]
        self.assertEqual(road_pulse_periods,
                         [module.DEFENSIVE_FIRE_PERIOD] * 2)
        self.assertTrue(all(int(record[7]) == module.DEFENSIVE_FIRE_PERIOD
                            for record in aimed_engagements))
        last_held = 0
        press_frames = []
        release_frames = []
        for frame in range(module.DEFENSIVE_FIRE_PERIOD * 3):
            held = (module.ACTION_FIRE
                    if frame % module.DEFENSIVE_FIRE_PERIOD == 0 else 0)
            if held & ~last_held:
                press_frames.append(frame)
            if last_held & ~held:
                release_frames.append(frame)
            last_held = held
        self.assertEqual(press_frames, [0, 45, 90])
        self.assertEqual(release_frames, [1, 46, 91])
        bungee = route_manifest["mission_landmarks"]["bungee"]
        level_scale = route_manifest["level_scale"]
        bungee_x = bungee["position_raw"][0] / level_scale
        bungee_z = bungee["position_raw"][2] / level_scale
        self.assertEqual(records[-2], module.target(
            bungee_x, bungee_z, 135.0, 0,
            module.EXIT_SETTLE_FRAMES))
        self.assertEqual(records[-1], module.target(
            bungee_x, bungee_z, 135.0, module.ACTION_FIRE, 1))

    def test_interactions_match_authored_tags_and_ai_semantics(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            tour = directory / "objectives.geview"
            manifest_path = directory / "objectives.json"
            subprocess.run([
                sys.executable, str(TOUR), "--route", "objectives",
                "--directions", "forward", "--frames", "1",
                "--output", str(tour), "--manifest", str(manifest_path),
            ], cwd=ROOT, check=True, capture_output=True, text=True)
            manifest = json.loads(manifest_path.read_text())
            lines = module.build_route(manifest, 55000, 135.0)

        landmarks = manifest["mission_landmarks"]
        self.assertEqual([entry["tag"] for entry in
                          landmarks["backup_terminals"]], [6, 7])
        self.assertEqual([entry["prop"] for entry in
                          landmarks["backup_terminals"]], [262, 264])
        self.assertEqual([entry["tag"] for entry in landmarks["alarms"]],
                         [0, 1, 2, 3])
        self.assertEqual([entry["prop"] for entry in landmarks["alarms"]],
                         [310, 312, 314, 316])

        records = [line.split() for line in lines[3:]]
        terminal_positions = {
            (f"{entry['position_raw'][0] / manifest['level_scale']:.6f}",
             f"{entry['position_raw'][2] / manifest['level_scale']:.6f}")
            for entry in landmarks["backup_terminals"]
        }
        terminal_use_dwells = {
            (record[1], record[2]) for record in records
            if int(record[4]) == module.ACTION_USE and int(record[5]) == 1
        }
        self.assertTrue(terminal_positions.issubset(terminal_use_dwells))
        gate_positions = {
            (f"{entry['position_raw'][0] / manifest['level_scale']:.6f}",
             f"{entry['position_raw'][2] / manifest['level_scale']:.6f}")
            for entry in landmarks["gates"]
        }
        self.assertEqual(terminal_use_dwells - terminal_positions,
                         gate_positions)

        alarm_positions = {
            (f"{entry['position_raw'][0] / manifest['level_scale']:.6f}",
             f"{entry['position_raw'][2] / manifest['level_scale']:.6f}")
            for entry in landmarks["alarms"]
        }
        alarm_fire_dwells = {
            (record[1], record[2]) for record in records
            if int(record[4]) == module.ACTION_FIRE
            and int(record[5]) == 180
            and int(record[7]) == module.DEFENSIVE_FIRE_PERIOD
        }
        self.assertEqual(alarm_fire_dwells, alarm_positions)

        # Every mission interaction is expressed as a normal fresh controller
        # edge.  In particular, releasing Z after the single-use modem gives
        # the unchanged gun tick the trigger_released frame on which it
        # auto-advances away from depleted AMMO_BUG before the alarm fights.
        modem = landmarks["modem"][0]
        modem_position = (
            f"{modem['position_raw'][0] / manifest['level_scale']:.6f}",
            f"{modem['position_raw'][2] / manifest['level_scale']:.6f}",
        )
        modem_change = next(index for index, record in enumerate(records)
                            if (record[1], record[2]) == modem_position
                            and int(record[4]) == module.ACTION_NEXT_WEAPON)
        self.assertEqual(
            [(int(record[4]), int(record[5]), int(record[7]))
             for record in records[modem_change - 1:modem_change + 4]],
            [(0, 0, 0),
             (module.ACTION_NEXT_WEAPON, 1, 0),
             (0, 90, 0),
             (module.ACTION_FIRE, 150, 0),
             (0, 240, 0)])

        for index, record in enumerate(records):
            if int(record[4]) != module.ACTION_USE:
                continue
            self.assertGreater(index, 0)
            self.assertLess(index + 1, len(records))
            self.assertEqual(int(records[index - 1][4]), 0)
            self.assertEqual(int(record[5]), 1)
            self.assertEqual(int(records[index + 1][4]), 0)

        alarm_press_frames = [
            frame for frame in range(180)
            if frame % module.DEFENSIVE_FIRE_PERIOD == 0
        ]
        self.assertEqual(alarm_press_frames, [0, 45, 90, 135])

        setup = (ROOT / "assets/obseg/setup/UsetupdamZ.c").read_text()
        for tag in range(4):
            self.assertIn(
                f"/* Type = ObjectiveDestroyObject; index = {6 + tag} */\n"
                f"    _mkword(0, _mkshort(0, 25)), {tag},",
                setup,
            )
        for tag in (6, 7):
            self.assertIn(f"if_object_was_activated(0x0{tag}, 0x07)", setup)

    def test_rejects_manifest_with_remapped_authored_object_identity(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            tour = directory / "objectives.geview"
            manifest_path = directory / "objectives.json"
            subprocess.run([
                sys.executable, str(TOUR), "--route", "objectives",
                "--directions", "forward", "--frames", "1",
                "--output", str(tour), "--manifest", str(manifest_path),
            ], cwd=ROOT, check=True, capture_output=True, text=True)
            manifest = json.loads(manifest_path.read_text())
        manifest["mission_landmarks"]["alarms"][0]["tag"] = 3
        with self.assertRaisesRegex(ValueError, "alarm tags 0-3"):
            module.build_route(manifest, 55000, 135.0)


if __name__ == "__main__":
    unittest.main()
