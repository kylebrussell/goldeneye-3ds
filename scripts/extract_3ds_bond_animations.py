#!/usr/bin/env python3
"""Extract exact US-ROM animation bytes needed by Bond's head runtime."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


ROM_SHA1 = "abe01e4aeb033b6c0836819f549c791b26cfde83"
ROM_SHA256 = "2cdcec8a9f0cb6e36337f3ee39d8ad105dc8afa6ba1c02d466e8f5b771f9a162"
ANIMATION_DATA = (0x28E980, 0x29D160)
ANIMATION_ENTRIES_BASE = 0x124AC0
ANIMATION_ENTRIES_END = 0x28E980
RECORDS = {
    "idle": {
        "data": (0x1C, 0x144),
        "entry": (0x0, 0x2B4C),
    },
    "sprinting": {
        "data": (0x4070, 0x40D4),
        "entry": (0x53CE0, 0x541EC),
    },
    "bond_eye_walk": {
        "data": (0x4144, 0x4298),
        "entry": (0x5484C, 0x55198),
    },
}
EXPECTED_SHA256 = {
    "animation_data.bin": "9bff0e8c50d2d386d7ce43cf20cb5d345a13b32afa388347724f18aa3343a228",
    "animation_entries.bin": "b3e1fa6e9487cab67760ca5ea09bca61216e34074436738acfe225f4ec662f5c",
    "idle.animdata.bin": "51bb858df176b1ac338f0b5a2c091175b9aa441842667cd3e2772450ae86d2b2",
    "idle.entry.bin": "0a12a7da810035cd23c8a318f7e26e2242390dda1161c4fc1d75ce42aa22716b",
    "sprinting.animdata.bin": "7cbc58ca976f63d87b6142d35a199e1b15e3825d1d949f6387af54c0c2858db7",
    "sprinting.entry.bin": "59adf5cfba179e5ff9b40f40a2678a6c5fe1cd09eeb2fc70666577e3da02da61",
    "bond_eye_walk.animdata.bin": "b346d3cd176c35be28864e613207582b67be3be30e9eebf2608299e03bd04de3",
    "bond_eye_walk.entry.bin": "ab4341236a56af0e3c55c16efefbc7a4a23677948788d267f129f7029c3efde6",
}


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--rom", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    rom = args.rom.read_bytes()
    sha1 = hashlib.sha1(rom).hexdigest()
    sha256 = digest(rom)
    if sha1 != ROM_SHA1 or sha256 != ROM_SHA256:
        raise SystemExit(
            f"unsupported ROM: sha1={sha1} sha256={sha256}; expected US big-endian ROM"
        )

    args.output.mkdir(parents=True, exist_ok=True)
    data_start, data_end = ANIMATION_DATA
    full_data = rom[data_start:data_end]
    (args.output / "animation_data.bin").write_bytes(full_data)
    full_entries = rom[ANIMATION_ENTRIES_BASE:ANIMATION_ENTRIES_END]
    (args.output / "animation_entries.bin").write_bytes(full_entries)

    files = {
        "animation_data.bin": {
            "rom_start": data_start,
            "rom_end": data_end,
            "segment_offset": 0,
            "size": len(full_data),
            "sha256": digest(full_data),
        },
        "animation_entries.bin": {
            "rom_start": ANIMATION_ENTRIES_BASE,
            "rom_end": ANIMATION_ENTRIES_END,
            "segment_offset": 0,
            "size": len(full_entries),
            "sha256": digest(full_entries),
        },
    }
    animations = {}
    for name, record in RECORDS.items():
        data_rel_start, data_rel_end = record["data"]
        entry_rel_start, entry_rel_end = record["entry"]
        entry_start = ANIMATION_ENTRIES_BASE + entry_rel_start
        entry_end = ANIMATION_ENTRIES_BASE + entry_rel_end

        data_name = f"{name}.animdata.bin"
        data_blob = full_data[data_rel_start:data_rel_end]
        (args.output / data_name).write_bytes(data_blob)
        files[data_name] = {
            "rom_start": data_start + data_rel_start,
            "rom_end": data_start + data_rel_end,
            "segment_offset": data_rel_start,
            "size": len(data_blob),
            "sha256": digest(data_blob),
        }

        entry_name = f"{name}.entry.bin"
        entry_blob = rom[entry_start:entry_end]
        (args.output / entry_name).write_bytes(entry_blob)
        files[entry_name] = {
            "rom_start": entry_start,
            "rom_end": entry_end,
            "segment_offset": entry_rel_start,
            "size": len(entry_blob),
            "sha256": digest(entry_blob),
        }
        animations[name] = {
            "animation_data_record_offset": data_rel_start,
            "animation_data_record_end": data_rel_end,
            "animation_entry_offset": entry_rel_start,
            "animation_entry_end": entry_rel_end,
        }

    manifest = {
        "format": "goldeneye-3ds-bond-animation-slices-v1",
        "source_rom": {"sha1": sha1, "sha256": sha256},
        "segments": {
            "animation_data": {"rom_start": data_start, "rom_end": data_end},
            "animation_entries": {
                "rom_start": ANIMATION_ENTRIES_BASE,
                "rom_end": ANIMATION_ENTRIES_END,
            },
        },
        "animations": animations,
        "files": files,
    }
    for filename, expected in EXPECTED_SHA256.items():
        actual = files[filename]["sha256"]
        if actual != expected:
            raise SystemExit(
                f"internal extraction contract failed for {filename}: {actual} != {expected}"
            )
    (args.output / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )


if __name__ == "__main__":
    main()
