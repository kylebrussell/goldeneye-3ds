#!/usr/bin/env python3
"""Extract the real Dam opening-area STAN polygons into a portable blob."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import tempfile


MAGIC = b"GESTAN01"
VERSION = 1
DEFAULT_ROOMS: tuple[int, ...] = ()
SPAWN_TILE_ID = 0x000631  # setup pad 33's STAN name, p6g1
SPAWN_ROOM = 135
EXPECTED_SOURCE_SHA256 = (
    "badc32f1da171e47499752be6df2e3a399992c85146b08a1d797339e68ff8dc1"
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def parse_rooms(specification: str) -> tuple[int, ...]:
    if not specification.strip():
        return ()
    result = []
    for value in specification.split(","):
        room = int(value.strip(), 0)
        if room < 0 or room > 255 or room in result:
            raise ValueError(f"invalid or duplicate room: {value}")
        result.append(room)
    return tuple(result)


def parse_tiles(source: bytes,
                expected_sha256: str | None = EXPECTED_SOURCE_SHA256,
                stage_label: str = "Dam") -> list[dict[str, object]]:
    if expected_sha256 is not None and sha256(source) != expected_sha256:
        raise ValueError(f"{stage_label} STAN SHA-256 mismatch: {sha256(source)}")
    text = source.decode("utf-8")
    tiles = []
    expected_index = 0
    for match in re.finditer(r"StandTile tile_(\d+) = \{(.*?)\n\};", text, re.S):
        index = int(match.group(1))
        if index != expected_index:
            raise ValueError(f"non-contiguous STAN tile index {index}")
        expected_index += 1
        body = match.group(2)
        header = body.split("{", 1)[0]
        numbers = re.findall(
            r"(?<![A-Za-z_])-?(?:0x[0-9a-fA-F]+|\d+)", header
        )
        if len(numbers) != 10:
            raise ValueError(f"unexpected STAN header for tile {index}")
        tile_id = int(numbers[0], 0)
        room = int(numbers[1], 0)
        special = int(numbers[2], 0) & 0xF
        red = int(numbers[3], 0) & 0xF
        green = int(numbers[4], 0) & 0xF
        blue = int(numbers[5], 0) & 0xF
        point_count = int(numbers[6], 0)
        header_c = int(numbers[7], 0) & 0xF
        header_d = int(numbers[8], 0) & 0xF
        header_e = int(numbers[9], 0) & 0xF
        points = [
            (int(x), int(y), int(z), int(link, 0))
            for x, y, z, link in re.findall(
                r"\{\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,"
                r"\s*(0x[0-9a-fA-F]+|\d+)\s*\}",
                body,
            )
        ]
        if point_count == 0 and tile_id == 0 and room == 0:
            continue
        if point_count != len(points) or point_count < 3 or point_count > 15:
            raise ValueError(f"invalid point count for STAN tile {index}")
        if tile_id < 0 or tile_id > 0xFFFFFF or room < 0 or room > 255:
            raise ValueError(f"out-of-range STAN header for tile {index}")
        if any(not -32768 <= coordinate <= 32767
               for point in points for coordinate in point[:3]):
            raise ValueError(f"out-of-range point for STAN tile {index}")
        tiles.append({
            "index": index,
            "id": tile_id,
            "room": room,
            "special": special,
            "mid": (special << 12) | (red << 8) | (green << 4) | blue,
            "tail": (point_count << 12) | (header_c << 8)
                    | (header_d << 4) | header_e,
            "points": points,
        })
    if not tiles:
        raise ValueError("Dam STAN contains no tiles")
    return tiles


def extract(source_path: Path, output: Path, rooms: tuple[int, ...]) -> dict[str, object]:
    source = source_path.read_bytes()
    all_tiles = parse_tiles(source)
    selected = [tile for tile in all_tiles
                if not rooms or tile["room"] in rooms]
    spawn_matches = [tile for tile in selected
                     if tile["room"] == SPAWN_ROOM and tile["id"] == SPAWN_TILE_ID]
    if len(spawn_matches) != 1:
        raise ValueError(f"expected one Dam spawn tile, found {len(spawn_matches)}")
    point_count = sum(len(tile["points"]) for tile in selected)
    spawn_record = selected.index(spawn_matches[0])
    header_size = 32
    tile_table_size = len(selected) * 20
    points_offset = header_size + tile_table_size
    encoded = bytearray(struct.pack(
        ">8s6I", MAGIC, VERSION, len(selected), point_count, spawn_record,
        SPAWN_ROOM, points_offset
    ))
    first_point = 0
    for tile in selected:
        points = tile["points"]
        encoded.extend(struct.pack(
            ">IIHBBIHH", tile["index"], first_point, len(points), tile["room"],
            tile["special"], tile["id"], tile["mid"], tile["tail"]
        ))
        first_point += len(points)
    for tile in selected:
        for x, y, z, link in tile["points"]:
            encoded.extend(struct.pack(">hhhH", x, y, z, link))
    if len(encoded) != points_offset + point_count * 8:
        raise AssertionError("Dam collision blob size accounting failed")

    output.mkdir(parents=True, exist_ok=True)
    blob_path = output / "collision.gestan"
    manifest_path = output / "manifest.json"
    manifest = {
        "schema": 1,
        "stage": "Dam",
        "coordinate_space": "original STAN/setup coordinates",
        "rooms": list(rooms) if rooms else "all",
        "tile_count": len(selected),
        "point_count": point_count,
        "spawn": {
            "setup_pad": 33,
            "position": [4719.0, -18.0, 3949.0],
            "stan_name": "p6g1",
            "room": SPAWN_ROOM,
            "source_tile_index": spawn_matches[0]["index"],
            "blob_tile_index": spawn_record,
        },
        "blob": {
            "path": blob_path.name,
            "size": len(encoded),
            "sha256": sha256(bytes(encoded)),
        },
        "source_sha256": sha256(source),
    }
    with tempfile.NamedTemporaryFile(dir=output, delete=False) as temporary:
        temporary.write(encoded)
        temporary_blob = Path(temporary.name)
    temporary_blob.replace(blob_path)
    with tempfile.NamedTemporaryFile("w", dir=output, delete=False) as temporary:
        json.dump(manifest, temporary, indent=2, sort_keys=True)
        temporary.write("\n")
        temporary_manifest = Path(temporary.name)
    temporary_manifest.replace(manifest_path)
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--rooms", default=",".join(map(str, DEFAULT_ROOMS)),
                        help="optional comma-separated room subset; default is all")
    args = parser.parse_args()
    try:
        manifest = extract(args.input, args.output, parse_rooms(args.rooms))
    except (OSError, UnicodeError, ValueError) as error:
        parser.error(str(error))
    print(
        f"extracted {manifest['tile_count']} Dam STAN tiles / "
        f"{manifest['point_count']} points -> {args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
