#!/usr/bin/env python3
"""Tests for the source-derived Dam mission-start map."""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "analyze_dam_mission_start.py"
ROOT = SCRIPT.parents[1]
SPEC = importlib.util.spec_from_file_location("analyze_dam_mission_start", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class DamMissionStartAnalysisTests(unittest.TestCase):
    def test_stan_id_encoding(self) -> None:
        self.assertEqual(MODULE.stan_pack_id("p6g1"), (6, 0x31))
        self.assertEqual(MODULE.stan_pack_id("q32767z7"), (0xFFFF, 0xCF))
        with self.assertRaisesRegex(ValueError, "invalid STAN"):
            MODULE.stan_pack_id("room135")

    @unittest.skipUnless((ROOT / "build/u/assets/obseg/bg/bg_dam_all_p.bin").is_file(),
                         "private Dam background is not present")
    def test_exact_opening_cluster_and_deterministic_json(self) -> None:
        args = (
            ROOT / "assets/obseg/setup/UsetupdamZ.c",
            ROOT / "assets/obseg/stan/Tbg_dam_all_p_stanZ.c",
            ROOT / "build/u/assets/obseg/bg/bg_dam_all_p.bin",
        )
        result = MODULE.analyze(*args)
        self.assertEqual(result["evidence"]["setup_pad_index"], 33)
        self.assertEqual(result["evidence"]["setup_pad_stan_name"], "p6g1")
        self.assertEqual(result["evidence"]["stan_tile_index"], 171)
        self.assertEqual(result["evidence"]["spawn_room_id"], 135)
        self.assertEqual(result["room_order_bfs"], [135, 133, 134, 132, 136, 124])
        self.assertEqual([portal["id"] for portal in result["internal_portals"]],
                         [0, 1, 2, 3, 4, 5, 6])
        self.assertEqual([(portal["id"], portal["rooms"])
                          for portal in result["boundary_portals"]], [(7, [124, 125])])
        self.assertEqual(result["cluster_bounds_raw"],
                         {"min": [3380.0, -405.0, 3467.0],
                          "max": [4888.0, 409.0, 4881.0]})
        self.assertEqual(sum(room["point_table"]["vertex_count"]
                             for room in result["rooms"]), 3906)
        first = (json.dumps(result, indent=2, sort_keys=True) + "\n").encode()
        second = (json.dumps(MODULE.analyze(*args), indent=2, sort_keys=True) + "\n").encode()
        self.assertEqual(first, second)
        with tempfile.TemporaryDirectory() as temporary_name:
            output = Path(temporary_name) / "analysis.json"
            output.write_bytes(first)
            self.assertEqual(json.loads(output.read_text())["room_order_bfs"],
                             [135, 133, 134, 132, 136, 124])


if __name__ == "__main__":
    unittest.main()
