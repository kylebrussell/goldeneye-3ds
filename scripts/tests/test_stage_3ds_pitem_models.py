#!/usr/bin/env python3
"""Focused tests for exact, table-driven PitemZ staging."""

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/stage_3ds_pitem_models.py"
SPEC = importlib.util.spec_from_file_location("stage_3ds_pitem_models", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def fixture(root: Path) -> tuple[Path, Path]:
    table = root / "assets/obseg/prop/propItemModelFileRecord.inc.c"
    table.parent.mkdir(parents=True)
    table.write_text(
        "ItemModelFileRecord PitemZ_entries[] = {\n"
        "#include <assets/obseg/prop/first/propFileRecord.inc.c>\n"
        "#include <assets/obseg/prop/second/propFileRecord.inc.c>\n"
        "{0, \"\", 1.0}\n};\n", encoding="utf-8")
    for name, scale in (("first", "0.1"), ("second", "1.0")):
        record = root / f"assets/obseg/prop/{name}/propFileRecord.inc.c"
        record.parent.mkdir(parents=True)
        record.write_text(f"PROPFILERECORD({name}, {scale})\n", encoding="utf-8")
    blobs = root / "build/u/assets/obseg/prop"
    blobs.mkdir(parents=True)
    (blobs / "PfirstZ.bin").write_bytes(b"exact-first")
    (blobs / "PsecondZ.bin").write_bytes(b"exact-second")
    return table, blobs


class PitemModelStagingTests(unittest.TestCase):
    def test_repository_table_has_canonical_order_and_scale(self) -> None:
        models = MODULE.parse_models(ROOT)
        self.assertEqual(340, len(models))
        self.assertEqual((0, "alarm1", 0.1),
                         (models[0]["model_id"], models[0]["name"], models[0]["scale"]))
        self.assertEqual((104, "window", 0.1),
                         (models[104]["model_id"], models[104]["name"],
                          models[104]["scale"]))
        self.assertEqual((178, "damgatedoor", 1.0),
                         (models[178]["model_id"], models[178]["name"],
                          models[178]["scale"]))
        self.assertEqual((335, "modembox", 0.1),
                         (models[335]["model_id"], models[335]["name"],
                          models[335]["scale"]))
        self.assertEqual("bollard", models[-1]["name"])
        self.assertEqual(275, sum(model["scale"] == 0.1 for model in models))
        self.assertEqual(65, sum(model["scale"] == 1.0 for model in models))

    def test_stage_is_exact_reproducible_checked_and_pruned(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            table, blobs = fixture(root)
            output = root / "stage"
            output.mkdir()
            (output / "stale.bin").write_bytes(b"stale")
            first = MODULE.stage(root, output, table, blobs)
            first_bytes = {path.name: path.read_bytes() for path in output.iterdir()}
            self.assertEqual(["first", "second"],
                             [model["name"] for model in first["models"]])
            self.assertEqual([0.1, 1.0],
                             [model["scale"] for model in first["models"]])
            self.assertEqual(b"exact-first", (output / "PfirstZ.bin").read_bytes())
            self.assertNotIn("stale.bin", first_bytes)
            MODULE.verify_stage(output, MODULE.checked_manifest(root, table, blobs))
            MODULE.stage(root, output, table, blobs)
            self.assertEqual(first_bytes,
                             {path.name: path.read_bytes() for path in output.iterdir()})
            parsed = json.loads((output / "manifest.json").read_text())
            self.assertEqual(2, parsed["model_count"])
            self.assertNotIn(temporary_name, (output / "manifest.json").read_text())

    def test_missing_additional_and_corrupt_payloads_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            table, blobs = fixture(root)
            output = root / "stage"
            MODULE.stage(root, output, table, blobs)
            (blobs / "PsecondZ.bin").unlink()
            with self.assertRaisesRegex(ValueError, "missing PsecondZ.bin"):
                MODULE.checked_manifest(root, table, blobs)
            (blobs / "PsecondZ.bin").write_bytes(b"exact-second")
            (blobs / "PthirdZ.bin").write_bytes(b"not in table")
            with self.assertRaisesRegex(ValueError, "additional PthirdZ.bin"):
                MODULE.checked_manifest(root, table, blobs)
            (blobs / "PthirdZ.bin").unlink()
            (output / "PfirstZ.bin").write_bytes(b"changed")
            with self.assertRaisesRegex(ValueError, "identity mismatch"):
                MODULE.verify_stage(output, MODULE.checked_manifest(root, table, blobs))

    def test_invalid_record_does_not_replace_previous_stage(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            table, blobs = fixture(root)
            output = root / "stage"
            MODULE.stage(root, output, table, blobs)
            before = {path.name: path.read_bytes() for path in output.iterdir()}
            record = root / "assets/obseg/prop/second/propFileRecord.inc.c"
            record.write_text("PROPFILERECORD(other_name, 1.0)\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "name/directory mismatch"):
                MODULE.stage(root, output, table, blobs)
            self.assertEqual(before,
                             {path.name: path.read_bytes() for path in output.iterdir()})


if __name__ == "__main__":
    unittest.main()
