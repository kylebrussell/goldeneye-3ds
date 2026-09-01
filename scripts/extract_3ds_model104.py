#!/usr/bin/env python3
"""Extract exact PwindowZ (Dam's first glass, setup model 104)."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys
import zlib

ROM_SHA1 = "abe01e4aeb033b6c0836819f549c791b26cfde83"
ROM_START = 0x823240
COMPRESSED_SIZE = 235
COMPRESSED_SHA256 = "c5dd888ea5651dff069609542acce29e8ccca265b6d52d4a42182e7e93e97518"
MODEL_SIZE = 448
MODEL_SHA256 = "af84e2a5a73335d9dd241f4007230370a27ae9e96c83d345a6e88fe6a80d1d72"


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def extract(rom_path: Path, output: Path) -> dict[str, object]:
    rom = rom_path.read_bytes()
    if hashlib.sha1(rom).hexdigest() != ROM_SHA1:
        raise ValueError("unsupported ROM; expected unmodified US big-endian release")
    compressed = rom[ROM_START:ROM_START + COMPRESSED_SIZE]
    if len(compressed) != COMPRESSED_SIZE or digest(compressed) != COMPRESSED_SHA256:
        raise ValueError("PwindowZ compressed resource hash mismatch")
    if compressed[:2] != b"\x11\x72":
        raise ValueError("PwindowZ is missing Rare 1172 header")
    inflater = zlib.decompressobj(-15)
    model = inflater.decompress(compressed[2:]) + inflater.flush()
    if not inflater.eof or any(inflater.unused_data) or len(model) != MODEL_SIZE:
        raise ValueError("PwindowZ deflate stream/layout mismatch")
    if digest(model) != MODEL_SHA256:
        raise ValueError("PwindowZ decompressed hash mismatch")
    output.mkdir(parents=True, exist_ok=True)
    (output / "model.bin").write_bytes(model)
    manifest = {
        "schema": 1,
        "model_id": 104,
        "name": "window",
        "pitem_scale": 0.1,
        "rom": {
            "start": ROM_START,
            "compressed_size": COMPRESSED_SIZE,
            "compressed_sha256": COMPRESSED_SHA256,
            "source_sha1": ROM_SHA1,
        },
        "model": {
            "size": MODEL_SIZE,
            "sha256": MODEL_SHA256,
            "base_address": 0x05000000,
        },
    }
    (output / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n"
    )
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        result = extract(args.rom, args.output)
    except (OSError, ValueError, zlib.error) as error:
        raise SystemExit(f"cannot extract model 104: {error}") from error
    print(f"extracted model {result['model_id']} -> {args.output.absolute()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
