#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/generate_stage_visual_tour.py"
INVENTORY = ROOT / "docs/generated/solo_stage_asset_inventory.json"
SPEC = importlib.util.spec_from_file_location("generate_stage_visual_tour", SCRIPT)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"cannot load {SCRIPT}")
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


@unittest.skipUnless(
    (ROOT / "build/3ds-levels/dam/collision/collision.gestan").is_file()
    and (ROOT / "build/3ds-levels/egyptian/collision/collision.gestan").is_file(),
    "generated solo-stage collision bundles unavailable",
)
class StageVisualTourTests(unittest.TestCase):
    def generate(self, stage: str, directions: list[str] | None = None):
        encoded, manifest = MODULE.generate(
            ROOT, stage, 1, directions or ["forward"], 384)
        lines = encoded.decode().splitlines()
        self.assertEqual("GEVIEW1", lines[0])
        self.assertEqual(manifest["view_count"] + 2, len(lines))
        return manifest

    def assert_authored_route(self, manifest: dict) -> None:
        views = manifest["views"]
        self.assertEqual(manifest["normal_spawn_pad"], views[0]["pad"])
        self.assertEqual("spawn", views[0]["route_segment"])
        self.assertIsNone(views[0]["waypoint"])
        evidence = {
            target["prop_index"]
            for view in views
            for target in view["target_props"]
        }
        self.assertEqual(manifest["positioned_prop_count"], len(evidence))
        inventory = MODULE.load_inventory_module(ROOT)
        catalog = MODULE.stage_catalog(ROOT, inventory)
        stage = catalog[manifest["stage_key"]]
        text = stage["setup_path"].read_text()
        pads = [{**pad, "index": index}
                for index, pad in enumerate(inventory.parse_pads(text))]
        waypoints = MODULE.parse_waypoints(text, pads, inventory.initializer)
        for previous, current in zip(views[1:], views[2:]):
            if previous["route_segment"] == current["route_segment"]:
                self.assertIn(current["waypoint"],
                              waypoints[previous["waypoint"]]["neighbors"])

    def test_outdoor_runway_uses_exact_spawn_and_prop_graph(self) -> None:
        manifest = self.generate("runway")
        self.assertEqual(35, manifest["level_id"])
        self.assertEqual(53, manifest["normal_spawn_pad"])
        self.assertEqual(1, manifest["views"][0]["room"])
        self.assertEqual(73, manifest["waypoint_count"])
        self.assert_authored_route(manifest)

    def test_dam_gates_use_canonical_bound_pad_namespace(self) -> None:
        manifest = self.generate("dam")
        gates = {
            target["prop_index"]: target
            for view in manifest["views"]
            for target in view["target_props"]
            if target["prop_index"] in (267, 268)
        }
        self.assertEqual({267, 268}, set(gates))
        self.assertEqual(("bound-pad", 6, [4061.0, -7.0, 2041.0]),
                         (gates[267]["pad_kind"], gates[267]["pad_index"],
                          gates[267]["position"]))
        self.assertEqual(("bound-pad", 9, [4061.0, -7.0, 1830.0]),
                         (gates[268]["pad_kind"], gates[268]["pad_index"],
                          gates[268]["position"]))

    def test_indoor_silo_retains_disconnected_authored_components(self) -> None:
        manifest = self.generate("silo")
        self.assertEqual(20, manifest["level_id"])
        self.assertEqual(209, manifest["normal_spawn_pad"])
        # The third authored waypoint component owns a bound-pad door. It was
        # previously hidden by resolving Door::pad through the ordinary-pad
        # namespace, so exhaustive object coverage correctly retains it now.
        self.assertEqual(3, manifest["route_component_count"])
        self.assert_authored_route(manifest)

    def test_zero_portal_cradle_still_has_authored_navigation_tour(self) -> None:
        manifest = self.generate("cradle")
        inventory = json.loads(INVENTORY.read_text())
        cradle = next(stage for stage in inventory["stages"]
                      if stage["runtime_key"] == "cradle")
        self.assertEqual(0, cradle["world"]["portal_count"])
        self.assertEqual(151, manifest["normal_spawn_pad"])
        self.assertEqual(35, manifest["views"][0]["room"])
        self.assert_authored_route(manifest)

    def test_surface_variants_share_world_not_setup_or_route(self) -> None:
        first = self.generate("surface1")
        second = self.generate("surface2")
        self.assertEqual(first["decomp_keys"]["background"],
                         second["decomp_keys"]["background"])
        self.assertEqual(first["decomp_keys"]["stan"],
                         second["decomp_keys"]["stan"])
        self.assertEqual(first["source_sha256"]["collision"],
                         second["source_sha256"]["collision"])
        self.assertNotEqual(first["decomp_keys"]["setup"],
                            second["decomp_keys"]["setup"])
        self.assertNotEqual(first["source_sha256"]["setup"],
                            second["source_sha256"]["setup"])
        self.assertNotEqual(first["normal_spawn_pad"], second["normal_spawn_pad"])
        self.assertNotEqual(first["target_waypoint_count"],
                            second["target_waypoint_count"])

    def test_every_campaign_stage_fits_one_direction_runtime_capacity(self) -> None:
        inventory = MODULE.load_inventory_module(ROOT)
        catalog = MODULE.stage_catalog(ROOT, inventory)
        counts = {}
        for stage in catalog:
            _encoded, manifest = MODULE.generate(ROOT, stage, 1, ["forward"], 384)
            counts[stage] = manifest["view_count"]
        self.assertEqual(21, len(counts))
        self.assertLessEqual(max(counts.values()), 384)
        self.assertGreaterEqual(min(counts.values()), 1)
        self.assertEqual(1, counts["cuba"])

    def test_capacity_rejects_oversized_direction_sweep(self) -> None:
        with self.assertRaisesRegex(ValueError, "exceeding runtime capacity"):
            MODULE.generate(ROOT, "surface1", 1,
                            ["forward", "left", "right"], 384)

    def test_runtime_filenames_follow_stage_key(self) -> None:
        manifest = self.generate("facility")
        self.assertEqual(
            "sdmc:/3ds/goldeneye-3ds/facility-visual-tour.geview",
            manifest["runtime"]["tour_path"])
        self.assertEqual("facility",
                         manifest["runtime"]["stage_selection_value"])


if __name__ == "__main__":
    unittest.main()
