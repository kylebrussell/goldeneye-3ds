#!/usr/bin/env python3
"""Extract the exact PmodemboxZ and PsatdishZ Dam objective models."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys
import zlib

ROM_SHA1 = "abe01e4aeb033b6c0836819f549c791b26cfde83"
MODELS = (
    {
        "model_id": 335,
        "name": "modembox",
        "resource": "PmodemboxZ",
        "rom_start": 0x7F4E10,
        "compressed_size": 820,
        "compressed_sha256":
            "56adea7b72fc511d8d88781d565a400ac07c3f7246dd601ecd4add67ca5e6f6d",
        "model_size": 1984,
        "model_sha256":
            "3884184d88a875c7007c2ef6d84e2dcd503dd11af62fb75eb045fa4692b01e5b",
    },
    {
        "model_id": 70,
        "name": "satdish",
        "resource": "PsatdishZ",
        "rom_start": 0x801750,
        "compressed_size": 1113,
        "compressed_sha256":
            "96a120b1f3e68736920ed1a38304a59169e8af641e43257a3571c20f69969c69",
        "model_size": 2864,
        "model_sha256":
            "c8ceaf7c50070ae523a7894678a1cf83a7b4d237143c9876377a7e8a70352368",
    },
)


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def extract_one(rom: bytes, output: Path, spec: dict[str, object]) -> dict[str, object]:
    start = int(spec["rom_start"])
    compressed_size = int(spec["compressed_size"])
    compressed = rom[start:start + compressed_size]
    resource = str(spec["resource"])
    if (len(compressed) != compressed_size
            or digest(compressed) != spec["compressed_sha256"]):
        raise ValueError(f"{resource} compressed resource hash mismatch")
    if compressed[:2] != b"\x11\x72":
        raise ValueError(f"{resource} is missing Rare 1172 header")
    inflater = zlib.decompressobj(-15)
    model = inflater.decompress(compressed[2:]) + inflater.flush()
    if not inflater.eof or inflater.unused_data or len(model) != spec["model_size"]:
        raise ValueError(f"{resource} deflate stream/layout mismatch")
    if digest(model) != spec["model_sha256"]:
        raise ValueError(f"{resource} decompressed hash mismatch")
    model_output = output / str(spec["name"])
    model_output.mkdir(parents=True, exist_ok=True)
    (model_output / "model.bin").write_bytes(model)
    manifest = {
        "schema": 1,
        "model_id": spec["model_id"],
        "name": spec["name"],
        "resource": resource,
        "pitem_scale": 0.1,
        "rom": {
            "start": start,
            "compressed_size": compressed_size,
            "compressed_sha256": spec["compressed_sha256"],
            "source_sha1": ROM_SHA1,
        },
        "model": {
            "size": spec["model_size"],
            "sha256": spec["model_sha256"],
            "base_address": 0x05000000,
        },
    }
    (model_output / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n"
    )
    return manifest


def extract(rom_path: Path, output: Path) -> list[dict[str, object]]:
    rom = rom_path.read_bytes()
    if hashlib.sha1(rom).hexdigest() != ROM_SHA1:
        raise ValueError("unsupported ROM; expected unmodified US big-endian release")
    return [extract_one(rom, output, spec) for spec in MODELS]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    try:
        manifests = extract(args.rom, args.output)
    except (OSError, ValueError, zlib.error) as error:
        raise SystemExit(f"cannot extract Dam objective models: {error}") from error
    for manifest in manifests:
        print(f"extracted model {manifest['model_id']} -> "
              f"{(args.output / str(manifest['name'])).absolute()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
