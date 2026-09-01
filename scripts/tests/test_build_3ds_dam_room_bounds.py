#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import importlib.util
from pathlib import Path
import struct
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / "build_3ds_dam_room_bounds.py"
SPEC = importlib.util.spec_from_file_location("build_3ds_dam_room_bounds", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def synthetic_fixture(root: Path) -> tuple[Path, Path]:
    background = bytearray(0x200)
    room_start = 0x14
    room_end = room_start + 5 * 24
    portal_start = 0xA0
    struct.pack_into(">5I", background, 0, 0, MODULE.BG_BASE + room_start,
                     MODULE.BG_BASE + portal_start, MODULE.BG_BASE + room_end, 0)
    entries = (
        (0, 0, 0, 0.0, 0.0, 0.0),
        (1, 1, 0, 10.0, 20.0, 30.0),
        (1, 1, 0, -10.0, -20.0, -30.0),
        (1, 1, 1, 0.0, 0.0, 0.0),
        (0, 0, 0, 0.0, 0.0, 0.0),
    )
    for index, entry in enumerate(entries):
        struct.pack_into(">IIIfff", background, room_start + index * 24, *entry)
    geometry = 0xC0
    struct.pack_into(">IBBBB", background, portal_start,
                     MODULE.BG_BASE + geometry, 1, 2, 0, 0)
    background[geometry] = 3
    for index, point in enumerate(((8.0, 18.0, 28.0),
                                   (12.0, 18.0, 28.0),
                                   (10.0, 22.0, 32.0))):
        struct.pack_into(">fff", background, geometry + 4 + index * 12, *point)
    background_path = root / "background.bin"
    rooms_path = root / "rooms"
    background_path.write_bytes(background)
    for room, vertices in ((1, ((-1, -2, -3), (1, 2, 3))),
                           (2, ((-4, -5, -6), (4, 5, 6)))):
        directory = rooms_path / f"room{room:03d}"
        directory.mkdir(parents=True)
        directory.joinpath("point_table.bin").write_bytes(b"".join(
            struct.pack(">hhhHhhBBBB", *vertex, 0, 0, 0, 1, 2, 3, 255)
            for vertex in vertices))
    return background_path, rooms_path


class DamBoundsTests(unittest.TestCase):
    def test_versioned_format_hashes_and_portal_expansion(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            background_path, rooms_path = synthetic_fixture(root)
            output = root / "bounds.gebounds"
            encoded = MODULE.build_asset(
                background_path, rooms_path, output,
                expected_source_size=None, expected_source_sha256=None,
                expected_room_count=3, expected_portal_count=1)
            self.assertEqual(output.read_bytes(), encoded)
            header = struct.unpack_from("<8s6I32sQQ", encoded)
            self.assertEqual(header[:6],
                (MODULE.MAGIC, 1, 80, 3, 24, 72))
            self.assertEqual(header[7], hashlib.sha256(
                background_path.read_bytes()).digest())
            self.assertEqual(header[8], MODULE.fnv1a64(background_path.read_bytes()))
            self.assertEqual(header[9], MODULE.fnv1a64(encoded[80:]))
            room1 = struct.unpack_from("<6f", encoded, 80 + 24)
            room2 = struct.unpack_from("<6f", encoded, 80 + 48)
            self.assertEqual(room1, (8.0, 18.0, 27.0, 12.0, 22.0, 33.0))
            self.assertEqual(room2, (-14.0, -25.0, -36.0, 12.0, 22.0, 32.0))

    @unittest.skipUnless(
        (SCRIPT.parents[1] / "build/u/assets/obseg/bg/bg_dam_all_p.bin").is_file(),
        "private Dam assets unavailable",
    )
    def test_private_asset_hash_and_capacity(self) -> None:
        root = SCRIPT.parents[1]
        background = (root / "build/u/assets/obseg/bg/bg_dam_all_p.bin").read_bytes()
        portal_address, global_vis_address = struct.unpack_from(">II", background, 8)
        global_vis = background[
            global_vis_address - MODULE.BG_BASE:portal_address - MODULE.BG_BASE]
        self.assertEqual(len(global_vis), 3112)
        self.assertEqual(len(global_vis) // 8, 389)
        self.assertEqual(hashlib.sha256(global_vis).hexdigest(),
                         "e03e1593952ae02ece42edcbd6a63b7e26626e225a8be2eb8c9f7e7c47f2ed67")
        with tempfile.TemporaryDirectory() as temporary_name:
            encoded = MODULE.build_asset(
                root / "build/u/assets/obseg/bg/bg_dam_all_p.bin",
                root / "build/3ds-levels/dam/rooms",
                Path(temporary_name) / "bounds.gebounds")
        self.assertEqual(len(encoded), 3368)
        self.assertEqual(hashlib.sha256(encoded).hexdigest(),
                         "361da8abf20d1104b877af1c6cc2dc3c21103adc06a30ccb507925252677eaaf")


if __name__ == "__main__":
    unittest.main()
