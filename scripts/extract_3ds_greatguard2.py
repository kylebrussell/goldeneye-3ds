#!/usr/bin/env python3
"""Extract exact Cgreatguard2Z (Dam authored body 37) from the US ROM."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys
import zlib

ROM_SHA1 = "abe01e4aeb033b6c0836819f549c791b26cfde83"
ROM_START = 0x70F580
RESOURCE_SIZE = 0x2720
RESOURCE_SHA256 = "a5362389a404112204532d0f31f30bdb64026f2269a314edb3de7c583bd5bc87"
MODEL_SIZE = 23840
MODEL_SHA256 = "25b149db172d11a26231a76b3dfffae611f356db814e5d7bbbaefc73753d5a3f"


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def extract(rom_path: Path, output: Path) -> dict[str, object]:
    rom = rom_path.read_bytes()
    if hashlib.sha1(rom).hexdigest() != ROM_SHA1:
        raise ValueError("unsupported ROM; expected unmodified US big-endian release")
    resource = rom[ROM_START:ROM_START + RESOURCE_SIZE]
    if len(resource) != RESOURCE_SIZE or digest(resource) != RESOURCE_SHA256:
        raise ValueError("Cgreatguard2Z compressed resource hash mismatch")
    if resource[:2] != b"\x11\x72":
        raise ValueError("Cgreatguard2Z is missing Rare 1172 header")
    inflater = zlib.decompressobj(-15)
    model = inflater.decompress(resource[2:]) + inflater.flush()
    if not inflater.eof or any(inflater.unused_data.strip(b"\0")):
        raise ValueError("Cgreatguard2Z deflate stream/layout mismatch")
    if len(model) != MODEL_SIZE or digest(model) != MODEL_SHA256:
        raise ValueError("Cgreatguard2Z decompressed model mismatch")
    output.mkdir(parents=True, exist_ok=True)
    (output / "model.bin").write_bytes(model)
    manifest = {
        "schema": 1,
        "body_id": 37,
        "name": "greatguard2",
        "model_scale": 0.1,
        "rom": {
            "start": ROM_START,
            "resource_size": RESOURCE_SIZE,
            "resource_sha256": RESOURCE_SHA256,
            "source_sha1": ROM_SHA1,
        },
        "model": {
            "size": MODEL_SIZE,
            "sha256": MODEL_SHA256,
            "base_address": 0x05000000,
            "root_node_offset": 0xDC,
            "matrix_count": 0x14,
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
        raise SystemExit(f"cannot extract greatguard2: {error}") from error
    print(f"extracted body {result['body_id']} -> {args.output.absolute()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
