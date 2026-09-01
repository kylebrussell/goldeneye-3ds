#!/usr/bin/env python3
"""Encode an authored stage STAN and its canonical setup spawn for the port."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import struct
import sys
import tempfile


MAGIC = b"GESTAN01"
VERSION = 1


def load_dam_collision_module():
    path = Path(__file__).with_name("extract_3ds_dam_collision.py")
    spec = importlib.util.spec_from_file_location("ge_dam_collision_shared", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load shared STAN parser: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def stan_pack_id(name: str) -> int:
    match = re.fullmatch(r"([pq])(\d+)([a-z])([0-7]?)", name)
    if match is None:
        raise ValueError(f"invalid setup STAN name: {name}")
    prefix, number_text, file_letter, subtriangle_text = match.groups()
    number = int(number_text)
    if number > 0x7fff:
        raise ValueError(f"setup STAN number is out of range: {name}")
    high = ((ord(prefix) - ord("p")) << 15) | number
    low = ((ord(file_letter) - ord("a")) * 8
           + (int(subtriangle_text) if subtriangle_text else 0))
    return (high << 8) | low


def parse_vector(text: str) -> list[float]:
    values = [float(value.strip().removesuffix("f")) for value in text.split(",")]
    if len(values) != 3:
        raise ValueError(f"unexpected setup vector: {text}")
    return values


def parse_setup(setup: bytes) -> dict[str, object]:
    text = setup.decode("utf-8")
    try:
        pad_section = text[text.index("PadRecord padlist[] = {"):
                           text.index("BoundPadRecord pad3dlist[] = {")]
        intro_section = text[text.index("s32 intro[] = {"):]
        intro_section = intro_section[:intro_section.index("\n};")]
    except ValueError as error:
        raise ValueError("setup is missing padlist or intro tables") from error

    pad_pattern = re.compile(
        r'^\s*\{ \{([^}]*)\}, \{([^}]*)\}, \{([^}]*)\}, "([^"]*)", 0 \},$',
        re.MULTILINE,
    )
    pads = [
        {
            "index": index,
            "position": parse_vector(match.group(1)),
            "up": parse_vector(match.group(2)),
            "look": parse_vector(match.group(3)),
            "stan_name": match.group(4),
        }
        for index, match in enumerate(pad_pattern.finditer(pad_section))
    ]
    if not pads:
        raise ValueError("setup contains no ordinary pads")

    spawns = [
        {"pad": int(pad), "demo": int(demo)}
        for pad, demo in re.findall(
            r'/\* Type = Spawn; index = \d+ \*/\s*\n'
            r'\s*_mkword\(0, _mkshort\(0, 0\)\),\s*(\d+),\s*(\d+),',
            intro_section,
        )
    ]
    if not spawns:
        raise ValueError("setup intro contains no spawn commands")
    normal = [spawn for spawn in spawns if spawn["demo"] == 0]
    if len(normal) != 1:
        raise ValueError(f"setup must contain exactly one normal spawn, found {len(normal)}")
    if any(int(spawn["pad"]) >= len(pads) for spawn in spawns):
        raise ValueError("setup intro spawn references an out-of-range pad")
    return {"pads": pads, "spawns": spawns, "normal_spawn": normal[0]}


def encode_stan(tiles: list[dict[str, object]], spawn_tile_index: int,
                spawn_room: int) -> bytes:
    point_count = sum(len(tile["points"]) for tile in tiles)
    points_offset = 32 + len(tiles) * 20
    encoded = bytearray(struct.pack(
        ">8s6I", MAGIC, VERSION, len(tiles), point_count, spawn_tile_index,
        spawn_room, points_offset,
    ))
    first_point = 0
    for tile in tiles:
        points = tile["points"]
        encoded.extend(struct.pack(
            ">IIHBBIHH", tile["index"], first_point, len(points), tile["room"],
            tile["special"], tile["id"], tile["mid"], tile["tail"],
        ))
        first_point += len(points)
    for tile in tiles:
        for x, y, z, link in tile["points"]:
            encoded.extend(struct.pack(">hhhH", x, y, z, link))
    if len(encoded) != points_offset + point_count * 8:
        raise AssertionError("stage collision blob size accounting failed")
    return bytes(encoded)


def atomic_write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as temporary:
        temporary.write(data)
        temporary_path = Path(temporary.name)
    os.replace(temporary_path, path)


def extract(stan_path: Path, setup_path: Path, output: Path, *, stage_key: str,
            stage_label: str, expected_stan_sha256: str,
            expected_setup_sha256: str, setup_bin_path: Path | None = None,
            expected_setup_bin_sha256: str | None = None,
            stan_manifest_path: str | None = None,
            setup_manifest_path: str | None = None) -> dict[str, object]:
    stan = stan_path.read_bytes()
    setup = setup_path.read_bytes()
    if sha256(stan) != expected_stan_sha256:
        raise ValueError(f"{stage_label} STAN SHA-256 mismatch: {sha256(stan)}")
    if sha256(setup) != expected_setup_sha256:
        raise ValueError(f"{stage_label} setup SHA-256 mismatch: {sha256(setup)}")

    shared = load_dam_collision_module()
    # Some generated STAN sources annotate ids inline (for example
    # ``0x026108 /*p609b*/``).  Strip comments only for numeric parsing after
    # validating the identity of the untouched authored source.
    stan_for_parse = re.sub(rb"/\*.*?\*/", b"", stan, flags=re.S)
    tiles = shared.parse_tiles(stan_for_parse, None, stage_label)
    setup_data = parse_setup(setup)
    normal_spawn = setup_data["normal_spawn"]
    spawn_pad = setup_data["pads"][normal_spawn["pad"]]
    packed_id = stan_pack_id(spawn_pad["stan_name"])
    matches = [tile for tile in tiles if tile["id"] == packed_id]
    if len(matches) != 1:
        raise ValueError(
            f"normal spawn STAN {spawn_pad['stan_name']} resolved to {len(matches)} tiles"
        )
    spawn_tile = matches[0]
    spawn_blob_index = tiles.index(spawn_tile)
    encoded = encode_stan(tiles, spawn_blob_index, int(spawn_tile["room"]))

    setup_bin_manifest = None
    setup_bin = None
    if setup_bin_path is not None:
        setup_bin = setup_bin_path.read_bytes()
        setup_bin_digest = sha256(setup_bin)
        if (expected_setup_bin_sha256 is not None
                and setup_bin_digest != expected_setup_bin_sha256):
            raise ValueError(
                f"{stage_label} setup binary SHA-256 mismatch: {setup_bin_digest}"
            )
        setup_bin_manifest = {
            "path": "setup.bin", "size": len(setup_bin), "sha256": setup_bin_digest,
        }

    spawn_manifest = {
        "setup_pad": int(normal_spawn["pad"]),
        "position": spawn_pad["position"],
        "up": spawn_pad["up"],
        "look": spawn_pad["look"],
        "stan_name": spawn_pad["stan_name"],
        "stan_id": packed_id,
        "room": int(spawn_tile["room"]),
        "source_tile_index": int(spawn_tile["index"]),
        "blob_tile_index": spawn_blob_index,
    }
    setup_manifest = {
        "schema": 1,
        "stage": stage_label,
        "source": {"path": setup_manifest_path or setup_path.as_posix(),
                   "size": len(setup),
                   "sha256": sha256(setup)},
        "pad_count": len(setup_data["pads"]),
        "intro_spawns": setup_data["spawns"],
        "normal_spawn": spawn_manifest,
    }
    if setup_bin_manifest is not None:
        setup_manifest["compiled_setup"] = setup_bin_manifest
    manifest = {
        "schema": 1,
        "stage": stage_label,
        "stage_key": stage_key,
        "coordinate_space": "original STAN/setup coordinates",
        "rooms": "all",
        "tile_count": len(tiles),
        "point_count": sum(len(tile["points"]) for tile in tiles),
        "spawn": spawn_manifest,
        "blob": {"path": "collision.gestan", "size": len(encoded),
                 "sha256": sha256(encoded)},
        "source": {"path": stan_manifest_path or stan_path.as_posix(),
                   "size": len(stan),
                   "sha256": sha256(stan)},
    }

    output.mkdir(parents=True, exist_ok=True)
    atomic_write(output / "collision.gestan", encoded)
    atomic_write(output / "manifest.json",
                 (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode())
    atomic_write(output / "setup.json",
                 (json.dumps(setup_manifest, indent=2, sort_keys=True) + "\n").encode())
    if setup_bin is not None:
        atomic_write(output / "setup.bin", setup_bin)
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stan", type=Path, required=True)
    parser.add_argument("--setup", type=Path, required=True)
    parser.add_argument("--setup-bin", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--stage-key", required=True)
    parser.add_argument("--stage-label", required=True)
    parser.add_argument("--stan-sha256", required=True)
    parser.add_argument("--setup-sha256", required=True)
    parser.add_argument("--setup-bin-sha256")
    args = parser.parse_args()
    try:
        manifest = extract(
            args.stan, args.setup, args.output, stage_key=args.stage_key,
            stage_label=args.stage_label, expected_stan_sha256=args.stan_sha256,
            expected_setup_sha256=args.setup_sha256, setup_bin_path=args.setup_bin,
            expected_setup_bin_sha256=args.setup_bin_sha256,
        )
    except (OSError, UnicodeError, ValueError) as error:
        parser.error(str(error))
    print(f"extracted {manifest['tile_count']} {args.stage_label} STAN tiles -> "
          f"{args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
