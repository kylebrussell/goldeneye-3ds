#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts/extract_3ds_facility_level.py"
COLLISION_SCRIPT = ROOT / "scripts/extract_3ds_stage_collision.py"
PACKER = ROOT / "scripts/pack_3ds_assets.py"
SOURCE_SHA1 = "abe01e4aeb033b6c0836819f549c791b26cfde83"


def load(path: Path, name: str):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module


FACILITY = load(SCRIPT, "test_facility_level_module")
COLLISION = load(COLLISION_SCRIPT, "test_stage_collision_module")


def tree_hashes(root: Path) -> dict[str, str]:
    return {
        path.relative_to(root).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
        for path in root.rglob("*") if path.is_file()
    }


def pack_names(path: Path) -> set[str]:
    data = path.read_bytes()
    magic, version, _flags, count, entry_size, index_offset, paths_offset, _data = \
        struct.unpack_from("<8sIIIIQQQ", data)
    if magic != b"GEPACK\0\0" or version != 1 or entry_size != 80:
        raise AssertionError("unexpected pack header")
    names = set()
    for index in range(count):
        _hash, path_offset, path_length, _offset, _size = struct.unpack_from(
            "<QIIQQ", data, index_offset + index * 32)
        names.add(data[paths_offset + path_offset:
                       paths_offset + path_offset + path_length].decode())
    return names


class FacilityLevelExtractionTests(unittest.TestCase):
    def test_canonical_setup_spawn_is_resolved_through_stan(self) -> None:
        parsed = COLLISION.parse_setup(
            (ROOT / "assets/obseg/setup/UsetuparkZ.c").read_bytes())
        self.assertEqual(len(parsed["pads"]), 311)
        self.assertEqual(parsed["spawns"], [
            {"pad": 167, "demo": 0}, {"pad": 109, "demo": 1},
            {"pad": 137, "demo": 2}, {"pad": 93, "demo": 3},
        ])
        spawn = parsed["pads"][167]
        self.assertEqual(spawn["position"], [137.0, 562.0, -1154.0])
        self.assertEqual(spawn["look"], [-1.0, 0.0, 0.0])
        self.assertEqual(spawn["stan_name"], "p1682a1")
        self.assertEqual(COLLISION.stan_pack_id(spawn["stan_name"]), 0x69201)

    @unittest.skipUnless(
        (ROOT / "build/u/assets/obseg/bg/bg_ark_all_p.bin").is_file(),
        "private US Facility background is unavailable",
    )
    def test_exact_private_bundle_and_reproducibility(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            output = Path(temporary_name) / "facility"
            manifest = FACILITY.extract(
                ROOT / "build/u/assets/obseg/bg/bg_ark_all_p.bin",
                ROOT / "assets/obseg/stan/Tbg_ark_all_p_stanZ.c",
                ROOT / "assets/obseg/setup/UsetuparkZ.c",
                ROOT / "build/u/assets/obseg/setup/UsetuparkZ.bin", output,
            )
            self.assertEqual(manifest["background"]["room_count"], 78)
            self.assertEqual(manifest["background"]["portal_count"], 109)
            self.assertEqual(manifest["background"]["visibility"], {
                "record_count": 101,
                "size": 808,
                "source_offset": 1940,
                "sha256": "3385bf9981210e8a4928a74a3ac2fb5e3131bd5b91883abd7ffbaf6a4796307e",
            })
            self.assertEqual(manifest["rooms"]["stream_count"], 186)
            self.assertEqual(manifest["rooms"]["manifest_sha256"],
                             "7dad0cbafb3e75d2608f8e55ccb54ebb078fee227a1df34bee7e802c468d785c")
            self.assertEqual(manifest["collision"]["tile_count"], 2599)
            self.assertEqual(manifest["collision"]["point_count"], 7908)
            self.assertEqual(manifest["collision"]["sha256"],
                             "3d7f761675d8853fc67704597fa558d3e679c5e484d8f8fc1b1c5a2557500400")
            self.assertEqual(manifest["room_bounds"]["sha256"],
                             "74282560cfc5b622b717f1e8929bf2389960eaa72953a98d17592f3f81d45730")
            self.assertEqual(manifest["setup"]["normal_spawn"]["room"], 13)
            self.assertEqual((output / "background.bin").read_bytes(),
                             (ROOT / "build/u/assets/obseg/bg/bg_ark_all_p.bin").read_bytes())
            self.assertEqual(json.loads((output / "rooms/manifest.json").read_text())
                             ["name"], "facility-rooms")
            empty_assets = Path(temporary_name) / "empty-assets"
            empty_assets.mkdir()
            pack = Path(temporary_name) / "facility.gepack"
            subprocess.run([
                sys.executable, str(PACKER), "--assets", str(empty_assets),
                "--source-sha1", SOURCE_SHA1, "--extra-dir",
                f"converted/levels/facility={output}", "--output", str(pack),
            ], check=True, capture_output=True, text=True)
            names = pack_names(pack)
            self.assertIn("converted/levels/facility/background.bin", names)
            self.assertIn("converted/levels/facility/rooms/room013/point_table.bin",
                          names)
            self.assertIn("converted/levels/facility/collision/collision.gestan",
                          names)
            self.assertIn("converted/levels/facility/collision/setup.bin", names)
            self.assertIn("converted/levels/facility/room_bounds.gebounds", names)
            before = tree_hashes(output)
            FACILITY.extract(
                ROOT / "build/u/assets/obseg/bg/bg_ark_all_p.bin",
                ROOT / "assets/obseg/stan/Tbg_ark_all_p_stanZ.c",
                ROOT / "assets/obseg/setup/UsetuparkZ.c",
                ROOT / "build/u/assets/obseg/setup/UsetuparkZ.bin", output,
            )
            self.assertEqual(tree_hashes(output), before)


if __name__ == "__main__":
    unittest.main()
