#!/usr/bin/env python3
"""Map the original Dam mission spawn to its first portal-connected rooms."""

from __future__ import annotations

import argparse
from collections import defaultdict, deque
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import sys
import tempfile
import zlib


BG_BASE = 0x0F000000
LEVEL_SCALE = 0.23363999
CLUSTER_ROOM_COUNT = 6
EXPECTED_HASHES = {
    "setup": "ce8e838896313aa1d7fc86ebf3136ef7634bf2c4f4c8d1fa8c358c4bfd45082f",
    "stan": "badc32f1da171e47499752be6df2e3a399992c85146b08a1d797339e68ff8dc1",
    "background": "40f74a2a087fffa6f1d3519d796d2a2989377ee85fd352deb72d774ea76430c0",
}


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def stan_pack_id(name: str) -> tuple[int, int]:
    match = re.fullmatch(r"([pq])(\d+)([a-z])([0-7]?)", name)
    if match is None:
        raise ValueError(f"invalid STAN tile name: {name}")
    letter, number_text, file_letter, subtri_text = match.groups()
    number = int(number_text)
    if number > 32767:
        raise ValueError(f"STAN tile number is out of range: {name}")
    high = ((ord(letter) - ord("p")) << 15) | number
    low = (ord(file_letter) - ord("a")) * 8 + (int(subtri_text) if subtri_text else 0)
    return high, low


def parse_setup(setup: bytes) -> dict[str, object]:
    if digest(setup) != EXPECTED_HASHES["setup"]:
        raise ValueError(f"Dam setup SHA-256 mismatch: {digest(setup)}")
    text = setup.decode("utf-8")
    pad_section = text[text.index("PadRecord padlist[] = {"):
                       text.index("BoundPadRecord pad3dlist[] = {")]
    pad_pattern = re.compile(
        r'^\s*\{ \{([^}]*)\}, \{([^}]*)\}, \{([^}]*)\}, "([^"]*)", 0 \},$',
        re.MULTILINE,
    )
    pads = []
    for match in pad_pattern.finditer(pad_section):
        vectors = [[float(value.strip().removesuffix("f")) for value in group.split(",")]
                   for group in match.groups()[:3]]
        if any(len(vector) != 3 for vector in vectors):
            raise ValueError("unexpected Dam pad vector layout")
        pads.append({"position": vectors[0], "up": vectors[1], "look": vectors[2],
                     "stan_name": match.group(4)})
    intro = text[text.index("s32 intro[] = {"):text.index("s32 path_neighbors_0[]")]
    spawns = [
        {"intro_record_index": int(index), "pad_index": int(pad), "demo_slot": int(slot)}
        for index, pad, slot in re.findall(
            r'/\* Type = Spawn; index = (\d+) \*/\s*\n'
            r'\s*_mkword\([^\n]+\),\s*(\d+),\s*(\d+),', intro
        )
    ]
    normal = [spawn for spawn in spawns if spawn["demo_slot"] == 0]
    if len(normal) != 1 or normal[0]["pad_index"] >= len(pads):
        raise ValueError(f"unexpected normal Dam spawn records: {normal}")
    spawn = dict(normal[0])
    spawn["pad"] = pads[spawn["pad_index"]]
    spawn["all_spawn_records"] = spawns
    return spawn


def resolve_stan_room(stan: bytes, stan_name: str) -> dict[str, int]:
    if digest(stan) != EXPECTED_HASHES["stan"]:
        raise ValueError(f"Dam STAN SHA-256 mismatch: {digest(stan)}")
    high, low = stan_pack_id(stan_name)
    packed = (high << 8) | low
    matches = [
        (int(tile), int(room, 16))
        for tile, tile_id, room in re.findall(
            r'StandTile tile_(\d+) = \{\s*\n\s*(0x[0-9a-f]+), (0x[0-9a-f]+),',
            stan.decode("utf-8"), re.IGNORECASE
        )
        if int(tile_id, 16) == packed
    ]
    if len(matches) != 1:
        raise ValueError(f"STAN name {stan_name} resolved to {len(matches)} tiles")
    return {"packed_id": packed, "tile_index": matches[0][0], "room_id": matches[0][1]}


def inflate_stream(span: bytes) -> tuple[bytes, int]:
    if span[:2] != b"\x11\x72":
        raise ValueError("room point table is missing its Rare 1172 header")
    inflater = zlib.decompressobj(-15)
    decoded = inflater.decompress(span[2:]) + inflater.flush()
    if not inflater.eof or inflater.unconsumed_tail:
        raise ValueError("room point table has an incomplete deflate stream")
    consumed = len(span) - len(inflater.unused_data)
    if any(inflater.unused_data):
        raise ValueError("room point table has nonzero alignment data")
    return decoded, consumed


def scaled(values: tuple[float, ...] | list[float]) -> list[float]:
    return [round(value / LEVEL_SCALE, 6) for value in values]


def analyze_background(background: bytes, spawn_room: int) -> dict[str, object]:
    if digest(background) != EXPECTED_HASHES["background"]:
        raise ValueError(f"Dam background SHA-256 mismatch: {digest(background)}")
    _reserved, room_address, portal_address, _visibility, _padding = struct.unpack_from(
        ">5I", background
    )
    room_offset = room_address - BG_BASE
    portal_offset = portal_address - BG_BASE
    rooms = []
    for index in range(139):
        point, primary, secondary, x, y, z = struct.unpack_from(
            ">IIIfff", background, room_offset + index * 24
        )
        rooms.append({
            "id": index,
            "point_offset": point - BG_BASE if point else 0,
            "primary_offset": primary - BG_BASE if primary else 0,
            "secondary_offset": secondary - BG_BASE if secondary else 0,
            "origin_raw": [x, y, z],
        })
    all_stream_offsets = sorted({room[key] for room in rooms
                                 for key in ("point_offset", "primary_offset", "secondary_offset")
                                 if room[key]})

    portals = []
    adjacency: dict[int, list[tuple[int, int]]] = defaultdict(list)
    for index in range(1000):
        address, room_a, room_b, control = struct.unpack_from(">IBBH", background,
                                                              portal_offset + index * 8)
        if address == 0 and room_a == 0 and room_b == 0:
            break
        geometry_offset = address - BG_BASE
        point_count = background[geometry_offset]
        points = [list(struct.unpack_from(">fff", background, geometry_offset + 4 + i * 12))
                  for i in range(point_count)]
        portal = {"id": index, "rooms": [room_a, room_b], "control": control,
                  "geometry_offset": geometry_offset, "points_raw": points}
        portals.append(portal)
        adjacency[room_a].append((room_b, index))
        adjacency[room_b].append((room_a, index))

    queue = deque([spawn_room])
    selected = []
    seen = {spawn_room}
    while queue and len(selected) < CLUSTER_ROOM_COUNT:
        room = queue.popleft()
        selected.append(room)
        for neighbor, _portal in adjacency[room]:
            if neighbor not in seen:
                seen.add(neighbor)
                queue.append(neighbor)
    if len(selected) != CLUSTER_ROOM_COUNT:
        raise ValueError(f"spawn portal component produced only {len(selected)} rooms")
    selected_set = set(selected)

    room_records = []
    aggregate_min = [float("inf")] * 3
    aggregate_max = [float("-inf")] * 3
    for room_id in selected:
        room = rooms[room_id]
        start = room["point_offset"]
        end = next(offset for offset in all_stream_offsets if offset > start)
        decoded, compressed_size = inflate_stream(background[start:end])
        if len(decoded) % 16:
            raise ValueError(f"room {room_id} point table is not 16-byte vertices")
        points = [struct.unpack_from(">hhh", decoded, offset)
                  for offset in range(0, len(decoded), 16)]
        local_min = [min(point[axis] for point in points) for axis in range(3)]
        local_max = [max(point[axis] for point in points) for axis in range(3)]
        world_min = [room["origin_raw"][axis] + local_min[axis] for axis in range(3)]
        world_max = [room["origin_raw"][axis] + local_max[axis] for axis in range(3)]
        aggregate_min = [min(aggregate_min[i], world_min[i]) for i in range(3)]
        aggregate_max = [max(aggregate_max[i], world_max[i]) for i in range(3)]
        room_records.append({
            "id": room_id,
            "origin_raw": room["origin_raw"],
            "origin_runtime": scaled(room["origin_raw"]),
            "bounds_raw": {"min": world_min, "max": world_max},
            "bounds_runtime": {"min": scaled(world_min), "max": scaled(world_max)},
            "point_table": {"source_offset": start, "compressed_size": compressed_size,
                            "decoded_size": len(decoded), "vertex_count": len(points),
                            "sha256": digest(decoded)},
        })

    internal = [portal for portal in portals
                if portal["rooms"][0] in selected_set and portal["rooms"][1] in selected_set]
    boundary = [portal for portal in portals
                if (portal["rooms"][0] in selected_set) != (portal["rooms"][1] in selected_set)]
    return {
        "room_order_bfs": selected,
        "rooms": room_records,
        "internal_portals": internal,
        "boundary_portals": boundary,
        "cluster_bounds_raw": {"min": aggregate_min, "max": aggregate_max},
        "cluster_bounds_runtime": {"min": scaled(aggregate_min), "max": scaled(aggregate_max)},
    }


def analyze(setup_path: Path, stan_path: Path, background_path: Path) -> dict[str, object]:
    setup = setup_path.read_bytes()
    stan = stan_path.read_bytes()
    background = background_path.read_bytes()
    spawn = parse_setup(setup)
    stan_resolution = resolve_stan_room(stan, str(spawn["pad"]["stan_name"]))
    background_analysis = analyze_background(background, stan_resolution["room_id"])
    position = spawn["pad"]["position"]
    return {
        "schema": 1,
        "stage": "Dam",
        "level_scale": LEVEL_SCALE,
        "asset_to_runtime_scale": round(1.0 / LEVEL_SCALE, 9),
        "evidence": {
            "normal_play_ramrom_slot": 0,
            "intro_spawn_record_index": spawn["intro_record_index"],
            "setup_pad_index": spawn["pad_index"],
            "setup_pad_stan_name": spawn["pad"]["stan_name"],
            "stan_packed_id": stan_resolution["packed_id"],
            "stan_tile_index": stan_resolution["tile_index"],
            "spawn_room_id": stan_resolution["room_id"],
            "all_intro_spawn_records": spawn["all_spawn_records"],
        },
        "spawn": {
            "position_raw": position,
            "position_runtime": scaled(position),
            "up": spawn["pad"]["up"],
            "look": spawn["pad"]["look"],
        },
        **background_analysis,
        "source_sha256": EXPECTED_HASHES,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--setup", type=Path, required=True)
    parser.add_argument("--stan", type=Path, required=True)
    parser.add_argument("--background", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        result = analyze(args.setup, args.stan, args.background)
        encoded = (json.dumps(result, indent=2, sort_keys=True) + "\n").encode()
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(dir=args.output.parent, delete=False) as temporary:
            temporary.write(encoded)
            temporary_path = Path(temporary.name)
        os.replace(temporary_path, args.output)
    except (OSError, ValueError, zlib.error) as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print(f"mapped Dam mission start rooms {result['room_order_bfs']} -> {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
