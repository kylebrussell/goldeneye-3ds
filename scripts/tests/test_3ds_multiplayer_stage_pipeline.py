#!/usr/bin/env python3
"""Public-source contracts for the private-ROM multiplayer asset pipeline."""
import importlib.util
from pathlib import Path
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
import extract_3ds_multiplayer_stages as pipeline

class MultiplayerAssets(unittest.TestCase):
    def test_canonical_distinct_setups_and_player_limits(self):
        arenas = pipeline.arenas(ROOT)
        self.assertEqual(len(arenas), 11)
        by_key = {a["key"]: a for a in arenas}
        self.assertEqual(by_key["temple"]["setup"], "Ump_setupdishZ")
        self.assertEqual(by_key["facility"]["setup"], "Ump_setuparkZ")
        self.assertEqual(by_key["archives"]["setup"], "Ump_setuparchZ")
        self.assertEqual({by_key[k]["background"] for k in ("library", "basement", "stack")}, {"ame"})
        self.assertEqual(len({by_key[k]["setup"] for k in ("library", "basement", "stack")}), 3)
        self.assertEqual(by_key["egypt"]["max_players"], 2)
        self.assertEqual(by_key["bunker2"]["max_players"], 3)
        self.assertEqual(by_key["temple"]["max_players"], 4)

    def test_unverified_rom_rejected_before_extraction(self):
        with self.assertRaisesRegex(ValueError, "unsupported ROM"):
            pipeline.rom_entries(ROOT, bytes(128))

    def test_stan_comparison_checks_every_header_point_and_link_byte(self):
        tile = dict(id=0x10203, room=4, mid=0x444, tail=0x3012,
                    points=[(1,2,3,0), (4,5,6,1), (7,8,9,2)])
        encoded = struct.pack(">IHH", tile["id"] << 8 | tile["room"], tile["mid"], tile["tail"])
        encoded += b"".join(struct.pack(">hhhH", *p) for p in tile["points"])
        raw = struct.pack(">III", 0, 12, 0) + encoded
        pipeline.validate_stan_binary(raw, [tile])
        for i in range(12, len(raw)):
            corrupted = bytearray(raw); corrupted[i] ^= 1
            with self.assertRaises(ValueError):
                pipeline.validate_stan_binary(corrupted, [tile])

if __name__ == "__main__": unittest.main()
