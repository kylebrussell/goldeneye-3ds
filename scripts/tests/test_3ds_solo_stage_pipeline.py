#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import re
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/generate_3ds_stage_registry.py"
INVENTORY = ROOT / "docs/generated/solo_stage_asset_inventory.json"
BUNDLES = ROOT / "build/3ds-levels"
REGISTRY = ROOT / "port/include/ge_solo_stage_registry.inc"
SPEC = importlib.util.spec_from_file_location("ge_stage_registry_tested", SCRIPT)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError(f"cannot load {SCRIPT}")
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class SoloStagePipelineTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.rows = MODULE.checked_rows(INVENTORY, BUNDLES)

    def test_all_generated_descriptors_are_current(self) -> None:
        self.assertEqual(18, len(self.rows))
        self.assertEqual(REGISTRY.read_text(), MODULE.render(self.rows))
        self.assertEqual(
            list(range(2, 20)),
            [row["stage"]["solo_sequence_index"] for row in self.rows],
        )

    def test_surface_world_is_shared_but_setup_is_not(self) -> None:
        rows = {row["stage"]["runtime_key"]: row for row in self.rows}
        surface1 = rows["surface1"]
        surface2 = rows["surface2"]
        for asset in ("background", "collision", "room_bounds"):
            self.assertEqual(
                surface1["manifest"]["files"][asset]["sha256"],
                surface2["manifest"]["files"][asset]["sha256"],
            )
        self.assertNotEqual(
            surface1["manifest"]["files"]["setup"]["sha256"],
            surface2["manifest"]["files"]["setup"]["sha256"],
        )

    def test_authored_portal_edge_cases_are_preserved(self) -> None:
        rows = {row["stage"]["runtime_key"]: row for row in self.rows}
        self.assertEqual(0, rows["cradle"]["manifest"]["portal_count"])
        counts: list[int] = []
        for path in (ROOT / "assets/obseg/bg").rglob("bg_*_all_p.c"):
            counts.extend(int(value) for value in re.findall(
                r"struct portal_(\d+)_point", path.read_text()))
        self.assertEqual(7, max(counts))
        self.assertEqual(2, counts.count(7))

    def test_bundle_identity_check_rejects_corruption(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            bundle = Path(temporary)
            path = bundle / "asset.bin"
            path.write_bytes(b"authored")
            record = {
                "path": "asset.bin",
                "size": len(b"authored"),
                "sha256": MODULE.sha256(b"authored"),
                "fnv1a64": MODULE.fnv1a64(b"authored"),
            }
            MODULE.check_file(bundle, record)
            path.write_bytes(b"authoreD")
            with self.assertRaisesRegex(ValueError, "identity mismatch"):
                MODULE.check_file(bundle, record)


if __name__ == "__main__":
    unittest.main()
