#!/usr/bin/env python3

from __future__ import annotations

import hashlib
import importlib.util
from pathlib import Path
import struct
import sys
import unittest
import zlib


REPO = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "convert_rareware_logo_3ds", REPO / "scripts/convert_rareware_logo_3ds.py"
)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class RarewareTextureTests(unittest.TestCase):
    def test_rgba5551_decode(self) -> None:
        pixels = bytes.fromhex("ffff f800 07c1 003f")
        padding = b"\0\0" * (MODULE.WIDTH * MODULE.HEIGHT - 4)
        decoded = MODULE.decode_rgba5551(pixels + padding)
        self.assertEqual(decoded[:16], bytes((255, 255, 255, 255,
                                              255, 0, 0, 0,
                                              0, 255, 0, 255,
                                              0, 0, 255, 255)))

    def test_png_is_lossless_rgba(self) -> None:
        rgba = bytes((index & 0xFF for index in range(MODULE.WIDTH * MODULE.HEIGHT * 4)))
        png = MODULE.encode_png(rgba)
        self.assertEqual(png[:8], b"\x89PNG\r\n\x1a\n")
        cursor = 8
        image_data = bytearray()
        while cursor < len(png):
            length = struct.unpack_from(">I", png, cursor)[0]
            kind = png[cursor + 4:cursor + 8]
            payload = png[cursor + 8:cursor + 8 + length]
            cursor += 12 + length
            if kind == b"IHDR":
                self.assertEqual(struct.unpack_from(">II", payload),
                                 (MODULE.WIDTH, MODULE.HEIGHT))
            elif kind == b"IDAT":
                image_data.extend(payload)
        rows = zlib.decompress(image_data)
        stride = MODULE.WIDTH * 4
        self.assertEqual(len(rows), MODULE.HEIGHT * (stride + 1))
        self.assertEqual(b"".join(rows[row * (stride + 1) + 1:(row + 1) * (stride + 1)]
                                  for row in range(MODULE.HEIGHT)), rgba)

    def test_verified_private_ranges_when_available(self) -> None:
        segment_path = REPO / "build/3ds-runtime/segments/rarewarelogo.bin"
        if not segment_path.is_file():
            self.skipTest("private runtime segment is not available")
        segment = segment_path.read_bytes()
        self.assertEqual(hashlib.sha256(segment).hexdigest(), MODULE.SEGMENT_SHA256)
        for texture in MODULE.TEXTURES:
            payload = segment[
                texture.offset:
                texture.offset + texture.width * texture.height * 2
            ]
            self.assertEqual(hashlib.sha256(payload).hexdigest(), texture.sha256)

    def test_exact_display_list_texture_inventory(self) -> None:
        self.assertEqual(
            [(texture.name, texture.offset, texture.width, texture.height)
             for texture in MODULE.TEXTURES],
            [
                ("rare-r", 0x0020, 49, 28),
                ("rare-a", 0x0AE0, 49, 28),
                ("rare-r2", 0x15A0, 49, 28),
                ("rare-e", 0x2060, 49, 28),
                ("rare-front", 0x4FE8, 32, 32),
                ("rare-body", 0x5FF0, 32, 32),
            ],
        )


if __name__ == "__main__":
    unittest.main()
