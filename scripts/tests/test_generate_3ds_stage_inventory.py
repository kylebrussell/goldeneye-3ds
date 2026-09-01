#!/usr/bin/env python3

from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import struct
import sys
import unittest


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/generate_3ds_stage_inventory.py"
CHECKED_MANIFEST = ROOT / "docs/generated/solo_stage_asset_inventory.json"
SPEC = importlib.util.spec_from_file_location("ge_stage_inventory_tested", SCRIPT)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"cannot load {SCRIPT}")
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class StageInventoryTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.generated = MODULE.generate(ROOT)

    def test_checked_manifest_is_current(self) -> None:
        self.assertEqual(self.generated, json.loads(CHECKED_MANIFEST.read_text()))

    def test_exact_solo_scope_and_shared_surface_world(self) -> None:
        stages = self.generated["stages"]
        self.assertEqual(18, len(stages))
        self.assertEqual("Runway", stages[0]["stage"])
        self.assertEqual("Egyptian", stages[-1]["stage"])
        self.assertEqual(list(range(2, 20)),
                         [stage["solo_sequence_index"] for stage in stages])
        by_name = {stage["stage"]: stage for stage in stages}
        self.assertEqual(35, by_name["Runway"]["level_id"]["value"])
        self.assertEqual(9, by_name["Bunker 1"]["level_id"]["value"])
        self.assertEqual(43, by_name["Surface 2"]["level_id"]["value"])
        self.assertEqual("sevx", by_name["Surface 1"]["decomp_keys"]["background"])
        self.assertEqual("sevx", by_name["Surface 2"]["decomp_keys"]["background"])
        self.assertEqual("UsetupsevxZ", by_name["Surface 1"]["decomp_keys"]["setup"])
        self.assertEqual("UsetupsevxbZ", by_name["Surface 2"]["decomp_keys"]["setup"])

    def test_normal_spawns_resolve_to_authored_stan_rooms(self) -> None:
        for stage in self.generated["stages"]:
            spawn = stage["setup"]["normal_intro_spawn"]
            with self.subTest(stage=stage["stage"]):
                self.assertEqual(0, spawn["demo_slot"])
                self.assertGreaterEqual(spawn["pad_index"], 0)
                self.assertGreater(spawn["stan_packed_id"], 0)
                self.assertGreaterEqual(spawn["stan_tile_index"], 0)
                self.assertGreater(spawn["room"], 0)
                self.assertEqual(3, len(spawn["position"]))
                self.assertEqual(3, len(spawn["look"]))
                self.assertEqual(3, len(spawn["up"]))

    def test_compiled_spawn_cross_check_rejects_mutation(self) -> None:
        stage = self.generated["stages"][0]
        setup_source = ROOT / stage["assets"]["setup"]["source"]["path"]
        setup_bin = ROOT / stage["assets"]["setup"]["compiled"]["path"]
        text = setup_source.read_text()
        pads = MODULE.parse_pads(text)
        spawns, _items = MODULE.parse_intro_source(text)
        damaged = bytearray(setup_bin.read_bytes())
        intro_offset = struct.unpack_from(">I", damaged, 8)[0]
        # Runway begins with a camera and cuff before the normal spawn. Locate
        # the compiled spawn rather than depending on that record ordering.
        offset = intro_offset
        while struct.unpack_from(">I", damaged, offset)[0] != 0:
            kind = struct.unpack_from(">I", damaged, offset)[0]
            offset += MODULE.INTRO_SIZES[kind]
        damaged[offset + 7] ^= 1
        with self.assertRaisesRegex(ValueError, "compiled setup intro spawns"):
            MODULE.validate_compiled_spawn(bytes(damaged), spawns, pads)

    def test_counts_cover_direct_dependencies(self) -> None:
        for stage in self.generated["stages"]:
            world = stage["world"]
            setup = stage["setup"]["dependencies"]
            collision = stage["collision"]
            with self.subTest(stage=stage["stage"]):
                self.assertGreater(world["room_count_including_dummy_room_0"], 1)
                self.assertEqual(world["room_count_including_dummy_room_0"] - 1,
                                 world["playable_room_slot_count"])
                self.assertGreater(world["unique_rare_texture_count"], 0)
                self.assertEqual(world["unique_rare_texture_count"],
                                 len(world["unique_rare_texture_ids"]))
                self.assertGreater(collision["stan_tile_count"], 0)
                self.assertGreater(collision["stan_point_count"], 0)
                self.assertGreater(setup["direct_prop_model_instance_count"], 0)
                self.assertGreater(len(setup["unique_prop_model_ids"]), 0)
                self.assertEqual(setup["unique_prop_model_count"],
                                 len(setup["unique_prop_model_ids"]))
                self.assertEqual(setup["unique_guard_body_count"],
                                 len(setup["unique_guard_body_ids"]))
                self.assertEqual(setup["unique_explicit_guard_head_count"],
                                 len(setup["unique_explicit_guard_head_ids"]))
                self.assertEqual(setup["normal_intro_item_count"],
                                 len(setup["normal_intro_item_ids"]))


if __name__ == "__main__":
    unittest.main()
