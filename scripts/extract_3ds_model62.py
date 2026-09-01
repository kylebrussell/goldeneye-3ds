#!/usr/bin/env python3
"""Extract exact PchrwppksilZ (setup model 62) from the verified US ROM."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys
import zlib

ROM_SHA1 = "abe01e4aeb033b6c0836819f549c791b26cfde83"
ROM_START = 0x7CF950
COMPRESSED_SIZE = 736
COMPRESSED_SHA256 = "19a9a21f88f9a2baad8321649129dc92109f74c84299cfb1ffce7e0af517c4ef"
MODEL_SIZE = 1808
MODEL_SHA256 = "d299cc45408f05acf06eeebc20e9930fcde429ee5a4dc3c29cd4eb7128694680"


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def extract(rom_path: Path, output: Path) -> dict[str, object]:
    rom = rom_path.read_bytes()
    if hashlib.sha1(rom).hexdigest() != ROM_SHA1:
        raise ValueError("unsupported ROM; expected unmodified US big-endian release")
    compressed = rom[ROM_START:ROM_START + COMPRESSED_SIZE]
    if len(compressed) != COMPRESSED_SIZE or digest(compressed) != COMPRESSED_SHA256:
        raise ValueError("PchrwppksilZ compressed resource hash mismatch")
    if compressed[:2] != b"\x11\x72":
        raise ValueError("PchrwppksilZ is missing Rare 1172 header")
    inflater = zlib.decompressobj(-15)
    model = inflater.decompress(compressed[2:]) + inflater.flush()
    if not inflater.eof or any(inflater.unused_data) or len(model) != MODEL_SIZE:
        raise ValueError("PchrwppksilZ deflate stream/layout mismatch")
    if digest(model) != MODEL_SHA256:
        raise ValueError("PchrwppksilZ decompressed hash mismatch")
    output.mkdir(parents=True, exist_ok=True)
    (output / "model.bin").write_bytes(model)
    manifest = {
        "schema": 1, "model_id": 62, "name": "chrwppksil",
        "pitem_scale": 0.1,
        "rom": {"start": ROM_START, "compressed_size": COMPRESSED_SIZE,
                "compressed_sha256": COMPRESSED_SHA256, "source_sha1": ROM_SHA1},
        "model": {"size": MODEL_SIZE, "sha256": MODEL_SHA256,
                  "base_address": 0x05000000},
    }
    (output / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        result = extract(args.rom, args.output)
    except (OSError, ValueError, zlib.error) as error:
        raise SystemExit(f"cannot extract model 62: {error}") from error
    print(f"extracted model {result['model_id']} -> {args.output.absolute()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
